/* SPDX-License-Identifier: GPL-3.0-or-later - main.c's loop on the computer (0.11.0): 1.9.4's inline
 * replies (a read, the editor's STATE check and a backup's burst come back between two notes: nothing
 * stops, no note inside a reply), the web editor's pause for a request that writes (the engine drains,
 * the reply streams, a second of quiet, playing resumes on the same chord), and the USB cable pulled
 * in the middle of a long reply, with a progression playing throughout. main.c runs unchanged
 * against stand-ins for TinyUSB, the pad scanner and the touch bus (tests/tb/main_hal);
 * tb_snapshot(), called at the end of every pass, moves time on and plays the computer.
 * 2.4.2 (review TBR-012): U3 held as Shift on the touch bus stays held through a latched bus fault and
 * main.c's recovery, at ten different points of the 10 ms poll cycle.
 * 2.4.3 (review TBR-015): a finished session is cleared for a fresh one after main.c's masked pass, with
 * interrupts on, never inside it (every restart of the run is watched).
 * 2.4.5 (review TBR-007): a whole pattern changed is saved one record a pass; main.c keeps the longest call.
 * 2.4.7 (review TBR-008): it starts on a log past half full: the warning, then the erase not before 1,500 ms.
 * 2.4.8 (review TBR-010): the stack's high-water mark follows a painted word changed deep down.
 * 2.4.8 (review TBR-023): built again with -DTB_FUZZ_MAIN=1 (the suite fuzz_main), the same stand-ins are driven at
 * random instead of the scenario: a host that polls late, unplugs, suspends and resumes, the editor's requests (inline,
 * pausing, the 2 KiB backup chunk, bad checksums, cut short) on either cable, lanes of up to 20 notes a step, the
 * transport and TRS out turned on and off. After every pass: no session ends except after a loss the run caused (a
 * receipt left in the ring or taken for the wrong batch would end one), every reply well formed and in time, nothing
 * inside a reply, no Note On twice; at the end, everything silent and playing again. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/i2c.h"
#include "../../src/tb/diagnostic.h"
#include "../../src/tb/lanes.h"
#include "stm32f4xx.h" /* main_hal's: the cycle counter (2.4.5) */
#include "tusb.h"
#include "device/dcd.h"
#include "device/usbd_pvt.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
void usb_main(void);
void tb_usb_event_queued(dcd_event_t const *event);
void tb_midi_tx_result(uint8_t itf, uint32_t count, bool success);
void tud_mount_cb(void);
void tud_suspend_cb(bool wakeup);
uint32_t test_primask, SystemCoreClock = 48000000u;
uint32_t tb_stack_paint[TB_PAINT_WORDS]; /* 2.4.8 (TBR-010): painted in main(), as startup.S does */
void test_halt(void) { printf("main.c stopped: fault %x\n", (unsigned)tb_status.fault); assert(!"main.c failed"); }
/* ---- the board's other parts ---- */
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
/* 2.4.7 (TBR-008): from the warning to the erase, each tb_millis() call moves time on 1 ms (main.c waits on it). */
static uint32_t us; /* simulated time, with the USB stand-ins below */
static bool booting;
static uint32_t warnings, erase_us = UINT32_MAX;
bool fs_erase(void) {
    if (booting) { erase_us = us; booting = false; }
    memset(flash, 0xFF, sizeof flash); return true;
}
tb_test_dwt test_dwt; /* 2.4.5: main.c times the settings calls; only programming moves it on here */
#define WORD_CYCLES (16u * 48u) /* a word takes 16 us to program (datasheet, typical) */
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    test_dwt.CYCCNT += count * WORD_CYCLES;
    return true;
}
bool nr_init(void) { return true; }
bool nr_present(void) { return true; }
void nr_id(uint8_t id[3]) { id[0] = 0xEF; id[1] = 0x40; id[2] = 0x18; }
uint32_t nr_size(void) { return 1u << 24; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) out[i] = (uint8_t)((a + i) * 7u);
    return true;
}
void jh_init(void) {}
void jh_kick(void) {}
uint32_t jh_levels(void) { return 0u; }
bool ui_init(void) { return true; }
void ui_saving_settings(void) { warnings++; booting = true; }
void td_wait(void) {}
static const tp_engine *engine; /* main.c's, for its error count (2.3.3) */
/* 2.4.3 (TBR-015): the engine's state when main.c's masked pass begins (its first masked call here is tud_mounted)
 * and when interrupts come on again after it; a session that had ended and is fresh (TK_IDLE) at the pass's end was
 * restarted in this pass: inside the masked pass if it was fresh already when interrupts came on. */
