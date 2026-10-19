/* SPDX-License-Identifier: GPL-3.0-or-later - 0.11.0 progressions, the edges: interruptions and
 * resuming, a DAW's Start/Stop/Continue and song position, clock and tempo changes, wraps,
 * recording corner cases, muted pads, switching slots while playing, the play screen's
 * progression line (ui.c with a fake display) and the guide lights' LED traffic, the
 * settings log (writes per action, a full log, compaction with every slot used), and a
 * seeded mix of everything at once. Real code throughout; the loop mirrors usb_main. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/chord_name.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef PREFIX /* compiling against code from before these checks: names they added */
#define TC_SEQ_KEPT 250
#define TC_SAVE_FAILED 251
uint32_t sq_changes;
#endif
volatile ts_abi ts_status;
/* ---- fakes: sector 11, the memory chip, the display (text by row), the LEDs, the jacks ---- */
static uint32_t flash[FS_SIZE / 4u];
static bool flash_full;
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    if (flash_full) return false;
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
static char rows[64][40];
bool td_init(void) { return true; }
void td_clear(void) { memset(rows, 0, sizeof rows); }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y < 64u && !x) { strncpy(rows[y], text, sizeof rows[y] - 1u); }
    return x + 6u * scale * (uint32_t)strlen(text);
}
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
static tl_rgb pad_led[TL_PADS];
static uint32_t led_frames; static bool led_dirty;
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) {
    if (pad < TL_PADS && memcmp(&pad_led[pad], &c, sizeof c)) { pad_led[pad] = c; led_dirty = true; }
}
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) { if (led_dirty) { led_dirty = false; led_frames++; } }
uint32_t jh_levels(void) { return 0u; }

static uint32_t seed = 0x0B5E55EDu;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
static tp_engine e;
static ts_scan scan;
static uint32_t now;
static bool block_arm; /* the editor's transfer: no new session meanwhile (main.c) */
/* What a synth hears; transport bytes sent; when chords start. */
static uint8_t sounding[16][128];
static uint32_t ons, offs, starts_at[256], starts, transport_count[256];
static uint8_t last_transport;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == 0x0Fu) { transport_count[p[1]]++; if (p[1] != 0xF8u) last_transport = p[1]; return true; }
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++;
        if (starts < 256u && (!starts || now - starts_at[starts - 1u] > 50u)) starts_at[starts++] = now;
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++; }
    return true;
}
static uint32_t heard_classes(uint32_t ch) {
    uint32_t set = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (sounding[ch][p]) set |= 1u << (p % 12u);
    return set;
}
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += sounding[c][p];
    return n;
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) { if (!block_arm) (void)tc_arm(&e, &scan, now, true); }
            else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
    }
}
static void until_active(void) { for (uint32_t i = 0; i < 3000u && e.state != TK_ACTIVE; ++i) run(1); assert(e.state == TK_ACTIVE); }
/* Chords in C major as pitch-class sets, and the pads that play them (bottom row). */
#define C_ 0x091u   /* C E G */
#define DM 0x224u   /* D F A */
#define EM 0x890u   /* E G B */
#define F_ 0x221u   /* F A C */
#define G_ 0x884u   /* G B D */
#define AM 0x211u   /* A C E */
enum { PC = 3, PDM = 7, PEM = 11, PF = 15, PG = 19, PAM = 23 };
static void fresh(void) {
    fs_erase(); flash_full = false; tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); block_arm = false;
    memset(sounding, 0, sizeof sounding); memset(transport_count, 0, sizeof transport_count);
    ons = offs = starts = 0u; last_transport = 0u;
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    run(1000); assert(e.state == TK_ACTIVE);
}
static void build(sq_prog *p, const uint8_t *pads, uint32_t n, uint8_t unit) {
    const tc_chord none = {0, 0, 0};
    sq_clear(p, unit, tc_state.scale);
    for (uint32_t i = 0; i < n; ++i) assert(sq_tap(p, pads[i], 100, tc_state.scale, none));
}
static void prog(const uint8_t *pads, uint32_t n, uint8_t unit) { build(&sq, pads, n, unit); }
static void all_off(void) {
    tc_state.transport = false; tc_state.arp = TC_ARP_OFF; tc_state.hold = 0u; tc_state.arp_latch = 0u;
    memset(scan.values, 0, sizeof scan.values);
    const uint8_t stop[4] = {0x0F, 0xFC, 0, 0}; tm_packet(stop, now);
    block_arm = false; run(1500); until_active(); run(1500);
    assert(!heard() && ons == offs);
}
static const uint8_t cgaf[4] = {PC, PG, PAM, PF};

