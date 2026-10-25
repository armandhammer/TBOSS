/* SPDX-License-Identifier: GPL-3.0-or-later - original event/dispatch policy from documented behavior. */
#include "controls.h"
#include "sequence.h"
#include "chord_detect.h"
#include <stddef.h>
#include <string.h>
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
const rh_rate tc_arp_rates[TC_ARP_RATES]={{24,1},{12,1},{8,1},{6,1},{4,1},{3,1}, /* 1/4 1/8 1/8T 1/16 1/16T 1/32 */
    {16,1},{2,1},{24,5},{24,7},{18,1},{9,1}}; /* 1.4.0: 1/4T 1/32T 5/beat 7/beat 1/8. 1/16. */
const rh_rate tc_note_repeat_rates[TC_NOTE_REPEATS]={{0,0},{24,1},{12,1},{6,1},{3,1},{8,1},{4,1},{24,5},{0,0}};
rh_rate tc_note_repeat_rate(uint8_t repeat, uint32_t k) {
    static const rh_rate rows[4]={{24,5},{6,1},{8,1},{12,1}}; /* row 0 (top) .. 3 (bottom): 5, 4, 3, 2 per beat */
    if(repeat==TC_REPEAT_BY_ROW) return rows[k%4u];
    return repeat<TC_NOTE_REPEATS ? tc_note_repeat_rates[repeat] : (rh_rate){0,0};
}
uint64_t tc_rhythm_mask(uint8_t value, uint8_t rotate, uint32_t *steps) {
    uint32_t hits, n;
    if(value>=TC_RHYTHMS && rh_euclid_at(value-TC_RHYTHMS,&hits,&n)) { *steps=n; return rh_euclid(hits,n,rotate); }
    *steps=8u;
    return rh_rotate(tc_rhythms[value%TC_RHYTHMS],8u,rotate);
}
_Static_assert(LN_RATES==TC_ARP_RATES,"a lane's rate is an arpeggiator rate");
const uint8_t tc_arp_gate_pct[TC_ARP_GATES]={25,50,75,100};
const uint8_t tc_feel_ms[TC_FEEL_TIMES]={0,5,10,20,40};
const uint8_t tc_feel_vel[TC_FEEL_VELS]={0,5,10,20,30};
const uint8_t tc_echo_ticks[TC_ECHO_TIMES]={0,6,8,12,18,24,36,48}; /* off 1/16 1/8T 1/8 1/8. 1/4 1/4. 1/2 */
const uint8_t tc_chance_pct[TC_CHANCES]={100,90,80,70,60,50,40,30,20,10};
void tc_harmonize(tp_layout layout, uint8_t harmony, tp_notes *notes) {
    /* Steps of a 7-note scale (7: the octave, whatever the scale's size); a scale of another size
     * counts the same number of its own steps. */
    static const int8_t by[TC_HARMONIES][2]={{0,0},{2,0},{3,0},{4,0},{5,0},{7,0},{2,4},{-2,0},{-5,0}};
    if(!harmony || harmony>=TC_HARMONIES) return;
    uint32_t played=notes->count, size=tc_scale_size(layout.mode);
    for(uint32_t i=0;i<played;i++) for(uint32_t h=0;h<2u && by[harmony][h];h++) {
        int32_t steps=by[harmony][h]==7 ? (int32_t)size : by[harmony][h];
        uint8_t note=tc_harmony_note(layout,notes->notes[i],steps);
        bool there=note>127u || notes->count>=TP_MAX_NOTES;
        for(uint32_t j=0;j<notes->count && !there;j++) there=notes->notes[j]==note;
        if(!there) notes->notes[notes->count++]=note;
    }
}
uint8_t tc_favourites[TC_FAVOURITES], tc_favourite_count;
uint8_t tc_mode_scale(const tc_controls *c, uint32_t button) {
    if(button>=12u) return TC_EMPTY_SLOT;
    return button>=2u && button<9u ? tc_family_mode(c->family,button-2u) : tc_mode_buttons[button];
}
uint8_t tc_presets_present;
#define END TC_MENU_ITEMS
#define B(n) (TC_MENU_BUTTON1+(n))
#define R TC_MENU_RESET_PAGE
const uint8_t tc_group_items[TC_GROUPS][TC_GROUP_MAX]={
    {TC_MENU_SCALE,TC_MENU_FAMILY,TC_MENU_HOLD,TC_MENU_VELOCITY,TC_MENU_MELODY_VEL,TC_MENU_CHORD_AT,TC_MENU_MELODY_AT,
     TC_MENU_LIVE,TC_MENU_NOTE_REPEAT,TC_MENU_REPEAT_RHYTHM,TC_MENU_HARMONY,TC_MENU_ECHO,TC_MENU_ECHOES,TC_MENU_TEMPO,
     TC_MENU_ARROWS,TC_MENU_MELODY_OCT,R},
    {TC_MENU_CHORD_SET,TC_MENU_CAPTURE,TC_MENU_VOICING,TC_MENU_SPREAD,TC_MENU_VOICE_LEAD,TC_MENU_FOLD,TC_MENU_HINTS,
     TC_MENU_PIVOT,TC_MENU_ADD7,TC_MENU_ADD9,TC_MENU_ADD6,TC_MENU_SUS,TC_MENU_PAD_STRUM,TC_MENU_SOUND,TC_MENU_RESET_CHORD,
     TC_MENU_RESET_ALL,R},
    {TC_MENU_LAYOUT,TC_MENU_CHORDS_SIDE,TC_MENU_MONO,TC_MENU_LIVE_CH,TC_MENU_LIVE_IN,TC_MENU_BASS_CH,TC_MENU_DRUM_CH,R,END,
     END,END,END,END,END,END,END,END},
    {TC_MENU_STRUM,TC_MENU_STRUM_SPEED,TC_MENU_REPEAT,TC_MENU_CUTOFF,TC_MENU_FEEL_TIME,TC_MENU_FEEL_VEL,
     TC_MENU_CAPO,R,END,END,END,END,END,END,END,END,END},
    {TC_MENU_ARP,TC_MENU_ARP_RATE,TC_MENU_ARP_OCTAVES,TC_MENU_ARP_GATE,TC_MENU_RHYTHM,TC_MENU_ARP_ROTATE,
     TC_MENU_CHANCE,TC_MENU_SWING,TC_MENU_ARP_SYNC,TC_MENU_ARP_LATCH,R,END,END,END,END,END,END},
    {TC_MENU_CHORD_CH,TC_MENU_MELODY_CH,TC_MENU_TRS_OUT,TC_MENU_CLOCK_OUT,TC_MENU_TRANSPORT,TC_MENU_LOCK,R,END,END,
     END,END,END,END,END,END,END,END},
    {TC_MENU_PEDAL,TC_MENU_PEDAL_TYPE,TC_MENU_PEDAL_NOTE,TC_MENU_PEDAL_TARGET,R,END,END,END,END,END,END,END,
     END,END,END,END,END},
    {TC_MENU_LOAD,TC_MENU_SAVE,END,END,END,END,END,END,END,END,END,END,END,END,END,END,END},
    {TC_MENU_BRIGHTNESS,TC_MENU_COLORS,TC_MENU_MIDI_LEDS,TC_MENU_LED_TEST,R,END,END,END,END,END,END,END,
     END,END,END,END,END},
    {TC_MENU_SEQ_SLOT,TC_MENU_SEQ_UNIT,TC_MENU_SEQ_RECORD,TC_MENU_SEQ_CLEAR,TC_MENU_SEQ_GUIDE,TC_MENU_STEPS,TC_MENU_LANE,
     TC_MENU_LANE_KIND,TC_MENU_LANE_LENGTH,TC_MENU_LANE_RATE,TC_MENU_LANE_CHANNEL,TC_MENU_LANE_MUTE,R,END,END,END,END},
    {TC_MENU_PAD_TRIGGER,TC_MENU_PAD_RELEASE,TC_MENU_PAD_FULL,TC_MENU_PAD_TEST,R,END,END,END,END,END,END,END,
     END,END,END,END,END},
    {B(0),B(1),B(2),B(3),B(4),B(5),B(6),B(7),B(8),B(9),B(10),B(11),B(12),B(13),TC_MENU_BUTTONS_RESET,END,END},
    {TC_MENU_VERSION,TC_MENU_IO_TEST,TC_MENU_FACTORY,END,END,END,END,END,END,END,END,END,END,END,END,END,END}};
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
        items[n++]=item;
    }
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
/* What a step tapped on plays before any value is picked: the first chord or scale step in root
 * position (bottom left), or the kick. */