static bool in_pass, unmasked_seen;
static tk_state state_masked, state_unmasked;
static uint32_t restarts, restarts_masked;
void test_unmasked(void) {
    if (in_pass && !unmasked_seen) { unmasked_seen = true; state_unmasked = engine->state; }
}
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb_ready, uint32_t now) {
    (void)c; (void)s; (void)usb_ready; (void)now;
    if (in_pass && unmasked_seen && (state_masked == TK_DONE || state_masked == TK_STOPPED) && e->state == TK_IDLE) {
        if (state_unmasked == TK_IDLE) restarts_masked++; else restarts++;
    }
    in_pass = false; unmasked_seen = false;
    engine = e;
}
static uint32_t micros_now;
static void select_position(uint8_t p) { (void)p; }
static bool sample(uint8_t bank, uint32_t timeout, uint16_t *value) { (void)bank; (void)timeout; *value = 0u; return true; }
static bool settle(uint32_t wait_us) { micros_now += wait_us; return true; }
static uint32_t micros(void) { return micros_now += 7u; }
bool ts_hardware_init(void) { return true; }
const ts_ops ts_hardware_ops = {select_position, sample, settle, micros};
static tc_bus *bus_of_main; /* main.c's touch bus (2.4.2: TBR-012) */
static bool buttons, u3_down;  /* the touch controllers answer (2.4.2); U3 (0x38 bit 12) held */
static uint32_t recoveries;
bool tc_hardware_init(tc_bus *b) { memset(b, 0, sizeof *b); bus_of_main = b; return false; } /* no buttons: pads still play */
bool tc_hardware_recover(tc_bus *b) { b->latched = false; recoveries++; return true; }
struct bi_reply tc_i2c_read(void *context, uint8_t address, uint8_t reg, uint32_t generation, uint32_t now, uint32_t budget) {
    (void)reg; (void)budget;
    struct bi_reply r = {BI_NACK, 0u, generation, now, 100u, {0u, 0u}};
    if (!buttons) return r;
    if (((tc_bus *)context)->latched) { r.status = BI_BLOCKED; return r; } /* i2c.c refuses every read while latched */
    r.status = BI_OK; r.count = 2u; r.data[1] = (uint8_t)(address == 0x38u && u3_down ? 0x10u : 0u); /* bit 12: U3 */
    return r;
}
/* ---- the USB stack at full speed: one transfer of up to 16 packets (64 bytes) per 1 ms frame;
 * time passes while main.c waits for room (us: simulated time, a pass is 1 ms). 2.3.3: as TinyUSB's
 * midi_device.c, a full 64-byte transfer with nothing after it is ended by a zero-length packet, which
 * completes (len 0) in a frame of its own; packets written meanwhile go in the frame after ---- */
static uint32_t us, inflight, frame_ms = UINT32_MAX, zlps;
static bool zlp;
#ifndef TB_FUZZ_MAIN
#define TB_FUZZ_MAIN 0
#endif
static const bool fuzzing = TB_FUZZ_MAIN; /* 2.4.8 (TBR-023): the suite fuzz_main */
static uint32_t rng = 0x2480F00Du;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static bool suspended; /* the host asleep: nothing polled until it resumes */
/* 2.4.8: as on the board, a completion is two steps: the interrupt queues it (tb_usb_event_queued, at any moment
 * interrupts are on) and tud_task handles it later (the endpoint free again, midi_device's tb_midi_tx_result). */
