/* SPDX-License-Identifier: GPL-3.0-or-later - pad geometry and the 96-pad degree layout.
 * Scale ids are an original replacement order; standard musical intervals.
 * Verified pad by pad against textbook scales/triads by scripts/check_notes.py. */
#include "layout.h"
#include "controls.h"
#include <string.h>
#define TB_SCALE_LIBRARY_DATA
#include "scale_library.h"
static const uint8_t scales[TC_SCALES][12]={
    {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10}, {0,2,3,5,7,9,10},
    {0,1,3,5,7,8,10}, {0,2,4,6,7,9,11}, {0,2,4,5,7,9,10},
    {0,1,3,5,6,8,10}, {0,2,3,5,7,8,11}, {0,2,3,5,7,9,11},
    {0,2,4,7,9}, {0,3,5,7,10}, {0,1,2,3,4,5,6,7,8,9,10,11}
};
static const uint8_t counts[TC_SCALES]={7,7,7,7,7,7,7,7,7,5,5,12};
static const char *const names[TC_SCALES]={"MAJOR","MINOR","DORIAN","PHRYGIAN","LYDIAN",
    "MIXOLYDIAN","LOCRIAN","HARM MINOR","MEL MINOR","MAJ PENT","MIN PENT","CHROMATIC"};
/* More built-in scales (ids 24..36), the same list the web editor offers to start from. */
static const uint8_t extra[TC_EXTRA_SCALES][12]={
    {0,3,5,6,7,10}, {0,2,4,6,8,10}, {0,2,4,5,7,8,11}, {0,2,3,6,7,8,11},
    {0,1,4,5,7,8,10}, {0,1,4,5,7,8,11}, {0,2,3,5,6,8,9,11}, {0,1,3,4,6,7,9,10},
    {0,2,4,5,7,9,10,11}, {0,2,3,7,8}, {0,1,5,7,10}, {0,2,4,6,7,9,10}, {0,1,3,4,6,8,10}
};
static const uint8_t extra_counts[TC_EXTRA_SCALES]={6,6,7,7,7,7,8,8,8,5,5,7,7};
static const char *const extra_names[TC_EXTRA_SCALES]={"BLUES","WHOLE TONE","HARM MAJOR","HUNG MINOR",
    "PHRYG DOM","DOUBLE HARM","DIM W-H","DIM H-W","BEBOP DOM","HIRAJOSHI","IN SEN","LYDIAN DOM","ALTERED"};
static tc_user_scale users[TC_USER_SCALES];
static uint8_t user_steps[TC_USER_SCALES][12], user_counts[TC_USER_SCALES];
static const char *const user_labels[TC_USER_SCALES]={"U1","U2","U3","U4","U5","U6","U7","U8","U9","U10",
    "U11","U12"};
static bool is_user(uint8_t scale) { return scale>=TC_SCALES && scale<TC_EXTRA_FIRST; }
static bool is_library(uint8_t scale) { return scale>=TC_LIBRARY_FIRST && scale<TC_ALL_SCALES; }
uint8_t tc_family_mode(uint8_t family, uint32_t mode) {
    return family<TC_FAMILIES && mode<7u ? family_modes[family][mode] : family_modes[0][mode%7u];
}
const char *tc_family_name(uint8_t family) { return family<TC_FAMILIES ? family_names[family] : "?"; }
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
    return scale<TC_ALL_SCALES && (!is_user(scale) || users[scale-TC_SCALES].mask);
}
const char *tc_scale_name(uint8_t scale) {
    if(scale<TC_SCALES) return names[scale];
    if(scale>=TC_ALL_SCALES) return "?";
    if(is_library(scale)) return library_names[scale-TC_LIBRARY_FIRST];
    if(!is_user(scale)) return extra_names[scale-TC_EXTRA_FIRST];
    const tc_user_scale *u=&users[scale-TC_SCALES];
    return u->mask && u->name[0] ? u->name : user_labels[scale-TC_SCALES];
}
/* Menu order: classic 0..11, then ids 24.. (more built-in, then the library: one run of
 * ids), then the user slots last. */