static ln_step brush_default(uint8_t kind) {
    ln_step s; memset(&s,0,sizeof s);
    s.on=true; s.value=kind==LN_DRUMS ? (uint8_t)TP_DRUM_BASE : 3u;
    return s;
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
    c->pad_trigger=TC_PAD_TRIGGER_DEFAULT; c->pad_release=TC_PAD_RELEASE_DEFAULT; c->pad_full=TC_PAD_FULL_DEFAULT;
    c->melody_vel=TC_VEL_DYNAMIC; c->pedal_target=TC_PEDAL_BOTH; c->seq_guide=1u;
    c->echoes=TC_ECHOES_DEFAULT;
    c->step_held=LN_NONE;
    for(uint32_t i=0;i<LN_LANES;i++) c->brush[i]=brush_default(LN_CHORDS);
    tc_buttons_default(c);
}
void tc_buttons_default(tc_controls *c) {
    for(uint32_t i=0;i<TC_FUNCTIONS;i++) c->buttons[i]=(uint8_t)i;
}
bool tc_pad_settings_ok(uint32_t trigger, uint32_t release, uint32_t full) {
    return trigger>=TC_PAD_TRIGGER_MIN && trigger<=TC_PAD_TRIGGER_MAX && release>=TC_PAD_RELEASE_MIN &&
           release<=TC_PAD_RELEASE_MAX && full>=TC_PAD_FULL_MIN && full<=TC_PAD_FULL_MAX &&
           trigger>=release+2u && full>=trigger+5u;
}
tk_config tc_pad_config(const tc_controls *c) {
    uint32_t full=100u*c->pad_full;
    return (tk_config){1u,100u*c->pad_trigger,100u*c->pad_release,full>4095u ? 4095u : full,4u,50u};
}
tk_config tc_pad_raise(tk_config d, uint32_t rest) {
    if(d.polarity!=1u) return d;
    if(rest>TC_PAD_REST_CAP) rest=TC_PAD_REST_CAP;
    if(d.release<rest+TC_PAD_REST_MARGIN) d.release=rest+TC_PAD_REST_MARGIN; /* a resting pad must read released */
    if(d.press<d.release+200u) d.press=d.release+200u;
    if(d.full_scale<d.press+500u) d.full_scale=d.press+500u;
    if(d.full_scale>4095u) d.full_scale=4095u;
    return d;
}
tp_layout tc_layout(const tc_controls *c) {
    return (tp_layout){c->tonic, c->melody_base, c->chord_base, c->scale, c->melody_layout, c->fold, c->chord_set};
}
uint32_t tc_physical(const tc_controls *c, uint32_t k) {
    return c->chords_right && k<TS_KEYS ? (k+48u)%TS_KEYS : k;
}
uint8_t tc_channel_of(const tc_controls *c, uint32_t k) {
    if(k<48u || k==TP_ARP) return (uint8_t)(c->chord_ch&15u);
    if(c->live_ch && tp_live_pad(tc_layout(c),k)) return (uint8_t)((c->live_ch-1u)&15u);
    if(k==TP_PEDAL) return (uint8_t)(c->drum_ch&15u);
    if(c->melody_layout==TP_MELODY_DRUMS) {
        uint32_t grid=tp_drum_grid(k);
        if(grid==1u) return (uint8_t)(c->bass_ch&15u);
        if(grid==2u) return (uint8_t)(c->drum_ch&15u);
    }
    return (uint8_t)(c->melody_ch&15u);
}
/* A chord pad's own edits in this layout: none in the chord grid (its rows shape the chords). */
static tc_chord own_edit(const tc_controls *c, tp_layout layout, uint32_t k) {
    return layout.chords==TC_CHORDS_GRID ? (tc_chord){0,0,0} : c->chord[k%48u];
}
static bool fits(const tc_controls *c, tp_layout layout) {
    tp_notes notes;
    for (uint32_t k=0;k<TS_KEYS;k++) if (!tc_notes(layout,k,own_edit(c,layout,k),&notes)) return false;
    return true;
}
bool tc_set_layout(tc_controls *c, tp_engine *e, tp_layout proposed) {
    /* Validate the entire 96-key proposal before a single live-state write. */
    if (proposed.chord_base+TP_OCTAVE_RANGE<TP_CHORD_BASE || proposed.chord_base>TP_CHORD_BASE+TP_OCTAVE_RANGE ||
        proposed.melody_base+TP_OCTAVE_RANGE<TP_MELODY_BASE || proposed.melody_base>TP_MELODY_BASE+TP_OCTAVE_RANGE ||
        !fits(c,proposed)) {
        c->rejected=inc(c->rejected); c->last_action=TC_REJECT; return false;
    }
    if (c->recording && proposed.chords!=c->chord_set) { c->last_action=TC_SEQ_BUSY; return false; } /* one set per progression */
    e->layout=proposed; c->tonic=proposed.tonic; c->scale=proposed.mode;
    c->chord_base=proposed.chord_base; c->melody_base=proposed.melody_base; c->melody_layout=proposed.melody_layout;
    c->fold=proposed.fold; c->chord_set=proposed.chords;
    c->changes++;
    if(c->live_update) c->relayout=true;
    return true;
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
        TC_HARMONIES,TC_ECHO_TIMES,TC_ECHOES,2u,TC_CHANCES};
    if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) return TC_ACTIONS;
    if(item==TC_MENU_BUTTONS_RESET) return 2u;
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
    case TC_MENU_LOAD: case TC_MENU_SAVE: return c->preset<TC_PRESETS ? c->preset : 0u;
    case TC_MENU_ADD7: return (edit.add&TC_ADD7) ? 1u : 0u;
    case TC_MENU_ADD9: return (edit.add&TC_ADD9) ? 1u : 0u;
    case TC_MENU_ADD6: return (edit.add&TC_ADD6) ? 1u : 0u;
    case TC_MENU_SUS: return (uint8_t)(edit.sus/2u);
    case TC_MENU_PAD_STRUM: return ((c->strum_off>>(c->last_chord%48u))&1u) ? (uint8_t)0u : (uint8_t)1u;
    case TC_MENU_SOUND: return ((c->chord_mute>>(c->last_chord%48u))&1u) ? (uint8_t)0u : (uint8_t)1u;
    case TC_MENU_TRANSPORT: return c->transport ? 1u : 0u;
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
    default:
        if(item>=TC_MENU_BUTTON1 && item<TC_MENU_BUTTON1+TC_FUNCTIONS) return c->buttons[item-TC_MENU_BUTTON1];
        { const uint8_t *f=field((tc_controls *)c,item); return f ? *f : 0u; }
    }
}
static void request(tc_controls *c, uint8_t kind, uint8_t slot) {
    c->request=kind; c->request_slot=slot; /* settings.c: st_requests */
}
/* Progressions (0.11.0): recording replaces the selected slot's progression; each chord pad
 * touch adds a unit (engine.c tc_frame), left removes the last one, centre or down finishes
 * and saves. Nothing recorded (down twice by mistake, or everything taken back) keeps what
 * the slot had: Clear empties a slot. Recording stops the transport; playing runs with it. */
