/* SPDX-License-Identifier: GPL-3.0-or-later - bounded source ownership, no heap/event bus.
 * Standalone: arms itself once USB is up, no host watchdog.
 * Ownership rule (unchanged): a source (pad or the arpeggiator) holds references
 * on (channel, pitch) voices; a voice's Note On goes out once while referenced and
 * its Note Off once all references are gone. One packet is in flight at a time and
 * every Note On reserves its Note Off before it is sent. */
#include "engine.h"
#include "controls.h"
#include "midi_in.h"
#include "sequence.h"
#include "chord_detect.h"
#include <string.h>
bool tk_config_ok(tk_config c) {
    if (c.press > 4095u || c.release > 4095u || c.full_scale > 4095u ||
        !c.deadband || c.deadband > 127u || c.pressure_ms < 20u || c.pressure_ms > 1000u) return false;
    if (c.polarity == 1u) return c.release < c.press && c.press < c.full_scale;
    if (c.polarity == 2u) return c.full_scale < c.press && c.press < c.release;
    return false;
}
static void stop(tp_engine *e) {
    e->errors++; e->state = TK_STOPPED;
    if (e->accepted) e->uncertain = true;
}
uint32_t tp_obligations(const tp_engine *e) {
    uint32_t n = 0;
    for (uint32_t ch = 0; ch < 16u; ++ch) n += e->exposed[ch];
    return n + (e->sustain_on[0] ? 1u : 0u) + (e->sustain_on[1] ? 1u : 0u);
}
static bool released(tk_config c, uint32_t v) {
    return c.polarity == 1u ? v <= c.release : v >= c.release;
}
static uint8_t pressure(tk_config c, uint32_t v) {
    uint32_t delta = c.polarity == 1u ? (v > c.release ? v - c.release : 0u) :
        (v < c.release ? c.release - v : 0u);
    uint32_t span = c.polarity == 1u ? c.full_scale - c.release : c.release - c.full_scale;
    return (uint8_t)(delta >= span ? 127u : delta * 127u / span);
}
static uint8_t channel_of(uint32_t k) { return tc_channel_of(&tc_state, k); }
tk_config tp_key_config(const tp_engine *e, uint32_t k) {
    uint32_t p = k < TS_KEYS ? tc_physical(&tc_state, k) : k; /* rests belong to the physical pad */
    return p < TS_KEYS && e->rest[p] ? tc_pad_raise(e->config, e->rest[p]) : e->config;
}
bool tp_rest_range(const tp_engine *e, uint32_t *lo, uint32_t *hi) {
    uint32_t a = UINT32_MAX, b = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (e->rest[k]) { if (e->rest[k] < a) a = e->rest[k]; if (e->rest[k] > b) b = e->rest[k]; }
    if (a == UINT32_MAX) return false;
    *lo = a; *hi = b; return true;
}
/* A resting pad's level follows its reading (1.5.0) every TC_PAD_REST_MS: down within a few steps, up
 * 1/128 of the way, at least 1 (about 2 s; a press that stays below Trigger for that long counts as rest). */
static void follow_rest(tp_engine *e, uint32_t p, uint32_t v) {
    uint32_t r = e->rest[p];
    if (v < r) r -= (r - v + 3u) / 4u;
    else r += (v - r + TC_PAD_REST_RISE - 1u) / TC_PAD_REST_RISE;
    e->rest[p] = (uint16_t)(r ? (r > 4095u ? 4095u : r) : 1u);
}
/* Chord pads stay sounding after the hand lifts with hold on, or feeding the arpeggiator
 * with its latch on. */
static bool latching(void) { return tc_state.hold || (tc_state.arp != TC_ARP_OFF && tc_state.arp_latch); }
/* One MIDI clock (1/24 beat) in microseconds: from the MIDI clock coming in, else the tempo. */
static uint32_t tick_us_at(uint32_t now) {
    return tm_clock_active(now) && tm.tick_us ? tm.tick_us : 2500000u / tc_tempo_bpm(tc_state.tempo);
}
/* The clock position (rhythm.h, 1/256 clocks): the clock in use, the DAW's while it comes in, else
 * the board's own (tick_clock has run this pass), plus how far into the current clock now is. */
static uint32_t clock_pos(const tp_engine *e, uint32_t now) {
    uint32_t tick = tick_us_at(now), ticks, into;
    if (tm_clock_active(now)) { ticks = tm.ticks; into = (now - tm.last_tick_ms) * 1000u; }
    else {
        ticks = e->clock_ticks;
        int32_t since = (int32_t)(now * 1000u - (e->clock_us - tick)); /* the board's last tick */
        into = since > 0 ? (uint32_t)since : 0u;
    }
    uint32_t sub = tick && into < tick ? into * RH_SUB / tick : RH_SUB - 1u; /* a clock is under 65 ms: no overflow */
    return ticks * RH_SUB + sub;
}
bool tp_arm(tp_engine *e, tk_config c, const ts_scan *s, uint32_t request, uint32_t now) {
    if (e->state != TK_IDLE || !request || e->consumed_arm) return false;
    e->consumed_arm = request;
    tp_layout layout = tc_layout(&tc_state);
    if (!tk_config_ok(c) || !tc_layout_ok(layout) || !s->valid || !s->frame || now - s->last_ms > 100u) {
        stop(e); return false;
    }
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_notes notes;
        if (s->values[k] > 4095u || !tc_pad(layout, k, &notes)) { stop(e); return false; }
    }
    /* A pad that reads pressed now (held, or stuck on this board) never blocks the start:
     * it stays silent until it has read released once, and lights as pressed meanwhile. */
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        uint32_t p = tc_physical(&tc_state, k), v = s->values[p];
        tk_config kc = e->rest[p] ? tc_pad_raise(c, e->rest[p]) : c; /* its own resting level (tc_arm) */
        e->keys[k].inhibited = !released(kc, v);
        e->keys[k].sensed = kc.polarity == 1u ? v >= kc.press : v <= kc.press;
        e->keys[k].rising = false; e->keys[k].low_at = now;
    }
    for (uint32_t p = 0; p < TS_POSITIONS; ++p) e->rest_at[p] = now;
    e->config = c; e->layout = layout; e->frame = s->frame; e->reads = s->reads; e->frame_at = now; e->started = now;
    e->arp_random = 0x9E3779B9u ^ now;
    e->state = TK_ACTIVE; return true;
}
/* Hold: a chord pad lifted while hold is on keeps sounding (latched). Touching any
 * chord pad releases the latched ones; touching a latched pad itself silences it. */
