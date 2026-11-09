/* SPDX-License-Identifier: GPL-3.0-or-later - the controls (controls.h): external control (2.1.0). The MIDI map:
 * controllers arriving on the Control channel (midi_in.c) set settings or do centre-button actions, and learning
 * them in Settings. */
#include "controls_internal.h"
#include "midi_in.h"
_Static_assert(TC_ACTIONS <= 64u, "cc_held has a bit per action");
_Static_assert(TC_CC_SLOTS <= 16u, "cc_picked and cc_down have a bit per slot");
bool tc_cc_mappable(uint8_t item) {
    if(item>=TC_MENU_ITEMS || tc_item_kind(item)!=TC_ITEM_VALUE || tc_menu_values(item)<2u) return false;
    if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) return false;
    switch(item) {
    case TC_MENU_CONTROL_CH: case TC_MENU_CC_ACTION: case TC_MENU_U3:
    case TC_MENU_SEQ_UNIT: /* stores the progression again on every change */
    case TC_MENU_LANE_EUCLID: case TC_MENU_LANE_ROTATE: /* their values follow the lane's length; Euclid refills it */
    case TC_MENU_LOOP_FROM: case TC_MENU_LOOP_TO: /* 2.4.0: as Rotate */
        return false;
    default: return true;
    }
}
bool tc_cc_slot_ok(const tc_cc_slot *s) {
    if(s->kind==TC_CC_EMPTY) return !s->cc && !s->target && !s->lo && !s->hi && !s->jump;
    if(s->cc>=TM_CONTROLS || s->jump>1u) return false;
    if(s->kind==TC_CC_ACTION) return s->target<TC_ACTIONS && !s->lo && !s->hi && !s->jump;
    if(s->kind!=TC_CC_SETTING || !tc_cc_mappable(s->target)) return false;
    uint8_t n=tc_menu_values(s->target);
    return s->lo<n && s->hi<n;
}
uint8_t tc_cc_count(const tc_controls *c) {
    uint8_t n=0;
    for(uint32_t i=0;i<TC_CC_SLOTS;i++) if(c->cc[i].kind!=TC_CC_EMPTY) n++;
    return n;
}
/* The actions held: every slot's button that is down. */
static void held_again(tc_controls *c) {
    c->cc_held=0u;
    for(uint32_t i=0;i<TC_CC_SLOTS;i++)
        if(c->cc[i].kind==TC_CC_ACTION && (((uint32_t)c->cc_down>>i)&1u)) c->cc_held|=(uint64_t)1u<<c->cc[i].target;
}
void tc_cc_restart(tc_controls *c, uint32_t slot) {
    if(slot>=TC_CC_SLOTS) return;
    uint16_t bit=(uint16_t)(1u<<slot);
    c->cc_pos[slot]=TC_CC_NONE; c->cc_set[slot]=TC_CC_NONE;
    c->cc_picked=(uint16_t)(c->cc_picked&~bit);
    if(c->cc_down&bit) {
        uint8_t act=c->cc[slot].target;
        c->cc_down=(uint16_t)(c->cc_down&~bit); held_again(c);
        if(act>=TC_ACT_ADD7 && act<=TC_ACT_SUS4) { if(c->live_update) c->relayout=true; if(c->arp!=TC_ARP_OFF) c->reshape=true; }
    }
}
void tc_cc_clear(tc_controls *c) {
    for(uint32_t i=0;i<TC_CC_SLOTS;i++) { tc_cc_restart(c,i); c->cc[i]=(tc_cc_slot){0,0,0,0,0,0}; }
}
/* Slot i's setting from controller value v (0..127). */
static void setting(tc_controls *c, tp_engine *e, uint32_t i, uint8_t v) {
    const tc_cc_slot *s=&c->cc[i];
    uint8_t item=s->target, n=tc_menu_values(item);
    if(n<2u) return;
    uint32_t lo=s->lo<n ? s->lo : n-1u, hi=s->hi<n ? s->hi : n-1u, steps=lo<=hi ? hi-lo : lo-hi;
    uint32_t old=c->cc_pos[i], pos=(v*steps+63u)/127u; /* the nearest step */
    /* The step in use kept until the controller is a quarter of a step past its edge with the next one; only for
     * steps wider than 4/3 of the controller's, so moving one at a time never skips a step. */
    int32_t off=(int32_t)(v*steps)-(int32_t)(127u*old);
    if(4u*steps<3u*127u && old<=steps && (pos==old+1u || pos+1u==old) && 4*(off<0 ? -off : off)<=3*127) pos=old;
    c->cc_pos[i]=(uint8_t)pos;
    uint8_t value=(uint8_t)(lo<=hi ? lo+pos : lo-pos), cur=tc_menu_value(c,item);
    uint16_t bit=(uint16_t)(1u<<i);
    if(!s->jump) {
        if(c->cc_set[i]!=cur) c->cc_picked=(uint16_t)(c->cc_picked&~bit); /* changed some other way: pick it up again */
        if(!(c->cc_picked&bit)) {
            int32_t before=old<=steps ? (int32_t)(lo<=hi ? lo+old : lo-old)-(int32_t)cur : 0, now=(int32_t)value-(int32_t)cur;
            bool reached=!now || (old<=steps && (before<0)!=(now<0)); /* there, or passed it since the last value */
            if(!reached) {
                if(pos==old) return;
                c->cc_item=item; c->cc_turn=(now<0)==(lo<=hi) ? 1u : 2u; c->last_action=TC_CC_VALUE;
                c->event_sequence=tc_inc(c->event_sequence);
                return;
            }
            c->cc_picked|=bit;
        }
    }
    if(value==cur && pos==old && c->cc_set[i]==cur) return; /* nothing new */
    if(value!=cur && tc_set_item(c,e,item,value) && c->last_action!=TC_SETTING) { /* LANE FILLED, THAT SET IS EMPTY ... */
        c->cc_set[i]=tc_menu_value(c,item); return;
    }
    c->cc_set[i]=tc_menu_value(c,item);
    if(c->last_action==TC_REJECT && c->cc_set[i]!=value) return; /* OUT OF RANGE (the pad thresholds' gaps) */
    if(c->ui==TC_UI_EDIT && c->menu==item) c->edit_value=c->cc_set[i];
    c->cc_item=item; c->cc_turn=0u; c->last_action=TC_CC_VALUE;
    c->event_sequence=tc_inc(c->event_sequence);
}
/* Slot i's button from controller value v: pressed at 64 and up. */
static void button(tc_controls *c, tp_engine *e, uint32_t i, uint8_t v, uint32_t now) {
    uint16_t bit=(uint16_t)(1u<<i);
    bool down=v>=64u;
    if(down==((c->cc_down&bit)!=0u)) return;
    c->cc_down=down ? (uint16_t)(c->cc_down|bit) : (uint16_t)(c->cc_down&~bit);
    held_again(c);
    uint8_t act=c->cc[i].target;
    /* A chord shape held: as a centre button's (controls.c tc_poll). */
    if(act>=TC_ACT_ADD7 && act<=TC_ACT_SUS4) { if(!down && c->live_update) c->relayout=true; if(c->arp!=TC_ARP_OFF) c->reshape=true; }
    c->event_sequence=tc_inc(c->event_sequence);
    if(!down || act==TC_ACT_SHIFT) return; /* Shift while held (tc_shift); the screen shows it */
    tc_action(c,e,act,now);
}
/* In Settings, a controller moved while an item is open: mapped to it (true; the move itself does nothing else).
 * One already mapped only once learning is armed, or for Map button. */
