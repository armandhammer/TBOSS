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
/* A name for the display: up to 12 printable characters in capitals, others become spaces. */
static void clean_name(const char *name, char out[TC_USER_NAME + 1u]) {
    memset(out,0,TC_USER_NAME+1u);
    for(uint32_t i=0;name && i<TC_USER_NAME && name[i];i++) {
        char ch=name[i]>='a' && name[i]<='z' ? (char)(name[i]-32) : name[i]; /* display font: capitals */
        out[i]=ch>=' ' && ch<='~' ? ch : ' ';
    }
}
bool tc_set_user_scale(uint8_t slot, uint16_t mask, const char *name) {
    if(slot>=TC_USER_SCALES || (mask && !tc_user_mask_ok(mask))) return false;
    tc_user_scale u={mask,{0}};
    if(mask) clean_name(name,u.name);
    users[slot]=u; user_counts[slot]=0;
    for(uint32_t b=0;b<12u;b++) if(((uint32_t)mask>>b)&1u) user_steps[slot][user_counts[slot]++]=(uint8_t)b;
    return true;
}
const tc_user_scale *tc_user_scale_get(uint8_t slot) { return slot<TC_USER_SCALES ? &users[slot] : 0; }
static tc_chord_set sets[TC_CHORD_SETS];
static uint32_t bit_count(uint32_t x) { uint32_t n=0; for(;x;x&=x-1u) n++; return n; }
bool tc_set_chord_ok(tc_set_chord ch) {
    if(!ch.mask) return ch.root==0u;
    return ch.mask<4096u && (ch.mask&1u) && bit_count(ch.mask)<=TC_SET_TONES && ch.root<TC_SET_ROOTS;
}
bool tc_chord_set_put(uint8_t slot, const tc_chord_set *set) {
    if(slot>=TC_CHORD_SETS || !set) return false;
    tc_chord_set t; memset(&t,0,sizeof t);
    bool any=false;
    for(uint32_t i=0;i<12u;i++) {
        if(!tc_set_chord_ok(set->column[i])) return false;
        t.column[i]=set->column[i]; any=any || set->column[i].mask;
    }
    if(any) clean_name(set->name,t.name);
    sets[slot]=t; return true;
}
const tc_chord_set *tc_chord_set_get(uint8_t slot) { return slot<TC_CHORD_SETS ? &sets[slot] : 0; }
bool tc_chord_set_defined(uint8_t slot) {
    if(slot>=TC_CHORD_SETS) return false;
    for(uint32_t i=0;i<12u;i++) if(sets[slot].column[i].mask) return true;
    return false;
}
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
static uint16_t scale_mask(uint8_t scale) {
    uint32_t n, mask=0; const uint8_t *s=steps(scale,&n);
    for(uint32_t i=0;i<n;i++) mask|=1u<<(s[i]%12u);
    return (uint16_t)mask;
}
bool tc_chords_ok(uint8_t chords) {
    return !chords || chords==TC_CHORDS_FULL || chords==TC_CHORDS_GRID || (chords<=TC_CHORD_SETS && tc_chord_set_defined((uint8_t)(chords-1u)));
}
/* ---- Full chord layout (1.3.0) ----
 * The original's records on the chip: from FULL_FIRST, 70 scales one after another, each as 12
 * records (one per key, in full_key_slot order) of 80 entries. An entry is 16 ASCII digits, the
 * chord's MIDI notes in pairs ("00" ends them, at most seven) with the scale step of its run
 * (1..7) as the last pair, then a 16-byte name. Entries 0..47 are the chord pads, top to bottom
 * down each column; 48 onwards the original's right half (48: the key's root, step 1). */
