/* SPDX-License-Identifier: GPL-3.0-or-later - 1.4.0 chord capture on the real code: the chord a set
 * of held notes is (chord_detect.c cd_chord, against every exact symbol of chord_name.c in every
 * key and inversion), the key a list of chords fits (cd_key), and the whole flow: Settings >
 * Chords > Capture, chords from the pads (edits included) and from MIDI in (rolled, inverted,
 * legato, any channel), undo, the 12-chord limit, saving (the set relative to its key, stored in
 * flash, the board moved to that key with the set selected), the screen, and recording conflicts. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/chord_name.h"
#include "../../src/tb/chord_detect.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { (void)pad; (void)c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
/* The screen: the text drawn at column 0 of each row, as the play screen's lines are. */
static char rows[64][48];
void td_clear(void) { memset(rows, 0, sizeof rows); }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (!x && y < 64u) snprintf(rows[y], sizeof rows[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
    return true;
}
static uint16_t heard_classes(uint32_t ch) {
    uint16_t m = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (sounding[ch][p]) m |= (uint16_t)(1u << (p % 12u));
    return m;
}
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
/* The loop of usb_main: controls every 10 ms, pad frames every 15, the engine every ms. */
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        if (now % 10u == 0u) (void)tc_poll(&tc_state, &e, reader, NULL, now);
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
enum { NAV_CENTRE = 6, NAV_LEFT = 8 };
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void tap_bit(uint8_t a, uint8_t bit) { hold_bit(a, bit, true); hold_bit(a, bit, false); }
static void tap_button(uint32_t i) { tap_bit(0x35, fn_bit[i]); }
static void centre(void) { tap_bit(0x36, NAV_CENTRE); }
static void left(void) { tap_bit(0x36, NAV_LEFT); }
static void touch(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; run(60); }
static void strike(uint32_t k) { touch(k, true); touch(k, false); }
static uint32_t pad(uint32_t column, uint32_t row) { return 4u * column + row; }
static void midi_note(uint8_t ch, uint8_t pitch, uint8_t velocity) {
    uint8_t packet[4] = {velocity ? 0x09u : 0x08u, (uint8_t)((velocity ? 0x90u : 0x80u) | ((ch - 1u) & 15u)), pitch, velocity};
    tm_packet(packet, now); run(20);
}
static void fresh(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding);
    tm_reset();
    for (uint8_t s = 0; s < TC_CHORD_SETS; ++s) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(s, &none)); }
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    fs_erase(); (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void quiet(void) {
    run(200);
    for (uint32_t ch = 0; ch < 16u; ++ch) assert(!heard_classes(ch));
}
static uint32_t count(uint32_t x) { uint32_t n = 0; for (; x; x &= x - 1u) n++; return n; }
static uint16_t up(uint32_t set, uint32_t by) { by %= 12u; return (uint16_t)(((set << by) | ((uint32_t)set >> (12u - by))) & 0xFFFu); }
static uint16_t classes(const uint8_t *pcs, uint32_t n) {
    uint16_t m = 0;
    for (uint32_t i = 0; i < n; ++i) m |= (uint16_t)(1u << (pcs[i] % 12u));
    return m;
}
static const char *name_of(uint8_t root, uint16_t tones, uint8_t bass) {
    static char out[TN_MAX];
    tp_notes n = {{0}, 0};
    n.notes[n.count++] = (uint8_t)(48u + bass);
    for (uint32_t b = 0; b < 12u; ++b) if (((uint32_t)tones >> b) & 1u) n.notes[n.count++] = (uint8_t)(60u + root + b);
    (void)tn_name(&n, root, out);
    return out;
}
/* A set of pitch classes is an exact chord symbol on pc: named on it in root position, with
 * neither another bass nor a bracket of extra notes. */
static bool exact_on(uint16_t set, uint32_t pc) {
    tp_notes n = {{0}, 0};
    for (uint32_t i = 0; i < 12u; ++i) if (((uint32_t)set >> ((pc + i) % 12u)) & 1u) n.notes[n.count++] = (uint8_t)(60u + pc + i);
    char name[TN_MAX];
    (void)tn_name(&n, (uint8_t)pc, name);
    for (const char *slash = strchr(name, '/'); slash; slash = strchr(slash + 1, '/'))
        if (slash[1] >= 'A' && slash[1] <= 'G') return false; /* over another bass (6/9 is a symbol) */
    return !strchr(name, '(');
}

/* --- cd_chord ----------------------------------------------------------------------------- */
static void chords(void) {
    uint8_t root; uint16_t tones;
    /* Every exact symbol (found through tn_name: three or more notes named on C with nothing
     * extra), in every key and every inversion. */
    uint32_t shapes = 0, cases = 0, moved = 0;
    for (uint32_t s = 1; s < 0x1000u; s += 2u) {
        if (count(s) < 3u || count(s) > TC_SET_TONES || !exact_on((uint16_t)s, 0u)) continue;
        shapes++;
        for (uint32_t r = 0; r < 12u; ++r) {
            uint16_t set = up(s, r);
            for (uint32_t b = 0; b < 12u; ++b) {
                if (!(((uint32_t)set >> b) & 1u)) continue;
                cases++;
                assert(cd_chord(set, (uint8_t)b, &root, &tones));
                assert(root < 12u && (tones & 1u) && up(tones, root) == set);
                /* The bass when it is a symbol there (root position always), else the first note
                 * upward from the bass on which it is one. */
                if (b == r) assert(root == r);
                uint32_t first = 12u;
                for (uint32_t i = 0; i < 12u && first == 12u; ++i)
                    if ((((uint32_t)set >> ((b + i) % 12u)) & 1u) && exact_on(set, (b + i) % 12u)) first = (b + i) % 12u;
                assert(first < 12u && root == first);
                assert(!strchr(name_of(root, tones, (uint8_t)b), '('));
                moved += root != r;
            }
        }
    }
    assert(shapes >= 40u);
    /* The everyday chords are never misread in any inversion. */
    static const uint16_t plain[6] = {0x091u, 0x089u, 0x049u, 0x491u, 0x891u, 0x495u}; /* maj m dim 7 maj7 9 */
    for (uint32_t i = 0; i < 6u; ++i) for (uint32_t r = 0; r < 12u; ++r) for (uint32_t b = 0; b < 12u; ++b) {
        uint16_t set = up(plain[i], r);
        if (!(((uint32_t)set >> b) & 1u)) continue;
        assert(cd_chord(set, (uint8_t)b, &root, &tones) && root == r && tones == plain[i]);
    }
    /* Inversions by name: C/E is C, Am7/C is C6 (the bass's symbol), Cm7 over Eb is Eb6 too. */
    static const uint8_t c_e[3] = {4, 7, 0}, am7[4] = {9, 0, 4, 7};
    assert(cd_chord(classes(c_e, 3), 4u, &root, &tones) && root == 0u && tones == 0x091u);
    assert(cd_chord(classes(am7, 4), 0u, &root, &tones) && root == 0u && !strcmp(name_of(root, tones, 0u), "C6"));
    assert(cd_chord(classes(am7, 4), 9u, &root, &tones) && root == 9u && !strcmp(name_of(root, tones, 9u), "Am7"));
    /* No symbol at all: the bass is the root, the notes as they are. */
    static const uint8_t cluster[3] = {0, 1, 2};
    assert(cd_chord(classes(cluster, 3), 1u, &root, &tones) && root == 1u && tones == up(classes(cluster, 3), 11u));
    /* Two notes: a chord (the bass); one or none: not. */
    assert(cd_chord(0x011u, 4u, &root, &tones) && root == 4u && tones == 0x101u);
    assert(!cd_chord(0x001u, 0u, &root, &tones) && !cd_chord(0u, 0u, &root, &tones));
    /* Seven or more: the lowest six above the root. */
    assert(cd_chord(0xAB5u, 0u, &root, &tones) && root == 0u && tones == 0x2B5u && count(tones) == TC_SET_TONES);
    assert(cd_chord(0xFFFu, 5u, &root, &tones) && root == 5u && tones == 0x03Fu);
    assert(cd_chord(0xF000u | 0x091u, 0u, &root, &tones) && tones == 0x091u); /* bits above 11 ignored */
    printf("cd_chord: %u exact symbols x 12 keys x every bass = %u cases (%u named on another note), triads and "
           "sevenths in every inversion, C6 vs Am7 by the bass, clusters, dyads, scales\n", shapes, cases, moved);
}
/* --- cd_key ------------------------------------------------------------------------------- */
static uint8_t key_of(const char *const *names, uint32_t n) {
    /* Chords by name: a root letter (with # or b) and m for minor, 7 for a dominant seventh. */
    uint8_t roots[12]; uint16_t tones[12];
    for (uint32_t i = 0; i < n; ++i) {
        const char *s = names[i];
        static const uint8_t letters[7] = {9, 11, 0, 2, 4, 5, 7};
        uint32_t r = letters[s[0] - 'A']; s++;
        if (*s == '#') { r++; s++; } else if (*s == 'b') { r += 11u; s++; }
        uint16_t t = 0x091u;
        if (*s == 'm') { t = 0x089u; s++; }
        if (*s == '7') t |= 0x400u;
        roots[i] = (uint8_t)(r % 12u); tones[i] = t;
    }
    return cd_key(roots, tones, n);
}
static uint8_t major(uint32_t pc) { return (uint8_t)(1u + pc); }
static uint8_t minor(uint32_t pc) { return (uint8_t)(13u + pc); }
static void keys(void) {
    static const char *const am_f_c_g[4] = {"Am", "F", "C", "G"}, *const c_f_g_c[4] = {"C", "F", "G", "C"},
        *const ii_v_i[3] = {"Dm", "G7", "C"}, *const e7_am[2] = {"E7", "Am"}, *const d_g_a[3] = {"D", "G", "A"},
        *const bb_f[3] = {"Bb", "Eb", "F"}, *const em_c_g_d[4] = {"Em", "C", "G", "D"},
        *const fsm_d_a_e[4] = {"F#m", "D", "A", "E"}, *const c[1] = {"C"}, *const am[1] = {"Am"};
    assert(key_of(am_f_c_g, 4) == minor(9));
    assert(key_of(c_f_g_c, 4) == major(0));
    assert(key_of(ii_v_i, 3) == major(0));
    assert(key_of(e7_am, 2) == minor(9));
    assert(key_of(d_g_a, 3) == major(2));
    assert(key_of(bb_f, 3) == major(10));
    assert(key_of(em_c_g_d, 4) == minor(4));
    assert(key_of(fsm_d_a_e, 4) == minor(6));
    assert(key_of(c, 1) == major(0) && key_of(am, 1) == minor(9));
    assert(cd_key(NULL, NULL, 0u) == 0u);
    /* Every key fits its own I IV V I and i iv v i (transposed). */
    for (uint32_t pc = 0; pc < 12u; ++pc) {
        uint8_t r[4] = {(uint8_t)pc, (uint8_t)((pc + 5u) % 12u), (uint8_t)((pc + 7u) % 12u), (uint8_t)pc};
        uint16_t maj[4] = {0x091u, 0x091u, 0x091u, 0x091u}, min[4] = {0x089u, 0x089u, 0x089u, 0x089u};
        assert(cd_key(r, maj, 4u) == major(pc) && cd_key(r, min, 4u) == minor(pc));
    }
    printf("cd_key: Am F C G = A minor, C F G C = C major, ii V I, E7 Am, D G A, Bb Eb F, Em C G D, F#m D A E; "
           "I IV V I and i iv v i in every key\n");
}

/* --- on the board ------------------------------------------------------------------------- */
static const char *screen_row(uint32_t y) {
    tc_state.event_sequence++;
    run(100); ui_service(&tc_state, &e, &scan, true, now);
    return rows[y];
}
static void start(uint8_t slot) {
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_CHORDS; tc_state.menu = TC_MENU_CAPTURE;
    centre(); assert(tc_state.ui == TC_UI_EDIT);
    tc_state.edit_value = slot; centre();
}
static bool in_group(uint8_t group, uint8_t item) {
    for (uint32_t i = 0; i < TC_GROUP_MAX; ++i) if (tc_group_items[group][i] == item) return true;
    return false;
}
static uint32_t captured(uint8_t roots[12], uint16_t tones[12]) { return tc_captured(roots, tones); }
static void from_pads(void) {
    fresh();
    assert(in_group(TC_GROUP_CHORDS, TC_MENU_CAPTURE) && tc_menu_values(TC_MENU_CAPTURE) == TC_CHORD_SETS);
    uint8_t roots[12]; uint16_t tones[12];
    start(1u);
    assert(tc_state.capturing && tc_state.capture_slot == 1u && tc_state.ui == TC_UI_PLAY);
    assert(tc_state.last_action == TC_CAPTURE_START && !captured(roots, tones));
    assert(!strcmp(screen_row(36), "CAPTURE: PLAY CHORDS") && !strcmp(rows[56], "0/12 <UNDO O SAVE"));
    /* Am F C G in C major; the pads still play while capturing; G twice counts once. */
    touch(pad(5, 3), true); assert(heard_classes(0) == 0x211u); touch(pad(5, 3), false);
    assert(!strcmp(screen_row(36), "CAPTURE 1/12 Am") && !strcmp(rows[56], "1/12 <UNDO O SAVE"));
    strike(pad(3, 2)); strike(pad(7, 3)); strike(pad(4, 0)); strike(pad(11, 3));
    assert(captured(roots, tones) == 4u);
    assert(roots[0] == 9u && tones[0] == 0x089u && roots[1] == 5u && tones[1] == 0x091u);
    assert(roots[2] == 0u && tones[2] == 0x091u && roots[3] == 7u && tones[3] == 0x091u);
    /* An edited pad: as it plays (Dm7); left takes it back. */
    tc_state.chord[pad(1, 3)].add = TC_ADD7;
    strike(pad(1, 3)); assert(captured(roots, tones) == 5u && roots[4] == 2u && tones[4] == 0x489u);
    assert(!strcmp(screen_row(36), "CAPTURE 5/12 Dm7"));
    left(); assert(captured(roots, tones) == 4u && tc_state.capturing && tc_state.last_action == TC_CAPTURE_UNDO);
    assert(!strcmp(screen_row(36), "CAPTURE 4/12 UNDO"));
    tc_state.chord[pad(1, 3)].add = 0u;
    /* Centre saves: A minor; the set relative to A; the board in A minor (M6) on set 2. */
    centre();
    assert(!tc_state.capturing && tc_state.last_action == TC_CAPTURE_SAVED && tc_state.capture_key == minor(9));
    assert(tc_state.tonic == 9u && tc_state.scale == tc_mode_buttons[7] && tc_state.chord_set == 2u && e.layout.chords == 2u);
    assert(!strcmp(screen_row(36), "SET 2 IN A MINOR") && tc_state.ui == TC_UI_PLAY);
    const tc_chord_set *s = tc_chord_set_get(1);
    assert(tc_chord_set_defined(1) && !strcmp(s->name, "CAPTURE 2"));
    assert(s->column[0].mask == 0x089u && s->column[0].root == 0u && s->column[1].mask == 0x091u && s->column[1].root == 8u);
    assert(s->column[2].mask == 0x091u && s->column[2].root == 3u && s->column[3].mask == 0x091u && s->column[3].root == 10u);
    for (uint32_t i = 4; i < 12u; ++i) assert(!s->column[i].mask && !s->column[i].root);
    /* It sounds as played: Am F C G on the first four columns. */
    static const uint16_t want[4] = {0x211u, 0x221u, 0x091u, 0x884u};
    for (uint32_t i = 0; i < 4u; ++i) { touch(pad(i, 3), true); assert(heard_classes(0) == want[i]); touch(pad(i, 3), false); }
    quiet();
    /* Stored: the next start has the set and the board on it. */
    run(ST_SAVE_DELAY_MS + 200u);
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_CHORDS, 1, &tc_state, p) == ST_OK);
    tc_chord_set copy = *s, none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(1, &none));
    tc_controls back; tc_init(&back);
    assert(st_load(&back) && back.chord_set == 2u && back.tonic == 9u && back.scale == tc_mode_buttons[7]);
    assert(!memcmp(tc_chord_set_get(1), &copy, sizeof copy));
    printf("pads: Am F C G (pads sound, a repeat ignored), Dm7 with its edit then undone, saved as A minor: set 2 "
           "relative to A, board in A minor on it, sounds as played, stored\n");
}
static void major_key(void) {
    fresh();
    uint8_t roots[12]; uint16_t tones[12];
    tp_layout l = tc_layout(&tc_state); l.tonic = 2u; l.mode = tc_mode_buttons[3]; assert(tc_set_layout(&tc_state, &e, l)); /* D Dorian */
    start(0u);
    /* In D Dorian the pads are Dm Em F G Am Bdim C. */
    strike(pad(3, 3)); strike(pad(6, 3)); strike(pad(3, 3)); /* G C G */
    assert(captured(roots, tones) == 3u && roots[0] == 7u && roots[1] == 0u && roots[2] == 7u);
    centre();
    assert(tc_state.capture_key == major(7) && tc_state.tonic == 7u && tc_state.scale == tc_mode_buttons[2] && tc_state.chord_set == 1u);
    const tc_chord_set *s = tc_chord_set_get(0);
    assert(s->column[0].root == 0u && s->column[1].root == 5u && s->column[2].root == 0u && !strcmp(s->name, "CAPTURE 1"));
    assert(!strcmp(screen_row(36), "SET 1 IN G MAJOR"));
    quiet();
    printf("major: G C G from D Dorian's pads: set 1 in G major, the board on G M1\n");
}
static void from_midi(void) {
    fresh();
    uint8_t roots[12]; uint16_t tones[12];
    start(2u);
    /* Rolled Am: one note at a time, taken at the first release with all three. */
    midi_note(1, 57, 90); midi_note(1, 60, 90); assert(!captured(roots, tones));
    midi_note(1, 64, 90); assert(!captured(roots, tones));
    midi_note(1, 57, 0); assert(captured(roots, tones) == 1u && roots[0] == 9u && tones[0] == 0x089u);
    midi_note(1, 60, 0); midi_note(1, 64, 0); assert(captured(roots, tones) == 1u);
    /* Legato: C and E kept, G added: C. */
    midi_note(1, 57, 90); midi_note(1, 60, 90); midi_note(1, 64, 90); midi_note(1, 57, 0); assert(captured(roots, tones) == 1u); /* Am again */
    midi_note(1, 67, 90); midi_note(1, 64, 0);
    assert(captured(roots, tones) == 2u && roots[1] == 0u && tones[1] == 0x091u);
    midi_note(1, 60, 0); midi_note(1, 67, 0);
    /* An inversion: F over A is F. Across channels: one chord. */
    midi_note(1, 45, 90); midi_note(10, 53, 90); midi_note(16, 60, 90);
    midi_note(1, 45, 0); midi_note(10, 53, 0); midi_note(16, 60, 0);
    assert(captured(roots, tones) == 3u && roots[2] == 5u && tones[2] == 0x091u);
    /* A melody (one note at a time) is no chord; nor is a note struck twice. */
    for (uint8_t n = 60; n < 72u; n += 2u) { midi_note(1, n, 90); midi_note(1, n, 0); }
    midi_note(1, 62, 90); midi_note(2, 62, 90); midi_note(1, 62, 0); midi_note(2, 62, 0);
    assert(captured(roots, tones) == 3u);
    /* G7 with doubled notes, released all at once. */
    static const uint8_t g7[5] = {43, 55, 59, 62, 65};
    for (uint32_t i = 0; i < 5u; ++i) midi_note(1, g7[i], 80);
    for (uint32_t i = 0; i < 5u; ++i) { uint8_t off[4] = {0x08, 0x80, g7[i], 0}; tm_packet(off, now); }
    run(20);
    assert(captured(roots, tones) == 4u && roots[3] == 7u && tones[3] == 0x491u);
    assert(!strcmp(screen_row(36), "CAPTURE 4/12 G7"));
    /* Nothing is taken once capture is over. */
    centre(); assert(!tc_state.capturing && tc_state.capture_key == minor(9) && tc_state.chord_set == 3u);
    midi_note(1, 60, 90); midi_note(1, 64, 90); midi_note(1, 60, 0); midi_note(1, 64, 0);
    assert(captured(roots, tones) == 4u);
    quiet();
    printf("MIDI in: rolled Am, legato Am -> C, F/A across channels, melodies ignored, G7 doubled, saved as A minor\n");
}
static void limits(void) {
    fresh();
    uint8_t roots[12]; uint16_t tones[12];
    /* Nothing captured: centre leaves capture and saves nothing. */
    start(4u); centre();
    assert(!tc_state.capturing && tc_state.last_action == TC_CAPTURE_EMPTY && !tc_chord_set_defined(4) && !tc_state.chord_set);
    assert(!strcmp(screen_row(36), "NOTHING CAPTURED"));
    /* Undo with nothing: harmless. */
    start(4u); left(); assert(tc_state.capturing && !captured(roots, tones));
    /* Twelve chords, then FULL; the thirteenth isn't taken. */
    for (uint32_t col = 0; col < 12u; ++col) strike(pad(col, 3));
    assert(captured(roots, tones) == 12u);
    strike(pad(5, 2)); assert(captured(roots, tones) == 12u && tc_state.last_action == TC_CAPTURE_FULL);
    assert(!strcmp(screen_row(36), "12 CHORDS: O SAVES"));
    left(); assert(captured(roots, tones) == 11u);
    strike(pad(5, 2)); assert(captured(roots, tones) == 12u && roots[11] == 9u);
    centre(); assert(tc_state.last_action == TC_CAPTURE_SAVED && tc_state.chord_set == 5u);
    const tc_chord_set *s = tc_chord_set_get(4);
    for (uint32_t i = 0; i < 12u; ++i) assert(s->column[i].mask);
    /* More than six notes from a pad (held shapes): the lowest six. */
    start(6u);
    tc_capture_chord(&tc_state, 0u, 0xFFFu);
    assert(captured(roots, tones) == 1u && tones[0] == 0x03Fu);
    tc_capture_chord(&tc_state, 0u, 0x090u); assert(captured(roots, tones) == 1u); /* no root: not a chord */
    centre(); assert(tc_state.last_action == TC_CAPTURE_SAVED && tc_chord_set_defined(6));
    /* Not while recording a progression, and no recording while capturing. */
    tc_state.buttons[13] = TC_ACT_RECORD;
    tap_button(13); assert(tc_state.recording);
    start(3u); assert(!tc_state.capturing && tc_state.last_action == TC_SEQ_BUSY);
    tc_state.ui = TC_UI_PLAY; tap_button(13); assert(!tc_state.recording);
    start(3u); assert(tc_state.capturing);
    tap_button(13); assert(!tc_state.recording && tc_state.capturing && tc_state.last_action == TC_SEQ_BUSY);
    centre(); assert(!tc_state.capturing);
    quiet();
    printf("limits: nothing captured, undo with nothing, 12 then FULL, undo and refill, more than six notes, no root, "
           "recording and capturing exclude each other\n");
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    chords();
    keys();
    from_pads();
    major_key();
    from_midi();
    limits();
    printf("PASS: capture: chords from held notes (every symbol, key and inversion), keys from chords, capture from "
           "pads and MIDI in, undo, 12 limit, saved relative to its key and stored, the board moved there\n");
    return 0;
}
