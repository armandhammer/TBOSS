/* SPDX-License-Identifier: GPL-3.0-or-later - the engine (engine.h): what plays by itself. The board's clock, the
 * pedal's note, the progression, the step sequencer's lanes and scale lock (notes arriving over MIDI). */
#include "engine_internal.h"
#include "midi_in.h"
#include "sequence.h"
#include <string.h>
/* One MIDI clock (1/24 beat) in microseconds: from the MIDI clock coming in, else the tempo. */
uint32_t tp_tick_us(uint32_t now) {
    return tm_clock_active(now) && tm.tick_us ? tm.tick_us : 2500000u / tc_tempo_bpm(tc_state.tempo);
}
/* The clock position (rhythm.h, 1/256 clocks): the clock in use, the DAW's while it comes in, else
 * the board's own (tick_clock has run this pass), plus how far into the current clock now is. */
uint32_t tp_clock_pos(const tp_engine *e, uint32_t now) {
    uint32_t tick = tp_tick_us(now), ticks, into;
    if (tm_clock_active(now)) { ticks = tm.ticks; into = (now - tm.last_tick_ms) * 1000u; }
    else {
        ticks = e->clock_ticks;
        int32_t since = (int32_t)(now * 1000u - (e->clock_us - tick)); /* the board's last tick */
        into = since > 0 ? (uint32_t)since : 0u;
    }
    uint32_t sub = tick && into < tick ? into * RH_SUB / tick : RH_SUB - 1u; /* a clock is under 65 ms: no overflow */
    return ticks * RH_SUB + sub;
}
/* The board's own clock: 24 ticks per beat at the tempo; with Clock out on (and no
 * MIDI clock coming in) every tick is owed as a MIDI clock byte. */
void tp_tick_clock(tp_engine *e, uint32_t now) {
    uint32_t now_us = now * 1000u, tick = 2500000u / tc_tempo_bpm(tc_state.tempo);
    bool send = tc_state.clock_out && !tm_clock_active(now) && e->state == TK_ACTIVE;
    if (!e->clock_started) { e->clock_started = true; e->clock_us = now_us; }
    for (uint32_t i = 0; i < 8u && (int32_t)(now_us - e->clock_us) >= 0; ++i) {
        e->clock_ticks++; e->clock_us += tick;
        if (send && e->clock_due < 8u) e->clock_due++;
    }
    if ((int32_t)(now_us - e->clock_us) >= 0) e->clock_us = now_us + tick; /* far behind: resync, never burst */
    if (!send) e->clock_due = 0u;
}
/* The pedal's note (Pedal mode Note): sounds while the pedal is down. */
void tp_pedal_note(tp_engine *e, uint32_t now) {
    tp_key *p = &e->keys[TP_PEDAL];
    bool down = e->state == TK_ACTIVE && tc_state.pedal_down_note;
    if (p->admitted && !down) { tp_disown(e, p); p->admitted = false; return; }
    if (p->admitted || !down) return;
    tp_notes one; memset(&one, 0, sizeof one);
    one.notes[0] = (uint8_t)(tc_state.pedal_note & 127u); one.count = 1;
    uint8_t ch = tc_channel_of(&tc_state, TP_PEDAL);
    if (tp_blocked(e, ch, &one)) return;
    tp_start(p, &one, ch, 100u, now);
    tp_own(e, p);
}
/* The progression (0.11.0, sequence.h): while the transport runs (or a DAW plays and sends
 * its clock), its current step sounds as if that chord pad were held: strum, the chord
 * panel, voice leading, fold and Sound off apply, and with the arpeggiator on it feeds the
 * arpeggiator instead. The step follows a position in MIDI clocks: with a DAW playing, its
 * song position (Start, Continue and Song Position Pointer all land on the right chord);
 * otherwise clocks since the first chord (the DAW's clock if one comes in, else the
 * tempo's), kept when the session pauses for the editor. A step that cannot be voiced stays
 * silent (never stops the session). A changed progression starts again from its first
 * chord (with a DAW, at the song position). */
void tp_seq_release(tp_engine *e) {
    tp_key *q = &e->keys[TP_SEQ];
    if (q->admitted) { if (!q->silent) tp_disown(e, q); q->admitted = false; }
}
/* The step at pos (clocks from the first chord): its entry, and a count that grows by one
 * with every step played, so a progression of one chord still strikes again each time round. */
