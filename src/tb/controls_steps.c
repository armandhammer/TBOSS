/* SPDX-License-Identifier: GPL-3.0-or-later - the controls (controls.h): the step screen (1.5.0, lanes.h). The
 * Sequence page's lane items, pads that turn steps on and off and pick their values, held steps'
 * fields (1.7.0) and step record. */
#include "controls_internal.h"
#include <string.h>
/* What a step tapped on plays before any value is picked: the first chord or scale step in root
 * position (bottom left), or the kick. */
ln_step tc_brush_default(uint8_t kind) {
    ln_step s; memset(&s,0,sizeof s);
    s.on=true; s.value=kind==LN_DRUMS ? (uint8_t)TP_DRUM_BASE : 3u;
    return s;
}
/* The step sequencer's lanes (1.5.0, lanes.h): the Sequence page's lane items set the lane Lane
 * chose (the step screen's up/down choose it too). A new kind starts picking from its default. */
void tc_lane_setting(tc_controls *c, uint8_t item, uint8_t v) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    switch(item) {
    case TC_MENU_LANE: c->lane=(uint8_t)(v%LN_LANES); return;
    case TC_MENU_LANE_KIND:
        if(l->kind!=v) c->brush[at]=tc_brush_default(v);
        l->kind=v<LN_KINDS ? v : (uint8_t)LN_OFF; break;
    case TC_MENU_LANE_LENGTH: ln_set_length(l,v+1u); break;
    case TC_MENU_LANE_RATE: ln_set_rate(l,v); break;
    case TC_MENU_LANE_CHANNEL: l->channel=v<LN_CHANNELS ? v : 0u; break;
    case TC_MENU_LANE_DIRECTION: l->direction=v<LN_DIRECTIONS ? v : (uint8_t)LN_FORWARD; break;
    case TC_MENU_LANE_EUCLID: case TC_MENU_LANE_ROTATE: /* 1.7.0: fills the lane's steps (lanes.c) */
        if(item==TC_MENU_LANE_ROTATE) c->lane_rotate[at]=(uint8_t)(v%ln_length(l));
        ln_euclid_fill(l,item==TC_MENU_LANE_EUCLID ? v : l->euclid,c->lane_rotate[at],c->brush[at]);
        for(uint32_t s=0;s<LN_STEPS;s+=LN_CHUNK) ln_changed(at,s);
        if(l->euclid) c->last_action=TC_EUCLID_FILLED;
        break;
    default: l->mute=v ? 1u : 0u; break;
    }
    ln_changed(at,LN_STEPS);
}
bool tc_lane_item(uint8_t item) {
    return (item>=TC_MENU_LANE && item<=TC_MENU_LANE_MUTE) || item==TC_MENU_LANE_DIRECTION || item==TC_MENU_LANE_EUCLID ||
           item==TC_MENU_LANE_ROTATE;
}
/* The step screen (1.5.0). Step record starts at the first step of the page shown. */
void tc_step_record(tc_controls *c) {
    uint32_t length=ln_length(&ln.lanes[c->lane%LN_LANES]);
    c->step_rec=!c->step_rec;
    if(!c->step_rec) { c->last_action=TC_STEP_STOPPED; return; }
    c->step_cursor=(uint8_t)(c->step_page*48u<length ? c->step_page*48u : 0u);
    c->step_page=(uint8_t)(c->step_cursor/48u); c->last_action=TC_STEP_RECORD;
}
/* Step record: the step at the cursor becomes s (off: a rest) and the cursor moves on, round the lane. */
static void step_fill(tc_controls *c, ln_step s) {
    ln_lane *l=&ln.lanes[c->lane%LN_LANES];
    uint32_t length=ln_length(l), at=c->step_cursor%length;
    l->steps[at]=s; ln_changed(c->lane%LN_LANES,at);
    c->step_cursor=(uint8_t)((at+1u)%length); c->step_page=(uint8_t)(c->step_cursor/48u);
}
/* 1.7.0: a held step's field one value on (d 1) or back (-1); an off step goes on first, with the
 * value a tap gives it. Velocity in fives (0 plays 100), gate in tens of percent (50 % stored as 0),
 * tie off/on, probability 10 % .. always, ratchet 1..8 strikes, condition in lanes.h's order. */
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v<lo ? lo : v>hi ? hi : v; }
void tc_step_field(ln_step *s, uint8_t field, int32_t d) {
    switch(field) {
    case TC_FIELD_VELOCITY: s->velocity=(uint8_t)clamp((s->velocity ? s->velocity : (int32_t)LN_VELOCITY_DEFAULT)+5*d,1,127); break;
    case TC_FIELD_GATE: { int32_t g=clamp((int32_t)ln_gate_pct(s)/10+d,1,10); s->gate=(uint8_t)(g==(int32_t)LN_GATE_PCT/10 ? 0 : g); break; }
    case TC_FIELD_TIE: s->tie=d>0; break;
    case TC_FIELD_PROBABILITY: s->probability=(uint8_t)clamp((s->probability<LN_PROBABILITIES ? s->probability : 9)-d,0,(int32_t)LN_PROBABILITIES-1); break;
    case TC_FIELD_RATCHET: s->ratchet=(uint8_t)clamp((int32_t)(s->ratchet&7u)+d,0,(int32_t)LN_RATCHETS-1); break;
    default: s->condition=(uint8_t)clamp((s->condition<LN_CONDITIONS ? s->condition : 0)+d,0,(int32_t)LN_CONDITIONS-1); break;
    }
}
static void step_edit(tc_controls *c, uint32_t index) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    if(index==TC_NAV_UP || index==TC_NAV_DOWN)
        c->step_field=(uint8_t)((c->step_field+(index==TC_NAV_UP ? TC_FIELDS-1u : 1u))%TC_FIELDS);
    else if(index==TC_NAV_LEFT || index==TC_NAV_RIGHT) {
        for(uint32_t i=0;i<LN_STEPS;i++) {
            if(!((c->step_mask>>i)&1u)) continue;
            ln_step *s=&l->steps[i];
            if(!s->on) { *s=c->brush[at]; s->on=true; }
            tc_step_field(s,c->step_field,index==TC_NAV_RIGHT ? 1 : -1); ln_changed(at,i);
        }
        c->step_set=true; /* not toggled when let go */
    } else return; /* centre: nothing while steps are held */
    c->last_index=c->step_held; c->last_action=TC_STEP_FIELD;
}
void tc_steps_nav(tc_controls *c, uint32_t index) {
    ln_lane *l=&ln.lanes[c->lane%LN_LANES];
    uint32_t length=ln_length(l);
    if(c->step_mask) { step_edit(c,index); return; }
    switch(index) {
    case TC_NAV_LEFT:
        if(c->step_rec) { /* take the last one back */
            uint32_t at=(c->step_cursor%length+length-1u)%length;
            memset(&l->steps[at],0,sizeof l->steps[at]); ln_changed(c->lane%LN_LANES,at);
            c->step_cursor=(uint8_t)at; c->step_page=(uint8_t)(at/48u); c->last_action=TC_STEP_BACK;
        } else if(c->step_page) { c->step_page=0; c->last_action=TC_PREVIOUS; }
        else { c->ui=TC_UI_MENU; c->step_held=LN_NONE; c->step_mask=0u; c->step_rec=false; c->last_action=TC_CANCEL; }
        return;
    case TC_NAV_RIGHT:
        if(c->step_rec) { ln_step rest; memset(&rest,0,sizeof rest); step_fill(c,rest); c->last_action=TC_STEP_REST; }
        else { c->step_page=1; c->last_action=TC_NEXT; }
        return;
    case TC_NAV_UP: case TC_NAV_DOWN:
        c->lane=(uint8_t)((c->lane+(index==TC_NAV_UP ? LN_LANES-1u : 1u))%LN_LANES);
        c->step_held=LN_NONE; c->step_mask=0u; c->step_cursor=(uint8_t)(c->step_rec ? 0u : c->step_cursor);
        if(c->step_rec) c->step_page=0;
        c->last_action=TC_STEP_LANE; return;
    default: tc_step_record(c); return;
    }
}
void tc_step_pad(tc_controls *c, tp_layout layout, uint32_t k, bool pressed, uint8_t velocity) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    if(k>=TS_KEYS) return;
    if(k>=48u) { /* a step: on or off when lifted, unless it was given a value while held */
        uint32_t pad=k-48u, step=c->step_page*48u+(pad%4u)*12u+pad/4u; /* rows top down, 12 a row */
        uint64_t bit=step<LN_STEPS ? (uint64_t)1u<<step : 0u; /* page 2 reaches 95: past any lane */
        if(pressed) {
            if(step>=ln_length(l)) { c->last_action=TC_STEP_PAST; c->event_sequence++; return; }
            c->step_set=c->step_mask!=0u; /* a second step held: several steps to edit, none toggled */
            c->step_mask|=bit; c->step_held=(uint8_t)step; return;
        }
        if(!(c->step_mask&bit)) return;
        c->step_mask&=~bit;
        if(c->step_held==step) c->step_held=c->step_mask ? (uint8_t)__builtin_ctzll(c->step_mask) : (uint8_t)LN_NONE;
        if(c->step_set) return;
        if(l->steps[step].on) { memset(&l->steps[step],0,sizeof l->steps[step]); c->last_action=TC_STEP_OFF; }
        else { l->steps[step]=c->brush[at]; l->steps[step].on=true; c->last_action=TC_STEP_ON; }
        c->last_index=step; ln_changed(at,step); c->event_sequence++;
        return;
    }
    if(!pressed) return;
    if(l->kind==LN_CHORDS && tc_pad_empty(layout,k)) return; /* a chord set's empty column */
    ln_step s; memset(&s,0,sizeof s);
    s.on=true; s.value=ln_pad_value(l->kind,k); s.velocity=(uint8_t)(velocity&127u);
    if(l->kind==LN_CHORDS) s.edit=ln_edit_byte(tc_pad_edit(c,layout,k)); /* as progressions keep it */
    c->brush[at]=s;
    if(c->step_mask) { /* every held step plays it; one already on keeps its gate, chance, ratchet .. (1.7.0) */
        for(uint32_t i=0;i<LN_STEPS;i++) {
            if(!((c->step_mask>>i)&1u)) continue;
            ln_step *t=&l->steps[i];
            if(t->on) { t->value=s.value; t->velocity=s.velocity; t->edit=s.edit; t->scale=s.scale; }
            else *t=s;
            ln_changed(at,i);
        }
        c->step_set=true; c->last_index=c->step_held; c->last_action=TC_STEP_SET;
    } else if(c->step_rec) { c->last_index=c->step_cursor%ln_length(l); step_fill(c,s); c->last_action=TC_STEP_FILLED; }
    else c->last_action=TC_STEP_PICKED;
    c->event_sequence++;
}
