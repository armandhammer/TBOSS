/* SPDX-License-Identifier: GPL-3.0-or-later - the controls (controls.h): the buttons that play. The play screen's
 * arrows, the pedal, the chord panel, the centre buttons' actions and Shift. */
#include "controls_internal.h"
#include "sequence.h"
#include "midi_in.h"
void tc_program_in(tc_controls *c) {
    if(tm.programs==c->program_seen) return;
    c->program_seen=tm.programs;
    if(!c->program_in || (c->program_in<=16u && c->program_in!=tm.program_ch) || tm.program>=TC_PRESETS) return;
    tc_request(c,TC_REQUEST_LOAD,tm.program); /* settings.c: PRESET n LOADED, NO PRESET THERE, or busy recording */
}
/* Next existing preset after (or before) the current one; false if there are none. */
static bool step_preset(const tc_controls *c, int32_t direction, uint8_t *slot) {
    int32_t at=c->preset<TC_PRESETS ? c->preset : (direction>0 ? -1 : (int32_t)TC_PRESETS);
    for(uint32_t n=0;n<TC_PRESETS;n++) {
        at=(at+direction+(int32_t)TC_PRESETS)%(int32_t)TC_PRESETS;
        if(tc_presets_present&(1u<<at)) { *slot=(uint8_t)at; return true; }
    }
    return false;
}
/* Next defined scale after (or before) the current one: through the favourites when there
 * are any (from the first or last if the current scale is not one), else the Scale menu's
 * order. */
static uint8_t step_scale(uint8_t scale, int32_t direction) {
    if(tc_favourite_count) {
        int32_t n=tc_favourite_count, at=direction>0 ? -1 : n;
        for(int32_t i=0;i<n;i++) if(tc_favourites[i]==scale) { at=i; break; }
        for(int32_t i=0;i<n;i++) {
            at=(at+direction+n)%n;
            if(tc_scale_defined(tc_favourites[at])) return tc_favourites[at];
        }
    }
    int32_t at=tc_scale_position(scale);
    for(uint32_t n=0;n<TC_ALL_SCALES;n++) {
        at=(at+direction+(int32_t)TC_ALL_SCALES)%(int32_t)TC_ALL_SCALES;
        if(tc_scale_defined(tc_scale_at((uint8_t)at))) return tc_scale_at((uint8_t)at);
    }
    return scale;
}
void tc_shortcut(tc_controls *c, tp_engine *e, uint32_t index) {
    /* Play screen: right = hold, up/down = what Settings > Play > Arrows says, hold
     * left + up/down = previous/next preset; left alone does nothing, so pressing it
     * once too often on the way home is harmless. */
    if(index==TC_NAV_RIGHT) { c->hold=!c->hold; c->changes++; c->last_action=TC_HOLD; return; }
    if(index==TC_NAV_LEFT && c->capturing) { /* capturing: take back the last chord */
        tc_capture_undo();
        c->last_action=TC_CAPTURE_UNDO; return;
    }
    if(index==TC_NAV_LEFT) { /* recording: take back the last unit */
        if(c->recording) { sq_undo(&sq); c->last_action=TC_SEQ_UNDO; }
        return;
    }
    int32_t up=index==TC_NAV_UP ? 1 : -1;
    bool left=(c->nav_held&(1u<<TC_NAV_LEFT))!=0u; /* hold left: presets, whatever the arrows do */
    if(c->arrows==TC_ARROWS_SEQUENCER && (!left || c->recording)) { /* up: play/stop, down: record/finish */
        if(up>0) tc_toggle_transport(c);
        else if(c->recording) tc_finish_recording(c);
        else tc_start_recording(c);
        return;
    }
    if(c->recording) return;
    if(left || c->arrows==TC_ARROWS_PRESET) {
        uint8_t slot;
        if(step_preset(c,-up,&slot)) { tc_request(c,TC_REQUEST_LOAD,slot); c->last_action=TC_COMMIT; }
        else c->last_action=TC_PRESET_EMPTY;
        return;
    }
    tp_layout layout=tc_layout(c);
    switch(c->arrows) {
    case TC_ARROWS_OCTAVE:
        layout.melody_base=(uint8_t)((int32_t)layout.melody_base+12*up);
        if(tc_set_layout(c,e,layout)) c->last_action=TC_OCTAVE;
        break;
    case TC_ARROWS_TEMPO:
        if(up>0 ? c->tempo+1u<TC_TEMPOS : c->tempo>0u) { c->tempo=(uint8_t)(c->tempo+up); c->changes++; }
        c->last_action=TC_TEMPO_STEP;
        break;
    default:
        layout.mode=step_scale(c->scale,up);
        if(tc_set_layout(c,e,layout)) c->last_action=tc_favourite_count ? TC_FAVOURITE_STEP : TC_SCALE_STEP;
        break;
    }
}
void tc_pedal(tc_controls *c, bool level, uint32_t now) {
    if(!c->pedal_known) { c->pedal_known=true; c->pedal_rest=level; c->pedal_level=level; c->pedal_at=now; }
    if(level!=c->pedal_level) { c->pedal_level=level; c->pedal_at=now; }
    if(now-c->pedal_at<TC_PEDAL_DEBOUNCE_MS) return;
    bool rest=c->pedal_type==TC_PEDAL_NORMAL ? true : c->pedal_type==TC_PEDAL_INVERTED ? false : c->pedal_rest;
    bool pressed=c->pedal_level!=rest, was=c->pedal_pressed;
    c->pedal_pressed=pressed;
    c->sustain=tc_pedal_cc(c->pedal_mode)!=0u && pressed;
    c->pedal_down_note=c->pedal_mode==TC_PEDAL_NOTE && pressed;
    if(pressed==was) return;
    /* Hold while the pedal is down; not saved as a setting change (no flash wear). */
    if(c->pedal_mode==TC_PEDAL_HOLD) { c->hold=pressed; c->last_action=TC_HOLD; c->event_sequence=tc_inc(c->event_sequence); }
    if(c->pedal_mode==TC_PEDAL_TRANSPORT && pressed) {
        tc_toggle_transport(c); c->event_sequence=tc_inc(c->event_sequence);
    }
}
/* The chord control panel. Voicing and Spread: one at a time, pressing the lit one again
 * turns it off. Octave (0.6.2, as the original firmware): each tap turns that octave on or
 * off, so lit octaves stack; turning off the last one goes back to 0. */
