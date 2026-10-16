/* SPDX-License-Identifier: GPL-3.0-or-later - I2C transaction engine and bus clear on a wire model. */
/* Independent register peripheral fake. No host MMIO or device interfaces. */
#include "../../src/tb/i2c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct event { unsigned write; enum tc_reg reg; uint32_t value; };
struct fake {
    uint32_t cr1, state, time, reads, starts, dr_reads, writes, consumed;
    uint32_t inject_state, inject_error, stall_state, nack_address, address, finish_error;
    uint32_t locked, locks;
    bool stuck_busy, stuck_stop, frozen_clock, corrupt_second, zero, af_cleared;
    struct event log[12000];
    unsigned logged;
};
static void log_event(struct fake *f, unsigned write, enum tc_reg reg, uint32_t v)
{
    assert(f->logged<12000);
    f->log[f->logged++]=(struct event){write,reg,v};
}
static uint32_t read_reg(void *context, enum tc_reg reg)
{
    struct fake *f=context;
    uint32_t value=0;
    f->reads++;
    if (reg==TC_CR1) value=f->cr1;
    else if (reg==TC_SR1) {
        if (f->inject_state==f->state)
            value=(f->inject_error==TC_AF && f->af_cleared) ? 0 : f->inject_error;
        else if (f->stall_state==f->state) value=0;
        else if ((f->state==2 || f->state==6) && f->address==f->nack_address)
            value=f->af_cleared ? 0 : TC_AF;
        else if (f->state==1 || f->state==5) value=TC_SB;
        else if (f->state==2 || f->state==6) value=TC_ADDR;
        else if (f->state==4 || f->state==7) value=TC_BTF|TC_TXE;
        else if (f->state==3 || f->state==0) value=TC_TXE;
    } else if (reg==TC_SR2) {
        value=(f->state || f->stuck_busy) ? TC_MSL|TC_BUSY : 0;
        bool af=(f->address==f->nack_address ||
                 (f->inject_state==f->state && f->inject_error==TC_AF));
        if (f->state==2 && !af) {
            assert((f->cr1&(TC_ACK|TC_POS))==TC_ACK); f->state=3;
        } else if (f->state==6 && !af) {
            assert(f->locked); assert((f->cr1&(TC_ACK|TC_POS))==TC_POS); f->state=7;
        }
    } else {
        assert(reg==TC_DR && f->state==8 && f->locked);
        f->dr_reads++; f->consumed++;
        value=f->consumed%2u || f->zero ? 0 : (f->corrupt_second ? 256u : 0x80u);
        if (!(f->consumed%2u) && !f->stuck_stop) {
            f->state=0; f->cr1=TC_PE|TC_POS;
            if (f->finish_error) { f->inject_state=0; f->inject_error=f->finish_error; }
        }
    }
    log_event(f,0,reg,value);
    return value;
}
static void write_reg(void *context, enum tc_reg reg, uint32_t value)
{
    struct fake *f=context;
    f->writes++;
    log_event(f,1,reg,value);
    if (reg==TC_CR1) {
        assert(!(value & ~(uint32_t)(TC_PE|TC_ACK|TC_POS|TC_STOP|TC_START)));
        f->cr1=value;
        if (value&TC_START) {
            assert(f->state==0 || f->state==4);
            f->state=f->state==0 ? 1u : 5u; f->starts++;
            f->cr1&=~(uint32_t)TC_START;
        }
        if (value&TC_STOP) {
            if (f->state==7) f->state=8;
            else {
                assert(f->state==2 || f->state==4 || f->state==6);
                if (!f->stuck_stop) { f->state=0; f->cr1=TC_PE; }
            }
        }
    } else if (reg==TC_DR) {
        if (f->state==1) {
            assert(!(value&1u)); f->address=value>>1; f->state=2; f->af_cleared=false;
        }
        else if (f->state==3) { assert(value==0xaa); f->state=4; }
        else { assert(f->state==5 && value==((f->address<<1)|1u)); f->state=6; }
    } else {
        assert(reg==TC_SR1 && value==0xfbffu); /* AF-only W0C */
        f->af_cleared=true;
    }
}
static uint32_t now(void *context)
{
    struct fake *f=context;
    if (!f->frozen_clock) f->time++;
    return f->time;
}
static uint32_t irq_lock(void *context) {
    struct fake *f=context; uint32_t mask=f->locked; f->locked=1; f->locks++; return mask;
}
static void irq_unlock(void *context, uint32_t mask) { ((struct fake *)context)->locked=mask; }
static void setup(struct fake *f, tc_bus *b) {
    *f=(struct fake){.cr1=TC_PE,.inject_state=99,.stall_state=99,.nack_address=99};
    tc_bus_init(b,(tc_io){f,read_reg,write_reg,now,irq_lock,irq_unlock});
}
static struct bi_reply sample(tc_bus *b, uint8_t a) {
    return tc_i2c_read(b,a,0xaa,123,456,1000);
}
/* Wire-level bus model for the startup bus clear: a client holding SDA low
 * shifts its next bit after each SCL falling edge; hold>=1000 never releases. */
