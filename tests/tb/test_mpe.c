/* SPDX-License-Identifier: GPL-3.0-or-later - 1.8.0, MPE and Glide: the configuration message (each zone's
 * member count on its master channel before any note, again 3 s later and at a DAW's Start, 0 when MPE
 * goes off, never with MPE off), a channel of its own for every pad (distinct while there are channels,
 * the longest unused first, a note two chord pads share sounding on each pad's channel, shared once all
 * are taken), each pad's pressure on its channel only (channel pressure for Poly and Channel, with a
 * controller or a bend of up to 2 semitones; nothing for Off), a channel's leftover values set before
 * its next Note On, every channel back to rest when a session drains, sustain and programs on the master
 * channels, MPE off as before; Glide (no new note for a pad beside a held one, the bend between the two
 * notes as their pressures weigh, the note handed over when the first pad lifts, a chain along a row, not
 * across a gap, not with Glide off or the Bass layout, Mono keeps the gliding pair); the settings (round
 * trip, out of range refused, what 1.7 reads) and the MIDI page.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
static event log_[32768];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 32768u);
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
static void quiet_at(int line) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p)
        if (sounding[ch][p]) { fprintf(stderr, "line %d: channel %u note %u still sounding\n", line, ch + 1u, p); assert(0); }
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
#define quiet() quiet_at(__LINE__)
static uint8_t kind_of(const event *v) { return v->status >> 4; }
static uint8_t ch_of(const event *v) { return v->status & 15u; }
static uint32_t bend_of(const event *v) { return v->a | (uint32_t)v->b << 7; }
static void lift_all(void) { memset(scan.values, 0, sizeof scan.values); }
/* A fresh session (the engine's carry-over cleared) with these settings, a second in. */
static void fresh(uint8_t mpe, uint8_t at) {
    lift_all(); memset(&e, 0, sizeof e); memset(sounding, 0, sizeof sounding);
    tc_init(&tc_state);
    tc_state.mpe = mpe; tc_state.at_chord = tc_state.at_melody = at; tc_state.unstick = 0u;
    tc_state.glide = TC_GLIDE_OFF; /* 1.8.6's default Roll would glide neighbours: each test chooses its Glide */
    tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
    run(1000);
    logged = 0;
}
static void press(uint32_t k, uint16_t level) { scan.values[k] = level; }
static void lift(uint32_t k) { scan.values[k] = 0u; }
/* Melody pad at a column (0..11) and row from the bottom (0..3); chord pad likewise. */
static uint32_t pad(uint32_t column, uint32_t up) { return 48u + 4u * column + (3u - up); }
static uint32_t chord(uint32_t column, uint32_t up) { return 4u * column + (3u - up); }
static uint8_t own_note(uint32_t k) { tp_notes n; assert(tc_pad(e.layout, k, &n) && n.count); return n.notes[0]; }
/* The channel the last Note On of this pitch went on (-1: none). */
static int on_channel(uint8_t pitch) {
    for (uint32_t j = logged; j-- > 0;) if (kind_of(&log_[j]) == TK_ON && log_[j].a == pitch) return ch_of(&log_[j]);
    return -1;
}
static uint32_t count(uint8_t kind, int ch) {
    uint32_t n = 0;
    for (uint32_t j = 0; j < logged; ++j) n += kind_of(&log_[j]) == kind && (ch < 0 || ch_of(&log_[j]) == ch);
    return n;
}
/* The last value of this kind on channel ch (a controller: its number), -1 when none was sent. */
static int32_t last(uint8_t kind, uint8_t ch, uint8_t number) {
    for (uint32_t j = logged; j-- > 0;) {
        const event *v = &log_[j];
        if (kind_of(v) != kind || ch_of(v) != ch || (kind == TK_CC && v->a != number)) continue;
        return kind == TK_BEND ? (int32_t)bend_of(v) : kind == TK_CHANNEL ? v->a : v->b;
    }
    return -1;
}
/* --- the configuration message ----------------------------------------------------------------- */
static uint32_t configs(uint8_t ch, uint8_t members) { /* complete RPN 6 messages saying `members` on ch */
    uint32_t n = 0;
    for (uint32_t j = 0; j + 4u < logged; ++j) {
        const event *v = &log_[j];
        if (v->status != (0xB0u | ch) || v->a != 101u || v->b != 0u) continue;
        assert(log_[j + 1].status == v->status && log_[j + 1].a == 100u && log_[j + 1].b == 6u);
        assert(log_[j + 2].status == v->status && log_[j + 2].a == 6u);
        assert(log_[j + 3].a == 101u && log_[j + 3].b == 127u && log_[j + 4].a == 100u && log_[j + 4].b == 127u);
        n += log_[j + 2].b == members;
    }
    return n;
}
static void config_message(void) {
    /* MPE off: none, ever. */
    lift_all(); memset(&e, 0, sizeof e); logged = 0;
    tc_init(&tc_state); e.layout = tc_layout(&tc_state); tc_state.unstick = 0u;
    run(4000); tm.starts++; run(100);
    for (uint32_t j = 0; j < logged; ++j) assert(kind_of(&log_[j]) != TK_CC);
    /* Melody: the Lower Zone's 7 on channel 1 first thing, before any note; again 3 s in. */
    lift_all(); memset(&e, 0, sizeof e); logged = 0; tc_state.mpe = TC_MPE_MELODY;
    run(700); /* a fresh session (500 ms after the last), its message at once */
    press(pad(0, 0), 2400u); run(50);
    assert(configs(0u, 7u) == 1u && !configs(15u, 7u) && log_[0].status == 0xB0u && log_[0].a == 101u);
    for (uint32_t j = 0; j < 5u; ++j) assert(kind_of(&log_[j]) == TK_CC);
    assert(count(TK_ON, -1) == 1u);
    run(3000); assert(configs(0u, 7u) == 2u);
    tm.starts++; run(50); assert(configs(0u, 7u) == 3u); /* a DAW's Start */
    /* Both: each zone, the Upper Zone's on channel 16; off: a 0 for each. */
    tc_state.mpe = TC_MPE_BOTH; run(50); assert(configs(15u, 7u) == 1u && configs(0u, 7u) == 3u);
    tc_state.mpe = TC_MPE_OFF; run(50); assert(configs(0u, 0u) == 1u && configs(15u, 0u) == 1u);
    run(5000); assert(configs(0u, 0u) == 1u && configs(15u, 0u) == 1u); /* once */
    lift_all(); run(200); quiet();
    /* The Drums layout: the melody half isn't MPE. */
    tc_state.mpe = TC_MPE_MELODY; tc_state.melody_layout = TP_MELODY_DRUMS;
    assert(!tc_mpe_half(&tc_state, 1u) && tc_mpe_half(&tc_state, 0u) == false);
    tc_state.melody_layout = TP_MELODY_SCALE;
    puts("configuration message: before notes, at +3 s, at Start, per zone, 0 when off, never with MPE off");
}
/* --- channels ------------------------------------------------------------------------------------ */
static void channels(void) {
    fresh(TC_MPE_MELODY, TC_AT_OFF);
    assert(tc_half_ch(&tc_state, 1u) == 0u && tc_half_ch(&tc_state, 0u) == 15u);
    /* Seven melody pads: seven channels, 2..8. */
    bool used[16] = {false};
    for (uint32_t i = 0; i < 7u; ++i) {
        press(pad(i, 1), 2400u); run(20);
        int ch = on_channel(own_note(pad(i, 1)));
        assert(ch >= 1 && ch <= 7 && !used[ch]); used[ch] = true;
    }
    /* An eighth shares one (sounding once per channel: the writer checks). */
    press(pad(8, 1), 2400u); run(20);
    int shared = on_channel(own_note(pad(8, 1)));
    assert(shared >= 1 && shared <= 7);
    lift_all(); run(200); quiet();
    /* The chord half not in MPE plays plainly on channel 16. */
    press(chord(0, 0), 2400u); run(30);
    tp_notes n; assert(tc_pad(e.layout, chord(0, 0), &n));
    for (uint32_t i = 0; i < n.count; ++i) assert(on_channel(n.notes[i]) == 15);
    lift_all(); run(200); quiet();
    /* The longest unused first: a pad lifted, the next takes another channel. */
    press(pad(0, 0), 2400u); run(20); int a = on_channel(own_note(pad(0, 0)));
    lift(pad(0, 0)); run(100);
    press(pad(0, 0), 2400u); run(20); int b = on_channel(own_note(pad(0, 0)));
    assert(a != b && b >= 1 && b <= 7);
    lift_all(); run(200); quiet();
    /* Two chord pads sharing notes (C and Am in C major share C and E): each pad's on its own channel. */
    fresh(TC_MPE_CHORDS, TC_AT_OFF);
    tp_notes c, am; uint32_t k1 = 0, k2 = 0, common = 0;
    for (uint32_t i = 0; i < 48u && !common; ++i) for (uint32_t j = i + 1u; j < 48u && !common; ++j) {
        if (j % 4u == i % 4u) continue; /* different rows: no pad beside another in the same chord */
        assert(tc_pad(e.layout, i, &c) && tc_pad(e.layout, j, &am));
        for (uint32_t x = 0; x < c.count; ++x) for (uint32_t y = 0; y < am.count; ++y) common += c.notes[x] == am.notes[y];
        k1 = i; k2 = j;
    }
    assert(common >= 1u);
    press(k1, 2400u); run(30);
    press(k2, 2400u); run(30);
    uint8_t cc = e.keys[k1].channel, ac = e.keys[k2].channel;
    assert(cc != ac && cc >= 8u && cc <= 14u && ac >= 8u && ac <= 14u);
    for (uint32_t i = 0; i < am.count; ++i) assert(on_channel(am.notes[i]) == ac); /* the shared ones too */
    uint32_t ons = count(TK_ON, -1);
    assert(ons == c.count + am.count); /* the shared notes twice, once per pad */
    lift_all(); run(200); quiet();
    /* MPE off: as before, the shared notes once, both on Chord ch. */
    fresh(TC_MPE_OFF, TC_AT_OFF);
    press(k1, 2400u); run(30); press(k2, 2400u); run(30);
    assert(count(TK_ON, -1) == c.count + am.count - common && count(TK_ON, 0) == count(TK_ON, -1));
    lift_all(); run(200); quiet();
    puts("channels: one per pad, 2..8 and 9..15, longest unused first, shared when all are taken, a shared note per pad");
}
/* --- pressure ------------------------------------------------------------------------------------ */
static void expression(void) {
    static const uint8_t modes[] = {TC_AT_POLY, TC_AT_CHANNEL, TC_AT_CC74, TC_AT_CC1, TC_AT_BEND, TC_AT_OFF};
    for (uint32_t m = 0; m < sizeof modes; ++m) {
        fresh(TC_MPE_MELODY, modes[m]);
        uint32_t a = pad(0, 0), b = pad(3, 0);
        press(a, 1400u); press(b, 2200u); run(200);
        uint8_t ca = (uint8_t)on_channel(own_note(a)), cb = (uint8_t)on_channel(own_note(b));
        uint8_t pa = e.keys[a].pressure, pb = e.keys[b].pressure;
        assert(ca != cb && pa && pb && pa != pb);
        assert(!count(TK_PRESSURE, -1)); /* never poly aftertouch on an MPE half */
        uint8_t number = tc_at_cc(modes[m]);
        if (modes[m] == TC_AT_OFF) {
            assert(!count(TK_CHANNEL, -1) && !count(TK_BEND, -1) && !count(TK_CC, ca) && !count(TK_CC, cb));
        } else {
            assert(last(TK_CHANNEL, ca, 0) == pa && last(TK_CHANNEL, cb, 0) == pb);
            if (number) assert(last(TK_CC, ca, number) == pa && last(TK_CC, cb, number) == pb);
            else assert(!count(TK_CC, ca) && !count(TK_CC, cb));
            if (modes[m] == TC_AT_BEND) {
                int32_t want = 8192 + pb * (8192 * 2 / TP_MPE_BEND_RANGE) / 127, got = last(TK_BEND, cb, 0);
                assert(got > 8192 && abs(got - want) < TP_MPE_BEND_DEADBAND && got <= 8192 + 341);
            } else assert(!count(TK_BEND, -1));
            /* Harder on one pad: only its channel moves. */
            uint32_t before = count(TK_CHANNEL, ca);
            press(b, 2500u); run(200);
            assert(last(TK_CHANNEL, cb, 0) == 127 && count(TK_CHANNEL, ca) == before);
            /* With a controller: each pressure and the same value on the controller together, not a pressure
             * time apart. */
            for (uint32_t j = 0; number && j < logged; ++j) {
                if (kind_of(&log_[j]) != TK_CHANNEL || !log_[j].a) continue;
                uint32_t i = j + 1u;
                while (i < logged && !(kind_of(&log_[i]) == TK_CC && ch_of(&log_[i]) == ch_of(&log_[j]) && log_[i].a == number)) i++;
                assert(i < logged && log_[i].b == log_[j].a && log_[i].at - log_[j].at <= 3u);
            }
            /* Lifted: its channel back to rest. */
            lift(b); run(200);
            assert(last(TK_CHANNEL, cb, 0) == 0 && last(TK_CHANNEL, ca, 0) == pa);
            if (number) assert(last(TK_CC, cb, number) == 0);
            if (modes[m] == TC_AT_BEND) assert(last(TK_BEND, cb, 0) == 8192);
        }
        lift_all(); run(300); quiet();
        for (uint8_t ch = 1; ch <= 14u; ++ch) assert(!e.mpe_z[ch] && !e.mpe_cc[ch] && !e.mpe_bend[ch]);
    }
    /* The other half plainly: chords on channel 16 with one channel pressure for the half. */
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL);
    press(chord(0, 0), 2000u); run(200);
    assert(last(TK_CHANNEL, 15u, 0) == e.keys[chord(0, 0)].pressure);
    lift_all(); run(300); quiet();
    puts("pressure: per pad on its channel (channel pressure, with a controller or a bend), Off sends none, back to rest");
}
/* --- a clean channel, draining ------------------------------------------------------------------- */
static void clean_and_drain(void) {
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL);
    /* Every member channel left at a pressure and a bend (as a lost session might leave them). */
    for (uint8_t ch = 1; ch <= 7u; ++ch) { e.mpe_z[ch] = 50u; e.mpe_bend[ch] = 300; e.mpe_at[ch] = e.mpe_bend_at[ch] = now; }
    uint32_t k = pad(2, 2);
    press(k, 2400u); run(30);
    uint32_t on = logged;
    for (uint32_t j = 0; j < logged; ++j) if (kind_of(&log_[j]) == TK_ON) { on = j; break; }
    assert(on < logged && on >= 2u);
    uint8_t ch = ch_of(&log_[on]);
    /* Before its Note On: the bend back to the centre, the pressure set to the pad's own. */
    assert(log_[on - 2u].status == (0xE0u | ch) && bend_of(&log_[on - 2u]) == 8192u);
    assert(log_[on - 1u].status == (0xD0u | ch) && log_[on - 1u].a == e.keys[k].pressure);
    /* A channel already at rest: the Note On straight away. */
    lift_all(); run(500); quiet();
    logged = 0; press(k, 2400u); run(30);
    assert(kind_of(&log_[0]) == TK_ON || kind_of(&log_[0]) == TK_CHANNEL);
    for (uint32_t j = 0; j < logged && kind_of(&log_[j]) != TK_ON; ++j) assert(kind_of(&log_[j]) != TK_BEND);
    /* Draining with pads held: every channel's values back to rest at once, then done. */
    fresh(TC_MPE_BOTH, TC_AT_BEND);
    press(pad(0, 0), 2400u); press(pad(5, 1), 2200u); press(chord(1, 0), 2100u); run(200);
    assert(count(TK_BEND, -1) >= 3u);
    tp_disable(&e);
    for (uint32_t i = 0; i < 100u && e.state == TK_DRAINING; ++i) {
        tc_step(&e, ++now, true, writer); if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
    }
    assert(e.state == TK_DONE);
    for (uint8_t c = 1; c <= 14u; ++c) {
        assert(!e.mpe_z[c] && !e.mpe_cc[c] && !e.mpe_bend[c]);
        int32_t z = last(TK_CHANNEL, c, 0), b = last(TK_BEND, c, 0);
        assert(z <= 0 && (b < 0 || b == 8192));
    }
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[c][p]);
    puts("clean: a channel's leftovers set before its Note On, none when at rest; a drain rests every channel");
}
/* --- sustain and programs ------------------------------------------------------------------------ */
static void masters(void) {
    fresh(TC_MPE_BOTH, TC_AT_OFF);
    tc_state.chord_prog = 5u; tc_state.melody_prog = 9u; tc_state.program_again = 3u;
    tc_state.pedal_mode = TC_PEDAL_SUSTAIN; tc_state.sustain = true;
    press(pad(0, 0), 2400u); run(50);
    assert(last(TK_CC, 0u, 64u) == 127 && last(TK_CC, 15u, 64u) == 127);
    bool p0 = false, p15 = false;
    for (uint32_t j = 0; j < logged; ++j) {
        if (log_[j].status == 0xC0u) p0 = log_[j].a == 8u;
        if (log_[j].status == 0xCFu) p15 = log_[j].a == 4u;
    }
    assert(p0 && p15);
    tc_state.sustain = false; lift_all(); run(300); quiet();
    puts("masters: sustain and programs on channels 1 and 16");
}
/* --- Glide --------------------------------------------------------------------------------------- */
static int32_t glide_want(uint32_t held, uint32_t gliding) {
    const tp_key *a = &e.keys[held], *b = &e.keys[gliding];
    int32_t t = TP_GLIDE_BAND + (int32_t)b->pressure - (int32_t)a->pressure; /* 1.8.4: a crossfade */
    t = t < 0 ? 0 : t > 2 * TP_GLIDE_BAND ? 2 * TP_GLIDE_BAND : t;
    return 8192 + (a->glide_offset * (2 * TP_GLIDE_BAND - t) + b->glide_offset * t) * 8192 / (2 * TP_GLIDE_BAND * TP_MPE_BEND_RANGE);
}
static void glide(void) {
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = 1u;
    assert(tc_glides(&tc_state));
    uint32_t a = pad(0, 1), b = pad(1, 1), c = pad(2, 1), far = pad(5, 1);
    uint8_t na = own_note(a), nb = own_note(b), nc = own_note(c);
    assert(nb > na && nc > nb);
    press(a, 2400u); run(50);
    int ch = on_channel(na); assert(ch >= 1 && ch <= 7);
    /* The pad beside it: no note of its own, the bend between the two as they weigh. */
    press(b, 2000u); run(100);
    assert(on_channel(nb) < 0 && e.keys[b].admitted && e.keys[b].glide == a + 1u);
    int32_t got = last(TK_BEND, (uint8_t)ch, 0), want = glide_want(a, b);
    assert(got > 8192 && abs(got - want) < TP_MPE_BEND_DEADBAND);
    assert(last(TK_CHANNEL, (uint8_t)ch, 0) == e.keys[a].pressure); /* the stronger */
    press(b, 2500u); press(a, 2000u); run(100);
    got = last(TK_BEND, (uint8_t)ch, 0); want = glide_want(a, b);
    assert(abs(got - want) < TP_MPE_BEND_DEADBAND);
    /* 1.8.4 (a board test: rolled hard onto D, the note stayed flat of D): hard on the new pad with the first
     * still lightly touched is exactly the new pad's note; even pressure is halfway. */
    int32_t at_b = 8192 + (int32_t)(nb - na) * 8192 / TP_MPE_BEND_RANGE;
    press(b, 2500u); press(a, 1000u); run(100);
    assert(e.keys[a].admitted && e.keys[a].pressure > 0u && e.keys[b].pressure == 127u);
    assert(abs(last(TK_BEND, (uint8_t)ch, 0) - at_b) < TP_MPE_BEND_DEADBAND);
    press(b, 2000u); press(a, 2000u); run(100);
    assert(abs(last(TK_BEND, (uint8_t)ch, 0) - (8192 + (at_b - 8192) / 2)) < TP_MPE_BEND_DEADBAND);
    press(b, 1000u); press(a, 2500u); run(100);
    assert(abs(last(TK_BEND, (uint8_t)ch, 0) - 8192) < TP_MPE_BEND_DEADBAND); /* hard on the first: its own */
    press(b, 2500u); press(a, 2000u); run(100);
    /* The first lifted: the note goes on (no Note Off), bent all the way to the second's. */
    uint32_t offs = count(TK_OFF, -1);
    lift(a); run(100);
    assert(count(TK_OFF, -1) == offs && e.keys[b].captured.count && !e.keys[b].glide && e.keys[b].channel == ch);
    int32_t full = 8192 + (int32_t)(nb - na) * 8192 / TP_MPE_BEND_RANGE;
    assert(abs(last(TK_BEND, (uint8_t)ch, 0) - full) < TP_MPE_BEND_DEADBAND);
    /* On along the row: the next pad glides from there. */
    press(c, 2400u); run(100);
    assert(on_channel(nc) < 0 && e.keys[c].glide == b + 1u && e.keys[c].glide_offset == (int8_t)(nc - na));
    lift(b); run(100);
    full = 8192 + (int32_t)(nc - na) * 8192 / TP_MPE_BEND_RANGE;
    assert(abs(last(TK_BEND, (uint8_t)ch, 0) - full) < TP_MPE_BEND_DEADBAND && count(TK_OFF, -1) == offs);
    /* A pad further along strikes its own note on its own channel. */
    press(far, 2400u); run(50);
    assert(on_channel(own_note(far)) >= 1 && on_channel(own_note(far)) != ch);
    /* All lifted: the original note's Note Off, the bend back to the centre. */
    lift_all(); run(300); quiet();
    assert(count(TK_OFF, ch) >= 1u && last(TK_BEND, (uint8_t)ch, 0) == 8192);
    /* The gliding pad lifted first: the bend comes back to the held note. */
    press(a, 2400u); run(50); ch = on_channel(na);
    press(b, 2400u); run(100); assert(last(TK_BEND, (uint8_t)ch, 0) > 8192);
    lift(b); run(100); assert(last(TK_BEND, (uint8_t)ch, 0) == 8192 && e.keys[a].admitted);
    lift_all(); run(300); quiet();
    /* Mono: the gliding pair stays (the held pad is not hidden under the gliding one). */
    tc_state.mono = 1u;
    press(a, 2400u); run(50); press(b, 2400u); run(100);
    assert(!e.keys[a].hidden && e.keys[a].admitted && !e.keys[a].silent && e.keys[b].glide == a + 1u);
    lift_all(); run(300); quiet(); tc_state.mono = 0u;
    /* Glide off, or the Bass layout, or note repeat: the pad beside strikes. */
    for (uint32_t w = 0; w < 3u; ++w) {
        fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = 1u;
        if (w == 0u) tc_state.glide = 0u;
        if (w == 1u) { tc_state.melody_layout = TP_MELODY_BASS; e.layout = tc_layout(&tc_state); }
        if (w == 2u) tc_state.note_repeat = 1u;
        assert(!tc_glides(&tc_state));
        press(a, 2400u); run(50); press(b, 2400u); run(50);
        assert(!e.keys[b].glide && e.keys[b].captured.count);
        lift_all(); run(300); quiet();
    }
    tc_state.note_repeat = 0u;
    /* MPE off: no glide. */
    fresh(TC_MPE_OFF, TC_AT_CHANNEL); tc_state.glide = 1u; assert(!tc_glides(&tc_state));
    puts("glide: the pad beside bends the held note as they weigh, takes it over, on along the row; not across a gap");
}
/* 1.8.5: Glide Pad (the new pad's pressure alone: a touch at Trigger doesn't bend, full pressure is exactly
 * the new note whatever the first pad does) and Glide Time (all the way over the Glide time, back the same
 * way when the new pad is lifted, the note handed over when the first pad lifts); every glide lands exactly. */
