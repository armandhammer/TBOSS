/* SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard replacement firmware, flash resident.
 * Foreground loop: USB, two pad-scan positions per pass (the engine takes their pads at once), controls
 * every 10 ms, the MIDI engine with per-packet USB receipts, MIDI input (lights, clock, SysEx).
 * Diagnostics block at 0x20018000, readable over SWD while running.
 * SysEx replies use transfer mode: the engine first drains (every Note Off sent
 * and receipted), then the reply streams through the TinyUSB FIFO without
 * per-packet receipts; playing resumes 1 s after the last request. 1.9.4: a short
 * reply to a request that only reads (sx_inline) goes out between two of the engine's
 * packets instead: the engine's writes wait (a few ms) while it streams, nothing stops.
 * TRS MIDI out: every packet USB accepts is also queued for the TRS jack (when TRS
 * out is on). With no computer (USB not ready 3 s after start), a session plays to
 * the TRS jack alone and each packet counts as delivered once queued. */
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
#include "sequence.h"
#include "jack.h"
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
enum { TX_IDLE, TX_DRAIN, TX_SEND, TX_LINGER, TX_HOLD, TX_INLINE };
/* TX_*; receipts are ignored while sending. TX_HOLD: an inline reply waits for the engine's last packet
 * (its receipt still counts); TX_INLINE: it streams, the engine's writes wait. */
static volatile uint32_t transfer;
static uint32_t tx_len, tx_pos, tx_at;
static uint8_t tx_cable; /* 2.0.6: a reply goes back on the request's cable (midi_in.h) */
static bool serial_session;   /* this engine session plays to the TRS jack only */
static uint32_t serial_token; /* a TRS-only packet queued in this pass, completed after tc_step */
static void observe_loss(void) {
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    bool had = lost; lost = false;
    __set_PRIMASK(mask);
    if (had && !serial_session) tp_loss(&keyboard); /* USB events don't end a TRS-only session */
}
static bool write_packet(const uint8_t packet[4], uint32_t token) {
    uint8_t bytes[3];
    uint32_t n = sm_packet_bytes(packet, bytes);
    if (serial_session) {
        if (!sm_push(&sm_out, bytes, n)) return false; /* also while draining after TRS out was turned off */
        jh_kick(); serial_token = token;
        return true;
    }
    if (lost || !tud_mounted() || tud_suspended()) return false;
    if (transfer == TX_HOLD || transfer == TX_INLINE) return false; /* an inline reply first (1.9.4) */
    bool trs = tc_state.trs_out != 0u;
    if (trs && sm_space(&sm_out) < n) return false; /* the jack is slower than USB: wait for room */
    wire_token = token; __DMB();
    if (tud_midi_packet_write(packet)) {
        if (trs && sm_push(&sm_out, bytes, n)) jh_kick();
        return true;
    }
    wire_token = 0u;
    return false;
}
/* Settings -> About -> I/O test: middle C on the melody channel to the TRS jack, on for
 * 300 ms of every second, whatever the TRS out setting; stops with a Note Off. */
/* The Full layout's chords come from the external memory chip. Read them here, when the key,
 * scale, progression or step pattern changes, never while a pad or step plays (interrupts are
 * masked there; a miss there plays built chords once and is read here next, tc_full_fetch): a
 * progression recorded with Full chords (its own scales, whatever the board's layout), chord lanes'
 * borrowed scales and the layout's own, up to the cache's 8, the layout's last so it stays. */
