/* SPDX-License-Identifier: GPL-3.0-or-later - bounded source ownership, no heap/event bus.
 * Standalone: arms itself once USB is up, no host watchdog.
 * Ownership rule (unchanged): a source (pad or the arpeggiator) holds references
 * on (channel, pitch) voices; a voice's Note On goes out once while referenced and
 * its Note Off once all references are gone. One packet is in flight at a time and
 * every Note On reserves its Note Off before it is sent.
 * The engine's files: engine.c (sessions, note ownership, what goes out next), engine_pads.c (reading
 * the pads), engine_effects.c (strum, note repeat, echo, Mono), engine_arp.c (the arpeggiator) and
 * engine_play.c (the clock, progression, lanes, pedal note, scale lock); engine_internal.h joins them. */
#include "engine_internal.h"
#include "midi_in.h"
#include "sequence.h"
#include "chord_detect.h"
#include "bits.h"
#include <string.h>
void tp_stop(tp_engine *e) {
    e->errors++; e->state = TK_STOPPED;
    if (e->accepted) e->uncertain = true;
}
uint32_t tp_obligations(const tp_engine *e) {
    uint32_t n = 0;
    for (uint32_t ch = 0; ch < 16u; ++ch) n += e->exposed[ch];
    return n + (e->sustain_on[0] ? 1u : 0u) + (e->sustain_on[1] ? 1u : 0u);
}
void tp_disable(tp_engine *e) {
    if (e->state == TK_IDLE) { e->state = TK_DONE; return; }
    if (e->state != TK_ACTIVE) return;
    e->state = TK_DRAINING;
    for (uint32_t k = 0; k < TP_SOURCES; ++k) if (e->keys[k].admitted) {
        e->keys[k].release_needed = true; e->keys[k].latched = false;
    }
}
void tp_loss(tp_engine *e) {
    if (e->state == TK_ACTIVE || e->state == TK_DRAINING || e->pending_id) tp_stop(e);
}
void tp_complete(tp_engine *e, uint32_t token, uint32_t bytes, bool success, uint32_t now) {
    if (e->state == TK_STOPPED) return;
    if (!token || token != e->pending_id || bytes != 4u || !success) { tp_stop(e); return; }
    if (e->pending_kind == TK_OFF) {
        tp_voice *v = &e->voices[e->pending_channel & 15u][e->pending_pitch & 127u];
        if (v->exposed) { v->exposed = false; e->exposed[e->pending_channel & 15u]--; }
    }
    if (e->pending_kind == TK_CLOCK) { /* MIDI clock, or the transport's Start/Stop */
        if (e->pending_pitch == 0xF8u) { if (e->clock_due) e->clock_due--; }
        else {
            e->transport_sent = e->pending_pitch != 0xFCu; /* Start or Continue */
            e->transport_paused = e->pending_pitch == 0xFCu && e->state != TK_ACTIVE;
        }
    }
    if (e->pending_kind == TK_CC && e->pending_group >= 2u) { /* sustain (CC64) */
        e->sustain_on[e->pending_group - 2u] = e->pending_pressure != 0u;
        e->sustain_ch[e->pending_group - 2u] = e->pending_channel;
        e->sustain_cc[e->pending_group - 2u] = e->pending_pitch;
    } else if (e->pending_kind == TK_CHANNEL || e->pending_kind == TK_CC || e->pending_kind == TK_BEND) {
        e->group_sent[e->pending_group] = e->pending_pressure; e->group_at[e->pending_group] = now;
    }
    if (e->pending_kind == TK_ON || e->pending_kind == TK_PRESSURE) {
        /* Per note (pending_group is the note's index in the pad's notes). Receipt may
         * outlive logical release. No ownership is restored. */
        tp_key *key = &e->keys[e->pending_key];
        uint8_t i = (uint8_t)(e->pending_group % TP_MAX_NOTES);
        key->note_at[i] = now;
        if (e->pending_kind == TK_PRESSURE) key->note_pressure[i] = e->pending_pressure;
    }
    e->pending_id = 0u; e->completed++;
}
tp_voice *tp_voice_of(tp_engine *e, uint8_t ch, uint8_t pitch) { return &e->voices[ch & 15u][pitch & 127u]; }
/* A Note Off still owed on any of these notes must go out before they are reused. */
bool tp_blocked(tp_engine *e, uint8_t ch, const tp_notes *n) {
    for (uint32_t i = 0; i < n->count; ++i) {
        tp_voice *v = tp_voice_of(e, ch, n->notes[i]);
        if (v->exposed && !v->refs) return true;
    }
    return false;
}
void tp_own(tp_engine *e, tp_key *key) {
    for (uint32_t i = 0; i < key->captured.count; ++i) tp_voice_of(e, key->channel, key->captured.notes[i])->refs++;
}
void tp_disown(tp_engine *e, tp_key *key) {
    for (uint32_t i = 0; i < key->captured.count; ++i) tp_voice_of(e, key->channel, key->captured.notes[i])->refs--;
}
/* A source starts playing notes on channel ch: sounding (not silent, not being released), every note
 * from now, no strum delay or feel yet, no pressure sent. Every start goes through here, so a new
 * per-note field of tp_key is reset in one place. The caller numbers the start (order) where that
 * counts, strums it, and owns its notes. */
