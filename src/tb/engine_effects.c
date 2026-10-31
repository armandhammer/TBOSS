/* SPDX-License-Identifier: GPL-3.0-or-later - the engine (engine.h): what changes how a source plays. Strum,
 * strum repeat and cutoff, note repeat, Chance, echo and Mono. */
#include "engine_internal.h"
#include <string.h>
/* Mono: whether lifting melody pad k brings back an older one still held (hidden). Not the gap
 * between note repeat's strikes: the pad is still held (1.7.0 fix). */
bool tp_mono_returns(const tp_engine *e, uint32_t k) {
    if (k < 48u || !tc_state.mono || e->layout.melody_layout == TP_MELODY_DRUMS || e->keys[k].cut) return false;
    for (uint32_t j = 48u; j < TS_KEYS; ++j) {
        const tp_key *key = &e->keys[j];
        if (key->admitted && key->hidden && !key->release_needed) return true;
    }
    return false;
}
void tp_mono_drop(tp_engine *e) {
    for (uint32_t i = 0; i < e->mono_hold.count; ++i) tp_voice_of(e, e->mono_hold_ch, e->mono_hold.notes[i])->refs--;
    e->mono_hold.count = 0u;
}
/* Whether chord pad k strums (strum on, and not switched off for this pad). */
bool tp_strums(uint32_t k) {
    return tc_state.strum != TC_STRUM_OFF && k < 48u && !((tc_state.strum_off >> k) & 1u);
}
static uint32_t strum_step_ms(uint32_t now) {
    uint32_t speed = tc_state.strum_speed % TC_STRUM_SPEEDS;
    return speed < TC_STRUM_MS ? tc_strum_ms[speed] : (tc_strum_ticks[speed - TC_STRUM_MS] * tp_tick_us(now) + 500u) / 1000u;
}
static uint32_t random_below(tp_engine *e, uint32_t n, uint32_t now) {
    if (!e->random) e->random = now | 1u;
    e->random ^= e->random << 13; e->random ^= e->random >> 17; e->random ^= e->random << 5;
    return n ? e->random % n : 0u;
}
/* Chance (1.6.0): true with the Chance setting's probability (at 100 % no random number is drawn). */
bool tp_odds(tp_engine *e, uint32_t pct, uint32_t now) { return pct >= 100u || random_below(e, 100u, now) < pct; }
bool tp_chance(tp_engine *e, uint32_t now) { return tp_odds(e, tc_chance_pct[tc_state.chance % TC_CHANCES], now); }
/* Strum: notes start one after another, low to high (up), high to low (down) or
 * alternating direction on each strike. due[i] is the delay of captured note i. Feel
 * (0.7.0): each strummed note up to tc_feel_ms late and its velocity up to tc_feel_vel
 * off, at random, never changing the order the notes start in. */
