/* SPDX-License-Identifier: GPL-3.0-or-later - actual sampler/layout/ownership/scanner code with host I/O. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now, ons[128], offs[128], seen[4], raw[4], status[4], invalid_kind;
static uint32_t writes, completions;
static bool busy;
static bool snapshot_output;
static const uint8_t addresses[4]={0x35,0x36,0x38,0x39};
static const tk_config config={1,2500,1500,4000,4,50};
static unsigned client(uint8_t a) {
    for(unsigned i=0;i<4;i++) if(addresses[i]==a) return i;
    assert(false); return 0;
}
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen,
    uint32_t stamp, uint32_t budget) {
    (void)ctx; assert(reg==0xaa && budget==1000);
    unsigned i=client(a); seen[i]++;
    struct bi_reply r={.status=status[i],.count=2,.generation=gen,.timestamp_ms=stamp,
        .elapsed_us=500,.data={(uint8_t)raw[i],(uint8_t)(raw[i]>>8)}};
    if(invalid_kind==1 && i==0) r.count=1;
    if(invalid_kind==2 && i==0) r.generation--;
    if(invalid_kind==3 && i==0) r.elapsed_us=1001;
    return r;
}
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token==e.issued+1u && p[2]<128u);
    if(busy) return false;
    if(p[0]==TK_ON) { assert(ons[p[2]]==offs[p[2]]); ons[p[2]]++; }
    if(p[0]==TK_OFF) { assert(ons[p[2]]==offs[p[2]]+1u); offs[p[2]]++; }
    if(p[0]==TK_PRESSURE) assert(ons[p[2]]==offs[p[2]]+1u);
    writes++; return true;
}
static void reset(bool arm) {
    tc_init(&tc_state); memset(&e,0,sizeof(e)); memset(&scan,0,sizeof(scan));
    memset((void *)&ts_status,0,sizeof(ts_status));
    memset(raw,0,sizeof(raw)); memset(status,0,sizeof(status)); memset(seen,0,sizeof(seen));
    memset(ons,0,sizeof(ons)); memset(offs,0,sizeof(offs));
    writes=0; completions=0; invalid_kind=0; busy=false; now=1;
    e.layout=(tp_layout)TP_DEFAULT_LAYOUT;
    scan.valid=true; scan.frame=1; scan.last_ms=now;
    if(arm) assert(tp_arm(&e,config,&scan,1,now));
    assert(tc_poll(&tc_state,&e,reader,NULL,now));
}
static void tick(bool receipt) {
    if(e.state==TK_ACTIVE) {
        scan.frame++; scan.last_ms=now; assert(tc_frame(&e,&scan,now));
    }
    tc_step(&e,now,true,writer);
    if(receipt && e.pending_id) {
        tp_complete(&e,e.pending_id,4,true,now); completions++;
    }
    now++;
}
static void pump(unsigned n) { while(n--) tick(true); }
static void sample_mode(bool receipt) {
    now+=10;
    assert(tc_poll(&tc_state,&e,reader,NULL,now));
    tick(receipt);
}
static void sample(void) { sample_mode(true); }
static void level(uint8_t a, uint32_t mask) {
    raw[client(a)]=mask;
    for(unsigned i=0;i<3;i++) sample();
}
static void press(uint8_t a, uint8_t bit) { level(a,1u<<bit); level(a,0); }
static void drain(void) {
    tp_disable(&e); pump(256);
    assert(e.state==TK_DONE && !tp_obligations(&e) && !e.pending_id && !e.errors && !e.uncertain);
    assert(writes==completions && e.accepted==e.completed);
    for(unsigned p=0;p<128;p++) assert(ons[p]==offs[p]);
}
static void decode_all(void) {
    static const uint8_t bits[4][14]={
        {6,5,0,1,7,4,3,8,9,10,11,12,14,2}, {8,7,6,1,3},
        {3,1,0,4,5,6,7,8,9,10,11,12}, {3,1,0,4,5,6,7,8,9,10,11,12}};
    static const unsigned counts[4]={14,5,12,12}, bases[4]={5,0,31,19}, sources[4]={2,1,4,3};
    bool all[43]={false}; unsigned count=0;
    for(unsigned a=0;a<4;a++) for(unsigned i=0;i<counts[a];i++) {
        uint32_t source,index,logical,aliases;
        assert(tc_decode(addresses[a],bits[a][i],&source,&index,&logical,&aliases));
        assert(source==sources[a] && index==i && logical==bases[a]+i && !all[logical]);
        assert(aliases&(1u<<i)); all[logical]=true; count++;
        reset(false); press(addresses[a],bits[a][i]);
        assert(tc_state.presses[logical]==1 && tc_state.releases[logical]==1);
        assert(tc_state.event_sequence==2);
        for(unsigned n=0;n<4;n++) assert(seen[n]==7); /* once per poll, no source starvation */
    }
    assert(count==43);
    uint32_t s,i,l,a;
    assert(tc_decode(0x35,10,&s,&i,&l,&a) && i==9 && a==((1u<<9)|(1u<<14)));
    assert(tc_decode(0x35,9,&s,&i,&l,&a) && i==8 && a==((1u<<8)|(1u<<15)));
    assert(!tc_decode(0x37,0,&s,&i,&l,&a) && !tc_decode(0x39,15,&s,&i,&l,&a));
}
static void debounce_and_health(void) {
    reset(false); raw[3]=8; sample(); raw[3]=0; sample(); raw[3]=8; sample(); sample();
    assert(!tc_state.presses[19]); sample(); assert(tc_state.presses[19]==1);
    for(unsigned n=0;n<10;n++) sample();
    assert(tc_state.presses[19]==1);
    level(0x39,0); assert(tc_state.releases[19]==1);
    /* Startup-held is baseline. Failure never synthesizes release or re-press. */
    reset(false); raw[3]=8; status[3]=BI_NACK; sample(); status[3]=BI_OK;
    for(unsigned n=0;n<5;n++) sample();
    assert(!tc_state.presses[19]);
    level(0x39,0); press(0x39,3); assert(tc_state.presses[19]==1);
    for(unsigned kind=1;kind<=3;kind++) {
        reset(false); invalid_kind=kind; sample();
        assert(!tc_state.sampler.records[0].valid);
        assert(tc_state.sampler.records[0].status==(kind==1 ? BI_PARTIAL : kind==2 ? BI_STALE : BI_TIMEOUT));
        for(unsigned n=1;n<4;n++) assert(tc_state.sampler.records[n].valid && seen[n]==2);
    }
    reset(false); level(0x35,(1u<<10)|(1u<<15));
    assert(tc_state.unknown[0]==(1u<<15) && tc_state.ambiguous[0]==(1u<<10));
    reset(false); level(0x39,8u|2u);
    assert(tc_state.conflicts==1 && e.layout.tonic==0 && tc_state.last_action==TC_CONFLICT);
    assert(tc_state.presses[19]==1 && tc_state.presses[20]==1);
    uint32_t cycles=tc_state.cycles; assert(!tc_poll(&tc_state,&e,reader,NULL,now));
    assert(tc_state.cycles==cycles);
}
static void musical_controls(void) {
    reset(true);
    scan.values[51]=3000; pump(3); assert(ons[48]==1);
    /* Keep an accepted On pending across the next layout change. */
    scan.values[55]=3000; tick(false); assert(e.pending_id);
    uint32_t token=e.pending_id; uint8_t pitch=e.pending_pitch;
    raw[3]=2;
    for(unsigned n=0;n<3;n++) sample_mode(false);
    /* Live tonic changes while the old On is still awaiting its receipt. */
    assert(e.layout.tonic==1 && e.keys[51].captured.notes[0]==48 && e.keys[55].captured.notes[0]==50);
    assert(pitch==50 && token!=0 && e.pending_id==token);
    tp_complete(&e,token,4,true,now); completions++;
    level(0x39,0); press(0x38,8); /* M6 (index7, bit8) = Aeolian */
    assert(e.layout.mode==1u);
    scan.values[59]=3000; pump(4); assert(ons[52]==1); /* C# + minor third */
    press(0x39,3); assert(e.layout.tonic==0); /* actual C event path */
    scan.values[51]=scan.values[55]=scan.values[59]=0; pump(8);
    assert(offs[48]==1 && offs[50]==1 && offs[52]==1);
    /* Two sources share a pitch across layout change: last source owns Off. */
    scan.values[55]=3000; pump(3); press(0x39,0); /* D tonic/index2 */
    scan.values[51]=3000; pump(4);
    assert(e.keys[51].captured.notes[0]==50 && e.keys[55].captured.notes[0]==50);
    assert(ons[50]==2 && offs[50]==1 && e.voices[0][50].refs==2);
    scan.values[55]=0; pump(4); assert(offs[50]==1);
    scan.values[51]=0; pump(4); assert(offs[50]==2);
    tp_layout before=e.layout, bad=before; bad.melody_base=120;
    assert(!tc_set_layout(&tc_state,&e,bad) && !memcmp(&before,&e.layout,sizeof(before)));
    bad=before; bad.mode=(uint8_t)99; assert(!tc_set_layout(&tc_state,&e,bad));
    assert(e.state==TK_ACTIVE && tc_state.rejected==2);
    busy=true; scan.values[51]=3000; pump(3);
    assert(e.keys[51].captured.notes[0]==50 && !e.pending_id);
    press(0x39,3); /* busy transport preserves admission capture across tonic change */
    busy=false; pump(3); assert(e.keys[51].captured.notes[0]==50 && e.layout.tonic==0);
    drain();
}
static void nav_and_functions(void) {
    reset(true); scan.values[3]=3000; pump(5);
    assert(tc_state.have_chord && tc_state.last_chord==3 && ons[36] && ons[40] && ons[43]);
    /* The chord control panel, by touch bit: voicing I 6, II 5, III 0, guitar 1; spread
     * I 7, II 4, III 3; octave -3 8, -2 9, -1 10, 0 11, +1 12, +2 14, +3 2 (checked on a board). */
    #define RESTRIKE() do { scan.values[3]=0; pump(5); scan.values[3]=3000; pump(10); } while(0)
    assert(tc_panel_active(&tc_state,TC_FN_OCT_0) && !tc_panel_active(&tc_state,TC_FN_VOICING1));
    press(0x35,6); assert(tc_state.chord_voicing==1u && tc_state.last_action==TC_PANEL && tc_state.last_function==0u);
    assert(tc_panel_active(&tc_state,TC_FN_VOICING1) && !tc_panel_active(&tc_state,TC_FN_VOICING2));
    assert(e.keys[3].captured.notes[0]==36); /* a held pad keeps what it captured */
    RESTRIKE(); /* voicing I: the bottom row starts at the first inversion: E2 G2 C3 */
    assert(e.keys[3].captured.count==3 && e.keys[3].captured.notes[0]==40 && e.keys[3].captured.notes[2]==48);
    press(0x35,5); assert(tc_state.chord_voicing==2u);                 /* another voicing replaces it */
    press(0x35,5); assert(tc_state.chord_voicing==0u);                 /* the same again: off */
    press(0x35,4); assert(tc_state.chord_spread==2u && tc_panel_active(&tc_state,TC_FN_SPREAD2)); /* root 2 octaves down */
    RESTRIKE(); assert(e.keys[3].captured.count==3 && e.keys[3].captured.notes[0]==12 && e.keys[3].captured.notes[1]==40);
    press(0x35,4); assert(tc_state.chord_spread==0u);
    /* Octaves toggle: each tap turns that octave on or off; the last one off means 0. */
    press(0x35,11); assert(tc_state.chord_octaves==0x08u);             /* 0 is the only one lit: back to 0 */
    press(0x35,12); assert(tc_state.chord_octaves==0x18u);             /* tap +1: 0 and +1 stack */
    assert(tc_panel_active(&tc_state,TC_FN_OCT_P1) && tc_panel_active(&tc_state,TC_FN_OCT_0));
    RESTRIKE(); assert(e.keys[3].captured.count==6 && e.keys[3].captured.notes[0]==36 && e.keys[3].captured.notes[5]==55);
    press(0x35,11); assert(tc_state.chord_octaves==0x10u);             /* tap 0 off: the chords an octave up */
    assert(!tc_panel_active(&tc_state,TC_FN_OCT_0));
    RESTRIKE(); assert(e.keys[3].captured.count==3 && e.keys[3].captured.notes[0]==48);
    level(0x35,1u<<12); level(0x35,1u<<12|1u<<11); level(0x35,1u<<11); level(0x35,0); /* overlapping taps */
    assert(tc_state.chord_octaves==0x08u && tc_state.fn_held==0u);    /* +1 off, 0 on: each tap counts once */
    press(0x35,8); press(0x35,14); press(0x35,2);                      /* -3, +2, +3 on */
    assert(tc_state.chord_octaves==0x69u);
    press(0x35,8); press(0x35,14); press(0x35,2); press(0x35,11);      /* all off again */
    assert(tc_state.chord_octaves==0x08u);                             /* ... back to 0 */
    press(0x35,1); assert(tc_state.chord_voicing==TC_VOICING_GUITAR);  /* guitar: open C, x32010 */
    RESTRIKE(); assert(e.keys[3].captured.count==5 && e.keys[3].captured.notes[0]==48 &&
                       e.keys[3].captured.notes[1]==52 && e.keys[3].captured.notes[4]==64);
    press(0x35,1); assert(tc_state.chord_voicing==0u);
    RESTRIKE(); assert(e.keys[3].captured.count==3 && e.keys[3].captured.notes[0]==36);
    #undef RESTRIKE
    /* Arrows (up bit7, down bit1) on the play screen. Scale (default): the next defined
     * scale in the Scale menu's order, skipping empty user slots, wrapping. */
    assert(tc_state.arrows==TC_ARROWS_SCALE && tc_state.scale==0u);
    press(0x36,7); assert(tc_state.scale==1u && e.layout.mode==1u && tc_state.last_action==TC_SCALE_STEP);
    { uint8_t last=TC_ALL_SCALES-1u; while(!tc_scale_defined(tc_scale_at(last))) last--; /* down from the first: the last */
      press(0x36,1); press(0x36,1); assert(tc_state.scale==tc_scale_at(last) && last>=TC_SCALES+TC_EXTRA_SCALES-1u); }
    for(uint32_t i=0;i<TC_ALL_SCALES;i++) { press(0x36,7); assert(tc_scale_defined(tc_state.scale)); }
    while(tc_state.scale!=0u) press(0x36,7);
    /* Melody octave: two octaves either way. */
    tc_state.arrows=TC_ARROWS_OCTAVE;
    press(0x36,7); assert(tc_state.melody_base==60 && e.layout.melody_base==60 && tc_state.last_action==TC_OCTAVE);
    press(0x36,1); press(0x36,1); press(0x36,1); assert(tc_state.melody_base==24);
    press(0x36,1); assert(tc_state.melody_base==24 && tc_state.last_action==TC_REJECT);
    press(0x36,7); press(0x36,7); assert(tc_state.melody_base==48);
    /* Tempo: 5 BPM a press, stopping at the ends. */
    tc_state.arrows=TC_ARROWS_TEMPO; tc_state.tempo=TC_TEMPOS-2u;
    press(0x36,7); press(0x36,7); assert(tc_state.tempo==TC_TEMPOS-1u && tc_state.last_action==TC_TEMPO_STEP);
    press(0x36,1); assert(tc_state.tempo==TC_TEMPOS-2u);
    tc_state.tempo=16u; tc_state.arrows=TC_ARROWS_SCALE;
    /* Families (0.7.0): M4 (bit 6) is Lydian in the Major family, Lydian dominant in melodic minor. */
    press(0x38,6); assert(tc_state.scale==4u);
    tc_state.family=1u; press(0x38,6); assert(tc_state.scale==35u && e.layout.mode==35u && tc_state.last_action==TC_MODE);
    tc_state.family=0u; press(0x38,0); assert(tc_state.scale==0u); /* M1 */
    /* Favourites: the arrows step through them in order and wrap, skipping an empty user
     * slot; from a scale outside the list, up starts at the first. */
    tc_favourites[0]=24u; tc_favourites[1]=40u; tc_favourites[2]=TC_SCALES+1u; tc_favourites[3]=97u; tc_favourite_count=4u;
    assert(!tc_scale_defined(TC_SCALES+1u));
    press(0x36,7); assert(tc_state.scale==24u && tc_state.last_action==TC_FAVOURITE_STEP);
    press(0x36,7); assert(tc_state.scale==40u);
    press(0x36,7); assert(tc_state.scale==97u);
    press(0x36,7); assert(tc_state.scale==24u);
    press(0x36,1); assert(tc_state.scale==97u);
    tc_favourite_count=0u; press(0x38,0); assert(tc_state.scale==0u);
    /* Navigation, bits left to right: left 8, up 7, centre 6, down 1, right 3.
     * Play screen: left does nothing, right toggles hold, centre opens the page list. */
    press(0x36,8); assert(tc_state.ui==TC_UI_PLAY && !tc_state.hold);
    press(0x36,3); assert(tc_state.hold && tc_state.last_action==TC_HOLD);
    press(0x36,3); assert(!tc_state.hold);
    press(0x36,6); assert(tc_state.ui==TC_UI_GROUPS && tc_state.group==TC_GROUP_PLAY && tc_state.last_action==TC_MENU);
    press(0x35,6); assert(tc_state.last_action==TC_PANEL && tc_state.chord_voicing==1u); /* the panel works here too */
    press(0x35,6); assert(tc_state.chord_voicing==0u);
    press(0x36,7); assert(tc_state.group==TC_GROUP_ABOUT);       /* up wraps to the last page */
    press(0x36,1); assert(tc_state.group==TC_GROUP_PLAY);        /* down wraps back */
    /* Centre enters a page; down moves; centre edits; up raises the value; centre sets it. */
    press(0x36,6); assert(tc_state.ui==TC_UI_MENU && tc_state.menu==TC_MENU_SCALE);
    while(tc_state.menu!=TC_MENU_CHORD_AT) press(0x36,1);
    press(0x36,6); assert(tc_state.ui==TC_UI_EDIT && tc_state.edit_value==TC_AT_POLY);
    press(0x36,7); press(0x36,6); assert(tc_state.at_chord==TC_AT_CHANNEL && tc_state.at_melody==TC_AT_POLY && tc_state.ui==TC_UI_MENU);
    /* Left cancels an edit; values stop at their ends. */
    press(0x36,6); press(0x36,1); press(0x36,1); press(0x36,1); assert(tc_state.edit_value==TC_AT_OFF);
    press(0x36,8); assert(tc_state.ui==TC_UI_MENU && tc_state.at_chord==TC_AT_CHANNEL);
    press(0x36,1); assert(tc_state.menu==TC_MENU_MELODY_AT);
    press(0x36,1); assert(tc_state.menu==TC_MENU_LIVE);
    press(0x36,1); assert(tc_state.menu==TC_MENU_NOTE_REPEAT);
    press(0x36,1); assert(tc_state.menu==TC_MENU_TEMPO);
    press(0x36,1); assert(tc_state.menu==TC_MENU_ARROWS);
    press(0x36,1); assert(tc_state.menu==TC_MENU_MELODY_OCT); /* the page's last item: down wraps to Scale */
    press(0x36,6); assert(tc_state.edit_value==2u);           /* melody octave: 0 of -2..+2 */
    press(0x36,7); press(0x36,6); assert(tc_state.melody_base==60 && e.layout.melody_base==60);
    press(0x36,6); press(0x36,1); press(0x36,6); assert(tc_state.melody_base==48);
    /* Right goes deeper too: into the edit, and sets the value. Up wraps within the page. */
    press(0x36,1); assert(tc_state.menu==TC_MENU_SCALE);
    press(0x36,3); press(0x36,7); press(0x36,3); assert(e.layout.mode==1u && tc_state.scale==1u && tc_state.ui==TC_UI_MENU);
    /* Left steps back one level at a time: page -> page list -> play. */
    press(0x36,8); assert(tc_state.ui==TC_UI_GROUPS);
    press(0x36,8); assert(tc_state.ui==TC_UI_PLAY);
    /* The settings reopen where they were left. */
    press(0x36,6); assert(tc_state.ui==TC_UI_GROUPS && tc_state.group==TC_GROUP_PLAY);
    /* Presets page: saving stays in the menu and leaves the work to settings.c. */
    for(uint32_t i=0;i<6u;i++) press(0x36,1);
    assert(tc_state.group==TC_GROUP_PEDAL);
    press(0x36,1); assert(tc_state.group==TC_GROUP_PRESETS);
    press(0x36,6); press(0x36,1); assert(tc_state.menu==TC_MENU_SAVE);
    press(0x36,6); press(0x36,7); press(0x36,7); press(0x36,6);
    assert(tc_state.ui==TC_UI_MENU && tc_state.request==TC_REQUEST_SAVE && tc_state.request_slot==2u);
    tc_state.request=TC_REQUEST_NONE;
    /* LED test starts from its page and stays there; the version item cannot be edited. */
    press(0x36,8); press(0x36,1); press(0x36,6); press(0x36,7); assert(tc_state.menu==TC_MENU_LED_TEST);
    press(0x36,6); assert(tc_state.led_test && tc_state.ui==TC_UI_MENU); tc_state.led_test=false;
    /* Pads page (0.10.0): Trigger, Release, Full, Pad test. */
    press(0x36,8); press(0x36,1); assert(tc_state.group==TC_GROUP_PADS);
    press(0x36,3); assert(tc_state.menu==TC_MENU_PAD_TRIGGER);
    press(0x36,6); assert(tc_state.ui==TC_UI_EDIT && tc_state.edit_value==TC_PAD_TRIGGER_DEFAULT-TC_PAD_TRIGGER_MIN);
    press(0x36,1); press(0x36,6); assert(tc_state.ui==TC_UI_MENU && tc_state.pad_trigger==11u); /* 1100 */
    assert(tc_pad_config(&tc_state).press==1100u && e.config.press==1100u); /* the session follows */
    press(0x36,6); press(0x36,1); press(0x36,1); press(0x36,1); press(0x36,6);
    assert(tc_state.pad_trigger==8u);                                       /* 800 = Release 600 + 200 */
    press(0x36,6); press(0x36,1); press(0x36,6);                            /* 700: too close to Release */
    assert(tc_state.ui==TC_UI_EDIT && tc_state.last_action==TC_REJECT && tc_state.pad_trigger==8u);
    press(0x36,8); assert(tc_state.ui==TC_UI_MENU);
    tc_state.pad_trigger=TC_PAD_TRIGGER_DEFAULT; e.config=config;
    press(0x36,1); assert(tc_state.menu==TC_MENU_PAD_RELEASE);
    press(0x36,1); assert(tc_state.menu==TC_MENU_PAD_FULL);
    /* Pad test: an edit with nothing to change, like the I/O test. */
    press(0x36,1); assert(tc_state.menu==TC_MENU_PAD_TEST);
    press(0x36,6); assert(tc_state.ui==TC_UI_EDIT);
    press(0x36,8); assert(tc_state.ui==TC_UI_MENU && tc_state.menu==TC_MENU_PAD_TEST);
    press(0x36,1); assert(tc_state.menu==TC_MENU_PAD_TRIGGER);
    press(0x36,8); press(0x36,1); assert(tc_state.group==TC_GROUP_ABOUT);
    press(0x36,3); assert(tc_state.menu==TC_MENU_VERSION);
    press(0x36,6); press(0x36,3); assert(tc_state.ui==TC_UI_MENU);
    /* I/O test: centre opens its page (an edit with nothing to change); centre or left leaves. */
    press(0x36,1); assert(tc_state.menu==TC_MENU_IO_TEST);
    press(0x36,6); assert(tc_state.ui==TC_UI_EDIT);
    press(0x36,7); press(0x36,6); assert(tc_state.ui==TC_UI_MENU && tc_state.last_action==TC_CANCEL);
    press(0x36,3); press(0x36,8); assert(tc_state.ui==TC_UI_MENU);
    press(0x36,1); assert(tc_state.menu==TC_MENU_VERSION);
    /* Hold left and press centre: home from any depth (left alone first steps back one level). */
    press(0x36,8); press(0x36,7); press(0x36,7); press(0x36,6); press(0x36,6); assert(tc_state.ui==TC_UI_EDIT);
    level(0x36,1u<<8); assert(tc_state.ui==TC_UI_MENU && tc_state.nav_held==1u);
    level(0x36,1u<<8|1u<<6); assert(tc_state.ui==TC_UI_PLAY);
    level(0x36,0); assert(tc_state.nav_held==0u && tc_state.ui==TC_UI_PLAY);
    /* Centre alone on the play screen still opens the settings (no stale held bit). */
    press(0x36,6); assert(tc_state.ui==TC_UI_GROUPS);
    press(0x36,8); assert(tc_state.ui==TC_UI_PLAY);
    for(uint8_t g=0;g<TC_GROUPS;g++) {
        assert(tc_group_size(g)>=1u && tc_group_size(g)<=TC_GROUP_MAX);
        for(uint8_t i=0;i<tc_group_size(g);i++) assert(tc_group_items[g][i]<TC_MENU_ITEMS);
    }
    { /* every menu item is on exactly one page */
        uint64_t seen_items=0;
        for(uint8_t g=0;g<TC_GROUPS;g++) for(uint8_t i=0;i<tc_group_size(g);i++) {
            uint64_t bit=(uint64_t)1u<<tc_group_items[g][i];
            assert(!(seen_items&bit)); seen_items|=bit;
        }
        assert(seen_items==((uint64_t)1u<<TC_MENU_ITEMS)-1u);
    }
    tc_state.at_chord=TC_AT_POLY; tc_state.at_melody=TC_AT_POLY;
    drain();
}
static void layout_matrix(void) {
    reset(false);
    static const uint8_t bits[12]={3,1,0,4,5,6,7,8,9,10,11,12};
    /* Mode buttons 5,12,M1..M7 select their scale; empty U1..U3 change nothing. */
    for(uint8_t button=0;button<12;button++) {
        tp_layout before=e.layout; uint32_t deferred=tc_state.deferred;
        press(0x38,bits[button]);
        if(!tc_scale_defined(tc_mode_buttons[button])) { /* U1..U3 until the editor sets them */
            assert(!memcmp(&before,&e.layout,sizeof(before)) && tc_state.deferred==deferred+1u);
            assert(tc_state.last_action==TC_TODO_FUNCTION);
        } else assert((unsigned)e.layout.mode==tc_mode_buttons[button] && tc_state.last_action==TC_MODE);
    }
    for(uint8_t mode=0;mode<12;mode++) {
        tp_layout chosen=e.layout; chosen.mode=(uint8_t)mode;
        assert(tc_set_layout(&tc_state,&e,chosen) && (unsigned)e.layout.mode==mode);
        for(uint8_t tonic=0;tonic<12;tonic++) {
            press(0x39,bits[tonic]); assert(e.layout.tonic==tonic);
            for(uint32_t k=0;k<96;k++) {
                tp_notes actual;
                assert(tc_pad(e.layout,k,&actual));
                for(unsigned n=0;n<actual.count;n++) assert(actual.notes[n]<=127);
            }
        }
    }
    volatile uint32_t words[TC_WORDS]={0}; tc_bus b={0};
    tc_publish(&tc_state,&b,words);
    assert(words[0]==TC_MAGIC && words[1]==1 && words[2]==1024 && words[3]==words[254] && !(words[3]&1u));
    assert(words[32]==BI_MAGIC && words[6]==11 && words[7]==11);
    uint32_t seq=words[3]; tc_publish(&tc_state,&b,words); assert(words[3]==seq);
    words[244]=1; tc_publish(&tc_state,&b,words);
    assert(words[3]==seq+2 && words[245]==1 && words[244]==1);
    if(snapshot_output) {
        for(unsigned i=0;i<TC_WORDS;i++) printf("%s%u",i ? "," : "",words[i]);
        puts("");
    }
}
static uint32_t scan_us;
static void select_position(uint8_t p) { assert(p<16); }
static bool settle(uint32_t us) { scan_us+=us; return true; }
static bool sample_adc(uint8_t bank, uint32_t timeout, uint16_t *v) {
    assert(bank<6 && timeout==1000); scan_us+=42; *v=1000; return true;
}
static uint32_t micros(void) { return scan_us++; }
static void service_continuity(void) {
    reset(true); scan=(ts_scan){0}; scan_us=0;
    const ts_ops ops={select_position,sample_adc,settle,micros};
    for(unsigned n=0;n<2000;n++) {
        now=scan_us/1000u+1u;
        if(ts_step(&scan,ops,50,1000,now) && scan.frame>e.frame)
            assert(tc_frame(&e,&scan,now));
        for(unsigned i=0;i<4;i++) status[i]=(n<1000 ? BI_NACK : BI_TIMEOUT);
        if(tc_poll(&tc_state,&e,reader,NULL,now)) scan_us+=4000; /* worst four wire budgets */
        tc_step(&e,now,true,writer);
        if(e.pending_id) { tp_complete(&e,e.pending_id,4,true,now); completions++; }
        assert(e.state==TK_ACTIVE && !e.errors);
    }
    assert(scan.frame==125 && !ts_status.invalid && tc_state.cycles>100);
    for(unsigned i=0;i<4;i++) assert(seen[i]==tc_state.cycles);
    drain();
    reset(true); scan.values[51]=3000; pump(3); now+=3000;
    tc_step(&e,now,true,writer); assert(e.state==TK_DRAINING); drain();
    reset(true); scan.values[51]=3000; tick(false); now+=2000;
    tc_step(&e,now,true,writer); assert(e.state==TK_STOPPED && e.uncertain);
    reset(true); scan.values[51]=3000; pump(3); tc_step(&e,now,false,writer);
    assert(e.state==TK_STOPPED && e.uncertain);
}
int main(int argc, char **argv) {
    snapshot_output=argc==2 && !strcmp(argv[1],"--snapshot");
    decode_all(); debounce_and_health(); musical_controls(); nav_and_functions();
    layout_matrix(); service_continuity();
    puts("PASS: 43 logical press/releases, aliases/unknown/conflicts, debounce, missing/stale/partial/late clients, 12x12x96 layouts, root/scale and captured Note Offs, lower-tone/nav contexts, whole-layout reject, diagnostics, real scanner/USB-ownership continuity, watchdog/transport faults");
    return 0;
}
