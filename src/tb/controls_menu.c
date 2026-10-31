/* SPDX-License-Identifier: GPL-3.0-or-later - the controls (controls.h): Settings. Its pages and items, each item's
 * values and stored setting, choosing and changing them, the per-chord edits, the resets and their
 * confirmation, and the navigation buttons on every screen but the step screen. */
#include "controls_internal.h"
#include "sequence.h"
#include <stddef.h>
#define END TC_MENU_ITEMS
#define B(n) (TC_MENU_BUTTON1+(n))
#define R TC_MENU_RESET_PAGE
const uint8_t tc_group_items[TC_GROUPS][TC_GROUP_MAX]={ /* 2.0.1: rebalanced, each page one kind of thing; 2.0.4: in the
                                                         * enum's order (controls.h) */
    [TC_GROUP_PLAY]={TC_MENU_SCALE,TC_MENU_FAMILY,TC_MENU_HOLD,TC_MENU_VELOCITY,TC_MENU_MELODY_VEL,TC_MENU_TEMPO,
     TC_MENU_ARROWS,TC_MENU_LIVE,R,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_LAYOUT]={TC_MENU_LAYOUT,TC_MENU_CHORDS_SIDE,TC_MENU_MELODY_OCT,R,END,END,END,END,END,END,END,END,END,END,
     END,END,END,END},
    [TC_GROUP_CHORDS]={TC_MENU_CHORD_SET,TC_MENU_CAPTURE,TC_MENU_VOICING,TC_MENU_SPREAD,TC_MENU_SPREAD_OF,
     TC_MENU_VOICE_LEAD,TC_MENU_FOLD,TC_MENU_HINTS,TC_MENU_PIVOT,R,END,END,END,END,END,END,END,END},
    [TC_GROUP_CHORD_EDITS]={TC_MENU_ADD7,TC_MENU_ADD9,TC_MENU_ADD6,TC_MENU_SUS,TC_MENU_PAD_STRUM,TC_MENU_SOUND,
     TC_MENU_RESET_CHORD,TC_MENU_RESET_ALL,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_STRUM]={TC_MENU_STRUM,TC_MENU_STRUM_SPEED,TC_MENU_REPEAT,TC_MENU_CUTOFF,TC_MENU_FEEL_TIME,
     TC_MENU_FEEL_VEL,TC_MENU_CAPO,R,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_ARP]={TC_MENU_ARP,TC_MENU_ARP_LOCK,TC_MENU_ARP_LOOP,TC_MENU_ARP_RATE,TC_MENU_ARP_OCTAVES,
     TC_MENU_ARP_GATE,TC_MENU_RHYTHM,TC_MENU_ARP_ROTATE,TC_MENU_CHANCE,TC_MENU_SWING,TC_MENU_ARP_SYNC,
     TC_MENU_ARP_LATCH,R,END,END,END,END,END},
    [TC_GROUP_EFFECTS]={TC_MENU_NOTE_REPEAT,TC_MENU_REPEAT_RHYTHM,TC_MENU_HARMONY,TC_MENU_ECHO,TC_MENU_ECHOES,
     TC_MENU_MONO,R,END,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_EXPRESSION]={TC_MENU_CHORD_AT,TC_MENU_MELODY_AT,TC_MENU_MPE,TC_MENU_GLIDE,TC_MENU_GLIDE_TIME,R,END,END,
     END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_PROGRESSIONS]={TC_MENU_SEQ_PLAY,TC_MENU_CHAIN,TC_MENU_SEQ_SLOT,TC_MENU_SEQ_UNIT,TC_MENU_SEQ_RECORD,
     TC_MENU_SEQ_CLEAR,TC_MENU_SEQ_GUIDE,R,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_STEPS]={TC_MENU_STEPS,TC_MENU_LANE,TC_MENU_LANE_KIND,TC_MENU_LANE_LENGTH,TC_MENU_LANE_RATE,
     TC_MENU_LANE_DIRECTION,TC_MENU_LANE_CHANNEL,TC_MENU_LANE_MUTE,TC_MENU_LANE_SWING,TC_MENU_LANE_TRANSPOSE,
     TC_MENU_LANE_EUCLID,TC_MENU_LANE_ROTATE,TC_MENU_LANE_MUTATE,TC_MENU_STEP_PADS,END,END,END,END},
    [TC_GROUP_PRESETS]={TC_MENU_LOAD,TC_MENU_SAVE,TC_MENU_CLEAR_PRESET,TC_MENU_KEEP_LOOK,TC_MENU_KEEP_FEEL,TC_MENU_KEEP_CHORDS,END,END,
     END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_MIDI]={TC_MENU_CHORD_CH,TC_MENU_MELODY_CH,TC_MENU_BASS_CH,TC_MENU_DRUM_CH,TC_MENU_LIVE_CH,
     TC_MENU_LIVE_IN,TC_MENU_CHORD_PROG,TC_MENU_MELODY_PROG,TC_MENU_PROGRAM_IN,TC_MENU_TRS_OUT,TC_MENU_CLOCK_OUT,
     TC_MENU_TRANSPORT,TC_MENU_LOCK,R,END,END,END,END},
    [TC_GROUP_PEDAL]={TC_MENU_PEDAL,TC_MENU_PEDAL_TYPE,TC_MENU_PEDAL_NOTE,TC_MENU_PEDAL_TARGET,R,END,END,END,END,END,
     END,END,END,END,END,END,END,END},
    [TC_GROUP_BUTTONS]={B(0),B(1),B(2),B(3),B(4),B(5),B(6),B(7),B(8),B(9),B(10),B(11),B(12),B(13),TC_MENU_U3,
     TC_MENU_BUTTONS_RESET,END,END},
    [TC_GROUP_LIGHTS]={TC_MENU_BRIGHTNESS,TC_MENU_COLORS,TC_MENU_COLOR_BY,TC_MENU_MIDI_LEDS,TC_MENU_SLEEP,
     TC_MENU_LED_TEST,R,END,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_PADS]={TC_MENU_PAD_TRIGGER,TC_MENU_PAD_RELEASE,TC_MENU_PAD_FULL,TC_MENU_PAD_VEL_FULL,TC_MENU_PAD_UNSTICK,
     TC_MENU_PAD_TEST,TC_MENU_PAD_RESCAN,R,END,END,END,END,END,END,END,END,END,END},
    [TC_GROUP_ABOUT]={TC_MENU_VERSION,TC_MENU_IO_TEST,TC_MENU_SHUT_DOWN,TC_MENU_FACTORY,END,END,END,END,END,END,END,
     END,END,END,END,END,END,END}};
