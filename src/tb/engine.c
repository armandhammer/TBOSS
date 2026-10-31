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
    bool level = e->pending_kind == TK_CHANNEL || e->pending_kind == TK_CC || e->pending_kind == TK_BEND;
    if (level && e->pending_group >= TP_GROUP_MPE) { /* MPE (1.8.0): a member channel's own */
        uint8_t ch = (uint8_t)((e->pending_group - TP_GROUP_MPE) & 15u);
        if (e->pending_kind == TK_CHANNEL) { e->mpe_z[ch] = e->pending_pressure; e->mpe_at[ch] = now; }
        if (e->pending_kind == TK_CC) { e->mpe_cc[ch] = e->pending_pressure; e->mpe_ccn[ch] = e->pending_pitch; e->mpe_cc_at[ch] = now; }
        if (e->pending_kind == TK_BEND) { e->mpe_bend[ch] = (int16_t)((int32_t)(e->pending_pitch | e->pending_pressure << 7) - 8192); e->mpe_bend_at[ch] = now; }
    } else if (e->pending_kind == TK_CC && e->pending_group >= TP_GROUP_MCM) { /* an MPE configuration message's step */
        uint32_t z = (e->pending_group - TP_GROUP_MCM) & 1u;
        if (++e->mpe_step[z] >= 5u) { e->mpe_step[z] = 0u; e->mpe_sent[z] = e->mpe_count[z]; }
    } else if (e->pending_kind == TK_CC && e->pending_group >= 2u) { /* sustain (CC64) */
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
    q->velocity = velocity; q->admitted_at = now; q->glide = 0u; q->glide_offset = 0;
    q->glide_from = q->glide_to = 0; q->glide_at = now;
    memset(q->note_pressure, 0, sizeof q->note_pressure); memset(q->due, 0, sizeof q->due); memset(q->feel, 0, sizeof q->feel);
    for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) q->note_at[i] = now;
}
/* MPE (1.8.0): the Lower Zone is channels 0..7 (master 0, members 1..7), the Upper Zone 8..15 (members
 * 8..14, master 15). */
int tp_mpe_zone(uint8_t ch) {
    if ((ch & 15u) < 8u) return tc_mpe_half(&tc_state, 1u) ? 0 : -1;
    return tc_mpe_half(&tc_state, 0u) ? 1 : -1;
}
static bool mpe_member(uint8_t ch) { return ch >= 1u && ch <= 14u; }
static uint32_t sources_on(const tp_engine *e, uint8_t ch) {
    uint32_t n = 0;
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        const tp_key *q = &e->keys[k];
        if (q->admitted && !q->silent && q->captured.count && q->channel == ch) n++;
    }
    return n;
}
/* Fewest sources playing on it, then no Note Off owed on these notes, then none owed at all (a note
 * that just ended may still ring out there), then the longest since a source started on it. When every
 * member channel is taken, pads share one (its pressure is then the strongest of theirs). */
uint8_t tp_mpe_channel(tp_engine *e, uint8_t ch, const tp_notes *notes) {
    int zone = tp_mpe_zone(ch);
    if (zone < 0) return ch;
    uint8_t first = zone ? 8u : 1u, pick = first;
    uint32_t pick_on = UINT32_MAX, pick_used = 0u;
    bool pick_blocked = true, pick_owed = true;
    for (uint8_t m = first; m < first + TP_MPE_MEMBERS; ++m) {
        uint32_t on = sources_on(e, m), used = e->mpe_used[m];
        bool blocked = tp_blocked(e, m, notes), owed = e->exposed[m] != 0u;
        bool better = on != pick_on ? on < pick_on : blocked != pick_blocked ? !blocked :
                      owed != pick_owed ? !owed : used < pick_used;
        if (better) { pick = m; pick_on = on; pick_used = used; pick_blocked = blocked; pick_owed = owed; }
    }
    e->mpe_used[pick] = ++e->mpe_uses;
    return pick;
}
/* Glide (1.8.0, Settings > MIDI > Glide): melody pad k touched beside a held melody pad (the next column
 * either side, the same row) glides on it instead of striking. It owns no notes; the held pad's channel
 * bends toward k's note as k is pressed harder than it (mpe_wants), and k takes the note over when the
 * held pad is lifted (release_sources), so a finger rocked along a row slides the note. One pad glides on
 * a held pad at a time; a pad gliding is never glided on. */
