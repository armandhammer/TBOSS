/* SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard replacement firmware, flash resident.
 * Foreground loop: USB, one pad-scan position per pass, controls every 10 ms, the
 * MIDI engine with per-packet USB receipts, MIDI input (lights, clock, SysEx).
 * Diagnostics block at 0x20018000, readable over SWD while running.
 * SysEx replies use transfer mode: the engine first drains (every Note Off sent
 * and receipted), then the reply streams through the TinyUSB FIFO without
 * per-packet receipts; playing resumes 1 s after the last request. */
#include "stm32f4xx.h"
#include "tusb.h"
#include "device/dcd.h"
#include "device/usbd_pvt.h"
#include "diagnostic.h"
#include "controls.h"
#include "settings.h"
#include "ui.h"
#include "midi_in.h"
#include "sysex.h"
#include "nor.h"
volatile tb_diagnostic tb_status __attribute__((section(".diagnostic")));
volatile ts_abi ts_status __attribute__((section(".scanabi")));
volatile uint32_t tc_status[TC_WORDS] __attribute__((section(".controls")));
volatile uint32_t tb_initialized_cookie = 0x40704801u;
static ts_scan scanner;
static tp_engine keyboard;
static tc_bus bus;
typedef struct { uint32_t token, count; bool success; } tx_receipt;
static volatile tx_receipt receipts[8];
static volatile uint32_t head, tail, wire_token;
static volatile bool lost;
enum { TX_IDLE, TX_DRAIN, TX_SEND, TX_LINGER };
static volatile uint32_t transfer; /* TX_*; receipts are ignored while sending */
static uint32_t tx_len, tx_pos, tx_at;
static void observe_loss(void) {
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    bool had = lost; lost = false;
    __set_PRIMASK(mask);
    if (had) tp_loss(&keyboard);
}
static bool write_packet(const uint8_t packet[4], uint32_t token) {
    if (lost || !tud_mounted() || tud_suspended()) return false;
    wire_token = token; __DMB();
    if (tud_midi_packet_write(packet)) return true;
    wire_token = 0u;
    return false;
}
static void fail(uint32_t code) {
    tb_status.fault = code; tb_snapshot();
    for (;;) __WFI();
}
/* Hooks called from the patched TinyUSB copies (scripts/tboss_build.py). */
__attribute__((noreturn)) void tb_usb_wait_fault(unsigned int code) {
    tp_loss(&keyboard);
    __disable_irq(); fail(code);
    for (;;) {}
}
void tb_usb_event_queued(dcd_event_t const *event) {
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    if (event->event_id == DCD_EVENT_BUS_RESET || event->event_id == DCD_EVENT_UNPLUGGED ||
        event->event_id == DCD_EVENT_SUSPEND) lost = true;
    if (event->event_id == DCD_EVENT_XFER_COMPLETE && event->xfer_complete.ep_addr == 0x81u &&
        transfer != TX_SEND && transfer != TX_LINGER) {
        uint32_t next = (head + 1u) & 7u;
        if (next == tail) lost = true;
        else {
            receipts[head].token = wire_token;
            receipts[head].count = event->xfer_complete.len;
            receipts[head].success = event->xfer_complete.result == XFER_RESULT_SUCCESS;
            __DMB(); head = next;
        }
    }
    __set_PRIMASK(mask);
}
void tud_mount_cb(void) { tb_status.mounts++; }
void tud_suspend_cb(bool wakeup) { (void)wakeup; tb_status.suspends++; }
void tud_event_hook_cb(uint8_t port, uint32_t event, bool isr) {
    (void)port; (void)isr;
    if (event == DCD_EVENT_BUS_RESET) tb_status.resets++;
}
void tb_midi_tx_result(uint8_t itf, uint32_t count, bool success) {
    observe_loss();
    if (transfer == TX_SEND || transfer == TX_LINGER) return; /* SysEx stream: no engine receipts */
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    if (tail == head) tp_complete(&keyboard, 0u, count, false, tb_millis());
    else {
        tx_receipt r = receipts[tail];
        tail = (tail + 1u) & 7u;
        tp_complete(&keyboard, r.token, count,
                    success && itf == 0u && r.success && r.count == count, tb_millis());
    }
    __set_PRIMASK(mask);
}
static struct bi_reply serviced_read(void *context, uint8_t a, uint8_t reg,
    uint32_t generation, uint32_t now, uint32_t budget) {
    /* Called with IRQs enabled. Bound each bus wait, service USB between clients. */
    tud_task_ext(0u, false);
    struct bi_reply reply = tc_i2c_read(context, a, reg, generation, now, budget);
    tud_task_ext(0u, false);
    return reply;
}
/* MIDI in: every waiting packet; a complete SysEx message is answered once no
 * reply is still streaming. */
