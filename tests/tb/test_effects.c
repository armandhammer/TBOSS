/* SPDX-License-Identifier: GPL-3.0-or-later - 1.6.0 effects: the harmonizer (every defined scale, key,
 * note and step count against walking the scale note by note; textbook cases; on the pads), echo
 * (times on the board's clock and a DAW's, fading velocities, a held note, the 8 sources, all notes
 * off, switched off), mono (the new note before the old one's Note Off, the one before comes back,
 * off, drums), chance (100 % as before, about the set share of arpeggio steps and note repeats) and
 * the settings (words 18 and 19: 0 at the defaults, round trip, out of range refused; the pages).
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/settings.h"
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
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[16384];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 16384u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    assert((status >> 4) == kind && p[2] < 128u && p[3] < 128u);
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1 && !p[3]); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static uint32_t daw_us, daw_next_us;
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    while (daw_us && (int32_t)(now * 1000u - daw_next_us) >= 0) {
        static const uint8_t clock[4] = {0x0Fu, 0xF8u, 0u, 0u};
        tm_packet(clock, now); daw_next_us += daw_us;
    }
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
/* Nothing sounding or owed (line: the caller's, printed when something is). */
static void quiet_at(int line) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p)
        if (sounding[ch][p]) { fprintf(stderr, "line %d: channel %u note %u still sounding\n", line, ch + 1u, p); assert(0); }
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
#define quiet() quiet_at(__LINE__)
static void defaults(void) {
    tc_init(&tc_state); tc_state.unstick = 0u; /* readings held exactly constant: Unstick would take them for a stuck pad */
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
}
static void melody_layout(uint8_t layout) { tc_state.melody_layout = layout; e.layout = tc_layout(&tc_state); }
/* The melody pad that plays exactly this note (the Scale layout has each scale note somewhere). */
static uint32_t pad_of(uint8_t note) {
    for (uint32_t k = 48u; k < TS_KEYS; ++k) {
        tp_notes n;
        if (tc_pad(e.layout, k, &n) && n.count == 1u && n.notes[0] == note) return k;
    }
    assert(0); return 0;
}
static void touch(uint32_t k) { scan.values[k] = 3000u; }
static void lift(uint32_t k) { scan.values[k] = 0u; }
static bool is_on(const event *v) { return (v->status & 0xF0u) == 0x90u; }
static bool is_off(const event *v) { return (v->status & 0xF0u) == 0x80u; }
/* The i-th logged Note On of pitch (from 0); NULL when there are fewer. */
static const event *on_of(uint8_t pitch, uint32_t i) {
    for (uint32_t j = 0; j < logged; ++j) if (is_on(&log_[j]) && log_[j].a == pitch && !i--) return &log_[j];
    return NULL;
}
static uint32_t count_ons(uint8_t pitch) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += is_on(&log_[j]) && log_[j].a == pitch; return n; }
static uint32_t all_ons(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += is_on(&log_[j]); return n; }
/* Where in the log a pitch's first Note On / Off is. */
static uint32_t index_of(uint8_t pitch, bool on) {
    for (uint32_t j = 0; j < logged; ++j) if ((on ? is_on(&log_[j]) : is_off(&log_[j])) && log_[j].a == pitch) return j;
    return UINT32_MAX;
}
/* --- the harmonizer ------------------------------------------------------------------------ */
static bool in_scale(uint8_t scale, uint32_t tonic, int32_t note) { return tc_in_scale(scale, (uint32_t)(note + 240 - (int32_t)tonic)); }
static uint32_t harmony_checks;
static void harmony_pure(void) {
    tp_layout l = TP_DEFAULT_LAYOUT;
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        int32_t n = (int32_t)tc_scale_size(scale);
        l.mode = scale;
        for (uint8_t tonic = 0; tonic < 12u; ++tonic) {
            l.tonic = tonic;
            for (int32_t pitch = 0; pitch < 128; ++pitch) for (int32_t by = -n - 2; by <= n + 2; ++by) {
                /* Walk the scale: from pitch (or the scale note below it) one scale note at a time. */
                int32_t r = pitch;
                while (!in_scale(scale, tonic, r)) r--;
                for (int32_t s = by; s > 0; --s) { r++; while (!in_scale(scale, tonic, r)) r++; }
                for (int32_t s = by; s < 0; ++s) { r--; while (!in_scale(scale, tonic, r)) r--; }
                uint8_t want = r < 0 || r > 127 ? 0xFFu : (uint8_t)r;
                assert(tc_harmony_note(l, (uint8_t)pitch, by) == want);
                if (by == n && want != 0xFFu && in_scale(scale, tonic, pitch)) assert(want == pitch + 12); /* the octave */
                harmony_checks++;
            }
        }
    }
    /* Textbook, C major (M1): E + a 3rd = G, B + a 3rd = D, F + a 5th = C, B + a 5th = F (diminished),
     * C - a 3rd = A, C - a 6th = E, C# (outside the scale) + a 3rd = E (from C). */
    l = (tp_layout)TP_DEFAULT_LAYOUT; l.mode = tc_mode_buttons[2];
    assert(tc_harmony_note(l, 64, 2) == 67 && tc_harmony_note(l, 71, 2) == 74 && tc_harmony_note(l, 65, 4) == 72);
    assert(tc_harmony_note(l, 71, 4) == 77 && tc_harmony_note(l, 60, -2) == 57 && tc_harmony_note(l, 60, -5) == 52);
    assert(tc_harmony_note(l, 61, 2) == 64 && tc_harmony_note(l, 127, 2) == 0xFFu && tc_harmony_note(l, 0, -1) == 0xFFu);
    /* A minor (M6, tonic A): A + a 3rd = C, E + a 3rd = G; G major (tonic G): F# + a 3rd = A. */
    l.mode = tc_mode_buttons[7]; l.tonic = 9u;
    assert(tc_harmony_note(l, 57, 2) == 60 && tc_harmony_note(l, 64, 2) == 67);
    l.mode = tc_mode_buttons[2]; l.tonic = 7u;
    assert(tc_harmony_note(l, 66, 2) == 69);
    /* tc_harmonize: each option's notes, no doubles, nothing past MIDI, at most TP_MAX_NOTES. */
    l = (tp_layout)TP_DEFAULT_LAYOUT; l.mode = tc_mode_buttons[2];
    static const uint8_t expect[TC_HARMONIES][3] = {{60,0,0},{60,64,0},{60,65,0},{60,67,0},{60,69,0},{60,72,0},{60,64,67},{60,57,0},{60,52,0}};
    for (uint8_t h = 0; h < TC_HARMONIES; ++h) {
        tp_notes notes = {{60}, 1};
        tc_harmonize(l, h, &notes);
        uint32_t want = 1u + (expect[h][1] != 0u) + (expect[h][2] != 0u);
        assert(notes.count == want);
        for (uint32_t i = 0; i < want; ++i) assert(notes.notes[i] == expect[h][i]);
    }
    tp_notes two = {{60, 64}, 2}; tc_harmonize(l, 1u, &two); /* C+E with 3rds: E is there already, G added */
    assert(two.count == 3u && two.notes[2] == 67);
    tp_notes top = {{127}, 1}; tc_harmonize(l, 6u, &top); assert(top.count == 1u);
    tp_notes full; full.count = TP_MAX_NOTES; for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) full.notes[i] = (uint8_t)(40u + i);
    tc_harmonize(l, 6u, &full); assert(full.count == TP_MAX_NOTES);
    tp_notes bad = {{60}, 1}; tc_harmonize(l, TC_HARMONIES, &bad); assert(bad.count == 1u);
}
static void harmony_pads(void) {
    defaults(); run(1000); assert(e.state == TK_ACTIVE);
    uint8_t ch = tc_state.melody_ch;
    tc_state.harmony = 6u; /* 3rd + 5th */
    uint32_t c4 = pad_of(60), e4 = pad_of(64);
    logged = 0; touch(c4); run(30); lift(c4); run(30); quiet();
    assert(count_ons(60) == 1u && count_ons(64) == 1u && count_ons(67) == 1u && all_ons() == 3u);
    for (uint32_t j = 0; j < logged; ++j) assert((log_[j].status & 15u) == ch && (!is_on(&log_[j]) || log_[j].b == 100u));
    /* Two pads with 3rds: C's 3rd is E, which E's pad plays too: one E, sounding until both lift. */
    tc_state.harmony = 1u;
    logged = 0; touch(c4); run(30); touch(e4); run(30); lift(c4); run(30);
    assert(sounding[ch][67] == 1 && sounding[ch][64] == 1 && !sounding[ch][60]);
    lift(e4); run(30); quiet();
    /* Chord pads are not harmonized; the Drums layout isn't either. */
    logged = 0; touch(3); run(30); lift(3); run(30); quiet();
    tp_notes chord; assert(tc_pad(e.layout, 3, &chord) && all_ons() == chord.count);
    melody_layout(TP_MELODY_DRUMS);
    logged = 0; touch(60); run(30); lift(60); run(30); quiet();
    assert(all_ons() == 1u);
    melody_layout(TP_MELODY_SCALE); tc_state.harmony = 0u;
    logged = 0; touch(c4); run(30); lift(c4); run(30); quiet();
    assert(all_ons() == 1u);
}
/* --- echo -------------------------------------------------------------------------------------- */
/* A short strike of pad k: the Note On times and velocities of pitch in the next ms. */
static uint32_t at_[64]; static uint8_t vel_[64];
static uint32_t strike(uint32_t k, uint8_t pitch, uint32_t held, uint32_t ms) {
    logged = 0; touch(k); run(held); lift(k); run(ms);
    uint32_t n = 0;
    for (uint32_t j = 0; j < logged && n < 64u; ++j) if (is_on(&log_[j]) && log_[j].a == pitch) { at_[n] = log_[j].at; vel_[n++] = log_[j].b; }
    return n;
}
static void near(uint32_t got, uint32_t want, uint32_t slack) { assert(got + slack >= want && got <= want + slack); }
static void echo(void) {
    defaults(); run(1000);
    uint32_t c4 = pad_of(60);
    /* 1/8 at 120 BPM (250 ms), 3 echoes (the default): 75, 50, 25 of the strike's 100, each 125 ms long. */
    tc_state.echo = 3u; assert(tc_state.echoes == TC_ECHOES_DEFAULT);
    uint32_t n = strike(c4, 60, 40, 1200); quiet();
    assert(n == 4u && vel_[0] == 100u && vel_[1] == 75u && vel_[2] == 50u && vel_[3] == 25u);
    for (uint32_t i = 1; i < n; ++i) near(at_[i] - at_[0], 250u * i, 2u);
    for (uint32_t j = 0; j < logged; ++j) if (is_off(&log_[j]) && log_[j].at > at_[1]) { near(log_[j].at - at_[1], 125u, 2u); break; }
    /* Every time and count: echo n of N at (N + 1 - n) / (N + 1). */
    for (uint8_t t = 1; t < TC_ECHO_TIMES; ++t) for (uint8_t c = 0; c < TC_ECHOES; c += 3u) {
        tc_state.echo = t; tc_state.echoes = c;
        uint32_t total = c + 1u, clock_x6 = 125u; /* a clock at 120 BPM is 125/6 ms */
        uint32_t len = tc_echo_ticks[t] * clock_x6 / 6u; /* the last echo ends half an echo after it starts */
        n = strike(c4, 60, 20, len * total + len / 2u + 100u); quiet();
        assert(n == total + 1u);
        for (uint32_t i = 1; i < n; ++i) {
            near(at_[i] - at_[0], tc_echo_ticks[t] * clock_x6 * i / 6u, 2u);
            uint32_t v = 100u * (total + 1u - i) / (total + 1u);
            assert(vel_[i] == (v ? v : 1u));
        }
    }
    tc_state.echoes = TC_ECHOES_DEFAULT;
    /* From a DAW at 125 BPM (a clock every 20 ms): 1/8 = 240 ms. */
    daw_us = 20000u; daw_next_us = now * 1000u + 1000u; run(100);
    tc_state.echo = 3u;
    n = strike(c4, 60, 20, 1000); quiet();
    assert(n == 4u);
    for (uint32_t i = 1; i < n; ++i) near(at_[i] - at_[0], 240u * i, 3u);
    daw_us = 0u; run(400);
    /* A held note: echoes that come while it sounds are not struck again; the one after is. */
    n = strike(c4, 60, 600, 600); quiet();
    assert(n == 2u && vel_[1] == 25u); near(at_[1] - at_[0], 750u, 2u);
    /* With the harmonizer: the echoes carry the harmony. */
    tc_state.harmony = 1u;
    n = strike(c4, 60, 20, 1000); quiet();
    assert(n == 4u && count_ons(64) == 4u);
    tc_state.harmony = 0u;
    /* Ten strikes 30 ms apart with 8 echoes of 1/2: 8 sources, so the first two strikes' echoes give way. */
    tc_state.echo = 7u; tc_state.echoes = 7u;
    static const uint8_t scale_notes[10] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76};
    logged = 0;
    for (uint32_t i = 0; i < 10u; ++i) { uint32_t k = pad_of(scale_notes[i]); touch(k); run(15); lift(k); run(15); }
    run(9000); quiet();
    assert(count_ons(60) == 1u && count_ons(62) == 1u);
    for (uint32_t i = 2; i < 10u; ++i) assert(count_ons(scale_notes[i]) == 9u);
    /* All notes off ends the echoes still to come; switching echo off too. */
    logged = 0; touch(c4); run(20); lift(c4); run(1100); tc_state.panic = true; run(3000); quiet();
    assert(count_ons(60) == 2u);
    logged = 0; touch(c4); run(20); lift(c4); run(1100); tc_state.echo = 0u; run(3000); quiet();
    assert(count_ons(60) == 2u);
    /* Chord pads don't echo, nor do drums. */
    tc_state.echo = 3u; tc_state.echoes = TC_ECHOES_DEFAULT;
    tp_notes chord; assert(tc_pad(e.layout, 3, &chord));
    logged = 0; touch(3); run(20); lift(3); run(1200); quiet();
    assert(all_ons() == chord.count);
    melody_layout(TP_MELODY_DRUMS);
    logged = 0; touch(60); run(20); lift(60); run(1200); quiet();
    assert(all_ons() == 1u);
    melody_layout(TP_MELODY_SCALE); tc_state.echo = 0u;
}
/* --- mono ---------------------------------------------------------------------------------------- */
static void mono(void) {
    defaults(); run(1000);
    uint8_t ch = tc_state.melody_ch;
    uint32_t c4 = pad_of(60), d4 = pad_of(62), e4 = pad_of(64);
    /* Off: two pads, two notes. */
    logged = 0; touch(c4); run(20); touch(d4); run(20);
    assert(sounding[ch][60] && sounding[ch][62]);
    lift(d4); lift(c4); run(20); quiet();
    /* On: D's Note On before C's Note Off (legato); lifting D brings C back; lifting C ends it. */
    tc_state.mono = 1u;
    logged = 0; touch(c4); run(20); touch(d4); run(20);
    assert(!sounding[ch][60] && sounding[ch][62] && index_of(62, true) < index_of(60, false));
    lift(d4); run(20);
    /* C's Note On before D's Note Off (legato on the way back too, for a mono synth's glide). */
    assert(sounding[ch][60] && !sounding[ch][62] && count_ons(60) == 2u && (uint32_t)(on_of(60, 1) - log_) < index_of(62, false));
    lift(c4); run(20); quiet();
    /* Three pads: lifting the newest brings back the one before it, not the first. */
    logged = 0; touch(c4); run(20); touch(d4); run(20); touch(e4); run(20);
    assert(!sounding[ch][60] && !sounding[ch][62] && sounding[ch][64]);
    lift(e4); run(20); assert(!sounding[ch][60] && sounding[ch][62]);
    lift(d4); run(20); assert(sounding[ch][60]);
    lift(c4); run(20); quiet();
    /* An older pad lifted while hidden stays quiet. */
    logged = 0; touch(c4); run(20); touch(d4); run(20); lift(c4); run(20); lift(d4); run(20); quiet();
    assert(count_ons(60) == 1u && count_ons(62) == 1u);
    /* With the harmonizer the newest pad's harmony replaces the old one's. */
    tc_state.harmony = 1u;
    logged = 0; touch(c4); run(20); touch(d4); run(20);
    assert(!sounding[ch][60] && !sounding[ch][64] && sounding[ch][62] && sounding[ch][65]);
    lift(d4); run(20); assert(sounding[ch][60] && sounding[ch][64]);
    assert((uint32_t)(on_of(60, 1) - log_) < index_of(62, false) && (uint32_t)(on_of(64, 1) - log_) < index_of(65, false));
    lift(c4); run(20); quiet();
    tc_state.harmony = 0u;
    /* Switched off while two are held: the hidden one comes back. */
    touch(c4); run(20); touch(d4); run(20); assert(!sounding[ch][60]);
    tc_state.mono = 0u; run(20); assert(sounding[ch][60] && sounding[ch][62]);
    lift(c4); lift(d4); run(20); quiet();
    /* With note repeat (1.7.0 fix): only the newest held pad repeats; the one under it stays quiet through
     * the gaps between repeats and doesn't take over. */
    tc_state.note_repeat = 3u; tc_state.mono = 1u; /* 1/16 */
    logged = 0; touch(c4); run(20); touch(d4); run(1000);
    assert(count_ons(60) == 1u && count_ons(62) >= 6u);
    lift(d4); run(300); assert(count_ons(60) >= 2u); /* it comes back and repeats */
    uint32_t d_ons = count_ons(62); run(300); assert(count_ons(62) == d_ons);
    lift(c4); run(200); quiet();
    /* ... also with the newer pad touched in a gap between the older one's repeats (1.7.0 fix: the
     * older one's next strike was then the newest and took over after one note of the newer pad). */
    for (uint32_t at = 0; at < 125u; at += 5u) {
        touch(c4); run(300u + at); logged = 0; touch(d4); run(1000);
        assert(count_ons(62) >= 6u && count_ons(60) == 0u);
        lift(d4); run(300); assert(count_ons(60) >= 1u); /* back on its repeats */
        lift(c4); run(200); quiet();
    }
    tc_state.note_repeat = 0u;
    /* Drums: every pad still sounds. */
    tc_state.mono = 1u; melody_layout(TP_MELODY_DRUMS);
    touch(60); run(20); touch(61); run(20);
    uint32_t sound = 0; for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) sound += (uint32_t)sounding[c][p];
    assert(sound == 2u);
    lift(60); lift(61); run(20); quiet();
    /* Chord pads stay chords. */
    melody_layout(TP_MELODY_SCALE);
    tp_notes chord; assert(tc_pad(e.layout, 3, &chord));
    logged = 0; touch(3); run(20); touch(c4); run(20); assert(all_ons() == chord.count + 1u);
    lift(3); lift(c4); run(20); quiet();
    tc_state.mono = 0u;
}
/* --- chance --------------------------------------------------------------------------------------- */
static uint32_t held_ons(uint32_t k, uint32_t ms) { logged = 0; touch(k); run(ms); lift(k); run(300); quiet(); return all_ons(); }
static void chance(void) {
    defaults(); run(1000);
    /* The arpeggiator at 1/16 for 20 s: 160 steps at 100 %, about half at 50 %, a tenth at 10 %. */
    tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; tc_state.arp_gate = 1u;
    uint32_t all = held_ons(3, 20000u);
    assert(all >= 159u && all <= 161u);
    tc_state.chance = 5u; uint32_t half = held_ons(3, 20000u);
    assert(half >= 55u && half <= 105u);
    tc_state.chance = 9u; uint32_t tenth = held_ons(3, 20000u);
    assert(tenth >= 4u && tenth <= 30u);
    /* The steps it plays stay on the grid. */
    for (uint32_t j = 0, last = 0; j < logged; ++j) if (is_on(&log_[j])) {
        if (last) assert((log_[j].at - last) % 125u <= 1u || (log_[j].at - last) % 125u >= 124u);
        last = log_[j].at;
    }
    tc_state.arp = TC_ARP_OFF; tc_state.chance = 0u;
    /* Note repeat at 1/16 for 10 s: about 80 strikes; at 50 % the touch's strike and about half the rest. */
    uint32_t c4 = pad_of(60);
    tc_state.note_repeat = 3u;
    all = held_ons(c4, 10000u);
    assert(all >= 78u && all <= 82u);
    tc_state.chance = 5u;
    logged = 0; uint32_t touched = now; touch(c4); run(10000); lift(c4); run(300); quiet();
    half = all_ons();
    assert(half >= 25u && half <= 55u && on_of(60, 0)->at - touched <= 2u);
    tc_state.note_repeat = 0u; tc_state.chance = 0u;
}
/* --- settings and pages ------------------------------------------------------------------------- */
static bool on_page(uint8_t group, uint8_t item) {
    for (uint8_t i = 0; i < tc_group_size(group); ++i) if (tc_group_items[group][i] == item) return true;
    return false;
}
static void stored(void) {
    tc_controls a; tc_init(&a);
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && !p[18] && !p[19]); /* defaults: what older firmware reads */
    a.harmony = TC_HARMONIES - 1u; a.echo = TC_ECHO_TIMES - 1u; a.echoes = 0u; a.mono = 1u; a.chance = TC_CHANCES - 1u;
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK);
    assert(p[18] == ((TC_HARMONIES - 1u) | (TC_ECHO_TIMES - 1u) << 8 | 1u << 16 | (TC_CHANCES - 1u) << 24) && p[19] == 1u);
    tc_controls b; tc_init(&b);
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK);
    assert(b.harmony == a.harmony && b.echo == a.echo && b.echoes == 0u && b.mono == 1u && b.chance == a.chance);
    for (uint8_t c = 0; c < TC_ECHOES; ++c) { /* every count; 3 stored as 0 */
        a.echoes = c; assert(st_get(ST_LIVE, 0, &a, p) == ST_OK);
        assert(((p[18] >> 16) & 0xFFu) == (c == TC_ECHOES_DEFAULT ? 0u : c + 1u));
        tc_init(&b); assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK && b.echoes == c);
    }
    /* Out of range: refused, nothing changes. */
    uint32_t bad[ST_PAYLOAD];
    static const uint32_t wrong[][2] = {{18, TC_HARMONIES}, {18, TC_ECHO_TIMES << 8}, {18, (TC_ECHOES + 1u) << 16},
                                        {18, (uint32_t)TC_CHANCES << 24}, {19, 2u}, {19, 0x400u}}; /* 0x100, 0x200: Unstick off, Spread of Chord (1.7.0) */
    for (uint32_t i = 0; i < sizeof wrong / sizeof wrong[0]; ++i) {
        memcpy(bad, p, sizeof bad); bad[18] = 0u; bad[19] = 0u; bad[wrong[i][0]] = wrong[i][1];
        tc_controls before = b;
        assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE && !memcmp(&before, &b, sizeof b));
    }
    /* A 1.5 record (words 18 and 19 0): every effect off, 3 echoes. */
    memcpy(bad, p, sizeof bad); bad[18] = 0u; bad[19] = 0u;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_OK && !b.harmony && !b.echo && b.echoes == TC_ECHOES_DEFAULT && !b.mono && !b.chance);
    /* The pages: Play has Harmony, Echo and Echoes after note repeat; Layout has Mono; the arpeggiator has Chance. */
    assert(on_page(TC_GROUP_PLAY, TC_MENU_HARMONY) && on_page(TC_GROUP_PLAY, TC_MENU_ECHO) && on_page(TC_GROUP_PLAY, TC_MENU_ECHOES));
    assert(on_page(TC_GROUP_LAYOUT, TC_MENU_MONO) && on_page(TC_GROUP_ARP, TC_MENU_CHANCE));
    assert(tc_group_size(TC_GROUP_PLAY) == TC_GROUP_MAX - 1u && tc_group_size(TC_GROUP_CHORDS) == TC_GROUP_MAX); /* 1.7.0: Spread of */
    assert(tc_menu_values(TC_MENU_HARMONY) == TC_HARMONIES && tc_menu_values(TC_MENU_ECHO) == TC_ECHO_TIMES &&
           tc_menu_values(TC_MENU_ECHOES) == TC_ECHOES && tc_menu_values(TC_MENU_MONO) == 2u && tc_menu_values(TC_MENU_CHANCE) == TC_CHANCES);
    tc_controls c; tc_init(&c);
    assert(!tc_menu_value(&c, TC_MENU_HARMONY) && !tc_menu_value(&c, TC_MENU_ECHO) && tc_menu_value(&c, TC_MENU_ECHOES) == 2u &&
           !tc_menu_value(&c, TC_MENU_MONO) && !tc_menu_value(&c, TC_MENU_CHANCE) && tc_chance_pct[0] == 100u);
}
int main(void) {
    harmony_pure();
    harmony_pads();
    echo();
    mono();
    chance();
    stored();
    printf("PASS: 1.6.0 effects: harmonizer (%u notes in every defined scale and key against walking the scale; "
           "textbook 3rds, 5ths, 6ths below, notes outside the scale; every option, no doubles, MIDI range, 16-note cap; "
           "on the pads, shared notes, not chords or drums), echo (every time and count on the tempo's clock, from a "
           "DAW's, fading velocities, a held note, with harmony, 10 strikes on 8 sources, all notes off, switched off, "
           "not chords or drums), mono (legato order, the one before comes back, three pads, harmony, switched off, "
           "drums, chords), chance (arp at 100/50/10 %%, on the grid; note repeat keeps the touch), settings words 18 "
           "and 19 (0 at the defaults, every echo count, refused out of range, 1.5 records), the pages\n",
           harmony_checks);
    return 0;
}
