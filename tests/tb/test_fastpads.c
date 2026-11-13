/* SPDX-License-Identifier: GPL-3.0-or-later - Faster pads (1.5.0): the engine takes each scan
 * position's pads as they are read (two positions a pass) and the ADC converts faster (scan_hw.c:
 * 24 MHz, 112 sample clocks). Before, it took whole frames of ~15 ms, so a note sounded 0-30 ms after
 * the touch and a quick lift between hits could fall between two readings.
 * The real scanner (ts_step) runs on a fake ADC whose readings follow simulated fingers, in main.c's
 * loop order, on a microsecond clock: the old timing (41 us conversions, one position a pass, whole
 * frames) beside the new (5 us, two a pass, each position's pads at once). Every assumption of a
 * ~15 ms frame as a check: latency, velocity (one more reading), fast repeats, resting levels
 * following at the same speed in time, aftertouch pacing, a stalled scanner, the read counter
 * wrapping, Chords on the right. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/engine_internal.h" /* tp_velocity_span (2.4.4, board_reads) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
/* ---- the board: a microsecond clock, the pad multiplexer and the ADC ---- */
static uint32_t us, conversion_us, per_pass, rest_level = 300u, extra_us; /* extra_us: more to each pass (board_reads) */
static bool whole_frames, stalled;
static uint8_t position;
/* A finger on a pad: from `at`, rising to `peak` over `rise` us, held `len` us (0: until lifted),
 * falling back over `fall` us; wobble: the reading swings +-400 every ms while held. */
typedef struct { uint32_t at, rise, peak, len, fall; bool wobble; } finger;
static finger fingers[TS_KEYS];
static uint32_t reading(uint32_t k) {
    const finger *f = &fingers[k];
    if (!f->peak || us < f->at) return rest_level;
    uint32_t t = us - f->at;
    if (t < f->rise) return rest_level + (f->peak - rest_level) * t / f->rise;
    if (!f->len || t < f->len) return f->wobble && (t / 1000u) % 2u ? f->peak - 400u : f->peak;
    t -= f->len;
    if (t < f->fall) return f->peak - (f->peak - rest_level) * t / f->fall;
    return rest_level;
}
static void select_position(uint8_t p) { position = p; }
static bool sample(uint8_t bank, uint32_t timeout, uint16_t *v) {
    (void)timeout;
    if (stalled) return false;
    us += conversion_us; *v = (uint16_t)reading(16u * bank + position);
    return true;
}
static bool settle(uint32_t t) { us += t; return true; }
static uint32_t micros(void) { return ++us; }
static const ts_ops ops = {select_position, sample, settle, micros};
/* ---- the computer ---- */
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[8192];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint8_t ch = p[1] & 15u;
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]]++; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]] == 1); sounding[ch][p[2]]--; }
    if (logged < 8192u) log_[logged++] = (event){us, p[1], p[2], p[3]};
    return true;
}
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) n += (uint32_t)sounding[ch][p];
    return n;
}
static uint32_t count(uint8_t kind, uint32_t from) {
    uint32_t n = 0;
    for (uint32_t i = from; i < logged; ++i) n += (log_[i].status >> 4) == kind && (kind != TK_ON || log_[i].b);
    return n;
}
static const event *first(uint8_t kind, uint32_t from) {
    for (uint32_t i = from; i < logged; ++i) if ((log_[i].status >> 4) == kind) return &log_[i];
    return NULL;
}
/* ---- main.c's loop: the scanner, then (interrupts off) arm on a complete frame or take the pads
 * read, then the engine's step; the rest of a pass (USB, controls, display) takes 150 us ---- */
