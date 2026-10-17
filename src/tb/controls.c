/* SPDX-License-Identifier: GPL-3.0-or-later - original event/dispatch policy from documented behavior. */
#include "controls.h"
#include <stddef.h>
#include <stdatomic.h>
tc_controls tc_state;
/* Touch-controller bit numbers of each button, as measured on the board.
 * Function aliases 14->10 and 15->8 retain first-match index and alias bitmap. */
static const uint8_t nav[5]={8,7,6,1,3};
static const uint8_t function[16]={6,5,0,1,7,4,3,8,9,10,11,12,14,2,10,9};
static const uint8_t top[12]={3,1,0,4,5,6,7,8,9,10,11,12};
/* Mode buttons left to right are printed 5, 12, M1..M7, U1..U3.
 * M1..M7 are the modes of the major scale in order; 5 is major pentatonic;
 * 12 is chromatic.
 * U1..U3 are the user scales (set from the web editor over SysEx). Values: layout ids. */
const uint8_t tc_mode_buttons[12]={9,11,0,2,3,4,5,1,6,TC_SCALES,TC_SCALES+1u,TC_SCALES+2u};
const uint8_t tc_strum_ms[TC_STRUM_MS]={10,15,20,30,40,60,80,120};
const uint8_t tc_strum_ticks[TC_STRUM_SPEEDS-TC_STRUM_MS]={1,2,3,4,6}; /* 1/96 1/48 1/32 1/24 1/16 */
const uint8_t tc_repeat_ticks[TC_REPEATS]={0,24,12,6,3};               /* off 1/4 1/8 1/16 1/32 */
const uint8_t tc_cutoff_ticks[TC_CUTOFFS]={0,3,6,12,24,48,96};         /* off 1/32 .. 1/1 */
const uint8_t tc_rhythms[TC_RHYTHMS]={0xFF,0x55,0xAA,0x5B,0x49,0x77,0x6D,0x11};
const uint8_t tc_swing_pct[TC_SWINGS]={0,10,20,30,40,50};
const uint8_t tc_arp_ticks[TC_ARP_RATES]={24,12,8,6,4,3}; /* 1/4 1/8 1/8T 1/16 1/16T 1/32 */
const uint8_t tc_arp_gate_pct[TC_ARP_GATES]={25,50,75,100};
uint8_t tc_presets_present;
#define END TC_MENU_ITEMS
const uint8_t tc_group_items[TC_GROUPS][TC_GROUP_MAX]={
    {TC_MENU_SCALE,TC_MENU_HOLD,TC_MENU_VELOCITY,TC_MENU_CHORD_AT,TC_MENU_MELODY_AT,TC_MENU_LIVE,
     TC_MENU_ARROWS,TC_MENU_MELODY_OCT,END},
    {TC_MENU_VOICING,TC_MENU_SPREAD,TC_MENU_ADD7,TC_MENU_ADD9,TC_MENU_ADD6,TC_MENU_SUS,TC_MENU_PAD_STRUM,
     TC_MENU_RESET_CHORD,TC_MENU_RESET_ALL},
    {TC_MENU_LAYOUT,TC_MENU_BASS_CH,TC_MENU_DRUM_CH,END,END,END,END,END,END},
    {TC_MENU_STRUM,TC_MENU_STRUM_SPEED,TC_MENU_REPEAT,TC_MENU_CUTOFF,END,END,END,END,END},
    {TC_MENU_ARP,TC_MENU_ARP_RATE,TC_MENU_ARP_OCTAVES,TC_MENU_ARP_GATE,TC_MENU_TEMPO,TC_MENU_RHYTHM,
     TC_MENU_SWING,TC_MENU_ARP_SYNC,TC_MENU_ARP_LATCH},
    {TC_MENU_CHORD_CH,TC_MENU_MELODY_CH,TC_MENU_TRS_OUT,TC_MENU_CLOCK_OUT,TC_MENU_TRANSPORT,END,END,END,END},
    {TC_MENU_PEDAL,TC_MENU_PEDAL_TYPE,TC_MENU_PEDAL_NOTE,END,END,END,END,END,END},
    {TC_MENU_LOAD,TC_MENU_SAVE,END,END,END,END,END,END,END},
    {TC_MENU_BRIGHTNESS,TC_MENU_COLOURS,TC_MENU_MIDI_LEDS,TC_MENU_LED_TEST,END,END,END,END,END},
    {TC_MENU_VERSION,TC_MENU_IO_TEST,END,END,END,END,END,END,END}};