void tp_start(tp_key *q, const tp_notes *notes, uint8_t ch, uint8_t velocity, uint32_t now) {
    q->captured = *notes; q->channel = ch; q->admitted = true; q->silent = false; q->release_needed = false;
    q->velocity = velocity; q->admitted_at = now;
    memset(q->note_pressure, 0, sizeof q->note_pressure); memset(q->due, 0, sizeof q->due); memset(q->feel, 0, sizeof q->feel);
    for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) q->note_at[i] = now;
}
static void release_sources(tp_engine *e) {
    bool hold = tp_latching();
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (key->latched && !hold) { key->latched = false; key->release_needed = true; }
        if (!key->admitted || !key->release_needed) continue;
        if (!key->silent) {
            if (tp_mono_returns(e, k)) { tp_mono_drop(e); e->mono_hold = key->captured; e->mono_hold_ch = key->channel; }
            else tp_disown(e, key);
        }
        key->admitted = false; key->release_needed = false; key->latched = false; key->silent = false; key->hidden = false;
        key->inhibited = key->sensed && !key->readmit; key->readmit = false;
    }
}
static void admit_sources(tp_engine *e, uint32_t now) {
    bool arp = tc_state.arp != TC_ARP_OFF;
    /* The step screen (1.5.0): the right half is the lane's steps (silent); on a melody or drum lane
     * the left half plays what a step of it would (the value a touch gives). */
    bool steps = tc_state.ui == TC_UI_STEPS;
    const ln_lane *lane = &ln.lanes[tc_state.lane % LN_LANES];
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->sensed || key->admitted || key->inhibited) continue;
        if (steps && k >= 48u) { key->inhibited = true; continue; } /* quiet until lifted */
        bool value = steps && k < 48u && (lane->kind == LN_MELODY || lane->kind == LN_DRUMS);
        tp_notes notes;
        uint8_t ch = tc_channel_of(&tc_state, k);
        if (value) {
            ln_step s; memset(&s, 0, sizeof s);
            s.value = ln_pad_value(lane->kind, k);
            if (!tc_step_notes(e->layout, lane->kind, lane->channel, &s, &notes, &ch)) notes.count = 0u;
        } else if (!tc_pad(e->layout, k, &notes)) { tp_stop(e); return; }
        if (!value && k < 48u && ((tc_state.chord_mute >> k) & 1u)) notes.count = 0u; /* sets the live chord only */
        if (k >= 48u && e->layout.melody_layout != TP_MELODY_DRUMS) tc_harmonize(e->layout, tc_state.harmony, &notes); /* 1.6.0 */
        tp_notes core; /* voice leading (0.9.0): the next chord leads from this one */
        if (!value && k < 48u && notes.count && tc_pad_core(e->layout, k, &core)) tc_state.lead = core;
        bool silent = arp && k < 48u && !value;
        if (k >= 48u && e->layout.melody_layout == TP_MELODY_BASS) tp_bass_take(e, k, ch, &notes); /* 1.7.0 */
        if (!silent && tp_blocked(e, ch, &notes)) continue; /* atomic whole-pad deferral, never steal cleanup */
        tp_start(key, &notes, ch, tp_velocity(e, k, key->pressure, k >= 48u), now);
        key->silent = silent; key->order = ++e->admissions;
        if (!silent) tp_strum(e, key, tp_strums(k) && !value, now);
        if (!silent) tp_own(e, key);
        if (k >= 48u && e->layout.melody_layout != TP_MELODY_DRUMS) tp_echo_start(e, key, now); /* 1.6.0 */
    }
}
static uint8_t side_at(uint32_t melody) { return melody ? tc_state.at_melody : tc_state.at_chord; }
/* Channel pressure or a controller, one value per side: the strongest held pad (chords
 * include the arpeggiator's input pads). Both sides on one channel with the same
 * setting merge into one value. Nothing held: back to 0 however small the last value
 * (the deadband would otherwise leave a bend or controller slightly off); `rest` (a
 * session ending): only that, at once. */