#define FULL_FIRST 0x70000u
#define FULL_RECORD 2560u
#define FULL_ENTRY 32u
#define FULL_RECORDS 70u
#define FULL_PADS 48u
#define FULL_CACHE 4u
/* Each record's scale and the 7-note mode whose steps number its runs (the same scale but for
 * the pentatonic, Japanese, blues and bebop ones at the end); bit n = n semitones above the
 * root. In chip order, 7 modes to a family: double harmonic, harmonic major, harmonic minor,
 * major, melodic minor from its 5th mode, melodic minor, Neapolitan major, Neapolitan minor,
 * then the pentatonic and Japanese scales, and blues, Lydian dominant pentatonic and bebop. */
static const uint16_t full_scales[FULL_RECORDS][2]={
    {0x9B3,0x9B3},{0xCD9,0xCD9},{0x39B,0x39B},{0x9CD,0x9CD},{0x673,0x673},{0xB39,0xB39},{0x367,0x367},
    {0x9B5,0x9B5},{0x66D,0x66D},{0x59B,0x59B},{0xACD,0xACD},{0x6B3,0x6B3},{0xB59,0xB59},{0x36B,0x36B},
    {0x9AD,0x9AD},{0x66B,0x66B},{0xB35,0xB35},{0x6CD,0x6CD},{0x5B3,0x5B3},{0xAD9,0xAD9},{0x35B,0x35B},
    {0xAB5,0xAB5},{0x6AD,0x6AD},{0x5AB,0x5AB},{0xAD5,0xAD5},{0x6B5,0x6B5},{0x5AD,0x5AD},{0x56B,0x56B},
    {0x5B5,0x5B5},{0x56D,0x56D},{0x55B,0x55B},{0xAAD,0xAAD},{0x6AB,0x6AB},{0xB55,0xB55},{0x6D5,0x6D5},
    {0xAAD,0xAAD},{0x6AB,0x6AB},{0xB55,0xB55},{0x6D5,0x6D5},{0x5B5,0x5B5},{0x56D,0x56D},{0x55B,0x55B},
    {0xAAB,0xAAB},{0xD55,0xD55},{0x755,0x755},{0x5D5,0x5D5},{0x575,0x575},{0x55D,0x55D},{0x557,0x557},
    {0x9AB,0x9AB},{0xCD5,0xCD5},{0x735,0x735},{0x5CD,0x5CD},{0x573,0x573},{0xAB9,0xAB9},{0x357,0x357},
    {0x295,0xAB5},{0x4A5,0x6AD},{0x529,0x5AB},{0x2A5,0x6B5},{0x4A9,0x5AD},{0x463,0x56B},{0x8D1,0xAD5},
    {0x18D,0x5AD},{0x4A3,0x5B3},{0x4E9,0x5AD},{0x655,0x6D5},{0xEB5,0x6B5},{0xBB5,0xAB5},{0x6BD,0x6AD}};
static const uint8_t full_key_slot[12]={4,6,5,8,7,9,11,10,1,0,3,2}; /* C..B */
/* Families in search order (a scale found in two plays the first): major, melodic minor,
 * harmonic minor, harmonic major, double harmonic, Neapolitan major and minor; the last three
 * are never a parent (the melodic minor modes from the 5th repeat melodic minor's scales). */
static const uint8_t full_search[10]={3,5,2,1,0,6,7,8,9,4};
#define FULL_PARENTS 7u
/* Chord types for a built layout (the ones the original's records use most), the usual ones
 * first: triads, add2, sixths, 6/9, sevenths, ninths, suspended, then the altered and added-note
 * ones, a fifth alone last. Bit n = n semitones above the root. */
static const uint16_t full_types[]={0x091,0x089,0x049,0x111, 0x095,0x08D, 0x291,0x289, 0x295,0x28D,
    0x891,0x491,0x489,0x889,0x449,0x249,0x911,0x511,0x451,0x509,0x851, 0x895,0x495,0x48D,
    0x085,0x0A1,0x4A1, 0x051,0x251,0x0B1,0x0A9,0x189,0x311,0x8B1,0x493,0x499,0x48B,0x459,0x2B1,
    0x08B,0x093,0x131,0x081};
