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
uint32_t tc_step_of_pad(const tc_controls *c, uint32_t pad) {
    uint32_t row=pad%4u, column=pad/4u%12u, step;
    if(c->step_grid==TC_GRID_ROWS) step=row*12u+column; /* rows top down, 12 a row */
    else step=column/4u*16u+(c->step_grid==TC_GRID_BOTTOM ? 3u-row : row)*4u+column%4u; /* 4 x 4 blocks */
    return c->step_page*48u+step;
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
    case TC_MENU_LANE_SWING: l->swing=(uint8_t)(v*TC_SWING_STEP<LN_SWING_MAX ? v*TC_SWING_STEP : LN_SWING_MAX); break; /* 2.0.3 */
    case TC_MENU_LANE_TRANSPOSE: l->transpose=(int8_t)((int32_t)(v<=2u*TC_TRANSPOSE_MENU ? v : TC_TRANSPOSE_MENU)-TC_TRANSPOSE_MENU); break;
    case TC_MENU_LANE_MUTATE: /* 2.0.3: changes the lane's steps a little, differently each time (lanes.c) */
        c->mutate=(uint8_t)(v<TC_MUTATES ? v : TC_MUTATE_DEFAULT);
        ln_mutate(l,at,ln.seed,10u*(c->mutate+1u),++c->mutations);
        for(uint32_t s=0;s<LN_STEPS;s+=LN_CHUNK) ln_changed(at,s);
        c->last_action=TC_LANE_MUTATED;
        break;
    case TC_MENU_LANE_SOLO: case TC_MENU_LANE_FOLLOW: /* 2.4.0: the pattern record */
        if(item==TC_MENU_LANE_SOLO) l->solo=(uint8_t)(v!=0u); else l->follow=(uint8_t)(v!=0u);
        ln_extra_changed(ln_current%LN_PATTERNS); return;
    case TC_MENU_LOOP_FROM: case TC_MENU_LOOP_TO: { /* 2.4.0: the loop window, first <= last kept */
        uint32_t first, n=ln_window(l,&first), last=first+n-1u, length=ln_length(l);
        if(v>=length) return;
        if(item==TC_MENU_LOOP_FROM) { first=v; if(last<first) last=length-1u; }
        else { last=v; if(first>last) first=v; }
        l->loop_from=(uint8_t)first; l->loop_to=(uint8_t)(last+1u==length ? 0u : last+1u);
        ln_extra_changed(ln_current%LN_PATTERNS); return;
    }
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
           item==TC_MENU_LANE_ROTATE || (item>=TC_MENU_LANE_SWING && item<=TC_MENU_LANE_MUTATE) ||
           (item>=TC_MENU_LANE_SOLO && item<=TC_MENU_LOOP_TO); /* 2.4.0 */
}
/* 2.4.0, the step screen's top row (controls.h). */
const uint8_t tc_mode_fields[7]={TC_FIELD_VELOCITY,TC_FIELD_GATE,TC_FIELD_PROBABILITY,TC_FIELD_RATCHET,TC_FIELD_CONDITION,
                                 TC_FIELD_NUDGE,TC_FIELD_TIE};
