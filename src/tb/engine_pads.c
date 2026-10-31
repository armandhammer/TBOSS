/* SPDX-License-Identifier: GPL-3.0-or-later - the engine (engine.h): reading the pads. A touch is sensed from the scan
 * against each pad's own resting level, with the velocity of how hard it starts; a session arms from
 * a frame where every pad reads (tp_arm). engine.c plays what is sensed. */
#include "engine_internal.h"
#include <string.h>
bool tk_config_ok(tk_config c) {
    if (c.press > 4095u || c.release > 4095u || c.full_scale > 4095u ||
        !c.deadband || c.deadband > 127u || c.pressure_ms < 20u || c.pressure_ms > 1000u) return false;
    if (c.polarity == 1u) return c.release < c.press && c.press < c.full_scale;
    if (c.polarity == 2u) return c.full_scale < c.press && c.press < c.release;
    return false;
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
tk_config tp_key_config(const tp_engine *e, uint32_t k) {
    uint32_t p = k < TS_KEYS ? tc_physical(&tc_state, k) : k; /* rests belong to the physical pad */
    return p < TS_KEYS && e->rested ? tc_pad_raise(e->config, e->rest[p]) : e->config;
}
bool tp_rest_range(const tp_engine *e, uint32_t *lo, uint32_t *hi) {
    uint32_t a = UINT32_MAX, b = 0u;
    if (!e->rested) return false;
    for (uint32_t k = 0; k < TS_KEYS; ++k) { if (e->rest[k] < a) a = e->rest[k]; if (e->rest[k] > b) b = e->rest[k]; }
    *lo = a; *hi = b; return true;
}
/* A resting pad's level follows its reading (1.5.0) every TC_PAD_REST_MS: down 1/32 of the way (about
 * 0.5 s; before 1.7.0 a quarter, so a tenth of a second of low readings took a pad's rest so far down
 * that its usual reading then played it), up 1/128 of the way (about 2 s; a press that stays below
 * Trigger for that long counts as rest), at least 1. */
static void follow_rest(tp_engine *e, uint32_t p, uint32_t v) {
    uint32_t r = e->rest[p];
    if (v < r) r -= (r - v + TC_PAD_REST_FALL - 1u) / TC_PAD_REST_FALL;
    else r += (v - r + TC_PAD_REST_RISE - 1u) / TC_PAD_REST_RISE;
    e->rest[p] = (uint16_t)(r > 4095u ? 4095u : r);
}
/* Each pad's resting level (1.5.0; before, one level for the board: the 12th highest pad), from a
 * frame: when a session starts (tc_arm) and on Settings > Pads > Rescan pads (1.7.0). A pad reading
 * 2000 or more is held, not resting: it starts from the board's middle level (at the start it stays
 * silent until let go, tp_arm); then its level follows it. */
void tp_measure_rest(tp_engine *e, const ts_scan *s) {
    uint8_t order[TS_KEYS];
    ts_strongest(s->values, order, TS_KEYS);
    uint32_t middle = s->values[order[TS_KEYS / 2u]];
    for (uint32_t k = 0; k < TS_KEYS; ++k) { /* k: the physical pad */
        uint32_t v = s->values[k];
        if (v >= TC_PAD_REST_CAP) v = middle < TC_PAD_REST_CAP ? middle : TC_PAD_REST_CAP;
        e->rest[k] = (uint16_t)v;
    }
    e->rested = true;
}
/* Chord pads stay sounding after the hand lifts with hold on, or feeding the arpeggiator
 * with its latch on. */
bool tp_latching(void) { return tc_state.hold || (tc_state.arp != TC_ARP_OFF && tc_state.arp_latch); }
bool tp_arm(tp_engine *e, tk_config c, const ts_scan *s, uint32_t request, uint32_t now) {
    if (e->state != TK_IDLE || !request || e->consumed_arm) return false;
    e->consumed_arm = request;
    tp_layout layout = tc_layout(&tc_state);
    if (!tk_config_ok(c) || !tc_layout_ok(layout) || !s->valid || !s->frame || now - s->last_ms > 100u) {
        tp_stop(e); return false;
    }
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_notes notes;
        if (s->values[k] > 4095u || !tc_pad(layout, k, &notes)) { tp_stop(e); return false; }
    }
    /* A pad that reads pressed now (held, or stuck on this board) never blocks the start:
     * it stays silent until it has read released once, and lights as pressed meanwhile. */
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        uint32_t p = tc_physical(&tc_state, k), v = s->values[p];
        tk_config kc = e->rested ? tc_pad_raise(c, e->rest[p]) : c; /* its own resting level (tc_arm) */
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
    key->sensed = true; key->rising = false; key->pressure = key->peak; key->raw = key->peak_raw;
    key->at = tc_feel_pressure(tc_state.feel, k >= 48u, key->pressure);
    /* 2.0.0: shut down, every pad silent until lifted (and woken). Shift + chord pad (1.0.0): chosen for the
     * chord edits, silent until lifted. */
    if (tc_state.dark == TC_SHUT_DOWN && !key->admitted) key->inhibited = true;
    else if (k < 48u && !key->admitted && tc_shift(&tc_state) && !tc_state.recording) key->inhibited = true;
    /* A latched pad touched again is released too and, being sensed, stays
     * silent until lifted (release_sources sets inhibited). */
    else if (k < 48u && (tp_latching() || key->latched)) hold_press(e);
}
/* Read every few ms (1.5.0), the first reading past Trigger is early in the press, too soft for its
 * velocity: the touch waits one more reading, unless it reads Full or its half plays a fixed velocity.
 * With the last reading below Trigger longer ago (TC_PAD_VELOCITY_MS: frames of 15 ms as before
 * 1.5.0) the press has had time to rise: sensed at once. */
