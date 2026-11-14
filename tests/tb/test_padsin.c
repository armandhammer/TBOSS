/* SPDX-License-Identifier: GPL-3.0-or-later - 2.6.1, pads from MIDI in (a user's request: another controller's pads play
 * the board's chords), on the real engine, controls and settings (a fake sector 11). The Pad channel's 48 notes and the
 * pads they press (an octave a row from the bottom row, C to B the columns); a note plays its pad's chord on the chord
 * channel with the note's velocity (no curve; Fixed still 100) and ends it; a note that ends in the pass it began in, one
 * struck again while held, and one whose last Note Offs are still on their way all play; aftertouch (per note, or the
 * channel's) is the pad's pressure and goes out as the half's pressure setting says; Hold latches; a finger on the pad
 * and a note for it leave each other alone; on the step screen a note plays and edits nothing; All Notes Off, an
 * unplug, the board's own All notes off and a change of the channel or its first note let go; a note that came with no
 * session playing presses nothing later; with the channel off the notes are the pad lights' as before; the two menu
 * items, their values and names, the settings record (round trip, refusals, after a restart), presets leave them, the
 * factory settings put them back. 1 ms ticks, a fresh scan frame every tick. */
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/ui_internal.h"
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
static bool late;          /* receipts a pass after the packets (a Note Off still on its way) */
static uint32_t late_token;
static bool writer(const uint8_t p[4], uint32_t token) {
    assert((token == e.issued + 1u || (e.pending_id && token == e.pending_id)) && logged < 16384u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (late && late_token && e.pending_id == late_token) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    late_token = e.pending_id;
    if (!late && e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
    st_requests(&tc_state, &e);
    (void)st_service(&tc_state, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void midi(uint8_t status, uint8_t a, uint8_t b) {
    const uint8_t p[4] = {(uint8_t)(status >> 4), status, a, b}; tm_packet(p, now);
}
/* The Pad channel in these tests is 16; the chords play on channel 2. */
static void on(uint8_t note, uint8_t velocity) { midi(0x9Fu, note, velocity); }
static void off(uint8_t note) { midi(0x8Fu, note, 0u); }
static uint32_t ons(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += (log_[j].status >> 4) == TK_ON; return n; }
static uint32_t offs(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += (log_[j].status >> 4) == TK_OFF; return n; }
static uint32_t with(uint8_t status) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += log_[j].status == status; return n; }
static int last(uint8_t status) { for (uint32_t j = logged; j-- > 0;) if (log_[j].status == status) return log_[j].a; return -1; }
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) n += (uint32_t)sounding[ch][p];
    return n;
}
static void quiet(void) { assert(!heard() && !tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE); }
static tp_notes chord(uint32_t k) { tp_notes n; assert(tc_pad(e.layout, k, &n) && n.count); return n; }
/* Pad k's chord sounds, on the chord channel, and nothing else does. */
static bool plays(uint32_t k) {
    tp_notes n = chord(k);
    for (uint32_t i = 0; i < n.count; ++i) if (sounding[1][n.notes[i]] != 1) return false;
    return heard() == n.count;
}
static void fresh(void) {
    memset(scan.values, 0, sizeof scan.values); memset(&e, 0, sizeof e); memset(sounding, 0, sizeof sounding);
    memset(flash, 0xFF, sizeof flash); tm_reset(); tc_init(&tc_state); (void)st_load(&tc_state);
    for (uint32_t p = 0; p < LN_PATTERNS; ++p) ln_clear(&ln_bank[p]);
    late = false; late_token = 0u;
    tc_state.unstick = 0u; tc_state.pad_trigger = 7u; tc_state.pad_release = 3u; tc_state.pad_full = 25u;
    tc_state.chord_ch = 1u; tc_state.melody_ch = 0u; tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    e.layout = tc_layout(&tc_state);
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 16u));
    run(1000); assert(e.state == TK_ACTIVE && tm.pad_channel == 16u && tm.pad_first == TC_PAD_NOTE_DEFAULT);
    logged = 0;
}
#define C2 36u /* the default first note: column 1 of the bottom row */
#define PAD_C2 3u  /* 4 * column + row, row 3 the bottom */
#define E2 40u
#define PAD_E2 19u
#define G2 43u
#define PAD_G2 31u