#define TC_BUILT_IN_AFTER (TC_ALL_SCALES-TC_EXTRA_FIRST) /* ids 24.. */
uint8_t tc_scale_at(uint8_t position) {
    if(position<TC_SCALES) return position;
    if(position<TC_SCALES+TC_BUILT_IN_AFTER) return (uint8_t)(TC_EXTRA_FIRST+position-TC_SCALES);
    return position<TC_ALL_SCALES ? (uint8_t)(TC_SCALES+position-TC_SCALES-TC_BUILT_IN_AFTER) : 0u;
}
uint8_t tc_scale_position(uint8_t scale) {
    if(scale<TC_SCALES) return scale;
    if(is_user(scale)) return (uint8_t)(TC_SCALES+TC_BUILT_IN_AFTER+scale-TC_SCALES);
    return scale<TC_ALL_SCALES ? (uint8_t)(TC_SCALES+scale-TC_EXTRA_FIRST) : 0u;
}
/* Steps of a defined scale; an undefined id reads as the root alone. */
static const uint8_t *steps(uint8_t scale, uint32_t *n) {
    static const uint8_t root[1]={0};
    if(scale<TC_SCALES) { *n=counts[scale]; return scales[scale]; }
    if(is_library(scale)) { *n=library_counts[scale-TC_LIBRARY_FIRST]; return library_steps[scale-TC_LIBRARY_FIRST]; }
    if(scale<TC_ALL_SCALES && !is_user(scale)) { *n=extra_counts[scale-TC_EXTRA_FIRST]; return extra[scale-TC_EXTRA_FIRST]; }
    if(tc_scale_defined(scale)) { *n=user_counts[scale-TC_SCALES]; return user_steps[scale-TC_SCALES]; }
    *n=1u; return root;
}
uint32_t tc_scale_size(uint8_t scale) { uint32_t n; (void)steps(scale,&n); return n; }
uint32_t tc_step_semitones(uint8_t scale, uint32_t d) {
    uint32_t n; const uint8_t *s=steps(scale,&n); return s[d%n];
}
bool tc_in_scale(uint8_t scale, uint32_t semis) {
    uint32_t n; const uint8_t *s=steps(scale,&n);
    for(uint32_t i=0;i<n;i++) if(s[i]==semis%12u) return true;
    return false;
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
uint32_t tp_drum_grid(uint32_t k) { return ((k % 48u) / 4u) / 4u; }
bool tc_layout_ok(tp_layout c) {
    return c.tonic<12u && tc_scale_defined(c.mode) && c.melody_base<=127u && c.chord_base<=127u &&
           c.melody_layout<TP_MELODY_LAYOUTS && c.fold<=1u;
}
static uint32_t degree(tp_layout c, uint32_t d) {
    uint32_t n; const uint8_t *s=steps(c.mode,&n); return 12u*(d/n)+s[d%n];
}
static uint32_t tones(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]);
bool tp_live_pad(tp_layout c, uint32_t k) {
    return k>=48u && k<96u && c.melody_layout==TP_MELODY_LIVE && (k%48u)/4u>=TP_LIVE_COLUMN;
}
static void live_from_mask(uint8_t root, uint32_t classes, tc_live_chord *out) {
    tc_live_chord ch={root,0,{0}};
    for(uint32_t s=0;s<12u;s++) if((classes>>((root+s)%12u))&1u) ch.tones[ch.count++]=(uint8_t)s;
    *out=ch;
}
void tc_live_of_pad(tp_layout c, uint32_t k, tc_chord edit, tc_live_chord *out) {
    tp_position p={0u,3u,0u}; int32_t t[6];
    (void)tp_position_of(k%48u,&p);
    uint32_t count=tones(c,p,edit,0u,t), classes=0, root=(c.tonic+degree(c,p.column))%12u;
    for(uint32_t i=0;i<count;i++) classes|=1u<<((uint32_t)(t[i]+120)%12u);
    live_from_mask((uint8_t)root,classes|1u<<root,out);
}
bool tc_live_of_notes(const uint8_t held[128], tc_live_chord *out) {
    uint32_t classes=0, n=0, root=128u;
    for(uint32_t p=0;p<128u;p++) if(held[p]) { if(root==128u) root=p%12u; classes|=1u<<(p%12u); }
    for(uint32_t b=0;b<12u;b++) n+=(classes>>b)&1u;
    if(n<2u) return false;
    live_from_mask((uint8_t)root,classes,out); return true;
}
/* Row: the chord's tones from its root (in the melody octave of the key), climbing on
 * into the next octave; rows an octave apart. Folded down into MIDI range. */
