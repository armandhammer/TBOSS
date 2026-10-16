/* SPDX-License-Identifier: GPL-3.0-or-later - original F407 polling transaction engine. */
#include "i2c.h"
#include <stddef.h>
typedef struct {
    tc_bus *b;
    uint32_t start, budget, polls, status;
} transfer;
static uint32_t rd(transfer *t, enum tc_reg r) { return t->b->io.read(t->b->io.context,r); }
static void wr(transfer *t, enum tc_reg r, uint32_t v) { t->b->io.write(t->b->io.context,r,v); }
static uint32_t lock(transfer *t) { return t->b->io.lock(t->b->io.context); }
static void unlock(transfer *t, uint32_t mask) { t->b->io.unlock(t->b->io.context,mask); }
static bool scl_high(const tc_pins *p) {
    /* Clients may stretch SCL; allow ~1ms (200 x 5us), then give up. */
    for (uint32_t i=0;i<200u;i++) {
        if (p->read_scl(p->context)) return true;
        p->delay_us(p->context,5u);
    }
    return false;
}
uint32_t tc_bus_clear(const tc_pins *p) {
    p->sda(p->context,true); p->scl(p->context,true); p->delay_us(p->context,5u);
    if (!scl_high(p)) return TC_CLEAR_SCL_STUCK;
    if (p->read_sda(p->context)) return 0u;
    uint32_t pulses=0;
    while (!p->read_sda(p->context)) {
        if (pulses==9u) return TC_CLEAR_SDA_LOW|TC_CLEAR_FAILED|pulses;
        p->scl(p->context,false); p->delay_us(p->context,5u);
        p->scl(p->context,true);
        if (!scl_high(p)) return TC_CLEAR_SDA_LOW|TC_CLEAR_SCL_STUCK|pulses;
        p->delay_us(p->context,5u); pulses++;
    }
    /* STOP: SDA low while SCL low, raise SCL, then SDA rises with SCL high. */
    p->scl(p->context,false); p->delay_us(p->context,5u);
    p->sda(p->context,false); p->delay_us(p->context,5u);
    p->scl(p->context,true);
    if (!scl_high(p)) { p->sda(p->context,true); return TC_CLEAR_SDA_LOW|TC_CLEAR_SCL_STUCK|pulses; }
    p->delay_us(p->context,5u);
    p->sda(p->context,true); p->delay_us(p->context,5u);
    return TC_CLEAR_SDA_LOW|pulses;
}
void tc_bus_init(tc_bus *b, tc_io io) {
    *b=(tc_bus){.io=io, .enabled=io.read && io.write && io.micros && io.lock && io.unlock};
}
bool tc_timing(uint32_t core, uint32_t cfgr, uint32_t *cr2, uint32_t *ccr, uint32_t *trise) {
    /* board.c: HCLK 48 MHz, PPRE1 /2 -> PCLK1 24 MHz. Fast mode 400 kHz (as the
     * original firmware uses): F/S, duty 2, CCR=24M/(3*400k)=20; TRISE=300ns*24M+1. */
    if (!cr2 || !ccr || !trise || core!=48000000u ||
        (cfgr & 0xfcfcu)!=0x1008u) return false;
    *cr2=24u; *ccr=0x8000u|20u; *trise=8u; return true;
}
static bool expired(transfer *t) {
    t->b->elapsed_us=t->b->io.micros(t->b->io.context)-t->start;
    return t->b->elapsed_us>=t->budget || ++t->polls>=4096u;
}
static uint32_t fault(uint32_t sr1) {
    if (sr1 & (TC_BERR|TC_ARLO|TC_OVR)) return BI_BUS;
    return sr1 & TC_AF ? BI_NACK : BI_OK;
}
static bool event(transfer *t, uint32_t bits, uint32_t phase) {
    t->b->phase=phase;
    for (;;) {
        if (expired(t)) { t->status=BI_TIMEOUT; return false; }
        t->b->sr1=rd(t,TC_SR1);
        t->status=fault(t->b->sr1);
        if (t->status!=BI_OK) return false;
        if ((t->b->sr1 & bits)==bits) return true;
    }
}
static bool stop_done(transfer *t) {
    for (;;) {
        if (expired(t)) return false;
        t->b->sr1=rd(t,TC_SR1);
        if (fault(t->b->sr1)!=BI_OK) return false;
        t->b->sr2=rd(t,TC_SR2);
        if (!(t->b->sr2 & (TC_BUSY|TC_MSL)) && !(rd(t,TC_CR1)&(TC_START|TC_STOP))) return true;
    }
}
static bool address(transfer *t, bool receive) {
    uint32_t mask=lock(t);
    /* POS=1/ACK=0 BEFORE clearing receive ADDR for N=2. No intervening
     * SR2 read; no interrupt can delay the ADDR-clear ordering. */
    wr(t,TC_CR1,TC_PE | (receive ? TC_POS : TC_ACK));
    t->b->sr1=rd(t,TC_SR1);
    if (!(t->b->sr1&TC_ADDR) || fault(t->b->sr1)!=BI_OK) {
        t->status=fault(t->b->sr1);
        if (t->status==BI_OK) t->status=BI_CORRUPT;
        unlock(t,mask); return false;
    }
    t->b->sr2=rd(t,TC_SR2);
    unlock(t,mask);
    if ((t->b->sr2&(TC_BUSY|TC_MSL))!=(TC_BUSY|TC_MSL)) {
        t->status=BI_BUS; return false;
    }
    return true;
}
struct bi_reply tc_i2c_read(void *context, uint8_t a, uint8_t reg,
    uint32_t generation, uint32_t now, uint32_t budget) {
    tc_bus *b=context;
    struct bi_reply reply={.status=BI_BLOCKED,.generation=generation,.timestamp_ms=now};
    if (!b || !b->enabled || b->latched) return reply;
    if ((a!=0x35 && a!=0x36 && a!=0x38 && a!=0x39) || reg!=0xaa || budget!=BI_BUDGET_US)
        return reply;
    transfer t={.b=b,.start=b->io.micros(b->io.context),.budget=budget,.status=BI_OK};
    b->phase=TC_IDLE; b->cleanup=BI_OK; b->elapsed_us=0;
    if (rd(&t,TC_CR1)!=TC_PE) { t.status=BI_CORRUPT; goto done; }
    /* Stuck BUSY is bounded and terminal; no synthetic GPIO recovery clocks. */
    for (;;) {
        if (expired(&t)) { t.status=BI_TIMEOUT; goto done; }
        b->sr1=rd(&t,TC_SR1);
        t.status=fault(b->sr1);
        if (t.status!=BI_OK) goto done;
        if (b->sr1 & ~(uint32_t)TC_TXE) { t.status=BI_CORRUPT; goto done; }
        b->sr2=rd(&t,TC_SR2);
        if (!(b->sr2&(TC_BUSY|TC_MSL))) break;
    }
    wr(&t,TC_CR1,TC_PE|TC_ACK|TC_START);
    if (!event(&t,TC_SB,TC_BEGIN)) goto done;
    wr(&t,TC_DR,(uint32_t)a<<1);
    if (!event(&t,TC_ADDR,TC_WRITE_ADDR) || !address(&t,false)) goto done;
    if (!event(&t,TC_TXE,TC_POINTER)) goto done;
    wr(&t,TC_DR,reg);
    if (!event(&t,TC_BTF,TC_POINTER)) goto done;
    wr(&t,TC_CR1,TC_PE|TC_ACK|TC_START);
    if (!event(&t,TC_SB,TC_RESTART)) goto done;
    wr(&t,TC_DR,((uint32_t)a<<1)|1u);
    if (!event(&t,TC_ADDR,TC_READ_ADDR) || !address(&t,true)) goto done;
    if (!event(&t,TC_BTF,TC_RECEIVE)) goto done;
    {
        uint32_t mask=lock(&t);
        wr(&t,TC_CR1,TC_PE|TC_POS|TC_STOP);
        uint32_t lo=rd(&t,TC_DR), hi=rd(&t,TC_DR);
        unlock(&t,mask);
        if (lo>255u || hi>255u) { t.status=BI_PARTIAL; goto done; }
        reply.data[0]=(uint8_t)lo; reply.data[1]=(uint8_t)hi;
    }
    b->phase=TC_FINISH;
    if (!stop_done(&t)) { t.status=BI_TIMEOUT; goto done; }
    wr(&t,TC_CR1,TC_PE);
    reply.count=2;
done:
    if (t.status!=BI_OK) {
        reply.data[0]=0; reply.data[1]=0; reply.count=0;
        bool nack=t.status==BI_NACK &&
            (b->phase==TC_WRITE_ADDR || b->phase==TC_POINTER || b->phase==TC_READ_ADDR) &&
            !(b->sr1&(TC_ADDR|TC_ARLO|TC_BERR|TC_OVR));
        b->cleanup=BI_BUS;
        if (nack) {
            b->sr2=rd(&t,TC_SR2);
            if ((b->sr2&(TC_MSL|TC_BUSY))==(TC_MSL|TC_BUSY)) {
                wr(&t,TC_CR1,TC_PE|TC_STOP);
                wr(&t,TC_SR1,0xffffu & ~(uint32_t)TC_AF);
                if (stop_done(&t)) { wr(&t,TC_CR1,TC_PE); b->cleanup=BI_OK; }
            }
        }
        if (b->cleanup!=BI_OK) b->latched=true;
    }
    b->fault=t.status;
    reply.status=t.status; reply.elapsed_us=b->elapsed_us;
    return reply;
}