struct wire { bool scl, sda, scl_stuck; unsigned hold, rises, starts, stops, delays; };
static bool line_scl(struct wire *w) { return w->scl && !w->scl_stuck; }
static bool line_sda(struct wire *w) { return w->sda && !w->hold; }
static void drive(struct wire *w, bool *pin, bool high) {
    bool scl=line_scl(w), sda=line_sda(w);
    *pin=high;
    if(scl && !line_scl(w) && w->hold && w->hold<1000u) w->hold--;
    if(!scl && line_scl(w)) w->rises++;
    if(scl && line_scl(w) && sda && !line_sda(w)) w->starts++;
    if(scl && line_scl(w) && !sda && line_sda(w)) w->stops++;
}
static void wire_scl(void *c, bool high) { struct wire *w=c; drive(w,&w->scl,high); }
static void wire_sda(void *c, bool high) { struct wire *w=c; drive(w,&w->sda,high); }
static bool wire_read_scl(void *c) { return line_scl(c); }
static bool wire_read_sda(void *c) { return line_sda(c); }
static void wire_delay(void *c, uint32_t us) { assert(us==5u); ((struct wire *)c)->delays++; }
static uint32_t clear(struct wire *w) {
    const tc_pins p={w,wire_scl,wire_sda,wire_read_scl,wire_read_sda,wire_delay};
    uint32_t r=tc_bus_clear(&p);
    assert(!w->starts && w->delays<2000u); /* never a START; every wait bounded */
    return r;
}
static void bus_clear(void) {
    struct wire w={.scl=true,.sda=true};
    assert(clear(&w)==0u && !w.rises && !w.stops);
    for(unsigned k=1;k<=9u;k++) {
        w=(struct wire){.scl=true,.sda=true,.hold=k};
        assert(clear(&w)==(TC_CLEAR_SDA_LOW|k) && w.rises==k+1u && w.stops==1u);
        assert(line_scl(&w) && line_sda(&w));
    }
    w=(struct wire){.scl=true,.sda=true,.hold=1000};
    assert(clear(&w)==(TC_CLEAR_SDA_LOW|TC_CLEAR_FAILED|9u) && w.rises==9u && !w.stops && w.sda);
    w=(struct wire){.scl=true,.sda=true,.scl_stuck=true,.hold=3};
    assert(clear(&w)==TC_CLEAR_SCL_STUCK && !w.rises && w.hold==3u);
}
int main(void) {
    struct fake f; tc_bus b;
    bus_clear();
    static const uint8_t addresses[]={0x35,0x36,0x38,0x39};
    static const enum tc_reg regs[]={TC_CR1,TC_DR,TC_CR1,TC_DR,TC_CR1,TC_DR,TC_CR1,TC_CR1,TC_CR1};
    static const uint32_t values[]={TC_PE|TC_ACK|TC_START,0x6a,TC_PE|TC_ACK,0xaa,
        TC_PE|TC_ACK|TC_START,0x6b,TC_PE|TC_POS,TC_PE|TC_POS|TC_STOP,TC_PE};
    setup(&f,&b);
    for(unsigned a=0;a<4;a++) {
        struct bi_reply r=sample(&b,addresses[a]);
        assert(r.status==BI_OK && r.count==2 && r.data[1]==0x80 && !r.data[0]);
        assert(r.generation==123 && r.timestamp_ms==456 && r.elapsed_us<1000);
        assert(!b.latched && !f.locked && f.locks==3u*(a+1u));
    }
    unsigned at=0;
    for(unsigned i=0;i<f.logged && at<9;i++) if(f.log[i].write) {
        assert(f.log[i].reg==regs[at] && f.log[i].value==values[at]); at++;
    }
    assert(at==9 && f.dr_reads==8 && f.starts==8);
    setup(&f,&b); f.zero=true;
    struct bi_reply r=sample(&b,0x35); assert(r.status==BI_OK && !r.data[0] && !r.data[1]);
    for(unsigned phase=2;phase<=6;phase+=2) {
        setup(&f,&b); f.inject_state=phase; f.inject_error=TC_AF;
        r=sample(&b,0x35); assert(r.status==BI_NACK && !b.latched && !r.count);
        assert(r.elapsed_us<1000 && b.cleanup==BI_OK);
    }
    setup(&f,&b); f.nack_address=0x35;
    assert(sample(&b,0x35).status==BI_NACK && !b.latched);
    assert(sample(&b,0x36).status==BI_OK); /* absent function client cannot starve nav */
    static const uint32_t errors[]={TC_BERR,TC_ARLO,TC_OVR};
    for(unsigned phase=1;phase<=7;phase++) {
        for(unsigned err=0;err<3;err++) {
            setup(&f,&b); f.inject_state=phase; f.inject_error=errors[err];
            r=sample(&b,0x35); assert(r.status==BI_BUS && b.latched && !r.count);
            unsigned count=f.logged;
            assert(sample(&b,0x36).status==BI_BLOCKED && count==f.logged && !f.locked);
        }
        setup(&f,&b); f.stall_state=phase;
        r=sample(&b,0x35); assert(r.status==BI_TIMEOUT && b.latched && r.elapsed_us==1000);
        assert(f.reads<4096 && !f.locked);
    }
    setup(&f,&b); f.frozen_clock=true; f.stall_state=5;
    assert(sample(&b,0x35).status==BI_TIMEOUT && b.latched && f.reads<4128);
    setup(&f,&b); f.time=UINT32_MAX-10u; assert(sample(&b,0x35).status==BI_OK);
    setup(&f,&b); f.stuck_busy=true;
    assert(sample(&b,0x35).status==BI_TIMEOUT && !f.writes && b.latched);
    setup(&f,&b); f.stuck_stop=true;
    assert(sample(&b,0x35).status==BI_TIMEOUT && b.latched);
    setup(&f,&b); f.nack_address=0x35; f.stuck_stop=true;
    assert(sample(&b,0x35).status==BI_NACK && b.latched && b.cleanup==BI_BUS);
    setup(&f,&b); f.corrupt_second=true;
    assert(sample(&b,0x35).status==BI_PARTIAL && b.latched);
    for(unsigned err=0;err<3;err++) {
        setup(&f,&b); f.finish_error=errors[err];
        r=sample(&b,0x35); assert(r.status!=BI_OK && !r.count && b.latched);
    }
    setup(&f,&b);
    assert(sample(&b,0x37).status==BI_BLOCKED && !f.logged);
    assert(tc_i2c_read(&b,0x35,0xab,1,2,1000).status==BI_BLOCKED && !f.logged);
    assert(tc_i2c_read(&b,0x35,0xaa,1,2,999).status==BI_BLOCKED && !f.logged);
    b.enabled=false; assert(sample(&b,0x35).status==BI_BLOCKED && !f.logged);
    uint32_t cr2, ccr, trise;
    assert(tc_timing(48000000,0x100a,&cr2,&ccr,&trise));
    assert(cr2==24 && ccr==(0x8000|20) && trise==8);
    assert(!tc_timing(16000000,0,&cr2,&ccr,&trise));
    assert(!tc_timing(48000000,0x100e,&cr2,&ccr,&trise));
    assert(!tc_timing(48000000,0x000a,&cr2,&ccr,&trise));
    puts("PASS: startup bus clear (idle, SDA held 1..9 bits, never released, SCL stuck; no START, bounded); actual transaction engine: 4 clients, write-AA/reSTART/two-byte NACK/STOP, critical ordering, zero, NACK, all-phase faults/timeouts, frozen/wrapped clock, BUSY/STOP/partial, strict 400kHz timing/allowlist");
    return 0;
}
