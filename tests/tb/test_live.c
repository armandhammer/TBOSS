/* SPDX-License-Identifier: GPL-3.0-or-later - Live notes (0.8.0) on the real engine and layout:
 * the right half's scale columns and live-chord columns, the live chord from chord pads (with
 * their edits) and from MIDI in, silent chord pads, the live area's own channel, re-strike of
 * held live pads, every pad in range in every key, scale and octave.
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
static event log_[4096];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 4096u);
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
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
static uint8_t pad(uint32_t side, uint32_t column, uint32_t row) {
    uint8_t k; assert(tp_index((tp_position){(uint8_t)side, (uint8_t)row, (uint8_t)column}, &k)); return k;
}
static void down(uint32_t k) { scan.values[k] = 3000u; run(30); }
static void up(uint32_t k) { scan.values[k] = 0u; run(30); }
static void strike(uint32_t k) { down(k); up(k); }
/* Notes started since the log was cleared, in order, on channel ch. */
static uint32_t ons(uint8_t ch, uint8_t *notes, uint32_t max) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < logged; ++i)
        if (log_[i].status == (0x90u | ch) && n < max) notes[n++] = log_[i].a;
    return n;
}
static uint8_t live_note(uint32_t column, uint32_t row) {
    tp_notes n; assert(tc_pad(tc_layout(&tc_state), pad(1, column, row), &n) && n.count == 1u); return n.notes[0];
}
static void midi(uint8_t status, uint8_t a, uint8_t b) {
    const uint8_t p[4] = {(uint8_t)(status >> 4), status, a, b}; tm_packet(p, now);
}
/* --- the layout, pure ----------------------------------------------------------------------- */
static void layout(void) {
    tc_controls c; tc_init(&c); c.melody_layout = TP_MELODY_LIVE;
    tp_layout l = tc_layout(&c);
    assert(tc_layout_ok(l) && !tp_live_pad(l, pad(1, 6, 3)) && tp_live_pad(l, pad(1, 7, 0)) &&
           !tp_live_pad(l, pad(0, 9, 0)));
    l.melody_layout = TP_MELODY_SCALE; assert(!tp_live_pad(l, pad(1, 9, 0)));
    l.melody_layout = TP_MELODY_LIVE;
    /* C major from C3: columns 0..6 the scale, rows an octave apart; 7..11 the first chord. */
    static const uint8_t scale[7] = {48, 50, 52, 53, 55, 57, 59}, first[5] = {48, 52, 55, 60, 64};
    tc_chord none = {0, 0, 0};
    for (uint32_t row = 0; row < 4u; ++row)
        for (uint32_t x = 0; x < 12u; ++x) {
            tp_notes n; assert(tc_notes(l, pad(1, x, row), none, &n) && n.count == 1u);
            assert(n.notes[0] == (x < 7u ? scale[x] : first[x - 7u]) + 12u * (3u - row));
        }
    /* Chord tones: G from pad column 4, Cmaj7 with the 7th added, Dm sus4, any pad row. */
    tc_live_chord ch;
    tc_live_of_pad(l, pad(0, 4, 1), none, &ch);
    assert(ch.root == 7u && ch.count == 3u && ch.tones[0] == 0u && ch.tones[1] == 4u && ch.tones[2] == 7u);
    tc_live_of_pad(l, pad(0, 0, 3), (tc_chord){0, 0, TC_ADD7}, &ch);
    assert(ch.root == 0u && ch.count == 4u && ch.tones[3] == 11u);
    tc_live_of_pad(l, pad(0, 1, 0), (tc_chord){1u, 4u, 0}, &ch); /* root dropped: same pitch classes */
    assert(ch.root == 2u && ch.count == 3u && ch.tones[1] == 5u && ch.tones[2] == 7u);
    /* G's row from G3: G B D G B; the next row up an octave. */
    tc_live_of_pad(l, pad(0, 4, 3), none, &ch);
    static const uint8_t g[5] = {55, 59, 62, 67, 71};
    for (uint32_t x = 0; x < 5u; ++x) {
        assert(tc_live_note(l, pad(1, 7u + x, 3), &ch) == g[x]);
        assert(tc_live_note(l, pad(1, 7u + x, 2), &ch) == g[x] + 12u);
    }
    /* Held MIDI notes: the lowest is the root; one pitch class is not a chord. */
    uint8_t held[128] = {0};
    held[62] = 1u; assert(!tc_live_of_notes(held, &ch));
    held[74] = 2u; assert(!tc_live_of_notes(held, &ch)); /* D twice */
    held[65] = 1u; held[57] = 1u; /* A3 D4 F4 D5: Dm over A */
    assert(tc_live_of_notes(held, &ch) && ch.root == 9u && ch.count == 3u && ch.tones[1] == 5u && ch.tones[2] == 8u);
    /* Every pad in range, every key, built-in scale, octave and chord. */
    for (uint8_t scale_id = 0; scale_id < TC_ALL_SCALES; ++scale_id) {
        if (!tc_scale_defined(scale_id)) continue;
        for (uint8_t t = 0; t < 12u; ++t)
            for (uint8_t base = TP_MELODY_BASE - TP_OCTAVE_RANGE; base <= TP_MELODY_BASE + TP_OCTAVE_RANGE; base += 12u) {
                tp_layout m = {t, base, TP_CHORD_BASE, scale_id, TP_MELODY_LIVE, 0u};
                for (uint32_t chord = 0; chord < 48u; chord += 4u) {
                    tc_live_of_pad(m, chord, (tc_chord){0, 0, TC_ADD6 | TC_ADD7 | TC_ADD9}, &ch);
                    assert(ch.count >= 1u && ch.count <= TC_LIVE_TONES && ch.tones[0] == 0u);
                    for (uint32_t i = 1; i < ch.count; ++i) assert(ch.tones[i] > ch.tones[i - 1u] && ch.tones[i] < 12u);
                    for (uint32_t k = 48u; k < 96u; ++k) if (tp_live_pad(m, k)) {
                        uint8_t note = tc_live_note(m, k, &ch);
                        assert(note <= 127u);
                        uint32_t tone = (note % 12u + 12u - ch.root) % 12u; bool found = false;
                        for (uint32_t i = 0; i < ch.count; ++i) found |= ch.tones[i] == tone;
                        assert(found); /* a chord tone */
                    }
                }
            }
    }
}
/* --- on the engine -------------------------------------------------------------------------- */
static void engine(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = TC_VEL_FIXED;
    tc_state.melody_layout = TP_MELODY_LIVE; tc_state.melody_ch = 1u;
    e.layout = tc_layout(&tc_state);
    run(600); /* armed */
    uint8_t got[16];
    /* Before any chord: the key's first chord. */
    logged = 0; strike(pad(1, 7, 3)); assert(ons(1, got, 16) == 1u && got[0] == 48u); quiet();
    /* G (column 4), lifted: the live area keeps G. */
    strike(pad(0, 4, 3)); logged = 0;
    strike(pad(1, 8, 3)); assert(ons(1, got, 16) == 1u && got[0] == 59u); quiet();
    strike(pad(1, 2, 3)); /* scale pads are unchanged */
    logged = 0; strike(pad(1, 2, 2)); assert(ons(1, got, 16) == 1u && got[0] == 64u); quiet();
    /* Silent chord pad: no sound at all, the live area follows it (Am). */
    tc_state.chord_mute = (uint64_t)1u << pad(0, 5, 3);
    logged = 0; strike(pad(0, 5, 3)); assert(logged == 0u);
    strike(pad(1, 7, 3)); assert(ons(1, got, 16) == 1u && got[0] == 57u); quiet();
    tc_state.chord_mute = 0u;
    logged = 0; strike(pad(0, 5, 3)); assert(ons(0, got, 16) == 3u); quiet(); /* it sounds again */
    /* The live area on its own channel; the scale pads stay on the melody channel. */
    tc_state.live_ch = 5u;
    logged = 0; strike(pad(1, 9, 3)); strike(pad(1, 0, 3));
    assert(ons(4, got, 16) == 1u && got[0] == 64u && ons(1, got, 16) == 1u && got[0] == 48u); quiet();
    tc_state.live_ch = 0u;
    /* Re-strike off: a held live pad keeps its note through a chord change. */
    down(pad(1, 7, 3)); logged = 0;
    strike(pad(0, 4, 3)); assert(ons(1, got, 16) == 0u && sounding[1][57]);
    up(pad(1, 7, 3)); quiet();
    /* Re-strike on: it moves to the new chord's note at once (Am -> F). */
    tc_state.live_update = 1u;
    strike(pad(0, 5, 3)); down(pad(1, 7, 3)); logged = 0;
    strike(pad(0, 3, 3)); assert(ons(1, got, 16) == 1u && got[0] == 53u && !sounding[1][57] && sounding[1][53]);
    logged = 0; strike(pad(0, 3, 3)); assert(ons(1, got, 16) == 0u); /* the same chord again: no re-strike */
    up(pad(1, 7, 3)); quiet();
    tc_state.live_update = 0u;
    /* MIDI in: off by default; on channel 2, chords there set the live chord, others don't. */
    midi(0x91, 50, 100); midi(0x91, 53, 100); midi(0x91, 57, 100); run(5);
    assert(!tc_state.live_from_midi && live_note(7, 3) == 53u);
    midi(0x81, 50, 0); midi(0x81, 53, 0); midi(0x81, 57, 0);
    tc_state.live_in = 2u; run(5);
    midi(0x90, 50, 100); midi(0x90, 53, 100); midi(0x90, 57, 100); run(5); /* channel 1 */
    assert(!tc_state.live_from_midi);
    midi(0x91, 50, 100); midi(0x91, 53, 100); midi(0x91, 57, 100); run(5); /* channel 2: Dm */
    assert(tc_state.live_from_midi && live_note(7, 3) == 50u && live_note(8, 3) == 53u && live_note(9, 3) == 57u);
    midi(0x81, 57, 0); midi(0x81, 53, 0); run(5); /* released note by note: still Dm */
    assert(live_note(8, 3) == 53u);
    midi(0x81, 50, 0); midi(0x91, 55, 100); run(5); assert(live_note(8, 3) == 53u); /* one note is no chord */
    midi(0x91, 59, 90); midi(0x91, 62, 90); run(5); assert(live_note(7, 3) == 55u && live_note(8, 3) == 59u); /* G */
    logged = 0; strike(pad(1, 9, 3)); assert(ons(1, got, 16) == 1u && got[0] == 62u); quiet();
    midi(0xB1, 123, 0); /* all notes off clears what is held, not the chord */
    assert(live_note(7, 3) == 55u);
    /* Any channel; then a chord pad takes over again. */
    tc_state.live_in = 17u; run(5);
    midi(0x9F, 52, 100); midi(0x9F, 56, 100); midi(0x9F, 59, 100); run(5); /* E on channel 16 */
    assert(live_note(7, 3) == 52u && live_note(8, 3) == 56u);
    strike(pad(0, 1, 3)); assert(!tc_state.live_from_midi && live_note(7, 3) == 50u && live_note(8, 3) == 53u);
    midi(0x8F, 52, 0); midi(0x8F, 56, 0); midi(0x8F, 59, 0);
    tc_state.live_in = 0u;
    quiet();
}
int main(void) {
    layout();
    engine();
    puts("PASS: live notes: scale columns and live-chord rows (tones climb from the root, rows an octave apart), chord tones from pads with edits and from held MIDI notes (lowest = root, 2+ pitch classes), every pad a chord tone in range in every key/scale/octave; engine: first chord before any, kept after release, silent chord pads, own channel, re-strike off/on/same chord, MIDI in off/channel/any, note-by-note release keeps the chord, pads take over again");
    return 0;
}