static void panel(tc_controls *c, uint32_t index) {
    c->last_function=(uint8_t)index;
    if(index<=TC_FN_GUITAR) {
        uint8_t v=(uint8_t)(index+1u);
        c->chord_voicing=c->chord_voicing==v ? 0u : v;
    } else if(index<=TC_FN_SPREAD3) {
        uint8_t s=(uint8_t)(index-TC_FN_SPREAD1+1u);
        c->chord_spread=c->chord_spread==s ? 0u : s;
    } else if(index<TC_FUNCTIONS) {
        c->chord_octaves^=(uint8_t)(1u<<(index-TC_FN_OCT_M3)); /* bit 0 = -3 ... bit 6 = +3 */
        if(!c->chord_octaves) c->chord_octaves=TC_OCTAVES_DEFAULT;
    } else return;
    c->changes++; c->last_action=TC_PANEL;
    if(c->live_update) c->relayout=true;
}
static bool panel_on(const tc_controls *c, uint32_t index) {
    if(index<=TC_FN_GUITAR) return c->chord_voicing==index+1u;
    if(index<=TC_FN_SPREAD3) return c->chord_spread==index-TC_FN_SPREAD1+1u;
    return index<TC_FUNCTIONS && ((c->chord_octaves>>(index-TC_FN_OCT_M3))&1u);
}
bool tc_fill(const tc_controls *c) {
    return tc_button_held(c,TC_ACT_FILL) || (c->pedal_mode==TC_PEDAL_FILL && c->pedal_pressed);
}
uint32_t tc_stutter_ticks(const tc_controls *c) {
    for(uint8_t a=TC_ACT_STUTTER4;a<=TC_ACT_STUTTER16;a++) if(tc_button_held(c,a)) return tc_stutter_ticks_of[a-TC_ACT_STUTTER4];
    return 0u;
}
bool tc_button_held(const tc_controls *c, uint8_t action) {
    for(uint32_t i=0;i<TC_FUNCTIONS;i++) if(c->buttons[i]==action && (((uint32_t)c->fn_held>>i)&1u)) return true;
    return false;
}
bool tc_shift(const tc_controls *c) {
    return c->shift_latched || tc_button_held(c,TC_ACT_SHIFT) || c->shift_once || (c->u3_shift && c->u3_held); /* 2.0.0: U3 */
}
bool tc_sustain_button(const tc_controls *c) {
    if(!c->button_sustain) return false;
    for(uint32_t i=0;i<TC_FUNCTIONS;i++) if(c->buttons[i]==TC_ACT_SUSTAIN) return true;
    return false;
}
bool tc_panel_active(const tc_controls *c, uint32_t index) {
    if(index>=TC_FUNCTIONS) return false;
    uint8_t act=c->buttons[index];
    switch(act) {
    case TC_ACT_SHIFT: return tc_shift(c);
    case TC_ACT_HOLD: return c->hold!=0u;
    case TC_ACT_ARP: return c->arp!=TC_ARP_OFF;
    case TC_ACT_STRUM: return c->strum!=TC_STRUM_OFF;
    case TC_ACT_VOICE_LEAD: return c->voice_lead!=0u;
    case TC_ACT_FOLD: return c->fold!=0u;
    case TC_ACT_HINTS: return c->hints!=0u;
    case TC_ACT_SUSTAIN: return tc_sustain_button(c);
    case TC_ACT_CHORD_SET: return c->chord_set!=0u;
    case TC_ACT_PLAY: return c->transport;
    case TC_ACT_RECORD: return c->recording;
    case TC_ACT_PIVOT: return c->pivot!=0u;
    case TC_ACT_NONE: return false;
    default: return act<TC_FUNCTIONS ? panel_on(c,act) : (((uint32_t)c->fn_held>>index)&1u)!=0u; /* the rest: while held */
    }
}
static void toggled(tc_controls *c, uint8_t action) {
    c->changes++; c->last_button=action; c->last_action=TC_BUTTON;
}
/* After the scale's chords the chord grid, the Full layout, then each chord set that has
 * chords, then the scale's chords again. */
