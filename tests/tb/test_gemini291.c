/* SPDX-License-Identifier: GPL-3.0-or-later - 2.9.1: the fixes of the 2026-11-23/24 Gemini audits (docs/review/FINDINGS.md),
 * each test failing on 2.9. The engine (TBR-108 to 118): Pads changed to or from All Melody under held or latched pads,
 * the step screen's previews with the arpeggiator and Hold, a note lane made a CC lane, MPE Chords with the arpeggiator
 * turned off, merged channels' pressure, a kept Stutter over Stop and Play, Mono with Note repeat, the MIDI map over Shut
 * down, a DAW's Start inside the first step. The settings (TBR-090, 098, 100): an interrupted Clear preset and
 * compaction, a restored song's pattern, a restored live record over a loaded preset's own colors and curves.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once; the writer checks every Note On has its
 * Note Off and none sounds twice. */
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/settings.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/midi_in.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef SX_RESTORE
#define SX_RESTORE 2u /* so the suite builds on 2.9 too, where each test fails */
#endif
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
static int32_t writes_left = -1; /* a write that fails after this many more (-1: never) */
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    if (writes_left == 0) return false;
    if (writes_left > 0) writes_left--;
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t kind, status, a, b; } event;
static event log_[32768];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert((token == e.issued + 1u || (e.pending_id && token == e.pending_id)) && logged < 32768u);
    uint8_t kind = p[0], ch = p[1] & 15u;
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, kind, p[1], p[2], p[3]};
    return true;
}
static bool with_cc; /* the MIDI map read every tick too */
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    if (with_cc) tc_cc_in(&tc_state, &e, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void press(uint32_t k) { scan.values[k] = 3000u; }
static void lift(uint32_t k) { scan.values[k] = 0u; }
static uint32_t heard(void) { uint32_t n = 0; for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += (uint32_t)sounding[c][p]; return n; }
static uint32_t heard_on(uint8_t ch) { uint32_t n = 0; for (uint32_t p = 0; p < 128u; ++p) n += (uint32_t)sounding[ch & 15u][p]; return n; }
static uint32_t ons_on(uint8_t ch) {
    uint32_t n = 0;
    for (uint32_t j = 0; j < logged; ++j) n += log_[j].kind == TK_ON && (log_[j].status & 15u) == ch;
    return n;
}
static void midi(uint8_t cin, uint8_t a, uint8_t b, uint8_t c) { uint8_t p[4] = {cin, a, b, c}; tm_packet(p, now); }
/* A fresh board a second into its session: chords on channel 2 (the left half), melody on 1, fixed velocities. */
static void fresh(void) {
    memset(scan.values, 0, sizeof scan.values); memset(&e, 0, sizeof e); memset(sounding, 0, sizeof sounding);
    memset(flash, 0xFF, sizeof flash); writes_left = -1; with_cc = false;
    tm_reset(); tc_init(&tc_state); (void)st_load(&tc_state);
    for (uint32_t p = 0; p < LN_PATTERNS; ++p) ln_clear(&ln_bank[p]);
    ln_current = 0u; ln_queued = LN_NO_PATTERN;
    tc_state.unstick = 0u; tc_state.glide = TC_GLIDE_OFF;
    tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    tc_state.pad_trigger = 7u; tc_state.pad_release = 3u; tc_state.pad_full = 25u;
    tc_state.chord_ch = 1u; tc_state.melody_ch = 0u;
    e.layout = tc_layout(&tc_state);
    now = 1000u; run(1000);
    logged = 0;
}
static void item(uint8_t which, uint32_t value) { assert(tc_set_item(&tc_state, &e, which, value)); }

/* ---- the engine ---- */

/* TBR-108: All Melody and Mono, two pads held (the older hidden), Pads to Left and back, both lifted: nothing sounds,
 * nothing owed. 2.9: the hidden pad owned twice, its note on for good (Panic could not end it). */
static void whole_toggled_mono(void) {
    fresh();
    item(TC_MENU_MONO, 1u); item(TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY); run(50);
    press(0); run(30); press(4); run(30);
    assert(e.keys[0].hidden && e.keys[0].silent);
    item(TC_MENU_CHORDS_SIDE, TC_PADS_LEFT); run(30);
    item(TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY); run(30);
    lift(4); run(50); lift(0); run(100);
    tc_state.panic = true; run(100);
    assert(!heard() && !tp_obligations(&e));
}
/* TBR-109: Hold on, a chord latched, Pads to All Melody: the chord ends and the pad plays its melody note. 2.9: the
 * chord sounded on, the pad dead. */
static void latched_then_whole(void) {
    fresh();
    item(TC_MENU_HOLD, 1u);
    press(3); run(30); lift(3); run(30);
    assert(e.keys[3].latched && heard_on(1u));
    item(TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY); run(50);
    assert(!heard());
    logged = 0; press(3); run(30);
    assert(ons_on(0u) == 1u && heard_on(0u) == 1u);
    lift(3); run(50);
    assert(!heard() && !tp_obligations(&e));
}
/* Step screen on lane 1, a drum lane on channel 10. */
static void drum_steps(void) {
    ln.lanes[0].kind = LN_DRUMS; ln.lanes[0].channel = 0u; ln_changes++; ln_pattern_changes[0]++;
    tc_state.drum_ch = 9u; tc_state.lane = 0u; tc_state.step_silent = 0u; tc_state.ui = TC_UI_STEPS;
}
/* TBR-110: the arpeggiator on, a drum preview on the step screen sounds on the drum channel, nothing on the chord
 * channel. 2.9: silenced, the arpeggiator played its note on the chord channel. */
static void preview_arp(void) {
    fresh();
    item(TC_MENU_ARP, TC_ARP_UP); run(20);
    drum_steps(); logged = 0;
    press(3); run(200);
    assert(heard_on(9u) == 1u && ons_on(1u) == 0u && !e.keys[3].silent);
    lift(3); run(50);
    assert(!heard());
}
/* TBR-112: Hold on, a drum preview lifted ends. 2.9: latched, it sounded on. */
static void preview_hold(void) {
    fresh();
    item(TC_MENU_HOLD, 1u);
    drum_steps(); logged = 0;
    press(3); run(30);
    assert(heard_on(9u) == 1u);
    lift(3); run(50);
    assert(!heard() && !e.keys[3].latched);
}
/* TBR-111: a lane's tied note sounding, its kind made CC: the note ends. 2.9: it sounded until Stop. */
static void lane_to_cc(void) {
    fresh();
    ln.lanes[0].kind = LN_MELODY; ln.lanes[0].length = 16u; ln.lanes[0].rate = 1u;
    ln.lanes[0].steps[0].on = true; ln.lanes[0].steps[0].value = 3u; ln.lanes[0].steps[0].tie = true;
    ln_changes++; ln_pattern_changes[0]++;
    tc_state.transport = true; run(100);
    assert(e.keys[TP_LANE0].admitted && heard());
    ln.lanes[0].kind = LN_CC; ln_changes++; ln_pattern_changes[0]++;
    run(50);
    assert(!heard() && !e.keys[TP_LANE0].admitted);
    /* bench J4: made a CC lane over the empty steps the tie holds the note across (2.9.1's first fix released it only
     * during a sounding step, so it sounded to the next round) */
    tc_state.transport = false; ln.lanes[0].kind = LN_MELODY; ln_changes++; ln_pattern_changes[0]++; run(50);
    tc_state.transport = true;
    for (int n = 0; n < 3000 && !(e.lane_step[0] >= 2u && e.lane_step[0] != LN_NONE); ++n) run(1);
    assert(e.lane_step[0] >= 2u && e.lane_step[0] != LN_NONE && e.keys[TP_LANE0].admitted && heard());
    ln.lanes[0].kind = LN_CC; ln_changes++; ln_pattern_changes[0]++;
    run(20);
    assert(!heard() && !e.keys[TP_LANE0].admitted);
    tc_state.transport = false; run(50);
}
/* TBR-113: MPE Chords, the arpeggiator on, a chord held, the arpeggiator off: the chord plays on a member channel
 * (9-15), never the zone's master channel 16. 2.9: on 16. */
static void mpe_arp_off(void) {
    fresh();
    item(TC_MENU_MPE, TC_MPE_CHORDS); item(TC_MENU_ARP, TC_ARP_UP); run(200);
    press(3); run(50);
    logged = 0;
    item(TC_MENU_ARP, TC_ARP_OFF); run(100);
    assert(ons_on(15u) == 0u && heard_on(15u) == 0u && e.keys[3].channel >= 8u && e.keys[3].channel < 15u);
    uint32_t members = 0; for (uint8_t ch = 8u; ch < 15u; ++ch) members += heard_on(ch);
    assert(members == e.keys[3].captured.count);
    lift(3); run(100);
    assert(!heard());
}
/* TBR-114: channel pressure on both halves, a melody pad pressed hard, the chord channel moved onto the melody's, the
 * pad lifted: the melody channel's pressure goes back to 0. 2.9: it stayed at its last value. */
static void merged_pressure(void) {
    fresh();
    tc_state.chord_ch = 1u; tc_state.melody_ch = 0u; tc_state.at_chord = tc_state.at_melody = TC_AT_CHANNEL;
    scan.values[51] = 4000u; run(150);
    assert(e.group_sent[1] > 0u);
    item(TC_MENU_CHORD_CH, 0u);
    logged = 0; lift(51); run(300);
    int last = -1;
    for (uint32_t j = 0; j < logged; ++j) if (log_[j].kind == TK_CHANNEL && (log_[j].status & 15u) == 0u) last = log_[j].a;
    assert(last == 0 && !e.group_sent[0] && !e.group_sent[1]);
}
/* TBR-115: a kept Stutter over Stop and Play loops the new bar's first window. 2.9: the place it was kept at. And
 * tp_stutter starts a new loop when the position goes back before it (a DAW's Song Position). */
static void stutter_kept_restart(void) {
    fresh();
    ln.lanes[0].kind = LN_MELODY; ln.lanes[0].length = 16u; ln.lanes[0].rate = 4u;
    for (uint32_t s = 0; s < 16u; ++s) { ln.lanes[0].steps[s].on = true; ln.lanes[0].steps[s].value = (uint8_t)(s % 8u); }
    ln_changes++; ln_pattern_changes[0]++;
    tc_state.transport = true; run(1200);
    tc_state.stutter_kept = 1u << 2; run(100);
    assert(e.lane_stutter && e.lane_stutter_from > 0u);
    tc_state.transport = false; run(100);
    tc_state.transport = true; run(30);
    assert(e.lane_stutter && e.lane_stutter_from < tc_stutter_units(&tc_state) && e.lane_step[0] == 0u);
    tc_state.transport = false; tc_state.stutter_kept = 0u; run(50);
    bool on = true; uint32_t from = 600u, last = 610u, looped;
    (void)tp_stutter(130u, 48u, &on, &from, &last, &looped);
    assert(from == 96u && looped == 130u);
}
/* TBR-116: Mono and Note repeat: A held, B held (A hidden), B between its repeats, C pressed and lifted, A lifted: C's
 * note does not sound on through B's gap. 2.9: kept for the line until B struck again. */
static void mono_repeat_gap(void) {
    fresh();
    item(TC_MENU_MONO, 1u); tc_state.note_repeat = 1u; tc_state.repeat_rhythm = 0u; tc_state.chance = 9u;
    press(48); run(50); press(52); run(50);
    for (int i = 0; i < 600 && !(e.keys[52].cut && !e.keys[52].admitted); ++i) run(1);
    assert(e.keys[52].cut && !e.keys[52].admitted && e.keys[52].repeating);
    press(56); run(5);
    uint8_t note = e.keys[56].captured.notes[0], ch = e.keys[56].channel;
    lift(56); run(2); lift(48); run(2);
    assert(!e.mono_hold[0].count && !sounding[ch][note]);
    lift(52); run(200);
    assert(!heard());
}
/* TBR-117: Half speed mapped to a controller and held, Shut down, the controller let go, woken: not held. 2.9: held. */
static void map_over_shut_down(void) {
    fresh(); with_cc = true;
    tc_state.control_ch = 1u;
    tc_state.cc[0] = (tc_cc_slot){TC_CC_ACTION, 20u, TC_ACT_HALF_SPEED, 0u, 0u, 0u, TM_SRC_CC, 0u};
    tc_state.changes++; run(30);
    midi(0x0Bu, 0xB0u, 20u, 127u); run(30);
    assert(tc_speed(&tc_state) == TC_SPEED_HALF);
    tc_shut_down(&tc_state); run(30);
    midi(0x0Bu, 0xB0u, 20u, 0u); run(30);
    tc_wake(&tc_state); run(30);
    assert(tc_speed(&tc_state) != TC_SPEED_HALF && !tc_state.cc_down);
    with_cc = false;
}
/* TBR-118: a DAW's clock, a lane whose first step is a quarter long; Start again inside that step: it plays again.
 * 2.9: silent until the next step. */
static void daw_start_again(void) {
    fresh();
    ln.lanes[0].kind = LN_MELODY; ln.lanes[0].length = 4u; ln.lanes[0].rate = 1u;
    ln.lanes[0].steps[0].on = true; ln.lanes[0].steps[0].value = 3u;
    ln_changes++; ln_pattern_changes[0]++;
    midi(0x0Fu, 0xFAu, 0u, 0u);
    for (int i = 0; i < 16; ++i) { midi(0x0Fu, 0xF8u, 0u, 0u); run(20); }
    uint32_t before = 0; for (uint32_t j = 0; j < logged; ++j) before += log_[j].kind == TK_ON;
    assert(before == 1u);
    logged = 0;
    midi(0x0Fu, 0xFAu, 0u, 0u);
    for (int i = 0; i < 4; ++i) { midi(0x0Fu, 0xF8u, 0u, 0u); run(20); }
    uint32_t after = 0; for (uint32_t j = 0; j < logged; ++j) after += log_[j].kind == TK_ON;
    assert(after == 1u);
    midi(0x0Fu, 0xFCu, 0u, 0u); run(100);
}

/* ---- the settings ---- */

static bool present(uint8_t kind, uint8_t slot) { uint32_t p[ST_PAYLOAD]; return st_get(kind, slot, &tc_state, p) == ST_OK; }
/* Preset 1 saved with its own colors and curves (Keep look, Keep feel) and a name. */
static void own_preset(void) {
    item(TC_MENU_KEEP_LOOK, 1u); item(TC_MENU_KEEP_FEEL, 1u);
    tc_request(&tc_state, TC_REQUEST_SAVE, 0u); st_requests(&tc_state, &e);
    memcpy(tc_preset_names[0], "SONG", 5u);
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_NAMES, 0, &tc_state, p) == ST_OK); assert(st_put(ST_NAMES, 0, p, &tc_state, &e) == ST_OK);
    assert((tc_presets_present & 1u) && present(ST_LOOK, 1u) && present(ST_FEEL, 1u));
}
/* TBR-090: a Clear preset whose second write fails leaves the preset there (its own look gone, its curves kept), never
 * an empty slot with its own records. 2.9: the preset emptied first, its curves kept with it. */