/* Semitones from the channel's note as bend units at the MPE range. */
static int32_t glide_units(int32_t semitones) { return semitones * 8192 / TP_MPE_BEND_RANGE; }
/* Glide Time (1.8.5): where pad q's channel is on its way from glide_from to glide_to, the Glide time long;
 * glide_retarget sets off from there toward a new bend. */
static int32_t glide_now(const tp_key *q, uint32_t now) {
    uint32_t span = tc_glide_ms[tc_state.glide_time % TC_GLIDE_TIMES], gone = now - q->glide_at;
    if (gone >= span) return q->glide_to;
    return q->glide_from + (q->glide_to - q->glide_from) * (int32_t)gone / (int32_t)span;
}
static void glide_retarget(tp_key *q, int32_t to, uint32_t now) {
    q->glide_from = (int16_t)glide_now(q, now); q->glide_to = (int16_t)to; q->glide_at = now;
}
static tp_key *glider(tp_engine *e, uint32_t k) {
    for (uint32_t j = 48u; j < TS_KEYS; ++j) if (e->keys[j].admitted && e->keys[j].glide == k + 1u) return &e->keys[j];
    return 0;
}
static bool glide_onto(tp_engine *e, uint32_t k, const tp_notes *notes, uint8_t own, uint32_t now) {
    if (!tc_glides(&tc_state) || !notes->count) return false;
    for (int32_t d = -4; d <= 4; d += 8) {
        int32_t j = (int32_t)k + d;
        if (j < 48 || j >= (int32_t)TS_KEYS) continue;
        tp_key *held = &e->keys[j];
        if (!held->admitted || held->silent || held->release_needed || held->glide || !held->captured.count ||
            !held->sensed || tp_mpe_zone(held->channel) != 0 || !mpe_member(held->channel) || glider(e, (uint32_t)j)) continue;
        int32_t offset = (int32_t)own - ((int32_t)held->glide_pitch - held->glide_offset);
        if (offset < -TP_MPE_BEND_RANGE || offset > TP_MPE_BEND_RANGE) continue;
        tp_key *key = &e->keys[k];
        tp_notes none; memset(&none, 0, sizeof none);
        tp_start(key, &none, held->channel, held->velocity, now);
        key->glide = (uint8_t)(j + 1); key->glide_pitch = own; key->glide_offset = (int8_t)offset;
        key->order = 0u; /* never the newest under Mono */
        glide_retarget(held, glide_units(offset), now); /* Time: off toward the new pad's note */
        return true;
    }
    return false;
}
/* The held pad lifted: the pad gliding on it plays its notes on (bent to its own). */
static void glide_over(tp_key *from, tp_key *to) {
    to->captured = from->captured; to->channel = from->channel; to->velocity = from->velocity;
    to->order = from->order; to->admitted_at = from->admitted_at; to->glide = 0u;
    memcpy(to->note_at, from->note_at, sizeof to->note_at); memcpy(to->due, from->due, sizeof to->due);
    memcpy(to->feel, from->feel, sizeof to->feel); memcpy(to->note_pressure, from->note_pressure, sizeof to->note_pressure);
    to->glide_from = from->glide_from; to->glide_to = from->glide_to; to->glide_at = from->glide_at; /* Time: on its way */
    from->captured.count = 0u; /* its references are the other pad's now */
}
static void release_sources(tp_engine *e, uint32_t now) {
    bool hold = tp_latching();
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (key->latched && !hold) { key->latched = false; key->release_needed = true; }
        if (!key->admitted || !key->release_needed) continue;
        tp_key *next = key->silent || key->glide ? 0 : glider(e, k);
        bool over = next && next->sensed && !next->release_needed; /* else lifted together: it ends too */
        if (over) glide_over(key, next);
        if (key->glide && e->keys[key->glide - 1u].admitted) { /* Time (1.8.5): the held note's way back */
            tp_key *held = &e->keys[key->glide - 1u];
            glide_retarget(held, glide_units(held->glide_offset), now);
        }
        if (key->glide || over) key->glide = 0u; /* a gliding pad owns nothing; nor does one glided over */
        else if (!key->silent) {
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
     * the left half plays what a step of it would (the value a touch gives); 1.9.4: Step pads Silent, no pad
     * sounds there (each still chooses what the held steps play). */
    bool steps = tc_state.ui == TC_UI_STEPS;
    const ln_lane *lane = &ln.lanes[tc_state.lane % LN_LANES];
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->sensed || key->admitted || key->inhibited) continue;
        if (steps && (k >= 48u || tc_state.step_silent)) { key->inhibited = true; continue; } /* quiet until lifted */
        bool value = steps && k < 48u && (lane->kind == LN_MELODY || lane->kind == LN_DRUMS);
        tp_notes notes;
        uint8_t ch = tc_channel_of(&tc_state, k);
        if (value) {
            ln_step s; memset(&s, 0, sizeof s);
            s.value = ln_pad_value(lane->kind, k);
            if (!tc_step_notes(e->layout, lane->kind, lane->channel, &s, &notes, &ch)) notes.count = 0u;
        } else if (!tc_pad(e->layout, k, &notes)) { tp_stop(e); return; }
        if (!value && k < 48u && ((tc_state.chord_mute >> k) & 1u)) notes.count = 0u; /* sets the live chord only */
        uint8_t own = notes.count ? notes.notes[0] : 0u; /* the pad's own note (Glide) */
        if (k >= 48u && e->layout.melody_layout != TP_MELODY_DRUMS) tc_harmonize(e->layout, tc_state.harmony, &notes); /* 1.6.0 */
        tp_notes core; /* voice leading (0.9.0): the next chord leads from this one */
        if (!value && k < 48u && notes.count && tc_pad_core(e->layout, k, &core)) tc_state.lead = core;
        bool silent = arp && k < 48u && !value;
        if (k >= 48u && e->layout.melody_layout == TP_MELODY_BASS) tp_bass_take(e, k, ch, &notes); /* 1.7.0 */
        if (k >= 48u && !value && glide_onto(e, k, &notes, own, now)) continue; /* 1.8.0 */
        if (!silent) ch = tp_mpe_channel(e, ch, &notes); /* MPE (1.8.0): a channel of its own */
        if (!silent && tp_blocked(e, ch, &notes)) continue; /* atomic whole-pad deferral, never steal cleanup */
        tp_start(key, &notes, ch, tp_velocity(e, k, k >= 48u), now);
        key->glide_pitch = own;
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
 * session ending): only that, at once. A half playing MPE (1.8.0) sends its pressure per pad instead
 * (mpe_candidate): its value here goes back to rest. */