static bool hw_done; static uint32_t hw_len;
static void usb_isr(void);
#define ms (us / 1000u)
static bool mounted = true;
static uint8_t inq[512][4];
static uint32_t in_r, in_w;
uint32_t tb_millis(void) { if (booting) us += 1000u; return ms; }
uint32_t tb_board_init(void) { return 0u; }
bool tusb_init(uint8_t rhport, const tusb_rhport_init_t *init) { (void)rhport; (void)init; return true; }
bool tud_mounted(void) {
    usb_isr();
    if (test_primask && engine && !in_pass) { in_pass = true; state_masked = engine->state; } /* the masked pass begins */
    return mounted;
}
bool tud_suspended(void) { return suspended; }
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep) { (void)rhport; (void)ep; usb_isr(); return inflight != 0u || zlp; }
bool tud_midi_packet_read(uint8_t p[4]) {
    if (in_r == in_w) return false;
    memcpy(p, inq[in_r++ % 512u], 4); return true;
}
static void computer(const uint8_t p[4]);
bool tud_midi_packet_write(const uint8_t p[4]) {
    if (!mounted || inflight >= 16u) return false;
    inflight++; computer(p); return true;
}
uint32_t tud_midi_n_packet_write_n(uint8_t itf, const uint8_t packets[], uint32_t n) { /* 2.2.0: main.c's batch */
    (void)itf;
    uint32_t i = 0;
    while (i < n && tud_midi_packet_write(&packets[4u * i])) i++;
    return i;
}
static void usb_isr(void) { /* fuzz_main: the IN transfer completes, at most once a frame, sometimes later */
    if (!fuzzing || test_primask || !mounted || suspended || hw_done || (!inflight && !zlp) || ms == frame_ms || rnd() % 3u) return;
    frame_ms = ms; hw_done = true; hw_len = zlp ? 0u : 4u * inflight;
    dcd_event_t ev; memset(&ev, 0, sizeof ev);
    ev.event_id = DCD_EVENT_XFER_COMPLETE; ev.xfer_complete.ep_addr = 0x81u;
    ev.xfer_complete.result = XFER_RESULT_SUCCESS; ev.xfer_complete.len = hw_len;
    tb_usb_event_queued(&ev);
}
static void usb_task_done(void) { /* tud_task: the endpoint free, the callback; a full 64 bytes then a zero-length packet */
    hw_done = false;
    if (!hw_len) { zlp = false; zlps++; tb_midi_tx_result(0u, 0u, true); return; }
    uint32_t n = hw_len / 4u; inflight -= n;
    tb_midi_tx_result(0u, hw_len, true);
    zlp = n == 16u && !inflight;
}
void tud_task_ext(uint32_t timeout_ms, bool in_isr) {
    (void)timeout_ms; (void)in_isr;
    if (fuzzing) { /* the interrupt, then its handling, each when it happens to come (time passes while waiting) */
        usb_isr();
        if (hw_done && rnd() % 4u) usb_task_done();
        else if (inflight || zlp || hw_done) us += 100u;
        return;
    }
    if ((!inflight && !zlp) || !mounted) return;
    if (ms == frame_ms) { us += 100u; return; } /* this frame's transfer has gone */
    frame_ms = ms;
    dcd_event_t ev; memset(&ev, 0, sizeof ev);
    ev.event_id = DCD_EVENT_XFER_COMPLETE; ev.xfer_complete.ep_addr = 0x81u;
    ev.xfer_complete.result = XFER_RESULT_SUCCESS;
    if (zlp) { /* the zero-length packet completes */
        zlp = false; zlps++;
        tb_usb_event_queued(&ev);
        tb_midi_tx_result(0u, 0u, true);
        return;
    }
    ev.xfer_complete.len = 4u * inflight;
    tb_usb_event_queued(&ev);
    uint32_t n = inflight; inflight = 0u;
    tb_midi_tx_result(0u, 4u * n, true);
    zlp = n == 16u && !inflight; /* midi_device.c: nothing more in the FIFO after a full transfer */
}
static void usb_event(uint8_t id) { dcd_event_t ev; memset(&ev, 0, sizeof ev); ev.event_id = id; tb_usb_event_queued(&ev); }
/* ---- the computer: what it hears, the transport bytes, the SysEx replies ---- */
/* 2.4.8 (fuzz_main): every note played, and those played before the last loss: a Note Off for one of those is right
 * even when the computer has it off (an Off sent but unconfirmed when the loss came is sent again). */