static void clear_interrupted(void) {
    fresh(); own_preset();
    writes_left = 1;
    tc_request(&tc_state, TC_REQUEST_CLEAR, 0u); st_requests(&tc_state, &e);
    writes_left = -1;
    assert(tc_state.last_action == TC_SAVE_FAILED);
    assert((tc_presets_present & 1u) || (!present(ST_LOOK, 1u) && !present(ST_FEEL, 1u)));
}
/* TBR-090: an empty slot with its own colors and curves (as 2.9's interrupted clear left it): gone after a compaction.
 * 2.9 kept them at every compaction. */
static void orphans_compacted(void) {
    fresh(); own_preset();
    uint32_t zero[ST_PAYLOAD]; memset(zero, 0, sizeof zero);
    assert(st_put(ST_PRESET, 0, zero, &tc_state, &e) == ST_OK && !(tc_presets_present & 1u) && present(ST_LOOK, 1u));
    for (uint32_t i = 0; i < FS_SIZE / 176u * 3u / 4u; ++i) { /* the log (176-byte records) past half full: the next start compacts */
        uint8_t bytes[4u * ST_PAYLOAD]; uint32_t f[ST_PAYLOAD];
        for (uint32_t b = 0; b < sizeof bytes; ++b) bytes[b] = b < 128u ? (b == 0u ? (uint8_t)(i & 1u) : 0xFFu) : 0u;
        for (uint32_t w = 0; w < ST_PAYLOAD; ++w)
            f[w] = bytes[4u * w] | (uint32_t)bytes[4u * w + 1u] << 8 | (uint32_t)bytes[4u * w + 2u] << 16 | (uint32_t)bytes[4u * w + 3u] << 24;
        assert(st_put(ST_FAVOURITES, 0, f, &tc_state, &e) == ST_OK);
    }
    tc_controls c; tc_init(&c); (void)st_load(&c); tc_state = c;
    assert(!present(ST_LOOK, 1u) && !present(ST_FEEL, 1u));
}
/* TBR-098: the lanes stopped, a restored song's pattern (Song on) is the pattern at once, and stays after a save. 2.9:
 * queued, and the old pattern saved over the record. */