static void hold_press(tp_engine *e) {
    for (uint32_t j = 0; j < 48u; ++j) {
        tp_key *other = &e->keys[j];
        if (other->latched) { other->latched = false; other->release_needed = true; }
    }
}
/* A touch is sensed, with the pressure its velocity comes from. */
static void sense(tp_engine *e, uint32_t k) {
    tp_key *key = &e->keys[k];
    key->sensed = true; key->rising = false; key->pressure = key->peak;
    /* Shift + chord pad (1.0.0): chosen for the chord edits, silent until lifted. */
    if (k < 48u && !key->admitted && tc_shift(&tc_state) && !tc_state.recording) key->inhibited = true;
    /* A latched pad touched again is released too and, being sensed, stays
     * silent until lifted (release_sources sets inhibited). */
    else if (k < 48u && (latching() || key->latched)) hold_press(e);
}
/* Read every few ms (1.5.0), the first reading past Trigger is early in the press, too soft for its
 * velocity: the touch waits one more reading, unless it reads Full or its half plays a fixed velocity.
 * With the last reading below Trigger longer ago (TC_PAD_VELOCITY_MS: frames of 15 ms as before
 * 1.5.0) the press has had time to rise: sensed at once. */
static bool velocity_waits(const tp_key *key, uint32_t k, uint8_t level, uint32_t now) {
    return level < 127u && (k < 48u ? tc_state.vel_mode : tc_state.melody_vel) != TC_VEL_FIXED &&
           now - key->low_at < TC_PAD_VELOCITY_MS;
}
static bool touched(tk_config c, uint32_t v) { return c.polarity == 1u ? v >= c.press : v <= c.press; }
/* Pad k (logical) reads v; follow: its resting level may follow the reading now. */
static void read_pad(tp_engine *e, uint32_t k, uint32_t v, bool follow, uint32_t now) {
    tp_key *key = &e->keys[k]; uint32_t p = tc_physical(&tc_state, k); /* Chords on Right: halves traded */
    tk_config kc = tp_key_config(e, k); /* raised for its own resting level (1.5.0) */
    uint8_t level = pressure(kc, v);
    key->pressure = level;
    if (key->rising) { /* the reading after the touch's first */
        if (level > key->peak) key->peak = level;
        sense(e, k); /* also when it reads released already: a touch that short still plays, */
        return;      /* and lets go at the next reading */
    }
    if (released(kc, v)) {
        key->sensed = false;
        if (key->admitted && !key->latched) {
            if (latching() && k < 48u && !key->release_needed) key->latched = true;
            else key->release_needed = true;
        } else if (!key->admitted) { key->inhibited = false; key->cut = false; }
    } else if (!key->sensed && touched(kc, v)) {
        key->peak = level;
        if (velocity_waits(key, k, level, now)) key->rising = true;
        else sense(e, k);
    }
    if (!key->sensed && !key->rising) key->low_at = now;
    /* Resting (not touched, and below Trigger): its resting level follows it. */
    if (follow && e->rest[p] && kc.polarity == 1u && !key->sensed && !key->rising && !key->admitted && !key->inhibited &&
        v < kc.press)
        follow_rest(e, p, v);
}
bool tp_frame(tp_engine *e, const ts_scan *s, uint32_t now) {
    if (e->state != TK_ACTIVE || !s->valid || !s->frame || now - s->last_ms > 100u || s->frame < e->frame)
        return false;
    /* The positions read since the last call, in turn up to s->last (a physical pad is 16 * bank +
     * its position); all 96 pads after a frame without position reads (or more than 15). */
    uint32_t n = s->reads - e->reads, read = 0u;
    if (!n && s->frame == e->frame) return false;
    bool some = n && n < TS_POSITIONS && s->last < TS_POSITIONS;
    for (uint32_t j = 0; j < (some ? n : TS_POSITIONS); ++j) read |= 1u << ((s->last + TS_POSITIONS - j) % TS_POSITIONS);
    for (uint32_t p = 0; p < TS_KEYS; ++p) if (((read >> (p % TS_POSITIONS)) & 1u) && s->values[p] > 4095u) return false;
    e->frame = s->frame; e->reads = s->reads; e->frame_at = now;
    uint32_t follow = 0u; /* the positions whose resting levels follow now */
    for (uint32_t p = 0; p < TS_POSITIONS; ++p)
        if (((read >> p) & 1u) && now - e->rest_at[p] >= TC_PAD_REST_MS) { follow |= 1u << p; e->rest_at[p] = now; }
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        uint32_t p = tc_physical(&tc_state, k);
        if ((read >> (p % TS_POSITIONS)) & 1u) read_pad(e, k, s->values[p], ((follow >> (p % TS_POSITIONS)) & 1u) != 0u, now);
    }
    return true;
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
    if (e->state == TK_ACTIVE || e->state == TK_DRAINING || e->pending_id) stop(e);
}
void tp_complete(tp_engine *e, uint32_t token, uint32_t bytes, bool success, uint32_t now) {
    if (e->state == TK_STOPPED) return;
    if (!token || token != e->pending_id || bytes != 4u || !success) { stop(e); return; }
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
/* Note-on velocity from the pad pressure on the frame it first counts as pressed:
 * just past the press threshold -> 1, full scale -> 127. Fixed mode -> 100. Each half has
 * its own curve (0.11.0: Chord vel, Melody vel). */
static uint8_t velocity(const tp_engine *e, uint32_t k, uint8_t p, bool melody) {
    uint8_t mode = melody ? tc_state.melody_vel : tc_state.vel_mode;
    if (mode == TC_VEL_FIXED) return 100u;
    tk_config kc = tp_key_config(e, k); /* the pad's own Trigger is velocity 1 */
    uint32_t p0 = pressure(kc, kc.press);
    if (p <= p0 || p0 >= 127u) return 1u;
    uint32_t r = (p - p0) * 126u / (127u - p0); /* 0..126 */
    /* Light: a soft touch already plays louder; firm: it takes a harder strike. */
    if (mode == TC_VEL_LIGHT) r = 126u - (126u - r) * (126u - r) / 126u;
    else if (mode == TC_VEL_FIRM) r = r * r / 126u;
    return (uint8_t)(1u + r);
}
static tp_voice *voice(tp_engine *e, uint8_t ch, uint8_t pitch) { return &e->voices[ch & 15u][pitch & 127u]; }
/* A Note Off still owed on any of these notes must go out before they are reused. */
static bool blocked(tp_engine *e, uint8_t ch, const tp_notes *n) {
    for (uint32_t i = 0; i < n->count; ++i) {
        tp_voice *v = voice(e, ch, n->notes[i]);
        if (v->exposed && !v->refs) return true;
    }
    return false;
}
static void own(tp_engine *e, tp_key *key) {
    for (uint32_t i = 0; i < key->captured.count; ++i) voice(e, key->channel, key->captured.notes[i])->refs++;
}
static void disown(tp_engine *e, tp_key *key) {
    for (uint32_t i = 0; i < key->captured.count; ++i) voice(e, key->channel, key->captured.notes[i])->refs--;
}
static void release_sources(tp_engine *e) {
    bool hold = latching();
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (key->latched && !hold) { key->latched = false; key->release_needed = true; }
        if (!key->admitted || !key->release_needed) continue;
        if (!key->silent) disown(e, key);
        key->admitted = false; key->release_needed = false; key->latched = false; key->silent = false;
        key->inhibited = key->sensed && !key->readmit; key->readmit = false;
    }
}
/* Whether chord pad k strums (strum on, and not switched off for this pad). */
static bool strums(uint32_t k) {
    return tc_state.strum != TC_STRUM_OFF && k < 48u && !((tc_state.strum_off >> k) & 1u);
}
static uint32_t strum_step_ms(uint32_t now) {
    uint32_t speed = tc_state.strum_speed % TC_STRUM_SPEEDS;
    return speed < TC_STRUM_MS ? tc_strum_ms[speed] : (tc_strum_ticks[speed - TC_STRUM_MS] * tick_us_at(now) + 500u) / 1000u;
}
static uint32_t random_below(tp_engine *e, uint32_t n, uint32_t now) {
    if (!e->random) e->random = now | 1u;
    e->random ^= e->random << 13; e->random ^= e->random >> 17; e->random ^= e->random << 5;
    return n ? e->random % n : 0u;
}
/* Strum: notes start one after another, low to high (up), high to low (down) or
 * alternating direction on each strike. due[i] is the delay of captured note i. Feel
 * (0.7.0): each strummed note up to tc_feel_ms late and its velocity up to tc_feel_vel
 * off, at random, never changing the order the notes start in. */
