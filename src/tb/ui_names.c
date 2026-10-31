/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): every name they show for a setting's values,
 * the menu's items and pages, the buttons' actions and the notes. */
#include "ui_internal.h"
const char *const ui_roots[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char *const ui_mode_labels[12] = {"5","12","M1","M2","M3","M4","M5","M6","M7","U1","U2","U3"};
const char *const ui_menu_items[TC_MENU_ITEMS] = {"SCALE","HOLD","STRUM","STRUM TIME","ARP",
    "ARP RATE","ARP OCTAVES","ARP GATE","TEMPO","CHORD CH","MELODY CH","CHORD AT","CHORD VEL",
    "LOAD PRESET","SAVE PRESET","MIDI IN LEDS","BRIGHTNESS","LED TEST","VERSION","TRS OUT","MODE",
    "TYPE","I/O TEST","COLORS","MELODY AT","VOICING","SPREAD","ADD 7TH","ADD 9TH","ADD 6TH","SUS",
    "STRUM PAD","RESET CHORD","RESET ALL EDITS","LAYOUT","BASS CH","DRUM CH","REPEAT","CUTOFF","RHYTHM","SWING",
    "SYNC START","LATCH","RESTRIKE","CLOCK OUT","TRANSPORT","NOTE","ARROWS","MELODY OCT","FAMILY","CAPO",
    "FEEL TIME","FEEL VEL","SOUND","LIVE CH","LIVE IN","FOLD","VOICE LEAD","NOTE REPEAT","PAD TEST",
    "TRIGGER","RELEASE","FULL","MELODY VEL","TARGET","PROGRESSION","UNIT","RECORD","CLEAR","GUIDE",
    "CHORD SET","HINTS","SCALE LOCK","ROTATE","REPEAT RHYTHM","PIVOT TO","CAPTURE","RESET OPTIONS","FACTORY SETTINGS",
    "STEPS","LANE","KIND","LENGTH","RATE","CHANNEL","MUTE","CHORDS ON",
    "HARMONY","ECHO","ECHOES","MONO","CHANCE",
    "DIRECTION","EUCLID","ROTATE","RESCAN PADS",
    "UNSTICK",
    "PROGRAM IN","CHORD PROG","MELODY PROG","SPREAD OF",
    "MPE","GLIDE","PLAY",
    "VEL FULL",
    "GLIDE TIME",
    "COLOR BY","KEEP LOOK",
    "KEEP FEEL",
    "LOCK","LOOP","CHAIN","KEEP CHORDS","U3 BUTTON","SHUT DOWN","SLEEP AFTER",
    "STEP PADS",
    "SWING","TRANSPOSE","MUTATE",
    "CLEAR PRESET",
    "VOICING I","VOICING II","VOICING III","GUITAR","SPREAD I","SPREAD II",
    "SPREAD III","OCTAVE -3","OCTAVE -2","OCTAVE -1","OCTAVE 0","OCTAVE +1","OCTAVE +2","OCTAVE +3","RESET ALL"};
/* What a centre button can do (controls.h TC_ACT_), as Settings > Buttons shows it. */
const char *const ui_action_names[TC_ACTIONS] = {"VOICING 1","VOICING 2","VOICING 3","GUITAR","SPREAD 1",
    "SPREAD 2","SPREAD 3","OCT -3","OCT -2","OCT -1","OCT 0","OCT +1","OCT +2","OCT +3","SHIFT","HOLD","ARP",
    "STRUM","LEAD","FOLD","HINTS","SUSTAIN","ALL OFF","TAP TEMPO","PRESET >","PRESET <","SCALE >","SCALE <",
    "MEL OCT +","MEL OCT -","PLAY","RECORD","ADD 7TH","ADD 9TH","ADD 6TH","SUS2","SUS4","CHORD SET","NOTHING","PIVOT",
    "FILL","STUT 1/4","STUT 1/8","STUT 1/16"};
const char *const ui_group_names[TC_GROUPS] = {
    [TC_GROUP_PLAY]="PLAY",[TC_GROUP_LAYOUT]="LAYOUT",[TC_GROUP_CHORDS]="CHORDS",[TC_GROUP_CHORD_EDITS]="CHORD EDITS",
    [TC_GROUP_STRUM]="STRUM",[TC_GROUP_ARP]="ARPEGGIATOR",[TC_GROUP_EFFECTS]="EFFECTS",
    [TC_GROUP_EXPRESSION]="EXPRESSION",[TC_GROUP_PROGRESSIONS]="PROGRESSIONS",[TC_GROUP_STEPS]="STEPS",
    [TC_GROUP_PRESETS]="PRESETS",[TC_GROUP_MIDI]="MIDI",[TC_GROUP_PEDAL]="PEDAL",[TC_GROUP_BUTTONS]="BUTTONS",
    [TC_GROUP_LIGHTS]="LIGHTS",[TC_GROUP_PADS]="PADS",[TC_GROUP_ABOUT]="ABOUT"};
/* The same, short enough for RESET ... OPTIONS? on one line (1.4.0). */
const char *const ui_group_short[TC_GROUPS] = {
    [TC_GROUP_PLAY]="PLAY",[TC_GROUP_LAYOUT]="LAYOUT",[TC_GROUP_CHORDS]="CHORDS",[TC_GROUP_CHORD_EDITS]="EDITS",
    [TC_GROUP_STRUM]="STRUM",[TC_GROUP_ARP]="ARP",[TC_GROUP_EFFECTS]="EFFECT",[TC_GROUP_EXPRESSION]="EXPR",
    [TC_GROUP_PROGRESSIONS]="PROG",[TC_GROUP_STEPS]="STEPS",[TC_GROUP_PRESETS]="PRESETS",[TC_GROUP_MIDI]="MIDI",
    [TC_GROUP_PEDAL]="PEDAL",[TC_GROUP_BUTTONS]="BUTTONS",[TC_GROUP_LIGHTS]="LIGHTS",[TC_GROUP_PADS]="PADS",
    [TC_GROUP_ABOUT]="ABOUT"};
const char *const ui_pedal_modes[TC_PEDAL_MODES] = {"OFF","SUSTAIN","HOLD","SOSTENUTO","SOFT",
    "PORTAMENTO","PLAY/STOP","NOTE","FILL"};
const char *const ui_voicings[TC_VOICINGS] = {"OFF","I","II","III","GUITAR"};
const char *const ui_romans[TC_SPREADS] = {"OFF","I","II","III"};
const char *const ui_layouts[TP_MELODY_LAYOUTS] = {"SCALE","CHROMATIC","FOURTHS","DRUMS","LIVE NOTES","BASS"};
const char *const ui_tick_names[] = {"1/96","1/48","1/32","1/24","1/16"};
const char *const ui_repeat_names[TC_NOTE_REPEATS] = {"OFF","1/4","1/8","1/16","1/32","1/8T","1/16T","5/BEAT",
    "BY ROW"}; /* strum repeat has the first TC_REPEATS */
const char *const ui_cutoff_names[TC_CUTOFFS] = {"OFF","1/32","1/16","1/8","1/4","1/2","1/1"};
const char *const ui_sus_names[3] = {"OFF","SUS2","SUS4"};
const char *const ui_pedal_types[TC_PEDAL_TYPES] = {"AUTO","NORMAL","INVERTED"};
const char *const ui_led_schemes[TC_LED_SCHEMES] = {"DEFAULT","ROOT+5TH","RAINBOW","PIANO","TRIAD","FUNCTION",
    "OCTAVES"};
const char *const ui_color_bys[TC_COLOR_BYS] = {"NOTE","DEGREE"};
const char *const ui_at_names[TC_AT_MODES] = {"OFF","POLY AT","CHANNEL AT","CC1 MOD","CC2 BREATH",
    "CC11 EXPR","CC74 FILT","PITCH BEND"};
const char *const ui_at_short[TC_AT_MODES] = {"OFF","POLY","CHAN","CC1","CC2","CC11","CC74","BEND"};
const char *const ui_vel_names[TC_VEL_MODES] = {"NORMAL","FIXED 100","LIGHT","FIRM","FIRMER","FIRMEST","CUSTOM"};
const char *const ui_vel_short[TC_VEL_MODES] = {"DYN","FIX","LGT","FRM","FR2","FR3","CUS"};
const char *const ui_strum_names[TC_STRUM_MODES] = {"OFF","UP","DOWN","UP+DOWN"};
const char *const ui_arp_names[TC_ARP_MODES] = {"OFF","UP","DOWN","UP-DOWN","RANDOM","AS PLAYED",
    "CHORDS","TURING"};
const char *const ui_rate_names[TC_ARP_RATES] = {"1/4","1/8","1/8T","1/16","1/16T","1/32","1/4T","1/32T",
    "5/BEAT","7/BEAT","1/8.","1/16."};
const char *const ui_off_on[2] = {"OFF","ON"};
const char *const ui_mpe_names[TC_MPE_MODES] = {"OFF","MELODY","CHORDS","BOTH"}; /* 1.8.0 */
const char *const ui_glide_names[TC_GLIDE_MODES] = {"OFF","ROLL","PAD","TIME"}; /* 1.8.5 */
const char *const ui_harmony_names[TC_HARMONIES] = {"OFF","3RD","4TH","5TH","6TH","OCTAVE","3RD+5TH",
    "3RD BELOW","6TH BELOW"};
const char *const ui_harmony_short[TC_HARMONIES] = {"OFF","3RD","4TH","5TH","6TH","OCT","3+5","-3RD","-6TH"};
const char *const ui_echo_names[TC_ECHO_TIMES] = {"OFF","1/16","1/8T","1/8","1/8.","1/4","1/4.","1/2"};
const char *const ui_arrow_names[TC_ARROWS_MODES] = {"SCALE","MELODY OCTAVE","PRESET","TEMPO","SEQUENCER"};
const char *const ui_targets[TC_PEDAL_TARGETS] = {"BOTH","CHORDS","MELODY"};
const char *const ui_unit_names[SQ_UNIT_KINDS] = {"1 BEAT","1/2 BAR","1 BAR","2 BARS"};
const char *const ui_kind_names[LN_KINDS] = {"CHORDS","MELODY","DRUMS","OFF"};
/* 1.7.0: a lane's direction, a held step's fields and conditions (lanes.h order) */
const char *const ui_direction_names[LN_DIRECTIONS] = {"FORWARD","REVERSE","PING-PONG","RANDOM"};
const char *const ui_field_names[TC_FIELDS] = {"VELOCITY","GATE","TIE","CHANCE","RATCHET","CONDITION","NUDGE"};
const char *const ui_condition_names[LN_CONDITIONS] = {"ALWAYS","1:2","2:2","1:3","2:3","3:3","1:4","2:4",
    "3:4","4:4","FIRST","NOT FIRST","FILL","NOT FILL"};
