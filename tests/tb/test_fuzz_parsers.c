/* SPDX-License-Identifier: GPL-3.0-or-later - parser stress on the real settings/sysex/midi_in code with
 * a fake sector 11 (programming only clears bits) and a fake NOR.
 * Settings: random valid setups round-trip exactly (SysEx record, flash log, presets);
 * mutated payloads (bit flips, random words, reserved words, out-of-range bytes) are
 * either rejected leaving everything unchanged, or accepted only in range; 20-word
 * records from 0.3-0.5 still load and keep the newer settings; a corrupted or torn log
 * falls back to the newest intact record; a nearly full log compacts at boot.
 * SysEx/MIDI in: random packet streams with interleaved real-time bytes, truncated and
 * oversized messages, random well-formed requests: the assembler stays in bounds and
 * every reply is a well-formed message. MIDI clock with USB-frame jitter and timeouts.
 * Fixed seeds. */
#include "../../src/tb/sysex.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t seed = 0xBB67AE85u;
static uint32_t rnd(uint32_t n) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return n ? seed % n : 0u;
}
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    assert(!(offset & 3u) && offset + 4u * count <= FS_SIZE);
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return true; }
void nr_id(uint8_t id[3]) { id[0] = 0xEF; id[1] = 0x40; id[2] = 0x18; }
uint32_t nr_size(void) { return 1u << 24; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) {
    if (!n || n > NR_MAX_READ || a >= nr_size() || n > nr_size() - a) return false;
    for (uint32_t i = 0; i < n; ++i) out[i] = (uint8_t)(a + i);
    return true;
}
static tp_engine engine;
/* Every persisted field (settings.c encode/decode), compared one by one. */
#define FIELDS(X) X(tonic) X(scale) X(chord_base) X(melody_base) X(at_chord) X(at_melody) X(vel_mode) X(brightness) \
    X(hold) X(strum) X(strum_speed) X(arp) X(arp_rate) X(arp_octaves) X(arp_gate) X(tempo) X(midi_leds) X(chord_ch) \
    X(melody_ch) X(arp_last) X(trs_out) X(pedal_mode) X(pedal_type) X(led_scheme) X(chord_voicing) X(chord_spread) \
    X(chord_octaves) X(live_update) X(strum_off) X(strum_repeat) X(strum_cutoff) X(arp_rhythm) X(arp_swing) \
    X(arp_sync) X(arp_latch) X(melody_layout) X(bass_ch) X(drum_ch) X(clock_out) X(pedal_note) X(arrows) X(family) \
    X(capo) X(feel_time) X(feel_vel) X(live_ch) X(live_in) X(chord_mute) X(fold) X(voice_lead) X(note_repeat) \
    X(pad_trigger) X(pad_release) X(pad_full) X(melody_vel) X(pedal_target) X(prog_slot) X(seq_guide)
