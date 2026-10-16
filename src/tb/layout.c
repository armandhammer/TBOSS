/* SPDX-License-Identifier: GPL-3.0-or-later - pad geometry and the 96-pad degree layout.
 * Scale ids are an original replacement order; standard musical intervals.
 * Verified pad by pad against textbook scales/triads by scripts/check_notes.py. */
#include "layout.h"
#include "controls.h"
static const uint8_t scales[TC_SCALES][12]={
    {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10}, {0,2,3,5,7,9,10},
    {0,1,3,5,7,8,10}, {0,2,4,6,7,9,11}, {0,2,4,5,7,9,10},
    {0,1,3,5,6,8,10}, {0,2,3,5,7,8,11}, {0,2,3,5,7,9,11},
    {0,2,4,7,9}, {0,3,5,7,10}, {0,1,2,3,4,5,6,7,8,9,10,11}
};
static const uint8_t counts[TC_SCALES]={7,7,7,7,7,7,7,7,7,5,5,12};
static const char *const names[TC_SCALES]={"MAJOR","MINOR","DORIAN","PHRYGIAN","LYDIAN",
    "MIXOLYDIAN","LOCRIAN","HARM MINOR","MEL MINOR","MAJ PENT","MIN PENT","CHROMATIC"};
static tc_user_scale users[TC_USER_SCALES];
static uint8_t user_steps[TC_USER_SCALES][12], user_counts[TC_USER_SCALES];
static const char *const user_labels[TC_USER_SCALES]={"U1","U2","U3"};
bool tc_user_mask_ok(uint16_t mask) {
    uint32_t n=0;
    for(uint32_t b=0;b<12u;b++) n+=((uint32_t)mask>>b)&1u;
    return mask<4096u && (mask&1u) && n>=5u;
}
bool tc_set_user_scale(uint8_t slot, uint16_t mask, const char *name) {
    if(slot>=TC_USER_SCALES || (mask && !tc_user_mask_ok(mask))) return false;
    tc_user_scale u={mask,{0}};
    for(uint32_t i=0;mask && name && i<TC_USER_NAME && name[i];i++) {
        char ch=name[i]>='a' && name[i]<='z' ? (char)(name[i]-32) : name[i]; /* display font: capitals */
        u.name[i]=ch>=' ' && ch<='~' ? ch : ' ';
    }
    users[slot]=u; user_counts[slot]=0;
    for(uint32_t b=0;b<12u;b++) if(((uint32_t)mask>>b)&1u) user_steps[slot][user_counts[slot]++]=(uint8_t)b;
    return true;
}
const tc_user_scale *tc_user_scale_get(uint8_t slot) { return slot<TC_USER_SCALES ? &users[slot] : 0; }
bool tc_scale_defined(uint8_t scale) {
    return scale<TC_SCALES || (scale<TC_ALL_SCALES && users[scale-TC_SCALES].mask);
}
const char *tc_scale_name(uint8_t scale) {
    if(scale<TC_SCALES) return names[scale];
    if(scale>=TC_ALL_SCALES) return "?";
    const tc_user_scale *u=&users[scale-TC_SCALES];
    return u->mask && u->name[0] ? u->name : user_labels[scale-TC_SCALES];
}
/* Steps of a defined scale; an undefined id reads as the root alone. */
static const uint8_t *steps(uint8_t scale, uint32_t *n) {
    static const uint8_t root[1]={0};
    if(scale<TC_SCALES) { *n=counts[scale]; return scales[scale]; }
    if(tc_scale_defined(scale)) { *n=user_counts[scale-TC_SCALES]; return user_steps[scale-TC_SCALES]; }
    *n=1u; return root;
}
uint32_t tc_scale_size(uint8_t scale) { uint32_t n; (void)steps(scale,&n); return n; }
uint32_t tc_step_semitones(uint8_t scale, uint32_t d) {
    uint32_t n; const uint8_t *s=steps(scale,&n); return s[d%n];
}
bool tp_index(tp_position p, uint8_t *k) {
    if (!k || p.side > 1u || p.row > 3u || p.column > 11u) return false;
    *k = (uint8_t)(48u * p.side + 4u * p.column + p.row); return true;
}
bool tp_position_of(uint32_t k, tp_position *p) {
    if (!p || k >= 96u) return false;
    *p = (tp_position){(uint8_t)(k / 48u), (uint8_t)(k % 4u), (uint8_t)((k % 48u) / 4u)};
    return true;
}
bool tc_layout_ok(tp_layout c) {
    return c.tonic<12u && tc_scale_defined(c.mode) && c.melody_base<=127u && c.chord_base<=127u;
}
static uint32_t degree(tp_layout c, uint32_t d) {
    uint32_t n; const uint8_t *s=steps(c.mode,&n); return 12u*(d/n)+s[d%n];
}
bool tc_notes(tp_layout c, uint32_t k, tc_chord edit, tp_notes *out) {
    tp_position p;
    if(!out || !tc_layout_ok(c) || !tp_position_of(k,&p) || edit.lower>7u ||
       (edit.sus!=0u && edit.sus!=2u && edit.sus!=4u) || edit.add>7u) return false;
    tp_notes result={{0,0,0,0,0,0},0};
    if(p.side) {
        uint32_t pitch=c.melody_base+c.tonic+12u*(3u-p.row)+degree(c,p.column);
        if(pitch>127u) return false;
        result.notes[0]=(uint8_t)pitch; result.count=1;
    } else {
        /* Bottom row root position, then first and second inversion, top row
         * root position an octave up. Members: 0 root, 1 third (or sus), 2 fifth. */
        static const uint8_t rotation[4]={0,2,1,0};
        const uint32_t steps[3]={0u, edit.sus ? edit.sus-1u : 2u, 4u};
        uint32_t inv=rotation[p.row], up=p.row==0u ? 12u : 0u;
        for(uint32_t i=0;i<3u;i++) {
            uint32_t member=(i+inv)%3u;
            uint32_t pitch=c.chord_base+c.tonic+degree(c,p.column+steps[member])+(i+inv>=3u ? 12u : 0u)+up;
            if(edit.lower&(1u<<member)) { if(pitch<12u) return false; pitch-=12u; }
            if(pitch>127u) return false;
            result.notes[i]=(uint8_t)pitch;
        }
        result.count=3;
        /* Diatonic extensions above the root-position chord: 6th, 7th, 9th. */
        static const uint8_t extra[3]={5u,6u,8u};
        for(uint32_t x=0;x<3u;x++) if(edit.add&(1u<<x)) {
            uint32_t pitch=c.chord_base+c.tonic+degree(c,p.column+extra[x])+up;
            if(pitch>127u) return false;
            result.notes[result.count++]=(uint8_t)pitch;
        }
    }
    *out=result; return true;
}
bool tc_pad(tp_layout layout, uint32_t k, tp_notes *out) {
    return tc_notes(layout,k,tc_state.chord[k%48u],out);
}
