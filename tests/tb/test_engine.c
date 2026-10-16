/* SPDX-License-Identifier: GPL-3.0-or-later - standalone flash behaviour on the real engine.
 * The loop mirrors usb_main's arm/frame/step order (src/tb/main.c). */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now, ons[128], offs[128], channel_packets;
static uint8_t velocity[128], channel_value;
static bool mounted;
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token==e.issued+1u && p[2]<128u);
    if(p[0]==TK_ON) { ons[p[2]]++; velocity[p[2]]=p[3]; assert(p[1]==0x90u && p[3]>=1u && p[3]<=127u); }
    if(p[0]==TK_OFF) offs[p[2]]++;
    if(p[0]==TK_CHANNEL) { assert(p[1]==0xD0u && !p[3]); channel_packets++; channel_value=p[2]; }
    if(p[0]==TK_PRESSURE) assert(p[1]==0xA0u);
    return true;
}
static void tick(void) {
    now+=10u; scan.frame++; scan.last_ms=now; scan.valid=true;
    if(e.state==TK_IDLE) (void)tc_arm(&e,&scan,now,mounted);
    else (void)tc_frame(&e,&scan,now);
    tc_step(&e,now,mounted,writer);
    if(e.pending_id) tp_complete(&e,e.pending_id,4,true,now);
}
static void run(uint32_t ms) { for(uint32_t t=0;t<ms;t+=10u) tick(); }
int main(void) {
    tc_init(&tc_state);
    now=1000u;
    /* No USB: never arms, never asks. */
    run(2000u); assert(e.state==TK_IDLE && e.consumed_arm==0u);
    /* USB ready: arms within the 500ms back-off plus one frame. */
    mounted=true; run(600u); assert(e.state==TK_ACTIVE && e.consumed_arm==1u);
    /* No debugger heartbeat and past both the 3s watchdog and the 60s session timer. */
    run(70000u); assert(e.state==TK_ACTIVE);
    scan.values[51]=3000u; run(100u); assert(ons[48]==1u);
    scan.values[51]=0u; run(100u); assert(offs[48]==1u);
    /* USB loss stops the session; nothing restarts until USB is back. */
    tp_loss(&e); assert(e.state==TK_STOPPED);
    mounted=false; run(1000u); assert(e.state!=TK_ACTIVE);
    /* A pad held while re-arming: arm refuses and retries until it is released. */
    scan.values[3]=3000u; mounted=true;
    uint32_t attempts=0u, last=0u;
    for(uint32_t t=0;t<3000u;t+=10u) {
        tick(); assert(e.state!=TK_ACTIVE);
        if(e.consumed_arm && e.consumed_arm!=last) { attempts++; last=e.consumed_arm; }
    }
    assert(attempts>=2u); /* retried about once a second, never armed while held */
    /* Fresh session after release; layout follows the current root/scale. */
    tc_state.tonic=2u;
    scan.values[3]=0u; run(1200u); assert(e.state==TK_ACTIVE && e.layout.tonic==2u);
    tp_notes d; assert(tc_pad(e.layout,55u,&d) && d.count==1u);
    scan.values[55]=3000u; run(100u); assert(ons[d.notes[0]]==1u);
    scan.values[55]=0u; run(100u);
    for(unsigned p=0;p<128u;p++) assert(ons[p]==offs[p]);
    /* Velocity from onset pressure: just past press (2600) -> 9, full scale -> 127. */
    tp_notes v; assert(tc_pad(e.layout,51u,&v) && v.count==1u);
    scan.values[51]=2600u; run(100u); assert(velocity[v.notes[0]]==9u);
    scan.values[51]=0u; run(100u);
    scan.values[51]=4000u; run(100u); assert(velocity[v.notes[0]]==127u);
    scan.values[51]=0u; run(100u);
    tc_state.vel_mode=TC_VEL_FIXED; scan.values[51]=2600u; run(100u); assert(velocity[v.notes[0]]==100u);
    scan.values[51]=0u; run(100u); tc_state.vel_mode=TC_VEL_DYNAMIC;
    /* Channel aftertouch follows the strongest held pad, chords included, then 0. */
    tc_state.at_mode=TC_AT_CHANNEL; channel_packets=0u;
    scan.values[3]=3000u; run(200u); assert(channel_packets>=1u && channel_value==76u);
    scan.values[3]=4000u; run(200u); assert(channel_value==127u);
    scan.values[3]=0u; run(200u); assert(channel_value==0u);
    uint32_t settled=channel_packets; run(500u); assert(channel_packets==settled); /* no chatter */
    tc_state.at_mode=TC_AT_OFF; scan.values[55]=3500u; run(300u);
    assert(channel_packets==settled); scan.values[55]=0u; run(200u);
    tc_state.at_mode=TC_AT_POLY;
    for(unsigned p=0;p<128u;p++) assert(ons[p]==offs[p]);
    puts("PASS: standalone flash mode: no arm without USB, auto-arm after 500ms, no host watchdog/session expiry, notes on/off, loss stops, held pad blocks re-arm, re-arm keeps root, dynamic/fixed velocity, channel/off aftertouch");
    return 0;
}