void tp_strum(tp_engine *e, tp_key *key, bool on, uint32_t now) {
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
/* Mono: whether a newer melody pad than k is held, sounding or between its repeats. A pad between its
 * own repeats isn't sounding, so it wasn't hidden when that newer pad came; its next strike would be
 * the newest and take over (1.7.0 fix). */
static bool mono_under(const tp_engine *e, uint32_t k) {
    if (!tc_state.mono || e->layout.melody_layout == TP_MELODY_DRUMS) return false;
    for (uint32_t j = 48u; j < TS_KEYS; ++j) {
        const tp_key *key = &e->keys[j];
        if (j != k && key->sensed && key->order > e->keys[k].order &&
            ((key->admitted && !key->release_needed) || (key->cut && key->repeating))) return true;
    }
    return false;
}
/* Strum repeat (strike the chord again while held) and cutoff (end strummed chords
 * after a note value), both at the tempo or the incoming MIDI clock. */
void tp_strum_timers(tp_engine *e, uint32_t now) {
    uint32_t tick = tp_tick_us(now);
    uint32_t repeat = (tc_repeat_ticks[tc_state.strum_repeat % TC_REPEATS] * tick + 500u) / 1000u;
    uint32_t cutoff = (tc_cutoff_ticks[tc_state.strum_cutoff % TC_CUTOFFS] * tick + 500u) / 1000u;
    /* Note repeat (0.9.0): a held right-half pad strikes again on every step of its rate, its
     * velocity from how hard it is pressed by then. 1.4.0: the steps are the clock's (rhythm.h),
     * so repeats on several pads, and a DAW, stay in step: the touch is the first strike, the next
     * comes on the first step at least half a step later. Each strike sounds half a step (less
     * when the next comes sooner) and rests at least a quarter step, so every strike is heard as
     * a new note (an Off and an On a millisecond apart can reach a synth as one unbroken note).
     * Repeat rhythm silences the steps a Euclidean pattern leaves out; By row gives each row its
     * own rate. Chance (1.6.0) silences strikes after the touch at random. */
    uint32_t pos = tp_clock_pos(e, now), rhythm_steps = 1u;
    uint64_t rhythm = ~(uint64_t)0;
    if (tc_state.repeat_rhythm) rhythm = tc_rhythm_mask((uint8_t)(TC_RHYTHMS + tc_state.repeat_rhythm - 1u), 0u, &rhythm_steps);
    for (uint32_t k = 48u; tc_state.note_repeat && k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        rh_rate rate = tc_note_repeat_rate(tc_state.note_repeat, k);
        if (!key->sensed || !rate.num) { key->repeating = false; continue; }
        if (key->hidden) continue; /* Mono (1.7.0 fix): a pad under a newer one doesn't repeat */
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
            if (((rhythm >> (step % rhythm_steps)) & 1u) && !mono_under(e, k) && tp_chance(e, now)) {
                key->inhibited = false; key->cut = false; /* strike again (under Mono: not under a newer pad) */
            }
        }
    }
    if (!repeat && !cutoff) return;
    for (uint32_t k = 0; k < 48u; ++k) {
        tp_key *key = &e->keys[k];
        if (!tp_strums(k)) continue;
        uint32_t age = now - key->admitted_at;
        if (key->admitted && !key->silent && !key->release_needed) {
            if (repeat && key->sensed && age >= repeat) { key->release_needed = true; key->readmit = true; }
            else if (cutoff && age >= cutoff) { key->release_needed = true; key->cut = key->sensed; }
        } else if (!key->admitted && key->cut && key->sensed && repeat && age >= repeat) {
            key->inhibited = false; key->cut = false; /* strike again */
        }
    }
}
/* Echo (1.6.0): each melody pad strike comes back Echoes times, the Echo time apart on the clock (the
 * DAW's while it comes in, else the tempo's), the n-th of N at (N + 1 - n) / (N + 1) of the strike's
 * velocity, each sounding half the echo time. A strike takes a free echo source, else the oldest one's.
 * An echo that comes too late (an owed Note Off kept it waiting half the echo time) is skipped. The
 * ownership rule holds: a note still sounding when its echo comes (the pad still held) gets no second
 * Note On, so that echo is not heard. */
void tp_echo_release(tp_engine *e, uint32_t i) {
    tp_key *q = &e->keys[TP_ECHO0 + i];
    if (q->admitted) { tp_disown(e, q); q->admitted = false; }
}
void tp_echo_start(tp_engine *e, const tp_key *key, uint32_t now) {
    uint32_t clocks = tc_echo_ticks[tc_state.echo % TC_ECHO_TIMES], pick = 0;
    if (!clocks || !key->captured.count) return;
    for (uint32_t i = 0; i < TP_ECHOES; ++i) {
        if (!e->echo[i].left && !e->keys[TP_ECHO0 + i].admitted) { pick = i; break; }
        if (e->echo[i].order < e->echo[pick].order) pick = i;
    }
    tp_echo_release(e, pick);
    tp_echo *x = &e->echo[pick];
    x->notes = key->captured; x->channel = key->channel; x->velocity = key->velocity; x->order = key->order;
    x->total = x->left = (uint8_t)(1u + tc_state.echoes % TC_ECHOES);
    x->len = clocks * RH_SUB; x->next = tp_clock_pos(e, now) + x->len;
}
void tp_echoes(tp_engine *e, uint32_t now) {
    bool run = e->state == TK_ACTIVE && tc_state.echo;
    uint32_t pos = tp_clock_pos(e, now);
    for (uint32_t i = 0; i < TP_ECHOES; ++i) {
        tp_echo *x = &e->echo[i];
        tp_key *q = &e->keys[TP_ECHO0 + i];
        if (!run) { tp_echo_release(e, i); x->left = 0u; continue; }
        /* Its end (or the clock went back: the board's or a DAW's Start) */
        if (q->admitted && ((int32_t)(pos - x->off) >= 0 || (int32_t)(x->off - pos) > (int32_t)x->len)) tp_echo_release(e, i);
        if (!x->left || q->admitted) continue;
        if ((int32_t)(x->next - pos) > (int32_t)(2u * x->len)) x->next = pos + x->len; /* the clock went back */
        if ((int32_t)(pos - x->next) < 0) continue;
        if ((int32_t)(pos - x->next) > (int32_t)(x->len / 2u)) { x->left--; x->next += x->len; continue; } /* too late */
        uint8_t ch = tp_mpe_channel(e, x->channel, &x->notes); /* MPE (1.8.0): its own, not the pad's */
        if (tp_blocked(e, ch, &x->notes)) continue; /* an owed Note Off first: next pass */
        uint32_t v = (uint32_t)x->velocity * x->left / (x->total + 1u); /* left = N + 1 - n */
        tp_start(q, &x->notes, ch, (uint8_t)(v ? v : 1u), now); q->order = ++e->admissions;
        tp_own(e, q);
        x->off = x->next + x->len / 2u; x->next += x->len; x->left--;
    }
}
/* Mono (1.6.0, Layout > Mono): the melody half plays one pad at a time, the newest. An older pad still
 * held goes quiet once the newest one's notes have started (so a mono synth hears the two overlap and
 * plays them legato) and comes back when the pads after it are lifted. Not with the Drums layout. */