#undef R
#undef B
#undef END
uint8_t tc_group_size(uint8_t g) {
    uint8_t n=0;
    while(g<TC_GROUPS && n<TC_GROUP_MAX && tc_group_items[g][n]<TC_MENU_ITEMS) n++;
    return n;
}
uint8_t tc_page_items(const tc_controls *c, uint8_t group, uint8_t items[TC_GROUP_MAX]) {
    uint8_t n=0;
    for(uint8_t i=0;i<tc_group_size(group);i++) {
        uint8_t item=tc_group_items[group][i];
        if(item==TC_MENU_PEDAL_NOTE && c->pedal_mode!=TC_PEDAL_NOTE) continue;
        if(item==TC_MENU_GLIDE_TIME && c->glide!=TC_GLIDE_TIME) continue; /* 1.8.5 */
        if((item==TC_MENU_ARP_LOCK || item==TC_MENU_ARP_LOOP) && c->arp!=TC_ARP_TURING) continue; /* 2.0.0 */
        items[n++]=item;
    }
    return n;
}
uint8_t tc_menu_values(uint8_t item) {
    static const uint8_t n[TC_MENU_BUTTON1]={TC_ALL_SCALES,2u,TC_STRUM_MODES,TC_STRUM_SPEEDS,TC_ARP_MODES,
        TC_ARP_RATES,TC_ARP_OCTAVES,TC_ARP_GATES,TC_TEMPOS,16u,16u,TC_AT_MODES,TC_VEL_MODES,
        TC_PRESETS,TC_PRESETS,2u,TC_BRIGHTNESS_LEVELS,1u,1u,2u,TC_PEDAL_MODES,TC_PEDAL_TYPES,1u,TC_LED_SCHEMES,
        TC_AT_MODES,
        TC_VOICINGS,TC_SPREADS,2u,2u,2u,3u,2u,2u,2u,TP_MELODY_LAYOUTS,16u,16u,TC_REPEATS,TC_CUTOFFS,
        TC_RHYTHM_VALUES,TC_SWINGS,2u,2u,2u,2u,2u,128u,TC_ARROWS_MODES,2u*TP_OCTAVE_RANGE/12u+1u,
        TC_FAMILIES,TC_CAPOS,TC_FEEL_TIMES,TC_FEEL_VELS,2u,TC_LIVE_CHANNELS,TC_LIVE_INPUTS,2u,2u,TC_NOTE_REPEATS,
        1u,TC_PAD_TRIGGER_MAX-TC_PAD_TRIGGER_MIN+1u,TC_PAD_RELEASE_MAX-TC_PAD_RELEASE_MIN+1u,
        TC_PAD_FULL_MAX-TC_PAD_FULL_MIN+1u,
        TC_VEL_MODES,TC_PEDAL_TARGETS,SQ_SLOTS,SQ_UNIT_KINDS,1u,1u,2u,
        TC_CHORDS_GRID+1u,2u,17u,
        TC_ROTATES,RH_EUCLIDS+1u,TC_PIVOTS,TC_CHORD_SETS,
        2u,2u,
        1u,LN_LANES,LN_KINDS,LN_STEPS,LN_RATES,LN_CHANNELS,2u,
        2u,
        TC_HARMONIES,TC_ECHO_TIMES,TC_ECHOES,2u,TC_CHANCES,
        LN_DIRECTIONS,LN_STEPS+1u,LN_STEPS,1u,
        2u,
        TC_PROGRAM_INPUTS,TC_PROGRAMS,TC_PROGRAMS,TC_SPREAD_OFS,
        TC_MPE_MODES,TC_GLIDE_MODES,2u,
        TC_PAD_VEL_FULLS,
        TC_GLIDE_TIMES,
        TC_COLOR_BYS,2u,
        2u,
        TC_ARP_LOCKS,TC_ARP_LOOPS,2u,2u,2u,1u,TC_SLEEPS,
        2u,
        LN_SWING_MAX/TC_SWING_STEP+1u,2u*TC_TRANSPOSE_MENU+1u,TC_MUTATES,
        TC_PRESETS};
    if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) return TC_ACTIONS;
    if(item==TC_MENU_BUTTONS_RESET) return 2u;
    /* 1.7.0: Euclid 0 (off) .. the lane's length hits, Rotate 0 .. length - 1 (the lane the page sets) */
    if(item==TC_MENU_LANE_EUCLID || item==TC_MENU_LANE_ROTATE)
        return (uint8_t)(ln_length(&ln.lanes[tc_state.lane%LN_LANES])+(item==TC_MENU_LANE_EUCLID ? 1u : 0u));
    return item<TC_MENU_BUTTON1 ? n[item] : 1u;
}
/* The stored byte behind a plain menu item; NULL for the rest (scale, layout, melody octave, presets,
 * per-chord edits, actions, version). */