static bool group_pressure(tp_engine *e, uint32_t now, bool rest, uint8_t *value, uint8_t *group, tk_kind *kind) {
    bool merged = (tc_state.chord_ch & 15u) == (tc_state.melody_ch & 15u) && side_at(0) == side_at(1);
    for (uint8_t g = 0; g < (merged ? 1u : 2u); ++g) {
        uint8_t mode = side_at(g), cc = tc_at_cc(mode);
        if (mode != TC_AT_CHANNEL && mode != TC_AT_BEND && !cc) continue;
        uint32_t peak = 0u;
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            if (!merged && (k < 48u) != (g == 0u)) continue;
            if (e->keys[k].admitted && e->keys[k].pressure > peak) peak = e->keys[k].pressure;
        }
        uint32_t sent = e->group_sent[g], diff = peak > sent ? peak - sent : sent - peak;
        bool to_rest = !peak && sent;
        if (rest ? to_rest : (diff >= e->config.deadband || to_rest) && now - e->group_at[g] >= e->config.pressure_ms) {
            *value = (uint8_t)peak; *group = g; *kind = cc ? TK_CC : mode == TC_AT_BEND ? TK_BEND : TK_CHANNEL;
            return true;
        }
    }
    return false;
}
typedef struct { tk_kind kind; uint8_t channel, pitch, key, value, group; } candidate_t;
/* Sustain per side (group 2 chords, 3 melody; one side when both share a channel): "on"
 * while active and the pedal is down; "off" when released, when draining, or before
 * following a channel change. */
static bool sustain(const tp_engine *e, candidate_t *c) {
    bool merged = (tc_state.chord_ch & 15u) == (tc_state.melody_ch & 15u);
    /* A centre button set to Sustain (1.0.0) sends CC64 while on, unless the pedal is down. */
    bool button = tc_sustain_button(&tc_state);
    uint8_t cc = tc_state.sustain || !button ? tc_pedal_cc(tc_state.pedal_mode) : 64u;
    for (uint8_t g = 0; g < 2u; ++g) {
        uint8_t ch = (uint8_t)((g ? tc_state.melody_ch : tc_state.chord_ch) & 15u);
        /* Target (0.11.0): one half only, unless both share a channel (then it is that channel). */
        bool side = merged || (g ? tc_state.pedal_target != TC_PEDAL_CHORDS : tc_state.pedal_target != TC_PEDAL_MELODY);
        bool want = e->state == TK_ACTIVE && (tc_state.sustain || button) && cc && !(merged && g) && side;
        if (e->sustain_on[g] && (!want || e->sustain_ch[g] != ch || e->sustain_cc[g] != cc)) {
            *c = (candidate_t){TK_CC, e->sustain_ch[g], e->sustain_cc[g], 0u, 0u, (uint8_t)(2u + g)}; return true;
        }
        if (!e->sustain_on[g] && want) { *c = (candidate_t){TK_CC, ch, cc, 0u, 127u, (uint8_t)(2u + g)}; return true; }
    }
    return false;
}
/* Program Change (1.7.0, Settings > MIDI > Chord prog, Melody prog): each half's program on its channel,
 * before its notes, at a session's start, TP_PROGRAM_LATE_MS later, at a DAW's Start and after
 * tc_state.program_again. Both halves on one channel:
 * the chord half's (the melody half's when the chords have none). */