static void midi_input(uint32_t now) {
    uint8_t packet[4];
    for (uint32_t n = 0; n < 32u && tud_midi_packet_read(packet); ++n) tm_packet(packet, now);
    if (!tm.sysex_ready || transfer == TX_DRAIN || (transfer == TX_SEND && tx_pos < tx_len)) return;
    uint32_t len = sx_handle(tm.sysex, tm.sysex_len, &tc_state, &keyboard, now);
    tm_sysex_consumed();
    tx_at = now;
    if (!len) return;
    tx_len = len; tx_pos = 0;
    if (transfer == TX_IDLE) {
        transfer = TX_DRAIN;
        uint32_t mask = __get_PRIMASK(); __disable_irq();
        tp_disable(&keyboard);
        __set_PRIMASK(mask);
    } else transfer = TX_SEND;
}
static void sysex_output(uint32_t now) {
    if (transfer == TX_DRAIN) {
        if (keyboard.state == TK_ACTIVE || keyboard.state == TK_DRAINING || keyboard.pending_id ||
            usbd_edpt_busy(0u, 0x81u)) return;
        transfer = TX_SEND;
    }
    if (transfer == TX_SEND) {
        /* Up to 3 ms per pass: refill the 64-byte FIFO as each USB transfer completes
         * (tud_task runs the completion), so a 2 KiB backup chunk is not paced by the
         * ~1 ms pad-scan pass. No notes are playing in transfer mode. */
        uint32_t began = tb_millis();
        while (tx_pos < tx_len && tud_mounted() && tb_millis() - began < 3u) {
            /* 3 bytes per packet: CIN 4 continues, CIN 5/6/7 ends with 1/2/3 bytes. */
            uint32_t left = tx_len - tx_pos, take = left < 3u ? left : 3u;
            uint8_t packet[4] = {0, 0, 0, 0};
            packet[0] = left <= 3u ? (uint8_t)(4u + take) : 4u;
            for (uint32_t i = 0; i < take; ++i) packet[1u + i] = sx_out[tx_pos + i];
            if (tud_midi_packet_write(packet)) tx_pos += take;
            else tud_task_ext(0u, false);
        }
        if (tx_pos == tx_len) { transfer = TX_LINGER; tx_at = now; }
        if (!tud_mounted()) { transfer = TX_LINGER; tx_pos = tx_len; }
        return;
    }
    if (transfer == TX_LINGER && now - tx_at >= 1000u && !usbd_edpt_busy(0u, 0x81u)) {
        uint32_t mask = __get_PRIMASK(); __disable_irq();
        tail = head; wire_token = 0u; transfer = TX_IDLE;
        __set_PRIMASK(mask);
    }
}
static void recover_bus(uint32_t now) {
    /* A latched I2C fault used to end touch input until reboot (review finding 1).
     * Recover after 200 ms latched, at most once a second; debounce re-baselines. */
    static uint32_t latched_at, tried_at;
    static bool latched;
    if (!bus.latched) { latched = false; return; }
    if (!latched) { latched = true; latched_at = now; return; }
    if (now - latched_at < 200u || (tc_state.recoveries && now - tried_at < 1000u)) return;
    tried_at = now; tc_state.recoveries++;
    (void)tc_hardware_recover(&bus);
    latched = false;
}
__attribute__((noreturn)) void usb_main(void) {
    tb_status.cookie = tb_initialized_cookie; tb_status.initialized_data = tb_initialized_cookie;
    tb_status.magic = 0x54423049u; tb_status.stage = 1u;
    const tk_config config = TK_DEFAULT_CONFIG;
    ts_status.magic = 0x53433049u; ts_status.version = 1u;
    ts_status.settle_us = 50u; ts_status.timeout_us = 1000u;
    ts_status.polarity = config.polarity; ts_status.press = config.press;
    ts_status.release = config.release; ts_status.full_scale = config.full_scale;
    ts_status.deadband = config.deadband; ts_status.pressure_ms = config.pressure_ms;
    tb_status.mode = 1u;
    if (tb_initialized_cookie != 0x40704801u) fail(0x100u);
    uint32_t result = tb_board_init();
    if (result) fail(result);
    tb_status.stage = 2u; __enable_irq();
    tc_init(&tc_state);
    (void)st_load(&tc_state); /* before USB: only here may the settings sector be erased */
    const tusb_rhport_init_t init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL};
    if (!tusb_init(0u, &init)) fail(0x109u);
    if (!ts_hardware_init()) fail(0x201u);
    (void)tc_hardware_init(&bus); /* missing controls never disable pads/USB */
    tc_publish(&tc_state, &bus, tc_status);
    (void)ui_init(); /* a missing display or LED chain never disables playing */
    (void)nr_init(); /* read-only; absence only disables the backup command */
    tb_status.stage = 3u;
    for (;;) {
        observe_loss();
        tud_task_ext(0u, false);
        bool fresh = ts_step(&scanner, ts_hardware_ops, ts_status.settle_us, ts_status.timeout_us, tb_millis());
        (void)tc_poll(&tc_state, &keyboard, serviced_read, &bus, tb_millis());
        recover_bus(tb_millis());
        tc_publish(&tc_state, &bus, tc_status);
        if (fresh) ts_capture(&scanner);
        uint32_t mask = __get_PRIMASK(); __disable_irq();
        observe_loss();
        uint32_t now = tb_millis();
        bool ready = tud_mounted() && !tud_suspended();
        if (fresh) {
            if (keyboard.state == TK_IDLE) { if (transfer == TX_IDLE) (void)tc_arm(&keyboard, &scanner, now, ready); }
            else (void)tc_frame(&keyboard, &scanner, now);
        }
        tc_step(&keyboard, now, ready, write_packet);
        __set_PRIMASK(mask);
        ts_status.arm_request = keyboard.consumed_arm; ts_status.arm_consumed = keyboard.consumed_arm;
        ts_status.engine_state = (uint32_t)keyboard.state;
        ts_status.accepted = keyboard.accepted; ts_status.completed = keyboard.completed;
        ts_status.errors = keyboard.errors; ts_status.obligations = tp_obligations(&keyboard);
        ts_status.uncertain = keyboard.uncertain ? 1u : 0u;
        tb_status.accepted = keyboard.accepted; tb_status.completed = keyboard.completed;
        tb_status.tx_errors = keyboard.errors; tb_status.demo_state = (uint32_t)keyboard.state;
        st_requests(&tc_state, &keyboard);
        st_service(&tc_state, now);
        midi_input(now);
        sysex_output(now);
        ui_service(&tc_state, &keyboard, ready, now);
        tb_status.loops++; tb_snapshot();
    }
}