static void seq_locate(uint32_t pos, uint32_t unit, uint8_t *entry, uint32_t *step) {
    uint32_t cycle = 0u, i = 0u;
    for (uint32_t j = 0; j < sq.count; ++j) cycle += sq.entries[j].units * unit;
    uint32_t at = pos % cycle;
    while (i + 1u < sq.count && at >= sq.entries[i].units * unit) { at -= sq.entries[i].units * unit; ++i; }
    *entry = (uint8_t)i; *step = pos / cycle * sq.count + i;
}
void tp_sequence(tp_engine *e, uint32_t now) {
    tp_key *q = &e->keys[TP_SEQ];
    bool clocked = tm_clock_active(now), daw = tm.running && clocked;
    bool run = e->state == TK_ACTIVE && sq.count && !tc_state.recording && (tc_state.transport || daw);
    if (!run) { /* stopped; a session ending (the editor, USB) keeps the place for the next one */
        tp_seq_release(e); e->seq_pending = false;
        if (e->state == TK_ACTIVE) e->seq_running = false;
        return;
    }
    uint32_t ticks = clocked ? tm.ticks : e->clock_ticks;
    bool top = !e->seq_running || daw != e->seq_daw; /* from the first chord */
    if (top) e->seq_starts = tm.starts;
    if (sq_changes != e->seq_changes) { e->seq_changes = sq_changes; e->seq_pending = true; top = top || !daw; }
    if (daw && tm.starts != e->seq_starts) { e->seq_starts = tm.starts; e->seq_pending = true; } /* Start again */
    if (top) e->seq_origin = ticks;
    else if (!daw && clocked != e->seq_clocked) e->seq_origin = ticks - e->seq_pos; /* other clock, same place */
    else if (!daw && (int32_t)(ticks - e->seq_origin) < 0) e->seq_origin = ticks;  /* the board's Start reset its clock */
    e->seq_pos = daw ? tm.position : ticks - e->seq_origin;
    uint8_t entry; uint32_t step;
    seq_locate(e->seq_pos, sq_unit_ticks(sq.unit), &entry, &step);
    if (top || step != e->seq_step || entry != e->seq_entry) { e->seq_step = step; e->seq_entry = entry; e->seq_pending = true; }
    e->seq_running = true; e->seq_daw = daw; e->seq_clocked = clocked;
    bool arp = tc_state.arp != TC_ARP_OFF;
    if (q->admitted && q->silent != arp) e->seq_pending = true; /* the arpeggiator was switched */
    if (!e->seq_pending) return;
    tp_seq_release(e);
    const sq_entry *en = &sq.entries[e->seq_entry];
    tp_layout l = sq_layout(&sq, e->layout, en);
    tp_notes notes;
    if (!tc_pad_with(l, en->pad, en->edit, &notes)) memset(&notes, 0, sizeof notes);
    if ((tc_state.chord_mute >> en->pad) & 1u) notes.count = 0u; /* Sound off: only the live notes follow */
    uint8_t ch = tc_channel_of(&tc_state, TP_ARP); /* the chord channel */
    if (!arp && tp_blocked(e, ch, &notes)) return; /* an owed Note Off first: next pass */
    e->seq_pending = false;
    tp_start(q, &notes, ch, (uint8_t)(tc_state.vel_mode == TC_VEL_FIXED ? 100u : en->velocity), now); /* Fixed wins */
    q->silent = arp; q->order = ++e->admissions;
    if (!arp) {
        tp_strum(e, q, tc_state.strum != TC_STRUM_OFF && !((tc_state.strum_off >> en->pad) & 1u), now);
        tp_own(e, q);
    }
    /* Voice leading continues from it, and the live notes follow it (Restrike re-strikes them). */
    tp_notes core;
    if (notes.count && tc_pad_core_with(l, en->pad, en->edit, &core)) tc_state.lead = core;
    tc_live_chord chord;
    tc_live_of_pad(l, en->pad, en->edit, &chord);
    bool same = tc_state.live_from_midi && !memcmp(&chord, &tc_state.live_midi, sizeof chord);
    tc_state.live_midi = chord; tc_state.live_from_midi = true;
    if (!same && tc_state.live_update) tc_state.relayout_live = true;
}
/* The step sequencer (1.5.0, lanes.h): while the transport runs or a DAW plays, each lane's step
 * comes from the position as the progression's does (with a DAW, its song position, so Start,
 * Continue and Song Position Pointer land on the right step; otherwise the position since the
 * pattern started, on the DAW's clock if one comes in, else the tempo's, kept when the session
 * pauses). A step that is on sounds for half its length through the lane's own source: chord steps
 * as a progression's chord (strum, the chord panel and voice leading apply), melody and drum steps
 * one note. A step that comes too late (an owed Note Off kept it waiting past its gate) is skipped.
 * Lanes always sound themselves: they don't feed the arpeggiator. */