static sq_prog before_recording;
static void start_recording(tc_controls *c) {
    if(c->capturing) { c->last_action=TC_SEQ_BUSY; return; }
    before_recording=sq;
    sq_clear(&sq,sq.unit,c->scale);
    sq.chords=c->chord_set; /* it keeps the chord set it was recorded with */
    c->recording=true; c->transport=false; c->ui=TC_UI_PLAY; c->last_action=TC_SEQ_RECORD;
}
static void finish_recording(tc_controls *c) {
    c->recording=false;
    if(!sq.count) { sq=before_recording; sq_changes++; c->last_action=TC_SEQ_KEPT; return; }
    request(c,TC_REQUEST_PROG_SAVE,c->prog_slot); c->last_action=TC_SEQ_SAVED;
}
/* Chord capture (1.4.0): Settings > Chords > Capture picks a set slot; then each chord played (a
 * chord pad, or notes held together on MIDI in, engine.c) is its next column, left takes the last
 * one back, centre finishes. The set is kept relative to the key the chords fit best, and the
 * board moves to that key (major M1, minor M6) with the set selected, so it sounds as played. */
static uint8_t capture_roots[12];
static uint16_t capture_tones[12];
static uint32_t capture_count;
static void start_capture(tc_controls *c, uint8_t slot) {
    if(c->recording) { c->last_action=TC_SEQ_BUSY; return; }
    c->capturing=true; c->capture_slot=(uint8_t)(slot%TC_CHORD_SETS); capture_count=0;
    c->ui=TC_UI_PLAY; c->last_action=TC_CAPTURE_START;
}
void tc_capture_chord(tc_controls *c, uint8_t root, uint16_t tones) {
    if(!c->capturing || !(tones&1u)) return;
    root%=12u;
    uint16_t kept=0u; /* a set's chord has at most 6 notes: the lowest */
    for(uint32_t b=0,n=0;b<12u && n<TC_SET_TONES;b++) if(((uint32_t)tones>>b)&1u) { kept|=(uint16_t)(1u<<b); n++; }
    tones=kept;
    if(capture_count && capture_roots[capture_count-1u]==root && capture_tones[capture_count-1u]==tones) return;
    if(capture_count>=12u) { c->last_action=TC_CAPTURE_FULL; c->event_sequence++; return; }
    capture_roots[capture_count]=root; capture_tones[capture_count]=tones; capture_count++;
    c->last_action=TC_CAPTURE_ADDED; c->event_sequence++;
}
uint32_t tc_captured(uint8_t roots[12], uint16_t tones[12]) {
    for(uint32_t i=0;i<capture_count;i++) { roots[i]=capture_roots[i]; tones[i]=capture_tones[i]; }
    return capture_count;
}
static void finish_capture(tc_controls *c, tp_engine *e) {
    c->capturing=false;
    if(!capture_count) { c->last_action=TC_CAPTURE_EMPTY; return; }
    uint8_t key=cd_key(capture_roots,capture_tones,capture_count), tonic=(uint8_t)((key-1u)%12u);
    tc_chord_set set; memset(&set,0,sizeof set);
    for(uint32_t i=0;i<capture_count;i++)
        set.column[i]=(tc_set_chord){capture_tones[i],(uint8_t)((capture_roots[i]+12u-tonic)%12u)};
    static const char name[]="CAPTURE ";
    for(uint32_t i=0;name[i];i++) set.name[i]=name[i];
    set.name[8]=(char)('1'+c->capture_slot);
    if(!tc_chord_set_put(c->capture_slot,&set)) { c->last_action=TC_REJECT; return; }
    request(c,TC_REQUEST_SET_SAVE,c->capture_slot);
    tp_layout layout=tc_layout(c);
    layout.tonic=tonic; layout.mode=key>12u ? tc_mode_buttons[7] : tc_mode_buttons[2]; layout.chords=(uint8_t)(c->capture_slot+1u);
    c->capture_key=key;
    if(tc_set_layout(c,e,layout)) c->last_action=TC_CAPTURE_SAVED;
}
static void toggle_transport(tc_controls *c) {
    if(c->recording) finish_recording(c);
    c->transport=!c->transport;
    c->last_action=c->transport && !sq.count && !ln_any(&ln) ? TC_SEQ_EMPTY : TC_TRANSPORT;
}
/* The step sequencer's lanes (1.5.0, lanes.h): the Sequence page's lane items set the lane Lane
 * chose (the step screen's up/down choose it too). A new kind starts picking from its default. */