static uint8_t next_chord_set(uint8_t at) {
    if(!at) return TC_CHORDS_GRID;
    if(at==TC_CHORDS_GRID) return TC_CHORDS_FULL;
    for(uint8_t n=(uint8_t)(at==TC_CHORDS_FULL ? 1u : at+1u);n<=TC_CHORD_SETS;n++) if(tc_chord_set_defined((uint8_t)(n-1u))) return n;
    return 0u;
}
/* A centre button's action (1.0.0). */
void tc_button(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now) {
    uint8_t act=c->buttons[index];
    if(act==TC_ACT_SHIFT) {
        /* Two taps close together latch it; a tap while latched lets go. */
        if(c->shift_latched) { c->shift_latched=false; c->shift_tapped=0u; }
        else if(c->shift_tapped && now-c->shift_tapped<=TC_SHIFT_LATCH_MS) { c->shift_latched=true; c->shift_tapped=0u; }
        else c->shift_tapped=now;
        c->last_button=act; c->last_action=TC_BUTTON; return;
    }
    if(tc_shift(c)) act=(uint8_t)index; /* Shift + a button: its printed panel function */
    tc_action(c,e,act,now);
}
void tc_action(tc_controls *c, tp_engine *e, uint8_t act, uint32_t now) {
    tp_layout layout=tc_layout(c);
    switch(act) {
    case TC_ACT_HOLD: c->hold=!c->hold; c->changes++; c->last_action=TC_HOLD; break;
    case TC_ACT_ARP:
        c->arp=c->arp==TC_ARP_OFF ? (c->arp_last!=TC_ARP_OFF ? c->arp_last : (uint8_t)TC_ARP_UP) : (uint8_t)TC_ARP_OFF;
        c->changes++; c->last_action=TC_ARP; break;
    case TC_ACT_STRUM: c->strum=c->strum ? (uint8_t)TC_STRUM_OFF : (uint8_t)TC_STRUM_UP; toggled(c,act); break;
    case TC_ACT_VOICE_LEAD: c->voice_lead=!c->voice_lead; toggled(c,act); break;
    case TC_ACT_HINTS: c->hints=!c->hints; toggled(c,act); break;
    case TC_ACT_SUSTAIN: /* not held (see controls.h), and not a setting: no flash write */
        c->button_sustain=!c->button_sustain; c->last_button=act; c->last_action=TC_BUTTON; break;
    case TC_ACT_FOLD:
        layout.fold=!layout.fold;
        if(tc_set_layout(c,e,layout)) { c->last_button=act; c->last_action=TC_BUTTON; }
        break;
    case TC_ACT_CHORD_SET:
        layout.chords=next_chord_set(c->chord_set);
        if(tc_set_layout(c,e,layout)) { c->last_button=act; c->last_action=TC_BUTTON; }
        break;
    case TC_ACT_PANIC: c->panic=true; c->transport=false; c->button_sustain=false; c->last_button=act; c->last_action=TC_BUTTON; break;
    case TC_ACT_TAP: {
        /* The time between two taps sets the tempo (40..240 BPM, steps of 5). */
        uint32_t gap=now-c->tapped;
        if(c->tapped && gap>=250u && gap<=1500u) {
            uint32_t bpm=(60000u+gap/2u)/gap, step=(bpm-40u+2u)/5u;
            c->tempo=(uint8_t)(step<TC_TEMPOS ? step : TC_TEMPOS-1u); c->changes++;
        }
        c->tapped=now; c->last_action=TC_TEMPO_STEP; break;
    }
    case TC_ACT_PRESET_NEXT: case TC_ACT_PRESET_PREV: {
        uint8_t slot;
        if(c->recording) break;
        if(step_preset(c,act==TC_ACT_PRESET_NEXT ? 1 : -1,&slot)) { tc_request(c,TC_REQUEST_LOAD,slot); c->last_action=TC_COMMIT; }
        else c->last_action=TC_PRESET_EMPTY;
        break;
    }
    case TC_ACT_SCALE_NEXT: case TC_ACT_SCALE_PREV:
        layout.mode=step_scale(c->scale,act==TC_ACT_SCALE_NEXT ? 1 : -1);
        if(tc_set_layout(c,e,layout)) c->last_action=tc_favourite_count ? TC_FAVOURITE_STEP : TC_SCALE_STEP;
        break;
    case TC_ACT_OCT_UP: case TC_ACT_OCT_DOWN:
        layout.melody_base=(uint8_t)((int32_t)layout.melody_base+(act==TC_ACT_OCT_UP ? 12 : -12));
        if(tc_set_layout(c,e,layout)) c->last_action=TC_OCTAVE;
        break;
    case TC_ACT_PLAY: tc_toggle_transport(c); break;
    case TC_ACT_RECORD: /* on the step screen: step record (1.5.0) */
        if(c->ui==TC_UI_STEPS) tc_step_record(c); else if(c->recording) tc_finish_recording(c); else tc_start_recording(c);
        break;
    case TC_ACT_FILL: case TC_ACT_STUTTER4: case TC_ACT_STUTTER8: case TC_ACT_STUTTER16:
        /* 2.0.3, while held (tc_fill, tc_stutter_ticks): the engine looks */
        c->last_button=act; c->last_action=TC_BUTTON; break;
    case TC_ACT_ADD7: case TC_ACT_ADD9: case TC_ACT_ADD6: case TC_ACT_SUS2: case TC_ACT_SUS4:
        /* while held (tc_button_held): tc_pad_edit looks */
        c->last_button=act; c->last_action=TC_BUTTON;
        if(c->live_update) c->relayout=true;
        break;
    case TC_ACT_PIVOT: /* a tap turns it off; held, a root button picks the key (dispatch) */
        if(c->pivot) { c->pivot=0u; c->changes++; }
        c->last_button=act; c->last_action=TC_BUTTON; break;
    case TC_ACT_NONE: break;
    default: panel(c,act); break; /* works on every screen: hear the change while browsing settings */
    }
}
/* 2.0.0, the U3 button as Shift: a tap is Shift for the next press (the next button or chord pad), two taps
 * close together latch it, a tap while it is on lets go; held, Shift while held (the touch chip lets go of a
 * button after about 5 s, and the other mode buttons don't answer while it is held: Shift + mode button
 * needs the tap). */