static uint8_t *field(tc_controls *c, uint8_t item) {
    switch(item) {
    case TC_MENU_HOLD: return &c->hold;
    case TC_MENU_STRUM: return &c->strum;
    case TC_MENU_STRUM_SPEED: return &c->strum_speed;
    case TC_MENU_ARP: return &c->arp;
    case TC_MENU_ARP_RATE: return &c->arp_rate;
    case TC_MENU_ARP_OCTAVES: return &c->arp_octaves;
    case TC_MENU_ARP_GATE: return &c->arp_gate;
    case TC_MENU_TEMPO: return &c->tempo;
    case TC_MENU_CHORD_CH: return &c->chord_ch;
    case TC_MENU_MELODY_CH: return &c->melody_ch;
    case TC_MENU_CHORD_AT: return &c->at_chord;
    case TC_MENU_MELODY_AT: return &c->at_melody;
    case TC_MENU_VELOCITY: return &c->vel_mode;
    case TC_MENU_MIDI_LEDS: return &c->midi_leds;
    case TC_MENU_BRIGHTNESS: return &c->brightness;
    case TC_MENU_TRS_OUT: return &c->trs_out;
    case TC_MENU_PEDAL: return &c->pedal_mode;
    case TC_MENU_PEDAL_TYPE: return &c->pedal_type;
    case TC_MENU_COLORS: return &c->led_scheme;
    case TC_MENU_VOICING: return &c->chord_voicing;
    case TC_MENU_SPREAD: return &c->chord_spread;
    case TC_MENU_BASS_CH: return &c->bass_ch;
    case TC_MENU_DRUM_CH: return &c->drum_ch;
    case TC_MENU_REPEAT: return &c->strum_repeat;
    case TC_MENU_CUTOFF: return &c->strum_cutoff;
    case TC_MENU_RHYTHM: return &c->arp_rhythm;
    case TC_MENU_SWING: return &c->arp_swing;
    case TC_MENU_ARP_SYNC: return &c->arp_sync;
    case TC_MENU_ARP_LATCH: return &c->arp_latch;
    case TC_MENU_LIVE: return &c->live_update;
    case TC_MENU_CLOCK_OUT: return &c->clock_out;
    case TC_MENU_PEDAL_NOTE: return &c->pedal_note;
    case TC_MENU_ARROWS: return &c->arrows;
    case TC_MENU_FAMILY: return &c->family;
    case TC_MENU_CAPO: return &c->capo;
    case TC_MENU_FEEL_TIME: return &c->feel_time;
    case TC_MENU_FEEL_VEL: return &c->feel_vel;
    case TC_MENU_LIVE_CH: return &c->live_ch;
    case TC_MENU_LIVE_IN: return &c->live_in;
    case TC_MENU_VOICE_LEAD: return &c->voice_lead;
    case TC_MENU_NOTE_REPEAT: return &c->note_repeat;
    case TC_MENU_MELODY_VEL: return &c->melody_vel;
    case TC_MENU_PEDAL_TARGET: return &c->pedal_target;
    case TC_MENU_SEQ_GUIDE: return &c->seq_guide;
    case TC_MENU_HINTS: return &c->hints;
    case TC_MENU_LOCK: return &c->lock;
    case TC_MENU_ARP_ROTATE: return &c->arp_rotate;
    case TC_MENU_REPEAT_RHYTHM: return &c->repeat_rhythm;
    case TC_MENU_PIVOT: return &c->pivot;
    case TC_MENU_CHORDS_SIDE: return &c->chords_right;
    case TC_MENU_HARMONY: return &c->harmony;
    case TC_MENU_ECHO: return &c->echo;
    case TC_MENU_ECHOES: return &c->echoes;
    case TC_MENU_MONO: return &c->mono;
    case TC_MENU_CHANCE: return &c->chance;
    case TC_MENU_PAD_UNSTICK: return &c->unstick;
    case TC_MENU_PROGRAM_IN: return &c->program_in;
    case TC_MENU_CHORD_PROG: return &c->chord_prog;
    case TC_MENU_MELODY_PROG: return &c->melody_prog;
    case TC_MENU_SPREAD_OF: return &c->spread_chord;
    case TC_MENU_MPE: return &c->mpe;
    case TC_MENU_PAD_VEL_FULL: return &c->pad_vel_full; /* 1.8.3 */
    case TC_MENU_GLIDE: return &c->glide;
    case TC_MENU_GLIDE_TIME: return &c->glide_time;
    case TC_MENU_COLOR_BY: return &c->color_by; /* 1.9.0 */
    case TC_MENU_KEEP_LOOK: return &c->keep_look;
    case TC_MENU_KEEP_FEEL: return &c->keep_feel;
    case TC_MENU_ARP_LOCK: return &c->arp_lock; /* 2.0.0 */
    case TC_MENU_ARP_LOOP: return &c->arp_loop;
    case TC_MENU_CHAIN: return &c->chain;
    case TC_MENU_KEEP_CHORDS: return &c->keep_chords;
    case TC_MENU_U3: return &c->u3_shift;
    case TC_MENU_SLEEP: return &c->sleep;
    case TC_MENU_STEP_PADS: return &c->step_silent;
    default: return NULL;
    }
}
static bool chord_item(uint8_t item) {
    return item==TC_MENU_ADD7 || item==TC_MENU_ADD9 || item==TC_MENU_ADD6 || item==TC_MENU_SUS ||
           item==TC_MENU_PAD_STRUM || item==TC_MENU_SOUND;
}
uint8_t tc_menu_value(const tc_controls *c, uint8_t item) {
    const tc_chord edit=c->chord[c->last_chord%48u];
    switch(item) {
    case TC_MENU_SCALE: return tc_scale_position(c->scale);
    case TC_MENU_LAYOUT: return c->melody_layout;
    case TC_MENU_FOLD: return c->fold;
    case TC_MENU_MELODY_OCT: return (uint8_t)((c->melody_base+TP_OCTAVE_RANGE-TP_MELODY_BASE)/12u);
    case TC_MENU_LOAD: case TC_MENU_SAVE: case TC_MENU_CLEAR_PRESET: return c->preset<TC_PRESETS ? c->preset : 0u;
    case TC_MENU_ADD7: return (edit.add&TC_ADD7) ? 1u : 0u;
    case TC_MENU_ADD9: return (edit.add&TC_ADD9) ? 1u : 0u;
    case TC_MENU_ADD6: return (edit.add&TC_ADD6) ? 1u : 0u;
    case TC_MENU_SUS: return (uint8_t)(edit.sus/2u);
    case TC_MENU_PAD_STRUM: return ((c->strum_off>>(c->last_chord%48u))&1u) ? (uint8_t)0u : (uint8_t)1u;
    case TC_MENU_SOUND: return ((c->chord_mute>>(c->last_chord%48u))&1u) ? (uint8_t)0u : (uint8_t)1u;
    case TC_MENU_TRANSPORT: case TC_MENU_SEQ_PLAY: return c->transport ? 1u : 0u;
    case TC_MENU_SEQ_SLOT: return c->prog_slot;
    case TC_MENU_SEQ_UNIT: return sq.unit;
    case TC_MENU_PAD_TRIGGER: return (uint8_t)(c->pad_trigger-TC_PAD_TRIGGER_MIN);
    case TC_MENU_PAD_RELEASE: return (uint8_t)(c->pad_release-TC_PAD_RELEASE_MIN);
    case TC_MENU_PAD_FULL: return (uint8_t)(c->pad_full-TC_PAD_FULL_MIN);
    case TC_MENU_CHORD_SET: return c->chord_set;
    case TC_MENU_CAPTURE: return c->chord_set && c->chord_set<=TC_CHORD_SETS ? (uint8_t)(c->chord_set-1u) : 0u;
    case TC_MENU_LANE: return (uint8_t)(c->lane%LN_LANES);
    case TC_MENU_LANE_KIND: return ln.lanes[c->lane%LN_LANES].kind;
    case TC_MENU_LANE_LENGTH: return (uint8_t)(ln_length(&ln.lanes[c->lane%LN_LANES])-1u);
    case TC_MENU_LANE_RATE: return (uint8_t)ln_rate(&ln.lanes[c->lane%LN_LANES]);
    case TC_MENU_LANE_CHANNEL: return ln.lanes[c->lane%LN_LANES].channel;
    case TC_MENU_LANE_MUTE: return ln.lanes[c->lane%LN_LANES].mute;
    case TC_MENU_LANE_DIRECTION: return ln.lanes[c->lane%LN_LANES].direction;
    case TC_MENU_LANE_EUCLID: return ln.lanes[c->lane%LN_LANES].euclid;
    case TC_MENU_LANE_ROTATE: return c->lane_rotate[c->lane%LN_LANES];
    case TC_MENU_LANE_SWING: { uint8_t s=ln.lanes[c->lane%LN_LANES].swing; return (uint8_t)((s<LN_SWING_MAX ? s : LN_SWING_MAX)/TC_SWING_STEP); }
    case TC_MENU_LANE_TRANSPOSE: {
        int32_t t=ln.lanes[c->lane%LN_LANES].transpose;
        return (uint8_t)((t<-TC_TRANSPOSE_MENU ? -TC_TRANSPOSE_MENU : t>TC_TRANSPOSE_MENU ? TC_TRANSPOSE_MENU : t)+TC_TRANSPOSE_MENU);
    }
    case TC_MENU_LANE_MUTATE: return c->mutate<TC_MUTATES ? c->mutate : (uint8_t)TC_MUTATE_DEFAULT;
    default:
        if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) return c->buttons[item-TC_MENU_BUTTON1];
        { const uint8_t *f=field((tc_controls *)c,item); return f ? *f : 0u; }
    }
}
/* Per-chord edits from the Chords page, on the last chord pad played. */
static void edit_chord(tc_controls *c, uint8_t item, uint8_t value) {
    if(!c->have_chord) { c->deferred=tc_inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return; }
    uint32_t k=c->last_chord%48u;
    if(item==TC_MENU_PAD_STRUM) {
        if(value) c->strum_off&=~((uint64_t)1u<<k); else c->strum_off|=(uint64_t)1u<<k;
        c->changes++; c->last_action=TC_CHORD_EDIT; return;
    }
    if(item==TC_MENU_SOUND) {
        if(value) c->chord_mute&=~((uint64_t)1u<<k); else c->chord_mute|=(uint64_t)1u<<k;
        c->changes++; c->last_action=TC_CHORD_EDIT; return;
    }
    if(c->chord_set) { c->last_action=TC_SET_NO_EDITS; return; } /* a chord set's chords are as typed, Full's as recorded, the grid's by row */
    tc_chord edit=c->chord[k];
    uint8_t bit=item==TC_MENU_ADD7 ? TC_ADD7 : item==TC_MENU_ADD9 ? TC_ADD9 : TC_ADD6;
    if(item==TC_MENU_SUS) edit.sus=(uint8_t)(2u*value);
    else edit.add=value ? (uint8_t)(edit.add|bit) : (uint8_t)(edit.add&~bit);
    tp_notes notes;
    if(tc_notes(tc_layout(c),k,edit,&notes)) {
        c->chord[k]=edit; c->changes++; c->last_action=TC_CHORD_EDIT;
        if(c->live_update && !c->live_from_midi && c->live_pad==k) c->relayout_live=true;
    }
    else { c->rejected=tc_inc(c->rejected); c->last_action=TC_REJECT; }
}
bool tc_confirm_item(uint8_t item) {
    return item==TC_MENU_RESET_CHORD || item==TC_MENU_RESET_ALL || item==TC_MENU_BUTTONS_RESET ||
           item==TC_MENU_RESET_PAGE || item==TC_MENU_FACTORY;
}
/* tc_init's values, which the resets go back to. */
static tc_controls defaults;
/* Items Reset options sets back: the plain settings, and the scale, layouts, chord set, fold,
 * melody octave, pad thresholds, progression slot and centre buttons. Not: per-chord edits
 * (Reset chord, Reset all edits do those), a progression's unit (part of the recording), actions
 * and saved content (presets, progressions, chord sets). */
