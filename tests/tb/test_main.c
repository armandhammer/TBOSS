/* SPDX-License-Identifier: GPL-3.0-or-later - main.c's loop on the computer (0.11.0): 1.9.4's inline
 * replies (a read, the editor's STATE check and a backup's burst come back between two notes: nothing
 * stops, no note inside a reply), the web editor's pause for a request that writes (the engine drains,
 * the reply streams, a second of quiet, playing resumes on the same chord), and the USB cable pulled
 * in the middle of a long reply, with a progression playing throughout. main.c runs unchanged
 * against stand-ins for TinyUSB, the pad scanner and the touch bus (tests/tb/main_hal);
 * tb_snapshot(), called at the end of every pass, moves time on and plays the computer. */
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
uint32_t test_primask, SystemCoreClock = 48000000u;
void test_wfi(void) { printf("main.c stopped: fault %x\n", (unsigned)tb_status.fault); assert(!"main.c failed"); }
/* ---- the board's other parts ---- */
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
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
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb_ready, uint32_t now) {
    (void)c; (void)e; (void)s; (void)usb_ready; (void)now;
}
static uint32_t micros_now;
static void select_position(uint8_t p) { (void)p; }
static bool sample(uint8_t bank, uint32_t timeout, uint16_t *value) { (void)bank; (void)timeout; *value = 0u; return true; }
static bool settle(uint32_t us) { micros_now += us; return true; }
static uint32_t micros(void) { return micros_now += 7u; }
bool ts_hardware_init(void) { return true; }
const ts_ops ts_hardware_ops = {select_position, sample, settle, micros};
bool tc_hardware_init(tc_bus *b) { memset(b, 0, sizeof *b); return false; } /* no buttons: pads still play */
bool tc_hardware_recover(tc_bus *b) { (void)b; return false; }
struct bi_reply tc_i2c_read(void *context, uint8_t address, uint8_t reg, uint32_t generation, uint32_t now, uint32_t budget) {
    (void)context; (void)address; (void)reg; (void)budget;
    struct bi_reply r = {BI_NACK, 0u, generation, now, 100u, {0u, 0u}};
    return r;
}
/* ---- the USB stack at full speed: one transfer of up to 16 packets (64 bytes) per 1 ms frame;
 * time passes while main.c waits for room (us: simulated time, a pass is 1 ms) ---- */