static void song_restored(void) {
    fresh();
    uint32_t p[ST_PAYLOAD]; memset(p, 0, sizeof p);
    p[0] = 1u | 3u << 8 | 1u << 16; p[1] = 4u | 2u << 8; /* pattern 4 playing, Song on: 4, 2 */
    assert(st_put(ST_SONG, 0, p, &tc_state, &e) == ST_OK);
    assert(ln_current == 3u);
    run(4000); st_service(&tc_state, now); run(10); st_service(&tc_state, now);
    uint32_t q[ST_PAYLOAD]; assert(st_get(ST_SONG, 0, &tc_state, q) == ST_OK && ((q[0] >> 8) & 255u) == 3u);
    tc_controls c; tc_init(&c); (void)st_load(&c); tc_state = c;
    assert(ln_current == 3u);
}
/* TBR-100: preset 1 loaded with its own colors and curves; a restore's live record (SX_RESTORE), then the board's
 * colors and curves (slot 0): those show at once. Without the flag (the editor's button save) the preset's stay. */
static uint8_t msg[300];
static uint32_t set_record(uint8_t kind, uint8_t slot, uint8_t flags, const uint32_t p[ST_PAYLOAD]) {
    uint8_t bytes[4u * ST_PAYLOAD], packed[4u * ST_PAYLOAD + 23u];
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t b = 0; b < 4u; ++b) bytes[4u * i + b] = (uint8_t)(p[i] >> (8u * b));
    uint32_t n = sx_pack(bytes, sizeof bytes, packed), len = 0, sum = SX_SET_RECORD;
    msg[len++] = 0xF0; msg[len++] = 0x7D; msg[len++] = 0x54; msg[len++] = 0x42; msg[len++] = SX_SET_RECORD;
    uint8_t head[3] = {kind, slot, flags};
    for (uint32_t i = 0; i < 3u; ++i) { msg[len++] = head[i]; sum += head[i]; }
    for (uint32_t i = 0; i < n; ++i) { msg[len++] = packed[i]; sum += packed[i]; }
    msg[len++] = (uint8_t)((128u - sum % 128u) % 128u); msg[len++] = 0xF7;
    uint32_t r = sx_handle(msg, len, &tc_state, &e, now);
    assert(r >= 8u);
    return sx_out[5];
}
static void restore_uncovers(void) {
    fresh();
    tc_state.led_scheme = 2u; own_preset();
    tc_request(&tc_state, TC_REQUEST_LOAD, 0u); st_requests(&tc_state, &e);
    assert(tc_state.look_slot == 1u && tc_state.feel_slot == 1u);
    uint32_t live[ST_PAYLOAD], look[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK && st_get(ST_LOOK, 0, &tc_state, look) == ST_OK);
    assert(set_record(ST_LIVE, 0, 0u, live) == SX_OK && tc_state.look_slot == 1u); /* the buttons' save: kept */
    assert(set_record(ST_LIVE, 0, 4u, live) == SX_BAD_VALUE); /* only the flags known */
    assert(set_record(ST_LIVE, 0, SX_RESTORE, live) == SX_OK && !tc_state.look_slot && !tc_state.feel_slot);
    look[0] = (look[0] & ~0xFF00u) | 3u << 8; /* the board's scheme 3 */
    assert(set_record(ST_LOOK, 0, SX_RESTORE, look) == SX_OK && tc_state.led_scheme == 3u);
}