uint8_t tc_live_note(tp_layout c, uint32_t k, const tc_live_chord *ch) {
    tp_position p={1u,3u,0u}; (void)tp_position_of(k,&p);
    uint32_t n=ch->count ? ch->count : 1u, j=p.column>=TP_LIVE_COLUMN ? p.column-TP_LIVE_COLUMN : 0u;
    uint32_t pitch=c.melody_base+c.tonic+(ch->root+12u-c.tonic%12u)%12u+12u*(3u-p.row)+ch->tones[j%n]+12u*(j/n);
    while(pitch>127u) pitch-=12u;
    return (uint8_t)pitch;
}
static bool melody(tp_layout c, tp_position p, tp_notes *out) {
    uint32_t pitch, up=3u-p.row, x=p.column;
    switch(c.melody_layout) {
    case TP_MELODY_LIVE: {
        if(x<TP_LIVE_COLUMN) { pitch=c.melody_base+c.tonic+12u*up+degree(c,x); break; }
        /* Pure layout (no live state): the key's first chord. tc_pad plays the live chord. */
        tc_live_chord first; const tc_chord none={0,0,0};
        tc_live_of_pad(c,0u,none,&first);
        uint8_t k; (void)tp_index(p,&k);
        pitch=tc_live_note(c,k,&first);
        break;
    }
    case TP_MELODY_CHROMATIC: pitch=c.melody_base+c.tonic+x+12u*up; break;
    case TP_MELODY_FOURTHS: pitch=c.melody_base+c.tonic+x+5u*up; break;
    case TP_MELODY_DRUMS: {
        /* Three 4x4 pads, each numbered from its bottom-left: 4*row-from-bottom + column. */
        uint32_t n=4u*up+x%4u, grid=x/4u;
        pitch=grid==2u ? TP_DRUM_BASE+n : c.melody_base+c.tonic+degree(c,n);
        if(grid==1u) { if(pitch<12u) return false; pitch-=12u; }
        break;
    }
    default: pitch=c.melody_base+c.tonic+12u*up+degree(c,x); break;
    }
    if(pitch>127u) return false;
    memset(out,0,sizeof *out); out->notes[0]=(uint8_t)pitch; out->count=1; return true;
}
/* Triad members of the inversion ladder step n (0 root position, 1 first inversion ...,
 * 3 root position an octave up), with the pad's edits; then its extensions. As signed
 * pitches, unchecked. Returns the count; the triad comes first, low to high. */
static void sort_pitches(int32_t *t, uint32_t count) {
    for(uint32_t i=1;i<count;i++)
        for(uint32_t j=i;j && t[j-1u]>t[j];j--) { int32_t x=t[j]; t[j]=t[j-1u]; t[j-1u]=x; }
}
/* Fold: the root-position chord and its extensions moved into one octave, G# to G above
 * chord_base's C (whatever the key), lowered members then an octave down, each pitch once;
 * ladder step n raises the lowest note an octave, n times. Low to high. */