static void pass(void) {
    bool fresh = false;
    for (uint32_t i = 0; i < per_pass && !fresh; ++i) fresh = ts_step(&scan, ops, 50u, 1000u, us / 1000u);
    uint32_t now = us / 1000u;
    if (e.state == TK_IDLE) { if (fresh) (void)tc_arm(&e, &scan, now, true); }
    else if (fresh || !whole_frames) (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
    us += 150u + extra_us;
}
static void run(uint32_t ms) { uint32_t end = us + ms * 1000u; while ((int32_t)(us - end) < 0) pass(); }
static void start(bool fast, uint32_t reads) {
    tc_init(&tc_state); tc_state.unstick = 0u; /* readings held exactly constant: Unstick would take them for a stuck pad */ memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(fingers, 0, sizeof fingers);
    memset(sounding, 0, sizeof sounding); logged = 0u; rest_level = 300u; stalled = false; extra_us = 0u;
    scan.reads = reads;
    fast ? (conversion_us = 5u, per_pass = 2u, whole_frames = false) : (conversion_us = 41u, per_pass = 1u, whole_frames = true);
    us += 1000000u; /* a new session starts 500 ms after the last one */
    run(200); assert(e.state == TK_ACTIVE && !heard());
}
static void lift(uint32_t k) { fingers[k].peak = 0u; }
/* A frame's length on each timing: ~3.8 ms now, ~15 ms before. */
static uint32_t frame_us(bool fast) {
    start(fast, 0u);
    uint32_t f = scan.frame, at = us;
    run(300);
    return (us - at) / (scan.frame - f);
}
/* 48 touches on the melody pads (one note each), each at a different point of the scan: the time
 * from the finger landing (rising to `peak` in `rise` us) to its Note On, average and worst. */
static void latency(bool fast, uint32_t rise, uint32_t peak, uint32_t *average, uint32_t *worst, uint8_t *velocity) {
    start(fast, 0u);
    uint32_t total = 0u, most = 0u;
    for (uint32_t i = 0; i < 48u; ++i) {
        uint32_t k = 48u + i, from = logged;
        run(1u + (i * 7u) % 13u);
        us += (i * 263u) % 1000u; /* land anywhere in a pass */
        fingers[k] = (finger){us, rise, peak, 0u, 1000u, false};
        uint32_t at = us;
        while (!count(TK_ON, from) && us - at < 100000u) pass();
        const event *on = first(TK_ON, from);
        assert(on);
        uint32_t t = on->at - at;
        total += t; if (t > most) most = t;
        if (velocity) velocity[i] = on->b;
        lift(k); run(60);
    }
    assert(!heard());
    *average = total / 48u; *worst = most;
}
static void timing(void) {
    uint32_t fast_frame = frame_us(true), slow_frame = frame_us(false);
    assert(slow_frame >= 14000u && slow_frame <= 17000u); /* as measured on the board before 1.5.0 */
    assert(fast_frame >= 3000u && fast_frame <= 4500u);
    uint32_t old_avg, old_worst, new_avg, new_worst, fixed_avg, fixed_worst, full_avg, full_worst;
    uint8_t full_v[48];
    latency(false, 2000u, 3000u, &old_avg, &old_worst, NULL);
    latency(true, 2000u, 3000u, &new_avg, &new_worst, NULL);
    /* Before: about a frame on average, up to two. Now: the next reading of the pad, and one more
     * for its velocity. */
    assert(old_avg >= 12000u && old_worst >= 25000u);
    assert(new_avg <= 7000u && new_worst <= 2u * fast_frame + 1000u && 2u * new_avg < old_avg);
    /* Fixed velocity: nothing to wait for. */
    start(true, 0u);
    tc_state.melody_vel = TC_VEL_FIXED;
    uint32_t total = 0u, most = 0u;
    for (uint32_t i = 0; i < 48u; ++i) {
        uint32_t k = 48u + i, from = logged;
        run(1u + (i * 7u) % 13u); us += (i * 263u) % 1000u;
        fingers[k] = (finger){us, 2000u, 3000u, 0u, 1000u, false};
        uint32_t at = us;
        while (!count(TK_ON, from) && us - at < 100000u) pass();
        uint32_t t = first(TK_ON, from)->at - at; total += t; if (t > most) most = t;
        assert(first(TK_ON, from)->b == 100u);
        lift(k); run(60);
    }
    fixed_avg = total / 48u; fixed_worst = most;
    assert(fixed_avg < new_avg && fixed_worst <= fast_frame + 1500u);
    /* A touch reading Full at its first reading: no wait, velocity 127. */
    latency(true, 1u, 3800u, &full_avg, &full_worst, full_v);
    assert(full_worst <= fast_frame + 1000u);
    for (uint32_t i = 0; i < 48u; ++i) assert(full_v[i] == 127u);
    printf("frames %u/%u us; Note On after the touch: before avg %u worst %u us, now avg %u worst %u us "
           "(fixed velocity avg %u worst %u, Full worst %u)\n", (unsigned)slow_frame, (unsigned)fast_frame,
           (unsigned)old_avg, (unsigned)old_worst, (unsigned)new_avg, (unsigned)new_worst, (unsigned)fixed_avg,
           (unsigned)fixed_worst, (unsigned)full_worst);
}
/* A firm strike (3 ms to 3200, below Full above a 300 rest, then held) plays the velocity of 3200 at every point of the scan:
 * the second reading is past the rise. Without it the first reading past Trigger, early in the rise,
 * would set it (as low as velocity 1). */
static void velocity(void) {
    start(false, 0u);
    fingers[60] = (finger){us, 1u, 3200u, 0u, 1000u, false}; /* a step: the reference */
    uint32_t from = logged; run(40);
    uint8_t ref = first(TK_ON, from)->b;
    assert(ref > 90u && ref < 127u);
    lift(60); run(60);
    start(true, 0u);
    for (uint32_t i = 0; i < 48u; ++i) {
        uint32_t k = 48u + i; from = logged;
        run(1u + (i * 7u) % 13u); us += (i * 263u) % 1000u;
        fingers[k] = (finger){us, 3000u, 3200u, 0u, 1000u, false};
        run(30);
        assert(first(TK_ON, from)->b == ref);
        lift(k); run(60);
    }
    /* A slow press (40 ms to 3200) is a soft note, as before. */
    from = logged; fingers[70] = (finger){us, 40000u, 3200u, 0u, 1000u, false}; run(80);
    assert(first(TK_ON, from)->b < ref / 2u);
    lift(70); run(60);
}
/* Hammering one pad: 20 hits of 15 ms with 10 ms lifts. Now every hit is a note; before, a lift
 * could fall between two readings 15 ms apart and the hits merged. */
static uint32_t hammer(bool fast) {
    start(fast, 0u);
    uint32_t from = logged;
    for (uint32_t i = 0; i < 20u; ++i) {
        fingers[60] = (finger){us, 1000u, 2500u, 15000u, 1000u, false};
        run(26);
    }
    run(100); assert(!heard());
    assert(count(TK_ON, from) == count(TK_OFF, from));
    return count(TK_ON, from);
}
static void repeats(void) {
    uint32_t now = hammer(true), before = hammer(false);
    assert(now == 20u && before < 20u);
    printf("20 hits with 10 ms lifts: %u notes now, %u before\n", (unsigned)now, (unsigned)before);
}
/* Resting levels follow in time (TC_PAD_REST_MS), not per frame: a board lifted onto a lap (all
 * pads 400 -> 1000) is followed at the same speed with 4x the frames, and back on the desk too. */
static void rests(void) {
    uint32_t level[2], back[2];
    for (uint32_t fast = 0; fast < 2u; ++fast) {
        start(fast != 0u, 0u);
        rest_level = 400u; run(200);
        assert(e.rest[51] <= 410u);
        rest_level = 1000u; run(1000);
        level[fast] = e.rest[51];
        rest_level = 300u; run(2500);
        back[fast] = e.rest[51];
        assert(!count(TK_ON, 0u));
    }
    assert(level[0] > 550u && level[0] < 700u); /* 1/128 of the way every 15 ms: ~40 % in a second */
    assert(level[1] + 40u >= level[0] && level[1] <= level[0] + 40u);
    assert(back[0] <= 330u && back[1] <= 330u); /* down 1/32 of the way: within 2.5 s */
}
/* Channel aftertouch from a wobbling press: paced by time (pressure_ms, 50), not by readings. */
static void aftertouch(void) {
    start(true, 0u);
    tc_state.at_melody = TC_AT_CHANNEL;
    fingers[60] = (finger){us, 1000u, 2800u, 0u, 1000u, true};
    uint32_t from = logged;
    run(1000);
    uint32_t n = count(TK_CHANNEL, from);
    assert(n >= 15u && n <= 1000u / 50u + 2u);
    lift(60); run(200); assert(!heard());
}
/* The scanner stalls with a note held: the engine ends the session ~100 ms later (scanner stale),
 * every note ends; once it reads again a new session starts. A rejected frame's pads are not taken
 * until a complete frame. */
static void stall(void) {
    start(true, 0u);
    fingers[60] = (finger){us, 1000u, 3000u, 0u, 1000u, false};
    run(30); assert(heard() == 1u);
    stalled = true;
    uint32_t at = us, from = logged;
    while (heard() && us - at < 500000u) pass();
    assert(!heard() && us - at >= 100000u && us - at <= 130000u && first(TK_OFF, from));
    stalled = false; lift(60);
    run(1000); assert(e.state == TK_ACTIVE && !heard());
    fingers[61] = (finger){us, 1000u, 3000u, 0u, 1000u, false};
    run(30); assert(heard() == 1u);
    lift(61); run(60); assert(!heard());
}
/* The read counter wraps (about 8 days at ~4,300 positions a second): playing carries on. */
static void wrap(void) {
    start(true, UINT32_MAX - 300u);
    for (uint32_t i = 0; i < 6u; ++i) {
        uint32_t from = logged;
        fingers[50u + i] = (finger){us, 1000u, 3000u, 0u, 1000u, false};
        run(30); assert(count(TK_ON, from) == 1u);
        lift(50u + i); run(40); assert(!heard());
    }
    assert(scan.reads < 100000u && e.reads == scan.reads); /* wrapped */
}
/* Chords on the right: the physical pad read is the melody pad it now is. */
static void traded(void) {
    start(true, 0u);
    tc_state.chords_right = 1u; run(50);
    tp_notes n; assert(tc_pad(e.layout, 49u, &n) && n.count == 1u);
    uint32_t from = logged;
    fingers[1] = (finger){us, 1000u, 3000u, 0u, 1000u, false}; /* physical 1 = melody pad 49 */
    run(30);
    const event *on = first(TK_ON, from);
    assert(on && on->a == n.notes[0] && (on->status & 15u) == (tc_state.melody_ch & 15u) && count(TK_ON, from) == 1u);
    lift(1); run(60); assert(!heard());
}
/* 2.5.3: a frame walks only the pads of the positions read (12 of 96 on a pass of two positions; before, all 96 twice)
 * and marks in e->turned the pads whose touch began or ended, which is all tc_frame acts on (before, it copied and
 * compared all 96 every pass). Fingers on both halves, on the fast timing and with whole frames, the halves as printed
 * and traded: after every frame taken the marks are exactly the pads whose `sensed` changed, a pad whose position was
 * not read is untouched to the byte, and one that was read holds its own physical pad's reading. */
static void walked(void) {
    static tp_key before[TS_KEYS];
    uint32_t began = 0u, ended = 0u, frames = 0u, partial = 0u;
    for (uint32_t variant = 0; variant < 4u; ++variant) {
        start(variant < 2u, 0u);
        tc_state.chords_right = (uint8_t)(variant & 1u); run(50);
        uint32_t at = us, next = us, landed = 0u;
        while (us - at < 600000u) {
            if ((int32_t)(us - next) >= 0) { /* a finger every 7 ms, for 20 to 32 ms, anywhere on the board */
                uint32_t k = (landed * 37u + 5u) % TS_KEYS;
                fingers[k] = (finger){us, 1000u, 2600u, 20000u + 2000u * (landed % 7u), 1000u, false};
                landed++; next += 7000u;
            }
            bool fresh = false;
            for (uint32_t j = 0; j < per_pass && !fresh; ++j) fresh = ts_step(&scan, ops, 50u, 1000u, us / 1000u);
            uint32_t now = us / 1000u;
            if (fresh || !whole_frames) {
                memcpy(before, e.keys, sizeof before);
                uint32_t n = scan.reads - e.reads, read = 0u; /* the positions a frame takes, as tp_frame counts them */
                bool some = n && n < TS_POSITIONS && scan.last < TS_POSITIONS;
                for (uint32_t j = 0; j < (some ? n : TS_POSITIONS); ++j) read |= 1u << ((scan.last + TS_POSITIONS - j) % TS_POSITIONS);
                if (tc_frame(&e, &scan, now)) {
                    frames++; partial += some;
                    for (uint32_t k = 0; k < TS_KEYS; ++k) {
                        const tp_key *key = &e.keys[k];
                        bool turned = (e.turned[k / 32u] >> (k % 32u)) & 1u;
                        assert(turned == (key->sensed != before[k].sensed));
                        began += turned && key->sensed; ended += turned && !key->sensed;
                        uint32_t v = scan.values[tc_physical(&tc_state, k)];
                        if (!((read >> (k % TS_POSITIONS)) & 1u)) assert(!memcmp(&before[k], key, sizeof *key));
                        else assert(key->raw == v || (before[k].rising && key->raw > v)); /* a touch's second reading: its peak */
                    }
                }
            }
            tc_step(&e, now, true, writer);
            if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
            us += 150u;
        }
        assert(landed >= 80u);
        memset(fingers, 0, sizeof fingers); run(100); assert(!heard());
    }
    assert(frames > 1000u && partial > 1000u && began >= 150u && ended >= 150u);
    printf("frames walked: %u (%u of them a few positions), %u touches begun and %u ended, each marked for tc_frame and no other pad\n",
           (unsigned)frames, (unsigned)partial, (unsigned)began, (unsigned)ended);
}
/* The newest touch in the velocity trace (engine.h tp_trace, 2.4.4: entries 0-3, the last touches). */
static tp_trace_entry newest(void) {
    tp_trace_entry t = {0, 0, 0, 0};
    for (uint32_t i = 0; i < TP_TRACE_LAST; ++i)
        if (tp_trace[i].timing >> 16 > t.timing >> 16) { t.touch = tp_trace[i].touch; t.readings = tp_trace[i].readings; t.span = tp_trace[i].span; t.timing = tp_trace[i].timing; }
    return t;
}
/* When pad k's scan position is next read: two reads of it apart from the last (the passes are all alike here). */
static uint32_t next_read(uint32_t k) {
    uint32_t p = k % TS_POSITIONS, a = scan.position_us[p];
    while (scan.position_us[p] == a) pass();
    a = scan.position_us[p];
    uint32_t b = a;
    while (scan.position_us[p] == b) pass();
    b = scan.position_us[p];
    return b + (b - a);
}
/* Review TBR-032. On the board a pad is read every 10-17 ms (here a pass of about 1.8 ms, two scan positions a pass),
 * so velocity_waits' 1.5.0 guard, which waits a reading only when the pad's last low reading was under
 * TC_PAD_VELOCITY_MS (8) before, never applied: a hard strike (3 ms up to 3400, near Full) whose first reading landed
 * just past Trigger played velocity 1-5, its next reading near the top (2.4.4 pinned that here, with the velocity trace
 * an SWD inspect prints; the board showed it on 2.5.3). 2.5.5: a first reading in the lowest quarter of the velocity
 * span (TC_PAD_VELOCITY_LOW) waits one more reading: the strike plays what it reaches, a soft touch stays soft, and a
 * first reading above that quarter plays at once, as before. */
static void board_reads(void) {
    /* the trace on the fast timing first: a strike's velocity, the Note On's */
    start(true, 0u);
    uint32_t from = logged;
    fingers[60] = (finger){us, 3000u, 3200u, 0u, 1000u, false}; run(30);
    tp_trace_entry t = newest();
    assert(first(TK_ON, from) && ((t.touch >> 7) & 127u) == first(TK_ON, from)->b && (t.touch & 127u) == 60u);
    assert(((t.touch >> 15) & 1u) && (t.readings >> 16) >= 3000u && (t.span & 0xFFFFu) == 1300u);
    lift(60); run(60);
    start(true, 0u);
    extra_us = 1350u; run(300); /* a pass of about 1.8 ms, as on the board */
    uint32_t f = scan.frame, at = us; run(600);
    uint32_t frame = (us - at) / (scan.frame - f);
    assert(frame >= 13000u && frame <= 17000u);
    uint32_t lo, hi, k = 61u, low10 = 0u, strikes = 0u, waited = 0u, at_once = 0u, latest = 0u; uint8_t lowest = 127u, highest = 0u;
    tp_trace_reset();
    assert(tp_velocity_span(&e, k, &lo, &hi) && lo == 1300u && hi > lo + 4u * 300u);
    uint32_t quarter = lo + (hi - lo) / TC_PAD_VELOCITY_LOW;
    uint32_t cross = 3000u * (lo - rest_level) / (3400u - rest_level); /* us from landing to Trigger */
    for (uint32_t d = 0; d <= 600u; d += 20u, ++strikes) { /* Trigger crossed d us before the read */
        uint32_t when = next_read(k);
        from = logged;
        fingers[k] = (finger){when - d - cross, 3000u, 3400u, 0u, 1000u, false};
        run(60);
        const event *on = first(TK_ON, from);
        assert(on);
        t = newest();
        uint32_t first_reading = t.readings & 0xFFFFu, next = t.readings >> 16, gap = t.touch >> 16;
        bool wait = (t.touch >> 14) & 1u;
        assert((t.touch & 127u) == k && ((t.touch >> 7) & 127u) == on->b && ((t.touch >> 15) & 1u));
        assert(gap >= 13u && first_reading >= lo); /* the last low reading a whole read interval back: the 1.5.0 guard is out of it */
        assert(wait == (first_reading < quarter)); /* 2.5.5: the lowest quarter waits, whatever the time */
        if (wait) { /* caught on the rise: the next reading near the top, and the strike plays that */
            assert(next >= 3300u && on->b >= 100u);
            assert(on->at >= when && on->at - when <= frame + 4000u); /* one reading later, no more */
            if (on->at - when > latest) latest = on->at - when;
            waited++;
        } else { /* read higher: plays on that reading, as before (a strike landing just after the read is first read a frame on, at the top) */
            assert(on->at - when <= (first_reading >= 3300u ? frame + 4000u : 4000u));
            at_once++;
        }
        low10 += on->b <= TP_TRACE_LOW;
        if (on->b < lowest) lowest = on->b;
        if (on->b > highest) highest = on->b;
        lift(k); run(60);
    }
    assert(waited >= 20u && at_once >= 1u && !low10 && lowest >= 30u && highest >= 100u); /* none near-silent (2.4.4: 4 at 5 or less) */
    for (uint32_t i = TP_TRACE_LAST; i < TP_TRACE; ++i) assert(!(tp_trace[i].timing >> 16)); /* nothing in the trace's low entries */
    /* a soft touch (up to 150 past Trigger) waits a reading too and stays soft; so does one that keeps rising slowly */
    uint32_t when = next_read(k);
    from = logged;
    fingers[k] = (finger){when - 3000u, 3000u, lo + 150u, 0u, 1000u, false};
    run(60);
    const event *soft = first(TK_ON, from);
    t = newest();
    assert(soft && soft->b >= 1u && soft->b <= 12u && ((t.touch >> 14) & 1u) && ((t.touch >> 7) & 127u) == soft->b);
    uint8_t soft_velocity = soft->b;
    lift(k); run(60);
    /* Fixed velocity never waits: there is nothing to wait for */
    tc_state.melody_vel = TC_VEL_FIXED;
    when = next_read(k);
    from = logged;
    fingers[k] = (finger){when - 3000u, 3000u, lo + 150u, 0u, 1000u, false};
    run(60);
    t = newest();
    assert(first(TK_ON, from) && first(TK_ON, from)->b == 100u && !((t.touch >> 14) & 1u));
    lift(k); run(60);
    assert(!heard());
    printf("TBR-032 (fixed in 2.5.5): pads read every %u us, %u hard strikes landing at every point of a read (Trigger crossed 0-600 us before it): velocity %u-%u, "
           "none at 10 or less; %u first read in the lowest quarter of the span waited a reading (Note On at most %u us after that read), %u played at once; "
           "a soft touch still plays velocity %u; Fixed never waits\n", (unsigned)frame, (unsigned)strikes, (unsigned)lowest,
           (unsigned)highest, (unsigned)waited, (unsigned)latest, (unsigned)at_once, (unsigned)soft_velocity);
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    timing(); velocity(); repeats(); rests(); aftertouch(); stall(); wrap(); traded(); walked(); board_reads();
    puts("PASS: fastpads: each scan position's pads taken as read, two positions a pass with the faster ADC "
         "(a frame ~3.8 ms, was ~15): Note On latency avg/worst down from ~15/~30 ms, a touch waits one more "
         "reading for its velocity (a firm strike plays the same velocity at every point of the scan; Full and "
         "fixed velocity don't wait), 20 hits with 10 ms lifts all play (merged before), resting levels follow "
         "in time as before, aftertouch paced by time, a stalled scanner ends the session in ~100 ms and a new "
         "one starts, the read counter wraps, Chords on the right");
    return 0;
}