static void strum(tp_engine *e, tp_key *key, bool on, uint32_t now) {
    uint32_t mode = tc_state.strum, ms = strum_step_ms(now);
    uint32_t late = on ? tc_feel_ms[tc_state.feel_time % TC_FEEL_TIMES] : 0u;
    uint32_t vary = on ? tc_feel_vel[tc_state.feel_vel % TC_FEEL_VELS] : 0u;
    bool down = mode == TC_STRUM_DOWN || (mode == TC_STRUM_ALTERNATE && e->strum_down);
    if (on && mode == TC_STRUM_ALTERNATE) e->strum_down = !e->strum_down;
    uint32_t at[TP_MAX_NOTES] = {0}; /* start time by rank */
    for (uint32_t r = 0, previous = 0; r < key->captured.count; ++r) {
        uint32_t t = r * ms + random_below(e, late + 1u, now);
        at[r] = previous = t < previous ? previous : t;
    }
    for (uint32_t i = 0; i < key->captured.count; ++i) {
        uint32_t rank = 0;
        for (uint32_t j = 0; j < key->captured.count; ++j) {
            uint8_t a = key->captured.notes[i], b = key->captured.notes[j];
            if (b < a || (b == a && j < i)) rank++;
        }
        if (down) rank = key->captured.count - 1u - rank;
        key->due[i] = on ? (uint16_t)at[rank] : 0u;
        key->feel[i] = (int8_t)((int32_t)random_below(e, 2u * vary + 1u, now) - (int32_t)vary);
    }
}
/* Strum repeat (strike the chord again while held) and cutoff (end strummed chords
 * after a note value), both at the tempo or the incoming MIDI clock. */