static bool program(const tp_engine *e, candidate_t *c) {
    bool merged = (tc_state.chord_ch & 15u) == (tc_state.melody_ch & 15u);
    for (uint8_t g = 0; g < 2u; ++g) {
        uint8_t want = g ? tc_state.melody_prog : tc_state.chord_prog;
        uint8_t ch = (uint8_t)((g ? tc_state.melody_ch : tc_state.chord_ch) & 15u);
        if (!want || want >= TC_PROGRAMS || (merged && g && tc_state.chord_prog)) continue;
        if (e->program_sent[g] != want || e->program_ch[g] != ch) {
            *c = (candidate_t){TK_PROGRAM, ch, (uint8_t)(want - 1u), 0u, 0u, g}; return true;
        }
    }
    return false;
}
static bool group_candidate(tp_engine *e, uint32_t now, bool rest, candidate_t *c) {
    uint8_t value, group; tk_kind kind;
    if (!group_pressure(e, now, rest, &value, &group, &kind)) return false;
    uint8_t ch = (uint8_t)((group ? tc_state.melody_ch : tc_state.chord_ch) & 15u);
    uint8_t cc = tc_at_cc(side_at(group));
    *c = (candidate_t){kind, ch, kind == TK_CC ? cc : value, 0u, value, group}; return true;
}
static bool candidate(tp_engine *e, uint32_t now, candidate_t *c) {
    /* Real-time first: MIDI clock, then the transport's Start/Continue/Stop. A session that
     * ends with the transport running stops it, and the next one continues it (external
     * sequencers carry on where they paused instead of restarting from bar one). */
    if (e->clock_due) { *c = (candidate_t){TK_CLOCK, 0u, 0xF8u, 0u, 0u, 0u}; return true; }
    if (tc_state.transport != e->transport_sent && e->state == TK_ACTIVE) {
        uint8_t message = !tc_state.transport ? 0xFCu : e->transport_paused ? 0xFBu : 0xFAu;
        *c = (candidate_t){TK_CLOCK, 0u, message, 0u, 0u, 0u}; return true;
    }
    if (e->transport_sent && e->state != TK_ACTIVE) { *c = (candidate_t){TK_CLOCK, 0u, 0xFCu, 0u, 0u, 0u}; return true; }
    for (uint8_t ch = 0; ch < 16u; ++ch) {
        if (!e->exposed[ch]) continue;
        for (uint32_t p = 0; p < 128u; ++p) {
            if (e->voices[ch][p].exposed && !e->voices[ch][p].refs) {
                *c = (candidate_t){TK_OFF, ch, (uint8_t)p, 0u, 0u, 0u}; return true;
            }
        }
    }
    if (sustain(e, c)) return true;
    if (e->state != TK_ACTIVE) return group_candidate(e, now, true, c); /* draining: pressure back to rest */
    if (program(e, c)) return true;
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        tp_key *source = &e->keys[k];
        if (!source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            if (!tp_voice_of(e, source->channel, p)->exposed && now - source->admitted_at >= source->due[i]) {
                int32_t v = (int32_t)source->velocity + source->feel[i];
                uint8_t velocity = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
                *c = (candidate_t){TK_ON, source->channel, p, (uint8_t)k, velocity, (uint8_t)i}; return true;
            }
        }
    }
    if (group_candidate(e, now, false, c)) return true;
    /* Poly aftertouch: each note a pad has started that no other held pad shares on its
     * channel (unambiguous), from that pad's pressure; chord pads send it for every such note. */
    for (uint32_t n = 0; n < TS_KEYS; ++n) {
        uint32_t k = (e->cursor + n) % TS_KEYS; tp_key *source = &e->keys[k];
        if (side_at(k >= 48u) != TC_AT_POLY || !source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            const tp_voice *v = tp_voice_of(e, source->channel, p);
            uint32_t a = source->pressure, z = source->note_pressure[i], diff = a > z ? a-z : z-a;
            if (v->exposed && v->refs == 1u && diff >= e->config.deadband &&
                now - source->note_at[i] >= e->config.pressure_ms) {
                *c = (candidate_t){TK_PRESSURE, source->channel, p, (uint8_t)k, source->pressure, (uint8_t)i};
                return true;
            }
        }
    }
    return false;
}
void tp_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer) {
    if (e->state != TK_ACTIVE && e->state != TK_DRAINING) return;
    if (!ready) { tp_loss(e); return; }
    if (e->pending_id && now - e->pending_at >= 2000u) { tp_stop(e); return; }
    if (e->state == TK_ACTIVE && now - e->frame_at > 100u) tp_disable(e); /* scanner stale */
    /* Keep enough monotonic tokens for all exposed OFFs. Pressure/ON cannot
     * consume the final cleanup tokens. No counter wrap or reused receipts. */
    if (e->state == TK_ACTIVE && UINT32_MAX - e->issued <= tp_obligations(e)) tp_disable(e);
    if (!tc_state.transport) e->transport_paused = false;
    for (uint32_t g = 0; g < 2u; ++g) if (((uint32_t)tc_state.program_again >> g) & 1u) e->program_sent[g] = 0u; /* 1.7.0: send again */
    tc_state.program_again = 0u;
    /* The programs again a few seconds into the session (its first ones went out before a DAW opened the
     * port) and at a DAW's Start (its synths get them before the first notes). */
    if (!e->program_late && now - e->started >= TP_PROGRAM_LATE_MS) { e->program_late = true; e->program_sent[0] = e->program_sent[1] = 0u; }
    if (e->program_starts != tm.starts) { e->program_starts = tm.starts; e->program_sent[0] = e->program_sent[1] = 0u; }
    tp_tick_clock(e, now);
    if (e->state == TK_ACTIVE && (tc_state.relayout || tc_state.relayout_live)) {
        /* Re-strike held: strike held pads again (after a live chord change, the live area's). */
        bool all = tc_state.relayout;
        tc_state.relayout = false; tc_state.relayout_live = false;
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            tp_key *key = &e->keys[k];
            if (!all && !tp_live_pad(e->layout, k)) continue;
            if (key->admitted && key->sensed && !key->release_needed) { key->release_needed = true; key->readmit = true; }
        }
    }
    if (e->state == TK_ACTIVE) tp_strum_timers(e, now);
    if (tc_state.panic) { /* all notes off (1.0.0): everything the board plays ends; held pads stay quiet until lifted */
        tc_state.panic = false;
        tp_seq_release(e); e->seq_pending = false; /* following a DAW, it comes back at the next chord */
        for (uint32_t i = 0; i < LN_LANES; ++i) { tp_lane_release(e, i); e->lane_pending[i] = false; } /* and the next step */
        for (uint32_t i = 0; i < TP_ECHOES; ++i) { tp_echo_release(e, i); e->echo[i].left = 0u; } /* echoes to come too */
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            tp_key *key = &e->keys[k];
            if (key->admitted) { key->latched = false; key->release_needed = true; key->readmit = false; }
        }
        memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++;
    }
    release_sources(e);
    if (e->state == TK_ACTIVE) { if (tc_state.reshape) tp_arp_reshape(e); admit_sources(e, now); tp_arp_membership(e, now); }
    if (e->state == TK_STOPPED) return;
    tp_mono(e, now);
    tp_sequence(e, now);
    tp_lanes(e, now);
    tp_echoes(e, now);
    tp_arpeggiate(e, now);
    tp_pedal_note(e, now);
    tp_lock_notes(e, now);
    if (e->pending_id) return;
    candidate_t c;
    if (!candidate(e, now, &c)) {
        e->waiting = false;
        if (e->state == TK_DRAINING) e->state = TK_DONE;
        return;
    }
    if (c.kind == TK_ON && UINT32_MAX - e->issued <= tp_obligations(e) + 1u) {
        tp_disable(e); release_sources(e); tp_arp_release(e); return;
    }
    if (!e->waiting) { e->waiting = true; e->wait_at = now; }
    if (now - e->wait_at >= 5000u || e->issued == UINT32_MAX) { tp_stop(e); return; }
    uint8_t packet[4] = {(uint8_t)c.kind, (uint8_t)((uint32_t)c.kind << 4 | c.channel), c.pitch,
        c.kind == TK_OFF ? 0u : c.kind == TK_CHANNEL ? 0u : c.value};
    if (c.kind == TK_CLOCK) { packet[0] = 0x0Fu; packet[1] = c.pitch; packet[2] = 0u; packet[3] = 0u; }
    if (c.kind == TK_BEND) { /* pressure bends up: 0 = centre, 127 = full scale */
        uint32_t bend = 8192u + c.value * 8191u / 127u;
        packet[2] = (uint8_t)(bend & 127u); packet[3] = (uint8_t)(bend >> 7);
    }
    uint32_t token = e->issued + 1u;
    if (!writer(packet, token)) return;
    e->issued = token; e->pending_id = token; e->pending_at = now;
    e->pending_pitch = c.pitch; e->pending_key = c.key; e->pending_kind = c.kind;
    e->pending_channel = c.channel; e->pending_group = c.group; e->pending_pressure = c.value;
    e->accepted++; e->waiting = false;
    if (c.kind == TK_CLOCK && c.pitch == 0xFAu) { e->clock_ticks = 0u; e->clock_us = now * 1000u; } /* Start: bar one */
    if (c.kind == TK_ON || c.kind == TK_PRESSURE) e->cursor = (uint8_t)((c.key + 1u) % TS_KEYS);
    if (c.kind == TK_ON) { /* cleanup reserved at acceptance */
        e->voices[c.channel][c.pitch].exposed = true; e->exposed[c.channel]++;
    }
    if (c.kind == TK_CC && c.group >= 2u && c.value) { /* a pedal "on" too (1.7.0 fix: a session lost before its */
        uint32_t g = c.group - 2u;                     /* receipt left the receiver's pedal down) */
        e->sustain_on[g] = true; e->sustain_ch[g] = c.channel; e->sustain_cc[g] = c.pitch;
    }
    if (c.kind == TK_PROGRAM) { e->program_sent[c.group & 1u] = (uint8_t)(c.pitch + 1u); e->program_ch[c.group & 1u] = c.channel; }
}
static uint32_t session_at, arm_requests;
bool tc_arm(tp_engine *e, const ts_scan *s, uint32_t now, bool ready) {
    if (e->state != TK_IDLE || !ready || now - session_at < 500u) return false;
    session_at = now;
    arm_requests = arm_requests % 65535u + 1u;
    tp_measure_rest(e, s); tc_state.rescan = false; /* from the starting frame */
    return tp_arm(e, tc_pad_config(&tc_state), s, arm_requests, now);
}
/* Chord capture (1.4.0) from MIDI in, any channel: a chord is the most pitch classes held at once,
 * taken at the first release (so a rolled chord counts whole); after it, a new chord starts when
 * every note is up or a note outside it arrives (legato playing). */