static bool learn(tc_controls *c) {
    uint8_t item=c->menu, cc=tm.cc_last;
    bool action=item==TC_MENU_CC_ACTION;
    if(c->ui!=TC_UI_EDIT || (!action && !tc_cc_mappable(item)) || cc>=TM_CONTROLS) return false;
    uint32_t slot=TC_CC_SLOTS, mapped=TC_CC_SLOTS;
    for(uint32_t i=TC_CC_SLOTS;i-->0u;) {
        if(c->cc[i].kind==TC_CC_EMPTY) slot=i;
        else if(c->cc[i].cc==cc) mapped=i;
    }
    if(mapped<TC_CC_SLOTS && !c->cc_learn && !action) return false;
    uint8_t v;
    (void)tm_cc_take(cc,&v); /* this move is the learning */
    if(mapped<TC_CC_SLOTS) slot=mapped;
    if(slot>=TC_CC_SLOTS) { c->last_action=TC_CC_FULL; c->event_sequence=tc_inc(c->event_sequence); return true; }
    tc_cc_restart(c,slot);
    c->cc[slot]=action ? (tc_cc_slot){TC_CC_ACTION,cc,c->edit_value<TC_ACTIONS ? c->edit_value : (uint8_t)TC_ACT_NONE,0,0,0} :
                         (tc_cc_slot){TC_CC_SETTING,cc,item,0,(uint8_t)(tc_menu_values(item)-1u),0};
    c->cc_learn=false; c->cc_learned=(uint8_t)slot; c->cc_learned_cc=cc; c->changes++;
    /* 2.3.2: a knob sends a stream; the rest of this turn doesn't set the setting (nor hide CCnn = ...) until the item closes */
    c->cc_hush=action ? 0u : (uint8_t)(cc+1u);
    c->last_action=TC_CC_LEARNED; c->event_sequence=tc_inc(c->event_sequence);
    return true;
}
void tc_cc_in(tc_controls *c, tp_engine *e, uint32_t now) {
    tm.cc_channel=c->control_ch<TC_CONTROL_CHANNELS ? c->control_ch : 0u;
    if(c->ui!=TC_UI_EDIT) c->cc_learn=false;
    if(c->cc_hush && (c->ui!=TC_UI_EDIT || c->cc_learn || c->menu!=c->cc[c->cc_learned%TC_CC_SLOTS].target ||
                      c->cc[c->cc_learned%TC_CC_SLOTS].kind!=TC_CC_SETTING)) c->cc_hush=0u;
    if(!tm.cc_channel) { /* off: nothing held */
        for(uint32_t i=0;i<TC_CC_SLOTS;i++) if(((uint32_t)c->cc_down>>i)&1u) tc_cc_restart(c,i);
        c->cc_seen=tm.cc_seen; return;
    }
    if(tm.cc_seen!=c->cc_seen && c->dark!=TC_SHUT_DOWN) (void)learn(c);
    c->cc_seen=tm.cc_seen;
    for(uint8_t cc=0;cc<TM_CONTROLS;cc++) {
        uint8_t v;
        if(!tm_cc_take(cc,&v) || c->dark==TC_SHUT_DOWN) continue; /* shut down: only centre wakes the board */
        if(cc+1u==c->cc_hush) continue; /* 2.3.2: just learned, its item still open */
        if(c->dark) tc_wake(c);
        c->touched_at=now;
        for(uint32_t i=0;i<TC_CC_SLOTS;i++) {
            if(c->cc[i].cc!=cc) continue;
            if(c->cc[i].kind==TC_CC_SETTING) setting(c,e,i,v);
            else if(c->cc[i].kind==TC_CC_ACTION) button(c,e,i,v,now);
        }
    }
}
/* 2.3.2: a transfer to the computer normally completes within a millisecond. Busy for TC_USB_STALL_MS means the computer
 * (its MIDI driver or service) stopped reading: the notes and the editor's replies wait, the pads seem dead. Said once
 * when it starts and once when it reads again; nothing when the cable goes or the computer sleeps. */
void tc_usb_watch(tc_controls *c, bool connected, bool busy, uint32_t now) {
    if(!connected) { c->usb_busy=false; c->usb_stalled=false; return; }
    if(!busy) {
        if(c->usb_stalled) { c->last_action=TC_USB_BACK; c->event_sequence=tc_inc(c->event_sequence); }
        c->usb_busy=false; c->usb_stalled=false; return;
    }
    if(!c->usb_busy) { c->usb_busy=true; c->usb_busy_at=now; return; }
    if(!c->usb_stalled && now-c->usb_busy_at>=TC_USB_STALL_MS) {
        c->usb_stalled=true; c->last_action=TC_USB_STALLED; c->event_sequence=tc_inc(c->event_sequence);
    }
}