static void strum_timers(tp_engine *e, uint32_t now) {
    uint32_t tick = tick_us_at(now);
    uint32_t repeat = (tc_repeat_ticks[tc_state.strum_repeat % TC_REPEATS] * tick + 500u) / 1000u;
    uint32_t cutoff = (tc_cutoff_ticks[tc_state.strum_cutoff % TC_CUTOFFS] * tick + 500u) / 1000u;
    /* Note repeat (0.9.0): a held right-half pad strikes again on every step of its rate, its
     * velocity from how hard it is pressed by then. 1.4.0: the steps are the clock's (rhythm.h),
     * so repeats on several pads, and a DAW, stay in step: the touch is the first strike, the next
     * comes on the first step at least half a step later. Each strike sounds half a step (less
     * when the next comes sooner) and rests at least a quarter step, so every strike is heard as
     * a new note (an Off and an On a millisecond apart can reach a synth as one unbroken note).
     * Repeat rhythm silences the steps a Euclidean pattern leaves out; By row gives each row its
     * own rate. */
    uint32_t pos = clock_pos(e, now), rhythm_steps = 1u;
    uint64_t rhythm = ~(uint64_t)0;
    if (tc_state.repeat_rhythm) rhythm = tc_rhythm_mask((uint8_t)(TC_RHYTHMS + tc_state.repeat_rhythm - 1u), 0u, &rhythm_steps);
    for (uint32_t k = 48u; tc_state.note_repeat && k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        rh_rate rate = tc_note_repeat_rate(tc_state.note_repeat, k);
        if (!key->sensed || !rate.num) { key->repeating = false; continue; }
        uint32_t len = rh_step_start(1u, rate);
        if (key->admitted && !key->repeating) { /* the touch */
            key->repeating = true;
            key->rep_next = rh_step_at(pos + len / 2u, rate) + 1u;
            uint32_t next = rh_step_start(key->rep_next, rate);
            key->rep_off = next - pos >= 3u * len / 4u ? pos + len / 2u : next - len / 4u;
        }
        if (!key->repeating) continue;
        uint32_t at = rh_step_start(key->rep_next, rate);
        if ((int32_t)(pos - at) > (int32_t)len || (int32_t)(at - pos) > (int32_t)(2u * len)) { /* the clock jumped */
            key->rep_next = rh_step_at(pos, rate) + 1u; at = rh_step_start(key->rep_next, rate);
        }
        if (key->admitted && !key->release_needed && (int32_t)(pos - key->rep_off) >= 0) {
            key->release_needed = true; key->cut = true;
        } else if (!key->admitted && key->cut && (int32_t)(pos - at) >= 0) {
            uint32_t step = key->rep_next++;
            key->rep_off = at + (rh_step_start(key->rep_next, rate) - at) / 2u;
            if ((rhythm >> (step % rhythm_steps)) & 1u) { key->inhibited = false; key->cut = false; } /* strike again */
        }
    }
    if (!repeat && !cutoff) return;
    for (uint32_t k = 0; k < 48u; ++k) {
        tp_key *key = &e->keys[k];
        if (!strums(k)) continue;
        uint32_t age = now - key->admitted_at;
        if (key->admitted && !key->silent && !key->release_needed) {
            if (repeat && key->sensed && age >= repeat) { key->release_needed = true; key->readmit = true; }
            else if (cutoff && age >= cutoff) { key->release_needed = true; key->cut = key->sensed; }
        } else if (!key->admitted && key->cut && key->sensed && repeat && age >= repeat) {
            key->inhibited = false; key->cut = false; /* strike again */
        }
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
        uint8_t ch = channel_of(k);
        if (value) {
            ln_step s; memset(&s, 0, sizeof s);
            s.value = ln_pad_value(lane->kind, k);
            if (!tc_step_notes(e->layout, lane->kind, lane->channel, &s, &notes, &ch)) notes.count = 0u;
        } else if (!tc_pad(e->layout, k, &notes)) { stop(e); return; }
        if (!value && k < 48u && ((tc_state.chord_mute >> k) & 1u)) notes.count = 0u; /* sets the live chord only */
        tp_notes core; /* voice leading (0.9.0): the next chord leads from this one */
        if (!value && k < 48u && notes.count && tc_pad_core(e->layout, k, &core)) tc_state.lead = core;
        bool silent = arp && k < 48u && !value;
        if (!silent && blocked(e, ch, &notes)) continue; /* atomic whole-pad deferral, never steal cleanup */
        key->captured = notes; key->admitted = true; key->channel = ch; key->silent = silent;
        memset(key->note_pressure, 0, sizeof key->note_pressure);
        for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) key->note_at[i] = now;
        key->admitted_at = now;
        key->velocity = velocity(e, k, key->pressure, k >= 48u); key->order = ++e->admissions;
        memset(key->due, 0, sizeof key->due); memset(key->feel, 0, sizeof key->feel);
        if (!silent) strum(e, key, strums(k) && !value, now);
        if (!silent) own(e, key);
    }
}
/* Chord pads change role when the arpeggiator is switched while they are held. */
static void arp_membership(tp_engine *e, uint32_t now) {
    bool arp = tc_state.arp != TC_ARP_OFF;
    for (uint32_t k = 0; k < 48u; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->admitted || key->release_needed || key->silent == arp) continue;
        if (arp) { disown(e, key); key->silent = true; continue; }
        if (blocked(e, key->channel, &key->captured)) continue;
        key->silent = false; key->admitted_at = now; memset(key->due, 0, sizeof key->due); memset(key->feel, 0, sizeof key->feel);
        own(e, key);
    }
}
uint32_t tp_arp_pool(const tp_engine *e, uint8_t pool[TP_ARP_POOL]) {
    uint8_t base[TP_ARP_POOL]; uint32_t n = 0;
    uint32_t mode = tc_state.arp, octaves = 1u + tc_state.arp_octaves % TC_ARP_OCTAVES;
    if (mode == TC_ARP_PLAYED) {
        /* Pads in the order they were struck, each chord low to high. */
        uint32_t last = 0;
        for (;;) {
            uint32_t next = UINT32_MAX, which = 0;
            for (uint32_t i = 0; i < 49u; ++i) { /* chord pads, then the progression */
                uint32_t k = i < 48u ? i : TP_SEQ;
                const tp_key *key = &e->keys[k];
                if (key->admitted && key->silent && !key->release_needed && key->order > last && key->order < next) {
                    next = key->order; which = k;
                }
            }
            if (next == UINT32_MAX) break;
            last = next;
            const tp_notes *c = &e->keys[which].captured;
            uint8_t sorted[TP_MAX_NOTES]; uint32_t m = c->count;
            memcpy(sorted, c->notes, m);
            for (uint32_t i = 1; i < m; ++i)
                for (uint32_t j = i; j && sorted[j - 1u] > sorted[j]; --j) {
                    uint8_t t = sorted[j]; sorted[j] = sorted[j - 1u]; sorted[j - 1u] = t;
                }
            for (uint32_t i = 0; i < m && n < TP_ARP_POOL; ++i) {
                bool dup = false;
                for (uint32_t j = 0; j < n; ++j) dup = dup || base[j] == sorted[i];
                if (!dup) base[n++] = sorted[i];
            }
        }
    } else {
        /* Every held note once, low to high. */
        bool present[128] = {false};
        for (uint32_t j = 0; j < 49u; ++j) {
            const tp_key *key = &e->keys[j < 48u ? j : TP_SEQ];
            if (!key->admitted || !key->silent || key->release_needed) continue;
            for (uint32_t i = 0; i < key->captured.count; ++i) present[key->captured.notes[i] & 127u] = true;
        }
        for (uint32_t p = 0; p < 128u && n < TP_ARP_POOL; ++p) if (present[p]) base[n++] = (uint8_t)p;
    }
    uint32_t total = 0;
    for (uint32_t o = 0; o < octaves; ++o)
        for (uint32_t i = 0; i < n && total < TP_ARP_POOL; ++i)
            if (base[i] + 12u * o <= 127u) pool[total++] = (uint8_t)(base[i] + 12u * o);
    if (mode != TC_ARP_PLAYED) { /* octave copies keep the low-to-high order */
        for (uint32_t i = 1; i < total; ++i)
            for (uint32_t j = i; j && pool[j - 1u] > pool[j]; --j) {
                uint8_t t = pool[j]; pool[j] = pool[j - 1u]; pool[j - 1u] = t;
            }
    }
    return total;
}
static uint32_t arp_pick(tp_engine *e, uint32_t n) {
    uint32_t step = e->arp_step;
    switch (tc_state.arp) {
    case TC_ARP_DOWN: return n - 1u - step % n;
    case TC_ARP_UPDOWN: {
        if (n < 2u) return 0u;
        uint32_t period = 2u * n - 2u, p = step % period;
        return p < n ? p : period - p;
    }
    case TC_ARP_RANDOM:
        e->arp_random ^= e->arp_random << 13; e->arp_random ^= e->arp_random >> 17; e->arp_random ^= e->arp_random << 5;
        return e->arp_random % n;
    default: return step % n;
    }
}
static void arp_release(tp_engine *e) {
    tp_key *a = &e->keys[TP_ARP];
    if (a->admitted) { disown(e, a); a->admitted = false; }
}
/* Where step n starts, counted from the arpeggio's origin: steps of the rate, swung in pairs (the
 * first of each pair longer by the swing, the second shorter). */