static bool resettable(uint8_t item) {
    switch(item) {
    case TC_MENU_SCALE: case TC_MENU_LAYOUT: case TC_MENU_FOLD: case TC_MENU_MELODY_OCT: case TC_MENU_CHORD_SET:
    case TC_MENU_PAD_TRIGGER: case TC_MENU_PAD_RELEASE: case TC_MENU_PAD_FULL: case TC_MENU_SEQ_SLOT: return true;
    default: return (item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) || field(&defaults,item)!=NULL;
    }
}
uint8_t tc_reset_count(uint8_t group) {
    uint8_t n=0;
    for(uint8_t i=0;i<tc_group_size(group);i++) if(resettable(tc_group_items[group][i])) n++;
    return n;
}
static void clear_edits(tc_controls *c) {
    for(uint32_t k=0;k<48u;k++) c->chord[k]=(tc_chord){0,0,0};
    c->strum_off=0u; c->chord_mute=0u; c->changes++;
}
/* A page's options back to their defaults; false (nothing changed) if the layout can't be. */
static bool reset_page(tc_controls *c, tp_engine *e, uint8_t group) {
    tc_init(&defaults);
    tp_layout layout=tc_layout(c), plain=tc_layout(&defaults);
    bool moves=false;
    for(uint8_t i=0;i<tc_group_size(group);i++) switch(tc_group_items[group][i]) {
    case TC_MENU_SCALE: layout.mode=plain.mode; moves=true; break;
    case TC_MENU_LAYOUT: layout.melody_layout=plain.melody_layout; moves=true; break;
    case TC_MENU_FOLD: layout.fold=plain.fold; moves=true; break;
    case TC_MENU_MELODY_OCT: layout.melody_base=plain.melody_base; moves=true; break;
    case TC_MENU_CHORD_SET: layout.chords=plain.chords; moves=true; break;
    default: break;
    }
    if(moves && !tc_set_layout(c,e,layout)) return false;
    for(uint8_t i=0;i<tc_group_size(group);i++) {
        uint8_t item=tc_group_items[group][i], *f=field(c,item);
        if(f) *f=*field(&defaults,item);
        else if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) c->buttons[item-TC_MENU_BUTTON1]=item-TC_MENU_BUTTON1;
        else if(item==TC_MENU_PAD_TRIGGER) {
            c->pad_trigger=defaults.pad_trigger; c->pad_release=defaults.pad_release; c->pad_full=defaults.pad_full;
            if(e->state==TK_ACTIVE) e->config=tc_pad_config(c);
        } else if(item==TC_MENU_SEQ_SLOT && c->prog_slot!=defaults.prog_slot) {
            c->prog_slot=defaults.prog_slot; tc_request(c,TC_REQUEST_PROG_LOAD,c->prog_slot);
        }
    }
    if(group==TC_GROUP_BUTTONS) { c->shift_latched=false; c->button_sustain=false; }
    c->changes++;
    if(c->live_update) c->relayout=true;
    return true;
}
/* A reset item's value set: CANCEL keeps everything; RESET does it. */
static void confirm(tc_controls *c, tp_engine *e) {
    c->ui=TC_UI_MENU;
    if(!c->edit_value) { c->last_action=TC_RESET_KEPT; return; }
    switch(c->menu) {
    case TC_MENU_RESET_CHORD:
        if(!c->have_chord) { c->deferred=tc_inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return; }
        c->chord[c->last_chord%48u]=(tc_chord){0,0,0}; c->strum_off&=~((uint64_t)1u<<(c->last_chord%48u));
        c->chord_mute&=~((uint64_t)1u<<(c->last_chord%48u));
        c->changes++; c->last_action=TC_RESET; return;
    case TC_MENU_RESET_ALL: clear_edits(c); c->last_action=TC_RESET; return;
    case TC_MENU_BUTTONS_RESET:
        (void)reset_page(c,e,TC_GROUP_BUTTONS); c->last_button=TC_ACTIONS; c->last_action=TC_BUTTON; return;
    default: break;
    }
    if(c->recording) { c->last_action=TC_SEQ_BUSY; return; } /* a progression keeps its chord set */
    if(c->menu==TC_MENU_FACTORY) {
        clear_edits(c); c->chord_octaves=TC_OCTAVES_DEFAULT;
        tc_init(&defaults);
        if(!tc_set_layout(c,e,tc_layout(&defaults))) return; /* the key, octaves and layouts */
        for(uint8_t g=0;g<TC_GROUPS;g++) (void)reset_page(c,e,g);
        c->last_action=TC_FACTORY_RESET; return;
    }
    if(reset_page(c,e,c->group)) c->last_action=TC_OPTIONS_RESET;
}
/* Item set to value as the menu's O sets it (1.9.0: also from the editor, tc_set_item). False when
 * refused with the value still open on the screen (the layout can't move, the thresholds' gaps). */
