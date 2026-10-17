/* SPDX-License-Identifier: GPL-3.0-or-later - bounded source ownership, no heap/event bus.
 * Standalone: arms itself once USB is up, no host watchdog.
 * Ownership rule (unchanged): a source (pad or the arpeggiator) holds references
 * on (channel, pitch) voices; a voice's Note On goes out once while referenced and
 * its Note Off once all references are gone. One packet is in flight at a time and
 * every Note On reserves its Note Off before it is sent. */
#include "engine.h"
#include "controls.h"
#include "midi_in.h"
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
static bool chord_side(uint32_t k) { return k < 48u || k == TP_ARP; }
static uint8_t channel_of(uint32_t k) {
    return (uint8_t)((chord_side(k) ? tc_state.chord_ch : tc_state.melody_ch) & 15u);
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
        uint32_t v = s->values[k];
        e->keys[k].inhibited = !released(c, v);
        e->keys[k].sensed = c.polarity == 1u ? v >= c.press : v <= c.press;
    }
    e->config = c; e->layout = layout; e->frame = s->frame; e->frame_at = now; e->started = now;
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
bool tp_frame(tp_engine *e, const ts_scan *s, uint32_t now) {
    if (e->state != TK_ACTIVE || !s->valid || !s->frame || s->frame <= e->frame || now - s->last_ms > 100u)
        return false;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (s->values[k] > 4095u) return false;
    e->frame = s->frame; e->frame_at = now;
    bool hold = tc_state.hold != 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k]; uint32_t v = s->values[k];
        bool was = key->sensed;
        if (released(e->config, v)) {
            key->sensed = false;
            if (key->admitted && !key->latched) {
                if (hold && k < 48u && !key->release_needed) key->latched = true;
                else key->release_needed = true;
            } else if (!key->admitted) key->inhibited = false;
        } else if (e->config.polarity == 1u ? v >= e->config.press : v <= e->config.press) {
            key->sensed = true;
            /* A latched pad touched again is released too and, being sensed, stays
             * silent until lifted (release_sources sets inhibited). */
            if (!was && k < 48u && (hold || key->latched)) hold_press(e);
        }
        key->pressure = pressure(e->config, v);
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
    tp_voice *v = &e->voices[e->pending_channel][e->pending_pitch];
    if (e->pending_kind == TK_OFF && v->exposed) { v->exposed = false; e->exposed[e->pending_channel]--; }
    if (e->pending_kind == TK_CC && e->pending_group >= 2u) { /* sustain (CC64) */
        e->sustain_on[e->pending_group - 2u] = e->pending_pressure != 0u;
        e->sustain_ch[e->pending_group - 2u] = e->pending_channel;
    } else if (e->pending_kind == TK_CHANNEL || e->pending_kind == TK_CC) {
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
 * just past the press threshold -> 1, full scale -> 127. Fixed mode -> 100. */
static uint8_t velocity(const tp_engine *e, uint8_t p) {
    if (tc_state.vel_mode == TC_VEL_FIXED) return 100u;
    uint32_t p0 = pressure(e->config, e->config.press);
    if (p <= p0 || p0 >= 127u) return 1u;
    return (uint8_t)(1u + (p - p0) * 126u / (127u - p0));
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
    bool hold = tc_state.hold != 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (key->latched && !hold) { key->latched = false; key->release_needed = true; }
        if (!key->admitted || !key->release_needed) continue;
        if (!key->silent) disown(e, key);
        key->admitted = false; key->release_needed = false; key->latched = false; key->silent = false;
        key->inhibited = key->sensed;
    }
}
/* Strum: chord notes start one after another, low to high (up), high to low (down)
 * or alternating direction on each strike. due[i] is the delay of captured note i. */
