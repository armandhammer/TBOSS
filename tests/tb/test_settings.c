/* SPDX-License-Identifier: GPL-3.0-or-later - settings log on a fake sector 11 with real flash
 * semantics: programming only clears bits, erase sets all bytes to 0xFF. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
#define RECORD 44u /* words per record (settings.c) */
static uint32_t flash[FS_SIZE / 4u];
static uint32_t erases, programs, fail_next_program;
static tp_engine engine;
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); erases++; return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    assert(!(offset & 3u) && offset + 4u * count <= FS_SIZE);
    programs++;
    if (fail_next_program) { fail_next_program = 0u; flash[offset / 4u] &= 0x12345678u; return false; }
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
static tc_controls fresh(void) {
    tc_controls c; tc_init(&c);
    for (uint8_t i = 0; i < TC_USER_SCALES; ++i) (void)tc_set_user_scale(i, 0, NULL);
    return c;
}
static void edit(tc_controls *c) {
    c->tonic = 7u; c->scale = 2u; c->chord_base = 24u; c->melody_base = 60u;
    c->at_chord = TC_AT_CC74; c->at_melody = TC_AT_CHANNEL; c->vel_mode = TC_VEL_FIXED; c->brightness = 6u;
    c->hold = 1u; c->strum = TC_STRUM_DOWN; c->strum_speed = 5u; c->arp = TC_ARP_UPDOWN; c->arp_rate = 2u;
    c->arp_octaves = 2u; c->arp_gate = 3u; c->tempo = 40u; c->midi_leds = 0u; c->chord_ch = 3u;
    c->melody_ch = 15u; c->arp_last = TC_ARP_RANDOM;
    c->trs_out = 0u; c->pedal_mode = TC_PEDAL_HOLD; c->pedal_type = TC_PEDAL_INVERTED; c->led_scheme = TC_LEDS_ROOT_FIFTH;
    c->chord[0] = (tc_chord){5u, 2u, TC_ADD7}; c->chord[47] = (tc_chord){0u, 4u, TC_ADD6 | TC_ADD9};
    /* 0.6.0 (words 20..25) */
    c->chord_voicing = TC_VOICING_GUITAR; c->chord_spread = 2u; c->chord_octaves = 0x1Cu; c->live_update = 1u;
    c->strum_off = (uint64_t)1u << 47 | 1u << 3; c->strum_repeat = 2u; c->strum_cutoff = 5u; c->arp_rhythm = 7u;
    c->arp_swing = 4u; c->arp_sync = 1u; c->arp_latch = 1u; c->melody_layout = TP_MELODY_DRUMS; c->bass_ch = 6u;
    c->drum_ch = 10u; c->clock_out = 1u; c->pedal_note = 38u;
    c->arrows = TC_ARROWS_TEMPO; /* 0.6.1 (word 26) */
    c->family = 2u; c->capo = 5u; c->feel_time = 3u; c->feel_vel = 4u; /* 0.7.0 (words 26, 27) */
    c->changes++;
}
static void same(const tc_controls *a, const tc_controls *b) {
    assert(a->tonic == b->tonic && a->scale == b->scale && a->chord_base == b->chord_base &&
           a->melody_base == b->melody_base && a->at_chord == b->at_chord && a->at_melody == b->at_melody && a->vel_mode == b->vel_mode &&
           a->brightness == b->brightness && a->hold == b->hold && a->strum == b->strum &&
           a->strum_speed == b->strum_speed && a->arp == b->arp && a->arp_rate == b->arp_rate &&
           a->arp_octaves == b->arp_octaves && a->arp_gate == b->arp_gate && a->tempo == b->tempo &&
           a->midi_leds == b->midi_leds && a->chord_ch == b->chord_ch && a->melody_ch == b->melody_ch &&
           a->arp_last == b->arp_last && a->trs_out == b->trs_out && a->pedal_mode == b->pedal_mode &&
           a->pedal_type == b->pedal_type && a->led_scheme == b->led_scheme);
    assert(a->chord_voicing == b->chord_voicing && a->chord_spread == b->chord_spread &&
           a->chord_octaves == b->chord_octaves && a->live_update == b->live_update && a->strum_off == b->strum_off &&
           a->strum_repeat == b->strum_repeat && a->strum_cutoff == b->strum_cutoff && a->arp_rhythm == b->arp_rhythm &&
           a->arp_swing == b->arp_swing && a->arp_sync == b->arp_sync && a->arp_latch == b->arp_latch &&
           a->melody_layout == b->melody_layout && a->bass_ch == b->bass_ch && a->drum_ch == b->drum_ch &&
           a->clock_out == b->clock_out && a->pedal_note == b->pedal_note && a->arrows == b->arrows &&
           a->family == b->family && a->capo == b->capo && a->feel_time == b->feel_time && a->feel_vel == b->feel_vel);
    for (uint32_t k = 0; k < 48u; ++k)
        assert(!memcmp(&a->chord[k], &b->chord[k], sizeof(tc_chord)));
}
static void save(tc_controls *c, uint32_t *t) { st_service(c, *t); *t += 3000u; st_service(c, *t); *t += 1000u; }
int main(void) {
    memset(flash, 0xFF, sizeof flash);
    uint32_t t = 1000u;
    /* Empty log: defaults stay, nothing written. */
    tc_controls c = fresh();
    assert(!st_load(&c) && st_records() == 0u && c.tonic == 0u && c.chord_base == TP_CHORD_BASE);
    assert(!tc_presets_present);
    st_service(&c, 1000u); assert(!programs);
    /* A change is written only after 3 s of quiet. */
    edit(&c); st_service(&c, 2000u); st_service(&c, 4999u); assert(!programs);
    st_service(&c, 5000u); assert(programs == 1u && st_records() == 1u);
    st_service(&c, 9000u); assert(programs == 1u); /* unchanged: no rewrite */
    tc_controls back = fresh(); assert(st_load(&back)); same(&back, &c);
    t = 10000u;
    /* Newer record wins; a corrupted newest record falls back to the previous one. */
    c.tonic = 9u; c.changes++; save(&c, &t);
    back = fresh(); assert(st_load(&back) && back.tonic == 9u && st_records() == 2u);
    flash[RECORD + 3u] ^= 1u; /* record 1, first payload word */
    back = fresh(); assert(st_load(&back) && back.tonic == 7u && st_records() == 1u);
    /* Valid CRC but impossible contents is ignored too (tonic 13, then channel 16). */
    tc_controls bad = c; bad.tonic = 13u; bad.changes++; save(&bad, &t);
    bad = c; bad.melody_ch = 16u; bad.changes++; save(&bad, &t);
    back = fresh(); assert(st_load(&back) && back.tonic == 7u);
    /* A failed program leaves its slot unusable; the next save uses the next slot. */
    c.tonic = 4u; c.changes += 2u; fail_next_program = 1u; save(&c, &t);
    c.tonic = 5u; c.changes++; save(&c, &t);
    back = fresh(); assert(st_load(&back) && back.tonic == 5u);
    /* Presets: save from the menu request, change everything, load it back. Brightness
     * and the MIDI-in lights are device preferences and stay as they are. */
    c.request = TC_REQUEST_SAVE; c.request_slot = 2u; st_requests(&c, &engine);
    assert(c.last_action == TC_PRESET_SAVED && c.preset == 2u && tc_presets_present == 4u);
    tc_controls saved = c;
    c.tonic = 0u; c.scale = 0u; c.arp = TC_ARP_OFF; c.chord[0] = (tc_chord){0, 0, 0}; c.brightness = 1u;
    c.trs_out = 1u; c.pedal_mode = TC_PEDAL_OFF; c.pedal_type = TC_PEDAL_AUTO; c.led_scheme = TC_LEDS_RAINBOW;
    c.arrows = TC_ARROWS_SCALE;
    c.request = TC_REQUEST_LOAD; c.request_slot = 2u; uint32_t changes = c.changes; st_requests(&c, &engine);
    assert(c.last_action == TC_PRESET_LOADED && c.changes == changes + 1u && c.brightness == 1u);
    assert(c.trs_out == 1u && c.pedal_mode == TC_PEDAL_OFF && c.pedal_type == TC_PEDAL_AUTO &&
           c.led_scheme == TC_LEDS_RAINBOW && c.arrows == TC_ARROWS_SCALE); /* the board's own */
    c.arrows = saved.arrows; c.brightness = saved.brightness; c.trs_out = saved.trs_out; c.pedal_mode = saved.pedal_mode; c.led_scheme = saved.led_scheme;
    c.pedal_type = saved.pedal_type; same(&c, &saved);
    assert(engine.layout.tonic == saved.tonic && engine.layout.mode == saved.scale);
    c.request = TC_REQUEST_LOAD; c.request_slot = 5u; st_requests(&c, &engine);
    assert(c.last_action == TC_PRESET_EMPTY && c.preset == 2u);
    back = fresh(); assert(st_load(&back) && tc_presets_present == 4u);
    /* User scales: stored, restored before the live settings that select one. */
    assert(tc_set_user_scale(1, 0x0AD5u, "my scale") && st_save_scale(1) == ST_OK); /* C D E F# G A B */
    c.scale = TC_SCALES + 1u; c.changes++; save(&c, &t);
    (void)tc_set_user_scale(1, 0, NULL);
    back = fresh(); assert(st_load(&back) && back.scale == TC_SCALES + 1u);
    assert(tc_user_scale_get(1)->mask == 0x0AD5u && !strcmp(tc_user_scale_get(1)->name, "MY SCALE"));
    /* Payload round trip through st_get/st_put; invalid payloads change nothing. */
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_PRESET, 2u, &c, p) == ST_OK && st_get(ST_PRESET, 3u, &c, p) == ST_ABSENT);
    assert(st_get(ST_PRESET, 8u, &c, p) == ST_BAD_VALUE && st_get(9u, 0u, &c, p) == ST_BAD_VALUE);
    assert(st_get(ST_LIVE, 0u, &c, p) == ST_OK);
    p[1] = (p[1] & ~0xFFu) | TC_AT_MODES; tc_controls before = c;
    assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE); same(&c, &before);
    /* Melody pressure (word 17): 0x80 | mode; a pre-0.5.0 record (0) applies the chord
     * side's setting to both; other values are refused. */
    uint32_t melody_word = p[17];
    p[1] &= ~0xFFu; /* chord side off: valid */
    assert(melody_word == (0x80u | TC_AT_CHANNEL));
    p[17] = 0x80u | TC_AT_MODES; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    p[17] = TC_AT_POLY;          assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE); /* no present bit */
    p[17] = 0x180u;              assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    same(&c, &before);
    p[1] = (p[1] & ~0xFFu) | TC_AT_CC1; p[17] = 0u;
    assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_OK && c.at_chord == TC_AT_CC1 && c.at_melody == TC_AT_CC1);
    c.at_chord = before.at_chord; c.at_melody = before.at_melody; p[17] = melody_word;
    /* Jack byte (top of word 4): present bit, TRS out, pedal mode and type. Valid otherwise
     * (pressure off), so each refusal below is the jack byte's. */
    p[1] &= ~0xFFu;
    assert((p[4] >> 24) == (0x80u | 0u | TC_PEDAL_HOLD << 1 | TC_PEDAL_INVERTED << 3 | TC_LEDS_ROOT_FIFTH << 5));
    uint32_t jacks = p[4];
    p[4] = (jacks & 0x00FFFFFFu) | 0xC0u << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE); /* bit 6 reserved */
    p[4] = (jacks & 0x00FFFFFFu) | (0x80u | 3u << 3) << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    p[4] = (jacks & 0x00FFFFFFu) | 0x01u << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    same(&c, &before);
    p[4] = jacks;
    /* 0.6.0 words 20..25, 0.6.1/0.7.0 words 26 and 27: each out-of-range value is refused, as are stray
     * bits in the reserved words and in word 22's top half. Index = word * 4 + byte. */
    static const struct { uint8_t at; uint8_t value; } v6_bad[] = {
        {80, TC_VOICINGS}, {81, TC_SPREADS}, {82, 0u}, {82, 0x80u}, {83, 2u}, {88 + 2, 1u},
        {92, TC_REPEATS}, {93, TC_CUTOFFS}, {94, TC_RHYTHMS}, {95, TC_SWINGS}, {96, 2u}, {97, 2u},
        {98, TP_MELODY_LAYOUTS}, {99, 16u}, {100, 16u}, {101, 2u}, {102, 128u}, {103, TC_PEDAL_MODES},
        {104, TC_ARROWS_MODES}, {105, TC_FAMILIES}, {106, TC_CAPOS}, {107, 1u}, {108, TC_FEEL_TIMES},
        {109, TC_FEEL_VELS}, {110, 1u}, {112, 1u}, {155, 1u}};
    assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_OK); c = before; /* the base is valid */
    for (uint32_t i = 0; i < sizeof v6_bad / sizeof v6_bad[0]; ++i) {
        uint32_t w = v6_bad[i].at / 4u, shift = v6_bad[i].at % 4u * 8u, keep = p[w];
        p[w] = (p[w] & ~(0xFFu << shift)) | (uint32_t)v6_bad[i].value << shift;
        assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
        p[w] = keep;
    }
    { uint32_t marker = p[39]; p[39] = 0u; /* without the 0.6.0 marker, words 20..25 must be empty */
      assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE); p[39] = marker; }
    same(&c, &before);
    /* A record from before 0.4.0 (no jack byte) keeps the current jack settings. */
    uint32_t v3[ST_PAYLOAD]; memcpy(v3, p, sizeof v3);
    p[4] = jacks & 0x00FFFFFFu; c.trs_out = 1u; c.pedal_mode = TC_PEDAL_SUSTAIN; c.led_scheme = TC_LEDS_RAINBOW;
    for (uint32_t i = 20u; i < ST_PAYLOAD; ++i) p[i] = 0u; /* nor any 0.6.0 words */
    uint32_t put = st_put(ST_LIVE, 0u, p, &c, &engine);
    assert(put == ST_OK && c.trs_out == 1u && c.pedal_mode == TC_PEDAL_SUSTAIN && c.pedal_type == TC_PEDAL_INVERTED &&
           c.led_scheme == TC_LEDS_RAINBOW);
    c.trs_out = 0u; c.pedal_mode = TC_PEDAL_HOLD; c.led_scheme = TC_LEDS_ROOT_FIFTH; memcpy(p, v3, sizeof v3); p[4] = jacks;
    p[1] &= ~0xFFu; p[0] = (p[0] & ~0xFFu) | 3u; /* tonic D#, pressure off */
    assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_OK && c.tonic == 3u && engine.layout.tonic == 3u);
    assert(st_put(ST_PRESET, 7u, p, &c, &engine) == ST_OK && (tc_presets_present & 0x80u));
    uint32_t s[ST_PAYLOAD] = {0x0FFFu, 0x4C4F5243u}; /* chromatic, "CROL" */
    assert(st_put(ST_SCALE, 0u, s, &c, &engine) == ST_OK && tc_user_scale_get(0)->mask == 0x0FFFu);
    s[0] = 0x0003u; assert(st_put(ST_SCALE, 0u, s, &c, &engine) == ST_BAD_VALUE); /* 2 notes */
    s[0] = 0x0FFEu; assert(st_put(ST_SCALE, 0u, s, &c, &engine) == ST_BAD_VALUE); /* no root */
    /* The scale in use cannot be cleared; another slot can. */
    assert(c.scale == TC_SCALES + 1u);
    s[0] = 0u; assert(st_put(ST_SCALE, 1u, s, &c, &engine) == ST_BAD_VALUE && tc_user_scale_get(1)->mask);
    c.scale = TC_SCALES; engine.layout = tc_layout(&c); c.changes++;
    assert(st_put(ST_SCALE, 1u, s, &c, &engine) == ST_OK && !tc_scale_defined(TC_SCALES + 1u));
    assert(st_put(ST_SCALE, 0u, s, &c, &engine) == ST_BAD_VALUE && tc_user_scale_get(0)->mask == 0x0FFFu);
    /* Favourites (0.7.0): scale ids in order, 0xFF after the last. */
    uint32_t f[ST_PAYLOAD];
    assert(st_get(ST_FAVOURITES, 0u, &c, f) == ST_ABSENT && f[0] == 0xFFFFFFFFu && tc_favourite_count == 0u);
    memset(f, 0, sizeof f);
    for (uint32_t i = 0; i < TC_FAVOURITES / 4u; ++i) f[i] = 0xFFFFFFFFu;
    f[0] = 5u | 40u << 8 | 12u << 16 | (TC_ALL_SCALES - 1u) << 24; f[1] = 0xFFFFFF00u | 9u;
    assert(st_put(ST_FAVOURITES, 0u, f, &c, &engine) == ST_OK && tc_favourite_count == 5u);
    assert(tc_favourites[0] == 5u && tc_favourites[1] == 40u && tc_favourites[3] == TC_ALL_SCALES - 1u && tc_favourites[4] == 9u);
    { uint32_t g[ST_PAYLOAD]; assert(st_get(ST_FAVOURITES, 0u, &c, g) == ST_OK && !memcmp(f, g, sizeof f)); }
    assert(st_put(ST_FAVOURITES, 1u, f, &c, &engine) == ST_BAD_VALUE);                  /* slot 0 only */
    static const struct { uint8_t word; uint32_t value; } fav_bad[] = {
        {0, 5u | 5u << 8 | 0xFFFF0000u},                  /* an id twice */
        {0, TC_ALL_SCALES | 0xFFFFFF00u},                 /* no such scale */
        {0, 0xFFu | 7u << 8 | 0xFFFF0000u},               /* an id after the end */
        {32, 1u}};                                         /* stray bits after the list */
    for (uint32_t i = 0; i < sizeof fav_bad / sizeof fav_bad[0]; ++i) {
        uint32_t g[ST_PAYLOAD]; memcpy(g, f, sizeof g); g[fav_bad[i].word] = fav_bad[i].value;
        assert(st_put(ST_FAVOURITES, 0u, g, &c, &engine) == ST_BAD_VALUE && tc_favourite_count == 5u &&
               tc_favourites[1] == 40u);
    }
    back = fresh(); tc_favourite_count = 0u; assert(st_load(&back) && tc_favourite_count == 5u && tc_favourites[4] == 9u);
    /* Nearly full at boot: erase, rewrite scales, favourites, presets and the live settings. */
    for (uint32_t i = 0; i < 1100u; ++i) { c.changes++; save(&c, &t); }
    uint32_t before_erases = erases;
    back = fresh(); assert(st_load(&back) && erases == before_erases + 1u);
    assert(st_records() == 2u + 1u + 2u + 1u); /* scales 0 and 1 (cleared), favourites, presets 2 and 7, live */
    same(&back, &c); assert(tc_presets_present == 0x84u && tc_user_scale_get(0)->mask == 0x0FFFu);
    assert(tc_favourite_count == 5u && tc_favourites[0] == 5u && tc_favourites[4] == 9u);
    back = fresh(); assert(st_load(&back) && erases == before_erases + 1u); same(&back, &c);
    /* Preset 7 selects user scale 2, cleared since: it is kept but refuses to load. */
    before = back; back.request = TC_REQUEST_LOAD; back.request_slot = 7u; st_requests(&back, &engine);
    assert(back.last_action == TC_PRESET_EMPTY); same(&back, &before);
    puts("PASS: settings log v3: defaults when empty, 3 s quiet before writing, every Stage 2, 0.6 and 0.7 setting round trips, 0.6/0.7 words validated, favourites stored/validated/restored/compacted, newest wins, corrupt/invalid records skipped, failed slot never reused, presets save/load (device prefs and jack settings kept), jack byte validation and pre-0.4.0 records, user scales restored first, payload validation, scale in use protected, boot-time compaction keeps scales and presets");
    return 0;
}