static uint32_t folded(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]) {
    static const uint8_t extension[3]={5u,6u,8u};
    const uint32_t member_steps[3]={0u, edit.sus ? edit.sus-1u : 2u, 4u};
    int32_t from=(int32_t)c.chord_base+8, f[6]; uint32_t m=0, count=0;
    for(uint32_t i=0;i<6u;i++) {
        bool member=i<3u;
        if(!member && !(edit.add&(1u<<(i-3u)))) continue;
        int32_t pitch=(int32_t)(c.chord_base+c.tonic+degree(c,p.column+(member ? member_steps[i] : extension[i-3u])));
        pitch=from+((pitch-from)%12+12)%12;
        if(member && (edit.lower&(1u<<i))) pitch-=12;
        f[m++]=pitch;
    }
    sort_pitches(f,m);
    for(uint32_t i=0;i<m;i++) if(!count || t[count-1u]!=f[i]) t[count++]=f[i];
    for(uint32_t s=0;s<n;s++) { t[0]+=12; sort_pitches(t,count); }
    return count;
}
static uint32_t tones(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]);
/* Ladder step n, also below 0: a step a whole round higher (3 for the plain chords, one
 * per note when folded), an octave down. */
static uint32_t ladder(tp_layout c, tp_position p, tc_chord edit, int32_t n, int32_t t[6]) {
    if(n>=0) return tones(c,p,edit,(uint32_t)n,t);
    int32_t period=c.fold ? (int32_t)tones(c,p,edit,0u,t) : 3, rounds=(-n+period-1)/period;
    uint32_t count=tones(c,p,edit,(uint32_t)(n+rounds*period),t);
    for(uint32_t i=0;i<count;i++) t[i]-=12*rounds;
    return count;
}
static uint32_t tones(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]) {
    if(c.fold) return folded(c,p,edit,n,t);
    const uint32_t member_steps[3]={0u, edit.sus ? edit.sus-1u : 2u, 4u};
    uint32_t inv=n%3u, up=12u*(n/3u), count=0;
    for(uint32_t i=0;i<3u;i++) {
        uint32_t member=(i+inv)%3u;
        int32_t pitch=(int32_t)(c.chord_base+c.tonic+degree(c,p.column+member_steps[member])+(i+inv>=3u ? 12u : 0u)+up);
        if(edit.lower&(1u<<member)) pitch-=12;
        t[count++]=pitch;
    }
    /* Diatonic extensions above the root-position chord: 6th, 7th, 9th. */
    static const uint8_t extension[3]={5u,6u,8u};
    for(uint32_t x=0;x<3u;x++) if(edit.add&(1u<<x)) t[count++]=(int32_t)(c.chord_base+c.tonic+degree(c,p.column+extension[x])+up);
    return count;
}
/* Guitar voicing: six strings in standard tuning (E2 A2 D3 G3 B3 E4), a capo at fret `capo`,
 * the hand in a five-fret window that moves up the neck with the row (ladder step n: open
 * position, then 3, 5 and 7 frets higher). Each string plays the lowest chord note it has
 * in the window; strings below the lowest one that can play the root are muted, so the
 * chord stands on its root (open C is x32010, G 320003, D xx0232). */