static bool group_pressure(tp_engine *e, uint32_t now, bool rest, uint8_t *value, uint8_t *group, tk_kind *kind) {
    bool merged = tc_half_ch(&tc_state, 0u) == tc_half_ch(&tc_state, 1u) && side_at(0) == side_at(1);
    for (uint8_t g = 0; g < (merged ? 1u : 2u); ++g) {
        uint8_t mode = side_at(g), cc = tc_at_cc(mode);
        if (mode != TC_AT_CHANNEL && mode != TC_AT_BEND && !cc) continue;
        uint32_t peak = 0u;
        for (uint32_t k = 0; k < TS_KEYS && !tc_mpe_half(&tc_state, g); ++k) {
            if (!merged && (k < 48u) != (g == 0u)) continue;
            if (e->keys[k].admitted && e->keys[k].at > peak) peak = e->keys[k].at; /* 1.9.0: through its curve */
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
/* An MPE bend (group TP_GROUP_MPE..) carries its 14 bits as the message's own: pitch the low 7, value the high. */
typedef struct { tk_kind kind; uint8_t channel, pitch, key, value, group; } candidate_t;
/* Sustain per side (group 2 chords, 3 melody; one side when both share a channel): "on"
 * while active and the pedal is down; "off" when released, when draining, or before
 * following a channel change. */
static bool sustain(const tp_engine *e, candidate_t *c) {
    bool merged = tc_half_ch(&tc_state, 0u) == tc_half_ch(&tc_state, 1u);
    /* A centre button set to Sustain (1.0.0) sends CC64 while on, unless the pedal is down. */
    bool button = tc_sustain_button(&tc_state);
    uint8_t cc = tc_state.sustain || !button ? tc_pedal_cc(tc_state.pedal_mode) : 64u;
    for (uint8_t g = 0; g < 2u; ++g) {
        uint8_t ch = tc_half_ch(&tc_state, g); /* MPE: the zone's master channel, so all its pads */
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
    bool merged = tc_half_ch(&tc_state, 0u) == tc_half_ch(&tc_state, 1u);
    for (uint8_t g = 0; g < 2u; ++g) {
        uint8_t want = g ? tc_state.melody_prog : tc_state.chord_prog;
        uint8_t ch = tc_half_ch(&tc_state, g);
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
    uint8_t ch = tc_half_ch(&tc_state, group);
    uint8_t cc = tc_at_cc(side_at(group));
    *c = (candidate_t){kind, ch, kind == TK_CC ? cc : value, 0u, value, group}; return true;
}
/* MPE (1.8.0): what each member channel should carry now. Its pressure: the strongest of the sources
 * playing on it (and of a pad gliding on one of them), as channel pressure unless the half's pressure
 * is Off, and as that controller too when it is one. Its bend: a glide's, between the two pads' notes
 * by which is pressed harder (a pad that glided over: its own note), plus up to 2 semitones from the
 * pressure when the half's pressure is Bend. A channel outside a zone playing MPE: at rest. */
typedef struct { uint8_t z, cc, number; int16_t bend; } mpe_value;
static void mpe_wants(const tp_engine *e, uint32_t now, mpe_value want[16]) {
    memset(want, 0, 16u * sizeof *want);
    uint8_t pad[16]; memset(pad, 0xFF, sizeof pad);
    int32_t bend[16]; memset(bend, 0, sizeof bend);
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        const tp_key *q = &e->keys[k];
        if (!q->admitted || q->silent || !q->captured.count) continue;
        uint8_t ch = q->channel & 15u;
        if (q->at > want[ch].z) want[ch].z = q->at;
        if (pad[ch] == 0xFFu && k < TS_KEYS) {
            pad[ch] = (uint8_t)k;
            bend[ch] = tc_state.glide == TC_GLIDE_TIME ? glide_now(q, now) : glide_units(q->glide_offset);
        }
    }
    for (uint32_t k = 48u; k < TS_KEYS; ++k) {
        const tp_key *b = &e->keys[k];
        if (!b->admitted || !b->glide) continue;
        const tp_key *a = &e->keys[b->glide - 1u];
        uint8_t ch = a->channel & 15u;
        if (!a->admitted || a->silent || !a->captured.count) continue;
        if (b->at > want[ch].z) want[ch].z = b->at;
        if (tc_state.glide == TC_GLIDE_ROLL) {
            /* 1.8.4: a crossfade on the difference (1.8.0 weighed the two pressures, so the last of a lifting
             * finger's pressure kept the note short of the new pad's): halfway when even, all the way once the
             * one pad is TP_GLIDE_BAND harder than the other. */
            int32_t t = TP_GLIDE_BAND + (int32_t)b->pressure - (int32_t)a->pressure;
            t = t < 0 ? 0 : t > 2 * TP_GLIDE_BAND ? 2 * TP_GLIDE_BAND : t;
            bend[ch] = (a->glide_offset * (2 * TP_GLIDE_BAND - t) + b->glide_offset * t) * 8192 / (2 * TP_GLIDE_BAND * TP_MPE_BEND_RANGE);
        } else if (tc_state.glide == TC_GLIDE_PAD) {
            /* 1.8.5: as far as the new pad is pressed, from a little past its own Trigger (a resting finger
             * doesn't bend) to full pressure, all the way. */
            int32_t floor = (int32_t)tp_trigger_pressure(e, k) + TP_GLIDE_FLOOR;
            if (floor > 120) floor = 120;
            int32_t t = (int32_t)b->pressure - floor, span = 127 - floor;
            t = t < 0 ? 0 : t > span ? span : t;
            bend[ch] = (a->glide_offset * (span - t) + b->glide_offset * t) * 8192 / (span * TP_MPE_BEND_RANGE);
        } /* Time: the held pad's own way (above) */
    }
    for (uint8_t ch = 1u; ch <= 14u; ++ch) {
        int zone = tp_mpe_zone(ch);
        if (zone < 0) { memset(&want[ch], 0, sizeof want[ch]); continue; }
        uint8_t mode = side_at(zone == 0), z = want[ch].z;
        if (mode == TC_AT_BEND) bend[ch] += (int32_t)z * (8192 * 2 / TP_MPE_BEND_RANGE) / 127;
        want[ch].bend = (int16_t)(bend[ch] < -8192 ? -8192 : bend[ch] > 8191 ? 8191 : bend[ch]);
        want[ch].number = tc_at_cc(mode); want[ch].cc = want[ch].number ? z : 0u;
        if (mode == TC_AT_OFF) want[ch].z = 0u;
    }
}
enum { MPE_SEND, MPE_REST, MPE_STALE };
/* One message toward what channel ch should carry: the bend first (TP_MPE_BEND_MS apart, by
 * TP_MPE_BEND_DEADBAND), then pressure and the controller (as group_pressure). MPE_REST (a session
 * ending): back to rest at once. MPE_STALE (before a Note On on a channel no note sounds on): only a value
 * a channel's last notes left there, set to the new note's now, so the note starts clean. */
static bool mpe_send(const tp_engine *e, uint32_t now, int how, uint8_t ch, const mpe_value *want, candidate_t *c) {
    uint8_t g = (uint8_t)(TP_GROUP_MPE + ch);
    int32_t sent = e->mpe_bend[ch], bd = want->bend > sent ? want->bend - sent : sent - want->bend;
    bool at_once = how != MPE_SEND;
    if (want->bend != sent && (how != MPE_STALE || sent) &&
        (at_once || (now - e->mpe_bend_at[ch] >= TP_MPE_BEND_MS && (bd >= TP_MPE_BEND_DEADBAND || !want->bend || now - e->mpe_bend_at[ch] >= TP_MPE_BEND_SETTLE_MS)))) {
        uint32_t bend = (uint32_t)(8192 + want->bend);
        *c = (candidate_t){TK_BEND, ch, (uint8_t)(bend & 127u), 0u, (uint8_t)(bend >> 7), g}; return true;
    }
    bool due = at_once || now - e->mpe_at[ch] >= e->config.pressure_ms;
    uint32_t z = e->mpe_z[ch], zd = want->z > z ? want->z - z : z - want->z;
    if (want->z != z && (how != MPE_STALE || z) && due && (at_once || zd >= e->config.deadband || !want->z)) {
        *c = (candidate_t){TK_CHANNEL, ch, want->z, 0u, want->z, g}; return true;
    }
    due = at_once || now - e->mpe_cc_at[ch] >= e->config.pressure_ms;
    if (e->mpe_cc[ch] && want->number != e->mpe_ccn[ch] && due) { /* the controller changed or went: the old one to 0 */
        *c = (candidate_t){TK_CC, ch, e->mpe_ccn[ch], 0u, 0u, g}; return true;
    }
    uint32_t v = e->mpe_cc[ch], vd = want->cc > v ? want->cc - v : v - want->cc;
    if (want->number && want->cc != v && (how != MPE_STALE || v) && due && (at_once || vd >= e->config.deadband || !want->cc)) {
        *c = (candidate_t){TK_CC, ch, want->number, 0u, want->cc, g}; return true;
    }
    return false;
}
static bool mpe_candidate(tp_engine *e, uint32_t now, bool rest, candidate_t *c) {
    mpe_value want[16];
    if (rest) memset(want, 0, sizeof want); else mpe_wants(e, now, want);
    for (uint32_t n = 0; n < 14u; ++n) {
        uint8_t ch = (uint8_t)(1u + (e->mpe_cursor + n) % 14u);
        if (mpe_send(e, now, rest ? MPE_REST : MPE_SEND, ch, &want[ch], c)) { e->mpe_cursor = (uint8_t)(ch % 14u); return true; }
    }
    return false;
}
static bool mpe_stale(const tp_engine *e, uint32_t now, uint8_t ch, candidate_t *c) {
    if (!mpe_member(ch) || e->exposed[ch] || (!e->mpe_z[ch] && !e->mpe_cc[ch] && !e->mpe_bend[ch])) return false;
    mpe_value want[16]; mpe_wants(e, now, want);
    return mpe_send(e, now, MPE_STALE, ch, &want[ch], c);
}
/* The MPE Configuration Message (RPN 6) on each zone's master channel: its member count while that half
 * plays MPE, 0 once it stops; at a session's start, TP_PROGRAM_LATE_MS later and at a DAW's Start (with
 * the programs). Five controllers: RPN 6 chosen, the count, then RPN null. */
static bool mpe_config(tp_engine *e, candidate_t *c) {
    static const uint8_t number[5] = {101u, 100u, 6u, 101u, 100u};
    for (uint8_t z = 0; z < 2u; ++z) {
        uint8_t want = tc_mpe_half(&tc_state, z == 0u) ? TP_MPE_MEMBERS : 0u, step = e->mpe_step[z];
        if (!step && want == e->mpe_sent[z]) continue;
        if (!step) e->mpe_count[z] = want;
        uint8_t value = step == 1u ? 6u : step == 2u ? e->mpe_count[z] : step >= 3u ? 127u : 0u;
        *c = (candidate_t){TK_CC, z ? 15u : 0u, number[step % 5u], 0u, value, (uint8_t)(TP_GROUP_MCM + z)};
        return true;
    }
    return false;
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
    if (e->state != TK_ACTIVE) return group_candidate(e, now, true, c) || mpe_candidate(e, now, true, c); /* draining: pressure back to rest */
    if (mpe_config(e, c)) return true;
    if (program(e, c)) return true;
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        tp_key *source = &e->keys[k];
        if (!source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            if (!tp_voice_of(e, source->channel, p)->exposed && now - source->admitted_at >= source->due[i]) {
                if (mpe_stale(e, now, source->channel, c)) return true; /* MPE: what its last notes left first */
                int32_t v = (int32_t)source->velocity + source->feel[i];
                uint8_t velocity = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
                *c = (candidate_t){TK_ON, source->channel, p, (uint8_t)k, velocity, (uint8_t)i}; return true;
            }
        }
    }
    if (group_candidate(e, now, false, c)) return true;
    if (mpe_candidate(e, now, false, c)) return true;
    /* Poly aftertouch: each note a pad has started that no other held pad shares on its
     * channel (unambiguous), from that pad's pressure; chord pads send it for every such note.
     * A half playing MPE sends channel pressure per pad instead (mpe_candidate). */
    for (uint32_t n = 0; n < TS_KEYS; ++n) {
        uint32_t k = (e->cursor + n) % TS_KEYS; tp_key *source = &e->keys[k];
        if (side_at(k >= 48u) != TC_AT_POLY || tc_mpe_half(&tc_state, k >= 48u) || !source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            const tp_voice *v = tp_voice_of(e, source->channel, p);
            uint32_t a = source->at, z = source->note_pressure[i], diff = a > z ? a-z : z-a;
            if (v->exposed && v->refs == 1u && diff >= e->config.deadband &&
                now - source->note_at[i] >= e->config.pressure_ms) {
                *c = (candidate_t){TK_PRESSURE, source->channel, p, (uint8_t)k, source->at, (uint8_t)i};
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
    bool again = false; /* MPE's configuration messages (1.8.0) go again with them */
    if (!e->program_late && now - e->started >= TP_PROGRAM_LATE_MS) { e->program_late = true; e->program_sent[0] = e->program_sent[1] = 0u; again = true; }
    if (e->program_starts != tm.starts) { e->program_starts = tm.starts; e->program_sent[0] = e->program_sent[1] = 0u; again = true; }
    for (uint32_t z = 0; again && z < 2u; ++z) if (tc_mpe_half(&tc_state, z == 0u)) e->mpe_sent[z] = 0u;
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
    release_sources(e, now);
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
        tp_disable(e); release_sources(e, now); tp_arp_release(e); return;
    }
    if (!e->waiting) { e->waiting = true; e->wait_at = now; }
    if (now - e->wait_at >= 5000u || e->issued == UINT32_MAX) { tp_stop(e); return; }
    uint8_t packet[4] = {(uint8_t)c.kind, (uint8_t)((uint32_t)c.kind << 4 | c.channel), c.pitch,
        c.kind == TK_OFF ? 0u : c.kind == TK_CHANNEL ? 0u : c.value};
    if (c.kind == TK_CLOCK) { packet[0] = 0x0Fu; packet[1] = c.pitch; packet[2] = 0u; packet[3] = 0u; }
    if (c.kind == TK_BEND && c.group < TP_GROUP_MPE) { /* pressure bends up: 0 = centre, 127 = full scale */
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
    if (c.kind == TK_CC && (c.group == 2u || c.group == 3u) && c.value) { /* a pedal "on" too (1.7.0 fix: a session lost before its */
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
    /* 2.0.0: a touch keeps the board awake (Sleep after) and wakes a dark one, playing as usual; shut down,
     * the pads do nothing. */
    for(uint32_t k=0;k<TS_KEYS;k++) if(e->keys[k].sensed && !was_sensed[k]) {
        tc_state.touched_at=now;
        if(tc_state.dark==TC_DARK) { tc_state.dark=TC_AWAKE; tc_state.last_action=TC_AWOKE; tc_state.event_sequence++; }
    }
    if(tc_state.dark==TC_SHUT_DOWN) { follow_midi(); capture_midi(); return true; }
    if(tc_state.ui==TC_UI_STEPS) { /* the step screen (1.5.0): pads edit the lane (controls_steps.c tc_step_pad) */
        for(uint32_t k=0;k<TS_KEYS;k++) if(e->keys[k].sensed!=was_sensed[k])
            tc_step_pad(&tc_state,e->layout,k,e->keys[k].sensed,tp_velocity(e,k,false));
        follow_midi();
        capture_midi();
        return true;
    }
    for(uint32_t k=0;k<48u;k++) if(e->keys[k].sensed && !was_sensed[k]) {
        if (tc_shift(&tc_state) && !tc_state.recording) { /* Shift + chord pad: the chord the Chords page edits, unplayed (recording: it records) */
            tc_state.last_chord=(uint8_t)k; tc_state.have_chord=true;
            tc_state.last_action=TC_PAD_CHOSEN; tc_state.event_sequence++;
            tc_state.shift_once=false; /* 2.0.0: U3 tapped: Shift for this pad only */
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
            bool added = sq_tap(&sq, (uint8_t)k, tp_velocity(e, k, false), tc_state.scale,
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
        uint8_t seq_link=e->seq_link, seq_slot=e->seq_slot, seq_plays=e->seq_plays; /* 2.0.0 */
        uint32_t seq_chain_changes=e->seq_chain_changes; bool seq_chained=e->seq_chained;
        uint32_t lane_origin=e->lane_origin, lane_pos=e->lane_pos;
        bool lane_running=e->lane_running, lane_clocked=e->lane_clocked, lane_daw=e->lane_daw;
        uint8_t mpe_z[16], mpe_cc[16], mpe_ccn[16]; int16_t mpe_bend[16]; /* MPE (1.8.0): what each channel was left at */
        memcpy(mpe_z,e->mpe_z,16u); memcpy(mpe_cc,e->mpe_cc,16u); memcpy(mpe_ccn,e->mpe_ccn,16u); memcpy(mpe_bend,e->mpe_bend,sizeof mpe_bend);
        memset(e,0,sizeof *e); /* fresh TK_IDLE session; tp_arm takes the current layout */
        memcpy(e->mpe_z,mpe_z,16u); memcpy(e->mpe_cc,mpe_cc,16u); memcpy(e->mpe_ccn,mpe_ccn,16u); memcpy(e->mpe_bend,mpe_bend,sizeof mpe_bend);
        e->clock_ticks=clock_ticks; e->seq_origin=seq_origin; e->seq_pos=seq_pos; e->seq_step=seq_step;
        e->seq_starts=seq_starts; e->seq_changes=seq_changes; e->seq_entry=seq_entry;
        e->seq_running=seq_running; e->seq_clocked=seq_clocked; e->seq_daw=seq_daw; e->seq_pending=seq_running;
        e->seq_link=seq_link; e->seq_slot=seq_slot; e->seq_plays=seq_plays; e->seq_chain_changes=seq_chain_changes;
        e->seq_chained=seq_chained;
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