#undef END
uint8_t tc_group_size(uint8_t g) {
    uint8_t n=0;
    while(g<TC_GROUPS && n<TC_GROUP_MAX && tc_group_items[g][n]<TC_MENU_ITEMS) n++;
    return n;
}
uint32_t tc_tempo_bpm(uint8_t tempo) { return 40u+5u*(tempo<TC_TEMPOS ? tempo : TC_TEMPOS-1u); }
uint8_t tc_at_cc(uint8_t m) {
    return m==TC_AT_CC1 ? 1u : m==TC_AT_CC2 ? 2u : m==TC_AT_CC11 ? 11u : m==TC_AT_CC74 ? 74u : 0u;
}
uint8_t tc_pedal_cc(uint8_t m) {
    return m==TC_PEDAL_SUSTAIN ? 64u : m==TC_PEDAL_SOSTENUTO ? 66u : m==TC_PEDAL_SOFT ? 67u :
           m==TC_PEDAL_PORTAMENTO ? 65u : 0u;
}
static uint32_t inc(uint32_t x) { return x==UINT32_MAX ? x : x+1u; }
bool tc_decode(uint8_t a, uint8_t bit, uint32_t *source, uint32_t *index,
    uint32_t *logical, uint32_t *aliases) {
    const uint8_t *table; uint32_t count, base;
    if (!source || !index || !logical || !aliases || bit>=16u) return false;
    switch(a) {
    case 0x36: *source=TC_NAV; table=nav; count=5; base=0; break;
    case 0x35: *source=TC_FUNCTION; table=function; count=16; base=5; break;
    case 0x39: *source=TC_ROOT; table=top; count=12; base=19; break;
    case 0x38: *source=TC_SCALE; table=top; count=12; base=31; break;
    default: return false;
    }
    *aliases=0; *index=UINT32_MAX;
    for (uint32_t i=0;i<count;i++) if(table[i]==bit) {
        if (*index==UINT32_MAX) *index=i;
        *aliases |= 1u<<i;
    }
    if (!*aliases) return false;
    *logical=base+*index; return true;
}
void tc_init(tc_controls *c) {
    *c=(tc_controls){0}; (void)bi_init(&c->sampler,0);
    c->chord_base=TP_CHORD_BASE; c->melody_base=TP_MELODY_BASE;
    c->at_chord=TC_AT_POLY; c->at_melody=TC_AT_POLY; c->vel_mode=TC_VEL_DYNAMIC; c->brightness=3u;
    c->strum_speed=2u; c->arp_rate=3u; c->arp_gate=1u; c->tempo=16u; /* 20 ms, 1/16, 50%, 120 BPM */
    c->midi_leds=1u; c->arp_last=TC_ARP_UP; c->preset=0xFFu;
    c->chord_octaves=TC_OCTAVES_DEFAULT; c->bass_ch=1u; c->drum_ch=9u; c->pedal_note=TP_DRUM_BASE;
    /* Pedal off until its pin is confirmed on a board: an unexpected signal there must
     * not send sustain to everyone. */
    c->trs_out=1u; c->pedal_mode=TC_PEDAL_OFF; c->pedal_type=TC_PEDAL_AUTO;
}
tp_layout tc_layout(const tc_controls *c) {
    return (tp_layout){c->tonic, c->melody_base, c->chord_base, c->scale, c->melody_layout};
}
uint8_t tc_channel_of(const tc_controls *c, uint32_t k) {
    if(k<48u || k==TP_ARP) return (uint8_t)(c->chord_ch&15u);
    if(k==TP_PEDAL) return (uint8_t)(c->drum_ch&15u);
    if(c->melody_layout==TP_MELODY_DRUMS) {
        uint32_t grid=tp_drum_grid(k);
        if(grid==1u) return (uint8_t)(c->bass_ch&15u);
        if(grid==2u) return (uint8_t)(c->drum_ch&15u);
    }
    return (uint8_t)(c->melody_ch&15u);
}
static bool fits(const tc_controls *c, tp_layout layout) {
    tp_notes notes;
    for (uint32_t k=0;k<TS_KEYS;k++) if (!tc_notes(layout,k,c->chord[k%48u],&notes)) return false;
    return true;
}
bool tc_set_layout(tc_controls *c, tp_engine *e, tp_layout proposed) {
    /* Validate the entire 96-key proposal before a single live-state write. */
    if (proposed.chord_base+TP_OCTAVE_RANGE<TP_CHORD_BASE || proposed.chord_base>TP_CHORD_BASE+TP_OCTAVE_RANGE ||
        proposed.melody_base+TP_OCTAVE_RANGE<TP_MELODY_BASE || proposed.melody_base>TP_MELODY_BASE+TP_OCTAVE_RANGE ||
        !fits(c,proposed)) {
        c->rejected=inc(c->rejected); c->last_action=TC_REJECT; return false;
    }
    e->layout=proposed; c->tonic=proposed.tonic; c->scale=proposed.mode;
    c->chord_base=proposed.chord_base; c->melody_base=proposed.melody_base; c->melody_layout=proposed.melody_layout;
    c->changes++;
    if(c->live_update) c->relayout=true;
    return true;
}
uint8_t tc_menu_values(uint8_t item) {
    static const uint8_t n[TC_MENU_ITEMS]={TC_ALL_SCALES,2u,TC_STRUM_MODES,TC_STRUM_SPEEDS,TC_ARP_MODES,
        TC_ARP_RATES,TC_ARP_OCTAVES,TC_ARP_GATES,TC_TEMPOS,16u,16u,TC_AT_MODES,TC_VEL_MODES,
        TC_PRESETS,TC_PRESETS,2u,TC_BRIGHTNESS_LEVELS,1u,1u,2u,TC_PEDAL_MODES,TC_PEDAL_TYPES,1u,TC_LED_SCHEMES,
        TC_AT_MODES,
        TC_VOICINGS,TC_SPREADS,2u,2u,2u,3u,2u,1u,1u,TP_MELODY_LAYOUTS,16u,16u,TC_REPEATS,TC_CUTOFFS,
        TC_RHYTHMS,TC_SWINGS,2u,2u,2u,2u,2u,128u,TC_ARROWS_MODES,2u*TP_OCTAVE_RANGE/12u+1u};
    return item<TC_MENU_ITEMS ? n[item] : 1u;
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
    case TC_MENU_COLOURS: return &c->led_scheme;
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
    default: return NULL;
    }
}
static bool chord_item(uint8_t item) {
    return item==TC_MENU_ADD7 || item==TC_MENU_ADD9 || item==TC_MENU_ADD6 || item==TC_MENU_SUS ||
           item==TC_MENU_PAD_STRUM;
}
uint8_t tc_menu_value(const tc_controls *c, uint8_t item) {
    const tc_chord edit=c->chord[c->last_chord%48u];
    switch(item) {
    case TC_MENU_SCALE: return tc_scale_position(c->scale);
    case TC_MENU_LAYOUT: return c->melody_layout;
    case TC_MENU_MELODY_OCT: return (uint8_t)((c->melody_base+TP_OCTAVE_RANGE-TP_MELODY_BASE)/12u);
    case TC_MENU_LOAD: case TC_MENU_SAVE: return c->preset<TC_PRESETS ? c->preset : 0u;
    case TC_MENU_ADD7: return (edit.add&TC_ADD7) ? 1u : 0u;
    case TC_MENU_ADD9: return (edit.add&TC_ADD9) ? 1u : 0u;
    case TC_MENU_ADD6: return (edit.add&TC_ADD6) ? 1u : 0u;
    case TC_MENU_SUS: return (uint8_t)(edit.sus/2u);
    case TC_MENU_PAD_STRUM: return ((c->strum_off>>(c->last_chord%48u))&1u) ? (uint8_t)0u : (uint8_t)1u;
    case TC_MENU_TRANSPORT: return c->transport ? 1u : 0u;
    default: { const uint8_t *f=field((tc_controls *)c,item); return f ? *f : 0u; }
    }
}
static void request(tc_controls *c, uint8_t kind, uint8_t slot) {
    c->request=kind; c->request_slot=slot; /* settings.c: st_requests */
}
/* Per-chord edits from the Chords page, on the last chord pad played. */
static void edit_chord(tc_controls *c, uint8_t item, uint8_t value) {
    if(!c->have_chord) { c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return; }
    uint32_t k=c->last_chord%48u;
    if(item==TC_MENU_PAD_STRUM) {
        if(value) c->strum_off&=~((uint64_t)1u<<k); else c->strum_off|=(uint64_t)1u<<k;
        c->changes++; c->last_action=TC_CHORD_EDIT; return;
    }
    tc_chord edit=c->chord[k];
    uint8_t bit=item==TC_MENU_ADD7 ? TC_ADD7 : item==TC_MENU_ADD9 ? TC_ADD9 : TC_ADD6;
    if(item==TC_MENU_SUS) edit.sus=(uint8_t)(2u*value);
    else edit.add=value ? (uint8_t)(edit.add|bit) : (uint8_t)(edit.add&~bit);
    tp_notes notes;
    if(tc_notes(tc_layout(c),k,edit,&notes)) { c->chord[k]=edit; c->changes++; c->last_action=TC_CHORD_EDIT; }
    else { c->rejected=inc(c->rejected); c->last_action=TC_REJECT; }
}
static void apply_setting(tc_controls *c, tp_engine *e) {
    tp_layout layout=tc_layout(c);
    uint8_t *f=field(c,c->menu);
    if(c->menu==TC_MENU_IO_TEST) { c->ui=TC_UI_MENU; c->last_action=TC_CANCEL; return; } /* leave the test page */
    if(c->menu==TC_MENU_SCALE || c->menu==TC_MENU_LAYOUT || c->menu==TC_MENU_MELODY_OCT) {
        if(c->menu==TC_MENU_SCALE) layout.mode=tc_scale_at(c->edit_value);
        else if(c->menu==TC_MENU_LAYOUT) layout.melody_layout=c->edit_value;
        else layout.melody_base=(uint8_t)(TP_MELODY_BASE-TP_OCTAVE_RANGE+12u*c->edit_value);
        if(!tc_set_layout(c,e,layout)) return;
    } else if(c->menu==TC_MENU_LOAD || c->menu==TC_MENU_SAVE) {
        /* settings.c carries it out and reports PRESET n LOADED/SAVED. */
        request(c,c->menu==TC_MENU_LOAD ? TC_REQUEST_LOAD : TC_REQUEST_SAVE,c->edit_value);
        c->ui=TC_UI_MENU; c->last_action=TC_COMMIT; return;
    } else if(chord_item(c->menu)) {
        edit_chord(c,c->menu,c->edit_value);
        c->ui=TC_UI_MENU; return;
    } else if(f) {
        if(c->menu==TC_MENU_ARP && c->edit_value!=TC_ARP_OFF) c->arp_last=c->edit_value;
        *f=c->edit_value; c->changes++;
        if((c->menu==TC_MENU_VOICING || c->menu==TC_MENU_SPREAD) && c->live_update) c->relayout=true;
    }
    c->ui=TC_UI_MENU; c->last_action=TC_SETTING;
}
/* Items that act when entered instead of opening a value. */
static bool action(tc_controls *c, uint8_t item) {
    switch(item) {
    case TC_MENU_LED_TEST: c->led_test=true; c->last_action=TC_COMMIT; return true;
    case TC_MENU_VERSION: return true;
    case TC_MENU_TRANSPORT: c->transport=!c->transport; c->last_action=TC_TRANSPORT; return true;
    case TC_MENU_RESET_ALL:
        for(uint32_t k=0;k<48u;k++) c->chord[k]=(tc_chord){0,0,0};
        c->strum_off=0u; c->changes++; c->last_action=TC_RESET; return true;
    case TC_MENU_RESET_CHORD:
        if(!c->have_chord) { c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return true; }
        c->chord[c->last_chord%48u]=(tc_chord){0,0,0}; c->strum_off&=~((uint64_t)1u<<(c->last_chord%48u));
        c->changes++; c->last_action=TC_RESET; return true;
    default: return false;
    }
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
/* Next defined scale after (or before) the current one, in the Scale menu's order. */
static uint8_t step_scale(uint8_t scale, int32_t direction) {
    int32_t at=tc_scale_position(scale);
    for(uint32_t n=0;n<TC_ALL_SCALES;n++) {
        at=(at+direction+(int32_t)TC_ALL_SCALES)%(int32_t)TC_ALL_SCALES;
        if(tc_scale_defined(tc_scale_at((uint8_t)at))) return tc_scale_at((uint8_t)at);
    }
    return scale;
}
static void shortcut(tc_controls *c, tp_engine *e, uint32_t index) {
    /* Play screen: right = hold, up/down = what Settings > Play > Arrows says, hold
     * left + up/down = previous/next preset; left alone does nothing, so pressing it
     * once too often on the way home is harmless. */
    if(index==TC_NAV_RIGHT) { c->hold=!c->hold; c->changes++; c->last_action=TC_HOLD; return; }
    if(index==TC_NAV_LEFT) return;
    int32_t up=index==TC_NAV_UP ? 1 : -1;
    if((c->nav_held&(1u<<TC_NAV_LEFT)) || c->arrows==TC_ARROWS_PRESET) {
        uint8_t slot;
        if(step_preset(c,-up,&slot)) { request(c,TC_REQUEST_LOAD,slot); c->last_action=TC_COMMIT; }
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
        if(tc_set_layout(c,e,layout)) c->last_action=TC_SCALE_STEP;
        break;
    }
}
/* Position of the selected item on its page (0 if it is not there). */
static uint8_t position(const tc_controls *c) {
    for(uint8_t i=0;i<tc_group_size(c->group);i++) if(tc_group_items[c->group][i]==c->menu) return i;
    return 0;
}
static void navigate(tc_controls *c, tp_engine *e, uint32_t index) {
    if(index==TC_NAV_ENTER && (c->nav_held&(1u<<TC_NAV_LEFT))) { c->ui=TC_UI_PLAY; c->last_action=TC_MENU; return; }
    switch(c->ui) {
    case TC_UI_PLAY:
        if(index==TC_NAV_ENTER) {
            if(c->group>=TC_GROUPS) c->group=0;
            c->ui=TC_UI_GROUPS; c->last_action=TC_MENU;
        } else shortcut(c,e,index);
        return;
    case TC_UI_GROUPS:
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_PLAY; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { c->group=(uint8_t)((c->group+TC_GROUPS-1u)%TC_GROUPS); c->last_action=TC_PREVIOUS; }
        else if(index==TC_NAV_DOWN) { c->group=(uint8_t)((c->group+1u)%TC_GROUPS); c->last_action=TC_NEXT; }
        else { c->ui=TC_UI_MENU; c->menu=tc_group_items[c->group][0]; c->last_action=TC_COMMIT; }
        return;
    case TC_UI_MENU: {
        uint8_t n=tc_group_size(c->group), at=position(c);
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_GROUPS; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { c->menu=tc_group_items[c->group][(at+n-1u)%n]; c->last_action=TC_PREVIOUS; }
        else if(index==TC_NAV_DOWN) { c->menu=tc_group_items[c->group][(at+1u)%n]; c->last_action=TC_NEXT; }
        else if(!action(c,c->menu)) { c->ui=TC_UI_EDIT; c->edit_value=tc_menu_value(c,c->menu); c->last_action=TC_COMMIT; }
        return;
    }
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
    if(c->pedal_mode==TC_PEDAL_HOLD) { c->hold=pressed; c->last_action=TC_HOLD; c->event_sequence=inc(c->event_sequence); }
    if(c->pedal_mode==TC_PEDAL_TRANSPORT && pressed) {
        c->transport=!c->transport; c->last_action=TC_TRANSPORT; c->event_sequence=inc(c->event_sequence);
    }
}
/* The chord control panel. Voicing and Spread: one at a time, pressing the lit one again
 * turns it off. Octave (0.6.2, as the original firmware): each tap turns that octave on or
 * off, so lit octaves stack; turning off the last one goes back to 0. */
static void panel(tc_controls *c, uint32_t index) {
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
bool tc_panel_active(const tc_controls *c, uint32_t index) {
    if(index<=TC_FN_GUITAR) return c->chord_voicing==index+1u;
    if(index<=TC_FN_SPREAD3) return c->chord_spread==index-TC_FN_SPREAD1+1u;
    return index<TC_FUNCTIONS && ((c->chord_octaves>>(index-TC_FN_OCT_M3))&1u);
}
static void dispatch(tc_controls *c, tp_engine *e, uint32_t source, uint32_t index) {
    tp_layout layout=tc_layout(c);
    switch(source) {
    case TC_ROOT:
        layout.tonic=(uint8_t)index;
        if(tc_set_layout(c,e,layout)) c->last_action=TC_TONIC;
        break;
    case TC_SCALE:
        if(tc_mode_buttons[index]==TC_EMPTY_SLOT || !tc_scale_defined(tc_mode_buttons[index])) {
            c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; break;
        }
        layout.mode=(uint8_t)tc_mode_buttons[index];
        if(tc_set_layout(c,e,layout)) c->last_action=TC_MODE;
        break;
    case TC_NAV: navigate(c,e,index); break;
    case TC_FUNCTION:
        c->last_function=(uint8_t)index;
        panel(c,index); /* works on every screen: hear the change while browsing settings */
        break;
    default: break;
    }
}
bool tc_poll(tc_controls *c, tp_engine *e, bi_read read, void *context, uint32_t now) {
    if(c->started && now-c->last_poll<TC_POLL_MS) return false;
    c->started=true; c->last_poll=now;
    if(!bi_sample(&c->sampler,read,context,now)) return false;
    c->cycles=inc(c->cycles);
    for(uint32_t client=0;client<BI_CLIENTS;client++) {
        const struct bi_record *r=&c->sampler.records[client];
        tc_debounce *d=&c->debounce[client];
        c->pressed[client]=0; c->released[client]=0;
        if(!r->valid) {
            d->qualified=false;
            if(r->address==0x36u) c->nav_held=0; /* a lost client must not leave "left held" behind */
            if(r->address==0x35u) c->fn_held=0;
            continue;
        }
        c->unknown[client]=0; c->ambiguous[client]=0;
        uint32_t rising=0, falling=0;
        if(!d->qualified || !r->edges_valid) {
            /* First good sample/reconnection is a baseline, never a phantom press. */
            d->stable=(uint16_t)r->raw; d->candidate=(uint16_t)r->raw;
            for(uint32_t bit=0;bit<16u;bit++) d->changed[bit]=now;
            d->qualified=true;
        } else for(uint32_t bit=0;bit<16u;bit++) {
            uint32_t mask=1u<<bit;
            if((r->raw^d->candidate)&mask) { d->candidate^=(uint16_t)mask; d->changed[bit]=now; }
            if(((d->candidate^d->stable)&mask) && now-d->changed[bit]>=TC_DEBOUNCE_MS) {
                d->stable^=(uint16_t)mask;
                if(d->stable&mask) rising|=mask; else falling|=mask;
            }
        }
        c->pressed[client]=rising; c->released[client]=falling;
        /* Multiple simultaneous root/scale presses have no natural ordering.
         * Publish all events and reject their musical choice as a conflict. */
        bool conflict=(r->address==0x38u || r->address==0x39u) && rising && (rising&(rising-1u));
        if(conflict) { c->conflicts=inc(c->conflicts); c->last_action=TC_CONFLICT; }
        for(uint8_t bit=0;bit<16u;bit++) {
            uint32_t mask=1u<<bit, source, index, logical, aliases;
            if(!tc_decode((uint8_t)r->address,bit,&source,&index,&logical,&aliases)) {
                c->unknown[client]|=r->raw&mask; continue;
            }
            if(aliases&(aliases-1u)) c->ambiguous[client]|=r->raw&mask;
            if(!((rising|falling)&mask)) continue;
            c->event_sequence=inc(c->event_sequence);
            c->last_source=source; c->last_index=index; c->last_aliases=aliases;
            c->last_pressed=(rising&mask) ? 1u : 0u;
            if(source==TC_NAV) {
                if(rising&mask) c->nav_held=(uint8_t)(c->nav_held|1u<<index);
                else c->nav_held=(uint8_t)(c->nav_held&~(1u<<index));
            }
            if(source==TC_FUNCTION && index<TC_FUNCTIONS) {
                if(rising&mask) c->fn_held=(uint16_t)(c->fn_held|1u<<index);
                else c->fn_held=(uint16_t)(c->fn_held&~(1u<<index));
            }
            if(rising&mask) {
                c->presses[logical]=inc(c->presses[logical]);
                if(!conflict) dispatch(c,e,source,index);
            } else c->releases[logical]=inc(c->releases[logical]);
        }
    }
    return true;
}
void tc_publish(const tc_controls *c, const tc_bus *b, volatile uint32_t w[TC_WORDS]) {
    /* Frozen snapshots: one initial publication, then one per changed request.
     * Host owns word244; firmware owns ack245. No fast-producer/telnet race. */
    uint32_t request=w[244];
    if(w[0]==TC_MAGIC && w[245]==request) return;
    uint32_t sequence=(w[3]+2u)&~1u;
    w[3]=sequence|1u; atomic_thread_fence(memory_order_release);
    w[0]=TC_MAGIC; w[1]=1; w[2]=TC_WORDS*4u;
    w[4]=c->cycles; w[5]=c->last_poll; w[6]=c->tonic; w[7]=c->scale;
    w[8]=c->ui; w[9]=c->ui==TC_UI_EDIT ? c->edit_value : c->menu; w[10]=c->have_chord; w[11]=c->last_chord;
    w[12]=c->event_sequence; w[13]=c->last_source; w[14]=c->last_index;
    w[15]=c->last_pressed; w[16]=c->last_aliases; w[17]=c->last_action;
    w[18]=c->rejected; w[19]=c->conflicts; w[20]=c->deferred;
    w[21]=b->enabled; w[22]=b->latched; w[23]=b->phase; w[24]=b->fault;
    w[25]=b->cleanup; w[26]=b->sr1; w[27]=b->sr2; w[28]=b->elapsed_us;
    w[29]=c->sampler.stopped; w[30]=TC_POLL_MS; w[31]=TC_DEBOUNCE_MS;
    (void)bi_publish(&c->sampler,&w[32]);
    for(uint32_t i=0;i<43u;i++) { w[90u+i]=c->presses[i]; w[133u+i]=c->releases[i]; }
    for(uint32_t i=0;i<4u;i++) {
        w[176u+i]=c->unknown[i]; w[180u+i]=c->ambiguous[i];
        w[184u+i]=c->pressed[i]; w[188u+i]=c->released[i];
        w[192u+i]=c->debounce[i].stable;
    }
    for(uint32_t i=0;i<48u;i++) w[196u+i]=c->chord[i].lower|(uint32_t)c->chord[i].sus<<4|(uint32_t)c->chord[i].add<<8;
    w[245]=request;
    for(uint32_t i=246;i<TC_WORDS-1u;i++) w[i]=0;
    w[246]=b->recovery; w[247]=c->recoveries; w[248]=c->chord_base; w[249]=c->melody_base;
    w[250]=c->at_chord|(uint32_t)c->vel_mode<<8|(uint32_t)c->brightness<<16|(uint32_t)c->at_melody<<24;
    w[251]=c->changes;
    w[252]=c->hold|(uint32_t)c->strum<<4|(uint32_t)c->arp<<8|(uint32_t)c->tempo<<16|(uint32_t)c->arp_rate<<24;
    w[253]=c->chord_ch|(uint32_t)c->melody_ch<<8|(uint32_t)c->preset<<16|(uint32_t)c->midi_leds<<24;
    w[254]=sequence; w[255]=0;
    atomic_thread_fence(memory_order_release); w[3]=sequence;
}