static uint32_t arp_boundary(uint32_t n, rh_rate rate, uint32_t swing_pct) {
    uint32_t base = rh_step_start(n & ~1u, rate);
    if (!(n & 1u)) return base;
    uint32_t one = rh_step_start((n & ~1u) + 1u, rate) - base;
    return base + one + one * swing_pct / 100u;
}
/* The arpeggiator counts MIDI clocks: from the DAW when its clock runs, else the board's own
 * clock from the tempo. 1.4.0: on the clock position (rhythm.h), so a rate that is not a whole
 * number of clocks (5 or 7 to a beat) stays exact; step n starts at the origin plus n steps,
 * swung in pairs. The rhythm (an 8-step pattern or a Euclidean one, rotated) mutes some steps
 * (silent steps still move the pattern on). With sync, it starts on the next step of the rate's
 * grid counted from the clock's start, instead of at once. */
static void arpeggiate(tp_engine *e, uint32_t now) {
    tp_key *a = &e->keys[TP_ARP];
    uint8_t pool[TP_ARP_POOL];
    uint32_t n = tc_state.arp != TC_ARP_OFF && e->state == TK_ACTIVE ? tp_arp_pool(e, pool) : 0u;
    bool clocked = tm_clock_active(now);
    uint32_t pos = clock_pos(e, now);
    uint8_t rate_at = (uint8_t)(tc_state.arp_rate % TC_ARP_RATES), swing_at = (uint8_t)(tc_state.arp_swing % TC_SWINGS);
    rh_rate rate = tc_arp_rates[rate_at];
    uint32_t swing = tc_swing_pct[swing_at], len = rh_step_start(1u, rate);
    if (!n) { arp_release(e); e->arp_running = false; e->arp_pending = false; e->arp_step = 0; return; }
    bool step_now = false;
    if (!e->arp_running) {
        e->arp_running = true; e->arp_step = 0; e->arp_starts = tm.starts;
        e->arp_rate_at = rate_at; e->arp_swing_at = swing_at; e->arp_origin = pos; e->arp_waiting = false;
        if (tc_state.arp_sync) {
            uint32_t grid = rh_step_start(rh_step_at(pos, rate), rate);
            if (grid != pos) { e->arp_origin = rh_step_start(rh_step_at(pos, rate) + 1u, rate); e->arp_waiting = true; }
        }
        e->arp_next_at = e->arp_origin;
        step_now = !e->arp_waiting;
    } else if (clocked != e->arp_clocked) { /* the timing source changed: the next step a step from now */
        e->arp_origin = pos + len - arp_boundary(e->arp_step, rate, swing); e->arp_next_at = pos + len;
        e->arp_rate_at = rate_at; e->arp_swing_at = swing_at; e->arp_waiting = false;
    } else if (clocked && tm.starts != e->arp_starts) { /* DAW Start: restart on the beat */
        e->arp_starts = tm.starts; e->arp_step = 0; e->arp_origin = pos; e->arp_next_at = pos; e->arp_waiting = false; step_now = true;
        e->arp_rate_at = rate_at; e->arp_swing_at = swing_at;
    } else if ((int32_t)(pos - e->arp_next_at) >= 0 || (int32_t)(e->arp_next_at - pos) > (int32_t)(2u * len)) {
        step_now = true; /* its time (or the clock went back: the board's Start) */
    }
    e->arp_clocked = clocked;
    uint32_t gate = tc_arp_gate_pct[tc_state.arp_gate % TC_ARP_GATES];
    if (step_now) {
        arp_release(e);
        uint32_t start = (int32_t)(pos - e->arp_next_at) >= 0 ? e->arp_next_at : pos;
        e->arp_waiting = false;
        if ((int32_t)(pos - start) > (int32_t)len) start = pos; /* fell behind: never burst to catch up */
        if (rate_at != e->arp_rate_at || swing_at != e->arp_swing_at || start != e->arp_origin + arp_boundary(e->arp_step, rate, swing)) {
            /* a new rate or swing, or a late step: count on from where this step starts */
            e->arp_rate_at = rate_at; e->arp_swing_at = swing_at;
            e->arp_origin = start - arp_boundary(e->arp_step, rate, swing);
        }
        uint32_t next = e->arp_origin + arp_boundary(e->arp_step + 1u, rate, swing);
        e->arp_next_at = next;
        e->arp_off_at = start + (next - start) * gate / 100u; /* a step is at most 1.5 beats of positions */
        uint32_t steps;
        uint64_t rhythm = tc_rhythm_mask(tc_state.arp_rhythm, tc_state.arp_rotate, &steps);
        e->arp_pending = (rhythm >> (e->arp_step % steps)) & 1u;
        e->arp_play = e->arp_step++;
    } else if (a->admitted && gate < 100u && (int32_t)(pos - e->arp_off_at) >= 0) arp_release(e);
    if (!e->arp_pending) return;
    tp_notes play; memset(&play, 0, sizeof play);
    if (tc_state.arp == TC_ARP_CHORDS) { /* the whole held chord every step */
        for (uint32_t i = 0; i < n && play.count < TP_MAX_NOTES; ++i) play.notes[play.count++] = pool[i];
    } else {
        uint32_t step = e->arp_step; e->arp_step = e->arp_play;
        play.notes[0] = pool[arp_pick(e, n)]; play.count = 1;
        e->arp_step = step;
    }
    uint8_t ch = channel_of(TP_ARP);
    if (blocked(e, ch, &play)) return; /* previous Off still owed: retry next pass */
    e->arp_pending = false;
    a->captured = play; a->channel = ch; a->admitted = true; a->admitted_at = now;
    memset(a->note_pressure, 0, sizeof a->note_pressure);
    for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) a->note_at[i] = now;
    strum(e, a, tc_state.arp == TC_ARP_CHORDS && tc_state.strum != TC_STRUM_OFF, now);
    /* Velocity of the most recently struck chord pad (or progression chord). */
    uint32_t newest = 0; a->velocity = 100u;
    for (uint32_t i = 0; i < 49u; ++i) {
        const tp_key *key = &e->keys[i < 48u ? i : TP_SEQ];
        if (key->admitted && key->silent && key->order >= newest) { newest = key->order; a->velocity = key->velocity; }
    }
    own(e, a);
}
/* The board's own clock: 24 ticks per beat at the tempo; with Clock out on (and no
 * MIDI clock coming in) every tick is owed as a MIDI clock byte. */
