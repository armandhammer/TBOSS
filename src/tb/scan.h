/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TB_SCAN_H
#define TB_SCAN_H
#include <stdbool.h>
#include <stdint.h>
#define TS_KEYS 96u
#define TS_POSITIONS 16u
#define TS_ABI_BYTES 608u
#define TS_DIAG_BYTES (256u + TS_ABI_BYTES)
typedef struct { uint8_t port, bit; } ts_pin;
extern const ts_pin ts_selectors[6][4];
extern const ts_pin ts_inputs[6];
extern const uint8_t ts_channels[6];
/* Header32, averages384, position timestamps64: frozen until next request. */
typedef struct {
    uint32_t sequence, request, frame, start_us, end_us, duration_us, count, valid;
    uint32_t values[TS_KEYS], position_us[TS_POSITIONS];
} ts_snapshot;
typedef struct {
    uint32_t magic, version, capture_request, capture_ack;
    uint32_t arm_request, arm_consumed, polarity, press;
    uint32_t release, full_scale, deadband, pressure_ms;
    uint32_t settle_us, timeout_us, disable, engine_state;
    uint32_t frames, invalid, timeouts, selector;
    uint32_t duration_us, last_frame_ms, accepted, completed;
    uint32_t errors, obligations, uncertain, adc_sr;
    uint32_t adc_cr2, adc_ccr, reserved0, reserved1;
    ts_snapshot capture;
} ts_abi;
_Static_assert(sizeof(ts_snapshot) == 480u, "snapshot ABI");
_Static_assert(sizeof(ts_abi) == TS_ABI_BYTES, "control ABI");
extern volatile ts_abi ts_status;
typedef struct {
    uint32_t values[TS_KEYS], position_us[TS_POSITIONS];
    uint32_t start_us, end_us, duration_us, frame, last_ms;
    uint8_t position, completed_positions;
    bool valid;
} ts_scan;
typedef struct {
    void (*select)(uint8_t position);
    bool (*sample)(uint8_t bank, uint32_t timeout_us, uint16_t *value);
    bool (*settle)(uint32_t us);
    uint32_t (*micros)(void);
} ts_ops;
void ts_masks(uint8_t p, uint32_t reset[5], uint32_t set[5]);
/* One position per call. True only for a complete fresh 96-key frame. */
bool ts_step(ts_scan *s, ts_ops ops, uint32_t settle_us, uint32_t timeout_us, uint32_t now_ms);
void ts_capture(const ts_scan *s);
/* The n (1..TS_KEYS) pads reading highest, strongest first; on a tie the lower pad (pad test page). */
void ts_strongest(const uint32_t values[TS_KEYS], uint8_t *out, uint32_t n);
bool ts_hardware_init(void);
extern const ts_ops ts_hardware_ops;
#endif