bool tc_step_notes(tp_layout layout, uint8_t kind, uint8_t channel, const ln_step *s, tp_notes *out, uint8_t *ch) {
    memset(out, 0, sizeof *out);
    if (s->scale && tc_scale_defined((uint8_t)(s->scale - 1u))) layout.mode = (uint8_t)(s->scale - 1u); /* borrowed */
    uint8_t own = 0u;
    bool ok = false;
    if (kind == LN_CHORDS) {
        own = tc_state.chord_ch;
        ok = s->value < 48u && !tc_pad_empty(layout, s->value) && tc_pad_with(layout, s->value, ln_edit(s->edit), out);
    } else if (kind == LN_MELODY) { /* the right half's Scale layout, whatever it shows now */
        own = tc_state.melody_ch; layout.melody_layout = TP_MELODY_SCALE;
        ok = s->value < 48u && tc_notes(layout, 48u + s->value, (tc_chord){0, 0, 0}, out);
    } else if (kind == LN_DRUMS) {
        own = tc_state.drum_ch; out->notes[0] = (uint8_t)(s->value & 127u); out->count = 1u; ok = true;
    }
    *ch = (uint8_t)((channel ? channel - 1u : own) & 15u);
    if (!ok) memset(out, 0, sizeof *out);
    return ok && out->count;
}
void tp_lane_release(tp_engine *e, uint32_t i) {
    tp_key *q = &e->keys[TP_LANE0 + i];
    if (q->admitted) { tp_disown(e, q); q->admitted = false; }
    e->lane_tied[i] = false;
}
void tp_lanes(tp_engine *e, uint32_t now) {
    bool clocked = tm_clock_active(now), daw = tm.running && clocked;
    bool run = e->state == TK_ACTIVE && !tc_state.recording && (tc_state.transport || daw);
    if (!run) { /* stopped; a session ending keeps the place for the next one */
        for (uint32_t i = 0; i < LN_LANES; ++i) { tp_lane_release(e, i); e->lane_pending[i] = false; e->lane_step[i] = LN_NONE; }
        if (e->state == TK_ACTIVE) e->lane_running = false;
        return;
    }
    uint32_t cpos = tp_clock_pos(e, now);
    bool top = !e->lane_running || daw != e->lane_daw; /* from the first step */
    if (top) e->lane_origin = cpos;
    else if (!daw && clocked != e->lane_clocked) e->lane_origin = cpos - e->lane_pos; /* other clock, same place */
    else if (!daw && (int32_t)(cpos - e->lane_origin) < 0) e->lane_origin = cpos;     /* the board's Start reset its clock */
    /* Song position: before the first clock after Start or a Song Position Pointer, exactly there. */
    e->lane_pos = daw ? tm.position * RH_SUB + (tm.position_next ? 0u : cpos % RH_SUB) : cpos - e->lane_origin;
    if (top) for (uint32_t i = 0; i < LN_LANES; ++i) e->lane_count[i] = UINT32_MAX;
    e->lane_running = true; e->lane_daw = daw; e->lane_clocked = clocked;
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        const ln_lane *l = &ln.lanes[i];
        tp_key *q = &e->keys[TP_LANE0 + i];
        rh_rate rate = tc_arp_rates[ln_rate(l)];
        uint32_t pos = e->lane_pos, count = rh_step_at(pos, rate);
        bool plays = l->kind != LN_OFF && !l->mute;
        if (count != e->lane_count[i]) { /* a new step: by the direction; its condition and probability */
            uint32_t loop, step = ln_order(l, i, ln.seed, count, &loop);
            e->lane_count[i] = count; e->lane_step[i] = (uint8_t)step;
            e->lane_sounds[i] = plays && ln_sounds(&l->steps[step], i, ln.seed, loop, step);
            /* A tie holds over steps that don't play, while its step is still on and tied, for at most a
             * time round of the lane (1.7.0 fix: a step that never plays again ended nothing). */
            const ln_step *t = &l->steps[e->lane_tie_step[i] % LN_STEPS];
            bool tie_ends = !t->on || !t->tie || count - e->lane_tie_count[i] >= ln_cycle(l);
            if (e->lane_sounds[i] || !e->lane_tied[i] || tie_ends) tp_lane_release(e, i);
            e->lane_pending[i] = e->lane_sounds[i]; e->lane_strike[i] = 0u;
        } else if (e->lane_step[i] >= ln_length(l)) e->lane_step[i] = (uint8_t)ln_order(l, i, ln.seed, count, NULL); /* shortened */
        if (!plays) { tp_lane_release(e, i); e->lane_pending[i] = false; continue; }
        if (!e->lane_sounds[i]) continue;
        const ln_step *s = &l->steps[e->lane_step[i]];
        /* Strike j of the step's n (ratchets) spans [at, end) and sounds for its gate; the last of a
         * tied step until a step after it plays. */
        uint32_t start = rh_step_start(count, rate), length = rh_step_start(count + 1u, rate) - start, strikes = ln_strikes(s);
        if (e->lane_strike[i] >= strikes) e->lane_strike[i] = (uint8_t)(strikes - 1u); /* edited while it plays */
        uint32_t j = e->lane_strike[i], at = start + length * j / strikes, end = start + length * (j + 1u) / strikes;
        uint32_t off = at + (end - at) * ln_gate_pct(s) / 100u;
        bool last = j + 1u >= strikes, held = last && s->tie, over = !held && (int32_t)(pos - off) >= 0;
        if (q->admitted) {
            if (over) { tp_lane_release(e, i); if (!last) { e->lane_strike[i]++; e->lane_pending[i] = true; } }
            continue;
        }
        if (!e->lane_pending[i] || (int32_t)(pos - at) < 0) continue;
        if (over) { if (last) e->lane_pending[i] = false; else e->lane_strike[i]++; continue; } /* too late for it */
        tp_notes notes; uint8_t ch;
        if (!tc_step_notes(e->layout, l->kind, l->channel, s, &notes, &ch)) { e->lane_pending[i] = false; continue; }
        if (tp_blocked(e, ch, &notes)) continue; /* an owed Note Off first: next pass */
        e->lane_pending[i] = false; e->lane_tied[i] = held;
        if (held) { e->lane_tie_step[i] = e->lane_step[i]; e->lane_tie_count[i] = count; }
        bool melody = l->kind != LN_CHORDS;
        bool fixed = (melody ? tc_state.melody_vel : tc_state.vel_mode) == TC_VEL_FIXED;
        tp_start(q, &notes, ch, fixed || !s->velocity ? (uint8_t)LN_VELOCITY_DEFAULT : s->velocity, now); /* Fixed wins, as progressions */
        q->order = ++e->admissions;
        tp_strum(e, q, !melody && tc_state.strum != TC_STRUM_OFF, now);
        tp_own(e, q);
        tp_notes core; /* a chord lane's chords lead on from each other (voice leading) */
        if (!melody && tc_pad_core_with(e->layout, s->value, ln_edit(s->edit), &core)) tc_state.lead = core;
    }
}
/* Scale lock (1.0.0): notes arriving on the lock channel (midi_in.c tm.lock_vel) play on the
 * melody channel, moved to the nearest note of the scale (tc_quantise). Each keeps the note it
 * was given until the incoming note ends (or strikes again), whatever the key does meanwhile. */