static void lane_setting(tc_controls *c, uint8_t item, uint8_t v) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    switch(item) {
    case TC_MENU_LANE: c->lane=(uint8_t)(v%LN_LANES); return;
    case TC_MENU_LANE_KIND:
        if(l->kind!=v) c->brush[at]=brush_default(v);
        l->kind=v<LN_KINDS ? v : (uint8_t)LN_OFF; break;
    case TC_MENU_LANE_LENGTH: ln_set_length(l,v+1u); break;
    case TC_MENU_LANE_RATE: ln_set_rate(l,v); break;
    case TC_MENU_LANE_CHANNEL: l->channel=v<LN_CHANNELS ? v : 0u; break;
    default: l->mute=v ? 1u : 0u; break;
    }
    ln_changed(at,LN_STEPS);
}
static bool lane_item(uint8_t item) { return item>=TC_MENU_LANE && item<=TC_MENU_LANE_MUTE; }
/* The step screen (1.5.0). Step record starts at the first step of the page shown. */
static void step_record(tc_controls *c) {
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
static void steps_nav(tc_controls *c, uint32_t index) {
    ln_lane *l=&ln.lanes[c->lane%LN_LANES];
    uint32_t length=ln_length(l);
    switch(index) {
    case TC_NAV_LEFT:
        if(c->step_rec) { /* take the last one back */
            uint32_t at=(c->step_cursor%length+length-1u)%length;
            memset(&l->steps[at],0,sizeof l->steps[at]); ln_changed(c->lane%LN_LANES,at);
            c->step_cursor=(uint8_t)at; c->step_page=(uint8_t)(at/48u); c->last_action=TC_STEP_BACK;
        } else if(c->step_page) { c->step_page=0; c->last_action=TC_PREVIOUS; }
        else { c->ui=TC_UI_MENU; c->step_held=LN_NONE; c->step_rec=false; c->last_action=TC_CANCEL; }
        return;
    case TC_NAV_RIGHT:
        if(c->step_rec) { ln_step rest; memset(&rest,0,sizeof rest); step_fill(c,rest); c->last_action=TC_STEP_REST; }
        else { c->step_page=1; c->last_action=TC_NEXT; }
        return;
    case TC_NAV_UP: case TC_NAV_DOWN:
        c->lane=(uint8_t)((c->lane+(index==TC_NAV_UP ? LN_LANES-1u : 1u))%LN_LANES);
        c->step_held=LN_NONE; c->step_cursor=(uint8_t)(c->step_rec ? 0u : c->step_cursor);
        if(c->step_rec) c->step_page=0;
        c->last_action=TC_STEP_LANE; return;
    default: step_record(c); return;
    }
}
void tc_step_pad(tc_controls *c, tp_layout layout, uint32_t k, bool pressed, uint8_t velocity) {
    uint8_t at=(uint8_t)(c->lane%LN_LANES);
    ln_lane *l=&ln.lanes[at];
    if(k>=TS_KEYS) return;
    if(k>=48u) { /* a step: on or off when lifted, unless it was given a value while held */
        uint32_t pad=k-48u, step=c->step_page*48u+(pad%4u)*12u+pad/4u; /* rows top down, 12 a row */
        if(pressed) {
            if(step>=ln_length(l)) { c->last_action=TC_STEP_PAST; c->event_sequence++; return; }
            c->step_held=(uint8_t)step; c->step_set=false; return;
        }
        if(c->step_held!=step) return;
        c->step_held=LN_NONE;
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
    if(c->step_held<LN_STEPS) {
        l->steps[c->step_held]=s; c->step_set=true; ln_changed(at,c->step_held);
        c->last_index=c->step_held; c->last_action=TC_STEP_SET;
    } else if(c->step_rec) { c->last_index=c->step_cursor%ln_length(l); step_fill(c,s); c->last_action=TC_STEP_FILLED; }
    else c->last_action=TC_STEP_PICKED;
    c->event_sequence++;
}
/* Per-chord edits from the Chords page, on the last chord pad played. */
static void edit_chord(tc_controls *c, uint8_t item, uint8_t value) {
    if(!c->have_chord) { c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return; }
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
    else { c->rejected=inc(c->rejected); c->last_action=TC_REJECT; }
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
            c->prog_slot=defaults.prog_slot; request(c,TC_REQUEST_PROG_LOAD,c->prog_slot);
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
        if(!c->have_chord) { c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; return; }
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
static void apply_setting(tc_controls *c, tp_engine *e) {
    tp_layout layout=tc_layout(c);
    uint8_t *f=field(c,c->menu);
    if(c->menu==TC_MENU_IO_TEST || c->menu==TC_MENU_PAD_TEST) { /* leave the test page */
        c->ui=TC_UI_MENU; c->last_action=TC_CANCEL; return;
    }
    if(tc_confirm_item(c->menu)) { confirm(c,e); return; }
    if(c->menu==TC_MENU_CAPTURE) { start_capture(c,c->edit_value); return; }
    if(c->menu==TC_MENU_CHORD_SET && !tc_chords_ok(c->edit_value)) {
        c->ui=TC_UI_MENU; c->last_action=TC_SET_EMPTY; return;
    }
    if(c->menu>=TC_MENU_BUTTON1 && c->menu<TC_MENU_BUTTON1+TC_FUNCTIONS) {
        c->buttons[c->menu-TC_MENU_BUTTON1]=c->edit_value; c->shift_latched=false; c->button_sustain=false; c->changes++;
    } else if(c->menu==TC_MENU_SCALE || c->menu==TC_MENU_LAYOUT || c->menu==TC_MENU_MELODY_OCT ||
              c->menu==TC_MENU_FOLD || c->menu==TC_MENU_CHORD_SET) {
        if(c->menu==TC_MENU_SCALE) layout.mode=tc_scale_at(c->edit_value);
        else if(c->menu==TC_MENU_LAYOUT) layout.melody_layout=c->edit_value;
        else if(c->menu==TC_MENU_FOLD) layout.fold=c->edit_value;
        else if(c->menu==TC_MENU_CHORD_SET) layout.chords=c->edit_value;
        else layout.melody_base=(uint8_t)(TP_MELODY_BASE-TP_OCTAVE_RANGE+12u*c->edit_value);
        if(!tc_set_layout(c,e,layout)) return;
    } else if(c->menu==TC_MENU_LOAD || c->menu==TC_MENU_SAVE) {
        /* settings.c carries it out and reports PRESET n LOADED/SAVED. */
        request(c,c->menu==TC_MENU_LOAD ? TC_REQUEST_LOAD : TC_REQUEST_SAVE,c->edit_value);
        c->ui=TC_UI_MENU; c->last_action=TC_COMMIT; return;
    } else if(chord_item(c->menu)) {
        edit_chord(c,c->menu,c->edit_value);
        c->ui=TC_UI_MENU; return;
    } else if(c->menu==TC_MENU_PAD_TRIGGER || c->menu==TC_MENU_PAD_RELEASE || c->menu==TC_MENU_PAD_FULL) {
        /* The thresholds keep their gaps; the playing session follows at once. */
        uint32_t trigger=c->pad_trigger, release=c->pad_release, full=c->pad_full;
        if(c->menu==TC_MENU_PAD_TRIGGER) trigger=TC_PAD_TRIGGER_MIN+c->edit_value;
        else if(c->menu==TC_MENU_PAD_RELEASE) release=TC_PAD_RELEASE_MIN+c->edit_value;
        else full=TC_PAD_FULL_MIN+c->edit_value;
        if(!tc_pad_settings_ok(trigger,release,full)) { c->rejected=inc(c->rejected); c->last_action=TC_REJECT; return; }
        c->pad_trigger=(uint8_t)trigger; c->pad_release=(uint8_t)release; c->pad_full=(uint8_t)full; c->changes++;
        if(e->state==TK_ACTIVE) e->config=tc_pad_config(c);
    } else if(c->menu==TC_MENU_SEQ_SLOT) {
        if(c->recording) c->recording=false; /* switching slots abandons a recording */
        c->prog_slot=c->edit_value; c->changes++; request(c,TC_REQUEST_PROG_LOAD,c->prog_slot);
    } else if(lane_item(c->menu)) {
        lane_setting(c,c->menu,c->edit_value);
    } else if(c->menu==TC_MENU_SEQ_UNIT) {
        sq.unit=c->edit_value; sq_changes++;
        if(sq.count) request(c,TC_REQUEST_PROG_SAVE,c->prog_slot);
    } else if(f) {
        if(c->menu==TC_MENU_ARP && c->edit_value!=TC_ARP_OFF) c->arp_last=c->edit_value;
        *f=c->edit_value; c->changes++;
        if((c->menu==TC_MENU_VOICING || c->menu==TC_MENU_SPREAD || c->menu==TC_MENU_CAPO) && c->live_update) c->relayout=true;
    }
    c->ui=TC_UI_MENU; c->last_action=TC_SETTING;
}
/* Items that act when entered instead of opening a value. */
static bool action(tc_controls *c, uint8_t item) {
    switch(item) {
    case TC_MENU_LED_TEST: c->led_test=true; c->last_action=TC_COMMIT; return true;
    case TC_MENU_VERSION: return true;
    case TC_MENU_TRANSPORT: toggle_transport(c); return true;
    case TC_MENU_SEQ_RECORD: start_recording(c); return true;
    case TC_MENU_STEPS: /* the step screen (1.5.0) */
        c->ui=TC_UI_STEPS; c->step_page=0; c->step_rec=false; c->step_held=LN_NONE; c->last_action=TC_MENU; return true;
    case TC_MENU_SEQ_CLEAR:
        c->recording=false; sq_clear(&sq,sq.unit,c->scale); request(c,TC_REQUEST_PROG_SAVE,c->prog_slot);
        c->last_action=TC_SEQ_CLEARED; return true;
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
static void shortcut(tc_controls *c, tp_engine *e, uint32_t index) {
    /* Play screen: right = hold, up/down = what Settings > Play > Arrows says, hold
     * left + up/down = previous/next preset; left alone does nothing, so pressing it
     * once too often on the way home is harmless. */
    if(index==TC_NAV_RIGHT) { c->hold=!c->hold; c->changes++; c->last_action=TC_HOLD; return; }
    if(index==TC_NAV_LEFT && c->capturing) { /* capturing: take back the last chord */
        if(capture_count) capture_count--;
        c->last_action=TC_CAPTURE_UNDO; return;
    }
    if(index==TC_NAV_LEFT) { /* recording: take back the last unit */
        if(c->recording) { sq_undo(&sq); c->last_action=TC_SEQ_UNDO; }
        return;
    }
    int32_t up=index==TC_NAV_UP ? 1 : -1;
    bool left=(c->nav_held&(1u<<TC_NAV_LEFT))!=0u; /* hold left: presets, whatever the arrows do */
    if(c->arrows==TC_ARROWS_SEQUENCER && (!left || c->recording)) { /* up: play/stop, down: record/finish */
        if(up>0) toggle_transport(c);
        else if(c->recording) finish_recording(c);
        else start_recording(c);
        return;
    }
    if(c->recording) return;
    if(left || c->arrows==TC_ARROWS_PRESET) {
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
        if(tc_set_layout(c,e,layout)) c->last_action=tc_favourite_count ? TC_FAVOURITE_STEP : TC_SCALE_STEP;
        break;
    }
}
/* Position of the selected item among a page's shown items (0 if it is not there). */
static uint8_t position(const tc_controls *c, const uint8_t *items, uint8_t n) {
    for(uint8_t i=0;i<n;i++) if(items[i]==c->menu) return i;
    return 0;
}
static void navigate(tc_controls *c, tp_engine *e, uint32_t index) {
    if(index==TC_NAV_ENTER && (c->nav_held&(1u<<TC_NAV_LEFT))) { c->ui=TC_UI_PLAY; c->last_action=TC_MENU; return; }
    switch(c->ui) {
    case TC_UI_PLAY:
        if(index==TC_NAV_ENTER && c->recording) { finish_recording(c); return; }
        if(index==TC_NAV_ENTER && c->capturing) { finish_capture(c,e); return; }
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
        uint8_t items[TC_GROUP_MAX], n=tc_page_items(c,c->group,items), at=position(c,items,n);
        if(index==TC_NAV_LEFT) { c->ui=TC_UI_GROUPS; c->last_action=TC_CANCEL; }
        else if(index==TC_NAV_UP) { c->menu=items[(at+n-1u)%n]; c->last_action=TC_PREVIOUS; }
        else if(index==TC_NAV_DOWN) { c->menu=items[(at+1u)%n]; c->last_action=TC_NEXT; }
        else if(!action(c,c->menu)) { c->ui=TC_UI_EDIT; c->edit_value=tc_menu_value(c,c->menu); c->last_action=TC_COMMIT; }
        return;
    }
    case TC_UI_STEPS: steps_nav(c,index); return;
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
        toggle_transport(c); c->event_sequence=inc(c->event_sequence);
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
bool tc_button_held(const tc_controls *c, uint8_t action) {
    for(uint32_t i=0;i<TC_FUNCTIONS;i++) if(c->buttons[i]==action && (((uint32_t)c->fn_held>>i)&1u)) return true;
    return false;
}
bool tc_shift(const tc_controls *c) { return c->shift_latched || tc_button_held(c,TC_ACT_SHIFT); }
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
tc_chord tc_pad_edit(const tc_controls *c, tp_layout layout, uint32_t k) {
    tc_chord edit=own_edit(c,layout,k);
    if(!c->fn_held || (layout.chords && layout.chords!=TC_CHORDS_GRID) || tc_shift(c)) return edit; /* Shift gives the buttons their panel function */
    tc_chord shaped=edit;
    if(tc_button_held(c,TC_ACT_ADD7)) shaped.add|=TC_ADD7;
    if(tc_button_held(c,TC_ACT_ADD9)) shaped.add|=TC_ADD9;
    if(tc_button_held(c,TC_ACT_ADD6)) shaped.add|=TC_ADD6;
    if(tc_button_held(c,TC_ACT_SUS2)) shaped.sus=2u;
    if(tc_button_held(c,TC_ACT_SUS4)) shaped.sus=4u;
    tp_notes notes; /* a shape that leaves MIDI range plays the pad as it is */
    return tc_notes(layout,k%48u,shaped,&notes) ? shaped : edit;
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
static void button(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now) {
    uint8_t act=c->buttons[index];
    if(act==TC_ACT_SHIFT) {
        /* Two taps close together latch it; a tap while latched lets go. */
        if(c->shift_latched) { c->shift_latched=false; c->shift_tapped=0u; }
        else if(c->shift_tapped && now-c->shift_tapped<=TC_SHIFT_LATCH_MS) { c->shift_latched=true; c->shift_tapped=0u; }
        else c->shift_tapped=now;
        c->last_button=act; c->last_action=TC_BUTTON; return;
    }
    if(tc_shift(c)) act=(uint8_t)index; /* Shift + a button: its printed panel function */
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
        if(step_preset(c,act==TC_ACT_PRESET_NEXT ? 1 : -1,&slot)) { request(c,TC_REQUEST_LOAD,slot); c->last_action=TC_COMMIT; }
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
    case TC_ACT_PLAY: toggle_transport(c); break;
    case TC_ACT_RECORD: /* on the step screen: step record (1.5.0) */
        if(c->ui==TC_UI_STEPS) step_record(c); else if(c->recording) finish_recording(c); else start_recording(c);
        break;
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
/* Shift + root button 1..8: play that progression (again: stop). */
static void shift_root(tc_controls *c, uint32_t index) {
    if(index>=SQ_SLOTS) { c->last_action=TC_SHIFT_UNUSED; return; }
    if(c->recording) { finish_recording(c); return; }
    if(c->prog_slot==index && c->transport) { c->transport=false; c->last_action=TC_TRANSPORT; return; }
    if(!(((uint32_t)sq_present>>index)&1u)) { c->prog_slot=(uint8_t)index; c->changes++; request(c,TC_REQUEST_PROG_LOAD,(uint8_t)index);
        c->transport=false; c->last_action=TC_SEQ_EMPTY; return; }
    if(c->prog_slot!=index) { c->prog_slot=(uint8_t)index; c->changes++; request(c,TC_REQUEST_PROG_LOAD,(uint8_t)index); }
    c->transport=true; c->last_action=TC_TRANSPORT;
}
static void dispatch(tc_controls *c, tp_engine *e, uint32_t source, uint32_t index, uint32_t now) {
    tp_layout layout=tc_layout(c);
    switch(source) {
    case TC_ROOT:
        if(tc_button_held(c,TC_ACT_PIVOT) && !tc_shift(c)) { /* Pivot held: that key becomes the target */
            bool minor=tc_in_scale(c->scale,3u) && !tc_in_scale(c->scale,4u);
            c->pivot=(uint8_t)(1u+index+(minor ? 12u : 0u)); c->changes++; c->last_action=TC_PIVOT_SET;
            break;
        }
        if(tc_shift(c)) { shift_root(c,index); break; }
        layout.tonic=(uint8_t)index;
        if(c->pivot && index==(c->pivot-1u)%12u) {
            /* Arriving at the pivot target: from major or natural minor, also its major or minor. */
            uint8_t major=tc_mode_buttons[2], minor=tc_mode_buttons[7];
            if(c->scale==major || c->scale==minor) layout.mode=c->pivot>12u ? minor : major;
            if(tc_set_layout(c,e,layout)) { c->pivot=0u; c->last_action=TC_PIVOT_ARRIVED; }
            break;
        }
        if(tc_set_layout(c,e,layout)) c->last_action=TC_TONIC;
        break;
    case TC_SCALE:
        if(tc_shift(c)) { /* Shift + mode button 1..8: load that preset */
            if(index>=TC_PRESETS) c->last_action=TC_SHIFT_UNUSED;
            else if(!(((uint32_t)tc_presets_present>>index)&1u)) c->last_action=TC_PRESET_EMPTY;
            else if(!c->recording) { request(c,TC_REQUEST_LOAD,(uint8_t)index); c->last_action=TC_COMMIT; }
            break;
        }
        if(!tc_scale_defined(tc_mode_scale(c,index))) {
            c->deferred=inc(c->deferred); c->last_action=TC_TODO_FUNCTION; break;
        }
        layout.mode=tc_mode_scale(c,index);
        if(tc_set_layout(c,e,layout)) c->last_action=TC_MODE;
        break;
    case TC_NAV: navigate(c,e,index); break;
    case TC_FUNCTION:
        if(c->ui==TC_UI_MENU && c->group==TC_GROUP_BUTTONS) { /* on the Buttons page: go to its item */
            c->menu=(uint8_t)(TC_MENU_BUTTON1+index); c->last_action=TC_NEXT; break;
        }
        button(c,e,index,now);
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
                /* A chord shape held on a button ends: re-strike held pads without it. */
                uint8_t act=c->buttons[index];
                if((falling&mask) && act>=TC_ACT_ADD7 && act<=TC_ACT_SUS4 && c->live_update) c->relayout=true;
            }
            if(rising&mask) {
                c->presses[logical]=inc(c->presses[logical]);
                if(!conflict) dispatch(c,e,source,index,now);
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