static bool set_item(tc_controls *c, tp_engine *e, uint8_t item, uint8_t value) {
    tp_layout layout=tc_layout(c);
    uint8_t *f=field(c,item);
    if(item==TC_MENU_CHORD_SET && !tc_chords_ok(value)) { c->last_action=TC_SET_EMPTY; return true; }
    if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) {
        c->buttons[item-TC_MENU_BUTTON1]=value; c->shift_latched=false; c->button_sustain=false; c->changes++;
    } else if(item==TC_MENU_SCALE || item==TC_MENU_LAYOUT || item==TC_MENU_MELODY_OCT ||
              item==TC_MENU_FOLD || item==TC_MENU_CHORD_SET) {
        if(item==TC_MENU_SCALE) layout.mode=tc_scale_at(value);
        else if(item==TC_MENU_LAYOUT) layout.melody_layout=value;
        else if(item==TC_MENU_FOLD) layout.fold=value;
        else if(item==TC_MENU_CHORD_SET) layout.chords=value;
        else layout.melody_base=(uint8_t)(TP_MELODY_BASE-TP_OCTAVE_RANGE+12u*value);
        if(!tc_set_layout(c,e,layout)) return false;
    } else if(item==TC_MENU_LOAD || item==TC_MENU_SAVE || item==TC_MENU_CLEAR_PRESET) {
        /* settings.c carries it out and reports PRESET n LOADED/SAVED/CLEARED. */
        tc_request(c,item==TC_MENU_LOAD ? TC_REQUEST_LOAD : item==TC_MENU_SAVE ? TC_REQUEST_SAVE : TC_REQUEST_CLEAR,value);
        c->last_action=TC_COMMIT; return true;
    } else if(chord_item(item)) {
        edit_chord(c,item,value); return true;
    } else if(item==TC_MENU_PAD_TRIGGER || item==TC_MENU_PAD_RELEASE || item==TC_MENU_PAD_FULL) {
        /* The thresholds keep their gaps; the playing session follows at once. */
        uint32_t trigger=c->pad_trigger, release=c->pad_release, full=c->pad_full;
        if(item==TC_MENU_PAD_TRIGGER) trigger=TC_PAD_TRIGGER_MIN+value;
        else if(item==TC_MENU_PAD_RELEASE) release=TC_PAD_RELEASE_MIN+value;
        else full=TC_PAD_FULL_MIN+value;
        if(!tc_pad_settings_ok(trigger,release,full)) { c->rejected=tc_inc(c->rejected); c->last_action=TC_REJECT; return false; }
        c->pad_trigger=(uint8_t)trigger; c->pad_release=(uint8_t)release; c->pad_full=(uint8_t)full; c->changes++;
        if(e->state==TK_ACTIVE) e->config=tc_pad_config(c);
    } else if(item==TC_MENU_SEQ_SLOT) {
        if(c->recording) c->recording=false; /* switching slots abandons a recording */
        c->prog_slot=value; c->changes++; tc_request(c,TC_REQUEST_PROG_LOAD,c->prog_slot);
    } else if(tc_lane_item(item)) {
        tc_lane_setting(c,item,value);
        if(c->last_action==TC_EUCLID_FILLED || c->last_action==TC_LANE_MUTATED) return true; /* LANE FILLED (1.7.0), MUTATED */
    } else if(item==TC_MENU_SEQ_UNIT) {
        sq.unit=value; sq_changes++;
        if(sq.count) tc_request(c,TC_REQUEST_PROG_SAVE,c->prog_slot);
    } else if(f) {
        if(item==TC_MENU_ARP && value!=TC_ARP_OFF) c->arp_last=value;
        *f=value; c->changes++;
        if((item==TC_MENU_VOICING || item==TC_MENU_SPREAD || item==TC_MENU_SPREAD_OF || item==TC_MENU_CAPO) &&
           c->live_update) c->relayout=true;
        if(item==TC_MENU_CHORD_PROG) c->program_again|=1u; /* set again: sent again */
        if(item==TC_MENU_MELODY_PROG) c->program_again|=2u;
        if(item==TC_MENU_U3) { c->shift_once=false; c->u3_held=false; c->shift_latched=false; } /* 2.0.0 */
    }
    c->last_action=TC_SETTING;
    return true;
}
static void apply_setting(tc_controls *c, tp_engine *e) {
    if(c->menu==TC_MENU_IO_TEST || c->menu==TC_MENU_PAD_TEST) { /* leave the test page */
        c->ui=TC_UI_MENU; c->last_action=TC_CANCEL; return;
    }
    if(tc_confirm_item(c->menu)) { confirm(c,e); return; }
    if(c->menu==TC_MENU_CAPTURE) { tc_start_capture(c,c->edit_value); return; }
    if(set_item(c,e,c->menu,c->edit_value)) c->ui=TC_UI_MENU;
}
/* 1.9.0: what the editor may do with an item (sysex.c MENU, ITEM, SET_ITEM). */
uint8_t tc_item_kind(uint8_t item) {
    if(item>=TC_MENU_ITEMS) return TC_ITEM_BOARD;
    if(item==TC_MENU_TRANSPORT || item==TC_MENU_SEQ_PLAY) return TC_ITEM_TOGGLE;
    if(item==TC_MENU_LOAD || item==TC_MENU_SAVE || item==TC_MENU_CLEAR_PRESET) return TC_ITEM_CHOOSE;
    if(tc_confirm_item(item) || chord_item(item) || item==TC_MENU_CAPTURE || item==TC_MENU_VERSION ||
       item==TC_MENU_IO_TEST || item==TC_MENU_PAD_TEST || item==TC_MENU_LED_TEST || item==TC_MENU_PAD_RESCAN ||
       item==TC_MENU_SEQ_RECORD || item==TC_MENU_SEQ_CLEAR || item==TC_MENU_STEPS || item==TC_MENU_SHUT_DOWN ||
       item==TC_MENU_LANE_MUTATE) return TC_ITEM_BOARD; /* 2.0.3: the editor mutates on its own */
    return TC_ITEM_VALUE;
}
bool tc_set_item(tc_controls *c, tp_engine *e, uint8_t item, uint32_t value) {
    switch(tc_item_kind(item)) {
    case TC_ITEM_TOGGLE:
        if(value>1u) return false;
        if(value!=tc_menu_value(c,item)) tc_toggle_transport(c);
        return true;
    case TC_ITEM_VALUE: case TC_ITEM_CHOOSE:
        return value<tc_menu_values(item) && set_item(c,e,item,(uint8_t)value);
    default: return false;
    }
}
/* Items that act when entered instead of opening a value. */
static bool action(tc_controls *c, uint8_t item) {
    switch(item) {
    case TC_MENU_LED_TEST: c->led_test=true; c->last_action=TC_COMMIT; return true;
    case TC_MENU_VERSION: return true;
    case TC_MENU_TRANSPORT: case TC_MENU_SEQ_PLAY: tc_toggle_transport(c); return true; /* 1.8.2: also on the Sequence page */
    case TC_MENU_SEQ_RECORD: tc_start_recording(c); return true;
    case TC_MENU_STEPS: /* the step screen (1.5.0) */
        c->ui=TC_UI_STEPS; c->step_page=0; c->step_rec=false; c->step_held=LN_NONE; c->step_mask=0u; c->last_action=TC_MENU;
        return true;
    case TC_MENU_PAD_RESCAN: c->rescan=true; c->last_action=TC_PADS_RESCANNED; return true; /* 1.7.0 */
    case TC_MENU_SHUT_DOWN: tc_shut_down(c); return true; /* 2.0.0 */
    case TC_MENU_SEQ_CLEAR:
        c->recording=false; sq_clear(&sq,sq.unit,c->scale); tc_request(c,TC_REQUEST_PROG_SAVE,c->prog_slot);
        c->last_action=TC_SEQ_CLEARED; return true;
    default: return false;
    }
}
/* 2.0.0, Shift + the buttons under the screen (the bottom line shows what they do while Shift is on): the play
 * screen's (controls_buttons.c) and the step screen's (controls_steps.c) own; in Settings left goes home,
 * up/down go to the first or last page or item, or change a value 10 at a time, centre sets the item back to
 * its default. */
