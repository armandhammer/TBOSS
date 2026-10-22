/* SPDX-License-Identifier: GPL-3.0-or-later - Stage 3 (0.6.0) on the real engine and layout:
 * the chord panel (voicing, spread, octave stacking), more scales and user slots, the
 * right-half layouts and drum pad channels, strum per pad / at the tempo / repeat /
 * cutoff, arpeggiator rhythm / swing / sync / chords / latch, velocity curves, pitch bend
 * from pressure, the pedal's controllers / note / transport, MIDI clock out, live update.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[8192];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 8192u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    assert((status >> 4) == kind && p[2] < 128u && p[3] < 128u);
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1 && !p[3]); sounding[ch][p[2]]--; }
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
static void clear_log(void) { logged = 0; }
static uint32_t count(uint8_t status) {
    uint32_t n = 0; for (uint32_t i = 0; i < logged; ++i) n += log_[i].status == status; return n;
}
static const event *nth(uint8_t status, uint32_t n) {
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == status && !n--) return &log_[i];
    return NULL;
}
static uint32_t at_of(uint8_t status, uint8_t note) { /* the first time this note went out */
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == status && log_[i].a == note) return log_[i].at;
    assert(0); return 0;
}
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
static void defaults(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
}
static void pad(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; }
static uint8_t key_of(uint32_t side, uint32_t column, uint32_t row) {
    uint8_t k; assert(tp_index((tp_position){(uint8_t)side, (uint8_t)row, (uint8_t)column}, &k)); return k;
}
static bool same_notes(const tp_notes *a, const uint8_t *b, uint32_t n) {
    if (a->count != n) return false;
    for (uint32_t i = 0; i < n; ++i) if (a->notes[i] != b[i]) return false;
    return true;
}
/* --- pure: panel, scales, layouts ------------------------------------------------------ */
static void panel_notes(void) {
    tc_chord none = {0, 0, 0};
    tc_panel neutral = TC_PANEL_NEUTRAL;
    /* The neutral panel is exactly the unshaped chord, everywhere. */
    for (uint8_t mode = 0; mode < TC_SCALES; ++mode) for (uint8_t tonic = 0; tonic < 12u; ++tonic) {
        tp_layout l = {tonic, TP_MELODY_BASE, TP_CHORD_BASE, mode, TP_MELODY_SCALE, 0u, 0u};
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            tp_notes a, b;
            assert(tc_notes(l, k, none, &a) && tc_notes_panel(l, k, none, neutral, &b));
            assert(!memcmp(&a, &b, sizeof a));
        }
    }
    tp_layout c = TP_DEFAULT_LAYOUT;
    tp_notes n;
    const uint8_t root_pos[] = {36, 40, 43}, inv1[] = {40, 43, 48}, inv2[] = {43, 48, 52}, inv3[] = {48, 52, 55};
    assert(tc_notes_panel(c, 3, none, neutral, &n) && same_notes(&n, root_pos, 3));
    tc_panel p = neutral;
    p.voicing = 1; assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, inv1, 3));
    p.voicing = 2; assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, inv2, 3));
    p.voicing = 3; assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, inv3, 3));
    const uint8_t top_inv1[] = {52, 55, 60}; /* the top row is one more octave up the ladder */
    p.voicing = 1; assert(tc_notes_panel(c, 0, none, p, &n) && same_notes(&n, top_inv1, 3));
    /* Guitar (0.7.0): open C x32010 on the bottom row; row 1 is the hand five frets up:
     * C at the 8th fret on the low E, then E G C E C. */
    const uint8_t guitar[] = {48, 52, 55, 60, 64}, guitar2[] = {48, 52, 55, 60, 64, 72};
    p.voicing = TC_VOICING_GUITAR; assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, guitar, 5));
    assert(tc_notes_panel(c, 1, none, p, &n) && same_notes(&n, guitar2, 6));
    p = neutral; p.spread = 3;
    const uint8_t spread3[] = {0, 40, 43}; assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, spread3, 3));
    p.spread = 1; p.octaves = 0x18; /* spread I and octaves 0, +1 */
    const uint8_t stacked[] = {24, 36, 40, 43, 52, 55};
    assert(tc_notes_panel(c, 3, none, p, &n) && same_notes(&n, stacked, 6));
    p = neutral; p.octaves = 0x7F; /* all seven octaves: capped at 16 notes, lowest first */
    assert(tc_notes_panel(c, 3, none, p, &n) && n.count == TP_MAX_NOTES && n.notes[0] == 0 && n.notes[15] == 60);
    /* Shapes that leave the MIDI range only lose those notes. */
    tp_layout high = c; high.chord_base = TP_CHORD_BASE + TP_OCTAVE_RANGE;
    p = neutral; p.voicing = 3; p.octaves = 0x40; /* +3 octaves */
    assert(tc_notes_panel(high, 0, none, p, &n));
    for (uint32_t i = 0; i < n.count; ++i) assert(n.notes[i] <= 127u);
    /* Invalid panel values are refused. */
    p = neutral; p.voicing = TC_VOICINGS; assert(!tc_notes_panel(c, 3, none, p, &n));
    p = neutral; p.spread = TC_SPREADS; assert(!tc_notes_panel(c, 3, none, p, &n));
    p = neutral; p.octaves = 0x80; assert(!tc_notes_panel(c, 3, none, p, &n));
    p = neutral; p.capo = TC_CAPOS; assert(!tc_notes_panel(c, 3, none, p, &n));
}
static void scales_and_layouts(void) {
    for (uint8_t i = 0; i < TC_ALL_SCALES; ++i) {
        assert(tc_scale_position(tc_scale_at(i)) == i);
        uint8_t id = tc_scale_at(i);
        if (id < TC_SCALES || id >= TC_EXTRA_FIRST) assert(tc_scale_defined(id));
    }
    assert(!strcmp(tc_scale_name(TC_EXTRA_FIRST), "BLUES") && tc_scale_size(TC_EXTRA_FIRST) == 6u);
    assert(tc_in_scale(TC_EXTRA_FIRST, 6u) && !tc_in_scale(TC_EXTRA_FIRST, 4u)); /* blue note, no major third */
    /* Menu order: classic, the more built-in scales and the library (0.7.0), then U1..U12. */
    assert(tc_scale_at(TC_SCALES) == TC_EXTRA_FIRST && tc_scale_at(TC_SCALES + TC_EXTRA_SCALES) == TC_LIBRARY_FIRST &&
           tc_scale_at(TC_ALL_SCALES - TC_USER_SCALES) == TC_SCALES);
    assert(tc_set_user_scale(11, 0xAD5u, "slot twelve") && tc_scale_defined(TC_SCALES + 11u));
    assert(!tc_set_user_scale(12, 0xAD5u, "x"));
    (void)tc_set_user_scale(11, 0, NULL);
    tp_layout l = TP_DEFAULT_LAYOUT; tp_notes n; tc_chord none = {0, 0, 0};
    l.melody_layout = TP_MELODY_CHROMATIC;
    assert(tc_notes(l, key_of(1, 5, 3), none, &n) && n.notes[0] == 53);  /* F3: 5 semitones */
    assert(tc_notes(l, key_of(1, 5, 0), none, &n) && n.notes[0] == 89);  /* three octaves up */
    l.melody_layout = TP_MELODY_FOURTHS;
    assert(tc_notes(l, key_of(1, 2, 1), none, &n) && n.notes[0] == 60);  /* 2 + 2 rows of fourths */
    l.melody_layout = TP_MELODY_DRUMS;
    assert(tc_notes(l, key_of(1, 8, 3), none, &n) && n.notes[0] == 36);  /* kick, bottom left */
    assert(tc_notes(l, key_of(1, 11, 0), none, &n) && n.notes[0] == 51); /* top right */
    assert(tc_notes(l, key_of(1, 0, 3), none, &n) && n.notes[0] == 48);  /* scale pad: root */
    assert(tc_notes(l, key_of(1, 1, 3), none, &n) && n.notes[0] == 50);  /* next scale step */
    assert(tc_notes(l, key_of(1, 4, 3), none, &n) && n.notes[0] == 36);  /* second pad: an octave down */
    l.melody_layout = TP_MELODY_LAYOUTS; assert(!tc_layout_ok(l));
    tc_controls c; tc_init(&c); c.melody_layout = TP_MELODY_DRUMS; c.melody_ch = 2u; c.bass_ch = 3u; c.drum_ch = 9u;
    assert(tc_channel_of(&c, key_of(1, 0, 0)) == 2u && tc_channel_of(&c, key_of(1, 4, 0)) == 3u);
    assert(tc_channel_of(&c, key_of(1, 8, 0)) == 9u && tc_channel_of(&c, TP_PEDAL) == 9u);
    assert(tc_channel_of(&c, 3u) == c.chord_ch && tc_channel_of(&c, TP_ARP) == c.chord_ch);
    c.melody_layout = TP_MELODY_SCALE; assert(tc_channel_of(&c, key_of(1, 8, 0)) == 2u);
}
/* --- engine ----------------------------------------------------------------------------- */
static void strum_extras(void) {
    defaults(); tc_state.strum = TC_STRUM_UP; tc_state.strum_speed = TC_STRUM_MS + 4u; /* 1/16 = 6 clocks */
    clear_log(); pad(3, true); run(400);
    assert(count(0x90) == 3u && nth(0x90, 1)->at - nth(0x90, 0)->at == 125u); /* at 120 BPM */
    pad(3, false); run(50); quiet();
    tc_state.strum_off = 1u << 3; clear_log(); pad(3, true); pad(7, true); run(400);
    assert(count(0x90) == 6u);
    /* Pad 3 has strum off: its notes go out back to back (one packet a millisecond);
     * pad 7 still strums, a 1/16 apart. */
    assert(at_of(0x90, 43) - at_of(0x90, 36) <= 5u && at_of(0x90, 45) - at_of(0x90, 38) >= 240u);
    pad(3, false); pad(7, false); run(50); quiet(); tc_state.strum_off = 0u;
    /* Repeat 1/4 (500 ms) while held, louder as pressure rises; strum time 20 ms. */
    tc_state.strum_speed = 2u; tc_state.strum_repeat = 1u; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_DYNAMIC;
    clear_log(); scan.values[3] = 2600u; run(250); scan.values[3] = 4000u; run(1100);
    assert(count(0x90) == 9u && count(0x80) == 6u);
    assert(nth(0x90, 3)->at - nth(0x90, 0)->at >= 500u && nth(0x90, 3)->at - nth(0x90, 0)->at <= 510u);
    assert(nth(0x90, 3)->b > nth(0x90, 0)->b); /* the second strike comes in harder */
    pad(3, false); run(100); quiet();
    /* Cutoff 1/8 (250 ms): the chord ends while still held, and does not strike again. */
    tc_state.strum_repeat = 0u; tc_state.strum_cutoff = 3u; clear_log(); pad(3, true); run(800);
    assert(count(0x90) == 3u && count(0x80) == 3u && nth(0x80, 0)->at - nth(0x90, 0)->at >= 250u);
    pad(3, false); run(50); pad(3, true); run(50); assert(count(0x90) == 6u); /* lift and touch: again */
    pad(3, false); run(400); quiet();
    /* Repeat with cutoff: on, off at 1/8, on again at 1/4. */
    tc_state.strum_repeat = 1u; clear_log(); pad(3, true); run(1100);
    assert(count(0x90) == 9u && count(0x80) >= 6u);
    pad(3, false); run(400); quiet();
    tc_state.strum_repeat = 0u; tc_state.strum_cutoff = 0u; tc_state.strum = TC_STRUM_OFF;
}
static void arp_extras(void) {
    defaults(); tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; tc_state.arp_gate = 1u; /* 1/16: 125 ms */
    /* Rhythm X.X.X.X.: every other step silent, the pattern still moves on. */
    tc_state.arp_rhythm = 1u; clear_log(); pad(3, true); run(1000);
    assert(count(0x90) == 4u && nth(0x90, 1)->at - nth(0x90, 0)->at == 250u);
    assert(nth(0x90, 0)->a == 36 && nth(0x90, 1)->a == 43); /* steps 0 and 2 of C E G */
    pad(3, false); run(200); quiet();
    /* Swing 50 %: 1/16 pairs of 9 and 3 clocks (187.5 and 62.5 ms). */
    tc_state.arp_rhythm = 0u; tc_state.arp_swing = 5u; clear_log(); pad(3, true); run(1000);
    uint32_t d1 = nth(0x90, 1)->at - nth(0x90, 0)->at, d2 = nth(0x90, 2)->at - nth(0x90, 1)->at;
    assert(d1 >= 186u && d1 <= 189u && d2 >= 61u && d2 <= 64u);
    pad(3, false); run(200); quiet(); tc_state.arp_swing = 0u;
    /* Chords: the whole held chord on every step. */
    tc_state.arp = TC_ARP_CHORDS; clear_log(); pad(3, true); run(500);
    assert(count(0x90) >= 9u && count(0x90) % 3u == 0u && nth(0x90, 2)->at - nth(0x90, 0)->at <= 3u);
    pad(3, false); run(200); quiet();
    /* Latch: the arpeggio continues after the hand lifts; touching the pad again stops it. */
    tc_state.arp = TC_ARP_UP; tc_state.arp_latch = 1u; pad(3, true); run(50); pad(3, false); clear_log(); run(600);
    assert(count(0x90) >= 4u && e.keys[3].latched);
    pad(3, true); run(50); pad(3, false); run(300); clear_log(); run(400); assert(!count(0x90));
    quiet(); tc_state.arp_latch = 0u;
    /* Sync: the first step waits for the clock's next step boundary. */
    tc_state.arp_sync = 1u; run(7); clear_log(); uint32_t touched = now; pad(3, true); run(600);
    assert(count(0x90) >= 3u && (e.clock_ticks - 0u) > 0u);
    assert(nth(0x90, 0)->at - touched <= 126u && nth(0x90, 1)->at - nth(0x90, 0)->at >= 124u &&
           nth(0x90, 1)->at - nth(0x90, 0)->at <= 126u);
    pad(3, false); run(200); quiet(); tc_state.arp_sync = 0u; tc_state.arp = TC_ARP_OFF;
}
static void expression(void) {
    defaults(); uint8_t v[3];
    const uint8_t modes[3] = {TC_VEL_LIGHT, TC_VEL_DYNAMIC, TC_VEL_FIRM};
    for (uint32_t i = 0; i < 3u; ++i) {
        tc_state.vel_mode = tc_state.melody_vel = modes[i]; clear_log(); scan.values[51] = 3000u; run(30);
        v[i] = nth(0x90, 0)->b; pad(51, false); run(30);
    }
    assert(v[0] > v[1] && v[1] > v[2]);
    /* Pitch bend from pressure, back to the centre on release. */
    tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED; tc_state.at_melody = TC_AT_BEND; tc_state.melody_ch = 4u; clear_log();
    scan.values[51] = 4000u; run(200);
    const event *b = nth(0xE4, count(0xE4) - 1u);
    assert(b && b->b == 127u); /* full pressure: full scale up */
    pad(51, false); run(200); b = nth(0xE4, count(0xE4) - 1u);
    assert(b->a == 0u && b->b == 64u); quiet(); tc_state.at_melody = TC_AT_OFF;
}
static void pedal_jobs(void) {
    defaults(); tc_state.pedal_mode = TC_PEDAL_SOSTENUTO; clear_log();
    tc_state.sustain = true; run(5); assert(nth(0xB0, 0)->a == 66u && nth(0xB0, 0)->b == 127u);
    tc_state.pedal_mode = TC_PEDAL_SOFT; run(5); /* the controller changed: off on the old one, on the new */
    assert(nth(0xB0, 1)->a == 66u && nth(0xB0, 1)->b == 0u && nth(0xB0, 2)->a == 67u);
    tc_state.sustain = false; run(5); quiet();
    /* Note: the pedal plays its note on the drum channel while down. */
    tc_state.pedal_mode = TC_PEDAL_NOTE; tc_state.drum_ch = 9u; tc_state.pedal_note = 38u; clear_log();
    tc_state.pedal_down_note = true; run(5); assert(count(0x99) == 1u && nth(0x99, 0)->a == 38u);
    tc_state.pedal_down_note = false; run(5); assert(count(0x89) == 1u); quiet();
    /* Transport: Start and Stop bytes. */
    clear_log(); tc_state.transport = true; run(5); assert(count(0xFA) == 1u);
    tc_state.transport = false; run(5); assert(count(0xFC) == 1u);
    /* The pedal input in transport mode toggles it. */
    tc_controls c; tc_init(&c); c.pedal_mode = TC_PEDAL_TRANSPORT;
    tc_pedal(&c, true, 0); tc_pedal(&c, true, 20); tc_pedal(&c, false, 30); tc_pedal(&c, false, 40);
    assert(c.transport); tc_pedal(&c, true, 50); tc_pedal(&c, true, 60); tc_pedal(&c, false, 70); tc_pedal(&c, false, 80);
    assert(!c.transport);
}
static void clock_out(void) {
    defaults(); tm_reset(); tc_state.clock_out = 1u; run(100); clear_log(); run(1000);
    uint32_t n = count(0xF8);
    assert(n >= 47u && n <= 49u); /* 120 BPM: 48 clocks a second */
    for (uint32_t i = 1; i + 1u < n; ++i) {
        uint32_t d = nth(0xF8, i + 1u)->at - nth(0xF8, i)->at; assert(d >= 20u && d <= 22u);
    }
    tc_state.clock_out = 0u; run(100); clear_log(); run(500); assert(!count(0xF8));
    /* A MIDI clock coming in wins: no clock out while it runs. */
    tc_state.clock_out = 1u; uint8_t f8[4] = {0x0F, 0xF8, 0, 0};
    clear_log(); for (uint32_t i = 0; i < 40u; ++i) { tm_packet(f8, now); run(20); }
    assert(count(0xF8) <= 2u); tc_state.clock_out = 0u; tm_reset(); run(400); quiet();
}
static void live_update(void) {
    defaults(); tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED; clear_log();
    uint8_t k = key_of(1, 0, 3); pad(k, true); run(30); assert(sounding[0][48]);
    tp_layout l = tc_layout(&tc_state); l.tonic = 2u;
    assert(tc_set_layout(&tc_state, &e, l)); run(30);
    assert(sounding[0][48] && !sounding[0][50]); /* off: the held pad keeps its note */
    pad(k, false); run(30); quiet();
    l.tonic = 0u; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.live_update = 1u; pad(k, true); run(30); assert(sounding[0][48]);
    l.tonic = 2u; assert(tc_set_layout(&tc_state, &e, l)); run(30);
    assert(!sounding[0][48] && sounding[0][50]); /* on: it moves to the new key while held */
    pad(k, false); run(30); quiet(); tc_state.live_update = 0u;
}
static void drum_layout(void) {
    defaults(); tc_state.melody_layout = TP_MELODY_DRUMS; tc_state.drum_ch = 9u; tc_state.bass_ch = 2u;
    e.layout = tc_layout(&tc_state); clear_log();
    pad(key_of(1, 8, 3), true); pad(key_of(1, 4, 3), true); run(30);
    assert(count(0x99) == 1u && nth(0x99, 0)->a == 36u && count(0x92) == 1u && nth(0x92, 0)->a == 36u);
    pad(key_of(1, 8, 3), false); pad(key_of(1, 4, 3), false); run(30); quiet();
}
int main(void) {
    panel_notes(); scales_and_layouts();
    tc_init(&tc_state); tm_reset();
    now = 1000u; run(600); assert(e.state == TK_ACTIVE);
    strum_extras(); arp_extras(); expression(); pedal_jobs(); clock_out(); live_update(); drum_layout();
    puts("PASS: stage 3: chord panel (neutral = unshaped chord on every pad/scale/key; voicing ladder, guitar, spread, octave stacks, 16-note cap, range drops, invalid refused), 37 scales in menu order, 12 user slots, chromatic/fourths/drums layouts and channels, strum at the tempo / per pad / repeat with pressure / cutoff, arp rhythm / swing / chords / latch / sync, velocity curves, pitch bend, pedal sostenuto/soft/note/transport, MIDI clock out (and silent under MIDI clock in), live update, drum pad channels");
    return 0;
}