static uint8_t played[16][128], forgotten[16][128];
static uint8_t sounding[16][128], transport[256], reply[3000];
static uint32_t ons, offs, transports, reply_len, replies, bad_replies, lane_ons, errors_at;
static bool quiet, quiet_next; /* no Note On allowed once the board has read a request */
static bool pause_expected, in_reply; /* a reply after every note ended; inside a reply now */
static uint32_t phase, at, mark, sent, burst_from, started, chord_at, inline_ms;
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += sounding[c][p];
    return n;
}
static uint32_t heard_classes(void) {
    uint32_t set = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (sounding[0][p]) set |= 1u << (p % 12u);
    return set;
}
static void computer(const uint8_t p[4]) {
    uint32_t cin = p[0] & 15u, ch = p[1] & 15u;
    if (cin < 4u || cin > 7u) assert(!in_reply); /* nothing inside a SysEx reply (1.9.4) */
    if (cin == 0x9u && p[3]) {
        if (quiet || sounding[ch][p[2]]) printf("DBG Note On %u ch %u at %u ms, phase %u, %u ms into it, quiet %d, already %d\n", p[2], (unsigned)ch, (unsigned)ms, (unsigned)phase, (unsigned)(ms - at), quiet, sounding[ch][p[2]]);
        assert(!quiet && !sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; played[ch][p[2]] = 1u; forgotten[ch][p[2]] = 0u; ons++; lane_ons += ch >= 10u && ch <= 13u;
    } else if (cin == 0x8u || (cin == 0x9u && !p[3])) {
        if (!sounding[ch][p[2]] && forgotten[ch][p[2]]) { forgotten[ch][p[2]] = 0u; return; } /* owed since a loss */
        assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++;
    } else if (cin == 0xFu) {
        if (p[1] != 0xF8u) transport[transports++ % 256u] = p[1];
    } else if (cin >= 4u && cin <= 7u) {
        uint32_t n = cin == 4u ? 3u : cin - 4u;
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t b = p[1u + i];
            if (b == 0xF0u) { assert(!pause_expected || !heard()); reply_len = 0u; in_reply = true; } /* a pause: every note ended first */
            if (reply_len < sizeof reply) reply[reply_len++] = b;
            if (b == 0xF7u) {
                in_reply = false;
                uint32_t sum = 0;
                for (uint32_t j = 4; j + 1u < reply_len; ++j) sum += reply[j];
                bool ok = reply_len >= 8u && reply[1] == 0x7Du && reply[2] == 0x54u && reply[3] == 0x42u && sum % 128u == 0u;
                replies++; bad_replies += !ok;
            }
        }
    }
}
static uint8_t req_cable, req_bad; /* 2.4.8: the fuzz's cable, and a checksum off by req_bad */
static void request(const uint8_t *data, uint32_t n) { /* F0 7D 54 42 data.. sum F7 as USB-MIDI packets */
    uint8_t m[32]; uint32_t len = 0, sum = req_bad;
    m[len++] = 0xF0u; m[len++] = 0x7Du; m[len++] = 0x54u; m[len++] = 0x42u;
    for (uint32_t i = 0; i < n; ++i) { m[len++] = data[i]; sum += data[i]; }
    m[len++] = (uint8_t)((128u - sum % 128u) % 128u); m[len++] = 0xF7u;
    for (uint32_t i = 0; i < len; i += 3u) {
        uint32_t left = len - i, take = left < 3u ? left : 3u;
        uint8_t *q = inq[in_w++ % 512u];
        q[0] = (uint8_t)((uint32_t)req_cable << 4 | (left <= 3u ? 4u + take : 4u)); q[1] = m[i]; q[2] = take > 1u ? m[i + 1u] : 0u; q[3] = take > 2u ? m[i + 2u] : 0u;
    }
}
static void daw(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { /* one packet from music software (cable 0) */
    uint8_t *q = inq[in_w++ % 512u]; q[0] = a; q[1] = b; q[2] = c; q[3] = d;
}
static uint32_t count_transport(uint8_t b, uint32_t from) {
    uint32_t n = 0;
    for (uint32_t i = from; i < transports; ++i) n += transport[i % 256u] == b;
    return n;
}
/* ---- 2.4.8 (review TBR-023): the fuzz (fuzz_main), one step per pass ---- */
static jmp_buf finished; /* where both end */
#define FZ_MS 1200000u /* twenty minutes of passes at random, then the check that everything recovers */
static uint32_t fz_phase, fz_at, fz_errors, fz_last_loss, fz_sent, fz_mark, fz_wait, fz_longest, fz_next;
static uint32_t fz_requests, fz_answered, fz_unplugs, fz_suspends, fz_late, fz_toggles, fz_losses_ended, fz_cut;
static bool fz_expect, fz_pausing, fz_stop_due;
static uint32_t fz_pending, fz_pending_since, fz_pending_longest, fz_stops; /* the engine's batch waiting for its receipt */
static void host_forgets(void) { /* a loss: the computer forgets what sounded and any reply it was reading */
    fz_stop_due = !tc_state.trs_out && (engine->state == TK_ACTIVE || engine->state == TK_DRAINING || engine->pending_id);
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t n = 0; n < 128u; ++n) forgotten[c][n] |= played[c][n];
    memset(sounding, 0, sizeof sounding); in_reply = false; reply_len = 0u;
    quiet = quiet_next = pause_expected = fz_expect = fz_pausing = false; fz_last_loss = ms;
}
static void fz_lanes(void) { /* 0..4 chord lanes (5 notes a step each, so up to 20 together), the rest empty */
    uint32_t chords = rnd() % 5u;
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        ln_lane *l = &ln.lanes[i];
        l->kind = i < chords ? LN_CHORDS : LN_OFF; l->channel = (uint8_t)(11u + i % 4u);
        for (uint32_t st = 0; st < 16u; ++st) {
            memset(&l->steps[st], 0, sizeof l->steps[st]);
            l->steps[st].on = rnd() % 4u != 0u; l->steps[st].value = (uint8_t)(rnd() % 2u ? 15u : 3u);
            l->steps[st].edit = (TC_ADD7 | TC_ADD9) << 5;
        }
    }
    ln_changes++;
}
static void fz_request(void) { /* as the editor: one request at a time, a new one once the reply is in */
    static const uint8_t get_live[3] = {SX_GET_RECORD, 1, 0}, state[1] = {SX_STATE};
    static const uint8_t set_scale[16] = {SX_SET_SCALE, 2, 53, 21, 'F', 'U', 'Z', 'Z', 0, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t nor[7] = {SX_NOR_READ, 0, 0, 0, 0, 0, 16};
    uint32_t kind = rnd() % 16u; /* 0-5 live, 6-8 STATE, 9 a preset, 10-12 bad checksum, 13 a write, 14 the backup chunk */
    req_cable = (uint8_t)(rnd() % 2u); req_bad = 0u;
    if (kind == 15u) { /* cut short (the tab closed): an unended message, which the next F0 replaces */
        uint8_t *q = inq[in_w++ % 512u]; q[0] = (uint8_t)((uint32_t)req_cable << 4 | 4u); q[1] = 0xF0u; q[2] = 0x7Du; q[3] = 0x54u;
        fz_cut++; kind = rnd() % 8u;
    }
    if (kind < 6u) request(get_live, 3);
    else if (kind < 9u) request(state, 1);
    else if (kind < 10u) { const uint8_t preset[3] = {SX_GET_RECORD, 2, (uint8_t)(rnd() % 8u)}; request(preset, 3); }
    else if (kind < 13u) { req_bad = (uint8_t)(1u + rnd() % 100u); request(state, 1); } /* answered SX_CHECKSUM, inline */
    else { /* the editor's pause: every note ended, a Stop, then the reply */
        if (kind == 13u) request(set_scale, 16); else request(nor, 7);
        quiet_next = pause_expected = fz_pausing = true;
    }
    req_bad = 0u; req_cable = 0u;
    fz_requests++; fz_expect = true; fz_sent = ms; fz_mark = replies;
}
static uint32_t fz_passes;
static void fuzz_pass(void) {
    fz_passes++; assert(!bad_replies);
    if (engine->errors > fz_errors) { /* a session ended: only right after a loss this run caused */
        if (ms - fz_last_loss >= 1500u) printf("DBG session ended at %u ms, %u ms after the last loss\n", (unsigned)ms, (unsigned)(ms - fz_last_loss));
        assert(ms - fz_last_loss < 1500u);
        fz_losses_ended += engine->errors - fz_errors;
    }
    fz_errors = engine->errors; /* the fresh session starts from 0 */
    if (fz_stop_due) { assert(engine->state == TK_STOPPED); fz_stop_due = false; fz_stops++; } /* the loss ended it (tp_loss) */
    if (engine->pending_id != fz_pending) { fz_pending = engine->pending_id; fz_pending_since = ms; }
    else if (fz_pending && engine->state != TK_STOPPED && mounted && !suspended && ms - fz_last_loss > 100u) {
        /* a receipt comes within a few ms (a session a loss stopped keeps its last batch until it restarts) */
        if (ms - fz_pending_since > fz_pending_longest) fz_pending_longest = ms - fz_pending_since;
        assert(ms - fz_pending_since < 100u);
    }
    if (fz_expect && replies != fz_mark) {
        uint32_t took = ms - fz_sent;
        if (took > fz_longest) fz_longest = took;
        fz_expect = false; fz_answered++; quiet = false; pause_expected = fz_pausing = false;
        fz_next = ms + rnd() % (fz_phase ? 1u : 2000u); /* about a request a second: mostly playing */
    }
    if (fz_expect) { /* 2 KiB through a late-polling host, or a drain of 20 notes: well inside 5 s */
        if (ms - fz_sent >= 5000u) printf("DBG no reply after %u ms (request %u)\n", (unsigned)(ms - fz_sent), (unsigned)fz_requests);
        assert(ms - fz_sent < 5000u);
    }
    switch (fz_phase) {
    case 0: { /* at random */
        if (ms >= FZ_MS) { fz_phase = 1u; fz_at = ms; break; }
        if (ms == 300u) { /* the progression of case 0, and lanes */
            sq_clear(&sq, SQ_UNIT_BEAT, 0u);
            static const uint8_t pads[4] = {3, 19, 23, 15};
            for (uint32_t i = 0; i < 4u; ++i) (void)sq_tap(&sq, pads[i], 100, 0u, (tc_chord){0, 0, 0});
            fz_lanes(); tc_state.transport = true;
        }
        uint32_t r = rnd() % 100000u;
        if (!mounted) { if (ms - fz_wait >= 50u && r < 200u) { mounted = true; usb_event(DCD_EVENT_BUS_RESET); tud_mount_cb(); } break; }
        if (suspended) { /* resumed, or reset by the host */
            if (ms - fz_wait < 20u || r >= 300u) break;
            suspended = false;
            if (r < 100u) {
                usb_event(DCD_EVENT_BUS_RESET); if (hw_done) usb_task_done();
                inflight = 0u; zlp = false; in_r = in_w; tud_mount_cb(); host_forgets();
            }
            fz_last_loss = ms; break;
        }
        if (r < 6u) { /* unplugged */
            usb_event(DCD_EVENT_UNPLUGGED); if (hw_done) usb_task_done(); /* queued before it: handled first */
            mounted = false; inflight = 0u; zlp = false; in_r = in_w; host_forgets();
            fz_unplugs++; fz_wait = ms; break;
        }
        if (r < 12u) { /* asleep (also how an unplug shows with VBUS sensing off) */
            suspended = true; usb_event(DCD_EVENT_SUSPEND); tud_suspend_cb(false); host_forgets();
            fz_suspends++; fz_wait = ms; break;
        }
        if (r < 60u) { tc_state.transport = !tc_state.transport; fz_toggles++; }
        else if (r < 90u) fz_lanes();
        else if (r < 100u) tc_state.trs_out = (uint8_t)!tc_state.trs_out;
        if (!fz_expect && ms >= fz_next && ms >= 500u) fz_request();
        break;
    }
    case 1: /* everything back: plugged in, awake, the transport off */
        if (suspended) { suspended = false; fz_last_loss = ms; }
        if (!mounted) { mounted = true; usb_event(DCD_EVENT_BUS_RESET); tud_mount_cb(); }
        tc_state.transport = false; tc_state.trs_out = 0u; fz_phase = 2u; fz_at = ms; break;
    case 2: /* silent, and no reply outstanding */
        if (ms - fz_at < 3000u) break;
        assert(!heard() && !fz_expect);
        tc_state.transport = true; fz_phase = 3u; fz_at = ms; break;
    case 3: /* playing again */
        if (!heard()) { assert(ms - fz_at < 3000u); break; }
        fz_late = ms - fz_at; fz_request(); fz_phase = 4u; break;
    case 4: /* and the editor answered */
        if (fz_expect) break;
        assert(fz_errors == engine->errors);
        longjmp(finished, 1);
    }
}
/* ---- the scenario, one step per pass ---- */
#define C_ 0x091u
#define G_ 0x884u
#define AM 0x211u
#define F_ 0x221u
static void next(void) { phase++; at = ms; }
void tb_snapshot(void) {
    us += 1000u;
    for (uint32_t i = 0; i < 3u; ++i) { uint8_t b; (void)sm_pop(&sm_out, &b); } /* 2.3.3: the TRS jack, ~3 bytes a ms */
    if (quiet_next) { quiet = true; quiet_next = false; } /* main.c read the request in this pass */
    if (fuzzing) { fuzz_pass(); return; }
    assert(ms < 60000u); /* every phase ends long before */
    switch (phase) {
    case 0: /* started, playing: a progression of a beat per chord (C G Am F) */
        assert(tb_save_time.stack_low == TB_PAINT_TOP); /* nothing under the stack changed yet */
        if (ms < 300u || ts_status.engine_state != TK_ACTIVE) break;
        sq_clear(&sq, SQ_UNIT_BEAT, 0u);
        static const uint8_t pads[4] = {3, 19, 23, 15};
        for (uint32_t i = 0; i < 4u; ++i) (void)sq_tap(&sq, pads[i], 100, 0u, (tc_chord){0, 0, 0});
        tc_state.transport = true; next(); break;
    case 1: /* halfway through Am, the editor reads the settings: the reply comes at once, Am plays on */
        if (ms - at < 1250u) break;
        assert(heard_classes() == AM && count_transport(0xFAu, 0) == 1u);
        started = at;
        { static const uint8_t get_live[3] = {SX_GET_RECORD, 1, 0}; request(get_live, 3); }
        mark = replies; burst_from = transports; next(); break;
    case 2:
        if (replies == mark) break;
        inline_ms = ms - at;
        assert(inline_ms < 20u && heard_classes() == AM && !count_transport(0xFCu, burst_from) && !bad_replies);
        { static const uint8_t state[1] = {SX_STATE}; request(state, 1); }
        mark = replies; next(); break;
    case 3: /* STATE: the check, 4 counts, the items and which show; still Am */
        if (replies == mark) break;
        assert(reply[4] == (SX_STATE | 0x40u) && reply[5] == SX_OK && reply_len == 6u + 20u + 2u + (TC_MENU_ITEMS + 6u) / 7u + 2u);
        assert(heard_classes() == AM && !count_transport(0xFCu, burst_from));
        next(); break;
    case 4: /* F, then a backup: eight requests, each sent when the previous reply has arrived */
        if (ms - started < 1650u) break;
        assert(heard_classes() == F_);
        mark = replies; sent = 0u; burst_from = transports; next(); break;
    case 5:
        if (replies - mark == sent && sent < 8u) {
            const uint8_t get_preset[3] = {SX_GET_RECORD, 2, (uint8_t)sent}; request(get_preset, 3); sent++;
        }
        if (replies - mark < 8u) break;
        assert(ms - at < 200u && heard() && !count_transport(0xFCu, burst_from) && !count_transport(0xFBu, burst_from));
        next(); break;
    case 6: /* a request that writes (user scale 3) pauses: the reply after every note ended and a Stop */
        if (ms - started < 2250u) break;
        chord_at = heard_classes(); assert(chord_at == C_); /* round to the first chord again */
        { static const uint8_t set_scale[16] = {SX_SET_SCALE, 2, 53, 21, 'T', 'E', 'S', 'T', 0, 0, 0, 0, 0, 0, 0, 0};
          request(set_scale, 16); }
        quiet_next = true; pause_expected = true; mark = replies; burst_from = transports; next(); break;
    case 7:
        if (replies == mark) break;
        assert(!heard() && count_transport(0xFCu, burst_from) == 1u && reply[5] == SX_OK); next(); break;
    case 8: /* a second of quiet, then playing resumes where it was: Continue, the same chord */
        if (ms - at == 990u) { assert(!heard()); quiet = false; }
        if (ms - at < 990u || heard() < 3u) break;
        assert(ms - at < 2500u && count_transport(0xFBu, burst_from) == 1u && heard_classes() == chord_at);
        /* The external memory backup: a 2 KiB chunk (too long to go between notes), the cable pulled while it streams. */
        { static const uint8_t nor[7] = {SX_NOR_READ, 0, 0, 0, 0, 0, 16}; request(nor, 7); }
        quiet_next = true; mark = replies; next(); break;
    case 9:
        if (reply_len < 300u) break;
        mounted = false; inflight = 0u; zlp = false; in_reply = false; usb_event(DCD_EVENT_UNPLUGGED); next(); break;
    case 10: /* plugged in again 300 ms later: playing comes back, nothing left sounding */
        if (ms - at == 300u) { mounted = true; usb_event(DCD_EVENT_BUS_RESET); quiet = false; }
        if (ms - at < 300u || !heard()) break;
        assert(ms - at < 3000u && replies == mark); /* the cut reply never completed */
        next(); break;
    case 11: /* 2.3.3: four chord lanes on their own channels, each step a chord with its 7th and 9th: 20 Note Ons due
              * on the same sixteenth, so a batch of 16 (a full 64-byte transfer, TinyUSB's zero-length packet after it)
              * then 4 (2.3.2 board test T2: that packet taken as the next batch's receipt ended the session every
              * second) */
        if (ms - at < 2000u) break; /* still playing through the progression */
        for (uint32_t i = 0; i < 4u; ++i) {
            ln_lane *l = &ln.lanes[i];
            l->kind = LN_CHORDS; l->channel = (uint8_t)(11u + i);
            for (uint32_t st = 0; st < 16u; ++st) {
                memset(&l->steps[st], 0, sizeof l->steps[st]);
                l->steps[st].on = true; l->steps[st].value = (uint8_t)(st % 2u ? 15u : 3u); l->steps[st].edit = (TC_ADD7 | TC_ADD9) << 5;
            }
        }
        ln_changes++;
        mark = zlps; errors_at = engine->errors; lane_ons = 0u; next(); break;
    case 12: /* two seconds of lanes: every step's 20 notes, a zero-length packet each time, the session never ended */
        if (ms - at < 2000u) break;
        assert(zlps - mark >= 10u && engine->errors == errors_at && lane_ons >= 10u * 20u && heard());
        ln_clear(&ln); ln_changes++;
        tc_state.transport = false; next(); break;
    case 13:
        if (ms - at < 300u) break;
        assert(!heard() && ons == offs && !bad_replies);
        buttons = true; sent = 0u; tc_state.u3_shift = true;
        tb_stack_paint[100] = 0u; /* 2.4.8 (TBR-010): the stack reached word 100 */
        next(); break;
    case 14: /* 2.4.2 (TBR-012): the touch bus answers (a baseline); then U3, Shift, is pressed and held */
        if (ms - at == 30u) u3_down = true;
        if (ms - at < 120u) break;
        assert(tc_state.u3_held && tc_state.debounce[2].qualified);
        bus_of_main->latched = true; mark = recoveries; next(); break;
    case 15: /* a latched fault refuses every read until main.c recovers the bus; U3 stays held in every pass (a
              * button let go and read again is only a baseline: it would look dead until pressed again) */
        assert(tc_state.u3_held);
        if (ms - at < 300u) break;
        assert(recoveries == mark + 1u && !bus_of_main->latched);
        next(); break;
    case 16: /* again 1,301 ms later (recovery at most once a second): 1 ms later in the poll cycle each time, ten times */
        assert(tc_state.u3_held);
        if (ms - at < 1001u) break;
        if (++sent < 10u) { bus_of_main->latched = true; mark = recoveries; phase = 15u; at = ms; break; }
        assert(recoveries == 10u && tc_state.recoveries == 10u);
        u3_down = false; next(); break;
    case 17: /* let go: released as usual */
        if (ms - at < 100u) break;
        assert(!tc_state.u3_held && !heard() && ons == offs && !bad_replies);
        assert(restarts >= 2u && !restarts_masked); /* 2.4.3 (TBR-015): every fresh session cleared with interrupts on */
        buttons = false;
        /* 2.4.4 (review TBR-026): a DAW sends Start and its clock, holds a note, and half a SysEx message */
        sq_clear(&sq, SQ_UNIT_BEAT, 0u);
        daw(0x0Fu, 0xFAu, 0u, 0u); daw(0x09u, 0x90u, 60u, 100u); daw(0x04u, 0xF0u, 0x7Du, 0x54u);
        next(); break;
    case 18: /* clock for 100 ms; then the computer goes to sleep (a suspend: also how an unplug shows) and a host
              * mounts the board again */
        if (ms % 20u == 0u) daw(0x0Fu, 0xF8u, 0u, 0u);
        if (ms - at < 100u) break;
        assert(tm.running && tm.notes[60] && tm.in_sysex && tm.position >= 3u);
        tud_suspend_cb(false); tud_mount_cb();
        assert(!tm.running && !tm.notes[60] && !tm.in_sysex);
        mark = tm.position; next(); break;
    case 19: /* the next host sends bare clock, no Start: nothing runs on from the old position */
        if (ms % 20u == 0u) daw(0x0Fu, 0xF8u, 0u, 0u);
        if (ms - at < 200u) break;
        assert(!tm.running && tm.position == mark && tm.ticks > 0u && !heard());
        /* 2.4.5 (review TBR-007): pattern 6 filled in (as a copy does: 32 chunks and its windows) */
        mark = tb_save_time.service_writes; assert(tb_save_time.service_us < 2u * 44u * 16u);
        for (uint32_t l = 0; l < LN_LANES; ++l) {
            ln_bank[5].lanes[l].kind = LN_DRUMS;
            for (uint32_t s = 0; s < LN_STEPS; ++s) { ln_bank[5].lanes[l].steps[s].on = true; ln_bank[5].lanes[l].steps[s].value = 36u; }
            for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed_in(5u, l, s);
        }
        ln_extra_changed(5u); next(); break;
    case 20: /* saved 3 s later, one record a pass: the longest st_service call one record's programming (704 us) */
        if (ms - at < ST_SAVE_DELAY_MS + 200u) break;
        assert(tb_save_time.service_writes - mark >= 32u && !ln_dirty[5]);
        assert(tb_save_time.service_us >= 44u * 16u && tb_save_time.service_us < 2u * 44u * 16u);
        assert(tb_save_time.stack_low == TB_PAINT_TOP - 4u * (TB_PAINT_WORDS - 100u)); /* 2.4.8 (TBR-010) */
        longjmp(finished, 1);
    }
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(flash, 0xFF, sizeof flash);
    for (uint32_t i = 0; i < TB_PAINT_WORDS; ++i) tb_stack_paint[i] = TB_PAINT;
    if (fuzzing) {
        if (!setjmp(finished)) usb_main();
        printf("PASS: main loop at random (seed %08x, %u s, %u passes): %u requests on both cables (%u cut short first), all "
               "answered, the longest in %u ms; every batch confirmed within %u ms; %u unplugs, %u suspends, %u transport changes; sessions ended only by "
               "those losses (%u, %u of them playing to USB: each ended at once); %u zero-length packets; silent at the end, then playing again in %u ms; %u Note On\n",
               0x2480F00Du, FZ_MS / 1000u, (unsigned)fz_passes, (unsigned)fz_requests, (unsigned)fz_cut, (unsigned)fz_longest, (unsigned)fz_pending_longest + 1u,
               (unsigned)fz_unplugs, (unsigned)fz_suspends, (unsigned)fz_toggles, (unsigned)fz_losses_ended, (unsigned)fz_stops,
               (unsigned)zlps, (unsigned)fz_late, (unsigned)ons);
        return 0;
    }
    { /* 2.4.7 (TBR-008): a log past half full (one favorite written over and over), so this start compacts */
        static tc_controls c; static tp_engine e;
        tc_init(&c); (void)st_load(&c);
        uint32_t p[ST_PAYLOAD]; memset(p, 0, sizeof p);
        for (uint32_t i = 0; i < TC_FAVOURITES / 4u; ++i) p[i] = 0xFFFFFFFFu;
        p[0] = 5u | 0xFFFFFF00u;
        while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 8u) assert(st_put(ST_FAVOURITES, 0, p, &c, &e) == ST_OK);
    }
    if (!setjmp(finished)) usb_main();
    assert(warnings == 1u && erase_us >= 1500000u && erase_us < 1600000u);
    printf("PASS: main loop: inline replies (a read back in %u ms, STATE, a backup's 8 requests: the chord plays on, no "
           "Stop, nothing inside a reply), the editor's pause for a write (every note off and Stop before the reply, a "
           "second of quiet, Continue on the same chord), the cable pulled during a 2 KiB reply (playing resumes, nothing "
           "left sounding), four chord lanes' 20 notes a step in full 64-byte transfers each ended by a zero-length packet "
           "(%u lane notes, the session never ended); a pattern saved one record a pass (longest %u us); the start-up rewrite's erase after the warning at %u ms (not before 1,500); U3 held as Shift kept through 10 latched bus faults and recoveries; the stack's high-water mark at 0x%08X; %u sessions restarted, each after the masked pass; %u Note On / %u Note Off, %u replies\n",
           (unsigned)inline_ms, (unsigned)lane_ons, (unsigned)tb_save_time.service_us, (unsigned)(erase_us / 1000u), (unsigned)tb_save_time.stack_low, (unsigned)restarts, (unsigned)ons, (unsigned)offs, (unsigned)replies);
    return 0;
}