/* ---- 1. the editor interrupts while it plays: the progression carries on where it was ---- */
static void resume(void) {
    fresh();
    prog(cgaf, 4, SQ_UNIT_BEAT);                 /* C G Am F, a beat (500 ms) each */
    tc_state.transport = true; run(1250);
    assert(heard_classes(0) == AM && e.seq_entry == 2u);
    tp_disable(&e); block_arm = true; run(1100); /* the editor's SysEx: the engine drains */
    assert(!heard() && last_transport == 0xFCu); /* Stop went out */
    block_arm = false; until_active(); run(40);
    assert(last_transport == 0xFBu);            /* Continue, not Start */
    assert(heard_classes(0) == AM && e.seq_entry == 2u);
    uint32_t t = now;
    while (heard_classes(0) != F_ && now - t < 600u) run(1);
    assert(heard_classes(0) == F_ && now - t >= 180u && now - t <= 300u); /* the rest of Am's beat */
    /* An editor write to the playing slot: it plays the new progression from its top. */
    static const uint8_t two[2] = {PDM, PEM};
    sq_prog p; build(&p, two, 2, SQ_UNIT_BEAT);
    uint32_t w[ST_PAYLOAD]; sq_encode(&p, w);
    tp_disable(&e); block_arm = true; run(300);
    assert(st_put(ST_PROG, tc_state.prog_slot, w, &tc_state, &e) == ST_OK);
    run(800); block_arm = false; until_active(); run(40);
    assert(heard_classes(0) == DM && e.seq_entry == 0u);
    all_off();
}
/* ---- 2. a DAW drives it: Start, exact step boundaries, Stop, Continue, song position ---- */
static const uint8_t START[4] = {0x0F, 0xFA, 0, 0}, STOP[4] = {0x0F, 0xFC, 0, 0}, CONT[4] = {0x0F, 0xFB, 0, 0},
    TICK[4] = {0x0F, 0xF8, 0, 0};
static void ticks(uint32_t n) { for (uint32_t i = 0; i < n; ++i) { tm_packet(TICK, now); run(21); } }
static void spp(uint32_t sixteenths) {
    const uint8_t p[4] = {0x03, 0xF2, (uint8_t)(sixteenths & 127u), (uint8_t)(sixteenths >> 7)};
    tm_packet(p, now);
}
static void daw(void) {
    fresh();
    prog(cgaf, 4, SQ_UNIT_BEAT);
    tm_packet(START, now); ticks(1); run(5);
    assert(heard_classes(0) == C_);             /* the first clock after Start: beat 1 */
    ticks(23); assert(heard_classes(0) == C_);  /* clocks 0..23: the first beat */
    ticks(1); run(5); assert(heard_classes(0) == G_); /* clock 24 starts beat 2 */
    ticks(35); assert(heard_classes(0) == AM);  /* clock 59: halfway through beat 3 */
    tm_packet(STOP, now); run(30); assert(!heard());
    run(400);                                   /* stopped a while (no clocks) */
    tm_packet(CONT, now); ticks(1); run(5);
    assert(heard_classes(0) == AM);             /* carries on mid-beat 3 */
    ticks(12); run(5); assert(heard_classes(0) == F_); /* clock 72: beat 4 */
    tm_packet(STOP, now); run(30);
    spp(4); tm_packet(CONT, now); ticks(1); run(5);
    assert(heard_classes(0) == G_);             /* song position 4 sixteenths = beat 2 */
    spp(16 + 4); ticks(1); run(5);
    assert(heard_classes(0) == G_);             /* sent while running: bar 2, beat 2 (the progression is a bar) */
    tm_packet(STOP, now); run(30); spp(16 + 12); tm_packet(CONT, now); ticks(1); run(5);
    assert(heard_classes(0) == F_);             /* bar 2, beat 4 */
    tm_packet(START, now); ticks(1); run(5);
    assert(heard_classes(0) == C_);             /* Start while running: from the top */
    tm_packet(STOP, now); run(30); assert(!heard());
    /* Start without a clock plays nothing; a clock that stops without a Stop ends it. */
    run(400); tm_packet(START, now); run(600); assert(!heard());
    ticks(30); assert(heard());
    run(400); assert(!heard() && ons == offs);
    all_off();
}
/* ---- 2b. striking again: a one-chord progression each time round, a DAW's Start on the first
 * chord; a DAW's clock arriving mid-step keeps the place; Stop in the middle of a strum ---- */