static void glide_modes(void) {
    uint32_t a = pad(0, 1), b = pad(1, 1);
    /* Pad. */
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = TC_GLIDE_PAD;
    uint8_t na = own_note(a), nb = own_note(b);
    int32_t at_b = 8192 + (int32_t)(nb - na) * 8192 / TP_MPE_BEND_RANGE;
    /* (The first pad pressed moderately: a much harder neighbor makes a light touch its bleed, 1.7.0.) */
    press(a, 1400u); run(50); int ch = on_channel(na);
    press(b, 750u); run(100); /* just past Trigger: a finger resting there */
    assert(e.keys[b].glide == a + 1u && last(TK_BEND, (uint8_t)ch, 0) <= 8192);
    press(b, 2500u); press(a, 2500u); run(100); /* full pressure on the new pad, the first pressed fully too */
    assert(e.keys[a].pressure == 127u && last(TK_BEND, (uint8_t)ch, 0) == at_b);
    press(b, 1600u); run(100);
    int32_t mid = last(TK_BEND, (uint8_t)ch, 0);
    assert(mid > 8192 + 20 && mid < at_b - 20);
    lift(b); run(100); assert(last(TK_BEND, (uint8_t)ch, 0) == 8192);
    lift_all(); run(300); quiet();
    /* Time, 100 ms (the default). */
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = TC_GLIDE_TIME;
    assert(tc_glide_ms[tc_state.glide_time] == 100u);
    press(a, 1400u); run(50); ch = on_channel(na);
    press(b, 750u); /* lightly: the pressure doesn't matter */
    uint32_t t0 = 0;
    for (uint32_t i = 0; i < 40u && !e.keys[b].glide; ++i) { run(1); t0 = now; }
    assert(e.keys[b].glide == a + 1u);
    run(45); int32_t half = last(TK_BEND, (uint8_t)ch, 0);
    assert(half > 8192 + (at_b - 8192) / 4 && half < 8192 + 3 * (at_b - 8192) / 4); /* about halfway */
    run(100); assert(last(TK_BEND, (uint8_t)ch, 0) == at_b && now - t0 >= 100u); /* there, exactly */
    lift(b); run(45);
    int32_t back = last(TK_BEND, (uint8_t)ch, 0);
    assert(back > 8192 && back < at_b); /* on its way back */
    run(100); assert(last(TK_BEND, (uint8_t)ch, 0) == 8192);
    /* Handed over mid-way: the note keeps going to the new pad's. */
    press(b, 750u); run(50); lift(a); run(150);
    assert(e.keys[b].captured.count && last(TK_BEND, (uint8_t)ch, 0) == at_b);
    lift_all(); run(300); quiet();
    /* A longer time takes longer. */
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = TC_GLIDE_TIME; tc_state.glide_time = TC_GLIDE_TIMES - 1u; /* 1 s */
    press(a, 1400u); run(50); ch = on_channel(na);
    press(b, 750u); run(500);
    assert(last(TK_BEND, (uint8_t)ch, 0) < at_b - 40);
    run(600); assert(last(TK_BEND, (uint8_t)ch, 0) == at_b);
    lift_all(); run(300); quiet();
    /* Roll lands exactly too, once still. */
    fresh(TC_MPE_MELODY, TC_AT_CHANNEL); tc_state.glide = TC_GLIDE_ROLL;
    press(a, 2400u); run(50); ch = on_channel(na);
    press(b, 2500u); press(a, 1000u); run(150);
    assert(last(TK_BEND, (uint8_t)ch, 0) == at_b);
    lift_all(); run(300); quiet();
    puts("glide modes: Pad (by the new pad's pressure, a resting touch doesn't bend), Time (100 ms and 1 s, back "
         "when lifted, handed over), every glide landing exactly");
}
/* --- settings and the page ----------------------------------------------------------------------- */
static bool on_page(uint8_t group, uint8_t item) {
    for (uint8_t i = 0; i < tc_group_size(group); ++i) if (tc_group_items[group][i] == item) return true;
    return false;
}
static void stored(void) {
    tc_controls a; tc_init(&a);
    uint32_t p[ST_PAYLOAD];
    assert(!a.mpe && a.glide == TC_GLIDE_ROLL); /* 1.8.6: Glide Roll by default */
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && !(p[28] >> 24)); /* defaults: what 1.7 reads */
    a.mpe = TC_MPE_BOTH; a.glide = TC_GLIDE_PAD;
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && p[28] >> 24 == (3u | 1u << 2));
    tc_controls b; tc_init(&b);
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK && b.mpe == TC_MPE_BOTH && b.glide == TC_GLIDE_PAD);
    a.mpe = TC_MPE_CHORDS; a.glide = TC_GLIDE_OFF;
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && p[28] >> 24 == (2u | 3u << 2));
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK && b.mpe == TC_MPE_CHORDS && b.glide == TC_GLIDE_OFF);
    /* 1.8.5: Glide Time and the slowest Glide time; 1.7's byte 0 reads as Roll, 100 ms (1.8.6). */
    a.mpe = TC_MPE_MELODY; a.glide = TC_GLIDE_TIME; a.glide_time = TC_GLIDE_TIMES - 1u;
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK && p[28] >> 24 == (1u | 2u << 2 | 5u << 4));
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK && b.glide == TC_GLIDE_TIME && b.glide_time == TC_GLIDE_TIMES - 1u);
    p[28] &= 0xFFFFFFu; /* as 1.7 stored it */
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK && !b.mpe && b.glide == TC_GLIDE_ROLL && b.glide_time == TC_GLIDE_TIME_DEFAULT);
    for (uint32_t v = 128u; v < 256u; v += 37u) {
        uint32_t bad[ST_PAYLOAD]; memcpy(bad, p, sizeof bad);
        bad[28] = (bad[28] & 0xFFFFFFu) | v << 24;
        tc_controls before = b;
        assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE && !memcmp(&before, &b, sizeof b));
    }
    assert(on_page(TC_GROUP_EXPRESSION, TC_MENU_MPE) && on_page(TC_GROUP_EXPRESSION, TC_MENU_GLIDE)); /* 2.0.1: Expression */
    assert(tc_menu_values(TC_MENU_MPE) == TC_MPE_MODES && tc_menu_values(TC_MENU_GLIDE) == TC_GLIDE_MODES &&
           tc_menu_values(TC_MENU_GLIDE_TIME) == TC_GLIDE_TIMES && on_page(TC_GROUP_EXPRESSION, TC_MENU_GLIDE_TIME));
    { /* Glide time shows on the page only with Glide Time */
      tc_controls g; tc_init(&g); uint8_t items[TC_GROUP_MAX]; uint8_t n = tc_page_items(&g, TC_GROUP_EXPRESSION, items); bool seen = false;
      for (uint8_t i = 0; i < n; ++i) seen = seen || items[i] == TC_MENU_GLIDE_TIME;
      assert(!seen); g.glide = TC_GLIDE_TIME; n = tc_page_items(&g, TC_GROUP_EXPRESSION, items);
      for (uint8_t i = 0; i < n; ++i) seen = seen || items[i] == TC_MENU_GLIDE_TIME;
      assert(seen); }
    tc_controls c; tc_init(&c);
    assert(!tc_menu_value(&c, TC_MENU_MPE) && tc_menu_value(&c, TC_MENU_GLIDE) == TC_GLIDE_ROLL);
    assert(tc_half_ch(&c, 0u) == c.chord_ch && tc_half_ch(&c, 1u) == c.melody_ch);
    puts("settings: word 28 byte 3 round trip, 0 by default (Glide Roll), out of range refused; on the MIDI page");
}
int main(void) {
    config_message();
    channels();
    expression();
    clean_and_drain();
    masters();
    glide();
    glide_modes();
    stored();
    puts("mpe: all passed");
    return 0;
}