void tc_u3_press(tc_controls *c, uint32_t now) {
    c->u3_held=true;
    if(c->shift_once && c->u3_tapped && now-c->u3_tapped<=TC_SHIFT_LATCH_MS) { c->shift_latched=true; c->shift_once=false; }
    else if(c->shift_once || c->shift_latched) { c->shift_once=false; c->shift_latched=false; }
    else { c->shift_once=true; c->u3_tapped=now; }
    c->last_button=TC_ACT_SHIFT; c->last_action=c->shift_once ? TC_SHIFT_ONCE : TC_BUTTON;
}
/* 2.0.0, Shift + the buttons under the play screen: left plays or stops, up/down the tempo 5 BPM faster or
 * slower (with Arrows on Tempo: the next or previous scale), right the arpeggiator on or off, centre taps the
 * tempo. */
void tc_play_shift(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now) {
    if(index==TC_NAV_LEFT) tc_action(c,e,TC_ACT_PLAY,now);
    else if(index==TC_NAV_RIGHT) tc_action(c,e,TC_ACT_ARP,now);
    else if(index==TC_NAV_ENTER) tc_action(c,e,TC_ACT_TAP,now);
    else if(c->arrows==TC_ARROWS_TEMPO) tc_action(c,e,index==TC_NAV_UP ? TC_ACT_SCALE_NEXT : TC_ACT_SCALE_PREV,now);
    else {
        if(index==TC_NAV_UP && c->tempo+1u<TC_TEMPOS) c->tempo++;
        else if(index==TC_NAV_DOWN && c->tempo) c->tempo--;
        c->changes++; c->last_action=TC_TEMPO_STEP;
    }
}
/* 2.0.0, Shut down (Settings > About): every note off, the transport stopped, the screen and lights dark and
 * the pads, buttons and arrows ignored until centre wakes the board (USB stays connected). Sleep after: only
 * the screen and lights dark, until any touch. */