static bool velocity_waits(const tp_engine *e, const tp_key *key, uint32_t k, uint32_t v, uint32_t now) {
    uint32_t lo, hi;
    return tp_velocity_span(e, k, &lo, &hi) && v < hi && (k < 48u ? tc_state.vel_mode : tc_state.melody_vel) != TC_VEL_FIXED &&
           now - key->low_at < TC_PAD_VELOCITY_MS;
}
static bool touched(tk_config c, uint32_t v) { return c.polarity == 1u ? v >= c.press : v <= c.press; }
/* Bleed (1.7.0): a press lifts the readings of the pads around it. Physical pad p reading v is a
 * neighbor's bleed, not a touch, while a pad next to it on the board (same half, a row or column
 * away) reads past its own Trigger and has risen more than twice as far above its rest. A pad
 * really pressed rises past half of its neighbor's rise and plays. */
static bool bleed(const tp_engine *e, const ts_scan *s, uint32_t p, uint32_t v) {
    tp_position at, near;
    if (!tp_position_of(p, &at)) return false;
    uint32_t rise = v > e->rest[p] ? v - e->rest[p] : 0u;
    for (uint32_t q = 0; q < TS_KEYS; ++q) {
        if (q == p || !tp_position_of(q, &near) || near.side != at.side || near.row + 1u < at.row ||
            near.row > at.row + 1u || near.column + 1u < at.column || near.column > at.column + 1u) continue;
        uint32_t w = s->values[q], r = e->rest[q];
        if (w > r && w - r > 2u * rise && touched(tp_key_config(e, tc_physical(&tc_state, q)), w)) return true;
    }
    return false;
}
/* Unstick (1.7.0, Settings > Pads): a touch also ends once its reading has stayed within
 * TC_PAD_SETTLE_BAND, below Trigger for TC_PAD_SETTLE_MS, at Trigger or above for TC_PAD_STILL_MS, and
 * that level becomes its rest (read_pad). A pad left reading high (a warped board: one settling higher
 * after a press, or one whose rest went too low) holds within a few steps; a finger wanders 50 to 75 or
 * more, so held notes and their aftertouch stay. A reading of TC_PAD_REST_CAP or more is a press, never
 * a stuck pad (a hard one can read a flat 4095). (Its first 1.7.0 form: a quarter of its rise, within
 * 600 of rest; a soft press, or one settling higher, stuck.) */
static void settling(tp_key *key, uint32_t v, uint32_t now) {
    uint32_t d = v > key->settle_ref ? v - key->settle_ref : key->settle_ref - v;
    if (!key->sensed || d > TC_PAD_SETTLE_BAND) { key->settle_ref = (uint16_t)v; key->settle_at = now; }
}
static bool unstuck(const tp_key *key, tk_config kc, uint32_t v, uint32_t now) {
    if (!tc_state.unstick || !key->sensed || kc.polarity != 1u || v >= TC_PAD_REST_CAP) return false; /* 2000 and up: */
    return now - key->settle_at >= (v < kc.press ? TC_PAD_SETTLE_MS : TC_PAD_STILL_MS); /* a press (maybe at 4095) */
}
/* Pad k (logical) reads v; follow: its resting level may follow the reading now. */
static void read_pad(tp_engine *e, const ts_scan *s, uint32_t k, uint32_t v, bool follow, uint32_t now) {
    tp_key *key = &e->keys[k]; uint32_t p = tc_physical(&tc_state, k); /* Chords on Right: halves traded */
    tk_config kc = tp_key_config(e, k); /* above its own resting level (1.7.0) */
    uint8_t level = pressure(kc, v);
    key->pressure = level; key->raw = (uint16_t)v; key->at = tc_feel_pressure(tc_state.feel, k >= 48u, level);
    settling(key, v, now);
    if (key->rising) { /* the reading after the touch's first */
        if (level > key->peak) key->peak = level;
        if (v > key->peak_raw) key->peak_raw = (uint16_t)v; /* 1.8.3: velocity's own, past Full too */
        sense(e, k); /* also when it reads released already: a touch that short still plays, */
        return;      /* and lets go at the next reading */
    }
    bool loose = !released(kc, v) && unstuck(key, kc, v, now);
    if (loose && v > e->rest[p]) e->rest[p] = (uint16_t)v; /* where it settled: its rest */
    if (loose || released(kc, v)) {
        key->sensed = false;
        if (key->admitted && !key->latched) {
            if (tp_latching() && k < 48u && !key->release_needed) key->latched = true;
            else key->release_needed = true;
        } else if (!key->admitted) { key->inhibited = false; key->cut = false; }
    } else if (!key->sensed && touched(kc, v) && !bleed(e, s, p, v)) {
        key->peak = level; key->peak_raw = (uint16_t)v;
        if (velocity_waits(e, key, k, v, now)) key->rising = true;
        else sense(e, k);
    }
    if (!key->sensed && !key->rising) key->low_at = now;
    /* Resting (not touched, and below Trigger): its resting level follows it. */
    if (follow && e->rested && kc.polarity == 1u && !key->sensed && !key->rising && !key->admitted && !key->inhibited &&
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
    if (tc_state.rescan) { tp_measure_rest(e, s); tc_state.rescan = false; } /* Rescan pads (1.7.0) */
    uint32_t follow = 0u; /* the positions whose resting levels follow now */
    for (uint32_t p = 0; p < TS_POSITIONS; ++p)
        if (((read >> p) & 1u) && now - e->rest_at[p] >= TC_PAD_REST_MS) { follow |= 1u << p; e->rest_at[p] = now; }
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        uint32_t p = tc_physical(&tc_state, k);
        if ((read >> (p % TS_POSITIONS)) & 1u) read_pad(e, s, k, s->values[p], ((follow >> (p % TS_POSITIONS)) & 1u) != 0u, now);
    }
    return true;
}
/* The readings pad k's velocity runs between: its Trigger (velocity 1) and Vel full (127), each above the
 * pad's own rest (1.8.3); Vel full = Full: its Full. False for inverted pads (polarity 2). */
