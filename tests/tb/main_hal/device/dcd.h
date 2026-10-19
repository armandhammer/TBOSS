/* SPDX-License-Identifier: GPL-3.0-or-later - test stand-in for TinyUSB's device events. */
#ifndef TB_TEST_MAIN_DCD
#define TB_TEST_MAIN_DCD
#include <stdint.h>
typedef enum { DCD_EVENT_INVALID, DCD_EVENT_BUS_RESET, DCD_EVENT_UNPLUGGED, DCD_EVENT_SOF, DCD_EVENT_SUSPEND,
               DCD_EVENT_RESUME, DCD_EVENT_SETUP_RECEIVED, DCD_EVENT_XFER_COMPLETE } dcd_eventid_t;
typedef enum { XFER_RESULT_SUCCESS, XFER_RESULT_FAILED } xfer_result_t;
typedef struct {
    uint8_t rhport, event_id;
    union { struct { uint8_t ep_addr, result; uint32_t len; } xfer_complete; };
} dcd_event_t;
#endif