static uint32_t us, inflight, frame_ms = UINT32_MAX;
#define ms (us / 1000u)
static bool mounted = true;
static uint8_t inq[512][4];
static uint32_t in_r, in_w;
uint32_t tb_millis(void) { return ms; }
uint32_t tb_board_init(void) { return 0u; }
bool tusb_init(uint8_t rhport, const tusb_rhport_init_t *init) { (void)rhport; (void)init; return true; }
bool tud_mounted(void) { return mounted; }
bool tud_suspended(void) { return false; }
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep) { (void)rhport; (void)ep; return inflight != 0u; }
bool tud_midi_packet_read(uint8_t p[4]) {
    if (in_r == in_w) return false;
    memcpy(p, inq[in_r++ % 512u], 4); return true;
}
static void computer(const uint8_t p[4]);
bool tud_midi_packet_write(const uint8_t p[4]) {
    if (!mounted || inflight >= 16u) return false;
    inflight++; computer(p); return true;
}
void tud_task_ext(uint32_t timeout_ms, bool in_isr) {
    (void)timeout_ms; (void)in_isr;
    if (!inflight || !mounted) return;
    if (ms == frame_ms) { us += 100u; return; } /* this frame's transfer has gone */
    frame_ms = ms;
    dcd_event_t ev; memset(&ev, 0, sizeof ev);
    ev.event_id = DCD_EVENT_XFER_COMPLETE; ev.xfer_complete.ep_addr = 0x81u;
    ev.xfer_complete.result = XFER_RESULT_SUCCESS; ev.xfer_complete.len = 4u * inflight;
    tb_usb_event_queued(&ev);
    uint32_t n = inflight; inflight = 0u;
    tb_midi_tx_result(0u, 4u * n, true);
}
static void usb_event(uint8_t id) { dcd_event_t ev; memset(&ev, 0, sizeof ev); ev.event_id = id; tb_usb_event_queued(&ev); }
/* ---- the computer: what it hears, the transport bytes, the SysEx replies ---- */
static uint8_t sounding[16][128], transport[256], reply[3000];
static uint32_t ons, offs, transports, reply_len, replies, bad_replies;
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
        assert(!quiet && !sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++;
    } else if (cin == 0x8u || (cin == 0x9u && !p[3])) {
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
static void request(const uint8_t *data, uint32_t n) { /* F0 7D 54 42 data.. sum F7 as USB-MIDI packets */
    uint8_t m[32]; uint32_t len = 0, sum = 0;
    m[len++] = 0xF0u; m[len++] = 0x7Du; m[len++] = 0x54u; m[len++] = 0x42u;
    for (uint32_t i = 0; i < n; ++i) { m[len++] = data[i]; sum += data[i]; }
    m[len++] = (uint8_t)((128u - sum % 128u) % 128u); m[len++] = 0xF7u;
    for (uint32_t i = 0; i < len; i += 3u) {
        uint32_t left = len - i, take = left < 3u ? left : 3u;
        uint8_t *q = inq[in_w++ % 512u];
        q[0] = (uint8_t)(left <= 3u ? 4u + take : 4u); q[1] = m[i]; q[2] = take > 1u ? m[i + 1u] : 0u; q[3] = take > 2u ? m[i + 2u] : 0u;
    }
}
static uint32_t count_transport(uint8_t b, uint32_t from) {
    uint32_t n = 0;
    for (uint32_t i = from; i < transports; ++i) n += transport[i % 256u] == b;
    return n;
}
/* ---- the scenario, one step per pass ---- */
#define C_ 0x091u
#define G_ 0x884u
#define AM 0x211u
#define F_ 0x221u
static jmp_buf finished;
static void next(void) { phase++; at = ms; }
void tb_snapshot(void) {
    us += 1000u;
    if (quiet_next) { quiet = true; quiet_next = false; } /* main.c read the request in this pass */
    assert(ms < 60000u); /* every phase ends long before */
    switch (phase) {
    case 0: /* started, playing: a progression of a beat per chord (C G Am F) */
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
        mounted = false; inflight = 0u; in_reply = false; usb_event(DCD_EVENT_UNPLUGGED); next(); break;
    case 10: /* plugged in again 300 ms later: playing comes back, nothing left sounding */
        if (ms - at == 300u) { mounted = true; usb_event(DCD_EVENT_BUS_RESET); quiet = false; }
        if (ms - at < 300u || !heard()) break;
        assert(ms - at < 3000u && replies == mark); /* the cut reply never completed */
        next(); break;
    case 11:
        if (ms - at < 2000u) break; /* still playing through the progression */
        tc_state.transport = false; next(); break;
    case 12:
        if (ms - at < 300u) break;
        assert(!heard() && ons == offs && !bad_replies);
        longjmp(finished, 1);
    }
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(flash, 0xFF, sizeof flash);
    if (!setjmp(finished)) usb_main();
    printf("PASS: main loop: inline replies (a read back in %u ms, STATE, a backup's 8 requests: the chord plays on, no "
           "Stop, nothing inside a reply), the editor's pause for a write (every note off and Stop before the reply, a "
           "second of quiet, Continue on the same chord), the cable pulled during a 2 KiB reply (playing resumes, nothing "
           "left sounding); %u Note On / %u Note Off, %u replies\n",
           (unsigned)inline_ms, (unsigned)ons, (unsigned)offs, (unsigned)replies);
    return 0;
}