bool tc_shift_nav(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now) {
    if(c->ui==TC_UI_PLAY) {
        if(c->recording || c->capturing) return false;
        tc_play_shift(c,e,index,now); return true;
    }
    if(c->ui==TC_UI_STEPS) { tc_steps_shift(c,index,now); return true; }
    if(c->ui==TC_UI_EDIT && (c->menu==TC_MENU_IO_TEST || c->menu==TC_MENU_PAD_TEST)) return false;
    if(index==TC_NAV_LEFT) { c->ui=TC_UI_PLAY; c->last_action=TC_MENU; return true; }
    if(index==TC_NAV_RIGHT) return false;
    if(c->ui==TC_UI_GROUPS) {
        if(index==TC_NAV_ENTER) return false;
        c->group=(uint8_t)(index==TC_NAV_UP ? 0u : TC_GROUPS-1u); c->last_action=TC_NEXT; return true;
    }
    uint8_t items[TC_GROUP_MAX], n=tc_page_items(c,c->group,items);
    if(c->ui==TC_UI_MENU && index!=TC_NAV_ENTER) { c->menu=items[index==TC_NAV_UP ? 0u : n-1u]; c->last_action=TC_NEXT; return true; }
    uint8_t values=tc_menu_values(c->menu);
    if(c->ui==TC_UI_EDIT && index!=TC_NAV_ENTER) {
        if(tc_confirm_item(c->menu)) return false;
        if(index==TC_NAV_UP) c->edit_value=(uint8_t)(c->edit_value+10u<values ? c->edit_value+10u : values-1u);
        else c->edit_value=(uint8_t)(c->edit_value>10u ? c->edit_value-10u : 0u);
        c->last_action=TC_NEXT; return true;
    }
    /* centre: the item's default, as Reset options sets it */
    if(!resettable(c->menu) || tc_item_kind(c->menu)!=TC_ITEM_VALUE) { c->last_action=TC_NO_DEFAULT; return true; }
    tc_init(&defaults);
    uint8_t value=tc_menu_value(&defaults,c->menu);
    if(c->ui==TC_UI_EDIT) { c->edit_value=value; apply_setting(c,e); }
    else (void)set_item(c,e,c->menu,value);
    return true;
}
/* Position of the selected item among a page's shown items (0 if it is not there). */
static uint8_t position(const tc_controls *c, const uint8_t *items, uint8_t n) {
    for(uint8_t i=0;i<n;i++) if(items[i]==c->menu) return i;
    return 0;
}
void tc_navigate(tc_controls *c, tp_engine *e, uint32_t index) {
    if(index==TC_NAV_ENTER && (c->nav_held&(1u<<TC_NAV_LEFT))) { c->ui=TC_UI_PLAY; c->last_action=TC_MENU; return; }
    switch(c->ui) {
    case TC_UI_PLAY:
        if(index==TC_NAV_ENTER && c->recording) { tc_finish_recording(c); return; }
        if(index==TC_NAV_ENTER && c->capturing) { tc_finish_capture(c,e); return; }
        if(index==TC_NAV_ENTER) {
            if(c->group>=TC_GROUPS) c->group=0;
            c->ui=TC_UI_GROUPS; c->last_action=TC_MENU;
        } else tc_shortcut(c,e,index);
        return;
    case TC_UI_GROUPS:
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_PLAY; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { c->group=(uint8_t)((c->group+TC_GROUPS-1u)%TC_GROUPS); c->last_action=TC_PREVIOUS; }
        else if(index==TC_NAV_DOWN) { c->group=(uint8_t)((c->group+1u)%TC_GROUPS); c->last_action=TC_NEXT; }
        else { c->ui=TC_UI_MENU; c->menu=tc_group_items[c->group][0]; c->last_action=TC_COMMIT; }
        return;
    case TC_UI_MENU: {
        uint8_t items[TC_GROUP_MAX], n=tc_page_items(c,c->group,items), at=position(c,items,n);
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_GROUPS; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { c->menu=items[(at+n-1u)%n]; c->last_action=TC_PREVIOUS; }
        else if(index==TC_NAV_DOWN) { c->menu=items[(at+1u)%n]; c->last_action=TC_NEXT; }
        else if(!action(c,c->menu)) { c->ui=TC_UI_EDIT; c->edit_value=tc_menu_value(c,c->menu); c->last_action=TC_COMMIT; }
        return;
    }
    case TC_UI_STEPS: tc_steps_nav(c,index); return;
    default: { /* editing: up/down change the value (no wrap), centre/right set it, left cancels */
        uint8_t n=tc_menu_values(c->menu);
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_MENU; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { if(c->edit_value+1u<n) c->edit_value++; c->last_action=TC_NEXT; }
        else if(index==TC_NAV_DOWN) { if(c->edit_value) c->edit_value--; c->last_action=TC_PREVIOUS; }
        else apply_setting(c,e);
        return;
    }
    }
}