static void strum(tp_engine *e, tp_key *key) {
    uint32_t mode = tc_state.strum, ms = tc_strum_ms[tc_state.strum_speed % TC_STRUM_SPEEDS];
    bool down = mode == TC_STRUM_DOWN || (mode == TC_STRUM_ALTERNATE && e->strum_down);
    if (mode == TC_STRUM_ALTERNATE) e->strum_down = !e->strum_down;
    for (uint32_t i = 0; i < key->captured.count; ++i) {
        uint32_t rank = 0;
        for (uint32_t j = 0; j < key->captured.count; ++j) {
            uint8_t a = key->captured.notes[i], b = key->captured.notes[j];
            if (b < a || (b == a && j < i)) rank++;
        }
        if (down) rank = key->captured.count - 1u - rank;
        key->due[i] = mode == TC_STRUM_OFF ? 0u : (uint16_t)(rank * ms);
    }
}
static void admit_sources(tp_engine *e, uint32_t now) {
    bool arp = tc_state.arp != TC_ARP_OFF;
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_key *key = &e->keys[k];
        if (!key->sensed || key->admitted || key->inhibited) continue;
        tp_notes notes;
        if (!tc_pad(e->layout, k, &notes)) { stop(e); return; }
        uint8_t ch = channel_of(k);
        bool silent = arp && k < 48u;
        if (!silent && blocked(e, ch, &notes)) continue; /* atomic whole-pad deferral, never steal cleanup */
        key->captured = notes; key->admitted = true; key->channel = ch; key->silent = silent;
        memset(key->note_pressure, 0, sizeof key->note_pressure);
        for (uint32_t i = 0; i < TP_MAX_NOTES; ++i) key->note_at[i] = now;
        key->admitted_at = now;
        key->velocity = velocity(e, key->pressure); key->order = ++e->admissions;
        memset(key->due, 0, sizeof key->due);
        if (k < 48u && !silent) strum(e, key);
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
        key->silent = false; key->admitted_at = now; memset(key->due, 0, sizeof key->due);
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
            for (uint32_t k = 0; k < 48u; ++k) {
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
        for (uint32_t k = 0; k < 48u; ++k) {
            const tp_key *key = &e->keys[k];
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
/* Step length in microseconds: from the external clock when it runs, else the tempo. */
static uint32_t arp_interval_us(bool clocked) {
    uint32_t ticks = tc_arp_ticks[tc_state.arp_rate % TC_ARP_RATES];
    if (clocked && tm.tick_us) return tm.tick_us * ticks;
    return 2500000u / tc_tempo_bpm(tc_state.tempo) * ticks; /* 60e6 / 24 ticks per beat */
}
static void arpeggiate(tp_engine *e, uint32_t now) {
    tp_key *a = &e->keys[TP_ARP];
    uint8_t pool[TP_ARP_POOL];
    uint32_t n = tc_state.arp != TC_ARP_OFF && e->state == TK_ACTIVE ? tp_arp_pool(e, pool) : 0u;
    uint32_t now_us = now * 1000u;
    bool clocked = tm_clock_active(now);
    if (!n) { arp_release(e); e->arp_running = false; e->arp_step = 0; return; }
    bool step_now;
    if (!e->arp_running) {
        step_now = true; e->arp_running = true; e->arp_step = 0;
        e->arp_due_us = now_us; e->arp_ticks = tm.ticks; e->arp_starts = tm.starts;
    } else if (clocked) {
        uint32_t per = tc_arp_ticks[tc_state.arp_rate % TC_ARP_RATES];
        if (tm.starts != e->arp_starts) { /* DAW Start: restart on the beat */
            e->arp_starts = tm.starts; e->arp_ticks = tm.ticks; e->arp_step = 0; step_now = true;
        } else {
            if (!e->arp_clocked) e->arp_ticks = tm.ticks; /* clock just appeared: count from here */
            step_now = tm.ticks - e->arp_ticks >= per;
        }
    } else step_now = (int32_t)(now_us - e->arp_due_us) >= 0;
    e->arp_clocked = clocked;
    uint32_t interval = arp_interval_us(clocked);
    if (!step_now) {
        /* Gate: the current note ends at its share of the step (100% = at the next step). */
        uint32_t gate = tc_arp_gate_pct[tc_state.arp_gate % TC_ARP_GATES];
        if (a->admitted && gate < 100u && (int32_t)(now_us - e->arp_off_us) >= 0) arp_release(e);
        return;
    }
    arp_release(e);
    uint8_t note = pool[arp_pick(e, n)];
    tp_notes one = {{note, 0, 0, 0, 0, 0}, 1};
    uint8_t ch = channel_of(TP_ARP);
    if (blocked(e, ch, &one)) return; /* previous Off still owed: retry next pass */
    a->captured = one; a->channel = ch; a->admitted = true; a->admitted_at = now;
    memset(a->due, 0, sizeof a->due);
    /* Velocity of the most recently struck chord pad. */
    uint32_t newest = 0; a->velocity = 100u;
    for (uint32_t k = 0; k < 48u; ++k)
        if (e->keys[k].admitted && e->keys[k].silent && e->keys[k].order >= newest) {
            newest = e->keys[k].order; a->velocity = e->keys[k].velocity;
        }
    own(e, a);
    e->arp_step++;
    e->arp_off_us = now_us + interval / 100u * tc_arp_gate_pct[tc_state.arp_gate % TC_ARP_GATES];
    if (clocked) {
        uint32_t per = tc_arp_ticks[tc_state.arp_rate % TC_ARP_RATES];
        if (tm.ticks - e->arp_ticks >= per) e->arp_ticks += per;
        if (tm.ticks - e->arp_ticks >= per) e->arp_ticks = tm.ticks; /* never burst to catch up */
    } else {
        e->arp_due_us += interval;
        if ((int32_t)(now_us - e->arp_due_us) > (int32_t)interval) e->arp_due_us = now_us + interval; /* fell behind */
    }
}
static uint8_t side_at(uint32_t melody) { return melody ? tc_state.at_melody : tc_state.at_chord; }
/* Channel pressure or a controller, one value per side: the strongest held pad (chords
 * include the arpeggiator's input pads). Both sides on one channel with the same
 * setting merge into one value. */
static bool group_pressure(tp_engine *e, uint32_t now, uint8_t *value, uint8_t *group, tk_kind *kind) {
    bool merged = (tc_state.chord_ch & 15u) == (tc_state.melody_ch & 15u) && side_at(0) == side_at(1);
    for (uint8_t g = 0; g < (merged ? 1u : 2u); ++g) {
        uint8_t mode = side_at(g), cc = tc_at_cc(mode);
        if (mode != TC_AT_CHANNEL && !cc) continue;
        uint32_t peak = 0u;
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            if (!merged && (k < 48u) != (g == 0u)) continue;
            if (e->keys[k].admitted && e->keys[k].pressure > peak) peak = e->keys[k].pressure;
        }
        uint32_t sent = e->group_sent[g], diff = peak > sent ? peak - sent : sent - peak;
        if (diff >= e->config.deadband && now - e->group_at[g] >= e->config.pressure_ms) {
            *value = (uint8_t)peak; *group = g; *kind = cc ? TK_CC : TK_CHANNEL; return true;
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
    for (uint8_t g = 0; g < 2u; ++g) {
        uint8_t ch = (uint8_t)((g ? tc_state.melody_ch : tc_state.chord_ch) & 15u);
        bool want = e->state == TK_ACTIVE && tc_state.sustain && !(merged && g);
        if (e->sustain_on[g] && (!want || e->sustain_ch[g] != ch)) {
            *c = (candidate_t){TK_CC, e->sustain_ch[g], 64u, 0u, 0u, (uint8_t)(2u + g)}; return true;
        }
        if (!e->sustain_on[g] && want) { *c = (candidate_t){TK_CC, ch, 64u, 0u, 127u, (uint8_t)(2u + g)}; return true; }
    }
    return false;
}
static bool candidate(tp_engine *e, uint32_t now, candidate_t *c) {
    for (uint8_t ch = 0; ch < 16u; ++ch) {
        if (!e->exposed[ch]) continue;
        for (uint32_t p = 0; p < 128u; ++p) {
            if (e->voices[ch][p].exposed && !e->voices[ch][p].refs) {
                *c = (candidate_t){TK_OFF, ch, (uint8_t)p, 0u, 0u, 0u}; return true;
            }
        }
    }
    if (sustain(e, c)) return true;
    if (e->state != TK_ACTIVE) return false;
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        tp_key *source = &e->keys[k];
        if (!source->admitted || source->silent) continue;
        for (uint32_t i = 0; i < source->captured.count; ++i) {
            uint8_t p = source->captured.notes[i];
            if (!voice(e, source->channel, p)->exposed && now - source->admitted_at >= source->due[i]) {
                *c = (candidate_t){TK_ON, source->channel, p, (uint8_t)k, source->velocity, (uint8_t)i}; return true;
            }
        }
    }
    uint8_t value, group; tk_kind kind;
    if (group_pressure(e, now, &value, &group, &kind)) {
        uint8_t ch = (uint8_t)((group ? tc_state.melody_ch : tc_state.chord_ch) & 15u);
        uint8_t cc = tc_at_cc(side_at(group));
        *c = (candidate_t){kind, ch, kind == TK_CC ? cc : value, 0u, value, group}; return true;
    }
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
    release_sources(e);
    if (e->state == TK_ACTIVE) { admit_sources(e, now); arp_membership(e, now); }
    if (e->state == TK_STOPPED) return;
    arpeggiate(e, now);
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
    uint32_t token = e->issued + 1u;
    if (!writer(packet, token)) return;
    e->issued = token; e->pending_id = token; e->pending_at = now;
    e->pending_pitch = c.pitch; e->pending_key = c.key; e->pending_kind = c.kind;
    e->pending_channel = c.channel; e->pending_group = c.group; e->pending_pressure = c.value;
    e->accepted++; e->waiting = false;
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
    const tk_config config = TK_DEFAULT_CONFIG;
    return tp_arm(e, config, s, arm_requests, now);
}
bool tc_frame(tp_engine *e, const ts_scan *s, uint32_t now) {
    bool was_sensed[48];
    for(uint32_t k=0;k<48u;k++) was_sensed[k]=e->keys[k].sensed;
    if(!tp_frame(e,s,now)) return false;
    for(uint32_t k=0;k<48u;k++) if(e->keys[k].sensed && !was_sensed[k]) {
        tc_state.last_chord=(uint8_t)k; tc_state.have_chord=true;
    }
    return true;
}
void tc_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer) {
    if(e->state==TK_ACTIVE || e->state==TK_DRAINING) session_at=now;
    else if((e->state==TK_DONE || e->state==TK_STOPPED) && now-session_at>=500u) {
        memset(e,0,sizeof *e); /* fresh TK_IDLE session; tp_arm takes the current layout */
        session_at=now;
    }
    tp_step(e,now,ready,writer);
}