static void mapping(void) {
    fresh();
    assert(tm_pad_of(36) == 3u && tm_pad_of(47) == 47u);  /* the bottom row, C to B */
    assert(tm_pad_of(48) == 2u && tm_pad_of(60) == 1u);   /* each octave up, a row up */
    assert(tm_pad_of(72) == 0u && tm_pad_of(83) == 44u);  /* the top row */
    assert(tm_pad_of(35) == TM_PADS && tm_pad_of(84) == TM_PADS && tm_pad_of(0) == TM_PADS && tm_pad_of(127) == TM_PADS);
    uint64_t seen = 0u;
    for (uint8_t p = 36; p < 84u; ++p) {
        uint32_t k = tm_pad_of(p); tp_position at;
        assert(k < TM_PADS && tp_position_of(k, &at) && at.side == 0u && at.column == (p - 36u) % 12u && at.row == 3u - (p - 36u) / 12u);
        seen |= (uint64_t)1u << k;
    }
    assert(seen == ((uint64_t)1u << 48) - 1u); /* every pad of the left half once */
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, 0u)); run(2);
    assert(tm_pad_of(0) == 3u && tm_pad_of(47) == 44u && tm_pad_of(48) == TM_PADS);
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, TC_PAD_NOTE_MAX)); run(2);
    assert(tm_pad_of(80) == 3u && tm_pad_of(127) == 44u && tm_pad_of(79) == TM_PADS);
    assert(!tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, TC_PAD_NOTE_MAX + 1u));
    puts("mapping: 48 notes from the first, C to B the columns, an octave a row from the bottom; the first note 0..80");
}
static void playing(void) {
    fresh();
    tp_notes n = chord(PAD_C2);
    on(C2, 77); run(1);
    assert(plays(PAD_C2) && ons() == n.count && e.keys[PAD_C2].sensed && e.keys[PAD_C2].remote && e.keys[PAD_C2].admitted);
    for (uint32_t j = 0; j < logged; ++j) assert(log_[j].status == 0x91u && log_[j].b == 77u); /* the chord channel, its velocity */
    assert(tc_state.have_chord && tc_state.last_chord == PAD_C2 && tc_state.live_pad == PAD_C2);
    assert(!tm.notes[C2] && !tm.pad_changed); /* a pad's note is not a light */
    run(500); assert(plays(PAD_C2) && ons() == n.count); /* held */
    off(C2); run(2);
    quiet(); assert(offs() == n.count && !e.keys[PAD_C2].sensed && !e.keys[PAD_C2].remote && !e.keys[PAD_C2].admitted);
    /* Two at once, each its own chord; the rows are the octaves. */
    logged = 0; on(E2, 100); on(60, 100); run(2);
    assert(e.keys[PAD_E2].admitted && e.keys[1].admitted && tc_state.last_chord == PAD_E2); /* lowest pad last in the walk: 1 then 19 */
    off(E2); off(60); run(2); quiet();
    /* A note outside the 48 on the Pad channel, and a pad's note on another channel: the lights', as ever. */
    logged = 0; on(20, 100); midi(0x90u, C2, 100); run(2);
    assert(!ons() && tm.notes[20] == 1u && tm.notes[C2] == 1u && !tm.pad_held);
    off(20); midi(0x80u, C2, 0); run(2); assert(!tm.notes[20] && !tm.notes[C2]);
    /* A Note On with velocity 0 is the note's end. */
    on(C2, 100); run(2); assert(plays(PAD_C2)); on(C2, 0); run(2); quiet();
    puts("playing: a note plays its pad's chord on the chord channel with its velocity and ends it; not a pad light");
}
static void taps(void) {
    fresh();
    uint32_t c = chord(PAD_E2).count;
    on(E2, 90); off(E2); run(1); assert(plays(PAD_E2) && ons() == c); /* over in the pass it began in: it plays */
    run(1); quiet(); assert(offs() == c);
    /* Struck again while held: its end, then its start, the new velocity. */
    on(E2, 90); run(5); logged = 0; on(E2, 50); run(1); assert(offs() == c && !ons());
    run(1); assert(plays(PAD_E2) && ons() == c && log_[logged - 1u].b == 50u);
    off(E2); run(2); quiet();
    /* Its end and its start in one pass (a sequencer's notes back to back). */
    on(E2, 90); run(5); logged = 0; off(E2); on(E2, 60); run(3);
    assert(offs() == c && ons() == c && plays(PAD_E2) && log_[logged - 1u].b == 60u);
    off(E2); run(2); quiet();
    /* On, off, on, off in one pass: one strike. */
    logged = 0; on(E2, 90); off(E2); on(E2, 91); off(E2); run(5); quiet(); assert(ons() == c && offs() == c);
    /* The last strike's Note Offs still on their way: the next one waits for them and plays. */
    late = true;
    logged = 0; on(E2, 90); run(5); assert(plays(PAD_E2));
    off(E2); on(E2, 70); off(E2); run(10);
    quiet(); assert(ons() == 2u * c && offs() == 2u * c && log_[logged - 1u - c].b == 70u);
    for (uint32_t i = 0; i < 20u; ++i) { on(E2, 100); off(E2); run(1); } /* a roll, a strike a pass */
    run(10); quiet(); assert(ons() == offs() && ons() >= 2u * c + 5u * c);
    late = false;
    puts("taps: a note over in its pass, struck again, back to back, and waiting on its last Note Offs all play");
}
static void velocity(void) {
    fresh();
    static const uint8_t v[5] = {1, 2, 64, 126, 127};
    for (uint32_t i = 0; i < 5u; ++i) {
        logged = 0; on(C2, v[i]); run(2); assert(logged && log_[0].b == v[i]); off(C2); run(2); quiet();
    }
    tc_state.vel_mode = TC_VEL_FIRMEST; logged = 0; on(C2, 64); run(2); assert(log_[0].b == 64u); off(C2); run(2); /* no curve */
    tc_state.vel_mode = TC_VEL_LIGHT; logged = 0; on(C2, 64); run(2); assert(log_[0].b == 64u); off(C2); run(2);
    tc_state.vel_mode = TC_VEL_FIXED; logged = 0; on(C2, 64); run(2); assert(log_[0].b == 100u); off(C2); run(2); /* Fixed is 100 */
    quiet();
    puts("velocity: the note's own, whatever the curve; Fixed 100");
}
static void pressure(void) {
    fresh();
    tc_state.at_chord = TC_AT_CHANNEL;
    on(C2, 100); run(50); assert(!with(0xD1u));
    midi(0xAFu, C2, 90); run(200); assert(last(0xD1u) == 90 && e.keys[PAD_C2].pressure == 90u && e.keys[PAD_C2].at == 90u);
    midi(0xDFu, 40, 0); run(200); assert(last(0xD1u) == 40); /* the channel's pressure: every pad held */
    midi(0xAFu, E2, 120); assert(!tm.pad_changed); /* a pad no note holds */
    midi(0xA0u, C2, 120); midi(0xD0u, 120, 0); run(200); assert(last(0xD1u) == 40); /* another channel's */
    off(C2); run(200); assert(last(0xD1u) == 0); quiet();
    midi(0xDFu, 55, 0); on(E2, 100); run(200); assert(last(0xD1u) == 55); /* a new note starts at the channel's pressure */
    midi(0xDFu, 0, 0); off(E2); run(200); assert(last(0xD1u) == 0); quiet();
    /* Poly aftertouch out (the default): each of the chord's notes. */
    tc_state.at_chord = TC_AT_POLY; logged = 0;
    uint32_t c = chord(PAD_C2).count;
    on(C2, 100); run(50); midi(0xAFu, C2, 80); run(300);
    assert(with(0xA1u) == c);
    for (uint32_t j = 0; j < logged; ++j) if (log_[j].status == 0xA1u) assert(log_[j].b == 80u);
    off(C2); run(50); quiet();
    puts("pressure: poly aftertouch on the note or the channel's pressure is the pad's, out as the half's setting says");
}
static void hold(void) {
    fresh();
    tc_state.hold = 1u;
    on(C2, 100); run(5); off(C2); run(5); assert(plays(PAD_C2) && e.keys[PAD_C2].latched);
    on(G2, 100); run(5); assert(!e.keys[PAD_C2].admitted && e.keys[PAD_G2].admitted && plays(PAD_G2));
    off(G2); run(5); assert(plays(PAD_G2));
    on(G2, 100); run(5); assert(!heard()); off(G2); run(5); /* the latched pad itself: silenced */
    quiet();
    on(C2, 100); run(5); off(C2); run(5); assert(plays(PAD_C2)); tc_state.hold = 0u; run(5); quiet();
    puts("hold: a note's chord stays, the next one replaces it, the same one again silences it");
}
static void finger(void) {
    fresh();
    scan.values[PAD_C2] = 3000u; run(30); assert(plays(PAD_C2) && !e.keys[PAD_C2].remote);
    uint32_t before = ons();
    on(C2, 100); run(5); assert(ons() == before && !e.keys[PAD_C2].remote && plays(PAD_C2)); /* the finger's */
    off(C2); run(5); assert(plays(PAD_C2));
    scan.values[PAD_C2] = 0u; run(30); quiet();
    on(C2, 100); run(5); before = ons(); /* the note's: the pad's readings wait */
    scan.values[PAD_C2] = 3000u; run(30); assert(ons() == before && e.keys[PAD_C2].remote && plays(PAD_C2));
    scan.values[PAD_C2] = 0u; run(30); assert(plays(PAD_C2));
    off(C2); run(5); quiet();
    /* Another pad under a finger plays beside a note's. */
    on(C2, 100); run(5); scan.values[PAD_G2] = 3000u; run(30); assert(e.keys[PAD_C2].admitted && e.keys[PAD_G2].admitted);
    scan.values[PAD_G2] = 0u; off(C2); run(30); quiet();
    puts("finger: a pad a finger is on ignores its note; a pad a note holds ignores its readings");
}
static void steps(void) {
    fresh();
    tc_open_steps(&tc_state); assert(tc_state.ui == TC_UI_STEPS);
    tc_state.step_silent = 1u; run(2);
    scan.values[PAD_C2] = 3000u; run(30); assert(!ons()); scan.values[PAD_C2] = 0u; run(30); /* Step pads Silent: a finger's */
    uint32_t changes = ln_changes, c = chord(PAD_E2).count; ln_step brush = tc_state.brush[tc_state.lane % LN_LANES];
    logged = 0; on(E2, 88); run(2);
    assert(plays(PAD_E2) && ons() == c && log_[0].b == 88u && tc_state.last_chord == PAD_E2);
    off(E2); run(2); quiet();
    assert(ln_changes == changes && !memcmp(&brush, &tc_state.brush[tc_state.lane % LN_LANES], sizeof brush)); /* edits nothing */
    /* A right-half step pad under a finger still edits, beside a note playing. */
    on(E2, 88); run(2); scan.values[48u + 3u] = 3000u; run(30); scan.values[48u + 3u] = 0u; run(30);
    assert(ln_changes != changes && plays(PAD_E2));
    off(E2); run(2); quiet();
    tc_state.ui = TC_UI_PLAY;
    puts("steps: on the step screen a note plays its chord as on the play screen and edits nothing");
}
static void letting_go(void) {
    fresh();
    on(C2, 100); on(E2, 100); run(5); assert(heard());
    midi(0xB3u, 123, 0); run(5); quiet(); assert(!e.keys[PAD_C2].remote && !e.keys[PAD_E2].remote && !tm.pad_held);
    off(C2); off(E2); run(5); quiet(); /* their ends: nothing */
    on(C2, 100); run(5); midi(0xB0u, 120, 0); run(5); quiet();
    on(C2, 100); run(5); tm_disconnected(); run(5); quiet(); assert(!e.keys[PAD_C2].remote);
    /* The board's own All notes off: quiet until the note ends. */
    on(C2, 100); run(5); tc_state.panic = true; run(5); assert(!heard() && e.keys[PAD_C2].remote && e.keys[PAD_C2].inhibited);
    run(200); assert(!heard()); off(C2); run(5); quiet(); assert(!e.keys[PAD_C2].remote && !e.keys[PAD_C2].inhibited);
    on(C2, 100); run(5); assert(plays(PAD_C2)); off(C2); run(5); quiet();
    /* The channel or its first note changed while a note holds a pad. */
    on(C2, 100); run(5); assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 5u)); run(5); quiet();
    off(C2); on(C2, 100); run(5); assert(!heard() && tm.notes[C2] == 1u); off(C2); /* channel 16: a light's now */
    midi(0x94u, C2, 100); run(5); assert(plays(PAD_C2));
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, 48u)); run(5); quiet();
    midi(0x84u, C2, 0); run(5); assert(!tm.notes[C2]); /* its end, a light's now: nothing left lit */
    midi(0x94u, 48, 100); run(5); assert(plays(PAD_C2)); midi(0x84u, 48, 0); run(5); quiet();
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 0u)); run(5);
    puts("letting go: All Notes Off, an unplug, the board's All notes off, and a change of the channel or first note");
}
static void sessions(void) {
    fresh();
    memset(&e, 0, sizeof e); /* no session (as after a restart), notes arriving meanwhile */
    on(C2, 100); off(C2); on(E2, 100);
    run(1000); assert(e.state == TK_ACTIVE && !ons() && !e.keys[PAD_E2].sensed); /* nothing plays later, held or not */
    off(E2); run(5); assert(!ons()); quiet();
    on(E2, 100); run(5); assert(plays(PAD_E2)); off(E2); run(5); quiet();
    /* A session that ends under a held note: the note Offs go out, and its end later does nothing. */
    on(C2, 100); run(5); tp_disable(&e); run(1500); assert(e.state == TK_ACTIVE && !heard() && !e.keys[PAD_C2].remote);
    off(C2); run(5); quiet();
    on(C2, 100); run(5); assert(plays(PAD_C2)); off(C2); run(5); quiet();
    puts("sessions: a note that came with no session playing presses nothing later");
}
static void channel_off(void) {
    fresh();
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 0u)); run(2);
    uint64_t turned = e.remote_turned;
    on(C2, 100); midi(0xAFu, C2, 90); midi(0xDFu, 90, 0); run(5);
    assert(!ons() && !tm.pad_changed && !tm.pad_held && tm.notes[C2] == 1u && !turned && !e.remote_turned); /* a light, as before */
    off(C2); run(5); assert(!tm.notes[C2]);
    puts("channel off: nothing waits for the engine; the notes are the pad lights' as before");
}
static bool on_page(uint8_t group, uint8_t item) {
    uint8_t items[TC_GROUP_MAX], n = tc_page_items(&tc_state, group, items);
    for (uint8_t i = 0; i < n; ++i) if (items[i] == item) return true;
    return false;
}
static uint8_t byte(uint32_t w, uint32_t n) { return (uint8_t)(w >> (8u * n)); }
static void settings(void) {
    fresh();
    assert(on_page(TC_GROUP_MIDI, TC_MENU_PAD_CH) && on_page(TC_GROUP_MIDI, TC_MENU_PAD_NOTE));
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 0u));
    assert(on_page(TC_GROUP_MIDI, TC_MENU_PAD_CH) && !on_page(TC_GROUP_MIDI, TC_MENU_PAD_NOTE)); /* off: the note hidden */
    assert(tc_menu_values(TC_MENU_PAD_CH) == 17u && tc_menu_values(TC_MENU_PAD_NOTE) == TC_PAD_NOTE_MAX + 1u);
    assert(!strcmp(ui_item_name(TC_MENU_PAD_CH), "PAD CH") && !strcmp(ui_item_name(TC_MENU_PAD_NOTE), "PAD NOTE"));
    assert(!strcmp(ui_item_name(TC_MENU_HUMANIZE), "HUMANIZE") && !strcmp(ui_item_name(TC_MENU_BUTTON1), "VOICING I")); /* the table in step */
    assert(!strcmp(ui_value_text(TC_MENU_PAD_CH, 0), "OFF") && !strcmp(ui_value_text(TC_MENU_PAD_CH, 16), "CH 16"));
    assert(!strcmp(ui_value_text(TC_MENU_PAD_NOTE, 36), "C2 36") && !strcmp(ui_value_text(TC_MENU_PAD_NOTE, 80), "G#5 80") &&
           !strcmp(ui_value_text(TC_MENU_PAD_NOTE, 0), "C-1 0"));
    assert(tc_item_kind(TC_MENU_PAD_CH) == TC_ITEM_VALUE && tc_item_kind(TC_MENU_PAD_NOTE) == TC_ITEM_VALUE);
    assert(!tc_cc_mappable(TC_MENU_PAD_CH) && !tc_cc_mappable(TC_MENU_PAD_NOTE)); /* not for a knob */
    assert(!tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 17u));
    /* The record: word 38's bytes 2 and 3; the defaults are zeros. */
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && !(p[38] >> 16));
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 9u) && tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, 48u));
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && byte(p[38], 2) == 9u && byte(p[38], 3) == 49u);
    tc_state.pad_ch = 0u; tc_state.pad_note = TC_PAD_NOTE_DEFAULT;
    assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_OK && tc_state.pad_ch == 9u && tc_state.pad_note == 48u);
    memcpy(q, p, sizeof q); q[38] = (q[38] & 0xFF00FFFFu) | 17u << 16; assert(st_put(ST_LIVE, 0, q, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[38] = (q[38] & 0x00FFFFFFu) | (TC_PAD_NOTE_MAX + 2u) << 24; assert(st_put(ST_LIVE, 0, q, &tc_state, &e) == ST_BAD_VALUE);
    assert(tc_state.pad_ch == 9u && tc_state.pad_note == 48u);
    memcpy(q, p, sizeof q); q[38] &= 0xFFFFu; /* a record from before 2.6.1: off, C2 */
    assert(st_put(ST_LIVE, 0, q, &tc_state, &e) == ST_OK && !tc_state.pad_ch && tc_state.pad_note == TC_PAD_NOTE_DEFAULT);
    assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_OK);
    /* Saved, and back after a restart. */
    run(ST_SAVE_DELAY_MS + 10u);
    tc_init(&tc_state); assert(!tc_state.pad_ch && tc_state.pad_note == TC_PAD_NOTE_DEFAULT);
    (void)st_load(&tc_state); assert(tc_state.pad_ch == 9u && tc_state.pad_note == 48u);
    /* A preset leaves them: which controller plays the pads is the board's. */
    tc_request(&tc_state, TC_REQUEST_SAVE, 2); run(2);
    assert(tc_set_item(&tc_state, &e, TC_MENU_PAD_CH, 3u) && tc_set_item(&tc_state, &e, TC_MENU_PAD_NOTE, 24u));
    tc_request(&tc_state, TC_REQUEST_LOAD, 2); run(2);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.pad_ch == 3u && tc_state.pad_note == 24u);
    /* Factory settings: off, C2. */
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_FACTORY; tc_state.group = TC_GROUP_ABOUT; tc_state.edit_value = 1u;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(tc_state.last_action == TC_FACTORY_RESET && !tc_state.pad_ch && tc_state.pad_note == TC_PAD_NOTE_DEFAULT);
    puts("settings: Pad ch and Pad note on the MIDI page (the note while the channel is on), their values and names, the "
         "record's word 38, refusals, a restart, presets leave them, factory settings");
}
/* In All Melody the left half's pads are melody pads: a note plays that pad's note, and never glides. */
static void all_melody(void) {
    fresh();
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY)); run(1000); logged = 0;
    tp_notes n = chord(PAD_C2); assert(n.count == 1u);
    on(C2, 70); run(2); assert(ons() == 1u && log_[0].a == n.notes[0] && log_[0].b == 70u && (log_[0].status & 15u) == 0u); /* the melody channel */
    off(C2); run(2); quiet();
    puts("all melody: a note plays its pad's melody note");
}
int main(void) {
    mapping();
    playing();
    taps();
    velocity();
    pressure();
    hold();
    finger();
    steps();
    letting_go();
    sessions();
    channel_off();
    settings();
    all_melody();
    printf("PASS: pads from MIDI in: the Pad channel's 48 notes press the left half's pads (an octave a row), with the "
           "note's velocity and aftertouch; taps, re-strikes and notes waiting on Note Offs play; Hold, a finger, the step "
           "screen, All Notes Off, sessions, the channel off; the two settings, their record, presets and factory settings\n");
    return 0;
}