#define FULL_TYPES (sizeof full_types/sizeof full_types[0])
typedef struct { uint16_t mask; uint8_t root, count, notes[TC_SET_TONES]; } full_chord; /* root: semitones above the key's */
typedef struct { uint8_t step, count, notes[7]; } full_entry;
static struct { uint16_t scale; uint8_t key; bool original; uint32_t used; full_chord pad[FULL_PADS]; } full_cache[FULL_CACHE];
static uint32_t full_clock;
static tc_read_fn full_reader;
void tc_full_reader(tc_read_fn read) { full_reader=read; memset(full_cache,0,sizeof full_cache); }
static void sort_notes(uint8_t *v, uint32_t n) {
    for(uint32_t i=1;i<n;i++) for(uint32_t j=i;j && v[j-1u]>v[j];j--) { uint8_t x=v[j]; v[j]=v[j-1u]; v[j-1u]=x; }
}
/* A built voicing, as the original's mostly are: every tone in the octave G#3..G4, and no half
 * step at the bottom (its lower note goes an octave up). */
static uint8_t full_voice(uint32_t root, uint16_t mask, uint8_t notes[TC_SET_TONES]) {
    uint8_t n=0;
    for(uint32_t b=0;b<12u && n<TC_SET_TONES;b++) if(((uint32_t)mask>>b)&1u) notes[n++]=(uint8_t)(56u+(root+b+4u)%12u);
    sort_notes(notes,n);
    for(uint32_t i=0;n>1u && i<n && (uint32_t)notes[1]-notes[0]==1u;i++) { notes[0]=(uint8_t)(notes[0]+12u); sort_notes(notes,n); }
    return n;
}
static bool full_parse(const uint8_t d[16], full_entry *e) {
    bool ended=false;
    for(uint32_t i=0;i<16u;i++) if(d[i]<'0' || d[i]>'9') return false;
    e->count=0;
    for(uint32_t i=0;i<14u;i+=2u) {
        uint32_t v=10u*(uint32_t)(d[i]-'0')+(uint32_t)(d[i+1u]-'0');
        if(!v) { ended=true; continue; }
        if(ended || v<24u || v>96u) return false;
        e->notes[e->count++]=(uint8_t)v;
    }
    e->step=(uint8_t)(10u*(uint32_t)(d[14]-'0')+(uint32_t)(d[15]-'0'));
    sort_notes(e->notes,e->count);
    for(uint32_t i=1;i<e->count;i++) if(e->notes[i]==e->notes[i-1u]) return false;
    return e->count && e->step>=1u && e->step<=7u;
}
static bool full_read(uint32_t record, uint32_t key, uint32_t entry, full_entry *e) {
    uint8_t d[16];
    uint32_t at=FULL_FIRST+FULL_RECORD*(12u*record+full_key_slot[key%12u])+FULL_ENTRY*entry;
    return full_reader && full_reader(at,d,16u) && full_parse(d,e);
}
/* The record for a scale: its own, else a parent mode's; FULL_RECORDS if none. */
static uint32_t full_record(uint16_t scale) {
    for(uint32_t f=0;f<10u;f++) for(uint32_t m=0;m<7u;m++)
        if(full_scales[7u*full_search[f]+m][0]==scale) return 7u*full_search[f]+m;
    for(uint32_t f=0;f<FULL_PARENTS;f++) for(uint32_t m=0;m<7u;m++)
        if(!(scale&~full_scales[7u*full_search[f]+m][0])) return 7u*full_search[f]+m;
    return FULL_RECORDS;
}
static uint16_t full_classes(const full_entry *e, uint32_t root) {
    uint32_t mask=0;
    for(uint32_t i=0;i<e->count;i++) mask|=1u<<((e->notes[i]+120u-root)%12u);
    return (uint16_t)mask;
}
static bool full_from_chip(uint16_t scale, uint32_t key, full_chord *out) {
    uint32_t record=full_record(scale), runs[7], n=0; /* runs: each scale step's semitones */
    if(record>=FULL_RECORDS) return false;
    for(uint32_t b=0;b<12u && n<7u;b++) if(((uint32_t)full_scales[record][1]>>b)&1u) runs[n++]=b;
    if(n<7u) return false;
    full_entry e;
    /* The record is there and in this key: its right half starts on the key's root. */
    if(!full_read(record,key,FULL_PADS,&e) || e.step!=1u || e.notes[0]%12u!=key) return false;
    /* Each entry against the same entry in other keys, far apart first (slips were copied to
     * neighboring keys, never as far as a tritone). The original's chords never leave the mode,
     * so one that does is a slip and has no say. This key's agreeing with the key a tritone away
     * settles it; otherwise the first chord three keys agree on (pitch classes from its run's
     * root), or the most common one. This key's own voicing when it is that chord, else the
     * chord voiced as built. */
    static const uint8_t order[12]={0,6,3,9,1,7,4,10,2,8,5,11};
    for(uint32_t i=0;i<FULL_PADS;i++) {
        full_entry seen[12]; uint16_t classes[12]; uint8_t votes[12]; bool ok[12]; uint32_t winner=12u;
        for(uint32_t j=0;j<12u && winner==12u;j++) {
            uint32_t k=(key+order[j])%12u, m=0;
            ok[j]=full_read(record,k,i,&seen[j]) && seen[j].count>=2u && seen[j].count<=TC_SET_TONES &&
                  !(full_classes(&seen[j],k)&~full_scales[record][1]);
            classes[j]=ok[j] ? full_classes(&seen[j],k+runs[seen[j].step-1u]) : (uint16_t)0u;
            votes[j]=0;
            if(!ok[j]) continue;
            while(m<j && !(ok[m] && votes[m] && seen[m].step==seen[j].step && classes[m]==classes[j])) m++;
            votes[m]++;
            if(votes[m]>=3u || (j==1u && !m)) winner=m;
        }
        for(uint32_t j=0;j<12u && winner==12u;j++) if(ok[j] && votes[j]) {
            winner=j;
            for(uint32_t m=j+1u;m<12u;m++) if(ok[m] && votes[m]>votes[winner]) winner=m;
        }
        full_chord *c=&out[i]; memset(c,0,sizeof *c);
        if(winner==12u) continue; /* nothing readable: a silent pad */
        c->mask=classes[winner]; c->root=(uint8_t)runs[seen[winner].step-1u];
        if(ok[0] && seen[0].step==seen[winner].step && classes[0]==classes[winner]) { c->count=seen[0].count; memcpy(c->notes,seen[0].notes,c->count); }
        else c->count=full_voice((key+c->root)%12u,c->mask,c->notes);
    }
    return true;
}
/* Every scale step gets the next chord type that fits the scale, in turns, until the 48 pads
 * are taken; then each step's chords in a run, in scale order. A scale with fewer chords than
 * pads plays them again after the last, an inversion higher, then two (then as at first). */
