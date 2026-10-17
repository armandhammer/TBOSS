/* SPDX-License-Identifier: GPL-3.0-or-later - settings log on a fake sector 11 with real flash
 * semantics: programming only clears bits, erase sets all bytes to 0xFF. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
#define RECORD 24u /* words per record (settings.c) */
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
    c->at_mode = TC_AT_CC74; c->vel_mode = TC_VEL_FIXED; c->brightness = 6u;
    c->hold = 1u; c->strum = TC_STRUM_DOWN; c->strum_speed = 5u; c->arp = TC_ARP_UPDOWN; c->arp_rate = 2u;
    c->arp_octaves = 2u; c->arp_gate = 3u; c->tempo = 40u; c->midi_leds = 0u; c->chord_ch = 3u;
    c->melody_ch = 15u; c->arp_last = TC_ARP_RANDOM;
    c->trs_out = 0u; c->pedal_mode = TC_PEDAL_HOLD; c->pedal_type = TC_PEDAL_INVERTED; c->led_scheme = TC_LEDS_ROOT_FIFTH;
    c->chord[0] = (tc_chord){5u, 2u, TC_ADD7}; c->chord[47] = (tc_chord){0u, 4u, TC_ADD6 | TC_ADD9};
    c->changes++;
}
static void same(const tc_controls *a, const tc_controls *b) {
    assert(a->tonic == b->tonic && a->scale == b->scale && a->chord_base == b->chord_base &&
           a->melody_base == b->melody_base && a->at_mode == b->at_mode && a->vel_mode == b->vel_mode &&
           a->brightness == b->brightness && a->hold == b->hold && a->strum == b->strum &&
           a->strum_speed == b->strum_speed && a->arp == b->arp && a->arp_rate == b->arp_rate &&
           a->arp_octaves == b->arp_octaves && a->arp_gate == b->arp_gate && a->tempo == b->tempo &&
           a->midi_leds == b->midi_leds && a->chord_ch == b->chord_ch && a->melody_ch == b->melody_ch &&
           a->arp_last == b->arp_last && a->trs_out == b->trs_out && a->pedal_mode == b->pedal_mode &&
           a->pedal_type == b->pedal_type && a->led_scheme == b->led_scheme);
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
    c.request = TC_REQUEST_LOAD; c.request_slot = 2u; uint32_t changes = c.changes; st_requests(&c, &engine);
    assert(c.last_action == TC_PRESET_LOADED && c.changes == changes + 1u && c.brightness == 1u);
    assert(c.trs_out == 1u && c.pedal_mode == TC_PEDAL_OFF && c.pedal_type == TC_PEDAL_AUTO &&
           c.led_scheme == TC_LEDS_RAINBOW); /* the board's own */
    c.brightness = saved.brightness; c.trs_out = saved.trs_out; c.pedal_mode = saved.pedal_mode; c.led_scheme = saved.led_scheme;
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
    /* Jack byte (top of word 4): present bit, TRS out, pedal mode and type. Valid otherwise
     * (pressure off), so each refusal below is the jack byte's. */
    p[1] &= ~0xFFu;
    assert((p[4] >> 24) == (0x80u | 0u | TC_PEDAL_HOLD << 1 | TC_PEDAL_INVERTED << 3 | TC_LEDS_ROOT_FIFTH << 5));
    uint32_t jacks = p[4];
    p[4] = (jacks & 0x00FFFFFFu) | 0xC0u << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE); /* bit 6 reserved */
    p[4] = (jacks & 0x00FFFFFFu) | (0x80u | 3u << 1) << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    p[4] = (jacks & 0x00FFFFFFu) | (0x80u | 3u << 3) << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    p[4] = (jacks & 0x00FFFFFFu) | 0x01u << 24; assert(st_put(ST_LIVE, 0u, p, &c, &engine) == ST_BAD_VALUE);
    same(&c, &before);
    /* A record from before 0.4.0 (no jack byte) keeps the current jack settings. */
    p[4] = jacks & 0x00FFFFFFu; c.trs_out = 1u; c.pedal_mode = TC_PEDAL_SUSTAIN; c.led_scheme = TC_LEDS_RAINBOW;
    uint32_t put = st_put(ST_LIVE, 0u, p, &c, &engine);
    assert(put == ST_OK && c.trs_out == 1u && c.pedal_mode == TC_PEDAL_SUSTAIN && c.pedal_type == TC_PEDAL_INVERTED &&
           c.led_scheme == TC_LEDS_RAINBOW);
    c.trs_out = 0u; c.pedal_mode = TC_PEDAL_HOLD; c.led_scheme = TC_LEDS_ROOT_FIFTH; p[4] = jacks;
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
    /* Nearly full at boot: erase, rewrite scales, presets and the live settings. */
    for (uint32_t i = 0; i < 1100u; ++i) { c.changes++; save(&c, &t); }
    uint32_t before_erases = erases;
    back = fresh(); assert(st_load(&back) && erases == before_erases + 1u);
    assert(st_records() == 2u + 2u + 1u); /* scales 0 and 1 (cleared), presets 2 and 7, live */
    same(&back, &c); assert(tc_presets_present == 0x84u && tc_user_scale_get(0)->mask == 0x0FFFu);
    back = fresh(); assert(st_load(&back) && erases == before_erases + 1u); same(&back, &c);
    /* Preset 7 selects user scale 2, cleared since: it is kept but refuses to load. */
    before = back; back.request = TC_REQUEST_LOAD; back.request_slot = 7u; st_requests(&back, &engine);
    assert(back.last_action == TC_PRESET_EMPTY); same(&back, &before);
    puts("PASS: settings log v2: defaults when empty, 3 s quiet before writing, every Stage 2 setting round trips, newest wins, corrupt/invalid records skipped, failed slot never reused, presets save/load (device prefs and jack settings kept), jack byte validation and pre-0.4.0 records, user scales restored first, payload validation, scale in use protected, boot-time compaction keeps scales and presets");
    return 0;
}