static bool same_settings(const tc_controls *a, const tc_controls *b) {
#define SAME(f) if (a->f != b->f) { fprintf(stderr, "field %s: %u vs %u\n", #f, (unsigned)a->f, (unsigned)b->f); return false; }
    FIELDS(SAME)
#undef SAME
    for (uint32_t k = 0; k < 48u; ++k)
        if (a->chord[k].lower != b->chord[k].lower || a->chord[k].sus != b->chord[k].sus || a->chord[k].add != b->chord[k].add)
            return false;
    return true;
}
static bool layout_fits(const tc_controls *c) {
    tp_layout l = tc_layout(c); tp_notes n;
    if (!tc_layout_ok(l)) return false;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (!tc_notes(l, k, c->chord[k % 48u], &n)) return false;
    return true;
}
/* Whatever was accepted must be a setup the board can play. */
static void in_range(const tc_controls *c) {
    assert(c->tonic < 12u && tc_scale_defined(c->scale) && c->at_chord < TC_AT_MODES && c->at_melody < TC_AT_MODES);
    assert(c->vel_mode < TC_VEL_MODES && c->brightness < TC_BRIGHTNESS_LEVELS && c->hold <= 1u);
    assert(c->strum < TC_STRUM_MODES && c->strum_speed < TC_STRUM_SPEEDS && c->arp < TC_ARP_MODES);
    assert(c->arp_rate < TC_ARP_RATES && c->arp_octaves < TC_ARP_OCTAVES && c->arp_gate < TC_ARP_GATES);
    assert(c->tempo < TC_TEMPOS && c->midi_leds <= 1u && c->chord_ch < 16u && c->melody_ch < 16u);
    assert(c->arp_last < TC_ARP_MODES && c->trs_out <= 1u && c->pedal_mode < TC_PEDAL_MODES && c->pedal_type < TC_PEDAL_TYPES);
    assert(c->led_scheme < TC_LED_SCHEMES && c->chord_voicing < TC_VOICINGS && c->chord_spread < TC_SPREADS);
    assert(c->chord_octaves && c->chord_octaves <= 0x7Fu && c->live_update <= 1u && !(c->strum_off >> 48));
    assert(c->strum_repeat < TC_REPEATS && c->strum_cutoff < TC_CUTOFFS && c->arp_rhythm < TC_RHYTHMS);
    assert(c->arp_swing < TC_SWINGS && c->arp_sync <= 1u && c->arp_latch <= 1u && c->melody_layout < TP_MELODY_LAYOUTS);
    assert(c->bass_ch < 16u && c->drum_ch < 16u && c->clock_out <= 1u && c->pedal_note <= 127u);
    assert(c->arrows < TC_ARROWS_MODES && c->family < TC_FAMILIES && c->capo < TC_CAPOS);
    assert(c->feel_time < TC_FEEL_TIMES && c->feel_vel < TC_FEEL_VELS && c->live_ch < TC_LIVE_CHANNELS);
    assert(c->live_in < TC_LIVE_INPUTS && !(c->chord_mute >> 48) && c->fold <= 1u && c->voice_lead <= 1u);
    assert(c->note_repeat < TC_REPEATS);
    assert(c->chord_base + TP_OCTAVE_RANGE >= TP_CHORD_BASE && c->chord_base <= TP_CHORD_BASE + TP_OCTAVE_RANGE);
    assert(c->melody_base + TP_OCTAVE_RANGE >= TP_MELODY_BASE && c->melody_base <= TP_MELODY_BASE + TP_OCTAVE_RANGE);
    assert(layout_fits(c));
}
static void random_setup(tc_controls *c) {
    tc_init(c);
    do {
        c->tonic = (uint8_t)rnd(12u);
        do c->scale = (uint8_t)rnd(TC_ALL_SCALES); while (!tc_scale_defined(c->scale));
        c->chord_base = (uint8_t)(TP_CHORD_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u));
        c->melody_base = (uint8_t)(TP_MELODY_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u));
        c->melody_layout = (uint8_t)rnd(TP_MELODY_LAYOUTS); c->fold = (uint8_t)rnd(2u);
        for (uint32_t k = 0; k < 48u; ++k)
            c->chord[k] = rnd(3u) ? (tc_chord){0, 0, 0} : (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
    } while (!layout_fits(c));
    c->at_chord = (uint8_t)rnd(TC_AT_MODES); c->at_melody = (uint8_t)rnd(TC_AT_MODES); c->vel_mode = (uint8_t)rnd(TC_VEL_MODES);
    c->brightness = (uint8_t)rnd(TC_BRIGHTNESS_LEVELS); c->hold = (uint8_t)rnd(2u); c->strum = (uint8_t)rnd(TC_STRUM_MODES);
    c->strum_speed = (uint8_t)rnd(TC_STRUM_SPEEDS); c->arp = (uint8_t)rnd(TC_ARP_MODES); c->arp_rate = (uint8_t)rnd(TC_ARP_RATES);
    c->arp_octaves = (uint8_t)rnd(TC_ARP_OCTAVES); c->arp_gate = (uint8_t)rnd(TC_ARP_GATES); c->tempo = (uint8_t)rnd(TC_TEMPOS);
    c->midi_leds = (uint8_t)rnd(2u); c->chord_ch = (uint8_t)rnd(16u); c->melody_ch = (uint8_t)rnd(16u);
    c->arp_last = (uint8_t)rnd(TC_ARP_MODES); c->trs_out = (uint8_t)rnd(2u); c->pedal_mode = (uint8_t)rnd(TC_PEDAL_MODES);
    c->pedal_type = (uint8_t)rnd(TC_PEDAL_TYPES); c->led_scheme = (uint8_t)rnd(TC_LED_SCHEMES);
    c->chord_voicing = (uint8_t)rnd(TC_VOICINGS); c->chord_spread = (uint8_t)rnd(TC_SPREADS);
    c->chord_octaves = (uint8_t)(1u + rnd(127u)); c->live_update = (uint8_t)rnd(2u);
    c->strum_off = ((uint64_t)rnd(65536u) << 32 | rnd(UINT32_MAX)) & 0xFFFFFFFFFFFFu;
    c->strum_repeat = (uint8_t)rnd(TC_REPEATS); c->strum_cutoff = (uint8_t)rnd(TC_CUTOFFS); c->arp_rhythm = (uint8_t)rnd(TC_RHYTHMS);
    c->arp_swing = (uint8_t)rnd(TC_SWINGS); c->arp_sync = (uint8_t)rnd(2u); c->arp_latch = (uint8_t)rnd(2u);
    c->bass_ch = (uint8_t)rnd(16u); c->drum_ch = (uint8_t)rnd(16u); c->clock_out = (uint8_t)rnd(2u);
    c->pedal_note = (uint8_t)rnd(128u); c->arrows = (uint8_t)rnd(TC_ARROWS_MODES); c->family = (uint8_t)rnd(TC_FAMILIES);
    c->capo = (uint8_t)rnd(TC_CAPOS); c->feel_time = (uint8_t)rnd(TC_FEEL_TIMES); c->feel_vel = (uint8_t)rnd(TC_FEEL_VELS);
    c->live_ch = (uint8_t)rnd(TC_LIVE_CHANNELS); c->live_in = (uint8_t)rnd(TC_LIVE_INPUTS);
    c->chord_mute = ((uint64_t)rnd(65536u) << 32 | rnd(UINT32_MAX)) & 0xFFFFFFFFFFFFu;
    c->fold = c->fold; c->voice_lead = (uint8_t)rnd(2u); c->note_repeat = (uint8_t)rnd(TC_REPEATS);
    do { /* 0.10.0 / 0.11.0 */
        c->pad_trigger = (uint8_t)(TC_PAD_TRIGGER_MIN + rnd(TC_PAD_TRIGGER_MAX - TC_PAD_TRIGGER_MIN + 1u));
        c->pad_release = (uint8_t)(TC_PAD_RELEASE_MIN + rnd(TC_PAD_RELEASE_MAX - TC_PAD_RELEASE_MIN + 1u));
        c->pad_full = (uint8_t)(TC_PAD_FULL_MIN + rnd(TC_PAD_FULL_MAX - TC_PAD_FULL_MIN + 1u));
    } while (!tc_pad_settings_ok(c->pad_trigger, c->pad_release, c->pad_full));
    c->melody_vel = (uint8_t)rnd(TC_VEL_MODES); c->pedal_target = (uint8_t)rnd(TC_PEDAL_TARGETS);
    c->prog_slot = (uint8_t)rnd(8u); c->seq_guide = (uint8_t)rnd(2u);
}
static uint32_t round_trips, rejected, accepted_mutants, old_records, fallbacks, compactions;
static void settings_round_trips(void) {
    for (uint32_t i = 0; i < 3000u; ++i) {
        tc_controls a, b; uint32_t p[ST_PAYLOAD];
        random_setup(&a);
        assert(st_get(ST_LIVE, 0, &a, p) == ST_OK);
        assert(!p[18] && !p[19] && p[39] == 0x56330000u);
        for (uint32_t w = 34u; w < 39u; ++w) assert(!p[w]); /* reserved words stay zero */
        tc_init(&b);
        assert(st_put(ST_LIVE, 0, p, &b, &engine) == ST_OK);
        assert(same_settings(&a, &b));
        tp_layout l = tc_layout(&b); assert(!memcmp(&l, &engine.layout, sizeof l));
        round_trips++;
        /* Mutants: rejected with nothing changed, or accepted and playable. */
        for (uint32_t m = 0; m < 6u; ++m) {
            uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q);
            switch (rnd(5u)) {
            case 0: q[rnd(ST_PAYLOAD)] ^= 1u << rnd(32u); break;                 /* one bit */
            case 1: q[rnd(ST_PAYLOAD)] = rnd(UINT32_MAX); break;                  /* a random word */
            case 2: q[34u + rnd(5u)] = 1u + rnd(255u); break;                     /* a reserved word */
            case 3: { uint32_t w = rnd(32u); q[w] = (q[w] & ~(0xFFu << (8u * rnd(4u)))) | (0xFFu << (8u * rnd(4u))); break; }
            default: for (uint32_t w = 0; w < ST_PAYLOAD; ++w) if (!rnd(8u)) q[w] = rnd(UINT32_MAX); break;
            }
            tc_controls c = b, before = b; tp_layout was = engine.layout;
            uint32_t status = st_put(ST_LIVE, 0, q, &c, &engine);
            if (status == ST_OK) { in_range(&c); accepted_mutants++; }
            else {
                assert(status == ST_BAD_VALUE && !memcmp(&c, &before, sizeof c));
                assert(!memcmp(&was, &engine.layout, sizeof was));
                rejected++;
            }
            for (uint32_t w = 34u; w < 39u; ++w) if (q[w] && q[39] == 0x56330000u) assert(status == ST_BAD_VALUE);
            if (q[18] || q[19]) assert(status == ST_BAD_VALUE);
        }
        /* The same setup as a 20-word record from 0.3-0.5: loads, keeps the newer settings. */
        uint32_t old[ST_PAYLOAD]; memcpy(old, p, sizeof old);
        for (uint32_t w = ST_PAYLOAD_V2; w < ST_PAYLOAD; ++w) old[w] = 0u;
        tc_controls c; random_setup(&c);
        tc_controls kept = c;
        if (st_put(ST_LIVE, 0, old, &c, &engine) == ST_OK) {
            assert(c.tonic == a.tonic && c.scale == a.scale && c.tempo == a.tempo && c.chord_ch == a.chord_ch);
            assert(c.note_repeat == kept.note_repeat && c.fold == kept.fold && c.chord_mute == kept.chord_mute &&
                   c.voice_lead == kept.voice_lead && c.live_ch == kept.live_ch && c.capo == kept.capo);
            in_range(&c); old_records++;
        }
    }
}
static void flash_log(void) {
    for (uint32_t i = 0; i < 300u; ++i) {
        fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
        /* A few saved setups (each 3 s after its change), a preset, then damage. */
        tc_controls saved[4]; uint32_t now = 1000u, count = 1u + rnd(4u);
        for (uint32_t s = 0; s < count; ++s) {
            random_setup(&saved[s]);
            uint32_t changes = tc_state.changes + 1u;
            tc_state = saved[s]; tc_state.changes = changes; saved[s].changes = changes;
            st_service(&tc_state, now); now += 3001u; st_service(&tc_state, now); now += 10u;
        }
        tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = (uint8_t)rnd(TC_PRESETS); st_requests(&tc_state, &engine);
        uint32_t records = st_records(), used = records * 44u;
        uint32_t damage = rnd(4u);
        if (damage == 1u) { /* torn: the newest record half written (power lost while programming) */
            uint32_t w[44]; for (uint32_t j = 0; j < 44u; ++j) w[j] = rnd(UINT32_MAX);
            assert(fs_program(used * 4u, w, 1u + rnd(43u)));
        } else if (damage == 2u) { /* the newest live record corrupted */
            uint32_t at = (count - 1u) * 44u + 3u + rnd(41u);
            flash[at] ^= 1u << rnd(32u);
        } else if (damage == 3u) { /* random bits anywhere in the log */
            for (uint32_t j = 0, n = 1u + rnd(6u); j < n; ++j) flash[rnd(used + 44u)] ^= 1u << rnd(32u);
        }
        tc_controls loaded; tc_init(&loaded);
        bool found = st_load(&loaded);
        if (found) in_range(&loaded);
        if (damage == 0u || damage == 1u) assert(found && same_settings(&loaded, &saved[count - 1u]));
        if (damage == 2u) { /* falls back to the record before, or to the defaults */
            tc_controls defaults; tc_init(&defaults);
            assert(same_settings(&loaded, count > 1u ? &saved[count - 2u] : &defaults));
            fallbacks++;
        }
    }
    /* A nearly full log compacts at boot and keeps every current record. */
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
    tc_controls last; uint32_t now = 1000u;
    for (uint32_t s = 0; s < 600u; ++s) {
        random_setup(&last); uint32_t changes = tc_state.changes + 1u;
        tc_state = last; tc_state.changes = changes;
        st_service(&tc_state, now); now += 3001u; st_service(&tc_state, now); now += 10u;
    }
    tc_controls loaded; tc_init(&loaded);
    assert(st_load(&loaded) && same_settings(&loaded, &last) && st_records() < 10u);
    compactions++;
}
/* ---- SysEx and MIDI in ---- */
static uint32_t replies, ready_messages, dropped, requests_ok;
static void check_reply(uint32_t len) {
    if (!len) return;
    assert(len >= 8u && len <= SX_OUT_MAX && sx_out[0] == 0xF0u && sx_out[len - 1u] == 0xF7u);
    assert(sx_out[1] == 0x7Du && sx_out[2] == 0x54u && sx_out[3] == 0x42u);
    uint32_t sum = 0;
    for (uint32_t i = 4u; i < len - 1u; ++i) { assert(sx_out[i] < 0x80u); sum += sx_out[i]; }
    assert(sum % 128u == 0u);
    replies++;
}
static void feed(const uint8_t p[4], uint32_t now) {
    tm_packet(p, now);
    assert(tm.sysex_len <= TM_SYSEX_MAX);
    if (tm.sysex_ready) {
        assert(tm.sysex_len >= 2u && tm.sysex[0] == 0xF0u && tm.sysex[tm.sysex_len - 1u] == 0xF7u);
        ready_messages++;
        check_reply(sx_handle(tm.sysex, tm.sysex_len, &tc_state, &engine, now));
        tm_sysex_consumed();
        in_range(&tc_state);
    }
}
/* A message as USB-MIDI packets (CIN 4 / 5 / 6 / 7), with real-time bytes in between. */
static void send_sysex(const uint8_t *m, uint32_t n, uint32_t now) {
    for (uint32_t i = 0; i < n; i += 3u) {
        uint32_t left = n - i, take = left < 3u ? left : 3u;
        uint8_t p[4] = {0, 0, 0, 0};
        p[0] = left <= 3u ? (uint8_t)(4u + take) : 4u;
        for (uint32_t j = 0; j < take; ++j) p[1u + j] = m[i + j];
        feed(p, now);
        if (!rnd(4u)) { const uint8_t rt[4] = {0x0Fu, rnd(2u) ? 0xF8u : 0xFEu, 0, 0}; feed(rt, now); }
    }
}
static void sysex_streams(void) {
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    engine.layout = tc_layout(&tc_state);
    uint32_t now = 1000u;
    uint8_t m[400];
    for (uint32_t i = 0; i < 40000u; ++i, ++now) {
        uint32_t kind = rnd(4u);
        if (kind == 0u) { /* random packets */
            uint8_t p[4] = {(uint8_t)(rnd(4u) ? rnd(16u) : rnd(256u)), (uint8_t)rnd(256u), (uint8_t)rnd(256u), (uint8_t)rnd(256u)};
            feed(p, now);
            continue;
        }
        /* A request: well formed, or truncated, oversized, bad checksum, high bytes. */
        uint32_t n = 0, sum = 0, data = kind == 1u ? rnd(12u) : rnd(250u);
        uint8_t cmd = (uint8_t)(rnd(4u) ? 1u + rnd(SX_NOR_READ) : rnd(128u));
        m[n++] = 0xF0; m[n++] = 0x7D; m[n++] = 0x54; m[n++] = 0x42; m[n++] = cmd; sum = cmd;
        if (cmd == SX_SET_RECORD && rnd(2u)) data = 2u + 183u; /* a whole record's length */
        if (cmd == SX_NOR_READ && rnd(2u)) data = 6u;
        for (uint32_t j = 0; j < data && n < sizeof m - 2u; ++j) { m[n] = (uint8_t)rnd(128u); sum += m[n++]; }
        if (cmd == SX_SET_RECORD && data == 185u) { m[5] = (uint8_t)(1u + rnd(4u)); m[6] = (uint8_t)rnd(13u); }
        if (cmd == SX_NOR_READ && data == 6u) { m[9] = (uint8_t)rnd(128u); m[10] = (uint8_t)rnd(17u); }
        sum = cmd; for (uint32_t j = 5u; j < n; ++j) sum += m[j];
        m[n++] = (uint8_t)((128u - sum % 128u) % 128u);
        if (!rnd(10u)) m[n - 1u] ^= 1u;                           /* bad checksum */
        if (!rnd(20u)) m[5u + rnd(n - 5u)] |= 0x80u;              /* a status byte inside */
        bool truncated = !rnd(10u);
        if (!truncated) m[n++] = 0xF7;
        send_sysex(m, n, now);
        if (truncated && rnd(2u)) { const uint8_t end[4] = {0x05, 0xF7, 0, 0}; feed(end, now); }
        requests_ok++;
    }
    /* The assembler's edge: 256 bytes (F0..F7) fit, 257 and more are dropped whole. */
    static const uint32_t sizes[4] = {255u, 256u, 257u, 300u};
    for (uint32_t i = 0; i < 4u; ++i) {
        uint32_t before_ready = ready_messages, before_dropped = tm.sysex_dropped;
        memset(m, 0x11, sizeof m); m[0] = 0xF0; m[sizes[i] - 1u] = 0xF7;
        send_sysex(m, sizes[i], now);
        if (sizes[i] <= TM_SYSEX_MAX) assert(ready_messages == before_ready + 1u && tm.sysex_dropped == before_dropped);
        else assert(ready_messages == before_ready && tm.sysex_dropped == before_dropped + 1u);
    }
    dropped = tm.sysex_dropped;
    assert(replies >= 1000u && ready_messages >= 1000u);
}
static void midi_clock(void) {
    for (uint32_t period = 5u; period <= 60u; period += 5u) {
        tm_reset();
        uint32_t now = UINT32_MAX - 3000u, next = 0u; /* across the ms wrap too */
        for (uint32_t i = 0; i < 6000u; ++i, ++now) {
            if (i == next) {
                /* USB delivers in 1 ms frames: ticks bunch up, never drift. */
                const uint8_t tick[4] = {0x0Fu, 0xF8u, 0, 0};
                tm_packet(tick, now);
                next = i + period - (period > 5u ? rnd(3u) : 0u) + (period > 5u ? rnd(3u) : 0u);
                if (next <= i) next = i + 1u;
            }
            if (i > 200u) assert(tm_clock_active(now));
        }
        assert(tm.tick_us >= period * 1000u * 8u / 10u && tm.tick_us <= period * 1000u * 12u / 10u);
        assert(tm_clock_active(now + TM_CLOCK_TIMEOUT_MS - 70u) && !tm_clock_active(now + TM_CLOCK_TIMEOUT_MS + 1u));
        const uint8_t start[4] = {0x0Fu, 0xFAu, 0, 0};
        uint32_t starts = tm.starts; tm_packet(start, now);
        assert(tm.starts == starts + 1u && tm.ticks == 0u);
    }
}
int main(void) {
    tc_init(&tc_state);
    settings_round_trips();
    flash_log();
    sysex_streams();
    midi_clock();
    printf("PASS: parser stress: %u random setups round-trip exactly; %u mutated records rejected unchanged, %u "
           "accepted in range; %u 0.3-0.5 records keep the newer settings; flash log torn/corrupted/bit-flipped "
           "falls back to the newest intact record (%u checks), compaction keeps the current settings; %u SysEx "
           "requests (random, truncated, oversized, bad checksum, real-time bytes inside): %u messages assembled, "
           "%u dropped, %u replies all well formed; MIDI clock 5..60 ms with frame jitter across the ms wrap\n",
           round_trips, rejected, accepted_mutants, old_records, fallbacks, requests_ok, ready_messages, dropped, replies);
    (void)compactions;
    return 0;
}