static const struct { const char *name; void (*run)(void); } tests[] = {
    {"whole_toggled_mono", whole_toggled_mono},
    {"latched_then_whole", latched_then_whole},
    {"preview_arp", preview_arp},
    {"preview_hold", preview_hold},
    {"lane_to_cc", lane_to_cc},
    {"mpe_arp_off", mpe_arp_off},
    {"merged_pressure", merged_pressure},
    {"stutter_kept_restart", stutter_kept_restart},
    {"mono_repeat_gap", mono_repeat_gap},
    {"map_over_shut_down", map_over_shut_down},
    {"daw_start_again", daw_start_again},
    {"clear_interrupted", clear_interrupted},
    {"orphans_compacted", orphans_compacted},
    {"song_restored", song_restored},
    {"restore_uncovers", restore_uncovers},
};
/* With a name, that test alone (each fails on 2.9); without, all of them. */
int main(int argc, char **argv) {
    for (uint32_t i = 0; i < sizeof tests / sizeof tests[0]; ++i) if (argc < 2 || !strcmp(argv[1], tests[i].name)) tests[i].run();
    if (argc >= 2) { printf("PASS: %s\n", argv[1]); return 0; }
    printf("PASS: 2.9.1 (the Gemini audits): Pads to and from All Melody under held and latched pads (nothing stuck), "
           "step screen previews with the arpeggiator and Hold, a note lane made a CC lane, MPE Chords with the arpeggiator "
           "off, merged channels' pressure back to 0, a kept Stutter over Stop and Play and a jump back, Mono with Note "
           "repeat, the MIDI map over Shut down, a DAW's Start inside the first step; an interrupted Clear preset and "
           "compaction, a restored song's pattern, a restored live record over a loaded preset\n");
    return 0;
}