static uint32_t capture_seen, capture_low;
static uint16_t capture_peak, capture_taken;
static void capture_midi(void) {
    if (!tc_state.capturing) { capture_peak = capture_taken = 0u; capture_seen = tm.notes_changed; return; }
    if (tm.notes_changed == capture_seen) return;
    capture_seen = tm.notes_changed;
    uint16_t held = 0u; uint32_t low = 128u;
    for (uint32_t p = 0; p < 128u; ++p) if (tm.notes[p]) { held |= (uint16_t)(1u << (p % 12u)); if (low == 128u) low = p; }
    if (capture_taken) {
        if (held && !(held & ~capture_taken)) return; /* still letting go of the last chord */
        capture_taken = 0u; capture_peak = 0u;
    }
    if (tb_count(held) >= tb_count(capture_peak) && held) { capture_peak = held; capture_low = low; return; }
    if (tb_count(capture_peak) >= 2u) { /* the first release: the chord as it was at its fullest */
        uint8_t root; uint16_t tones;
        if (cd_chord(capture_peak, (uint8_t)(capture_low % 12u), &root, &tones)) tc_capture_chord(&tc_state, root, tones);
        capture_taken = capture_peak;
    }
    capture_peak = held; capture_low = low;
    if (capture_taken && held && !(held & ~capture_taken)) capture_peak = 0u;
}
/* The live chord follows chords arriving over MIDI in (Settings > Layout > Live in). */
static uint32_t follow_seen;
static void follow_midi(void) {
    tm.follow_channel = tc_state.live_in;
    if (tm.follow_changed == follow_seen) return;
    follow_seen = tm.follow_changed;
    tc_live_chord chord;
    if (!tc_state.live_in || !tc_live_of_notes(tm.follow, &chord)) return;
    bool same = tc_state.live_from_midi && !memcmp(&chord, &tc_state.live_midi, sizeof chord);
    tc_state.live_midi = chord; tc_state.live_from_midi = true;
    if (!same && tc_state.live_update) tc_state.relayout_live = true;
}
bool tc_frame(tp_engine *e, const ts_scan *s, uint32_t now) {
    bool was_sensed[TS_KEYS];
    for(uint32_t k=0;k<TS_KEYS;k++) was_sensed[k]=e->keys[k].sensed;
    if(!tp_frame(e,s,now)) return false;
    if(tc_state.ui==TC_UI_STEPS) { /* the step screen (1.5.0): pads edit the lane (controls_steps.c tc_step_pad) */
        for(uint32_t k=0;k<TS_KEYS;k++) if(e->keys[k].sensed!=was_sensed[k])
            tc_step_pad(&tc_state,e->layout,k,e->keys[k].sensed,tp_velocity(e,k,e->keys[k].pressure,false));
        follow_midi();
        capture_midi();
        return true;
    }
    for(uint32_t k=0;k<48u;k++) if(e->keys[k].sensed && !was_sensed[k]) {
        if (tc_shift(&tc_state) && !tc_state.recording) { /* Shift + chord pad: the chord the Chords page edits, unplayed (recording: it records) */
            tc_state.last_chord=(uint8_t)k; tc_state.have_chord=true;
            tc_state.last_action=TC_PAD_CHOSEN; tc_state.event_sequence++;
            continue;
        }
        if (tc_pad_empty(e->layout, k)) continue; /* a chord set's empty column: silent, not a chord */
        if (tc_state.capturing) { /* chord capture (1.4.0): the chord as this pad plays it */
            uint8_t root = tc_pad_root(e->layout, k);
            uint32_t classes = tc_pad_classes(e->layout, k, tc_pad_edit(&tc_state, e->layout, k));
            tc_capture_chord(&tc_state, root, (uint16_t)tb_rotate_down(classes, root));
        }
        bool same = !tc_state.live_from_midi && tc_state.live_pad == k;
        tc_state.last_chord=(uint8_t)k; tc_state.have_chord=true;
        tc_state.live_pad=(uint8_t)k; tc_state.live_from_midi=false;
        if (!same && tc_state.live_update) tc_state.relayout_live = true;
        if (tc_state.recording) { /* recording a progression: each touch adds one unit of this chord */
            bool added = sq_tap(&sq, (uint8_t)k, tp_velocity(e, k, e->keys[k].pressure, false), tc_state.scale,
                                tc_pad_edit(&tc_state, e->layout, k));
            tc_state.last_action = added ? TC_SEQ_TAP : TC_SEQ_FULL; tc_state.event_sequence++;
        }
    }
    follow_midi();
    capture_midi();
    return true;
}
void tc_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer) {
    if(e->state==TK_ACTIVE || e->state==TK_DRAINING) session_at=now;
    else if((e->state==TK_DONE || e->state==TK_STOPPED) && now-session_at>=500u) {
        /* What the receiver may still hear carries into the fresh session, which ends it
         * first: the Note Offs and pedal "on"s a stopped session still owed (a lost USB
         * session, a missing receipt), pressure values not yet back at rest, and a transport
         * the old session stopped (or left running when it failed) continues, with the
         * progression where it was. */
        uint32_t owed[16][4]; uint8_t sustain_ch[2], sustain_cc[2], group_sent[2]; bool sustain_on[2];
        for(uint32_t ch=0;ch<16u;ch++) for(uint32_t w=0;w<4u;w++) {
            owed[ch][w]=0u;
            for(uint32_t b=0;b<32u;b++) if(e->voices[ch][32u*w+b].exposed) owed[ch][w]|=1u<<b;
        }
        memcpy(sustain_ch,e->sustain_ch,2u); memcpy(sustain_cc,e->sustain_cc,2u);
        memcpy(group_sent,e->group_sent,2u); memcpy(sustain_on,e->sustain_on,sizeof sustain_on);
        bool paused=e->transport_paused || (e->state==TK_STOPPED && e->transport_sent);
        /* A playing progression and the board's clock carry on from where they paused. */
        uint32_t clock_ticks=e->clock_ticks, seq_origin=e->seq_origin, seq_pos=e->seq_pos, seq_step=e->seq_step;
        uint32_t seq_starts=e->seq_starts, seq_changes=e->seq_changes; uint8_t seq_entry=e->seq_entry;
        bool seq_running=e->seq_running, seq_clocked=e->seq_clocked, seq_daw=e->seq_daw;
        uint32_t lane_origin=e->lane_origin, lane_pos=e->lane_pos;
        bool lane_running=e->lane_running, lane_clocked=e->lane_clocked, lane_daw=e->lane_daw;
        memset(e,0,sizeof *e); /* fresh TK_IDLE session; tp_arm takes the current layout */
        e->clock_ticks=clock_ticks; e->seq_origin=seq_origin; e->seq_pos=seq_pos; e->seq_step=seq_step;
        e->seq_starts=seq_starts; e->seq_changes=seq_changes; e->seq_entry=seq_entry;
        e->seq_running=seq_running; e->seq_clocked=seq_clocked; e->seq_daw=seq_daw; e->seq_pending=seq_running;
        e->lane_origin=lane_origin; e->lane_pos=lane_pos; e->lane_running=lane_running;
        e->lane_clocked=lane_clocked; e->lane_daw=lane_daw;
        for(uint32_t i=0;i<LN_LANES;i++) { e->lane_count[i]=UINT32_MAX; e->lane_step[i]=LN_NONE; } /* the step now sounds again */
        for(uint32_t ch=0;ch<16u;ch++) for(uint32_t p=0;p<128u;p++) if((owed[ch][p/32u]>>(p%32u))&1u) {
            e->voices[ch][p].exposed=true; e->exposed[ch]++;
        }
        memcpy(e->sustain_ch,sustain_ch,2u); memcpy(e->sustain_cc,sustain_cc,2u);
        memcpy(e->group_sent,group_sent,2u); memcpy(e->sustain_on,sustain_on,sizeof sustain_on);
        e->transport_paused=paused;
        session_at=now;
    }
    tp_step(e,now,ready,writer);
}