static void unhide(tp_engine *e, tp_key *key, uint32_t now) {
    if (tp_blocked(e, key->channel, &key->captured)) return; /* an owed Note Off first: next pass */
    key->hidden = false; key->silent = false; key->admitted_at = now;
    memset(key->note_pressure, 0, sizeof key->note_pressure);
    for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) key->note_at[i] = now;
    tp_own(e, key);
}
void tp_mono(tp_engine *e, uint32_t now) {
    bool on = tc_state.mono && e->layout.melody_layout != TP_MELODY_DRUMS;
    tp_key *newest = 0;
    for (uint32_t k = 48u; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->admitted || key->release_needed || key->glide) continue; /* Glide (1.8.0): the pair stays */
        if (!on) { if (key->hidden && e->layout.melody_layout != TP_MELODY_BASS) unhide(e, key, now); continue; }
        if (!newest || key->order > newest->order) newest = key;
    }
    /* A newer pad still held between its repeats (cut, not lifted) keeps the older ones quiet. */
    for (uint32_t k = 48u; on && k < TS_KEYS; ++k) {
        const tp_key *key = &e->keys[k];
        if (key->sensed && key->cut && key->repeating && !key->admitted && (!newest || key->order > newest->order)) return;
    }
    if (!newest) { tp_mono_drop(e); return; }
    if (newest->hidden) { unhide(e, newest, now); return; }
    for (uint32_t i = 0; i < newest->captured.count; ++i)
        if (!tp_voice_of(e, newest->channel, newest->captured.notes[i])->exposed) return; /* not started yet */
    tp_mono_drop(e); /* the lifted pad's notes end now that the one back has started */
    for (uint32_t k = 48u; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (key == newest || !key->admitted || key->release_needed || key->hidden || key->glide) continue;
        tp_disown(e, key); key->hidden = true; key->silent = true;
    }
}
/* Bass (1.7.0, the Bass layout): melody pad k struck on a note an older held melody pad plays (its pair's
 * twin, for two-finger runs) takes the note over. The older pad goes quiet as under Mono and stays so
 * until lifted (Mono on: until the pads after it are lifted); the note's Note Off goes out first
 * (admit_sources waits for it), then its Note On at the new strike's velocity. */
void tp_bass_take(tp_engine *e, uint32_t k, uint8_t ch, const tp_notes *notes) {
    for (uint32_t j = 48u; j < TS_KEYS; ++j) {
        tp_key *key = &e->keys[j];
        if (j == k || !key->admitted || key->silent || key->release_needed) continue;
        if (key->channel != ch && (tp_mpe_zone(ch) < 0 || tp_mpe_zone(key->channel) != tp_mpe_zone(ch))) continue; /* MPE: its zone */
        bool shared = false;
        for (uint32_t a = 0; a < key->captured.count && !shared; ++a)
            for (uint32_t b = 0; b < notes->count; ++b) shared = shared || key->captured.notes[a] == notes->notes[b];
        if (shared) { tp_disown(e, key); key->hidden = true; key->silent = true; }
    }
}