static void again(void) {
    fresh();
    static const uint8_t one[1] = {PC};
    prog(one, 1, SQ_UNIT_BEAT);
    tc_state.transport = true; run(100);
    uint32_t before = ons; run(2000);
    assert(ons - before == 12u);                /* C struck again every beat: 4 x 3 notes */
    tc_state.transport = false; run(100);
    prog(cgaf, 4, SQ_UNIT_BAR);
    tc_state.transport = true; run(1000); assert(heard_classes(0) == C_); /* halfway through bar 1 */
    ticks(40); assert(heard_classes(0) == C_);  /* a DAW's clock now (stopped DAW): same place */
    ticks(12); run(5); assert(heard_classes(0) == G_); /* the rest of the bar on its clock */
    all_off();
    prog(cgaf, 4, SQ_UNIT_BEAT);
    tm_packet(START, now); ticks(5); assert(heard_classes(0) == C_);
    before = ons; tm_packet(START, now); ticks(1); run(5);
    assert(heard_classes(0) == C_ && ons - before == 3u); /* Start again: C struck again */
    ticks(30); assert(heard_classes(0) == G_);  /* clock 30 */
    tm_packet(STOP, now); run(30);
    ticks(40);                                  /* a DAW that keeps sending its clock while stopped */
    tm_packet(CONT, now); ticks(1); run(5);
    assert(heard_classes(0) == G_);             /* the song didn't move: still beat 2 */
    ticks(17); run(5); assert(heard_classes(0) == AM); /* clock 48 */
    tm_packet(STOP, now); run(30);
    /* The arpeggiator switched on and off in the middle of a chord. */
    prog(cgaf, 4, SQ_UNIT_BAR);
    tc_state.transport = true; run(500); assert(heard() == 3u);
    tc_state.arp = TC_ARP_UP; run(60);
    for (uint32_t i = 0; i < 300u; ++i) { run(1); assert(heard() <= 1u); }
    tc_state.arp = TC_ARP_OFF; run(60); assert(heard_classes(0) == C_ && heard() == 3u);
    tc_state.transport = false; run(100);
    /* Stop while a strum is still starting its notes: every note that started ends. */
    tc_state.strum = TC_STRUM_UP; tc_state.strum_speed = 7u; /* 120 ms apart */
    tc_state.transport = true; run(150);
    assert(heard() == 2u);
    tc_state.transport = false; run(400);
    assert(!heard() && ons == offs);
    tc_state.strum = TC_STRUM_OFF;
    all_off();
}
/* ---- 3. the board's clock: every unit, a tempo change mid-step, a DAW clock, wraps ---- */
static void board_clock(void) {
    for (uint8_t unit = 0; unit < SQ_UNIT_KINDS; ++unit) {
        fresh();
        prog(cgaf, 4, unit);
        uint32_t ms = sq_unit_ticks(unit) * 2500u / 120u;      /* 120 BPM */
        tc_state.transport = true; starts = 0u; run(ms * 4u + 100u);
        assert(starts >= 4u);
        for (uint32_t i = 1; i < 4u; ++i) {
            uint32_t gap = starts_at[i] - starts_at[i - 1u];
            assert(gap + 15u >= ms && gap <= ms + 25u);
        }
        all_off();
    }
    /* Tempo doubled halfway through a bar: the rest of the bar takes half as long. */
    fresh();
    prog(cgaf, 4, SQ_UNIT_BAR);
    tc_state.transport = true; starts = 0u; run(1000);
    tc_state.tempo = 40u; /* 240 BPM */
    uint32_t t = now;
    while (heard_classes(0) != G_ && now - t < 3000u) run(1);
    assert(now - t >= 470u && now - t <= 540u);
    all_off();
    /* The transport on while a DAW's clock runs but the DAW is stopped: steps follow its clock. */
    fresh();
    prog(cgaf, 4, SQ_UNIT_BEAT);
    tm.ticks = 0xFFFFFFF0u;                     /* and its count about to wrap */
    ticks(3); tc_state.transport = true; run(5);
    assert(heard_classes(0) == C_);
    ticks(24); run(5); assert(heard_classes(0) == G_);
    ticks(24); run(5); assert(heard_classes(0) == AM);
    all_off();
    /* The millisecond counter wraps mid-progression. */
    now = 0xFFFFF000u - 1500u;
    fresh();
    prog(cgaf, 4, SQ_UNIT_BEAT);
    tc_state.transport = true; starts = 0u; run(3000);
    assert(starts >= 5u);
    for (uint32_t i = 1; i < starts; ++i) { uint32_t gap = starts_at[i] - starts_at[i - 1u]; assert(gap >= 485u && gap <= 525u); }
    all_off();
}
/* ---- 4. recording corner cases, with the buttons ---- */
static uint16_t nav;
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint16_t v = a == 0x36u ? nav : 0u;
    struct bi_reply r = {BI_OK, 2u, gen, stamp, 300u, {(uint8_t)v, (uint8_t)(v >> 8)}};
    return r;
}
enum { CENTRE = 6, UP = 7, LEFT = 8, DOWN = 1 };
static void button(uint32_t bit) {
    nav = (uint16_t)(1u << bit);
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
    nav = 0u;
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
}
static void tap(uint32_t pad) { scan.values[pad] = 3000u; run(60); scan.values[pad] = 0u; run(60); }
static void save_slot(uint8_t slot, const uint8_t *pads, uint32_t n) {
    prog(pads, n, SQ_UNIT_BAR);
    tc_state.request = TC_REQUEST_PROG_SAVE; tc_state.request_slot = slot; st_requests(&tc_state, &e);
}
static void recording(void) {
    fresh();
    tc_state.arrows = TC_ARROWS_SEQUENCER; tc_state.prog_slot = 1u;
    (void)tc_poll(&tc_state, &e, reader, NULL, now); run(20);
    save_slot(1u, cgaf, 4);
    uint32_t records = st_records();
    /* Down twice by mistake: nothing recorded, the slot keeps its progression. */
    button(DOWN); assert(tc_state.recording && !sq.count);
    button(DOWN); assert(!tc_state.recording && sq.count == 4u && (sq_present & 2u));
    assert(tc_state.last_action == TC_SEQ_KEPT && st_records() == records);
    /* Undo everything recorded, then centre: kept too. */
    button(DOWN); tap(PC); tap(PG); button(LEFT); button(LEFT); assert(!sq.count && tc_state.recording);
    button(CENTRE); assert(!tc_state.recording && sq.count == 4u && st_records() == records);
    /* Up while recording nothing: finishes (kept) and plays the slot. */
    button(DOWN); button(UP); assert(!tc_state.recording && tc_state.transport && sq.count == 4u);
    run(300); assert(heard_classes(0) == C_);
    button(UP); run(100); assert(!heard());
    /* Recording with strum, hold, the arpeggiator and note repeat on: every tap counts once. */
    tc_state.strum = TC_STRUM_UP; tc_state.hold = 1u; tc_state.arp = TC_ARP_UP; tc_state.note_repeat = 2u;
    button(DOWN);
    tap(PC); tap(PC); tap(PF); tap(51u); tap(PG); /* a melody pad in between is not recorded */
    assert(sq.count == 3u && sq.total == 4u && sq.entries[0].units == 2u);
    /* A mode button between taps: the chord keeps the mode it was tapped in (borrowed). */
    tp_layout dorian = tc_layout(&tc_state); dorian.mode = 2u; assert(tc_set_layout(&tc_state, &e, dorian));
    tap(PF);
    assert(sq.count == 4u && sq.entries[3].scale == 2u && sq.entries[2].scale == SQ_NO_SCALE);
    button(CENTRE); assert(!tc_state.recording && (sq_present & 2u) && st_records() == records + 1u);
    tc_state.strum = TC_STRUM_OFF; tc_state.note_repeat = 0u;
    all_off();
    /* 64 taps fill it; the 65th says so. */
    button(DOWN);
    for (uint32_t i = 0; i < SQ_UNITS_MAX; ++i) tap(i / 2u % 2u ? PC : PG);
    assert(sq.total == SQ_UNITS_MAX && tc_state.last_action == TC_SEQ_TAP);
    tap(PF); assert(sq.total == SQ_UNITS_MAX && tc_state.last_action == TC_SEQ_FULL);
    button(CENTRE); assert(sq.total == SQ_UNITS_MAX);
    /* The editor writes the slot being recorded: the recording ends, the editor's version stays. */
    button(DOWN); tap(PC);
    static const uint8_t two[2] = {PDM, PEM};
    sq_prog p; build(&p, two, 2, SQ_UNIT_HALF);
    uint32_t w[ST_PAYLOAD]; sq_encode(&p, w);
    assert(st_put(ST_PROG, 1u, w, &tc_state, &e) == ST_OK);
    assert(!tc_state.recording && sq.count == 2u && sq.unit == SQ_UNIT_HALF);
    tap(PC); assert(sq.count == 2u);
    all_off();
}
/* ---- 5. a pad set silent (Sound off) is silent in the progression too ---- */
static void muted(void) {
    fresh();
    static const uint8_t two[2] = {PC, PG};
    prog(two, 2, SQ_UNIT_BEAT);
    tc_state.chord_mute = 1ull << PC;
    tc_state.transport = true; run(300);
    assert(!heard());
    tc_live_chord live; tc_live_current(tc_layout(&tc_state), &live);
    assert(live.root == 0u && live.count == 3u); /* it still picks the live notes */
    run(500); assert(heard_classes(0) == G_);
    tc_state.chord_mute = 0u;
    all_off();
}
/* ---- 6. another slot while it plays: the new progression at once, from its top ---- */
static void switching(void) {
    fresh();
    static const uint8_t c4[1] = {PC}, gam[2] = {PG, PAM};
    save_slot(3u, gam, 2);
    prog(c4, 1, SQ_UNIT_TWO_BARS);
    tc_state.transport = true; run(1000); assert(heard_classes(0) == C_);
    tc_state.prog_slot = 3u; tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 3u; st_requests(&tc_state, &e);
    run(30); assert(heard_classes(0) == G_ && e.seq_entry == 0u);
    uint32_t t = now;
    while (heard_classes(0) != AM && now - t < 3000u) run(1);
    assert(now - t >= 1950u && now - t <= 2030u); /* G for its whole bar */
    /* The unit changed while playing: the steps follow the new length. */
    sq.unit = SQ_UNIT_BEAT; sq_changes++;
    run(30); assert(heard_classes(0) == G_);
    t = now; while (heard_classes(0) != AM && now - t < 3000u) run(1);
    assert(now - t >= 450u && now - t <= 530u);
    all_off();
}
/* ---- 7. the play screen's progression line and the guide lights ---- */
static const char *row(uint32_t y) { return rows[y]; }
static bool drawable(const char *s) { for (; *s; ++s) if (!strchr(td_font_chars, *s)) return false; return true; }
static void screen(void) {
    fresh();
    tc_state.ui = TC_UI_PLAY;
    prog(cgaf, 4, SQ_UNIT_BAR);
    tc_state.transport = true; run(100);
    tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now);
    assert(!strcmp(row(56), "P1 C > G 1/4"));
    /* A step's name comes from its own root: A C E G on the Am pad is Am7 (on C it would be C6). */
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    assert(sq_tap(&sq, PAM, 100, 0u, (tc_chord){0, 0, TC_ADD7}) && sq_tap(&sq, PC, 100, 0u, (tc_chord){0, 0, 0}));
    run(50); tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now);
    assert(!strcmp(row(56), "P1 Am7 > C 1/2"));
    prog(cgaf, 4, SQ_UNIT_BAR); run(50);
    /* The longest chord names any pad can show: the line still fits 21 characters, the
     * position stays, nothing undrawable. */
    char longest[TN_MAX] = "", name[TN_MAX]; tc_chord worst = {0, 0, 0}; uint8_t worst_pad = 0, worst_scale = 0;
    for (uint8_t mode = 0; mode < TC_ALL_SCALES; ++mode) {
        if (!tc_scale_defined(mode)) continue;
        tp_layout l = {1u, TP_MELODY_BASE, TP_CHORD_BASE, mode, TP_MELODY_SCALE, 0u};
        for (uint32_t k = 3u; k < 48u; k += 4u) for (uint8_t sus = 0; sus <= 4u; sus += 2u) for (uint8_t add = 0; add < 8u; ++add)
            for (uint8_t lower = 0; lower < 8u; ++lower) {
                tc_chord ed = {lower, sus, add}; tp_notes n;
                if (!tc_pad_with(l, k, ed, &n)) continue;
                uint32_t len = tn_name(&n, (uint8_t)((1u + tc_step_semitones(mode, k / 4u)) % 12u), name);
                if (len > strlen(longest)) { strcpy(longest, name); worst = ed; worst_pad = (uint8_t)k; worst_scale = mode; }
            }
    }
    printf("longest step name: %s (%zu)\n", longest, strlen(longest));
    tc_set_layout(&tc_state, &e, (tp_layout){1u, TP_MELODY_BASE, TP_CHORD_BASE, worst_scale, TP_MELODY_SCALE, 0u});
    sq_clear(&sq, SQ_UNIT_BEAT, worst_scale);
    for (uint32_t i = 0; i < 3u; ++i) {
        assert(sq_tap(&sq, worst_pad, 100, worst_scale, worst));
        assert(sq_tap(&sq, PC, 100, worst_scale, (tc_chord){0, 0, 0}));
    }
    for (uint32_t i = 0; i < 4000u; i += 100u) {
        run(100); tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now);
        assert(strlen(row(56)) <= 21u && drawable(row(56)) && !strncmp(row(56), "P1 ", 3) && strchr(row(56), '/'));
    }
    tc_state.transport = false; run(100);
    /* Recording: REC, the slot, the units, the last chord when it fits. */
    tc_state.chord[worst_pad] = worst;
    tc_state.recording = true; sq_clear(&sq, SQ_UNIT_BEAT, worst_scale);
    tap(PC); tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now);
    assert(!strncmp(row(56), "REC P1 1/64 ", 12) && strlen(row(56)) <= 21u && drawable(row(56)));
    tap(worst_pad); tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now);
    assert(!strncmp(row(56), "REC P1 2/64", 11) && strlen(row(56)) <= 21u && drawable(row(56)));
    tc_state.recording = false; tc_state.chord[worst_pad] = (tc_chord){0, 0, 0};
    /* Guide lights: the pulse steps in visible levels, so the pad LEDs aren't resent every pass. */
    tc_set_layout(&tc_state, &e, (tp_layout){0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u});
    prog(cgaf, 4, SQ_UNIT_BAR); tc_state.transport = true; run(100);
    led_frames = 0u;
    for (uint32_t i = 0; i < 2000u; ++i) { run(1); ui_service(&tc_state, &e, &scan, true, now); }
    printf("guide lights: %u pad LED frames in 2 s\n", led_frames);
    assert(led_frames <= 60u);
    all_off();
}
/* ---- 8. the settings log: writes per action, a full log, compaction with everything ---- */
static void settings_log(void) {
    fresh();
    uint32_t r0 = st_records();
    save_slot(0u, cgaf, 4); assert(st_records() == r0 + 1u);
    sq.unit = SQ_UNIT_HALF; tc_state.request = TC_REQUEST_PROG_SAVE; tc_state.request_slot = 0u; st_requests(&tc_state, &e);
    assert(st_records() == r0 + 2u);
    /* A full log: the save fails and says so (the progression still plays until restart). */
    flash_full = true;
    save_slot(0u, cgaf, 3);
    assert(tc_state.last_action == TC_SAVE_FAILED && sq.count == 3u);
    tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 0u; st_requests(&tc_state, &e);
    assert(tc_state.last_action == TC_SAVE_FAILED);
    flash_full = false;
    /* Every slot of every kind in use, then enough changes to compact at the next start. */
    fresh();
    uint32_t want[SQ_SLOTS][ST_PAYLOAD], scales[TC_USER_SCALES][ST_PAYLOAD], presets[TC_PRESETS][ST_PAYLOAD];
    for (uint8_t s = 0; s < TC_USER_SCALES; ++s) {
        assert(tc_set_user_scale(s, (uint16_t)(0xAB5u | (1u << (s % 11u + 1u))), "U") && st_save_scale(s) == ST_OK);
        assert(st_get(ST_SCALE, s, &tc_state, scales[s]) == ST_OK);
    }
    uint8_t favs[ST_PAYLOAD * 4u]; memset(favs, 0xFF, sizeof favs); memset(favs + 128, 0, sizeof favs - 128u);
    for (uint32_t i = 0; i < 40u; ++i) favs[i] = (uint8_t)(i + 37u);
    uint32_t fw[ST_PAYLOAD]; memcpy(fw, favs, sizeof fw);
    assert(st_put(ST_FAVOURITES, 0, fw, &tc_state, &e) == ST_OK);
    for (uint8_t i = 0; i < SQ_SLOTS; ++i) {
        sq_prog p; sq_clear(&p, (uint8_t)(i % 4u), (uint8_t)(i * 5u));
        for (uint32_t k = 0; k < 30u; ++k) (void)sq_tap(&p, (uint8_t)rnd(48u), (uint8_t)rnd(128u), rnd(2u) ? p.scale : (uint8_t)rnd(37u),
                                                    (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)});
        sq_encode(&p, want[i]);
        assert(st_put(ST_PROG, i, want[i], &tc_state, &e) == ST_OK);
    }
    for (uint8_t i = 0; i < TC_PRESETS; ++i) {
        tc_state.tonic = i; tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = i; st_requests(&tc_state, &e);
        assert(st_get(ST_PRESET, i, &tc_state, presets[i]) == ST_OK);
    }
    tc_state.prog_slot = 5u; tc_state.melody_vel = TC_VEL_FIRM; tc_state.pedal_target = TC_PEDAL_CHORDS;
    uint32_t live[ST_PAYLOAD];
    for (uint32_t i = 0; i < 600u; ++i) { tc_state.changes++; st_service(&tc_state, now); st_service(&tc_state, now += 4000u); }
    assert(st_records() > 560u);
    assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    tc_controls t; tc_init(&t); (void)st_load(&t);
    assert(st_records() == 1u + TC_PRESETS + TC_USER_SCALES + 1u + SQ_SLOTS);
    uint32_t got[ST_PAYLOAD];
    for (uint8_t i = 0; i < SQ_SLOTS; ++i) assert(st_get(ST_PROG, i, &t, got) == ST_OK && !memcmp(got, want[i], sizeof got));
    for (uint8_t i = 0; i < TC_PRESETS; ++i) assert(st_get(ST_PRESET, i, &t, got) == ST_OK && !memcmp(got, presets[i], sizeof got));
    for (uint8_t i = 0; i < TC_USER_SCALES; ++i) assert(st_get(ST_SCALE, i, &t, got) == ST_OK && !memcmp(got, scales[i], sizeof got));
    assert(st_get(ST_FAVOURITES, 0, &t, got) == ST_OK && !memcmp(got, fw, sizeof got));
    assert(st_get(ST_LIVE, 0, &t, got) == ST_OK && !memcmp(got, live, sizeof got));
    assert(t.prog_slot == 5u && sq_present == 0xFFu);
    sq_prog five; assert(sq_decode(want[5], &five) && five.count == sq.count && !memcmp(five.entries, sq.entries, sizeof five.entries));
    printf("log after compaction: %u of %u records used\n", st_records(), (unsigned)(FS_SIZE / ((4u + ST_PAYLOAD) * 4u)));
    /* Whatever a start finds, the log then has room for at least 360 saves: half an hour of a
     * settings change every 5 s. */
    static const uint32_t used[4] = {30u, 371u, 372u, 600u};
    for (uint32_t i = 0; i < 4u; ++i) {
        fs_erase(); tc_init(&t); (void)st_load(&t);
        while (st_records() < used[i]) { t.changes++; st_service(&t, now); st_service(&t, now += 4000u); }
        tc_init(&t); (void)st_load(&t);
        uint32_t room = 0;
        for (;;) {
            uint32_t before = st_records();
            t.changes++; st_service(&t, now); st_service(&t, now += 4000u);
            if (st_records() == before) break;
            room++;
        }
        printf("log: a start with %u records used leaves room for %u saves\n", used[i], room);
        assert(room >= 360u);
    }
}
/* ---- 8b. rules no other test pinned down (each found by planting a bug that every test missed) ---- */
static uint32_t crc(const uint32_t *w, uint32_t n) { /* settings.c's CRC-32 over the words' bytes */
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; ++i) for (uint32_t b = 0; b < 4u; ++b) {
        c ^= (w[i] >> (8u * b)) & 0xFFu;
        for (uint32_t k = 0; k < 8u; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static void append_raw(uint8_t kind, uint8_t slot, const uint32_t payload[ST_PAYLOAD]) { /* a record st_put would refuse */
    const uint32_t words = 4u + ST_PAYLOAD;
    uint32_t s = 0;
    while (flash[s * words] != 0xFFFFFFFFu) s++;
    uint32_t *r = &flash[s * words];
    r[0] = 0x33534254u; r[1] = 1000000u + s; r[2] = (uint32_t)kind << 8 | slot;
    memcpy(&r[3], payload, ST_PAYLOAD * 4u); r[words - 1u] = crc(r, words - 1u);
}
static void combo(uint32_t hold, uint32_t press) { /* hold one navigation button, press another */
    nav = (uint16_t)(1u << hold);
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
    nav = (uint16_t)(1u << hold | 1u << press);
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
    nav = (uint16_t)(1u << hold);
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
    nav = 0u;
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
}
static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out) { /* SysEx 7-bit groups, as sysex.c reads them */
    uint32_t m = 0;
    for (uint32_t i = 0; i < n; i += 7u) {
        uint8_t top = 0; uint32_t g = n - i < 7u ? n - i : 7u;
        for (uint32_t j = 0; j < g; ++j) top = (uint8_t)(top | ((in[i + j] >> 7) << j));
        out[m++] = top;
        for (uint32_t j = 0; j < g; ++j) out[m++] = in[i + j] & 127u;
    }
    return m;
}
static void rules(void) {
    fresh();
    tc_state.arrows = TC_ARROWS_SEQUENCER;
    (void)tc_poll(&tc_state, &e, reader, NULL, now); run(20);
    /* A damaged progression record (its CRC right, its content not) never hides the good one before it. */
    save_slot(2u, cgaf, 4);
    uint32_t bad[ST_PAYLOAD]; sq_encode(&sq, bad); bad[1] |= 3u << 20; /* sus 6 */
    append_raw(ST_PROG, 2u, bad);
    tc_controls t; tc_init(&t); t.prog_slot = 2u; (void)st_load(&t);
    assert((sq_present & 4u) && sq.count == 4u);
    tc_state.prog_slot = 2u;
    /* Channels end at 16 (stored 15). */
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    uint32_t keep = p[4];
    p[4] = (keep & ~0xFFu) | 16u; assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_BAD_VALUE);
    p[4] = (keep & ~0xFF00u) | 16u << 8; assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_BAD_VALUE);
    p[4] = (keep & ~0xFFFFu) | 15u | 15u << 8; assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_OK && tc_state.chord_ch == 15u);
    tc_state.chord_ch = tc_state.melody_ch = 0u;
    /* A preset keeps the board's own settings: the progression slot, the pedal's target, the guide. */
    tc_state.prog_slot = 2u; tc_state.pedal_target = TC_PEDAL_MELODY; tc_state.seq_guide = 0u;
    tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 0u; st_requests(&tc_state, &e);
    tc_state.prog_slot = 5u; tc_state.pedal_target = TC_PEDAL_BOTH; tc_state.seq_guide = 1u;
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 0u; st_requests(&tc_state, &e);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.prog_slot == 5u && tc_state.pedal_target == TC_PEDAL_BOTH && tc_state.seq_guide == 1u);
    /* An editor write the log can't take says so. */
    uint32_t w[ST_PAYLOAD]; sq_encode(&sq, w);
    flash_full = true; assert(st_put(ST_PROG, 3u, w, &tc_state, &e) == ST_STORAGE); flash_full = false;
    /* An editor's record write shows on the board (CHANGED BY EDITOR). */
    uint8_t msg[256] = {0xF0, 0x7D, 0x54, 0x42, SX_SET_RECORD, ST_PROG, 4u}, bytes[4u * ST_PAYLOAD];
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t b = 0; b < 4u; ++b) bytes[4u * i + b] = (uint8_t)(w[i] >> (8u * b));
    uint32_t n = 7u + pack7(bytes, sizeof bytes, msg + 7u), sum = 0;
    for (uint32_t i = 4; i < n; ++i) sum += msg[i];
    msg[n++] = (uint8_t)((128u - sum % 128u) % 128u); msg[n++] = 0xF7u;
    tc_state.last_action = TC_NONE;
    assert(sx_handle(msg, n, &tc_state, &e, now) && sx_out[5] == SX_OK && tc_state.last_action == TC_REMOTE);
    /* Arrows: Sequencer. Up on an empty slot says so; left outside a recording changes nothing. */
    tc_state.prog_slot = 6u; tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 6u; st_requests(&tc_state, &e);
    button(UP); assert(tc_state.transport && tc_state.last_action == TC_SEQ_EMPTY);
    button(UP); assert(!tc_state.transport);
    tc_state.prog_slot = 2u; tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 2u; st_requests(&tc_state, &e);
    button(LEFT); assert(sq.count == 4u && sq.total == 4u);
    /* While recording, up and down (Arrows: Scale) don't change the scale. */
    button(DOWN); assert(tc_state.recording);
    tc_state.arrows = TC_ARROWS_SCALE; uint8_t scale = tc_state.scale;
    button(UP); assert(tc_state.scale == scale && tc_state.recording);
    tc_state.arrows = TC_ARROWS_SEQUENCER; button(DOWN); assert(!tc_state.recording && sq.count == 4u);
    /* Hold left + up: the previous preset; hold left + down: the next. */
    for (uint8_t i = 0; i < 3u; ++i) {
        tc_state.tonic = i; tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = i; st_requests(&tc_state, &e);
    }
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 1u; st_requests(&tc_state, &e); assert(tc_state.tonic == 1u);
    combo(LEFT, UP); assert(tc_state.preset == 0u && tc_state.tonic == 0u);
    combo(LEFT, DOWN); assert(tc_state.preset == 1u && tc_state.tonic == 1u);
    combo(LEFT, DOWN); assert(tc_state.preset == 2u && tc_state.tonic == 2u);
    all_off();
}
/* ---- 8c. the guide lights' colours ---- */
static uint32_t brightness(tl_rgb c) { return (uint32_t)c.r + c.g + c.b; }
static void guide_colours(void) {
    fresh();
    tc_state.ui = TC_UI_PLAY;
    static const uint8_t cg[2] = {PC, PG};
    prog(cg, 2, SQ_UNIT_BAR);
    tc_state.transport = true;
    uint32_t levels[64], nlevels = 0;
    for (uint32_t i = 0; i < 1500u; ++i) {
        run(1); ui_service(&tc_state, &e, &scan, true, now);
        uint32_t b = brightness(pad_led[PG]);
        bool seen = false;
        for (uint32_t j = 0; j < nlevels; ++j) seen = seen || levels[j] == b;
        if (!seen && nlevels < 64u) levels[nlevels++] = b;
    }
    tl_rgb now_pad = pad_led[PC];
    assert(now_pad.r && now_pad.r == now_pad.g && now_pad.g == now_pad.b); /* the chord now: white */
    assert(nlevels >= 6u);                                                    /* the next one pulses */
    uint32_t c_note = brightness(pad_led[48u + 3u]), d_note = brightness(pad_led[48u + 4u + 3u]); /* right half, bottom row: C, D */
    assert(c_note > 2u * d_note);                                             /* the chord's notes bright, others dim */
    tc_state.seq_guide = 0u;
    run(5); ui_service(&tc_state, &e, &scan, true, now);
    assert(!(pad_led[PC].r == pad_led[PC].g && pad_led[PC].g == pad_led[PC].b)); /* guide off: its own colour */
    tc_state.seq_guide = 1u;
    all_off();
}
/* ---- 9. a seeded mix of everything while progressions play ---- */
static void mix(void) {
    fresh();
    static const uint8_t a[4] = {PC, PAM, PF, PG}, b[3] = {PDM, PG, PC}, c[1] = {PEM};
    save_slot(0u, a, 4); save_slot(1u, b, 3); save_slot(2u, c, 1);
    tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 0u; st_requests(&tc_state, &e);
    uint32_t chords = 0u, blocked_at = 0u;
    for (uint32_t step = 0; step < 4000u; ++step) {
        if (block_arm && now - blocked_at > 1100u) block_arm = false; /* the editor's pause ends */
        switch (rnd(40u)) {
        case 0: case 1: tc_state.transport = !tc_state.transport; break;
        case 2: tc_state.arp = tc_state.arp ? TC_ARP_OFF : (uint8_t)(1u + rnd(TC_ARP_MODES - 1u)); break;
        case 3: tc_state.hold ^= 1u; break;
        case 4: tc_state.strum = (uint8_t)rnd(TC_STRUM_MODES); break;
        case 5: tc_state.voice_lead ^= 1u; break;
        case 6: { tp_layout l = tc_layout(&tc_state); l.fold ^= 1u; (void)tc_set_layout(&tc_state, &e, l); break; }
        case 7: { tp_layout l = tc_layout(&tc_state); l.melody_layout = (uint8_t)rnd(TP_MELODY_LAYOUTS); (void)tc_set_layout(&tc_state, &e, l); break; }
        case 8: tc_state.live_update ^= 1u; break;
        case 9: { tp_layout l = tc_layout(&tc_state); l.tonic = (uint8_t)rnd(12u); (void)tc_set_layout(&tc_state, &e, l); break; }
        case 10: { tp_layout l = tc_layout(&tc_state); l.mode = (uint8_t)rnd(TC_SCALES); (void)tc_set_layout(&tc_state, &e, l); break; }
        case 11: { uint8_t s = (uint8_t)rnd(4u); tc_state.prog_slot = s; tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = s; st_requests(&tc_state, &e); break; }
        case 12: if (e.state == TK_ACTIVE && !rnd(4u)) { tp_disable(&e); block_arm = true; blocked_at = now; } break;
        case 13: run(200u + rnd(2000u)); break; /* let it play a while */
        case 14: tm_packet(START, now); break;
        case 15: tm_packet(STOP, now); break;
        case 16: tm_packet(CONT, now); break;
        case 17: spp(rnd(64u)); break;
        case 18: case 19: case 20: case 21: ticks(1u + rnd(30u)); break;
        case 22: tc_state.chord_mute ^= 1ull << rnd(48u); break;
        case 23: tc_state.chord[rnd(48u)] = (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)}; break;
        case 24: sq.unit = (uint8_t)rnd(SQ_UNIT_KINDS); sq_changes++; break;
        case 25: tc_state.tempo = (uint8_t)rnd(TC_TEMPOS); break;
        case 26: case 27: case 28: { uint32_t k = rnd(96u); scan.values[k] = scan.values[k] ? 0u : 1500u + rnd(2500u); break; }
        case 29: tc_state.arp_latch ^= 1u; break;
        default: run(1u + rnd(80u)); break;
        }
        run(1u + rnd(10u));
        if (e.state != TK_ACTIVE) continue;
        if (e.seq_running && e.keys[TP_SEQ].admitted) chords++;
        assert(!(e.seq_running && sq.count && e.seq_entry >= sq.count));
    }
    printf("mix: %u passes with a progression chord admitted, %u Note On / %u Note Off so far\n", chords, ons, offs);
    assert(chords > 1500u && ons > 1500u);
    all_off();
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    now = 1000u;
    static const struct { const char *name; void (*run)(void); } tests[] = {
        {"resume", resume}, {"daw", daw}, {"again", again}, {"clock", board_clock}, {"recording", recording}, {"muted", muted},
        {"switching", switching}, {"screen", screen}, {"settings", settings_log}, {"rules", rules},
        {"guide", guide_colours}, {"mix", mix},
    };
    for (uint32_t i = 0; i < sizeof tests / sizeof tests[0]; ++i)
        if (argc < 2 || !strcmp(argv[1], tests[i].name)) { tests[i].run(); if (argc >= 2) printf("ok %s\n", tests[i].name); }
    if (argc < 2) puts("PASS: progression edges: resume after the editor's interruption (Continue, same step, the rest "
         "of its beat; an editor write plays the new progression), DAW Start on the beat, Stop, Continue "
         "mid-beat, Song Position Pointer, Start while running, no clock, clock lost; a one-chord progression and a second Start strike again, a DAW clock arriving keeps the place, Stop mid-strum; every unit's timing, a "
         "tempo change mid-step, a DAW clock with the DAW stopped, tick and millisecond wraps; recording: "
         "nothing recorded keeps the slot, undo to empty, up finishes and plays, strum/hold/arp/note repeat "
         "on, a mode change between taps, 64 units full, an editor write ends it; muted pads; another slot "
         "and another unit while playing; the progression and REC lines fit and draw with the longest "
         "chord names; guide-light LED traffic; settings writes per action, a full log says so, compaction "
         "keeps all 8 progressions, presets, scales, favourites and settings; a damaged record never hides a "
         "good one, channel limits, presets keep board settings, a failed editor save says so, editor writes "
         "show, up on an empty slot, left and up/down while not recording or recording, hold left for presets; "
         "guide-light colours; a seeded mix of everything");
    return 0;
}
