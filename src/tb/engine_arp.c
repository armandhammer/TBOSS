/* SPDX-License-Identifier: GPL-3.0-or-later - the engine (engine.h): the arpeggiator, one source (TP_ARP) that
 * plays the notes of the chord pads (and the progression) feeding it, on the clock. */
#include "engine_internal.h"
#include "midi_in.h"
#include <string.h>
/* Chord pads change role when the arpeggiator is switched while they are held. */
/* A chord shape button went down or up (1.7.0): the chord pads feeding the arpeggiator (silent) take
 * their notes as a touch now would; the arpeggio plays them from its next step. */
void tp_arp_reshape(tp_engine *e) {
    tc_state.reshape = false;
    if (tc_state.arp == TC_ARP_OFF) return;
    for (uint32_t k = 0; k < 48u; ++k) {
        tp_key *key = &e->keys[k];
        tp_notes notes;
        if (!key->admitted || !key->silent || key->release_needed || ((tc_state.chord_mute >> k) & 1u)) continue;
        if (tc_pad(e->layout, k, &notes)) key->captured = notes;
    }
}
void tp_arp_membership(tp_engine *e, uint32_t now) {
    bool arp = tc_state.arp != TC_ARP_OFF;
    for (uint32_t k = 0; k < 48u; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->admitted || key->release_needed || key->silent == arp) continue;
        if (arp) { tp_disown(e, key); key->silent = true; continue; }
        if (tp_blocked(e, key->channel, &key->captured)) continue;
        key->silent = false; key->admitted_at = now; memset(key->due, 0, sizeof key->due); memset(key->feel, 0, sizeof key->feel);
        tp_own(e, key);
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
static uint32_t arp_random(tp_engine *e) {
    e->arp_random ^= e->arp_random << 13; e->arp_random ^= e->arp_random >> 17; e->arp_random ^= e->arp_random << 5;
    return e->arp_random;
}
/* The Turing pattern (2.0.0), at each step: other held notes than the loop was drawn for draw a new loop; then
 * the loop's step is drawn again unless Lock keeps it, and is the one to sound. */
static void turing_step(tp_engine *e, const uint8_t *pool, uint32_t n) {
    uint32_t hash = 2166136261u ^ n; /* FNV-1a over the held notes, low to high */
    for (uint32_t i = 0; i < n; ++i) { hash ^= pool[i]; hash *= 16777619u; }
    if (hash != e->turing_notes) {
        e->turing_notes = hash;
        for (uint32_t i = 0; i < TP_TURING; ++i) e->turing[i] = (uint8_t)arp_random(e);
    }
    uint32_t at = e->arp_step % (TC_ARP_LOOP_MIN + tc_state.arp_loop % TC_ARP_LOOPS);
    uint32_t lock = 10u * (tc_state.arp_lock % TC_ARP_LOCKS);
    if (lock < 100u && arp_random(e) % 100u >= lock) e->turing[at] = (uint8_t)arp_random(e);
    e->turing_pick = e->turing[at];
}
static uint32_t arp_pick(tp_engine *e, uint32_t n) {
    uint32_t step = e->arp_step;
    switch (tc_state.arp) {
    case TC_ARP_TURING: return e->turing_pick % n;
    case TC_ARP_DOWN: return n - 1u - step % n;
    case TC_ARP_UPDOWN: {
        if (n < 2u) return 0u;
        uint32_t period = 2u * n - 2u, p = step % period;
        return p < n ? p : period - p;
    }
    case TC_ARP_RANDOM: return arp_random(e) % n;
    default: return step % n;
    }
}
void tp_arp_release(tp_engine *e) {
    tp_key *a = &e->keys[TP_ARP];
    if (a->admitted) { tp_disown(e, a); a->admitted = false; }
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
void tp_arpeggiate(tp_engine *e, uint32_t now) {
    tp_key *a = &e->keys[TP_ARP];
    uint8_t pool[TP_ARP_POOL];
    uint32_t n = tc_state.arp != TC_ARP_OFF && e->state == TK_ACTIVE ? tp_arp_pool(e, pool) : 0u;
    bool clocked = tm_clock_active(now);
    uint32_t pos = tp_clock_pos(e, now);
    uint8_t rate_at = (uint8_t)(tc_state.arp_rate % TC_ARP_RATES), swing_at = (uint8_t)(tc_state.arp_swing % TC_SWINGS);
    rh_rate rate = tc_arp_rates[rate_at];
    uint32_t swing = tc_swing_pct[swing_at], len = rh_step_start(1u, rate);
    if (!n) { tp_arp_release(e); e->arp_running = false; e->arp_pending = false; e->arp_step = 0; return; }
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
        tp_arp_release(e);
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
        if (tc_state.arp == TC_ARP_TURING) turing_step(e, pool, n); /* silent steps move the loop on too */
        e->arp_pending = ((rhythm >> (e->arp_step % steps)) & 1u) && tp_chance(e, now); /* Chance (1.6.0) */
        e->arp_play = e->arp_step++;
    } else if (a->admitted && gate < 100u && (int32_t)(pos - e->arp_off_at) >= 0) tp_arp_release(e);
    if (!e->arp_pending) return;
    tp_notes play; memset(&play, 0, sizeof play);
    if (tc_state.arp == TC_ARP_CHORDS) { /* the whole held chord every step */
        for (uint32_t i = 0; i < n && play.count < TP_MAX_NOTES; ++i) play.notes[play.count++] = pool[i];
    } else {
        uint32_t step = e->arp_step; e->arp_step = e->arp_play;
        play.notes[0] = pool[arp_pick(e, n)]; play.count = 1;
        e->arp_step = step;
    }
    uint8_t ch = tp_mpe_channel(e, tc_channel_of(&tc_state, TP_ARP), &play); /* MPE (1.8.0): a channel each step */
    if (tp_blocked(e, ch, &play)) return; /* previous Off still owed: retry next pass */
    e->arp_pending = false;
    /* Velocity of the most recently struck chord pad (or progression chord). */
    uint32_t newest = 0; uint8_t velocity = 100u;
    for (uint32_t i = 0; i < 49u; ++i) {
        const tp_key *key = &e->keys[i < 48u ? i : TP_SEQ];
        if (key->admitted && key->silent && key->order >= newest) { newest = key->order; velocity = key->velocity; }
    }
    tp_start(a, &play, ch, velocity, now);
    tp_strum(e, a, tc_state.arp == TC_ARP_CHORDS && tc_state.strum != TC_STRUM_OFF, now);
    tp_own(e, a);
}
