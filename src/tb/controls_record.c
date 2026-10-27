/* SPDX-License-Identifier: GPL-3.0-or-later - the controls (controls.h): recording a progression and capturing a
 * chord set, and the transport. */
#include "controls_internal.h"
#include "sequence.h"
#include "chord_detect.h"
#include "bits.h"
#include <string.h>
/* Progressions (0.11.0): recording replaces the selected slot's progression; each chord pad
 * touch adds a unit (engine.c tc_frame), left removes the last one, centre or down finishes
 * and saves. Nothing recorded (down twice by mistake, or everything taken back) keeps what
 * the slot had: Clear empties a slot. Recording stops the transport; playing runs with it. */
static sq_prog before_recording;
void tc_start_recording(tc_controls *c) {
    if(c->capturing) { c->last_action=TC_SEQ_BUSY; return; }
    before_recording=sq;
    sq_clear(&sq,sq.unit,c->scale);
    sq.chords=c->chord_set; /* it keeps the chord set it was recorded with */
    c->recording=true; c->transport=false; c->ui=TC_UI_PLAY; c->last_action=TC_SEQ_RECORD;
}
void tc_finish_recording(tc_controls *c) {
    c->recording=false;
    if(!sq.count) { sq=before_recording; sq_changes++; c->last_action=TC_SEQ_KEPT; return; }
    tc_request(c,TC_REQUEST_PROG_SAVE,c->prog_slot); c->last_action=TC_SEQ_SAVED;
}
/* Chord capture (1.4.0): Settings > Chords > Capture picks a set slot; then each chord played (a
 * chord pad, or notes held together on MIDI in, engine.c) is its next column, left takes the last
 * one back, centre finishes. The set is kept relative to the key the chords fit best, and the
 * board moves to that key (major M1, minor M6) with the set selected, so it sounds as played. */
static uint8_t capture_roots[12];
static uint16_t capture_tones[12];
static uint32_t capture_count;
void tc_start_capture(tc_controls *c, uint8_t slot) {
    if(c->recording) { c->last_action=TC_SEQ_BUSY; return; }
    c->capturing=true; c->capture_slot=(uint8_t)(slot%TC_CHORD_SETS); capture_count=0;
    c->ui=TC_UI_PLAY; c->last_action=TC_CAPTURE_START;
}
void tc_capture_chord(tc_controls *c, uint8_t root, uint16_t tones) {
    if(!c->capturing || !(tones&1u)) return;
    root%=12u;
    tones=(uint16_t)tb_lowest(tones,TC_SET_TONES); /* a set's chord has at most 6 notes: the lowest */
    if(capture_count && capture_roots[capture_count-1u]==root && capture_tones[capture_count-1u]==tones) return;
    if(capture_count>=12u) { c->last_action=TC_CAPTURE_FULL; c->event_sequence++; return; }
    capture_roots[capture_count]=root; capture_tones[capture_count]=tones; capture_count++;
    c->last_action=TC_CAPTURE_ADDED; c->event_sequence++;
}
uint32_t tc_captured(uint8_t roots[12], uint16_t tones[12]) {
    for(uint32_t i=0;i<capture_count;i++) { roots[i]=capture_roots[i]; tones[i]=capture_tones[i]; }
    return capture_count;
}
void tc_finish_capture(tc_controls *c, tp_engine *e) {
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
    tc_request(c,TC_REQUEST_SET_SAVE,c->capture_slot);
    tp_layout layout=tc_layout(c);
    layout.tonic=tonic; layout.mode=key>12u ? tc_mode_buttons[7] : tc_mode_buttons[2]; layout.chords=(uint8_t)(c->capture_slot+1u);
    c->capture_key=key;
    if(tc_set_layout(c,e,layout)) c->last_action=TC_CAPTURE_SAVED;
}
void tc_toggle_transport(tc_controls *c) {
    if(c->recording) tc_finish_recording(c);
    c->transport=!c->transport;
    c->last_action=c->transport && !sq.count && !ln_any(&ln) ? TC_SEQ_EMPTY : TC_TRANSPORT;
}
void tc_capture_undo(void) { if(capture_count) capture_count--; }