static void full_build(uint16_t scale, uint32_t key, full_chord *out) {
    uint32_t semis[12], n=0, count[12]={0}, at[12]={0}, total=0;
    for(uint32_t b=0;b<12u;b++) if(((uint32_t)scale>>b)&1u) semis[n++]=b;
    #define FITS(s,t) (!(((((uint32_t)full_types[t]<<(s))|((uint32_t)full_types[t]>>(12u-(s))))&0xFFFu)&~(uint32_t)scale))
    for(bool more=true;more && total<FULL_PADS;) {
        more=false;
        for(uint32_t d=0;d<n && total<FULL_PADS;d++) {
            while(at[d]<FULL_TYPES && !FITS(semis[d],at[d])) at[d]++;
            if(at[d]<FULL_TYPES) { at[d]++; count[d]++; total++; more=true; }
        }
    }
    memset(out,0,FULL_PADS*sizeof *out);
    uint32_t i=0;
    for(uint32_t d=0;d<n;d++)
        for(uint32_t t=0,taken=0;t<FULL_TYPES && taken<count[d];t++) if(FITS(semis[d],t)) {
            out[i].mask=full_types[t]; out[i].root=(uint8_t)semis[d];
            out[i].count=full_voice((key+semis[d])%12u,full_types[t],out[i].notes);
            i++; taken++;
        }
    for(uint32_t j=i;i && j<FULL_PADS;j++) {
        out[j]=out[j%i];
        for(uint32_t r=0;r<(j/i)%3u;r++) { out[j].notes[0]=(uint8_t)(out[j].notes[0]+12u); sort_notes(out[j].notes,out[j].count); }
    }
    #undef FITS
}
/* The layouts last used stay; a new one replaces the one unused longest. */
static const full_chord *full_pads(uint16_t scale, uint32_t key, bool *original) {
    uint32_t slot=FULL_CACHE, oldest=0;
    for(uint32_t i=0;i<FULL_CACHE && slot==FULL_CACHE;i++) {
        if(full_cache[i].scale==scale && full_cache[i].key==key) slot=i;
        else if(full_cache[i].used<full_cache[oldest].used) oldest=i;
    }
    if(slot==FULL_CACHE) {
        slot=oldest; full_cache[slot].scale=scale; full_cache[slot].key=(uint8_t)key;
        full_cache[slot].original=full_from_chip(scale,key,full_cache[slot].pad);
        if(!full_cache[slot].original) { /* built, as the original would: on the parent mode's notes */
            uint32_t record=full_record(scale);
            full_build(record<FULL_RECORDS ? full_scales[record][1] : scale,key,full_cache[slot].pad);
        }
    }
    full_cache[slot].used=++full_clock;
    if(original) *original=full_cache[slot].original;
    return full_cache[slot].pad;
}
void tc_full_prepare(tp_layout c) {
    if(c.chords==TC_CHORDS_FULL && tc_scale_defined(c.mode)) (void)full_pads(scale_mask(c.mode),c.tonic%12u,NULL);
}
bool tc_full_original(tp_layout c) {
    bool original=false;
    if(c.chords==TC_CHORDS_FULL && tc_scale_defined(c.mode)) (void)full_pads(scale_mask(c.mode),c.tonic%12u,&original);
    return original;
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
           c.melody_layout<TP_MELODY_LAYOUTS && c.fold<=1u && tc_chords_ok(c.chords);
}
static uint32_t degree(tp_layout c, uint32_t d) {
    uint32_t n; const uint8_t *s=steps(c.mode,&n); return 12u*(d/n)+s[d%n];
}
/* The chord set's chord on chord-pad position p; NULL while the pads play the scale's chords. */
static const tc_set_chord *set_chord(tp_layout c, tp_position p) {
    return c.chords && c.chords<=TC_CHORD_SETS ? &sets[c.chords-1u].column[p.column%12u] : NULL;
}
/* Its tones as semitones above its root, low to high; returns how many. */
static uint32_t set_tones(const tc_set_chord *s, uint32_t semis[12]) {
    uint32_t n=0;
    for(uint32_t b=0;b<12u;b++) if(((uint32_t)s->mask>>b)&1u) semis[n++]=b;
    return n;
}
/* The Full layout's chord on chord-pad position p; NULL in the other layouts. */
static const full_chord *full_chord_at(tp_layout c, tp_position p) {
    if(c.chords!=TC_CHORDS_FULL) return NULL;
    return &full_pads(scale_mask(c.mode),c.tonic%12u,NULL)[(4u*p.column+p.row)%FULL_PADS];
}
/* Semitones from the key's root up to the chord's root. */
static uint32_t chord_root(tp_layout c, tp_position p) {
    const full_chord *f=full_chord_at(c,p);
    if(f) return f->root;
    const tc_set_chord *s=set_chord(c,p);
    return s ? s->root : degree(c,p.column);
}
uint8_t tc_pad_root(tp_layout c, uint32_t k) {
    tp_position p={0u,3u,0u};
    (void)tp_position_of(k%48u,&p);
    return (uint8_t)((c.tonic+chord_root(c,p))%12u);
}
bool tc_pad_empty(tp_layout c, uint32_t k) {
    tp_position p={0u,3u,0u};
    (void)tp_position_of(k%48u,&p);
    const full_chord *f=full_chord_at(c,p);
    if(f) return !f->count;
    const tc_set_chord *s=set_chord(c,p);
    return s && !s->mask;
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
    uint32_t count=tones(c,p,edit,0u,t), classes=0, root=(c.tonic+chord_root(c,p))%12u;
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
    const tc_set_chord *set=set_chord(c,p);
    uint32_t semis[12], in_set=set ? set_tones(set,semis) : 0u;
    int32_t from=(int32_t)c.chord_base+8, f[6]; uint32_t m=0, count=0;
    for(uint32_t i=0;i<6u;i++) {
        bool member=i<3u;
        int32_t pitch;
        if(set) { /* a chord set's chord: its own tones */
            if(i>=in_set) break;
            pitch=(int32_t)((uint32_t)c.chord_base+c.tonic+set->root+semis[i]);
        } else {
            if(!member && !(edit.add&(1u<<(i-3u)))) continue;
            pitch=(int32_t)(c.chord_base+c.tonic+degree(c,p.column+(member ? member_steps[i] : extension[i-3u])));
        }
        pitch=from+((pitch-from)%12+12)%12;
        if(member && (edit.lower&(1u<<i))) pitch-=12;
        f[m++]=pitch;
    }
    sort_pitches(f,m);
    for(uint32_t i=0;i<m;i++) if(!count || t[count-1u]!=f[i]) t[count++]=f[i];
    for(uint32_t s=0;count && s<n;s++) { t[0]+=12; sort_pitches(t,count); }
    return count;
}
static uint32_t tones(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]);
/* Ladder step n, also below 0: a step a whole round higher (3 for the plain chords, one
 * per note when folded), an octave down. */