static void prepare_full(void) {
    static uint32_t seen = UINT32_MAX, seen_sq = UINT32_MAX, seen_ln = UINT32_MAX;
    while (tc_full_fetch()) {}
    if (tc_state.changes == seen && sq_changes == seen_sq && ln_changes == seen_ln) return;
    seen = tc_state.changes; seen_sq = sq_changes; seen_ln = ln_changes;
    tp_layout layout = tc_layout(&tc_state), want[7];
    uint32_t n = 0;
    for (uint32_t i = 0; sq.chords == TC_CHORDS_FULL && i < sq.count && n < 4u; ++i) want[n++] = sq_layout(&sq, layout, &sq.entries[i]);
    for (uint32_t i = 0; layout.chords == TC_CHORDS_FULL && i < LN_LANES; ++i) {
        const ln_lane *l = &ln.lanes[i];
        for (uint32_t s = 0; l->kind == LN_CHORDS && s < ln_length(l) && n < 7u; ++s) {
            uint8_t scale = l->steps[s].scale;
            if (!l->steps[s].on || !scale || !tc_scale_defined((uint8_t)(scale - 1u))) continue;
            want[n] = layout; want[n].mode = (uint8_t)(scale - 1u); n++;
        }
    }
    for (uint32_t i = 0; i < n; ++i) {
        bool again = want[i].chords == layout.chords && want[i].mode == layout.mode && want[i].tonic == layout.tonic;
        for (uint32_t j = 0; j < i; ++j) again = again || (want[j].mode == want[i].mode && want[j].tonic == want[i].tonic);
        if (!again) tc_full_prepare(want[i]);
    }
    tc_full_prepare(layout);
}
static void io_test(uint32_t now) {
    static uint32_t at;
    static bool on;
    bool active = tc_state.ui == TC_UI_EDIT && tc_state.menu == TC_MENU_IO_TEST;
    if (!active && !on) return;
    if (active && now - at < (on ? 300u : 700u)) return;
    uint8_t channel = tc_state.melody_ch & 15u;
    uint8_t message[3] = {(uint8_t)((on ? 0x80u : 0x90u) | channel), 60u, on ? 0u : 100u};
    if (!sm_push(&sm_out, message, 3u)) return;
    jh_kick(); on = !on; at = now;
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
        transfer != TX_SEND && transfer != TX_LINGER && transfer != TX_INLINE) {
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
    if (transfer == TX_SEND || transfer == TX_LINGER || transfer == TX_INLINE) return; /* SysEx stream: no engine receipts */
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
    if (!tm.sysex_ready || transfer == TX_DRAIN || transfer == TX_HOLD || transfer == TX_INLINE ||
        (transfer == TX_SEND && tx_pos < tx_len)) return;
    bool inline_ok = sx_inline(tm.sysex, tm.sysex_len);
    uint32_t len = sx_handle(tm.sysex, tm.sysex_len, &tc_state, &keyboard, now);
    tx_cable = tm.sysex_cable;
    tm_sysex_consumed();
    tx_at = now;
    if (!len) return;
    tx_len = len; tx_pos = 0;
    if (transfer == TX_IDLE && inline_ok && len <= SX_INLINE_MAX) { transfer = TX_HOLD; return; } /* 1.9.4 */
    if (transfer == TX_IDLE) {
        transfer = TX_DRAIN;
        uint32_t mask = __get_PRIMASK(); __disable_irq();
        tp_disable(&keyboard);
        __set_PRIMASK(mask);
    } else transfer = TX_SEND;
}
static void sysex_output(uint32_t now) {
    if (transfer == TX_HOLD) { /* the engine's packet on its way first, then nothing in the endpoint */
        if (!tud_mounted()) { transfer = TX_IDLE; return; }
        if (keyboard.pending_id || head != tail || usbd_edpt_busy(0u, 0x81u)) return;
        transfer = TX_INLINE;
    }
    if (transfer == TX_INLINE) {
        uint32_t began = tb_millis();
        while (tx_pos < tx_len && tud_mounted() && tb_millis() - began < 3u) {
            uint32_t left = tx_len - tx_pos, take = left < 3u ? left : 3u;
            uint8_t packet[4] = {0, 0, 0, 0};
            packet[0] = (uint8_t)((uint32_t)tx_cable << 4 | (left <= 3u ? 4u + take : 4u));
            for (uint32_t i = 0; i < take; ++i) packet[1u + i] = sx_out[tx_pos + i];
            if (tud_midi_packet_write(packet)) tx_pos += take;
            else tud_task_ext(0u, false);
        }
        /* Done once every byte has gone (a zero-length packet after a full 64 too): the engine's next
         * receipt is its own again. */
        if (!tud_mounted() || (tx_pos == tx_len && !usbd_edpt_busy(0u, 0x81u))) {
            uint32_t mask = __get_PRIMASK(); __disable_irq();
            tail = head; wire_token = 0u; transfer = TX_IDLE;
            __set_PRIMASK(mask);
        }
        return;
    }
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
            packet[0] = (uint8_t)((uint32_t)tx_cable << 4 | (left <= 3u ? 4u + take : 4u));
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
    sx_scanner = &scanner; /* the PADS request's readings (1.9.0) */
    const tusb_rhport_init_t init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL};
    if (!tusb_init(0u, &init)) fail(0x109u);
    if (!ts_hardware_init()) fail(0x201u);
    (void)tc_hardware_init(&bus); /* missing controls never disable pads/USB */
    tc_publish(&tc_state, &bus, tc_status);
    (void)ui_init(); /* a missing display or LED chain never disables playing */
    if (nr_init()) tc_full_reader(nr_read); /* read-only; absence disables the backup and the original's Full chords */
    jh_init();       /* TRS MIDI out and the pedal input */
    tb_status.stage = 3u;
    for (;;) {
        observe_loss();
        tud_task_ext(0u, false);
        /* Two pad-scan positions a pass (1.5.0; was one): a 96-pad frame every 8 passes, ~3.5 ms. */
        bool fresh = false;
        for (uint32_t i = 0; i < 2u && !fresh; ++i)
            fresh = ts_step(&scanner, ts_hardware_ops, ts_status.settle_us, ts_status.timeout_us, tb_millis());
        (void)tc_poll(&tc_state, &keyboard, serviced_read, &bus, tb_millis());
        recover_bus(tb_millis());
        prepare_full();
        tc_publish(&tc_state, &bus, tc_status);
        if (fresh) ts_capture(&scanner);
        uint32_t mask = __get_PRIMASK(); __disable_irq();
        tc_full_quick(true); /* no chip reads from here to the PRIMASK restore (1.7.0) */
        observe_loss();
        uint32_t now = tb_millis();
        bool usb_ready = tud_mounted() && !tud_suspended();
        /* Each session picks its route when it starts; a computer appearing during a
         * TRS-only session ends it gracefully (Note Offs on the jack), then USB takes over. */
        if (keyboard.state == TK_IDLE) serial_session = !usb_ready && tc_state.trs_out && now >= 3000u;
        else if (serial_session && (usb_ready || !tc_state.trs_out) && keyboard.state == TK_ACTIVE) tp_disable(&keyboard);
        bool ready = serial_session || usb_ready;
        /* A session starts from a complete frame; then the engine takes each position's pads as
         * they are read (1.5.0; before, whole frames only: up to ~30 ms later) and ends velocity
         * windows (engine_pads.c read_pad), so it runs every pass. */
        if (keyboard.state == TK_IDLE) { if (fresh && transfer == TX_IDLE) (void)tc_arm(&keyboard, &scanner, now, ready); }
        else (void)tc_frame(&keyboard, &scanner, now);
        tc_step(&keyboard, now, ready, write_packet);
        if (serial_token) { tp_complete(&keyboard, serial_token, 4u, true, now); serial_token = 0u; }
        tc_full_quick(false);
        __set_PRIMASK(mask);
        ts_status.arm_request = keyboard.consumed_arm; ts_status.arm_consumed = keyboard.consumed_arm;
        ts_status.engine_state = (uint32_t)keyboard.state;
        ts_status.accepted = keyboard.accepted; ts_status.completed = keyboard.completed;
        ts_status.errors = keyboard.errors; ts_status.obligations = tp_obligations(&keyboard);
        ts_status.uncertain = keyboard.uncertain ? 1u : 0u;
        tb_status.accepted = keyboard.accepted; tb_status.completed = keyboard.completed;
        tb_status.tx_errors = keyboard.errors; tb_status.demo_state = (uint32_t)keyboard.state;
        tc_pedal(&tc_state, (jh_levels() & 1u) != 0u, now);
        io_test(now);
        st_requests(&tc_state, &keyboard);
        st_service(&tc_state, now);
        midi_input(now);
        tc_program_in(&tc_state); /* 1.7.0: a Program Change on Program in's channel loads a preset (next pass) */
        sysex_output(now);
        ui_service(&tc_state, &keyboard, &scanner, usb_ready || serial_session, now);
        tb_status.loops++; tb_snapshot();
    }
}