static uint32_t guitar(uint32_t root, const int32_t *t, uint32_t count, uint32_t n, uint32_t capo, int32_t v[6]) {
    static const uint8_t open[6]={40u,45u,50u,55u,59u,64u}, position[4]={0u,3u,5u,7u};
    uint32_t classes=0, from=capo+position[n%4u], m=0;
    for(uint32_t i=0;i<count;i++) classes|=1u<<((uint32_t)(t[i]+120)%12u);
    int32_t fret[6]; uint32_t bass=6u;
    for(uint32_t s=0;s<6u;s++) {
        fret[s]=-1;
        for(uint32_t f=from;f<from+5u && fret[s]<0;f++) if((classes>>((open[s]+f)%12u))&1u) fret[s]=(int32_t)f;
        for(uint32_t f=from;f<from+5u && bass==6u;f++) if((open[s]+f)%12u==root) { bass=s; fret[s]=(int32_t)f; }
    }
    for(uint32_t s=bass==6u ? 0u : bass;s<6u;s++) if(fret[s]>=0) v[m++]=(int32_t)open[s]+fret[s];
    return m;
}
bool tc_notes_panel(tp_layout c, uint32_t k, tc_chord edit, tc_panel panel, tp_notes *out) {
    tp_position p;
    if(!out || !tc_layout_ok(c) || !tp_position_of(k,&p) || edit.lower>7u ||
       (edit.sus!=0u && edit.sus!=2u && edit.sus!=4u) || edit.add>7u ||
       panel.voicing>=TC_VOICINGS || panel.spread>=TC_SPREADS || panel.octaves>0x7Fu ||
       panel.capo>=TC_CAPOS || panel.lead>TC_LEAD_ZERO+12u) return false;
    if(p.side) return melody(c,p,out);
    /* Rows: bottom root position, then first and second inversion, top root position an
     * octave up (ladder steps 0..3 from the bottom). The unshaped chord must fit. Voice
     * leading picks the step itself, the voicing's inversions included. */
    int32_t n=panel.lead ? (int32_t)panel.lead-TC_LEAD_ZERO : 3-(int32_t)p.row;
    int32_t shift=panel.lead ? 0 : (int32_t)panel.voicing;
    int32_t t[6];
    uint32_t count=ladder(c,p,edit,n,t);
    for(uint32_t i=0;i<count;i++) if(t[i]<0 || t[i]>127) return false;
    tp_notes result; memset(&result,0,sizeof result);
    bool neutral=panel.voicing==0u && panel.spread==0u && (panel.octaves==TC_OCTAVES_DEFAULT || !panel.octaves);
    if(neutral) {
        for(uint32_t i=0;i<count;i++) result.notes[i]=(uint8_t)t[i];
        result.count=(uint8_t)count; *out=result; return true;
    }
    /* Voicing: further up the ladder, or spread like a guitar chord. */
    int32_t v[TP_MAX_NOTES]; uint32_t m=0;
    if(panel.voicing && panel.voicing<TC_VOICING_GUITAR) {
        count=ladder(c,p,edit,n+shift,t);
        for(uint32_t i=0;i<count;i++) v[m++]=t[i];
    } else if(panel.voicing==TC_VOICING_GUITAR) {
        m=guitar((c.tonic+degree(c,p.column))%12u,t,count,(uint32_t)n,panel.capo,v);
    } else for(uint32_t i=0;i<count;i++) v[m++]=t[i];
    /* Spread: the chord's lowest root down 1..3 octaves. */
    if(panel.spread) {
        uint32_t root=(c.tonic+degree(c,p.column))%12u, low=m;
        for(uint32_t i=0;i<m;i++) if(((uint32_t)(v[i]+120))%12u==root && (low==m || v[i]<v[low])) low=i;
        if(low<m) v[low]-=12*(int32_t)panel.spread;
    }
    /* Octaves: a copy for every chosen octave, low to high, each pitch once, at most 16. */
    uint32_t octaves=panel.octaves ? panel.octaves : TC_OCTAVES_DEFAULT;
    bool present[128]; memset(present,0,sizeof present);
    for(uint32_t o=0;o<7u;o++) if(octaves&(1u<<o))
        for(uint32_t i=0;i<m;i++) {
            int32_t pitch=v[i]+12*((int32_t)o-3);
            if(pitch>=0 && pitch<=127) present[pitch]=true;
        }
    for(uint32_t pitch=0;pitch<128u && result.count<TP_MAX_NOTES;pitch++)
        if(present[pitch]) result.notes[result.count++]=(uint8_t)pitch;
    *out=result; return true;
}
bool tc_notes(tp_layout c, uint32_t k, tc_chord edit, tp_notes *out) {
    const tc_panel neutral=TC_PANEL_NEUTRAL;
    return tc_notes_panel(c,k,edit,neutral,out);
}
void tc_live_current(tp_layout layout, tc_live_chord *out) {
    if(tc_state.live_from_midi && tc_state.live_midi.count) *out=tc_state.live_midi;
    else tc_live_of_pad(layout,tc_state.live_pad%48u,tc_state.chord[tc_state.live_pad%48u],out);
}
/* How far apart two chords are: every note's distance to the nearest note of the other
 * chord, both ways. */
