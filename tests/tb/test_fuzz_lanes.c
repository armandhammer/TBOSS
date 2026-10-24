/* SPDX-License-Identifier: GPL-3.0-or-later - 1.5.0 step sequencer stress (seeded, so a failure
 * reproduces): random pattern records (any decoded record encodes back to itself; a refused one
 * changes nothing), then seeded play on the real code: random patterns and lane settings changed while
 * playing, the step screen's pads and buttons, the board's transport and a DAW (Start, Stop,
 * Continue, Song Position Pointer, its clock coming and going), tempo changes, all notes off. Every
 * millisecond the engine's voices match its sources and what was sent, every lane's step is inside
 * its length and the step screen's state in range; every round ends silent; the same seed plays the
 * same packets again (randomness comes from the seed, never from a stored state); the pattern saved
 * is the pattern played. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
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
static uint32_t seed;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
/* --- records --------------------------------------------------------------------------------- */
static uint32_t decoded, refused;
static void records(void) {
    seed = 0x7A11E5u;
    ln_pattern p; ln_clear(&p);
    for (uint32_t r = 0; r < 200000u; ++r) {
        uint32_t w[LN_WORDS], slot = rnd(LN_RECORDS + 2u);
        if (rnd(4u)) { /* a real record, a few bits or words changed */
            ln_pattern q; ln_clear(&q);
            uint32_t s = slot % LN_RECORDS;
            ln_lane *l = &q.lanes[s / LN_CHUNKS];
            l->kind = (uint8_t)rnd(LN_KINDS); l->channel = (uint8_t)rnd(LN_CHANNELS); l->length = (uint8_t)rnd(LN_STEPS + 1u);
            l->rate = (uint8_t)rnd(LN_RATES + 1u); l->mute = (uint8_t)rnd(2u); q.seed = (uint16_t)rnd(65536u);
            for (uint32_t i = 0; i < LN_CHUNK; ++i) {
                ln_step *st = &l->steps[s % LN_CHUNKS * LN_CHUNK + i];
                st->on = rnd(2u); st->value = (uint8_t)rnd(128u); st->velocity = (uint8_t)rnd(128u);
                st->edit = (uint8_t)(rnd(8u) | rnd(3u) << 3 | rnd(8u) << 5); st->scale = (uint8_t)rnd(TC_ALL_SCALES + 1u);
            }
            ln_encode(&q, s, w);
            for (uint32_t m = rnd(3u); m; --m) {
                if (rnd(2u)) w[rnd(LN_WORDS)] ^= 1u << rnd(32u); else w[rnd(LN_WORDS)] = rnd(UINT32_MAX);
            }
        } else for (uint32_t i = 0; i < LN_WORDS; ++i) w[i] = rnd(3u) ? 0u : rnd(UINT32_MAX);
        if (!rnd(3u)) w[0] = LN_VERSION;
        ln_pattern before = p;
        if (ln_decode(w, slot, &p)) {
            uint32_t back[LN_WORDS]; ln_encode(&p, slot, back);
            assert(slot < LN_RECORDS && !memcmp(w, back, sizeof w)); /* the stored form is exact */
            const ln_lane *l = &p.lanes[slot / LN_CHUNKS];
            assert(l->kind < LN_KINDS && l->channel < LN_CHANNELS && l->length <= LN_STEPS && l->rate <= LN_RATES &&
                   ln_length(l) >= 1u && ln_length(l) <= LN_STEPS && ln_rate(l) < LN_RATES);
            decoded++;
        } else { assert(!memcmp(&before, &p, sizeof p)); refused++; }
    }
    assert(decoded >= 20000u && refused >= 20000u);
}
/* --- play ------------------------------------------------------------------------------------ */
static tp_engine e;
static ts_scan scan;
static uint32_t now;
static uint8_t sounding[16][128];
static uint32_t packets, ons, log_hash, lane_passes, lane_ons, screen_passes, rounds_quiet;
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u);
    uint8_t kind = p[0], ch = p[1] & 15u;
    assert((p[1] >> 4) == kind || kind == 0x0Fu);
    if (kind == TK_ON) { assert(!sounding[ch][p[2]] && p[3] >= 1u && p[3] <= 127u); sounding[ch][p[2]] = 1u; ons++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
    packets++;
    for (uint32_t i = 0; i < 4u; ++i) log_hash = (log_hash ^ p[i]) * 16777619u; /* FNV-1a */
    log_hash = (log_hash ^ now) * 16777619u;
    return true;
}
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4], nav_left;
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
static uint32_t daw_us, daw_next_us;
static void midi(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { const uint8_t p[4] = {a, b, c, d}; tm_packet(p, now); }
static void invariants(void) {
    static uint32_t refs[16][128];
    memset(refs, 0, sizeof refs);
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        const tp_key *key = &e.keys[k];
        assert(key->captured.count <= TP_MAX_NOTES && key->channel < 16u);
        if (!key->admitted || key->silent) continue;
        for (uint32_t i = 0; i < key->captured.count; ++i) refs[key->channel][key->captured.notes[i] & 127u]++;
    }
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) {
        const tp_voice *v = &e.voices[c][p];
        assert(v->refs == refs[c][p]);
        if (sounding[c][p]) assert(v->exposed);
        if (v->exposed && !sounding[c][p]) assert(e.pending_id && e.pending_kind == TK_OFF);
    }
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        const ln_lane *l = &ln.lanes[i];
        assert(l->kind < LN_KINDS && l->channel < LN_CHANNELS && ln_length(l) <= LN_STEPS);
        assert(e.lane_step[i] == LN_NONE || e.lane_step[i] < ln_length(l) || !e.lane_running ||
               e.lane_count[i] == UINT32_MAX);
        if (e.keys[TP_LANE0 + i].admitted) { lane_passes++; assert(!e.keys[TP_LANE0 + i].silent); }
    }
    const tc_controls *c = &tc_state;
    assert(c->lane < LN_LANES && c->step_page < 2u && c->step_cursor < LN_STEPS && (c->step_held == LN_NONE || c->step_held < LN_STEPS));
    assert(c->ui <= TC_UI_STEPS && !e.errors && e.state == TK_ACTIVE);
    if (c->ui == TC_UI_STEPS) screen_passes++;
}
static void tick(void) {
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
    if (e.state == TK_ACTIVE) invariants();
    now++;
}
static void random_lane(uint32_t i) {
    ln_lane *l = &ln.lanes[i];
    l->kind = (uint8_t)(rnd(5u) ? rnd(3u) : LN_OFF); l->channel = (uint8_t)(rnd(3u) ? 0u : rnd(LN_CHANNELS));
    ln_set_length(l, 1u + rnd(LN_STEPS)); ln_set_rate(l, rnd(LN_RATES)); l->mute = !rnd(6u);
    for (uint32_t s = 0; s < LN_STEPS; ++s) {
        ln_step *st = &l->steps[s];
        memset(st, 0, sizeof *st);
        if (rnd(3u)) continue;
        st->on = true; st->value = (uint8_t)(l->kind == LN_DRUMS ? 30u + rnd(60u) : rnd(rnd(8u) ? 48u : 128u));
        st->velocity = (uint8_t)rnd(128u);
        if (!rnd(4u)) st->edit = (uint8_t)(rnd(8u) | rnd(3u) << 3 | rnd(8u) << 5);
        if (!rnd(6u)) st->scale = (uint8_t)(1u + rnd(TC_ALL_SCALES)); /* borrowed (or an empty user slot) */
    }
    ln_changed(i, LN_STEPS);
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(i, s);
}
static void random_event(void) {
    tc_controls *c = &tc_state;
    switch (rnd(16u)) {
    case 0: random_lane(rnd(LN_LANES)); break;
    case 1: case 2: case 3: case 4: { /* a lane's length, rate, mute or kind while it plays */
        uint32_t i = rnd(LN_LANES), what = rnd(4u);
        ln_lane *l = &ln.lanes[i];
        if (!what) ln_set_length(l, 1u + rnd(LN_STEPS));
        else if (what == 1u) ln_set_rate(l, rnd(LN_RATES));
        else if (what == 2u) l->mute = !l->mute;
        else l->kind = (uint8_t)rnd(LN_KINDS);
        ln_changed(i, LN_STEPS);
        break;
    }
    case 5: c->transport = rnd(5u) != 0u; c->recording = false; break; /* mostly running (the buttons may have started a recording) */
    case 6: midi(0x0Fu, rnd(3u) ? 0xFAu : rnd(2u) ? 0xFBu : 0xFCu, 0u, 0u); break;              /* Start, Continue, Stop */
    case 7: midi(0x03u, 0xF2u, (uint8_t)rnd(128u), (uint8_t)rnd(4u)); break;                      /* Song Position Pointer */
    case 8: daw_us = rnd(3u) ? 0u : 15000u + rnd(15000u); daw_next_us = now * 1000u; break;     /* its clock comes and goes */
    case 9: c->tempo = (uint8_t)rnd(TC_TEMPOS); break;
    case 10: c->ui = c->ui == TC_UI_STEPS ? (uint8_t)TC_UI_PLAY : (uint8_t)TC_UI_STEPS; c->step_held = LN_NONE; break;
    case 11: if (!nav_left) { raw[1] |= 1u << (const uint8_t[]){8, 7, 6, 1, 3}[rnd(5u)]; nav_left = 50u; } break;
    case 12: { /* a pad: touched or let go (at most a few held) */
        uint32_t k = rnd(TS_KEYS), held = 0;
        for (uint32_t j = 0; j < TS_KEYS; ++j) held += scan.values[j] != 0u;
        scan.values[k] = scan.values[k] || held >= 3u ? 0u : 1500u + rnd(2500u);
        break;
    }
    case 13: c->panic = !rnd(4u); break;
    case 14: { tp_layout l = tc_layout(c); l.tonic = (uint8_t)rnd(12u); if (rnd(2u)) l.mode = tc_mode_scale(c, rnd(9u)); (void)tc_set_layout(c, &e, l); break; }
    default: c->strum = (uint8_t)rnd(TC_STRUM_MODES); c->vel_mode = (uint8_t)rnd(TC_VEL_MODES); c->melody_vel = (uint8_t)rnd(TC_VEL_MODES); break;
    }
}
static void quiet_round(void) {
    tc_state.transport = false; midi(0x0Fu, 0xFCu, 0u, 0u); daw_us = 0u; memset(scan.values, 0, sizeof scan.values);
    raw[1] = 0u; nav_left = 0u;
    tc_state.hold = 0u; /* right on the play screen toggles it: chords held on would sound on */
    for (uint32_t i = 0; i < 600u; ++i) tick();
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[c][p]);
    assert(!tp_obligations(&e));
    for (uint32_t i = 0; i < LN_LANES; ++i) assert(!e.keys[TP_LANE0 + i].admitted && !e.lane_running);
    rounds_quiet++;
}
/* One seeded session: rounds of random play, each ending silent. The packets' hash. */
static uint32_t session(uint32_t s, uint32_t rounds) {
    seed = s; now = 1000u; log_hash = 2166136261u;
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw); memset(sounding, 0, sizeof sounding);
    sq_clear(&sq, SQ_UNIT_BAR, 0u); daw_us = 0u; nav_left = 0u;
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    for (uint32_t i = 0; i < 600u; ++i) tick();
    for (uint32_t i = 0; i < LN_LANES; ++i) random_lane(i);
    tc_state.transport = true;
    for (uint32_t r = 0; r < rounds; ++r) {
        for (uint32_t i = 0; i < 5000u; ++i) {
            if (nav_left && !--nav_left) raw[1] = 0u;
            if (!rnd(60u)) random_event();
            tick();
        }
        quiet_round();
    }
    /* What is played is what is kept: the next start loads the same pattern. */
    for (uint32_t i = 0; i < ST_SAVE_DELAY_MS + 100u; ++i) tick();
    assert(!ln_dirty);
    ln_pattern played = ln; ln_clear(&ln);
    tc_controls c; tc_init(&c); (void)st_load(&c);
    assert(!memcmp(&played, &ln, sizeof ln));
    return log_hash;
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    records();
    uint32_t hashes[3];
    for (uint32_t s = 0; s < 3u; ++s) hashes[s] = session(0xC0FFEEu + 7919u * s, 12u);
    uint32_t again = session(0xC0FFEEu, 12u); /* the same seed: the same packets */
    assert(again == hashes[0] && hashes[0] != hashes[1]);
    lane_ons = ons;
    assert(packets >= 2000u && ons >= 1000u && lane_passes >= 10000u && screen_passes >= 1000u && rounds_quiet == 48u);
    printf("PASS: lanes stress: %u random records (%u decoded, each encoding back to itself; %u refused, nothing "
           "changed); 4 seeded sessions of 12 rounds: %u packets (%u Note On), lanes sounding on %u passes, the step "
           "screen open on %u, patterns, lane settings, pads, buttons, transport, DAW Start/Stop/Continue/SPP and "
           "clock loss, tempo, all notes off; voices always match, every round ends silent, the same seed plays the "
           "same packets, the pattern played is the pattern stored\n",
           decoded + refused, decoded, refused, packets, lane_ons, lane_passes, screen_passes);
    return 0;
}
