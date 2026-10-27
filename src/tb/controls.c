/* SPDX-License-Identifier: GPL-3.0-or-later - original event/dispatch policy from documented behavior.
 * The controls' files: controls.c (the touch controllers' buttons, the board's state, layouts and
 * pad settings), controls_menu.c (Settings), controls_buttons.c (playing buttons), controls_steps.c
 * (the step screen) and controls_record.c (recording, capture); controls_internal.h joins them. */
#include "controls_internal.h"
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
uint32_t tc_tempo_bpm(uint8_t tempo) { return 40u+5u*(tempo<TC_TEMPOS ? tempo : TC_TEMPOS-1u); }
uint8_t tc_at_cc(uint8_t m) {
    return m==TC_AT_CC1 ? 1u : m==TC_AT_CC2 ? 2u : m==TC_AT_CC11 ? 11u : m==TC_AT_CC74 ? 74u : 0u;
}
uint8_t tc_pedal_cc(uint8_t m) {
    return m==TC_PEDAL_SUSTAIN ? 64u : m==TC_PEDAL_SOSTENUTO ? 66u : m==TC_PEDAL_SOFT ? 67u :
           m==TC_PEDAL_PORTAMENTO ? 65u : 0u;
}
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
    /* Pedal: sustain by default (1.7.0; off before, until its pin was confirmed on a board). Saved
     * settings keep the mode they have. Auto: the level at start-up is "up". */
    c->trs_out=1u; c->pedal_mode=TC_PEDAL_SUSTAIN; c->pedal_type=TC_PEDAL_AUTO;
    c->pad_trigger=TC_PAD_TRIGGER_DEFAULT; c->pad_release=TC_PAD_RELEASE_DEFAULT; c->pad_full=TC_PAD_FULL_DEFAULT;
    c->unstick=1u;
    c->melody_vel=TC_VEL_DYNAMIC; c->pedal_target=TC_PEDAL_BOTH; c->seq_guide=1u;
    c->echoes=TC_ECHOES_DEFAULT;
    c->step_held=LN_NONE;
    for(uint32_t i=0;i<LN_LANES;i++) c->brush[i]=tc_brush_default(LN_CHORDS);
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
    d.release+=rest; d.press+=rest; d.full_scale+=rest; /* at most 2000 + 2000 for Trigger: below Full */
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
    return (uint8_t)(c->melody_ch&15u); /* also the Bass layout (1.7.0): pedal, pressure and Melody prog go there too */
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
        c->rejected=tc_inc(c->rejected); c->last_action=TC_REJECT; return false;
    }
    if (c->recording && proposed.chords!=c->chord_set) { c->last_action=TC_SEQ_BUSY; return false; } /* one set per progression */
    e->layout=proposed; c->tonic=proposed.tonic; c->scale=proposed.mode;
    c->chord_base=proposed.chord_base; c->melody_base=proposed.melody_base; c->melody_layout=proposed.melody_layout;
    c->fold=proposed.fold; c->chord_set=proposed.chords;
    c->changes++;
    if(c->live_update) c->relayout=true;
    return true;
}
void tc_request(tc_controls *c, uint8_t kind, uint8_t slot) {
    c->request=kind; c->request_slot=slot; /* settings.c: st_requests */
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
static void dispatch(tc_controls *c, tp_engine *e, uint32_t source, uint32_t index, uint32_t now) {
    tp_layout layout=tc_layout(c);
    switch(source) {
    case TC_ROOT:
        if(tc_button_held(c,TC_ACT_PIVOT) && !tc_shift(c)) { /* Pivot held: that key becomes the target */
            bool minor=tc_in_scale(c->scale,3u) && !tc_in_scale(c->scale,4u);
            c->pivot=(uint8_t)(1u+index+(minor ? 12u : 0u)); c->changes++; c->last_action=TC_PIVOT_SET;
            break;
        }
        if(tc_shift(c)) { tc_shift_root(c,index); break; }
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
            else if(!c->recording) { tc_request(c,TC_REQUEST_LOAD,(uint8_t)index); c->last_action=TC_COMMIT; }
            break;
        }
        if(!tc_scale_defined(tc_mode_scale(c,index))) {
            c->deferred=tc_inc(c->deferred); c->last_action=TC_TODO_FUNCTION; break;
        }
        layout.mode=tc_mode_scale(c,index);
        if(tc_set_layout(c,e,layout)) c->last_action=TC_MODE;
        break;
    case TC_NAV: tc_navigate(c,e,index); break;
    case TC_FUNCTION:
        if(c->ui==TC_UI_MENU && c->group==TC_GROUP_BUTTONS) { /* on the Buttons page: go to its item */
            c->menu=(uint8_t)(TC_MENU_BUTTON1+index); c->last_action=TC_NEXT; break;
        }
        tc_button(c,e,index,now);
        break;
    default: break;
    }
}
bool tc_poll(tc_controls *c, tp_engine *e, bi_read read, void *context, uint32_t now) {
    if(c->started && now-c->last_poll<TC_POLL_MS) return false;
    c->started=true; c->last_poll=now;
    if(!bi_sample(&c->sampler,read,context,now)) return false;
    c->cycles=tc_inc(c->cycles);
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
        if(conflict) { c->conflicts=tc_inc(c->conflicts); c->last_action=TC_CONFLICT; }
        for(uint8_t bit=0;bit<16u;bit++) {
            uint32_t mask=1u<<bit, source, index, logical, aliases;
            if(!tc_decode((uint8_t)r->address,bit,&source,&index,&logical,&aliases)) {
                c->unknown[client]|=r->raw&mask; continue;
            }
            if(aliases&(aliases-1u)) c->ambiguous[client]|=r->raw&mask;
            if(!((rising|falling)&mask)) continue;
            c->event_sequence=tc_inc(c->event_sequence);
            c->last_source=source; c->last_index=index; c->last_aliases=aliases;
            c->last_pressed=(rising&mask) ? 1u : 0u;
            if(source==TC_NAV) {
                if(rising&mask) c->nav_held=(uint8_t)(c->nav_held|1u<<index);
                else c->nav_held=(uint8_t)(c->nav_held&~(1u<<index));
            }
            if(source==TC_FUNCTION && index<TC_FUNCTIONS) {
                if(rising&mask) c->fn_held=(uint16_t)(c->fn_held|1u<<index);
                else c->fn_held=(uint16_t)(c->fn_held&~(1u<<index));
                /* A chord shape held on a button ends: re-strike held pads without it. A running
                 * arpeggio takes the shape at once, on and off (1.7.0). */
                uint8_t act=c->buttons[index];
                if((falling&mask) && act>=TC_ACT_ADD7 && act<=TC_ACT_SUS4 && c->live_update) c->relayout=true;
                if(act>=TC_ACT_ADD7 && act<=TC_ACT_SUS4 && c->arp!=TC_ARP_OFF) c->reshape=true;
            }
            if(rising&mask) {
                c->presses[logical]=tc_inc(c->presses[logical]);
                if(!conflict) dispatch(c,e,source,index,now);
            } else c->releases[logical]=tc_inc(c->releases[logical]);
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
