/* SPDX-License-Identifier: GPL-3.0-or-later - the step sequencer's core (lanes.h). */
#include "lanes.h"
#include <string.h>
ln_pattern ln;
uint16_t ln_dirty;
uint32_t ln_changes;
void ln_clear(ln_pattern *p) {
    memset(p, 0, sizeof *p);
    p->version = (uint8_t)LN_VERSION;
}
void ln_changed(uint32_t lane, uint32_t step) {
    if (lane >= LN_LANES) return;
    uint32_t chunk = step < LN_STEPS ? step / LN_CHUNK : 0u;
    ln_dirty = (uint16_t)(ln_dirty | 1u << (lane * LN_CHUNKS + chunk));
    ln_changes++;
}
uint32_t ln_length(const ln_lane *l) { return l->length && l->length <= LN_STEPS ? l->length : LN_LENGTH_DEFAULT; }
uint32_t ln_rate(const ln_lane *l) { return l->rate && l->rate <= LN_RATES ? l->rate - 1u : LN_RATE_DEFAULT; }
/* The defaults are stored as 0. */
void ln_set_length(ln_lane *l, uint32_t length) {
    l->length = length == LN_LENGTH_DEFAULT || !length || length > LN_STEPS ? 0u : (uint8_t)length;
}
void ln_set_rate(ln_lane *l, uint32_t rate) {
    l->rate = rate == LN_RATE_DEFAULT || rate >= LN_RATES ? 0u : (uint8_t)(rate + 1u);
}
bool ln_any(const ln_pattern *p) {
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        const ln_lane *l = &p->lanes[i];
        if (l->kind == LN_OFF || l->mute) continue;
        for (uint32_t s = 0; s < ln_length(l); ++s) if (l->steps[s].on) return true;
    }
    return false;
}
uint64_t ln_step_pack(ln_step s) {
    return (uint64_t)(s.on ? 1u : 0u) | (uint64_t)(s.value & 127u) << 1 | (uint64_t)(s.velocity & 127u) << 8 |
           (uint64_t)s.edit << 15 | (uint64_t)(s.scale & 127u) << 23 | (uint64_t)(s.gate & 15u) << 30 |
           (uint64_t)(s.probability & 15u) << 34 | (uint64_t)(s.ratchet & 7u) << 38 |
           (uint64_t)(s.condition & 31u) << 41 | (uint64_t)(s.nudge & 15u) << 46 | (uint64_t)(s.tie ? 1u : 0u) << 50;
}
static uint8_t bits(uint64_t w, uint32_t at, uint32_t n) { return (uint8_t)((w >> at) & ((1u << n) - 1u)); }
bool ln_step_unpack(uint64_t w, ln_step *out) {
    ln_step s;
    if (w >> 51) return false;
    s.on = w & 1u; s.value = bits(w, 1, 7); s.velocity = bits(w, 8, 7); s.edit = bits(w, 15, 8);
    s.scale = bits(w, 23, 7); s.gate = bits(w, 30, 4); s.probability = bits(w, 34, 4); s.ratchet = bits(w, 38, 3);
    s.condition = bits(w, 41, 5); s.nudge = bits(w, 46, 4); s.tie = (w >> 50) & 1u;
    if (((s.edit >> 3) & 3u) == 3u || s.scale > TC_ALL_SCALES) return false;
    *out = s;
    return true;
}
tc_chord ln_edit(uint8_t b) { return (tc_chord){(uint8_t)(b & 7u), (uint8_t)(2u * ((b >> 3) & 3u)), (uint8_t)(b >> 5)}; }
uint8_t ln_edit_byte(tc_chord e) { return (uint8_t)((e.lower & 7u) | (uint32_t)(e.sus / 2u) << 3 | (uint32_t)(e.add & 7u) << 5); }
void ln_encode(const ln_pattern *p, uint32_t slot, uint32_t w[LN_WORDS]) {
    memset(w, 0, LN_WORDS * 4u);
    w[0] = LN_VERSION;
    if (slot >= LN_RECORDS) return;
    uint32_t lane = slot / LN_CHUNKS, chunk = slot % LN_CHUNKS;
    const ln_lane *l = &p->lanes[lane];
    for (uint32_t i = 0; i < LN_CHUNK; ++i) {
        uint64_t s = ln_step_pack(l->steps[chunk * LN_CHUNK + i]);
        w[1u + 2u * i] = (uint32_t)s; w[2u + 2u * i] = (uint32_t)(s >> 32);
    }
    if (chunk) return;
    w[33] = l->kind | (uint32_t)l->channel << 8 | (uint32_t)l->length << 16 | (uint32_t)l->rate << 24;
    w[34] = l->direction | (uint32_t)l->swing << 8 | (uint32_t)l->mute << 16 | (uint32_t)(uint8_t)l->transpose << 24;
    w[35] = l->euclid;
    if (!lane) w[36] = p->seed;
}
static uint8_t byte_of(uint32_t w, uint32_t n) { return (uint8_t)(w >> (8u * n)); }
bool ln_decode(const uint32_t w[LN_WORDS], uint32_t slot, ln_pattern *p) {
    if (slot >= LN_RECORDS || w[0] != LN_VERSION) return false;
    uint32_t lane = slot / LN_CHUNKS, chunk = slot % LN_CHUNKS;
    for (uint32_t i = 37u; i < LN_WORDS; ++i) if (w[i]) return false;
    if (chunk && (w[33] || w[34] || w[35])) return false; /* a lane's own settings live in chunk 0 */
    if ((lane || chunk) ? w[36] != 0u : w[36] > 0xFFFFu) return false;
    ln_step steps[LN_CHUNK];
    for (uint32_t i = 0; i < LN_CHUNK; ++i)
        if (!ln_step_unpack(w[1u + 2u * i] | (uint64_t)w[2u + 2u * i] << 32, &steps[i])) return false;
    ln_lane *l = &p->lanes[lane];
    if (!chunk) {
        int8_t transpose = (int8_t)byte_of(w[34], 3);
        if (byte_of(w[33], 0) >= LN_KINDS || byte_of(w[33], 1) >= LN_CHANNELS || byte_of(w[33], 2) > LN_STEPS ||
            byte_of(w[33], 3) > LN_RATES || byte_of(w[34], 0) > 3u || byte_of(w[34], 1) > 50u ||
            byte_of(w[34], 2) > 1u || transpose < -48 || transpose > 48 || w[35] > LN_STEPS) return false;
        l->kind = byte_of(w[33], 0); l->channel = byte_of(w[33], 1); l->length = byte_of(w[33], 2);
        l->rate = byte_of(w[33], 3); l->direction = byte_of(w[34], 0); l->swing = byte_of(w[34], 1);
        l->mute = byte_of(w[34], 2); l->transpose = transpose; l->euclid = (uint8_t)w[35];
        if (!lane) p->seed = (uint16_t)w[36];
    }
    memcpy(&l->steps[chunk * LN_CHUNK], steps, sizeof steps);
    p->version = (uint8_t)LN_VERSION;
    return true;
}
bool ln_chunk_default(const ln_pattern *p, uint32_t slot) {
    uint32_t w[LN_WORDS];
    ln_encode(p, slot, w);
    for (uint32_t i = 1u; i < LN_WORDS; ++i) if (w[i]) return false;
    return true;
}
uint32_t ln_locate(uint32_t pos, rh_rate rate, uint32_t length, uint32_t *count) {
    uint32_t n = rh_step_at(pos, rate);
    if (count) *count = n;
    return length ? n % length : 0u;
}
uint32_t ln_gate_end(uint32_t count, rh_rate rate, uint32_t pct) {
    uint32_t start = rh_step_start(count, rate);
    return start + (rh_step_start(count + 1u, rate) - start) * pct / 100u; /* a step is at most 36 clocks */
}
uint8_t ln_pad_value(uint8_t kind, uint32_t pad) {
    pad %= 48u;
    if (kind != LN_DRUMS) return (uint8_t)pad;
    return (uint8_t)(TP_DRUM_BASE + 4u * (pad / 4u) + 3u - pad % 4u); /* row 0 is the top */
}
/* lowbias32 (Chris Wellons), chained over the inputs. */
static uint32_t mix(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
uint32_t ln_hash(uint32_t seed, uint32_t lane, uint32_t loop, uint32_t step) {
    return mix(mix(mix(mix(seed ^ 0x9E3779B9u) ^ lane) ^ loop) ^ step);
}
uint32_t ln_cycle(const ln_lane *l) {
    uint32_t n = ln_length(l);
    return l->direction == LN_PINGPONG && n > 1u ? 2u * n - 2u : n;
}
/* The i-th of n steps in a shuffled order (Fisher-Yates from key). */
static uint32_t shuffled(uint32_t n, uint32_t key, uint32_t i) {
    uint8_t order[LN_STEPS];
    for (uint32_t k = 0; k < n; ++k) order[k] = (uint8_t)k;
    for (uint32_t k = n - 1u; k > 0u; --k) {
        uint32_t j = mix(key ^ k) % (k + 1u);
        uint8_t t = order[k]; order[k] = order[j]; order[j] = t;
    }
    return order[i];
}
uint32_t ln_order(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *loop) {
    uint32_t n = ln_length(l), cycle = ln_cycle(l), i = count % cycle;
    if (loop) *loop = count / cycle;
    switch (l->direction) {
    case LN_REVERSE: return n - 1u - i;
    case LN_PINGPONG: return i < n ? i : cycle - i;
    case LN_RANDOM: return shuffled(n, ln_hash(seed, lane, count / cycle, LN_STEPS), i); /* step LN_STEPS: not a probability's */
    default: return i;
    }
}
bool ln_condition_ab(uint8_t condition, uint8_t *a, uint8_t *b) {
    static const uint8_t ab[9][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4}};
    if (condition < LN_FIRST_AB || condition >= LN_FIRST) return false;
    *a = ab[condition - LN_FIRST_AB][0]; *b = ab[condition - LN_FIRST_AB][1];
    return true;
}
uint32_t ln_probability_pct(uint8_t p) { return !p ? 100u : p < LN_PROBABILITIES ? 100u - 10u * p : 10u; }
uint32_t ln_gate_pct(const ln_step *s) { return s->gate && s->gate < LN_GATES ? 10u * s->gate : LN_GATE_PCT; }
uint32_t ln_strikes(const ln_step *s) { return (s->ratchet & 7u) + 1u; }
bool ln_sounds(const ln_step *s, uint32_t lane, uint32_t seed, uint32_t loop, uint32_t step) {
    uint8_t a, b;
    if (!s->on) return false;
    if (ln_condition_ab(s->condition, &a, &b) && loop % b != a - 1u) return false;
    if (s->condition == LN_FIRST && loop) return false;
    if (s->condition == LN_NOT_FIRST && !loop) return false;
    return !s->probability || ln_hash(seed, lane, loop, step) % 100u < ln_probability_pct(s->probability);
}
void ln_euclid_fill(ln_lane *l, uint32_t hits, uint32_t rotate, ln_step brush) {
    uint32_t n = ln_length(l);
    l->euclid = (uint8_t)(hits < n ? hits : n);
    if (!hits) return;
    uint64_t mask = rh_euclid(hits, n, rotate);
    brush.on = true;
    for (uint32_t i = 0; i < n; ++i) {
        ln_step *s = &l->steps[i];
        if (!((mask >> i) & 1u)) memset(s, 0, sizeof *s);
        else if (!s->on) *s = brush;
    }
}
