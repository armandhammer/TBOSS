/* SPDX-License-Identifier: GPL-3.0-or-later - 1.5.0 step sequencer: the pure core (lanes.c: every
 * field's default, steps and records round trip, malformed records refused, locate, polymeter, Song
 * Position) and, on the real code, lanes playing with the board's transport and with a DAW (Start,
 * Song Position Pointer, Continue), mute, kinds, channels and velocities, pattern records (only changed
 * chunks written, reloaded, SysEx, compaction), and the step screen (Settings > Sequence > Steps):
 * steps on and off, a value given to a held step, step record, lanes, pages, its pads and text.
 * 1 ms ticks; buttons through tc_poll, pads through a fake scan every 15 ms. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
bool tl_init(void) { return true; }
static tl_rgb pad_led[TL_PADS];
void tl_set_pad(uint32_t pad, tl_rgb c) { if (pad < TL_PADS) pad_led[pad] = c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
typedef struct { uint32_t x, y; char text[40]; } piece;
static piece pieces[64];
static uint32_t count;
void td_clear(void) { count = 0u; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (count < 64u && text[0]) { pieces[count].x = x; pieces[count].y = y; snprintf(pieces[count].text, sizeof pieces[count].text, "%s", text); count++; }
    return x + 6u * scale * (uint32_t)strlen(text);
}
static const char *at(uint32_t x, uint32_t y) {
    for (uint32_t i = 0; i < count; ++i) if (pieces[i].x == x && pieces[i].y == y) return pieces[i].text;
    return "";
}
static uint32_t seed = 0x1A2E5u;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
/* --- the real loop --------------------------------------------------------------------------- */
static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
typedef struct { uint32_t at; uint8_t ch, note, velocity; } strike;
static strike ons[4096];
static uint32_t nons, offs;
static uint8_t sounding[16][128];
static uint32_t on_at[16][128], longest_gate, shortest_gate = UINT32_MAX;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint8_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; on_at[ch][p[2]] = now;
        if (nons < 4096u) ons[nons++] = (strike){now, ch, p[2], p[3]};
    }
    if (p[0] == TK_OFF) {
        assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++;
        uint32_t gate = now - on_at[ch][p[2]];
        if (gate > longest_gate) longest_gate = gate;
        if (gate < shortest_gate) shortest_gate = gate;
    }
    return true;
}
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += sounding[c][p];
    return n;
}
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
/* A DAW's clock: one MIDI clock every daw_us microseconds while daw_us is set. */
static uint32_t daw_us, daw_next_us;
static void midi(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { const uint8_t p[4] = {a, b, c, d}; tm_packet(p, now); }
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        while (daw_us && (int32_t)(now * 1000u - daw_next_us) >= 0) { midi(0x0Fu, 0xF8u, 0u, 0u); daw_next_us += daw_us; }
        if (now % 10u == 0u) (void)tc_poll(&tc_state, &e, reader, NULL, now);
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
enum { NAV_LEFT = 8, NAV_UP = 7, NAV_CENTRE = 6, NAV_DOWN = 1, NAV_RIGHT = 3 };
static void nav(uint8_t bit) { raw[1] |= 1u << bit; run(50); raw[1] &= ~(1u << bit); run(50); }
static void hold(uint32_t k) { scan.values[k] = 3000u; run(60); }
static void lift(uint32_t k) { scan.values[k] = 0u; run(60); }
static void touch(uint32_t k) { hold(k); lift(k); }
static void quiet(void) { assert(!heard() && !tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE); }
static void fresh(void) {
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw); memset(sounding, 0, sizeof sounding);
    daw_us = 0u; nons = offs = 0u; longest_gate = 0u; shortest_gate = UINT32_MAX;
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    run(600); assert(e.state == TK_ACTIVE);
}
/* The pad of step s on the step screen's page (12 to a row, rows top down). */
static uint32_t step_pad(uint32_t s) { s %= 48u; return 48u + 4u * (s % 12u) + s / 12u; }
static ln_step on_step(uint8_t value) { ln_step s; memset(&s, 0, sizeof s); s.on = true; s.value = value; return s; }
/* Note Ons of note (on channel ch) since strike `from`, their times into out; how many. */
static uint32_t times_of(uint32_t from, uint8_t ch, uint8_t note, uint32_t *out, uint32_t max) {
    uint32_t n = 0;
    for (uint32_t i = from; i < nons && n < max; ++i) if (ons[i].ch == ch && ons[i].note == note) out[n++] = ons[i].at;
    return n;
}
static bool near(uint32_t a, uint32_t b, uint32_t slack) { return a > b ? a - b <= slack : b - a <= slack; }
/* --- pure ------------------------------------------------------------------------------------ */
static ln_step random_step(void) {
    ln_step s;
    s.on = rnd(2u); s.tie = rnd(2u); s.value = (uint8_t)rnd(128u); s.velocity = (uint8_t)rnd(128u);
    s.edit = (uint8_t)(rnd(8u) | rnd(3u) << 3 | rnd(8u) << 5); s.scale = (uint8_t)rnd(TC_ALL_SCALES + 1u);
    s.gate = (uint8_t)rnd(16u); s.probability = (uint8_t)rnd(16u); s.ratchet = (uint8_t)rnd(8u);
    s.condition = (uint8_t)rnd(32u); s.nudge = (uint8_t)rnd(16u);
    return s;
}
static void defaults(void) {
    ln_pattern p; memset(&p, 0x5A, sizeof p); ln_clear(&p);
    assert(p.version == LN_VERSION && !p.seed);
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        const ln_lane *l = &p.lanes[i];
        assert(l->kind == LN_CHORDS && !l->channel && !l->length && !l->rate && !l->direction && !l->swing && !l->mute &&
               !l->transpose && !l->euclid);
        assert(ln_length(l) == 16u && ln_rate(l) == 3u && tc_arp_rates[ln_rate(l)].num == 6u); /* 16 steps of 1/16 */
        for (uint32_t s = 0; s < LN_STEPS; ++s) assert(!ln_step_pack(l->steps[s]));
    }
    assert(!ln_any(&p));
    /* A record of nothing but defaults: only its version word; it decodes to the defaults. */
    for (uint32_t slot = 0; slot < LN_RECORDS; ++slot) {
        uint32_t w[LN_WORDS]; ln_encode(&p, slot, w);
        assert(w[0] == LN_VERSION && ln_chunk_default(&p, slot));
        for (uint32_t i = 1; i < LN_WORDS; ++i) assert(!w[i]);
        ln_pattern q; ln_clear(&q); q.lanes[slot / 4u].steps[slot % 4u * 16u].on = true; q.lanes[slot / 4u].kind = LN_DRUMS;
        assert(ln_decode(w, slot, &q) && !q.lanes[slot / 4u].steps[slot % 4u * 16u].on);
        assert(slot % 4u || q.lanes[slot / 4u].kind == LN_CHORDS); /* chunk 0 brings the lane's settings */
    }
    /* Zero means default: a zero step is off and plays the pad's value 0 at the lane's gate. */
    ln_step z; assert(ln_step_unpack(0u, &z) && !z.on && !z.velocity && !z.scale && !z.gate && !z.probability && !z.ratchet &&
                      !z.condition && !z.nudge && !z.tie);
    /* Lengths and rates keep their defaults as 0. */
    ln_lane l; memset(&l, 0, sizeof l);
    ln_set_length(&l, 16u); assert(!l.length); ln_set_length(&l, 5u); assert(l.length == 5u && ln_length(&l) == 5u);
    ln_set_length(&l, 64u); assert(ln_length(&l) == 64u); ln_set_length(&l, 65u); assert(ln_length(&l) == 16u);
    ln_set_rate(&l, 3u); assert(!l.rate); ln_set_rate(&l, 0u); assert(l.rate == 1u && ln_rate(&l) == 0u);
    ln_set_rate(&l, 11u); assert(ln_rate(&l) == 11u); ln_set_rate(&l, 12u); assert(ln_rate(&l) == 3u);
    /* The left half's values: chords and melody the pad's position, drums 36 bottom left, up each column. */
    for (uint32_t k = 0; k < 48u; ++k) assert(ln_pad_value(LN_CHORDS, k) == k && ln_pad_value(LN_MELODY, k) == k);
    assert(ln_pad_value(LN_DRUMS, 3u) == 36u && ln_pad_value(LN_DRUMS, 0u) == 39u && ln_pad_value(LN_DRUMS, 7u) == 40u &&
           ln_pad_value(LN_DRUMS, 44u) == 83u && ln_pad_value(LN_DRUMS, 47u) == 80u);
    /* The edit byte is settings.c's chord byte, both ways. */
    for (uint32_t b = 0; b < 256u; ++b) if (((b >> 3) & 3u) != 3u) assert(ln_edit_byte(ln_edit((uint8_t)b)) == b);
    /* Randomness for later: the same inputs, the same number; any input changes it. */
    assert(ln_hash(1, 2, 3, 4) == ln_hash(1, 2, 3, 4) && ln_hash(1, 2, 3, 4) != ln_hash(1, 2, 3, 5) &&
           ln_hash(1, 2, 3, 4) != ln_hash(1, 2, 4, 4) && ln_hash(1, 2, 3, 4) != ln_hash(1, 3, 3, 4) &&
           ln_hash(1, 2, 3, 4) != ln_hash(2, 2, 3, 4));
    printf("defaults: every field 0 (kind chords, 16 steps of 1/16, gate, probability, ratchet, condition, nudge "
           "default), default records hold only the version, left-half values, edit bytes, hash; ");
}
static void round_trip(void) {
    /* Steps: every field survives 64 bits; spare bits, sus 3 and unknown scales are refused. */
    for (uint32_t r = 0; r < 20000u; ++r) {
        ln_step s = random_step(), back;
        bool valid = ((s.edit >> 3) & 3u) != 3u;
        assert(ln_step_unpack(ln_step_pack(s), &back) == valid);
        if (valid) assert(!memcmp(&s, &back, sizeof s));
        uint64_t bits = ln_step_pack(s);
        ln_step keep = back;
        assert(!ln_step_unpack(bits | (uint64_t)1u << (51u + rnd(13u)), &back) && !memcmp(&keep, &back, sizeof back));
    }
    ln_step sus3; assert(!ln_step_unpack((uint64_t)3u << 18, &sus3) && !ln_step_unpack((uint64_t)(TC_ALL_SCALES + 1u) << 23, &sus3));
    /* Records: random patterns round trip through all 16 slots; malformed records are refused and leave
     * the pattern as it was. */
    uint32_t refused = 0;
    for (uint32_t r = 0; r < 400u; ++r) {
        ln_pattern p; ln_clear(&p);
        p.seed = (uint16_t)rnd(65536u);
        for (uint32_t i = 0; i < LN_LANES; ++i) {
            ln_lane *l = &p.lanes[i];
            l->kind = (uint8_t)rnd(LN_KINDS); l->channel = (uint8_t)rnd(LN_CHANNELS); l->length = (uint8_t)rnd(65u);
            l->rate = (uint8_t)rnd(LN_RATES + 1u); l->direction = (uint8_t)rnd(4u); l->swing = (uint8_t)rnd(51u);
            l->mute = (uint8_t)rnd(2u); l->transpose = (int8_t)((int32_t)rnd(97u) - 48); l->euclid = (uint8_t)rnd(65u);
            for (uint32_t s = 0; s < LN_STEPS; ++s) { do l->steps[s] = random_step(); while (((l->steps[s].edit >> 3) & 3u) == 3u); }
        }
        ln_pattern q; ln_clear(&q);
        for (uint32_t slot = 0; slot < LN_RECORDS; ++slot) {
            uint32_t w[LN_WORDS]; ln_encode(&p, slot, w);
            assert(ln_decode(w, slot, &q));
            uint32_t bad[LN_WORDS]; memcpy(bad, w, sizeof bad);
            uint32_t chunk = slot % 4u;
            switch (rnd(14u)) {
            case 0: bad[0] = 2u; break;                                                      /* another format */
            case 1: bad[37u + rnd(3u)] = 1u; break;                                           /* unused words */
            case 2: if (!chunk) continue; bad[33u + rnd(3u)] = 1u; break;                     /* lane settings outside chunk 0 */
            case 3: if (!slot) continue; bad[36] = 1u; break;                                 /* the seed outside lane 0 chunk 0 */
            case 4: if (chunk) continue; bad[33] = (bad[33] & ~0xFFu) | LN_KINDS; break;      /* no such kind */
            case 5: if (chunk) continue; bad[33] = (bad[33] & ~0xFF00u) | LN_CHANNELS << 8; break;
            case 6: if (chunk) continue; bad[33] = (bad[33] & ~0xFF0000u) | (LN_STEPS + 1u) << 16; break;
            case 7: if (chunk) continue; bad[33] = (bad[33] & 0x00FFFFFFu) | (LN_RATES + 1u) << 24; break;
            case 8: if (chunk) continue; bad[34] = (bad[34] & ~0xFFu) | 4u; break;            /* direction */
            case 9: if (chunk) continue; bad[34] = (bad[34] & ~0xFF0000u) | 2u << 16; break;   /* mute 2 */
            case 10: if (chunk) continue; bad[34] = (bad[34] & 0x00FFFFFFu) | 49u << 24; break; /* transpose 49 */
            case 11: if (chunk) continue; bad[35] = LN_STEPS + 1u; break;                      /* Euclid fill */
            case 12: bad[2u + 2u * rnd(16u)] |= 1u << (19u + rnd(13u)); break;                 /* a step's spare bits */
            default: if (slot) continue; bad[36] = 0x10000u; break;                            /* the seed's 16 bits */
            }
            ln_pattern keep = q;
            assert(!ln_decode(bad, slot, &q) && !memcmp(&keep, &q, sizeof q));
            refused++;
        }
        assert(!memcmp(&p, &q, sizeof p));
        uint32_t w[LN_WORDS]; ln_encode(&p, 0, w); assert(!ln_decode(w, LN_RECORDS, &q)); /* no such slot */
    }
    printf("round trip: 20000 steps, 400 patterns through 16 records each, %u malformed records refused; ", refused);
}
static void locate(void) {
    rh_rate sixteenth = {6, 1}, five = {24, 5}, triplet = {4, 1};
    uint32_t n;
    /* 1/16: step s from 6s clocks; a lane of 16 goes round every bar. */
    for (uint32_t clock = 0; clock < 24u * 4u * 3u; ++clock) {
        uint32_t step = ln_locate(clock * RH_SUB, sixteenth, 16u, &n);
        assert(n == clock / 6u && step == clock / 6u % 16u);
        assert(ln_locate(clock * RH_SUB + RH_SUB - 1u, sixteenth, 16u, NULL) == step); /* the whole clock */
    }
    /* Polymeter: 5 steps of 5 per beat against 3 of 1/16 against 16th triplets in 4: they meet again at the
     * least common period (5 steps = 24 clocks, 3 = 18 clocks, 4 = 16 clocks: 144 clocks), never before. */
    uint32_t first = UINT32_MAX;
    for (uint32_t clock = 1; clock <= 200u && first == UINT32_MAX; ++clock) {
        uint32_t pos = clock * RH_SUB;
        if (!ln_locate(pos, five, 5u, NULL) && !ln_locate(pos, sixteenth, 3u, NULL) && !ln_locate(pos, triplet, 4u, NULL) &&
            rh_step_start(rh_step_at(pos, five), five) == pos && rh_step_start(rh_step_at(pos, sixteenth), sixteenth) == pos &&
            rh_step_start(rh_step_at(pos, triplet), triplet) == pos) first = clock;
    }
    assert(first == 144u);
    /* Song position: located at once, a position is the same step as counted from the start. */
    for (uint32_t r = 0; r < 2000u; ++r) {
        uint32_t sixteenths = rnd(16384u), pos = sixteenths * 6u * RH_SUB, length = 1u + rnd(64u);
        rh_rate rate = tc_arp_rates[rnd(TC_ARP_RATES)];
        uint32_t step = ln_locate(pos, rate, length, &n), counted = 0;
        while (rh_step_start(counted + 1u, rate) <= pos) counted++;
        assert(n == counted && step == counted % length);
    }
    /* The gate: half of each step, also on fractional steps. */
    assert(ln_gate_end(0, sixteenth, 50u) == 3u * RH_SUB && ln_gate_end(4, sixteenth, 50u) == 27u * RH_SUB);
    for (uint32_t c = 0; c < 50u; ++c) {
        uint32_t s0 = rh_step_start(c, five), s1 = rh_step_start(c + 1u, five), g = ln_gate_end(c, five, 50u);
        assert(g > s0 && g < s1 && near(g - s0, (s1 - s0) / 2u, 1u));
    }
    printf("locate: 1/16, polymeter 5:3:4 meeting at 144 clocks, 2000 song positions, gates; ");
}
/* --- playing --------------------------------------------------------------------------------- */
static void transport(void) {
    fresh();
    ln_lane *l = &ln.lanes[0];
    ln_set_length(l, 8u); l->steps[0] = on_step(3u); l->steps[0].velocity = 90u; l->steps[4] = on_step(7u); /* C, Dm */
    tp_notes c, f;
    assert(tc_pad_with(e.layout, 3u, (tc_chord){0, 0, 0}, &c) && tc_pad_with(e.layout, 7u, (tc_chord){0, 0, 0}, &f));
    run(500); assert(!nons); /* nothing until the transport runs */
    tc_state.transport = true; uint32_t start = now;
    run(2010);
    /* 1/16 at 120 BPM is 125 ms: step 0 (pad 3) every second, step 4 (pad 7) half a second later. */
    uint32_t t[16];
    assert(times_of(0, 0u, c.notes[0], t, 16u) == 3u);
    for (uint32_t i = 0; i < 3u; ++i) assert(near(t[i], start + 1000u * i, 3u));
    uint32_t chords = 0;
    for (uint32_t i = 0; i < nons; ++i) if (ons[i].note == c.notes[0]) {
        chords++; assert(ons[i].velocity == 90u && ons[i].ch == 0u); /* the chord channel */
    }
    assert(times_of(0, 0u, f.notes[0], t, 16u) == 2u && near(t[0], start + 500u, 3u) && near(t[1], start + 1500u, 3u));
    /* Each strike sounds half its step: 62.5 ms (a packet a millisecond: a chord's notes go out in turn). */
    assert(shortest_gate >= 58u && longest_gate <= 67u);
    tc_state.transport = false; run(100); quiet();
    /* Stopped and started again: from the first step. */
    nons = 0u; tc_state.transport = true; start = now; run(200);
    assert(nons && near(ons[0].at, start, 3u) && ons[0].note == c.notes[0]);
    tc_state.transport = false; run(100); quiet();
    printf("transport: steps 1 and 5 of 8 at 1/16 (125 ms), chords as the pad plays them, half-step gates; ");
}
static void polymeter(void) {
    fresh();
    /* Three drum lanes: 3 steps and 4 steps of 1/16, 5 of 5 per beat; each sounds its step 1. */
    ln_lane *a = &ln.lanes[0], *b = &ln.lanes[1], *q = &ln.lanes[2];
    a->kind = b->kind = q->kind = LN_DRUMS;
    ln_set_length(a, 3u); a->steps[0] = on_step(36u);
    ln_set_length(b, 4u); b->steps[0] = on_step(38u); b->channel = 3u;                 /* its own channel */
    ln_set_length(q, 5u); ln_set_rate(q, 8u); q->steps[0] = on_step(42u); q->steps[2] = on_step(46u);
    tc_state.transport = true; uint32_t start = now;
    run(3005);
    uint32_t t[32];
    assert(times_of(0, 9u, 36u, t, 32u) == 9u);                                         /* every 375 ms */
    for (uint32_t i = 0; i < 9u; ++i) assert(near(t[i], start + 375u * i, 3u));
    assert(times_of(0, 2u, 38u, t, 32u) == 7u);                                         /* every 500 ms on channel 3 */
    for (uint32_t i = 0; i < 7u; ++i) assert(near(t[i], start + 500u * i, 3u));
    assert(times_of(0, 9u, 42u, t, 32u) == 7u);                                         /* 5 per beat, 5 steps: every beat */
    for (uint32_t i = 0; i < 7u; ++i) assert(near(t[i], start + 500u * i, 3u));
    assert(times_of(0, 9u, 46u, t, 32u) == 6u);                                         /* its step 3: 200 ms later */
    for (uint32_t i = 0; i < 6u; ++i) assert(near(t[i], start + 200u + 500u * i, 3u));
    /* Mute and Off: silent at once and from then on. */
    a->mute = 1u; b->kind = LN_OFF; run(5); uint32_t from = nons; run(1500);
    assert(!times_of(from, 9u, 36u, t, 32u) && !times_of(from, 2u, 38u, t, 32u) && times_of(from, 9u, 42u, t, 32u) == 3u);
    tc_state.transport = false; run(100); quiet();
    printf("polymeter: 3 against 4 against 5-per-beat lanes on the board's clock, own channel, mute and off; ");
}
static void daw(void) {
    fresh();
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_DRUMS;
    for (uint8_t s = 0; s < 16u; ++s) l->steps[s] = on_step((uint8_t)(36u + s)); /* step n plays note 36 + n */
    daw_us = 20833u; daw_next_us = now * 1000u; run(300);                     /* the DAW's clock, stopped */
    assert(!nons && !e.lane_running);
    midi(0x0Fu, 0xFAu, 0u, 0u); uint32_t start = now;                          /* Start */
    run(1010);
    assert(nons >= 8u && ons[0].note == 36u && near(ons[0].at, start, 22u));
    for (uint32_t i = 1; i < 8u; ++i) assert(ons[i].note == 36u + i && near(ons[i].at - ons[i - 1u].at, 125u, 22u));
    midi(0x0Fu, 0xFCu, 0u, 0u); run(100); quiet();                             /* Stop */
    /* Song Position Pointer to the 6th sixteenth, then Continue: that step at once, then on. */
    nons = 0u;
    midi(0x03u, 0xF2u, 5u, 0u); run(50); assert(!nons);
    midi(0x0Fu, 0xFBu, 0u, 0u); start = now; run(260);
    assert(nons >= 2u && ons[0].note == 41u && near(ons[0].at, start, 22u) && ons[1].note == 42u);
    /* Jumping while it plays (a loop back to bar 1): the step where it lands. */
    midi(0x03u, 0xF2u, 12u, 0u); uint32_t from = nons; run(140);
    assert(nons > from && ons[from].note == 48u);
    /* Start again: from step 1. */
    midi(0x0Fu, 0xFAu, 0u, 0u); from = nons; run(60);
    assert(nons > from && ons[from].note == 36u);
    /* The DAW's clock stops arriving: the lanes stop with it, nothing left sounding. */
    daw_us = 0u; run(400); quiet(); assert(!e.lane_running);
    printf("DAW: Start from step 1, 1/16 on its clock, Song Position Pointer + Continue, a jump while playing, "
           "Start again, clock lost; ");
}
static void sounds(void) {
    fresh();
    ln_lane *l = &ln.lanes[1];
    l->kind = LN_MELODY; ln_set_length(l, 4u); l->steps[0] = on_step(3u); l->steps[1] = on_step(7u); l->steps[1].velocity = 30u;
    tc_state.melody_vel = TC_VEL_FIXED;
    tc_state.transport = true; run(300);
    /* A melody step is that position of the right half's Scale layout (whatever it shows): bottom left the
     * root C3, the next column D3; the melody channel; Fixed velocity wins over the step's. */
    assert(nons >= 2u && ons[0].note == 48u && ons[1].note == 50u && ons[0].ch == tc_state.melody_ch);
    assert(ons[0].velocity == LN_VELOCITY_DEFAULT && ons[1].velocity == LN_VELOCITY_DEFAULT);
    tc_state.transport = false; run(100); quiet();
    tc_state.melody_vel = TC_VEL_DYNAMIC; tc_state.melody_layout = TP_MELODY_DRUMS; e.layout = tc_layout(&tc_state);
    nons = 0u; tc_state.transport = true; run(300);
    assert(ons[0].note == 48u && ons[1].note == 50u && ons[0].velocity == LN_VELOCITY_DEFAULT && ons[1].velocity == 30u);
    tc_state.transport = false; run(100); quiet();
    /* The root buttons transpose a chord lane; a borrowed scale stays. */
    l->kind = LN_CHORDS; l->steps[0] = on_step(3u); l->steps[1] = on_step(15u); l->steps[1].scale = (uint8_t)(tc_mode_buttons[3] + 1u);
    tp_layout key = tc_layout(&tc_state); key.tonic = 2u; assert(tc_set_layout(&tc_state, &e, key));
    tp_notes d, dorian; tp_layout borrowed = e.layout; borrowed.mode = tc_mode_buttons[3];
    assert(tc_pad_with(e.layout, 3u, (tc_chord){0, 0, 0}, &d) && tc_pad_with(borrowed, 15u, (tc_chord){0, 0, 0}, &dorian));
    nons = 0u; tc_state.transport = true; run(300);
    assert(ons[0].note == d.notes[0] && d.notes[0] % 12u == 2u);
    bool found = false;
    for (uint32_t i = 0; i < nons; ++i) found = found || (ons[i].at > ons[0].at + 100u && ons[i].note == dorian.notes[0]);
    assert(found);
    tc_state.transport = false; run(100); quiet();
    /* A step whose value the kind has no pad for is silent; a lane on the drum channel by default. */
    l->steps[0].value = 60u; l->steps[1].on = false; nons = 0u; tc_state.transport = true; run(600); assert(!nons);
    tc_state.transport = false; run(100); quiet();
    /* All notes off ends lane notes; they come back at the next step. */
    l->kind = LN_DRUMS; l->steps[0] = on_step(36u); ln_set_rate(l, 0u); /* 1/4: 500 ms steps */
    tc_state.transport = true; run(20); assert(heard() == 1u);
    tc_state.panic = true; run(20); assert(!heard());
    uint32_t from = nons; run(2000); assert(nons == from + 1u && ons[from].note == 36u); /* the next time round */
    tc_state.transport = false; run(100); quiet();
    printf("sounds: melody steps as the Scale layout, velocities (Fixed wins), the root buttons transpose, a "
           "borrowed scale stays, unplayable values silent, all notes off; ");
}
/* --- storage --------------------------------------------------------------------------------- */
static void storage(void) {
    fresh();
    uint32_t base = st_records();
    ln.lanes[0].kind = LN_MELODY; ln_changed(0, LN_STEPS);
    ln.lanes[2].steps[20] = on_step(9u); ln_changed(2, 20);
    run(ST_SAVE_DELAY_MS - 200u); assert(st_records() == base); /* 3 s after the last change */
    ln.lanes[2].steps[21] = on_step(10u); ln_changed(2, 21);
    run(ST_SAVE_DELAY_MS - 100u); assert(st_records() == base);
    run(300); assert(st_records() == base + 2u && !ln_dirty); /* lane 1 chunk 1 and lane 3 chunk 2 only */
    /* A step on then off again in a chunk never stored: nothing written. */
    ln.lanes[3].steps[63] = on_step(1u); ln_changed(3, 63); run(100);
    memset(&ln.lanes[3].steps[63], 0, sizeof(ln_step)); ln_changed(3, 63);
    run(ST_SAVE_DELAY_MS + 100u); assert(st_records() == base + 2u);
    /* The next start has the pattern. */
    ln_pattern before = ln;
    ln_clear(&ln); tc_controls c; tc_init(&c); (void)st_load(&c);
    assert(!memcmp(&before, &ln, sizeof ln));
    /* SysEx: a default chunk is absent, a stored one as it plays; a new one plays and is stored at once. */
    uint32_t w[ST_PAYLOAD];
    assert(st_get(ST_LANE, 1, &tc_state, w) == ST_ABSENT && w[0] == LN_VERSION);
    assert(st_get(ST_LANE, 9, &tc_state, w) == ST_OK);
    uint32_t mine[ST_PAYLOAD]; ln_encode(&ln, 9, mine); assert(!memcmp(w, mine, sizeof w));
    ln_pattern edit = ln; edit.lanes[1].steps[5] = on_step(44u); edit.lanes[1].kind = LN_DRUMS; ln_set_length(&edit.lanes[1], 32u);
    ln_encode(&edit, 4, w);
    uint32_t n = st_records();
    assert(st_put(ST_LANE, 4, w, &tc_state, &e) == ST_OK && st_records() == n + 1u);
    assert(ln.lanes[1].kind == LN_DRUMS && ln.lanes[1].steps[5].on && ln_length(&ln.lanes[1]) == 32u);
    w[37] = 1u; before = ln;
    assert(st_put(ST_LANE, 4, w, &tc_state, &e) == ST_BAD_VALUE && !memcmp(&before, &ln, sizeof ln));
    ln_encode(&ln, 0, w);
    assert(st_put(ST_LANE, LN_RECORDS, w, &tc_state, &e) == ST_BAD_VALUE && st_get(ST_LANE, LN_RECORDS, &tc_state, w) == ST_BAD_VALUE);
    /* Compaction at the next start keeps the pattern and only its current records. */
    before = ln;
    ln_encode(&ln, 0, w);
    for (uint32_t i = 0; i < 400u; ++i) assert(st_put(ST_LANE, 0, w, &tc_state, &e) == ST_OK);
    ln_clear(&ln); tc_init(&c); (void)st_load(&c);
    assert(!memcmp(&before, &ln, sizeof ln) && st_records() < 20u);
    ln_clear(&ln); tc_init(&c); (void)st_load(&c); assert(!memcmp(&before, &ln, sizeof ln));
    printf("storage: saved 3 s after the last edit, only changed chunks, default chunks never written, reloaded, "
           "SysEx get/put (absent, refused, out of range), compaction keeps the pattern; ");
}
/* --- the step screen ------------------------------------------------------------------------- */
static void draw(void) { tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now); }
static void screen(void) {
    fresh();
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_SEQUENCE; tc_state.menu = TC_MENU_STEPS;
    draw(); assert(!strcmp(at(0, 56), "<BACK   O OPEN"));
    nav(NAV_CENTRE); assert(tc_state.ui == TC_UI_STEPS && tc_state.lane == 0u && !tc_state.step_page);
    draw();
    assert(!strcmp(at(0, 0), "L1 CHORDS 16 1/16") && !strcmp(at(0, 11), "PAGE 1-48 CH1") && !strcmp(at(0, 56), "<BACK  O REC  >49-64"));
    assert(!strcmp(at(0, 22), "PICKED: C") && !strcmp(at(0, 45), "STOPPED"));
    /* Tap a step: on with the value picked (the first chord), again: off. Silent: the right half is the steps. */
    touch(step_pad(5)); assert(ln.lanes[0].steps[5].on && ln.lanes[0].steps[5].value == 3u && !heard() && !nons);
    assert(tc_state.last_action == TC_STEP_ON && (ln_dirty & 1u));
    draw(); assert(!strcmp(at(0, 34), "STEP 6 ON"));
    touch(step_pad(5)); assert(!ln.lanes[0].steps[5].on && tc_state.last_action == TC_STEP_OFF);
    /* Step 13: the second row. */
    touch(step_pad(12)); assert(ln.lanes[0].steps[12].on && step_pad(12) == 49u);
    /* Past the lane's end (16 steps): nothing. */
    touch(step_pad(36)); assert(!ln.lanes[0].steps[36].on && tc_state.last_action == TC_STEP_PAST);
    /* Hold a step, touch a chord pad: the step plays it (heard), and lifting the step leaves it on. */
    hold(step_pad(7)); assert(tc_state.step_held == 7u);
    draw(); assert(!strcmp(at(0, 22), "STEP 8: OFF"));
    touch(23u); assert(nons); /* the chord sounds */
    assert(ln.lanes[0].steps[7].on && ln.lanes[0].steps[7].value == 23u && tc_state.last_action == TC_STEP_SET);
    draw(); assert(!strcmp(at(0, 22), "STEP 8: Am"));
    lift(step_pad(7)); assert(ln.lanes[0].steps[7].on && tc_state.step_held == LN_NONE);
    /* The value picked last is what a tapped step gets. */
    touch(step_pad(9)); assert(ln.lanes[0].steps[9].value == 23u);
    /* Step record: centre; each chord pad fills the cursor's step and moves on; right a rest; left takes back. */
    nav(NAV_CENTRE); assert(tc_state.step_rec && tc_state.step_cursor == 0u);
    draw(); assert(!strcmp(at(0, 56), "REC 1/16 >REST O END") && !strcmp(at(0, 34), "REC: PLAY THE PADS"));
    touch(7u); touch(11u); nav(NAV_RIGHT); touch(15u);
    assert(ln.lanes[0].steps[0].value == 7u && ln.lanes[0].steps[1].value == 11u && !ln.lanes[0].steps[2].on &&
           ln.lanes[0].steps[3].value == 15u && tc_state.step_cursor == 4u);
    nav(NAV_LEFT); assert(!ln.lanes[0].steps[3].on && tc_state.step_cursor == 3u && tc_state.last_action == TC_STEP_BACK);
    for (uint32_t i = 0; i < 13u; ++i) touch(3u);
    assert(tc_state.step_cursor == 0u); /* round the 16 steps */
    nav(NAV_CENTRE); assert(!tc_state.step_rec && tc_state.last_action == TC_STEP_STOPPED);
    /* A centre button set to Record does the same here. */
    tc_state.buttons[0] = TC_ACT_RECORD; raw[0] |= 1u << 6; run(50); raw[0] &= ~(1u << 6); run(50);
    assert(tc_state.step_rec && !tc_state.recording);
    nav(NAV_CENTRE); assert(!tc_state.step_rec); tc_state.buttons[0] = 0u;
    /* Up and down choose the lane, round the four. */
    nav(NAV_DOWN); assert(tc_state.lane == 1u); draw(); assert(!strcmp(at(0, 0), "L2 CHORDS 16 1/16"));
    nav(NAV_UP); nav(NAV_UP); assert(tc_state.lane == 3u);
    /* A melody lane: the left half plays the right half's Scale layout; values are positions. */
    ln.lanes[3].kind = LN_MELODY; nons = 0u;
    hold(3u); assert(nons == 1u && ons[0].note == 48u && ons[0].ch == tc_state.melody_ch); lift(3u);
    touch(step_pad(0)); assert(ln.lanes[3].steps[0].value == 3u); /* picked by touching pad 3 */
    draw(); assert(!strcmp(at(0, 0), "L4 MELODY 16 1/16") && !strcmp(at(0, 22), "PICKED: C3"));
    /* A drum lane: notes from 36 at the bottom left, on the drum channel. */
    ln.lanes[3].kind = LN_DRUMS; nons = 0u;
    hold(7u); assert(nons == 1u && ons[0].note == 40u && ons[0].ch == tc_state.drum_ch); lift(7u);
    draw(); assert(!strcmp(at(0, 22), "PICKED: E2 40") && !strcmp(at(0, 11), "PAGE 1-48 CH10"));
    /* Pages: right shows 49-64 (past 16 steps nothing); with 64 steps a step there goes on. */
    nav(NAV_RIGHT); assert(tc_state.step_page == 1u); draw(); assert(!strcmp(at(0, 56), "<1-48      O REC"));
    touch(step_pad(0)); assert(!ln.lanes[3].steps[48].on && tc_state.last_action == TC_STEP_PAST);
    ln_set_length(&ln.lanes[3], 64u); touch(step_pad(0)); assert(ln.lanes[3].steps[48].on && ln.lanes[3].steps[48].value == 40u);
    nav(NAV_LEFT); assert(tc_state.step_page == 0u && tc_state.ui == TC_UI_STEPS);
    /* Playing: the playhead on screen and on the pads (white). */
    tc_state.transport = true; run(130);
    draw(); assert(!strcmp(at(0, 45), "PLAYING 2/64"));
    assert(pad_led[step_pad(1)].r == pad_led[step_pad(1)].g && pad_led[step_pad(1)].g == pad_led[step_pad(1)].b && pad_led[step_pad(1)].r);
    assert(pad_led[step_pad(13)].r < pad_led[step_pad(1)].r); /* an off step: dim */
    tc_state.transport = false; run(100); quiet();
    /* Pads past the end are dark. */
    ln_set_length(&ln.lanes[3], 20u); draw();
    assert(!pad_led[step_pad(20)].r && !pad_led[step_pad(20)].g && pad_led[step_pad(20)].b <= 1u);
    /* Left on the first page goes back to the Sequence page. */
    nav(NAV_LEFT); assert(tc_state.ui == TC_UI_MENU && tc_state.menu == TC_MENU_STEPS);
    printf("step screen: tap on/off, rows, past the end, hold + pad sets and plays, picked value, step record "
           "with rest and take back, the Record button, lanes, melody and drum left halves, pages, playhead, "
           "lights, back; ");
}
static void menu(void) {
    fresh();
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_SEQUENCE;
    /* Lane picks the lane the next items set. */
    tc_state.menu = TC_MENU_LANE; nav(NAV_CENTRE); nav(NAV_UP); nav(NAV_UP); nav(NAV_CENTRE); assert(tc_state.lane == 2u);
    tc_state.menu = TC_MENU_LANE_KIND; nav(NAV_CENTRE); nav(NAV_UP); nav(NAV_UP); nav(NAV_CENTRE);
    assert(ln.lanes[2].kind == LN_DRUMS && tc_state.brush[2].value == TP_DRUM_BASE); /* a new kind picks from its default */
    tc_state.menu = TC_MENU_LANE_LENGTH; nav(NAV_CENTRE); for (int i = 0; i < 5; ++i) nav(NAV_DOWN); nav(NAV_CENTRE);
    assert(ln_length(&ln.lanes[2]) == 11u);
    tc_state.menu = TC_MENU_LANE_RATE; nav(NAV_CENTRE); nav(NAV_UP); nav(NAV_CENTRE); assert(ln_rate(&ln.lanes[2]) == 4u); /* 1/16T */
    tc_state.menu = TC_MENU_LANE_CHANNEL; nav(NAV_CENTRE); nav(NAV_UP); nav(NAV_UP); nav(NAV_CENTRE); assert(ln.lanes[2].channel == 2u);
    tc_state.menu = TC_MENU_LANE_MUTE; nav(NAV_CENTRE); nav(NAV_UP); nav(NAV_CENTRE); assert(ln.lanes[2].mute == 1u);
    assert(ln_dirty == 1u << 8 && ln.lanes[0].kind == LN_CHORDS && !ln.lanes[1].mute);
    draw(); assert(!strcmp(at(0, 56), "SETTING CHANGED"));
    tc_state.menu = TC_MENU_LANE; draw();
    /* Stored with the pattern (no live settings change for them). */
    run(ST_SAVE_DELAY_MS + 100u); assert(!ln_dirty);
    ln_pattern before = ln; ln_clear(&ln); tc_controls c; tc_init(&c); (void)st_load(&c); assert(!memcmp(&before, &ln, sizeof ln));
    /* Transport with only a pattern: PLAY, not NO PROGRESSION YET. */
    tc_state.menu = TC_MENU_TRANSPORT; nav(NAV_CENTRE); assert(tc_state.transport && tc_state.last_action == TC_SEQ_EMPTY);
    nav(NAV_CENTRE); ln.lanes[1].steps[0] = on_step(3u);
    nav(NAV_CENTRE); assert(tc_state.transport && tc_state.last_action == TC_TRANSPORT);
    nav(NAV_CENTRE); run(100); quiet();
    printf("menu: Lane, Kind, Length, Rate, Channel, Mute set the chosen lane and are stored with it; transport message\n");
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    (void)ui_init();
    defaults(); round_trip(); locate();
    transport(); polymeter(); daw(); sounds();
    storage(); screen(); menu();
    printf("PASS: lanes\n");
    return 0;
}