static uint32_t lock_seen;
static bool lock_waiting; /* a note waits for a free source or an owed Note Off */
void tp_lock_notes(tp_engine *e, uint32_t now) {
    if (tm.lock_channel != tc_state.lock) { tm.lock_channel = tc_state.lock; memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++; }
    bool active = e->state == TK_ACTIVE;
    for (uint32_t i = 0; i < TP_LOCKS; ++i) {
        tp_key *key = &e->keys[TP_LOCK + i];
        if (key->admitted && (!active || !tm.lock_vel[key->input] || tm.lock_strikes[key->input] != key->strike)) {
            tp_disown(e, key); key->admitted = false; lock_waiting = true;
        }
    }
    if (!active || (tm.lock_changed == lock_seen && !lock_waiting)) return;
    lock_seen = tm.lock_changed; lock_waiting = false;
    for (uint32_t p = 0; p < 128u; ++p) {
        if (!tm.lock_vel[p]) continue;
        tp_key *free = 0; bool playing = false;
        for (uint32_t i = 0; i < TP_LOCKS; ++i) {
            tp_key *key = &e->keys[TP_LOCK + i];
            if (key->admitted && key->input == p) playing = true;
            else if (!key->admitted && !free) free = key;
        }
        if (playing) continue;
        if (!free) { lock_waiting = true; return; }
        tp_notes one; memset(&one, 0, sizeof one);
        one.notes[0] = tc_quantise(e->layout, (uint8_t)p); one.count = 1;
        uint8_t ch = (uint8_t)(tc_state.melody_ch & 15u);
        if (tp_blocked(e, ch, &one)) { lock_waiting = true; continue; }
        tp_start(free, &one, ch, tm.lock_vel[p], now); free->order = ++e->admissions;
        free->input = (uint8_t)p; free->strike = tm.lock_strikes[p];
        tp_own(e, free);
    }
}