static uint32_t movement(const int32_t *t, uint32_t count, const tp_notes *prev) {
    uint32_t sum=0;
    for(uint32_t i=0;i<count;i++) {
        uint32_t best=UINT32_MAX;
        for(uint32_t j=0;j<prev->count;j++) {
            uint32_t d=(uint32_t)(t[i]>prev->notes[j] ? t[i]-prev->notes[j] : prev->notes[j]-t[i]);
            if(d<best) best=d;
        }
        sum+=best;
    }
    for(uint32_t j=0;j<prev->count;j++) {
        uint32_t best=UINT32_MAX;
        for(uint32_t i=0;i<count;i++) {
            uint32_t d=(uint32_t)(t[i]>prev->notes[j] ? t[i]-prev->notes[j] : prev->notes[j]-t[i]);
            if(d<best) best=d;
        }
        sum+=best;
    }
    return sum;
}
static int32_t centre(const int32_t *t, uint32_t count) {
    int32_t sum=0;
    for(uint32_t i=0;i<count;i++) sum+=t[i];
    return count ? sum/(int32_t)count : 0;
}
/* Candidates: two ladder steps either side of home (the row's step plus voicing I..III), so
 * a higher row still plays higher;
 * cost = twice the movement from prev plus the distance of the chord's centre from home's,
 * so a progression flows smoothly yet stays near where its pads are. Ties: nearer home.
 * None inside MIDI range (high chords with voicing III): the row's own step, which every
 * accepted layout fits, so the pad still plays. */
int32_t tc_lead_step(tp_layout c, uint32_t k, tc_chord edit, uint32_t voicing, const tp_notes *prev) {
    tp_position p={0u,3u,0u};
    (void)tp_position_of(k%48u,&p);
    int32_t home=3-(int32_t)p.row+(int32_t)(voicing<TC_VOICING_GUITAR ? voicing : 0u), best=home;
    uint32_t best_cost=UINT32_MAX;
    if(!prev || !prev->count) return home;
    int32_t t[6];
    int32_t middle=centre(t,ladder(c,p,edit,home,t));
    for(int32_t n=home-2;n<=home+2;n++) {
        uint32_t count=ladder(c,p,edit,n,t);
        bool fits=count>0u;
        for(uint32_t i=0;i<count;i++) fits=fits && t[i]>=0 && t[i]<=127;
        if(!fits) continue;
        int32_t off=centre(t,count)-middle;
        uint32_t cost=2u*movement(t,count,prev)+(uint32_t)(off<0 ? -off : off);
        int32_t away=n>home ? n-home : home-n, best_away=best>home ? best-home : home-best;
        if(cost<best_cost || (cost==best_cost && away<best_away)) { best=n; best_cost=cost; }
    }
    return best_cost==UINT32_MAX ? 3-(int32_t)p.row : best;
}
static tc_panel live_panel(tp_layout layout, uint32_t k) {
    tc_panel panel={tc_state.chord_voicing,tc_state.chord_spread,tc_state.chord_octaves,tc_state.capo,0u};
    if(tc_state.voice_lead && k<48u && panel.voicing!=TC_VOICING_GUITAR && tc_state.lead.count)
        panel.lead=(uint8_t)(TC_LEAD_ZERO+tc_lead_step(layout,k,tc_state.chord[k%48u],panel.voicing,&tc_state.lead));
    return panel;
}
bool tc_pad(tp_layout layout, uint32_t k, tp_notes *out) {
    if(out && tc_layout_ok(layout) && tp_live_pad(layout,k)) {
        tc_live_chord ch; tc_live_current(layout,&ch);
        memset(out,0,sizeof *out); out->notes[0]=tc_live_note(layout,k,&ch); out->count=1; return true;
    }
    return tc_notes_panel(layout,k,tc_state.chord[k%48u],live_panel(layout,k),out);
}
bool tc_pad_core(tp_layout layout, uint32_t k, tp_notes *out) {
    if(!out || k>=48u || !tc_layout_ok(layout)) return false;
    tc_panel panel=live_panel(layout,k);
    tc_panel core={panel.voicing==TC_VOICING_GUITAR ? 0u : panel.voicing,0u,TC_OCTAVES_DEFAULT,0u,panel.lead};
    return tc_notes_panel(layout,k,tc_state.chord[k],core,out);
}