uint8_t tp_trigger_pressure(const tp_engine *e, uint32_t k) { tk_config kc = tp_key_config(e, k); return pressure(kc, kc.press); }
bool tp_velocity_span(const tp_engine *e, uint32_t k, uint32_t *lo, uint32_t *hi) {
    tk_config kc = tp_key_config(e, k);
    if (kc.polarity != 1u) return false;
    uint32_t rest = kc.press - e->config.press, top = kc.full_scale; /* tc_pad_raise: the same rest */
    if (tc_state.pad_vel_full) top = rest + 100u * (TC_PAD_VEL_FULL_MIN + tc_state.pad_vel_full - 1u);
    *lo = kc.press; *hi = top > 4095u ? 4095u : top;
    if (*hi <= *lo) *hi = *lo + 1u;
    return true;
}
/* Note-on velocity from the touch's reading on the frame it first counts as pressed (the pad's raw
 * reading, 1.8.3; before, its pressure, which ends at Full): just past Trigger -> 1, Vel full -> 127.
 * Fixed mode -> 100. Each half has its own curve (0.11.0: Chord vel, Melody vel). */
uint8_t tp_velocity(const tp_engine *e, uint32_t k, bool melody) {
    uint8_t mode = melody ? tc_state.melody_vel : tc_state.vel_mode;
    if (mode == TC_VEL_FIXED) return 100u;
    const tp_key *key = &e->keys[k];
    uint32_t r, lo, hi;
    if (tc_state.pad_vel_full && tp_velocity_span(e, k, &lo, &hi)) { /* = Full: exactly as before 1.8.3 */
        uint32_t v = key->raw;
        r = v <= lo ? 0u : v >= hi ? 126u : (v - lo) * 126u / (hi - lo); /* 0..126 */
    } else {
        tk_config kc = tp_key_config(e, k); /* the pad's own Trigger is velocity 1 */
        uint32_t p = key->pressure, p0 = pressure(kc, kc.press);
        if (p <= p0 || p0 >= 127u) return 1u;
        r = (p - p0) * 126u / (127u - p0);
    }
    if (mode == TC_VEL_CUSTOM) { /* 1.9.0: the half's own curve, between its lowest and highest */
        const uint8_t *p = &tc_state.feel[TC_FEEL_VEL(melody)];
        uint32_t s = tc_feel_curve(p, r * 127u / 126u), low = p[TC_CURVE_POINTS], high = p[TC_CURVE_POINTS + 1u];
        uint32_t v = low + (high > low ? (high - low) * s / 127u : 0u);
        return (uint8_t)(v < 1u ? 1u : v > 127u ? 127u : v);
    }
    /* Light: a soft touch already plays louder; firm: it takes a harder strike, firmer and firmest
     * harder still (r squared, cubed, to the fourth). */
    if (mode == TC_VEL_LIGHT) r = 126u - (126u - r) * (126u - r) / 126u;
    else if (mode == TC_VEL_FIRM) r = r * r / 126u;
    else if (mode == TC_VEL_FIRMER) r = r * r * r / (126u * 126u);
    else if (mode == TC_VEL_FIRMEST) r = r * r * r * r / (126u * 126u * 126u);
    return (uint8_t)(1u + r);
}
