/* SPDX-License-Identifier: GPL-3.0-or-later - 1.7.0, three of the 2020 campaign's promises: the Bass layout
 * (every defined scale and key against walking the scale pair by pair; a two-finger roll strikes every
 * hit, Note Off before Note On, at each hit's velocity; the older pad stays quiet; other layouts keep one
 * Note On for a shared note; Mono; nothing left sounding), Program Change (sent once a session per half,
 * before notes, again when set or a preset loads, one per shared channel; received programs 1..8 load
 * presets on Program in's channel only, Program in stays when a preset loads) and Spread of Chord (every
 * scale, key, chord pad, voicing I..III and spread against opening the chord by hand; Root as before;
 * the guitar voicing in range), with their settings (round trip, out of range refused) and pages.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls_internal.h"
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
    if (kind == TK_PROGRAM) assert(!p[3]);
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void quiet_at(int line) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p)
        if (sounding[ch][p]) { fprintf(stderr, "line %d: channel %u note %u still sounding\n", line, ch + 1u, p); assert(0); }
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
#define quiet() quiet_at(__LINE__)
static void defaults(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
}
static void melody_layout(uint8_t layout) { tc_state.melody_layout = layout; e.layout = tc_layout(&tc_state); }
static void press(uint32_t k, uint16_t level) { scan.values[k] = level; }
static void touch(uint32_t k) { press(k, 3000u); }
static void lift(uint32_t k) { scan.values[k] = 0u; }
static bool is_on(const event *v) { return (v->status & 0xF0u) == 0x90u; }
static bool is_off(const event *v) { return (v->status & 0xF0u) == 0x80u; }
static bool is_program(const event *v) { return (v->status & 0xF0u) == 0xC0u; }
static uint32_t count_ons(uint8_t pitch) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += is_on(&log_[j]) && log_[j].a == pitch; return n; }
static uint32_t all_ons(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += is_on(&log_[j]); return n; }
static uint32_t programs(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += is_program(&log_[j]); return n; }
/* The i-th program change in the log (from 0); NULL when there are fewer. */
static const event *program_at(uint32_t i) {
    for (uint32_t j = 0; j < logged; ++j) if (is_program(&log_[j]) && !i--) return &log_[j];
    return NULL;
}
static bool on_page(uint8_t group, uint8_t item) {
    for (uint8_t i = 0; i < tc_group_size(group); ++i) if (tc_group_items[group][i] == item) return true;
    return false;
}
/* Melody pad at a column (0..11) and row from the bottom (0..3). */
static uint32_t pad(uint32_t column, uint32_t up) { return 48u + 4u * column + (3u - up); }
/* --- the Bass layout --------------------------------------------------------------------------- */
static void bass_pure(void) {
    tp_layout l = TP_DEFAULT_LAYOUT;
    l.melody_layout = TP_MELODY_BASS;
    uint32_t checked = 0;
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        uint32_t n = tc_scale_size(scale);
        l.mode = scale;
        for (uint8_t tonic = 0; tonic < 12u; ++tonic) for (uint32_t base = 0; base < 3u; ++base) {
            l.tonic = tonic; l.melody_base = (uint8_t)(TP_MELODY_BASE - 12u + 12u * base);
            /* Walk the scale up from the root, two pads a note, bottom row left to right, then the next row. */
            uint32_t pitch = l.melody_base - 24u + tonic, step = 0;
            for (uint32_t up = 0; up < 4u; ++up) for (uint32_t pair = 0; pair < 6u; ++pair) {
                for (uint32_t twin = 0; twin < 2u; ++twin) {
                    tp_notes got;
                    assert(tc_notes(l, pad(2u * pair + twin, up), (tc_chord){0, 0, 0}, &got));
                    assert(got.count == 1u && got.notes[0] == pitch);
                    checked++;
                }
                uint32_t from = tc_step_semitones(scale, step % n), to = tc_step_semitones(scale, (step + 1u) % n);
                pitch += (to + 12u - from) % 12u ? (to + 12u - from) % 12u : 12u; /* one scale step up */
                step++;
            }
        }
    }
    /* C major, C3 melody base: the bottom-left pair is C1, the top-right pair E4. */
    l = (tp_layout)TP_DEFAULT_LAYOUT; l.melody_layout = TP_MELODY_BASS;
    tp_notes n;
    assert(tc_notes(l, pad(0, 0), (tc_chord){0, 0, 0}, &n) && n.notes[0] == 24u);
    assert(tc_notes(l, pad(11, 3), (tc_chord){0, 0, 0}, &n) && n.notes[0] == 64u);
    assert(tc_layout_ok(l) && !tc_layout_ok((tp_layout){0, TP_MELODY_BASE, TP_CHORD_BASE, 0, TP_MELODY_LAYOUTS, 0, 0}));
    printf("bass layout: %u pads checked\n", checked);
}
static void bass_play(void) {
    defaults(); tc_state.melody_vel = TC_VEL_DYNAMIC; tc_state.melody_ch = 3u; melody_layout(TP_MELODY_BASS);
    run(1000);
    const uint32_t a = pad(0, 0), b = pad(1, 0); /* the C1 pair */
    /* One pad: one note, on the melody channel. */
    logged = 0; touch(a); run(20); assert(sounding[3][24] && logged == 1u && log_[0].status == 0x93u);
    /* Its twin under half as far as the held pad: a neighbor's bleed (1.7.0), as next to any held pad. */
    uint8_t first = log_[0].b;
    press(b, 1400u); run(20); assert(logged == 1u);
    lift(b); run(20);
    /* Its twin, more softly, while it is still held: the note ends, then starts again at the new velocity. */
    press(b, 2000u); run(20);
    assert(logged == 3u && is_off(&log_[1]) && log_[1].a == 24u && is_on(&log_[2]) && log_[2].a == 24u && log_[2].b < first);
    /* The twin lifted: the older pad, still held, stays quiet; lifted too, nothing more. */
    lift(b); run(20); assert(!sounding[3][24] && logged == 4u);
    lift(a); run(20); quiet(); assert(logged == 4u);
    /* A two-finger roll, each finger touching before the other lifts: every hit sounds. */
    logged = 0;
    for (uint32_t hit = 0; hit < 16u; ++hit) {
        uint32_t k = hit % 2u ? b : a, other = hit % 2u ? a : b;
        touch(k); run(15); lift(other); run(15);
        assert(sounding[3][24] == 1);
    }
    lift(a); lift(b); run(20); quiet();
    assert(count_ons(24) == 16u);
    for (uint32_t j = 1; j < logged; ++j) if (is_on(&log_[j])) assert(is_off(&log_[j - 1u])); /* Off, then On */
    /* Different notes on two pairs play together, as before. */
    logged = 0; touch(a); touch(pad(2, 0)); run(20); assert(sounding[3][24] && sounding[3][26] && all_ons() == 2u);
    lift(a); lift(pad(2, 0)); run(20); quiet();
    /* Both twins in the same frame: one note. */
    logged = 0; touch(a); touch(b); run(20); assert(all_ons() == 1u);
    lift(a); lift(b); run(20); quiet();
    /* With Mono: the twin still strikes again; lifting it, the older pad takes the note on (legato). */
    tc_state.mono = 1u;
    logged = 0; touch(a); run(20); touch(b); run(20); assert(count_ons(24) == 2u && sounding[3][24]);
    lift(b); run(20); assert(sounding[3][24]);
    lift(a); run(20); quiet();
    tc_state.mono = 0u;
    /* Other layouts keep one Note On for a note two held pads share (Fourths: column 5 = the next row's column 0). */
    melody_layout(TP_MELODY_FOURTHS);
    tp_notes x, y; assert(tc_pad(e.layout, pad(5, 0), &x) && tc_pad(e.layout, pad(0, 1), &y) && x.notes[0] == y.notes[0]);
    logged = 0; touch(pad(5, 0)); run(20); touch(pad(0, 1)); run(20); assert(all_ons() == 1u && logged == 1u);
    lift(pad(5, 0)); run(20); assert(sounding[3][x.notes[0]]);
    lift(pad(0, 1)); run(20); quiet();
    printf("bass play: a roll of %u hits\n", 16u);
}
/* --- Program Change ---------------------------------------------------------------------------- */
static void program_out(void) {
    defaults(); tp_loss(&e); run(1200u + TP_PROGRAM_LATE_MS); /* a fresh session, past its late programs */
    logged = 0; run(100); assert(!programs()); /* none set: nothing */
    /* Set: sent once on each half's channel, then not again. */
    tc_state.chord_ch = 0u; tc_state.melody_ch = 1u; tc_state.chord_prog = 5u; tc_state.melody_prog = 128u;
    logged = 0; run(100);
    assert(programs() == 2u && program_at(0)->status == 0xC0u && program_at(0)->a == 4u &&
           program_at(1)->status == 0xC1u && program_at(1)->a == 127u);
    logged = 0; run(500); assert(!programs());
    /* Again on request (a preset loaded, the same value set again); before a note touched at the same time. */
    tc_state.program_again = 3u; touch(3); run(20);
    assert(programs() == 2u && is_program(&log_[0]) && is_program(&log_[1]) && is_on(&log_[2]));
    lift(3); run(20); quiet();
    /* A new channel or value: that half only. */
    logged = 0; tc_state.melody_ch = 2u; run(20); assert(programs() == 1u && program_at(0)->status == 0xC2u);
    logged = 0; tc_state.chord_prog = 6u; run(20); assert(programs() == 1u && program_at(0)->status == 0xC0u && program_at(0)->a == 5u);
    /* Both halves on one channel: the chord half's program; the melody half's when the chords have none. */
    logged = 0; tc_state.melody_ch = 0u; tc_state.program_again = 3u; run(20); assert(programs() == 1u && program_at(0)->a == 5u);
    logged = 0; tc_state.chord_prog = 0u; tc_state.program_again = 3u; run(20); assert(programs() == 1u && program_at(0)->a == 127u);
    /* A new session (the computer reconnected) sends them again. */
    tc_state.chord_prog = 6u; tc_state.melody_ch = 1u; run(20);
    tp_loss(&e); logged = 0; run(1200); assert(e.state == TK_ACTIVE && programs() == 2u);
    /* Through the menu: setting a value sends it, setting the same one again too. */
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_CHORD_PROG; tc_state.edit_value = 7u;
    logged = 0; tc_navigate(&tc_state, &e, TC_NAV_ENTER); run(20);
    assert(tc_state.chord_prog == 7u && programs() == 1u && program_at(0)->a == 6u);
    tc_state.ui = TC_UI_EDIT; tc_state.edit_value = 7u;
    logged = 0; tc_navigate(&tc_state, &e, TC_NAV_ENTER); run(20); assert(programs() == 1u);
    /* A preset brings its programs, sent at once; Program in (the board's) stays. */
    fs_erase(); (void)st_load(&tc_state); run(20);
    tc_state.ui = TC_UI_PLAY; tc_state.chord_prog = 9u; tc_state.melody_prog = 0u; tc_state.program_in = 0u;
    tc_request(&tc_state, TC_REQUEST_SAVE, 2u); st_requests(&tc_state, &e); assert(tc_state.last_action == TC_PRESET_SAVED);
    tc_state.chord_prog = 20u; tc_state.melody_prog = 21u; tc_state.program_in = 4u; run(50);
    logged = 0; tc_request(&tc_state, TC_REQUEST_LOAD, 2u); st_requests(&tc_state, &e); run(20);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.chord_prog == 9u && !tc_state.melody_prog && tc_state.program_in == 4u);
    assert(programs() == 1u && program_at(0)->a == 8u);
    quiet();
    /* Again TP_PROGRAM_LATE_MS into a session (a DAW opens the port after the board connects: the first
     * ones reached no one), once; and at each DAW Start. */
    tc_state.melody_prog = 21u; run(20);
    tp_loss(&e); logged = 0; run(1200); assert(e.state == TK_ACTIVE && programs() == 2u);
    logged = 0; run(TP_PROGRAM_LATE_MS); assert(programs() == 2u);
    logged = 0; run(5000); assert(!programs());
    static const uint8_t start[4] = {0x0Fu, 0xFAu, 0u, 0u};
    for (uint32_t i = 0; i < 2u; ++i) {
        logged = 0; tm_packet(start, now); run(20); assert(programs() == 2u);
        logged = 0; run(1000); assert(!programs());
    }
    quiet();
}
static void send_program(uint8_t channel, uint8_t program) {
    const uint8_t packet[4] = {0x0Cu, (uint8_t)(0xC0u | ((channel - 1u) & 15u)), program, 0u};
    tm_packet(packet, now);
    tc_program_in(&tc_state);
}
static void program_in(void) {
    defaults(); tm_reset(); tc_state.program_seen = 0u;
    tc_state.request = TC_REQUEST_NONE;
    send_program(1, 2); assert(tc_state.request == TC_REQUEST_NONE && tm.program == 2u && tm.program_ch == 1u); /* off */
    tc_state.program_in = 1u;
    send_program(2, 2); assert(tc_state.request == TC_REQUEST_NONE); /* another channel */
    send_program(1, 8); assert(tc_state.request == TC_REQUEST_NONE); /* program 9: no preset 9 */
    send_program(1, 2); assert(tc_state.request == TC_REQUEST_LOAD && tc_state.request_slot == 2u);
    tc_state.request = TC_REQUEST_NONE; tc_program_in(&tc_state); assert(tc_state.request == TC_REQUEST_NONE); /* once */
    tc_state.program_in = 17u; /* any channel */
    send_program(16, 0); assert(tc_state.request == TC_REQUEST_LOAD && tc_state.request_slot == 0u);
    /* Not a program change (status byte of another kind under CIN 0xC): ignored. */
    tc_state.request = TC_REQUEST_NONE;
    const uint8_t odd[4] = {0x0Cu, 0x90u, 3u, 0u}; tm_packet(odd, now); tc_program_in(&tc_state);
    assert(tc_state.request == TC_REQUEST_NONE);
    /* An empty slot says so; a full one loads. */
    fs_erase(); (void)st_load(&tc_state);
    tc_state.program_in = 1u; tc_state.request = TC_REQUEST_NONE;
    send_program(1, 5); st_requests(&tc_state, &e); assert(tc_state.last_action == TC_PRESET_EMPTY);
    tc_request(&tc_state, TC_REQUEST_SAVE, 5u); st_requests(&tc_state, &e);
    send_program(1, 5); st_requests(&tc_state, &e); assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.preset == 5u);
    /* While recording a progression: busy, as Shift + mode button. */
    tc_state.recording = true; send_program(1, 5); st_requests(&tc_state, &e); assert(tc_state.last_action == TC_SEQ_BUSY);
    tc_state.recording = false;
}
/* --- Spread of Chord --------------------------------------------------------------------------- */
static void sort_ints(int32_t *v, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) for (uint32_t j = i; j && v[j - 1u] > v[j]; --j) { int32_t x = v[j]; v[j] = v[j - 1u]; v[j - 1u] = x; }
}
static void spread_of(void) {
    tp_layout l = TP_DEFAULT_LAYOUT;
    const tc_chord plain = {0, 0, 0}, seventh = {0, 0, TC_ADD7};
    tc_panel p = TC_PANEL_NEUTRAL;
    tp_notes n;
    /* C major, bottom-left pad (C2 E2 G2): Root drops the C; Chord opens C G E, then drops the C too. */
    p.spread = 1u; assert(tc_notes_panel(l, 3, plain, p, &n) && n.count == 3u && n.notes[0] == 24u && n.notes[1] == 40u && n.notes[2] == 43u);
    p.spread_chord = 1u;
    assert(tc_notes_panel(l, 3, plain, p, &n) && n.count == 3u && n.notes[0] == 36u && n.notes[1] == 43u && n.notes[2] == 52u);
    p.spread = 2u; assert(tc_notes_panel(l, 3, plain, p, &n) && n.notes[0] == 24u && n.notes[1] == 43u && n.notes[2] == 52u);
    p.spread = 3u; assert(tc_notes_panel(l, 3, plain, p, &n) && n.notes[0] == 12u && n.notes[1] == 43u && n.notes[2] == 52u);
    p.spread = 1u; /* Cmaj7: C G E B */
    assert(tc_notes_panel(l, 3, seventh, p, &n) && n.count == 4u && n.notes[0] == 36u && n.notes[1] == 43u && n.notes[2] == 52u && n.notes[3] == 59u);
    p.octaves = 0x18u; /* with the octave above: both copies, each pitch once */
    assert(tc_notes_panel(l, 3, plain, p, &n) && n.count == 6u && n.notes[0] == 36u && n.notes[2] == 48u && n.notes[4] == 55u && n.notes[5] == 64u);
    p.octaves = TC_OCTAVES_DEFAULT;
    p.spread_chord = 2u; assert(!tc_notes_panel(l, 3, plain, p, &n)); /* out of range: refused */
    p.spread_chord = 1u; p.spread = 0u; /* no spread: as plain */
    tp_notes as_is; assert(tc_notes(l, 3, plain, &as_is) && tc_notes_panel(l, 3, plain, p, &n) && !memcmp(&n, &as_is, sizeof n));
    /* Every scale, key, chord pad, voicing I..III, spread 1..3: the chord opened by hand (with its 7th where
     * the 7th is a note of its own: a 6-note scale's 7th step doubles the root, and both copies spread). */
    uint32_t checked = 0;
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        l.mode = scale;
        const tc_chord shape = tc_scale_size(scale) >= 7u ? seventh : plain;
        for (uint8_t tonic = 0; tonic < 12u; ++tonic) for (uint32_t k = 0; k < 48u; ++k)
            for (uint8_t voicing = 0; voicing < TC_VOICING_GUITAR; ++voicing) for (uint8_t s = 1; s < TC_SPREADS; ++s) {
                l.tonic = tonic;
                tc_panel base = TC_PANEL_NEUTRAL, open = TC_PANEL_NEUTRAL;
                base.voicing = open.voicing = voicing; open.spread = s; open.spread_chord = 1u;
                tp_notes before, after;
                if (!tc_notes_panel(l, k, shape, base, &before)) continue;
                assert(tc_notes_panel(l, k, shape, open, &after));
                int32_t v[TP_MAX_NOTES]; uint32_t m = before.count;
                for (uint32_t i = 0; i < m; ++i) v[i] = before.notes[i];
                sort_ints(v, m); /* unspread voicing I: the triad, then its added notes */
                for (uint32_t i = 1; i < m; i += 2u) v[i] += 12;
                uint32_t root = tc_pad_root(l, k), low = m;
                for (uint32_t i = 0; i < m; ++i) if ((uint32_t)(v[i] + 120) % 12u == root && (low == m || v[i] < v[low])) low = i;
                if (low < m) v[low] -= 12 * (s - 1);
                sort_ints(v, m);
                uint32_t want = 0; int32_t w[TP_MAX_NOTES];
                for (uint32_t i = 0; i < m; ++i) if (v[i] >= 0 && v[i] <= 127 && (!want || w[want - 1u] != v[i])) w[want++] = v[i];
                assert(after.count == want);
                for (uint32_t i = 0; i < want; ++i) assert(after.notes[i] == (uint8_t)w[i]);
                checked++;
            }
    }
    /* Guitar: opened inside MIDI range, the root still there. */
    l = (tp_layout)TP_DEFAULT_LAYOUT;
    for (uint32_t k = 0; k < 48u; ++k) for (uint8_t s = 1; s < TC_SPREADS; ++s) {
        tc_panel g = TC_PANEL_NEUTRAL; g.voicing = TC_VOICING_GUITAR; g.spread = s; g.spread_chord = 1u;
        assert(tc_notes_panel(l, k, plain, g, &n) && n.count >= 2u);
        bool root = false;
        for (uint32_t i = 0; i < n.count; ++i) root = root || n.notes[i] % 12u == tc_pad_root(l, k);
        assert(root);
    }
    /* Played: the Spread buttons with Spread of Chord. */
    defaults(); tc_state.spread_chord = 1u; tc_state.chord_spread = 1u;
    assert(tc_pad(e.layout, 3, &n) && n.count == 3u && n.notes[0] == 36u && n.notes[1] == 43u && n.notes[2] == 52u);
    printf("spread of chord: %u chords checked\n", checked);
}
/* --- settings and pages ------------------------------------------------------------------------ */
static void stored(void) {
    tc_controls a; tc_init(&a);
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && !p[19] && !(p[33] >> 24)); /* defaults: what 1.6 reads */
    a.spread_chord = 1u; a.chord_prog = 128u; a.melody_prog = 1u; a.program_in = 17u; a.unstick = 0u;
    a.melody_layout = TP_MELODY_BASS;
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK);
    assert(p[19] == (0x300u | 128u << 16 | 1u << 24) && p[33] >> 24 == 17u && ((p[24] >> 16) & 0xFFu) == TP_MELODY_BASS);
    tc_controls b; tc_init(&b);
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK);
    assert(b.spread_chord == 1u && b.chord_prog == 128u && b.melody_prog == 1u && b.program_in == 17u && !b.unstick &&
           b.melody_layout == TP_MELODY_BASS);
    /* Out of range, each in place of its field (word, the field's bits, the bad value): refused, nothing changed. */
    static const uint32_t wrong[][3] = {{19, 0xFC00u, 0x400u}, {19, 0xFF0000u, 129u << 16}, {19, 0xFF000000u, 129u << 24},
                                        {33, 0xFF000000u, 18u << 24}, {24, 0xFF0000u, TP_MELODY_LAYOUTS << 16}};
    for (uint32_t i = 0; i < sizeof wrong / sizeof wrong[0]; ++i) {
        uint32_t bad[ST_PAYLOAD]; memcpy(bad, p, sizeof bad);
        bad[wrong[i][0]] = (bad[wrong[i][0]] & ~wrong[i][1]) | wrong[i][2];
        tc_controls before = b;
        assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE && !memcmp(&before, &b, sizeof b));
    }
    /* The pages and values. */
    assert(on_page(TC_GROUP_MIDI, TC_MENU_PROGRAM_IN) && on_page(TC_GROUP_MIDI, TC_MENU_CHORD_PROG) &&
           on_page(TC_GROUP_MIDI, TC_MENU_MELODY_PROG) && on_page(TC_GROUP_CHORDS, TC_MENU_SPREAD_OF));
    assert(tc_menu_values(TC_MENU_PROGRAM_IN) == TC_PROGRAM_INPUTS && tc_menu_values(TC_MENU_CHORD_PROG) == TC_PROGRAMS &&
           tc_menu_values(TC_MENU_MELODY_PROG) == TC_PROGRAMS && tc_menu_values(TC_MENU_SPREAD_OF) == 2u &&
           tc_menu_values(TC_MENU_LAYOUT) == TP_MELODY_LAYOUTS);
    tc_controls c; tc_init(&c);
    assert(!tc_menu_value(&c, TC_MENU_PROGRAM_IN) && !tc_menu_value(&c, TC_MENU_CHORD_PROG) &&
           !tc_menu_value(&c, TC_MENU_MELODY_PROG) && !tc_menu_value(&c, TC_MENU_SPREAD_OF));
}
int main(void) {
    bass_pure();
    bass_play();
    program_out();
    program_in();
    spread_of();
    stored();
    puts("promised: all passed");
    return 0;
}