void tc_shut_down(tc_controls *c) {
    c->panic=true; c->transport=false; c->button_sustain=false; c->recording=false; c->capturing=false;
    c->shift_once=false; c->shift_latched=false;
    c->dark=TC_SHUT_DOWN; c->ui=TC_UI_PLAY; c->event_sequence++;
}
void tc_wake(tc_controls *c) { c->dark=TC_AWAKE; c->last_action=TC_AWOKE; c->event_sequence++; }
/* Shift + root button 1..8: play that progression (again: stop), Chain off (2.0.0); 9: Chain on or off. */
void tc_shift_root(tc_controls *c, uint32_t index) {
    if(index==SQ_SLOTS) { c->chain=!c->chain; c->changes++; c->last_action=TC_CHAIN; return; }
    if(index>=SQ_SLOTS) { c->last_action=TC_SHIFT_UNUSED; return; }
    if(c->recording) { tc_finish_recording(c); return; }
    if(c->chain) { c->chain=0u; c->changes++; if(c->transport) c->transport=false; }
    if(c->prog_slot==index && c->transport) { c->transport=false; c->last_action=TC_TRANSPORT; return; }
    if(!(((uint32_t)sq_present>>index)&1u)) { c->prog_slot=(uint8_t)index; c->changes++; tc_request(c,TC_REQUEST_PROG_LOAD,(uint8_t)index);
        c->transport=false; c->last_action=TC_SEQ_EMPTY; return; }
    if(c->prog_slot!=index) { c->prog_slot=(uint8_t)index; c->changes++; tc_request(c,TC_REQUEST_PROG_LOAD,(uint8_t)index); }
    c->transport=true; c->last_action=TC_TRANSPORT;
}
