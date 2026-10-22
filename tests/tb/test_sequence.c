/* SPDX-License-Identifier: GPL-3.0-or-later - 0.11.0 on the real code: chord progressions (sequence.c
 * and the engine's progression source), recording with the buttons, saving them (settings.c
 * on a fake sector 11), velocity per half and the pedal's target. The loop mirrors usb_main. */
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
static uint32_t seed = 0x51ED270Bu;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
/* What a synth hears, and when chords start. */
static uint8_t sounding[16][128], velocity_of[128];
static uint32_t ons, offs, cc[16][128], starts_at[64], starts;
static int32_t cc_value[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++; velocity_of[p[2]] = p[3];
        if (starts < 64u && (!starts || now - starts_at[starts - 1u] > 50u)) starts_at[starts++] = now;
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++; }
    if (p[0] == TK_CC) { cc[ch][p[2]]++; cc_value[ch][p[2]] = p[3]; }
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
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
    }
}
/* A velocity after a trip through the record (half resolution: even exact, odd 1 less, ends 1 and 127). */
static uint8_t stored(uint8_t v) { return (uint8_t)(v < 2u ? 1u : v >= 126u ? 127u : v & ~1u); }
static uint32_t classes_of(uint32_t root, const uint8_t *semis, uint32_t n) {
    uint32_t set = 0;
    for (uint32_t i = 0; i < n; ++i) set |= 1u << ((root + semis[i]) % 12u);
    return set;
}
static void fresh(void) {
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan);
    memset(sounding, 0, sizeof sounding); memset(cc, 0, sizeof cc); ons = offs = starts = 0u;
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    run(1000); assert(e.state == TK_ACTIVE);
}
/* ---- recording rules and the record format ---- */
static void rules(void) {
    sq_prog p; const tc_chord none = {0, 0, 0}, seventh = {0, 0, TC_ADD7};
    sq_clear(&p, SQ_UNIT_BAR, 1u);
    assert(sq_tap(&p, 3, 90, 1, none) && sq_tap(&p, 3, 60, 1, none));       /* Am twice: one step of 2 */
    assert(sq_tap(&p, 23, 80, 1, none) && sq_tap(&p, 15, 80, 2, none));     /* F, then D borrowed from Dorian */
    assert(sq_tap(&p, 19, 70, 7, seventh));                                  /* E7 borrowed from harmonic minor */
    assert(p.count == 4u && p.total == 5u && p.entries[0].units == 2u && p.entries[0].velocity == 90u);
    assert(p.entries[1].scale == SQ_NO_SCALE && p.entries[2].scale == 2u && p.entries[3].scale == 7u);
    assert(sq_start(&p, 0) == 0u && sq_start(&p, 2) == 3u && sq_start(&p, 4) == 5u);
    sq_undo(&p); assert(p.count == 3u && p.total == 4u);
    sq_undo(&p); sq_undo(&p); sq_undo(&p); assert(p.count == 1u && p.entries[0].units == 1u);
    sq_undo(&p); sq_undo(&p); assert(!p.count && !p.total);
    for (uint32_t i = 0; i < 40u; ++i) assert(sq_tap(&p, 3, 100, 1, none));  /* 32 per step, then a new step */
    assert(p.count == 2u && p.entries[0].units == SQ_RUN_MAX && p.entries[1].units == 8u);
    sq_clear(&p, SQ_UNIT_BAR, 0u);
    for (uint32_t i = 0; i < SQ_ENTRIES; ++i) assert(sq_tap(&p, (uint8_t)(i % 2u ? 3u : 7u), 100, 0, none));
    assert(!sq_tap(&p, 11, 100, 0, none) && p.count == SQ_ENTRIES);          /* 38 steps: full */
    sq_clear(&p, SQ_UNIT_BAR, 0u);
    for (uint32_t i = 0; i < SQ_UNITS_MAX; ++i) assert(sq_tap(&p, (uint8_t)(i / 2u % 2u ? 3u : 7u), 100, 0, none));
    assert(!sq_tap(&p, 7, 100, 0, none) && p.total == SQ_UNITS_MAX);          /* 64 units: full */
    /* Records: random progressions round-trip; malformed ones are refused, the target untouched. */
    for (uint32_t r = 0; r < 2000u; ++r) {
        sq_clear(&p, (uint8_t)rnd(SQ_UNIT_KINDS), (uint8_t)rnd(TC_ALL_SCALES));
        for (uint32_t i = 0, n = rnd(80u); i < n; ++i) {
            tc_chord ed = {(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
            (void)sq_tap(&p, (uint8_t)rnd(48u), (uint8_t)(1u + rnd(127u)), rnd(3u) ? p.scale : (uint8_t)rnd(TC_ALL_SCALES), ed);
        }
        uint32_t w[SQ_WORDS]; sq_prog back;
        sq_encode(&p, w); assert(sq_decode(w, &back));
        assert(back.count == p.count && back.total == p.total && back.unit == p.unit && back.scale == p.scale);
        for (uint32_t i = 0; i < p.count; ++i) {
            const sq_entry *a = &p.entries[i], *b = &back.entries[i];
            assert(a->pad == b->pad && a->units == b->units && a->scale == b->scale && !memcmp(&a->edit, &b->edit, sizeof a->edit));
            assert(b->velocity == stored(a->velocity));
        }
        uint32_t q[SQ_WORDS]; memcpy(q, w, sizeof q);
        switch (rnd(5u)) {
        case 0: q[0] = (q[0] & ~0xFFu) | 2u; break;                                   /* another format */
        case 1: q[0] = (q[0] & 0x00FFFFFFu) | (uint32_t)(SQ_ENTRIES + 1u) << 24; break; /* too many steps */
        case 2: q[SQ_WORDS - 1u] = TC_CHORD_SETS + 1u; break;                            /* no such chord set (1.0.0) */
        case 3: if (!p.count) continue; q[1] = (q[1] & ~63u) | 50u; break;              /* no such pad */
        default: if (!p.count) continue; q[1] |= 3u << 20; break;                       /* sus 6 */
        }
        sq_prog keep = back;
        assert(!sq_decode(q, &back) && !memcmp(&keep, &back, sizeof back));
    }
}
/* ---- playback ---- */
static const uint8_t minor_triad[3] = {0, 3, 7}, major_triad[3] = {0, 4, 7}, dom7[4] = {0, 4, 7, 10};
static void am_f_c_g(void) {
    fresh();
    tc_set_layout(&tc_state, &e, (tp_layout){9u, TP_MELODY_BASE, TP_CHORD_BASE, 1u, TP_MELODY_SCALE, 0u, 0u}); /* A minor */
    sq_clear(&sq, SQ_UNIT_BAR, 1u);
    const tc_chord none = {0, 0, 0};
    static const uint8_t pads[4] = {3, 23, 11, 27}; /* i, VI, III, VII: Am F C G */
    for (uint32_t i = 0; i < 4u; ++i) (void)sq_tap(&sq, pads[i], 100, 1, none);
    tc_state.transport = true; starts = 0u;
    uint32_t t0 = now;
    static const uint32_t roots[4] = {9u, 5u, 0u, 7u};
    static const bool minor[4] = {true, false, false, false};
    for (uint32_t i = 0; i < 8u; ++i) { /* two times round: each chord a bar (2 s at 120 BPM) */
        uint32_t before = ons;
        run(1000);
        assert(heard_classes(0) == classes_of(roots[i % 4u], minor[i % 4u] ? minor_triad : major_triad, 3));
        assert(ons - before == 3u); /* every note struck, also those the chord before shared (Am -> F: A, C) */
        run(1000);
    }
    assert(starts >= 8u);
    for (uint32_t i = 1; i < 8u; ++i) {
        uint32_t gap = starts_at[i] - starts_at[i - 1u];
        assert(gap >= 1990u && gap <= 2025u); /* a bar of 96 clocks of 20.83 ms */
    }
    assert(starts_at[0] - t0 < 30u);
    /* Live transposition: the root buttons move the next chords; the mode buttons re-colour. */
    tc_set_layout(&tc_state, &e, (tp_layout){2u, TP_MELODY_BASE, TP_CHORD_BASE, 1u, TP_MELODY_SCALE, 0u, 0u}); /* D minor */
    run(2100); assert(heard_classes(0) == classes_of(10u, major_triad, 3)); /* the 2nd chord, VI of D minor: Bb */
    run(2000); assert(heard_classes(0) == classes_of(5u, major_triad, 3));  /* III: F */
    /* Stop: every note ends. */
    tc_state.transport = false; run(200); assert(!heard() && ons == offs);
}
/* The user's song: Am D Dm Am C G E7 Am, recorded with the mode buttons for the borrowed chords. */
static void borrowed(void) {
    fresh();
    tc_set_layout(&tc_state, &e, (tp_layout){9u, TP_MELODY_BASE, TP_CHORD_BASE, 1u, TP_MELODY_SCALE, 0u, 0u}); /* A minor */
    sq_clear(&sq, SQ_UNIT_BEAT, 1u);
    const tc_chord none = {0, 0, 0}, seventh = {0, 0, TC_ADD7};
    (void)sq_tap(&sq, 3, 100, 1, none);    /* Am */
    (void)sq_tap(&sq, 15, 100, 2, none);   /* D: column 4 of A Dorian */
    (void)sq_tap(&sq, 15, 100, 1, none);   /* Dm */
    (void)sq_tap(&sq, 3, 100, 1, none);    /* Am */
    (void)sq_tap(&sq, 11, 100, 1, none);   /* C */
    (void)sq_tap(&sq, 27, 100, 1, none);   /* G */
    (void)sq_tap(&sq, 19, 100, 7, seventh); /* E7: column 5 of A harmonic minor */
    (void)sq_tap(&sq, 3, 100, 1, none);    /* Am */
    assert(sq.count == 8u);
    tc_state.transport = true;
    static const uint32_t roots[8] = {9u, 2u, 2u, 9u, 0u, 7u, 4u, 9u};
    static const uint8_t *shapes[8] = {minor_triad, major_triad, minor_triad, minor_triad, major_triad, major_triad, dom7, minor_triad};
    for (uint32_t i = 0; i < 8u; ++i) {
        run(250); assert(heard_classes(0) == classes_of(roots[i], shapes[i], i == 6u ? 4u : 3u));
        run(250);
    }
    /* Major instead of minor: the recorded chords follow, the borrowed D and E7 keep theirs. */
    tc_set_layout(&tc_state, &e, (tp_layout){9u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u}); /* A major */
    tc_state.transport = false; run(100); tc_state.transport = true;
    run(250); assert(heard_classes(0) == classes_of(9u, major_triad, 3));   /* A */
    run(500); assert(heard_classes(0) == classes_of(2u, major_triad, 3));   /* D (borrowed, unchanged) */
    run(500); assert(heard_classes(0) == classes_of(2u, major_triad, 3));   /* IV of A major: D */
    tc_state.transport = false; run(100); assert(!heard());
}
/* With the arpeggiator on, the progression feeds it: single notes of the current chord. */
static void arpeggiated(void) {
    fresh();
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    const tc_chord none = {0, 0, 0};
    (void)sq_tap(&sq, 3, 100, 0, none); (void)sq_tap(&sq, 19, 100, 0, none); /* C, G */
    tc_state.arp = TC_ARP_UP; tc_state.transport = true;
    uint32_t c_seen = 0, g_seen = 0;
    for (uint32_t i = 0; i < 1900u; ++i) {
        run(1); assert(heard() <= 1u);
        c_seen |= heard_classes(0);
    }
    run(200);
    for (uint32_t i = 0; i < 1800u; ++i) { run(1); g_seen |= heard_classes(0); }
    assert(c_seen == classes_of(0u, major_triad, 3) && g_seen == classes_of(7u, major_triad, 3));
    tc_state.transport = false; tc_state.arp = TC_ARP_OFF; run(200); assert(!heard() && ons == offs);
}
/* A DAW's Start (and clock) plays it from the top; its Stop stops it. */
static void daw(void) {
    fresh();
    sq_clear(&sq, SQ_UNIT_BEAT, 0u);
    const tc_chord none = {0, 0, 0};
    (void)sq_tap(&sq, 3, 100, 0, none); (void)sq_tap(&sq, 19, 100, 0, none); /* C, G: a beat each */
    const uint8_t start[4] = {0x0F, 0xFA, 0, 0}, stop[4] = {0x0F, 0xFC, 0, 0}, tick[4] = {0x0F, 0xF8, 0, 0};
    tm_packet(start, now);
    for (uint32_t i = 0; i < 24u * 3u; ++i) { tm_packet(tick, now); run(i % 24u == 0u ? 1u : 21u); }
    assert(heard_classes(0) == classes_of(0u, major_triad, 3) || heard_classes(0) == classes_of(7u, major_triad, 3));
    tm_packet(start, now); run(5); assert(e.seq_entry == 0u);              /* Start: from the top */
    tm_packet(stop, now); run(30); assert(!heard());                        /* Stop */
}
/* ---- recording with the buttons (Settings > Play > Arrows: Sequencer) ---- */
static uint16_t nav;
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint16_t v = a == 0x36u ? nav : 0u;
    struct bi_reply r = {BI_OK, 2u, gen, stamp, 300u, {(uint8_t)v, (uint8_t)(v >> 8)}};
    return r;
}
static void button(uint32_t bit) { /* press and release a navigation button */
    nav = (uint16_t)(1u << bit);
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
    nav = 0u;
    for (uint32_t i = 0; i < 4u; ++i) { (void)tc_poll(&tc_state, &e, reader, NULL, now); st_requests(&tc_state, &e); run(10); }
}
static void tap(uint32_t pad) { scan.values[pad] = 3000u; run(60); scan.values[pad] = 0u; run(60); }
static void recording(void) {
    fresh();
    tc_state.arrows = TC_ARROWS_SEQUENCER; tc_state.prog_slot = 2u;
    (void)tc_poll(&tc_state, &e, reader, NULL, now); run(20);
    button(1);                        /* down: record */
    assert(tc_state.recording && !tc_state.transport && !sq.count);
    tap(3); tap(3); tap(23); tap(11); /* C major: I twice, vi, iii */
    assert(sq.count == 3u && sq.total == 4u && sq.entries[0].units == 2u);
    button(8);                        /* left: take back the last unit */
    assert(sq.total == 3u && tc_state.recording);
    tap(27);
    button(6);                        /* centre: done, saved to slot 3 */
    assert(!tc_state.recording && sq.count == 3u && (sq_present & (1u << 2)));
    button(7);                        /* up: play */
    assert(tc_state.transport); run(500); assert(heard());
    button(7); run(100); assert(!tc_state.transport && !heard());
    /* After a restart the slot's progression is back. */
    sq_prog kept = sq;
    for (uint32_t i = 0; i < kept.count; ++i) kept.entries[i].velocity = stored(kept.entries[i].velocity);
    tc_controls t; tc_init(&t); sq_clear(&sq, SQ_UNIT_BAR, 0u);
    t.prog_slot = 2u; (void)st_load(&t);
    assert(sq.count == kept.count && sq.total == kept.total && !memcmp(sq.entries, kept.entries, sizeof kept.entries));
    /* Settings > Sequence: another slot is empty; clearing empties this one. */
    tc_state = t; tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 5u; st_requests(&tc_state, &e);
    assert(!sq.count);
    tc_state.request = TC_REQUEST_PROG_LOAD; tc_state.request_slot = 2u; st_requests(&tc_state, &e); assert(sq.count == 3u);
    sq_clear(&sq, SQ_UNIT_BAR, 0u); tc_state.request = TC_REQUEST_PROG_SAVE; tc_state.request_slot = 2u;
    st_requests(&tc_state, &e); assert(!(sq_present & (1u << 2)));
}
/* ---- the editor's records (SysEx GET/SET_RECORD), and the log's compaction ---- */
static void records(void) {
    fresh();
    sq_prog p; sq_clear(&p, SQ_UNIT_HALF, 1u);
    const tc_chord none = {0, 0, 0};
    for (uint32_t i = 0; i < 6u; ++i) (void)sq_tap(&p, (uint8_t)(4u * i + 3u), 100, 1, none);
    uint32_t w[ST_PAYLOAD]; sq_encode(&p, w);
    tc_state.prog_slot = 4u;
    assert(st_put(ST_PROG, 4, w, &tc_state, &e) == ST_OK && sq.count == 6u); /* the selected slot plays it at once */
    assert(st_put(ST_PROG, 6, w, &tc_state, &e) == ST_OK);
    uint32_t back[ST_PAYLOAD];
    assert(st_get(ST_PROG, 6, &tc_state, back) == ST_OK && !memcmp(back, w, sizeof w));
    assert(st_get(ST_PROG, 7, &tc_state, back) == ST_ABSENT && st_get(ST_PROG, 8, &tc_state, back) == ST_BAD_VALUE);
    w[1] |= 63u; assert(st_put(ST_PROG, 6, w, &tc_state, &e) == ST_BAD_VALUE);
    /* Fill the log until it compacts at boot: the progressions survive. */
    uint32_t live[ST_PAYLOAD];
    for (uint32_t i = 0; i < 600u; ++i) { tc_state.changes++; st_service(&tc_state, now); st_service(&tc_state, now += 4000u); }
    (void)st_get(ST_LIVE, 0, &tc_state, live);
    assert(st_records() > 600u); /* past half: the next start compacts */
    tc_controls t; tc_init(&t); t.prog_slot = 4u; (void)st_load(&t);
    assert(st_records() < 20u && sq.count == 6u && (sq_present & (1u << 6)) && (sq_present & (1u << 4)));
}
/* ---- velocity per half, the pedal's target, and their settings ---- */
static void halves(void) {
    fresh();
    tc_state.vel_mode = TC_VEL_FIXED; tc_state.melody_vel = TC_VEL_DYNAMIC;
    tp_notes c, m; assert(tc_pad(e.layout, 3u, &c) && tc_pad(e.layout, 51u, &m));
    scan.values[3] = 2000u; scan.values[51] = 2000u; run(100);
    assert(velocity_of[c.notes[0]] == 100u && velocity_of[m.notes[0]] < 100u);
    scan.values[3] = scan.values[51] = 0u; run(100);
    /* The pedal's target: chords on channel 1, melody on channel 2. */
    tc_state.chord_ch = 0u; tc_state.melody_ch = 1u; tc_state.pedal_mode = TC_PEDAL_SUSTAIN;
    static const uint8_t targets[3] = {TC_PEDAL_BOTH, TC_PEDAL_CHORDS, TC_PEDAL_MELODY};
    for (uint32_t i = 0; i < 3u; ++i) {
        memset(cc, 0, sizeof cc);
        tc_state.pedal_target = targets[i]; tc_state.sustain = true; run(100);
        assert((cc[0][64] != 0u) == (i != 2u) && (cc[1][64] != 0u) == (i != 1u));
        tc_state.sustain = false; run(100);
        assert(cc_value[0][64] == 0 && cc_value[1][64] == 0);
    }
    /* One channel for both: the pedal goes to that channel whatever the target. */
    tc_state.melody_ch = 0u; memset(cc, 0, sizeof cc);
    tc_state.pedal_target = TC_PEDAL_MELODY; tc_state.sustain = true; run(100); assert(cc[0][64] == 1u);
    tc_state.sustain = false; run(100);
    /* Settings: written as 0 while at the defaults; round trips; old records keep the chords' curve. */
    uint32_t p[ST_PAYLOAD];
    tc_init(&tc_state); assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && !p[33] && !(p[31] >> 24) && !(p[28] >> 16));
    tc_state.melody_vel = TC_VEL_LIGHT; tc_state.pedal_target = TC_PEDAL_MELODY; tc_state.prog_slot = 5u; tc_state.seq_guide = 0u;
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    tc_controls t; tc_init(&t);
    assert(st_put(ST_LIVE, 0, p, &t, &e) == ST_OK && t.melody_vel == TC_VEL_LIGHT && t.pedal_target == TC_PEDAL_MELODY &&
           t.prog_slot == 5u && t.seq_guide == 0u);
    p[31] &= 0x00FFFFFFu; p[1] = (p[1] & ~0xFF00u) | (uint32_t)TC_VEL_FIRM << 8; /* as 0.10.0 wrote it */
    assert(st_put(ST_LIVE, 0, p, &t, &e) == ST_OK && t.vel_mode == TC_VEL_FIRM && t.melody_vel == TC_VEL_FIRM);
    p[33] = 8u; assert(st_put(ST_LIVE, 0, p, &t, &e) == ST_BAD_VALUE);   /* no such slot */
    p[33] = 0u; p[28] |= 3u << 16; assert(st_put(ST_LIVE, 0, p, &t, &e) == ST_BAD_VALUE); /* no such target */
}
/* Recorded velocities play while Chord vel is not Fixed; Fixed plays every chord at 100. */
static void recorded_velocity(void) {
    fresh();
    assert(tc_state.vel_mode == TC_VEL_DYNAMIC);
    tc_set_layout(&tc_state, &e, (tp_layout){9u, TP_MELODY_BASE, TP_CHORD_BASE, 1u, TP_MELODY_SCALE, 0u, 0u}); /* A minor */
    sq_clear(&sq, SQ_UNIT_BAR, 1u);
    const tc_chord none = {0, 0, 0};
    (void)sq_tap(&sq, 3u, 40u, 1u, none); (void)sq_tap(&sq, 23u, 100u, 1u, none); /* Am soft, F as typed */
    uint32_t w[SQ_WORDS]; sq_encode(&sq, w); assert(sq_decode(w, &sq)); /* as stored: 100 stays 100 */
    assert(sq.entries[0].velocity == 40u && sq.entries[1].velocity == 100u);
    tp_notes am, f; assert(tc_pad(e.layout, 3u, &am) && tc_pad(e.layout, 23u, &f));
    tc_state.transport = true;
    run(1000); assert(velocity_of[am.notes[0]] == 40u);
    run(2000); assert(velocity_of[f.notes[0]] == 100u);
    tc_state.vel_mode = TC_VEL_FIXED;
    run(2000); assert(velocity_of[am.notes[0]] == 100u); /* round again: Fixed wins */
    tc_state.vel_mode = TC_VEL_LIGHT;
    run(4000); assert(velocity_of[am.notes[0]] == 40u);  /* a curve does not: recorded again */
    tc_state.transport = false; run(200); assert(!heard() && ons == offs);
}
int main(void) {
    rules(); am_f_c_g(); borrowed(); arpeggiated(); daw(); recording(); records(); halves(); recorded_velocity();
    puts("PASS: progressions: recording (same chord lengthens, 32 per step, 38 steps, 64 units, undo, borrowed "
         "scales), 2000 random records round-trip, malformed refused; Am F C G a bar each on the clock, live "
         "transposition, stop ends every note; the Am D Dm Am C G E7 Am song with borrowed D and E7, re-coloured "
         "to major; feeding the arpeggiator; DAW Start/Stop; recording with Arrows: Sequencer (down, taps, left "
         "undo, centre saves, up plays), slots load/clear/survive a restart; editor records and log compaction; "
         "velocity per half; recorded velocities unless Chord vel is Fixed; pedal target both/chords/melody; their settings words");
    return 0;
}