static void tick_clock(tp_engine *e, uint32_t now) {
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
static void pedal_note(tp_engine *e, uint32_t now) {
    tp_key *p = &e->keys[TP_PEDAL];
    bool down = e->state == TK_ACTIVE && tc_state.pedal_down_note;
    if (p->admitted && !down) { disown(e, p); p->admitted = false; return; }
    if (p->admitted || !down) return;
    tp_notes one; memset(&one, 0, sizeof one);
    one.notes[0] = (uint8_t)(tc_state.pedal_note & 127u); one.count = 1;
    uint8_t ch = channel_of(TP_PEDAL);
    if (blocked(e, ch, &one)) return;
    p->captured = one; p->channel = ch; p->admitted = true; p->admitted_at = now; p->velocity = 100u;
    memset(p->due, 0, sizeof p->due); memset(p->note_pressure, 0, sizeof p->note_pressure);
    own(e, p);
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
static void seq_release(tp_engine *e) {
    tp_key *q = &e->keys[TP_SEQ];
    if (q->admitted) { if (!q->silent) disown(e, q); q->admitted = false; }
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
static void sequence(tp_engine *e, uint32_t now) {
    tp_key *q = &e->keys[TP_SEQ];
    bool clocked = tm_clock_active(now), daw = tm.running && clocked;
    bool run = e->state == TK_ACTIVE && sq.count && !tc_state.recording && (tc_state.transport || daw);
    if (!run) { /* stopped; a session ending (the editor, USB) keeps the place for the next one */
        seq_release(e); e->seq_pending = false;
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
    seq_release(e);
    const sq_entry *en = &sq.entries[e->seq_entry];
    tp_layout l = sq_layout(&sq, e->layout, en);
    tp_notes notes;
    if (!tc_pad_with(l, en->pad, en->edit, &notes)) memset(&notes, 0, sizeof notes);
    if ((tc_state.chord_mute >> en->pad) & 1u) notes.count = 0u; /* Sound off: only the live notes follow */
    uint8_t ch = channel_of(TP_ARP); /* the chord channel */
    if (!arp && blocked(e, ch, &notes)) return; /* an owed Note Off first: next pass */
    e->seq_pending = false;
    q->captured = notes; q->admitted = true; q->channel = ch; q->silent = arp; q->release_needed = false;
    q->admitted_at = now; q->velocity = tc_state.vel_mode == TC_VEL_FIXED ? 100u : en->velocity; /* Fixed wins */ q->order = ++e->admissions;
    memset(q->note_pressure, 0, sizeof q->note_pressure); memset(q->due, 0, sizeof q->due); memset(q->feel, 0, sizeof q->feel);
    for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) q->note_at[i] = now;
    if (!arp) {
        strum(e, q, tc_state.strum != TC_STRUM_OFF && !((tc_state.strum_off >> en->pad) & 1u), now);
        own(e, q);
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
static void lane_release(tp_engine *e, uint32_t i) {
    tp_key *q = &e->keys[TP_LANE0 + i];
    if (q->admitted) { disown(e, q); q->admitted = false; }
}
static void lanes(tp_engine *e, uint32_t now) {
    bool clocked = tm_clock_active(now), daw = tm.running && clocked;
    bool run = e->state == TK_ACTIVE && !tc_state.recording && (tc_state.transport || daw);
    if (!run) { /* stopped; a session ending keeps the place for the next one */
        for (uint32_t i = 0; i < LN_LANES; ++i) { lane_release(e, i); e->lane_pending[i] = false; e->lane_step[i] = LN_NONE; }
        if (e->state == TK_ACTIVE) e->lane_running = false;
        return;
    }
    uint32_t cpos = clock_pos(e, now);
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
        uint32_t count, step = ln_locate(e->lane_pos, rate, ln_length(l), &count);
        bool plays = l->kind != LN_OFF && !l->mute;
        e->lane_step[i] = (uint8_t)step;
        if (count != e->lane_count[i]) { /* a new step */
            lane_release(e, i);
            e->lane_count[i] = count; e->lane_off[i] = ln_gate_end(count, rate, LN_GATE_PCT);
            e->lane_pending[i] = plays && l->steps[step].on;
        }
        bool over = (int32_t)(e->lane_pos - e->lane_off[i]) >= 0;
        if (over || !plays) { lane_release(e, i); e->lane_pending[i] = false; continue; }
        if (!e->lane_pending[i]) continue;
        const ln_step *s = &l->steps[step];
        tp_notes notes; uint8_t ch;
        if (!tc_step_notes(e->layout, l->kind, l->channel, s, &notes, &ch)) { e->lane_pending[i] = false; continue; }
        if (blocked(e, ch, &notes)) continue; /* an owed Note Off first: next pass */
        e->lane_pending[i] = false;
        bool melody = l->kind != LN_CHORDS;
        bool fixed = (melody ? tc_state.melody_vel : tc_state.vel_mode) == TC_VEL_FIXED;
        q->captured = notes; q->admitted = true; q->channel = ch; q->silent = false; q->release_needed = false;
        q->admitted_at = now; q->order = ++e->admissions;
        q->velocity = fixed || !s->velocity ? (uint8_t)LN_VELOCITY_DEFAULT : s->velocity; /* Fixed wins, as progressions */
        memset(q->note_pressure, 0, sizeof q->note_pressure); memset(q->due, 0, sizeof q->due); memset(q->feel, 0, sizeof q->feel);
        for (uint32_t n = 0; n < TP_MAX_NOTES; ++n) q->note_at[n] = now;
        strum(e, q, !melody && tc_state.strum != TC_STRUM_OFF, now);
        own(e, q);
        tp_notes core; /* a chord lane's chords lead on from each other (voice leading) */
        if (!melody && tc_pad_core_with(e->layout, s->value, ln_edit(s->edit), &core)) tc_state.lead = core;
    }
}
/* Scale lock (1.0.0): notes arriving on the lock channel (midi_in.c tm.lock_vel) play on the
 * melody channel, moved to the nearest note of the scale (tc_quantise). Each keeps the note it
 * was given until the incoming note ends (or strikes again), whatever the key does meanwhile. */
static uint32_t lock_seen;
static bool lock_waiting; /* a note waits for a free source or an owed Note Off */
static void lock_notes(tp_engine *e, uint32_t now) {
    if (tm.lock_channel != tc_state.lock) { tm.lock_channel = tc_state.lock; memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++; }
    bool active = e->state == TK_ACTIVE;
    for (uint32_t i = 0; i < TP_LOCKS; ++i) {
        tp_key *key = &e->keys[TP_LOCK + i];
        if (key->admitted && (!active || !tm.lock_vel[key->input] || tm.lock_strikes[key->input] != key->strike)) {
            disown(e, key); key->admitted = false; lock_waiting = true;
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
        if (blocked(e, ch, &one)) { lock_waiting = true; continue; }
        free->captured = one; free->channel = ch; free->admitted = true; free->silent = false; free->release_needed = false;
        free->input = (uint8_t)p; free->strike = tm.lock_strikes[p]; free->velocity = tm.lock_vel[p];
        free->admitted_at = now; free->order = ++e->admissions;
        memset(free->due, 0, sizeof free->due); memset(free->feel, 0, sizeof free->feel);
        memset(free->note_pressure, 0, sizeof free->note_pressure);
        for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) free->note_at[i] = now;
        own(e, free);
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
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        tp_key *source = &e->keys[k];
        if (!source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            if (!voice(e, source->channel, p)->exposed && now - source->admitted_at >= source->due[i]) {
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
            const tp_voice *v = voice(e, source->channel, p);
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
    if (e->pending_id && now - e->pending_at >= 2000u) { stop(e); return; }
    if (e->state == TK_ACTIVE && now - e->frame_at > 100u) tp_disable(e); /* scanner stale */
    /* Keep enough monotonic tokens for all exposed OFFs. Pressure/ON cannot
     * consume the final cleanup tokens. No counter wrap or reused receipts. */
    if (e->state == TK_ACTIVE && UINT32_MAX - e->issued <= tp_obligations(e)) tp_disable(e);
    if (!tc_state.transport) e->transport_paused = false;
    tick_clock(e, now);
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
    if (e->state == TK_ACTIVE) strum_timers(e, now);
    if (tc_state.panic) { /* all notes off (1.0.0): everything the board plays ends; held pads stay quiet until lifted */
        tc_state.panic = false;
        seq_release(e); e->seq_pending = false; /* following a DAW, it comes back at the next chord */
        for (uint32_t i = 0; i < LN_LANES; ++i) { lane_release(e, i); e->lane_pending[i] = false; } /* and the next step */
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            tp_key *key = &e->keys[k];
            if (key->admitted) { key->latched = false; key->release_needed = true; key->readmit = false; }
        }
        memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++;
    }
    release_sources(e);
    if (e->state == TK_ACTIVE) { admit_sources(e, now); arp_membership(e, now); }
    if (e->state == TK_STOPPED) return;
    sequence(e, now);
    lanes(e, now);
    arpeggiate(e, now);
    pedal_note(e, now);
    lock_notes(e, now);
    if (e->pending_id) return;
    candidate_t c;
    if (!candidate(e, now, &c)) {
        e->waiting = false;
        if (e->state == TK_DRAINING) e->state = TK_DONE;
        return;
    }
    if (c.kind == TK_ON && UINT32_MAX - e->issued <= tp_obligations(e) + 1u) {
        tp_disable(e); release_sources(e); arp_release(e); return;
    }
    if (!e->waiting) { e->waiting = true; e->wait_at = now; }
    if (now - e->wait_at >= 5000u || e->issued == UINT32_MAX) { stop(e); return; }
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
}
static uint32_t session_at, arm_requests;
bool tc_arm(tp_engine *e, const ts_scan *s, uint32_t now, bool ready) {
    if (e->state != TK_IDLE || !ready || now - session_at < 500u) return false;
    session_at = now;
    arm_requests = arm_requests % 65535u + 1u;
    /* Each pad's resting level (1.5.0; before, one level for the board: the 12th highest pad), from
     * the starting frame. A pad reading 2000 or more is held, not resting: it starts from the
     * board's middle level and stays silent until let go (tp_arm); then its level follows it. */
    uint8_t order[TS_KEYS];
    ts_strongest(s->values, order, TS_KEYS);
    uint32_t middle = s->values[order[TS_KEYS / 2u]];
    for (uint32_t k = 0; k < TS_KEYS; ++k) { /* k: the physical pad */
        uint32_t v = s->values[k];
        if (v >= TC_PAD_REST_CAP) v = middle < TC_PAD_REST_CAP ? middle : TC_PAD_REST_CAP;
        e->rest[k] = (uint16_t)(v ? v : 1u);
    }
    return tp_arm(e, tc_pad_config(&tc_state), s, arm_requests, now);
}
/* Chord capture (1.4.0) from MIDI in, any channel: a chord is the most pitch classes held at once,
 * taken at the first release (so a rolled chord counts whole); after it, a new chord starts when
 * every note is up or a note outside it arrives (legato playing). */
static uint32_t capture_seen, capture_low;
static uint16_t capture_peak, capture_taken;
static uint32_t classes_of(uint16_t set) { uint32_t n = 0; for (; set; set &= (uint16_t)(set - 1u)) n++; return n; }
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
    if (classes_of(held) >= classes_of(capture_peak) && held) { capture_peak = held; capture_low = low; return; }
    if (classes_of(capture_peak) >= 2u) { /* the first release: the chord as it was at its fullest */
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
    if(tc_state.ui==TC_UI_STEPS) { /* the step screen (1.5.0): pads edit the lane (controls.c tc_step_pad) */
        for(uint32_t k=0;k<TS_KEYS;k++) if(e->keys[k].sensed!=was_sensed[k])
            tc_step_pad(&tc_state,e->layout,k,e->keys[k].sensed,velocity(e,k,e->keys[k].pressure,false));
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
            tc_capture_chord(&tc_state, root, (uint16_t)(((classes >> root) | (classes << (12u - root))) & 0xFFFu));
        }
        bool same = !tc_state.live_from_midi && tc_state.live_pad == k;
        tc_state.last_chord=(uint8_t)k; tc_state.have_chord=true;
        tc_state.live_pad=(uint8_t)k; tc_state.live_from_midi=false;
        if (!same && tc_state.live_update) tc_state.relayout_live = true;
        if (tc_state.recording) { /* recording a progression: each touch adds one unit of this chord */
            bool added = sq_tap(&sq, (uint8_t)k, velocity(e, k, e->keys[k].pressure, false), tc_state.scale,
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