void tc_choose_pattern(tc_controls *c, uint32_t pattern) {
    if(pattern>=LN_PATTERNS) return;
    if(ln_song.on) { ln_song.on=0u; ln_song_changed(); }
    ln_queued=pattern==ln_current%LN_PATTERNS ? (uint8_t)LN_NO_PATTERN : (uint8_t)pattern;
    c->named=(uint8_t)pattern; c->last_action=TC_PATTERN_NEXT;
}
static void pattern_step(tc_controls *c, int32_t by) {
    uint32_t from=ln_queued<LN_PATTERNS ? ln_queued : ln_current%LN_PATTERNS;
    tc_choose_pattern(c,(from+LN_PATTERNS+(uint32_t)(by+(int32_t)LN_PATTERNS))%LN_PATTERNS);
}
void tc_steps_root(tc_controls *c, uint32_t index) {
    if(c->pattern_held) { /* 5 held: the patterns, in turn */
        if(index<LN_PATTERNS && c->song_picks<LN_SONG) c->song_pick[c->song_picks++]=(uint8_t)index;
        c->last_action=TC_PATTERN_PICK; return;
    }
    if(index<LN_LANES) {
        ln_lane *l=&ln.lanes[index];
        if(tc_shift(c)) { l->solo=(uint8_t)!l->solo; ln_extra_changed(ln_current%LN_PATTERNS); }
        else { l->mute=(uint8_t)!l->mute; ln_changed(index,LN_STEPS); }
        c->named=(uint8_t)index; c->last_action=TC_LANE_MUTE; return;
    }
    switch(index) {
    case TC_ROOT_FILL: c->root_fill=true; break; /* while held (tc_steps_release) */
    case TC_ROOT_LANES_ON:
        for(uint32_t i=0;i<LN_LANES;i++) {
            ln_lane *l=&ln.lanes[i];
            if(l->mute) { l->mute=0u; ln_changed(i,LN_STEPS); }
            if(l->solo) { l->solo=0u; ln_extra_changed(ln_current%LN_PATTERNS); }
        }
        c->last_action=TC_LANES_ON; break;
    default: pattern_step(c,index==TC_ROOT_PATTERN_UP ? 1 : -1); break;
    }
}
bool tc_steps_mode(tc_controls *c, uint32_t index) {
    if(index==TC_MODE_PATTERN) { c->pattern_held=true; c->song_picks=0u; c->last_action=TC_PATTERN_PICK; return true; }
    if(index==TC_MODE_LOOP) { /* the steps held, or the whole lane */
        ln_lane *l=&ln.lanes[c->lane%LN_LANES];
        uint32_t length=ln_length(l);
        if(c->step_mask) {
            uint32_t first=(uint32_t)__builtin_ctzll(c->step_mask), last=63u-(uint32_t)__builtin_clzll(c->step_mask);
            if(last>=length) last=length-1u;
            l->loop_from=(uint8_t)first; l->loop_to=(uint8_t)(last+1u==length ? 0u : last+1u);
            c->step_set=true; /* held steps aren't toggled when let go */
        } else { l->loop_from=0u; l->loop_to=0u; }
        ln_extra_changed(ln_current%LN_PATTERNS); c->last_action=TC_LOOP_SET; return true;
    }
    if(index>=TC_MODE_M1 && index<TC_MODE_M1+7u) {
        c->step_field=tc_mode_fields[index-TC_MODE_M1];
        if(c->step_mask) c->step_set=true;
        c->last_index=c->step_held; c->last_action=TC_STEP_FIELD; return true;
    }
    return false; /* U1..U3: their scales */
}
void tc_steps_release(tc_controls *c, uint32_t source, uint32_t index) {
    if(source==TC_ROOT && index==TC_ROOT_FILL) c->root_fill=false;
    if(source!=TC_SCALE || index!=TC_MODE_PATTERN || !c->pattern_held) return;
    c->pattern_held=false;
    if(c->song_picks==1u) tc_choose_pattern(c,c->song_pick[0]);
    else if(c->song_picks>1u) { /* a song: the patterns picked, in turn */
        ln_song.count=c->song_picks; memcpy(ln_song.links,c->song_pick,c->song_picks);
        ln_song.on=1u; ln_queued=LN_NO_PATTERN; ln_song_changed(); c->last_action=TC_SONG_SET;
    } else { c->last_action=TC_PATTERN_NEXT; c->named=(uint8_t)(ln_current%LN_PATTERNS); }
    c->event_sequence++;
}
void tc_steps_follow(tc_controls *c, tp_layout layout, uint32_t k) {
    const ln_lane *l=&ln.lanes[c->lane%LN_LANES];
    if(k>=48u) return;
    if(l->kind==LN_MELODY) { /* the note the pad shows */
        ln_step s; memset(&s,0,sizeof s); s.value=ln_pad_value(LN_MELODY,k);
        tp_notes n; uint8_t ch;
        if(!tc_step_notes(layout,LN_MELODY,l->channel,&s,&n,&ch) || !n.count) return;
        c->follow_pad=LN_NONE; c->follow_semis=(uint8_t)((n.notes[0]+120u-layout.tonic%12u)%12u);
        c->follow_rows=(uint8_t)(3u-k%4u); /* each row up an octave up, as the pads show (board test S8) */
    } else { /* a chord lane's chord pad; a drum lane's: the chord pad in its place */
        if(tc_pad_empty(layout,k)) return;
        c->follow_pad=(uint8_t)k; c->follow_semis=LN_NONE; c->follow_rows=0u;
    }
    c->last_action=TC_FOLLOW_SET; c->event_sequence++;
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
 * tie off/on, probability 10 % .. always, ratchet 1..8 strikes, condition in lanes.h's order, nudge (2.0.3)
 * -8/16 .. +7/16 of a step. */
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v<lo ? lo : v>hi ? hi : v; }
void tc_step_field(ln_step *s, uint8_t field, int32_t d) {
    switch(field) {
    case TC_FIELD_VELOCITY: s->velocity=(uint8_t)clamp((s->velocity ? s->velocity : (int32_t)LN_VELOCITY_DEFAULT)+5*d,1,127); break;
    case TC_FIELD_GATE: { int32_t g=clamp((int32_t)ln_gate_pct(s)/10+d,1,10); s->gate=(uint8_t)(g==(int32_t)LN_GATE_PCT/10 ? 0 : g); break; }
    case TC_FIELD_TIE: s->tie=d>0; break;
    case TC_FIELD_PROBABILITY: s->probability=(uint8_t)clamp((s->probability<LN_PROBABILITIES ? s->probability : 9)-d,0,(int32_t)LN_PROBABILITIES-1); break;
    case TC_FIELD_RATCHET: s->ratchet=(uint8_t)clamp((int32_t)(s->ratchet&7u)+d,0,(int32_t)LN_RATCHETS-1); break;
    case TC_FIELD_NUDGE: s->nudge=(uint8_t)(clamp(ln_nudge(s->nudge)+d,-8,7)&15); break;
    default: s->condition=(uint8_t)clamp((s->condition<LN_CONDITIONS ? s->condition : 0)+d,0,(int32_t)LN_CONDITIONS-1); break;
    }
}
static void step_edit(tc_controls *c, uint32_t index) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    if(index==TC_NAV_UP || index==TC_NAV_DOWN) {
        c->step_field=(uint8_t)((c->step_field+(index==TC_NAV_UP ? TC_FIELDS-1u : 1u))%TC_FIELDS);
        c->step_set=true; /* 2.0.5: a field looked at is not a tap either (board test U13) */
    } else if(index==TC_NAV_LEFT || index==TC_NAV_RIGHT) {
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
        c->lane=(uint8_t)((c->lane+(index==TC_NAV_UP ? 1u : LN_LANES-1u))%LN_LANES); /* 2.4.0: up the next lane (board test S2) */
        c->step_held=LN_NONE; c->step_mask=0u; c->step_cursor=(uint8_t)(c->step_rec ? 0u : c->step_cursor);
        if(c->step_rec) c->step_page=0;
        c->last_action=TC_STEP_LANE; return;
    default: /* centre (1.8.2): a tap plays or stops, a hold starts step record; recording, it ends it */
        if(c->step_rec) { tc_step_record(c); c->enter_used=true; }
        else c->enter_used=false;
        c->enter_down=true; c->enter_at=c->last_poll;
        return;
    }
}
void tc_steps_enter_up(tc_controls *c) {
    bool tap=c->enter_down && !c->enter_used && c->ui==TC_UI_STEPS;
    c->enter_down=false; c->enter_used=false;
    if(tap) tc_toggle_transport(c);
}
void tc_steps_hold(tc_controls *c) {
    if(c->ui!=TC_UI_STEPS) { c->enter_down=false; return; } /* left the screen: the press is spent */
    if(!c->enter_down || c->enter_used || c->last_poll-c->enter_at<TC_HOLD_REC_MS) return;
    c->enter_used=true; tc_step_record(c);
}
void tc_step_pad(tc_controls *c, tp_layout layout, uint32_t k, bool pressed, uint8_t velocity) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    if(k>=TS_KEYS) return;
    if(k>=48u) { /* a step: on or off when lifted, unless it was given a value, a field was chosen or it was held
                  * TC_HOLD_REC_MS (2.0.5) */
        uint32_t step=tc_step_of_pad(c,k-48u); /* 2.2.0: as Step grid lays them out */
        uint64_t bit=step<LN_STEPS ? (uint64_t)1u<<step : 0u; /* page 2 reaches 95: past any lane */
        if(pressed) {
            if(step>=ln_length(l)) { c->last_action=TC_STEP_PAST; c->event_sequence++; return; }
            c->step_set=c->step_mask!=0u; /* a second step held: several steps to edit, none toggled */
            if(!c->step_mask) c->step_at=c->last_poll;
            c->step_mask|=bit; c->step_held=(uint8_t)step; return;
        }
        if(!(c->step_mask&bit)) return;
        c->step_mask&=~bit;
        if(c->last_poll-c->step_at>=TC_HOLD_REC_MS) c->step_set=true; /* 2.0.5: a long press is not a tap */
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
/* 2.0.0, Shift + the buttons under the step screen: left/right move the lane's steps one earlier/later (round
 * within its length), up copies the lane, down pastes the copy onto the lane shown (steps and settings, as the
 * editor's Paste), centre clears its steps when pressed again within TC_CLEAR_ASK_MS. */
static ln_lane copied;
static bool have_copy;
static void lane_changed(uint32_t lane) {
    for(uint32_t s=0;s<LN_STEPS;s+=LN_CHUNK) ln_changed(lane,s);
    ln_changed(lane,LN_STEPS);
}
void tc_steps_shift(tc_controls *c, uint32_t index, uint32_t now) {
    uint32_t at=c->lane%LN_LANES;
    ln_lane *l=&ln.lanes[at];
    if(index!=TC_NAV_ENTER) c->clear_asked=0u;
    if(index==TC_NAV_LEFT || index==TC_NAV_RIGHT) {
        ln_rotate(l,index==TC_NAV_RIGHT ? 1 : -1); lane_changed(at); c->last_action=TC_LANE_SHIFTED;
    } else if(index==TC_NAV_UP) { copied=*l; have_copy=true; c->last_action=TC_LANE_COPIED; }
    else if(index==TC_NAV_DOWN) {
        if(!have_copy) { c->last_action=TC_LANE_NO_COPY; return; }
        *l=copied; lane_changed(at); c->last_action=TC_LANE_PASTED;
    } else if(c->clear_asked && now-c->clear_asked<=TC_CLEAR_ASK_MS) {
        ln_clear_steps(l); lane_changed(at); c->clear_asked=0u; c->last_action=TC_LANE_CLEARED;
    } else { c->clear_asked=now ? now : 1u; c->last_action=TC_LANE_CLEAR_ASK; }
    c->step_mask=0u; c->step_held=LN_NONE; c->step_rec=false;
}
