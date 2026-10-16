/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TC_I2C_H
#define TC_I2C_H
#include "input.h"
enum tc_reg { TC_CR1, TC_SR1, TC_SR2, TC_DR };
enum { TC_PE=1, TC_START=0x100, TC_STOP=0x200, TC_ACK=0x400, TC_POS=0x800,
    TC_SB=1, TC_ADDR=2, TC_BTF=4, TC_TXE=0x80, TC_BERR=0x100,
    TC_ARLO=0x200, TC_AF=0x400, TC_OVR=0x800, TC_MSL=1, TC_BUSY=2 };
typedef struct {
    void *context;
    uint32_t (*read)(void *, enum tc_reg);
    void (*write)(void *, enum tc_reg, uint32_t);
    uint32_t (*micros)(void *);
    /* Short critical sections for two-byte ADDR/STOP sequences only. */
    uint32_t (*lock)(void *);
    void (*unlock)(void *, uint32_t);
} tc_io;
typedef struct {
    tc_io io;
    uint32_t sr1, sr2, phase, fault, cleanup, elapsed_us;
    uint32_t recovery; /* startup bus clear: TC_CLEAR_* flags | SCL pulses used */
    bool enabled, latched;
} tc_bus;
/* Startup bus clear (I2C spec UM10204 3.1.16) on open-drain GPIO, before the pins
 * go to the peripheral. A client left mid-byte by a debugger reset can hold SDA low
 * and make BUSY permanent. Up to nine SCL pulses with SDA released, then STOP.
 * Never drives SDA low while SCL is high except as STOP's preamble, so no START,
 * address or data reaches the clients. Every wait is iteration-bounded. */
typedef struct {
    void *context;
    void (*scl)(void *, bool high);
    void (*sda)(void *, bool high);
    bool (*read_scl)(void *);
    bool (*read_sda)(void *);
    void (*delay_us)(void *, uint32_t);
} tc_pins;
enum { TC_CLEAR_SDA_LOW=0x100, TC_CLEAR_SCL_STUCK=0x200, TC_CLEAR_FAILED=0x400 };
uint32_t tc_bus_clear(const tc_pins *p);
enum tc_phase { TC_IDLE, TC_BEGIN, TC_WRITE_ADDR, TC_POINTER,
    TC_RESTART, TC_READ_ADDR, TC_RECEIVE, TC_FINISH };
/* No peripheral reset, runtime recovery or client configuration writes. A clean
 * NACK is recoverable; uncertain peripheral errors latch the bus, leaving pad/USB
 * service running. */
void tc_bus_init(tc_bus *b, tc_io io);
struct bi_reply tc_i2c_read(void *context, uint8_t address, uint8_t reg,
    uint32_t generation, uint32_t timestamp_ms, uint32_t budget_us);
bool tc_hardware_init(tc_bus *b);
/* Mid-session recovery after a latched fault: RCC-reset I2C1 only, then the same
 * init as at startup (bus clear included). No client configuration writes. */
bool tc_hardware_recover(tc_bus *b);
bool tc_timing(uint32_t core_hz, uint32_t cfgr, uint32_t *cr2,
    uint32_t *ccr, uint32_t *trise);
#endif