static uint32_t ladder(tp_layout c, tp_position p, tc_chord edit, int32_t n, int32_t t[6]) {
    if(n>=0) return tones(c,p,edit,(uint32_t)n,t);
    int32_t period=c.fold || set_chord(c,p) || full_chord_at(c,p) ? (int32_t)tones(c,p,edit,0u,t) : 3;
    if(period<1) period=1; /* a chord set's empty column, a silent Full pad */
    int32_t rounds=(-n+period-1)/period;
    uint32_t count=tones(c,p,edit,(uint32_t)(n+rounds*period),t);
    for(uint32_t i=0;i<count;i++) t[i]-=12*rounds;
    return count;
}
/* A Full pad: its voicing (the original's, for chord_base C2 an octave lower), ladder step n
 * raising the lowest note an octave n times. Edits don't apply. */
static uint32_t full_tones(tp_layout c, const full_chord *f, uint32_t n, int32_t t[6]) {
    uint32_t count=f->count<6u ? f->count : 6u;
    for(uint32_t i=0;i<count;i++) t[i]=(int32_t)f->notes[i]+(int32_t)c.chord_base-48;
    for(uint32_t s=0;count && s<n;s++) { t[0]+=12; sort_pitches(t,count); }
    return count;
}
/* The chord grid's row shape on top of the pad's edit (layout.h TC_CHORDS_GRID). */
static tc_chord grid_shape(tp_layout c, tp_position p, tc_chord edit) {
    static const uint8_t add[4]={TC_ADD7|TC_ADD9,0u,TC_ADD7,0u}; /* rows top to bottom */
    if(c.chords!=TC_CHORDS_GRID) return edit;
    edit.add|=add[p.row%4u];
    if(p.row==1u && !edit.sus) {
        uint32_t root=degree(c,p.column);
        edit.sus=degree(c,p.column+3u)-root!=5u && degree(c,p.column+1u)-root==2u ? 2u : 4u;
    }
    return edit;
}
static uint32_t tones(tp_layout c, tp_position p, tc_chord edit, uint32_t n, int32_t t[6]) {
    const full_chord *f=full_chord_at(c,p);
    if(f) return full_tones(c,f,n,t);
    edit=grid_shape(c,p,edit);
    if(c.fold) return folded(c,p,edit,n,t);
    const tc_set_chord *set=set_chord(c,p);
    if(set) { /* a chord set's chord: inversion n of all its tones (n / count octaves up) */
        uint32_t semis[12], m=set_tones(set,semis), count=0;
        if(!m) return 0u;
        uint32_t inv=n%m, up=12u*(n/m);
        for(uint32_t i=0;i<m;i++) {
            uint32_t member=(i+inv)%m;
            int32_t pitch=(int32_t)((uint32_t)c.chord_base+c.tonic+set->root+semis[member]+(i+inv>=m ? 12u : 0u)+up);
            if(member<3u && (edit.lower&(1u<<member))) pitch-=12;
            t[count++]=pitch;
        }
        return count;
    }
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
/* A chord pad's own ladder step: its row's inversion (bottom row root position), or 0 for a
 * Full or chord grid pad (each row its own chord). */
static int32_t home_step(tp_layout c, tp_position p) {
    return c.chords==TC_CHORDS_FULL || c.chords==TC_CHORDS_GRID ? 0 : 3-(int32_t)p.row;
}
bool tc_notes_panel(tp_layout c, uint32_t k, tc_chord edit, tc_panel panel, tp_notes *out) {
    tp_position p;
    if(!out || !tc_layout_ok(c) || !tp_position_of(k,&p) || edit.lower>7u ||
       (edit.sus!=0u && edit.sus!=2u && edit.sus!=4u) || edit.add>7u ||
       panel.voicing>=TC_VOICINGS || panel.spread>=TC_SPREADS || panel.octaves>0x7Fu ||
       panel.capo>=TC_CAPOS || panel.lead>TC_LEAD_ZERO+12u) return false;
    if(p.side) return melody(c,p,out);
    /* Rows: bottom root position, then first and second inversion, top root position an
     * octave up (ladder steps 0..3 from the bottom); Full and the chord grid: every pad its
     * own chord, step 0.
     * The unshaped chord must fit. Voice
     * leading picks the step itself, the voicing's inversions included. */
    int32_t n=panel.lead ? (int32_t)panel.lead-TC_LEAD_ZERO : home_step(c,p);
    int32_t shift=panel.lead ? 0 : (int32_t)panel.voicing;
    int32_t t[6];
    uint32_t count=ladder(c,p,edit,n,t);
    for(uint32_t i=0;i<count;i++) if(t[i]<0 || t[i]>127) return false;
    tp_notes result; memset(&result,0,sizeof result);
    if(!count) { *out=result; return true; } /* a chord set's empty column: silent in every voicing */
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
        m=guitar((c.tonic+chord_root(c,p))%12u,t,count,(uint32_t)n,panel.capo,v);
    } else for(uint32_t i=0;i<count;i++) v[m++]=t[i];
    /* Spread: the chord's lowest root down 1..3 octaves. */
    if(panel.spread) {
        uint32_t root=(c.tonic+chord_root(c,p))%12u, low=m;
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
    else tc_live_of_pad(layout,tc_state.live_pad%48u,tc_pad_edit(&tc_state,layout,tc_state.live_pad%48u),out);
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
    int32_t home=home_step(c,p)+(int32_t)(voicing<TC_VOICING_GUITAR ? voicing : 0u), best=home;
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
    return best_cost==UINT32_MAX ? home_step(c,p) : best;
}
static tc_panel live_panel(tp_layout layout, uint32_t k, tc_chord edit) {
    tc_panel panel={tc_state.chord_voicing,tc_state.chord_spread,tc_state.chord_octaves,tc_state.capo,0u};
    if(tc_state.voice_lead && k<48u && panel.voicing!=TC_VOICING_GUITAR && tc_state.lead.count)
        panel.lead=(uint8_t)(TC_LEAD_ZERO+tc_lead_step(layout,k,edit,panel.voicing,&tc_state.lead));
    return panel;
}
bool tc_pad(tp_layout layout, uint32_t k, tp_notes *out) {
    if(out && tc_layout_ok(layout) && tp_live_pad(layout,k)) {
        tc_live_chord ch; tc_live_current(layout,&ch);
        memset(out,0,sizeof *out); out->notes[0]=tc_live_note(layout,k,&ch); out->count=1; return true;
    }
    tc_chord edit=k<48u ? tc_pad_edit(&tc_state,layout,k) : tc_state.chord[k%48u]; /* melody pads ignore shapes */
    return tc_notes_panel(layout,k,edit,live_panel(layout,k,edit),out);
}
bool tc_pad_with(tp_layout layout, uint32_t k, tc_chord edit, tp_notes *out) {
    if(k>=48u) return false;
    return tc_notes_panel(layout,k,edit,live_panel(layout,k,edit),out);
}
bool tc_pad_core_with(tp_layout layout, uint32_t k, tc_chord edit, tp_notes *out) {
    if(!out || k>=48u || !tc_layout_ok(layout)) return false;
    tc_panel panel=live_panel(layout,k,edit);
    tc_panel core={panel.voicing==TC_VOICING_GUITAR ? 0u : panel.voicing,0u,TC_OCTAVES_DEFAULT,0u,panel.lead};
    return tc_notes_panel(layout,k,edit,core,out);
}
bool tc_pad_core(tp_layout layout, uint32_t k, tp_notes *out) {
    return k<48u && tc_pad_core_with(layout,k,tc_pad_edit(&tc_state,layout,k),out);
}
uint64_t tc_hints(tp_layout c, uint32_t k, uint64_t *also) {
    /* The next scale steps after each step (I..vii), the strong one first; 7 = none. */
    static const uint8_t next[7][3]={{3,4,5},{4,6,7},{5,3,7},{4,0,1},{0,5,7},{1,3,7},{0,2,7}};
    uint64_t strong=0, other=0;
    tp_position p;
    if(also) *also=0;
    if(!tc_layout_ok(c) || !tp_position_of(k%48u,&p)) return 0;
    bool by_step=(!c.chords || c.chords==TC_CHORDS_GRID) && tc_scale_size(c.mode)==7u;
    uint32_t from=chord_root(c,p)%12u;
    for(uint32_t j=0;j<48u;j++) {
        tp_position q; (void)tp_position_of(j,&q);
        uint64_t bit=(uint64_t)1u<<j;
        if(by_step) {
            uint32_t d=q.column%7u; const uint8_t *to=next[p.column%7u];
            if(d==to[0]) strong|=bit; else if(d==to[1] || d==to[2]) other|=bit;
            continue;
        }
        if(tc_pad_empty(c,j)) continue;
        uint32_t up=(chord_root(c,q)+12u-from)%12u;
        if(up==5u) strong|=bit; else if(up==7u || up==9u) other|=bit;
    }
    if(!strong) { strong=other; other=0; } /* no chord a fifth down (pentatonic keys): the others lead */
    if(also) *also=other;
    return strong;
}
uint8_t tc_quantise(tp_layout c, uint8_t pitch) {
    for(int32_t d=0;d<12;d++) {
        int32_t down=(int32_t)pitch-d, up=(int32_t)pitch+d;
        if(down>=0 && tc_in_scale(c.mode,(uint32_t)(down+120-(int32_t)c.tonic))) return (uint8_t)down;
        if(up<=127 && tc_in_scale(c.mode,(uint32_t)(up+120-(int32_t)c.tonic))) return (uint8_t)up;
    }
    return pitch;
}
