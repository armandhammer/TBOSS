/* SPDX-License-Identifier: GPL-3.0-or-later - settings log in flash sector 11 (flash_store.c).
 * Record = 24 words: magic, sequence, kind<<8|slot, 20 payload words, CRC-32.
 * Records are appended; for each kind/slot the newest valid one wins. A record
 * interrupted by power loss fails its CRC and is skipped. Runtime saves only
 * program; the sector is erased only by st_load at boot, before USB starts (an
 * erase stalls flash for 1-2 s). Kinds: live settings, presets 0..7, scales 0..2.
 * Settings payload: tonic/scale/bases, modes, strum/arp, arp/tempo/LEDs,
 * channels, then 48 chord-edit bytes. Scale payload: mask, 12 name bytes. */
#include "settings.h"
#include "flash_store.h"
#include <string.h>
#define ST_MAGIC 0x32534254u /* "TBS2" */
#define ST_WORDS (4u + ST_PAYLOAD)
#define ST_BYTES (ST_WORDS * 4u)
#define ST_SLOTS (FS_SIZE / ST_BYTES)
#define ST_ERASE_AT (ST_SLOTS * 3u / 4u)
static uint32_t next_slot, saved_changes, seen_changes, changed_at, sequence, records;
static bool writable;
/* Newest valid record per kind/slot as log index + 1 (0 = none). */
static uint32_t live_at, preset_at[TC_PRESETS], scale_at[TC_USER_SCALES];
static uint32_t crc32(const uint32_t *words, uint32_t count) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < count; ++i)
        for (uint32_t b = 0; b < 4u; ++b) {
            crc ^= (words[i] >> (8u * b)) & 0xFFu;
            for (uint32_t k = 0; k < 8u; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    return ~crc;
}
static uint32_t *index_of(uint8_t kind, uint8_t slot) {
    if (kind == ST_LIVE && slot == 0u) return &live_at;
    if (kind == ST_PRESET && slot < TC_PRESETS) return &preset_at[slot];
    if (kind == ST_SCALE && slot < TC_USER_SCALES) return &scale_at[slot];
    return NULL;
}
static uint8_t chord_byte(tc_chord e) {
    return (uint8_t)(e.lower | (uint32_t)(e.sus / 2u) << 3 | (uint32_t)e.add << 5);
}
static void encode(const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = c->tonic | (uint32_t)c->scale << 8 | (uint32_t)c->chord_base << 16 | (uint32_t)c->melody_base << 24;
    p[1] = c->at_mode | (uint32_t)c->vel_mode << 8 | (uint32_t)c->brightness << 16 | (uint32_t)c->hold << 24;
    p[2] = c->strum | (uint32_t)c->strum_speed << 8 | (uint32_t)c->arp << 16 | (uint32_t)c->arp_rate << 24;
    p[3] = c->arp_octaves | (uint32_t)c->arp_gate << 8 | (uint32_t)c->tempo << 16 | (uint32_t)c->midi_leds << 24;
    p[4] = c->chord_ch | (uint32_t)c->melody_ch << 8 | (uint32_t)c->arp_last << 16;
    for (uint32_t i = 0; i < 12u; ++i)
        for (uint32_t b = 0; b < 4u; ++b) p[5u + i] |= (uint32_t)chord_byte(c->chord[4u * i + b]) << (8u * b);
}
static uint8_t byte(uint32_t w, uint32_t n) { return (uint8_t)(w >> (8u * n)); }
/* Every field in range and the whole layout inside MIDI range, or nothing changes. */
static bool decode(const uint32_t p[ST_PAYLOAD], tc_controls *c) {
    tc_controls t = *c;
    t.tonic = byte(p[0], 0); t.scale = byte(p[0], 1); t.chord_base = byte(p[0], 2); t.melody_base = byte(p[0], 3);
    t.at_mode = byte(p[1], 0); t.vel_mode = byte(p[1], 1); t.brightness = byte(p[1], 2); t.hold = byte(p[1], 3);
    t.strum = byte(p[2], 0); t.strum_speed = byte(p[2], 1); t.arp = byte(p[2], 2); t.arp_rate = byte(p[2], 3);
    t.arp_octaves = byte(p[3], 0); t.arp_gate = byte(p[3], 1); t.tempo = byte(p[3], 2); t.midi_leds = byte(p[3], 3);
    t.chord_ch = byte(p[4], 0); t.melody_ch = byte(p[4], 1); t.arp_last = byte(p[4], 2);
    if (t.at_mode >= TC_AT_MODES || t.vel_mode >= TC_VEL_MODES || t.brightness >= TC_BRIGHTNESS_LEVELS ||
        t.hold > 1u || t.strum >= TC_STRUM_MODES || t.strum_speed >= TC_STRUM_SPEEDS ||
        t.arp >= TC_ARP_MODES || t.arp_rate >= TC_ARP_RATES || t.arp_octaves >= TC_ARP_OCTAVES ||
        t.arp_gate >= TC_ARP_GATES || t.tempo >= TC_TEMPOS || t.midi_leds > 1u ||
        t.chord_ch > 15u || t.melody_ch > 15u || t.arp_last >= TC_ARP_MODES || (p[4] >> 24)) return false;
    for (uint32_t i = 17u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    for (uint32_t k = 0; k < 48u; ++k) {
        uint8_t b = byte(p[5u + k / 4u], k % 4u);
        uint32_t sus = (b >> 3) & 3u;
        if (sus == 3u) return false;
        t.chord[k] = (tc_chord){(uint8_t)(b & 7u), (uint8_t)(2u * sus), (uint8_t)(b >> 5)};
    }
    tp_layout layout = tc_layout(&t);
    if (!tc_layout_ok(layout) || layout.chord_base + TP_OCTAVE_RANGE < TP_CHORD_BASE ||
        layout.chord_base > TP_CHORD_BASE + TP_OCTAVE_RANGE ||
        layout.melody_base + TP_OCTAVE_RANGE < TP_MELODY_BASE ||
        layout.melody_base > TP_MELODY_BASE + TP_OCTAVE_RANGE) return false;
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tp_notes n;
        if (!tc_notes(layout, k, t.chord[k % 48u], &n)) return false;
    }
    *c = t;
    return true;
}
static void scale_payload(uint8_t slot, uint32_t p[ST_PAYLOAD]) {
    const tc_user_scale *u = tc_user_scale_get(slot);
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = u->mask;
    for (uint32_t i = 0; i < TC_USER_NAME; ++i) p[1u + i / 4u] |= (uint32_t)(uint8_t)u->name[i] << (8u * (i % 4u));
}
static bool scale_ok(const uint32_t p[ST_PAYLOAD], char name[TC_USER_NAME + 1u]) {
    if (p[0] > 0xFFFu || (p[0] && !tc_user_mask_ok((uint16_t)p[0]))) return false;
    for (uint32_t i = 4u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    for (uint32_t i = 0; i < TC_USER_NAME; ++i) {
        char ch = (char)byte(p[1u + i / 4u], i % 4u);
        if (ch && (ch < ' ' || ch > '~')) return false;
        name[i] = ch;
    }
    name[TC_USER_NAME] = 0;
    return true;
}
static const uint32_t *slot_words(uint32_t s) { return (const uint32_t *)fs_base() + s * ST_WORDS; }
static bool erased(const uint32_t *w) {
    for (uint32_t i = 0; i < ST_WORDS; ++i) if (w[i] != 0xFFFFFFFFu) return false;
    return true;
}
static bool write_record(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD]) {
    uint32_t *at = index_of(kind, slot);
    if (!at || !writable || next_slot >= ST_SLOTS) return false;
    uint32_t w[ST_WORDS];
    w[0] = ST_MAGIC; w[1] = sequence + 1u; w[2] = (uint32_t)kind << 8 | slot;
    memcpy(&w[3], p, ST_PAYLOAD * 4u);
    w[ST_WORDS - 1u] = crc32(w, ST_WORDS - 1u);
    bool ok = fs_program(next_slot * ST_BYTES, w, ST_WORDS);
    next_slot++; /* a failed slot is never reused */
    if (ok) { sequence++; records++; *at = next_slot; }
    return ok;
}
static void note_presets(void) {
    tc_presets_present = 0u;
    for (uint32_t i = 0; i < TC_PRESETS; ++i) if (preset_at[i]) tc_presets_present |= (uint8_t)(1u << i);
}
static const uint32_t *payload_at(uint32_t at) { return slot_words(at - 1u) + 3u; }
/* CRC-valid record in log slot s: its kind/slot index pointer, else NULL. */
static uint32_t *valid(uint32_t s, uint8_t *kind, uint8_t *slot) {
    const uint32_t *w = slot_words(s);
    uint32_t copy[ST_WORDS];
    for (uint32_t i = 0; i < ST_WORDS; ++i) copy[i] = w[i];
    if (copy[0] != ST_MAGIC || copy[ST_WORDS - 1u] != crc32(copy, ST_WORDS - 1u) || copy[2] >> 16) return NULL;
    *kind = (uint8_t)(copy[2] >> 8); *slot = (uint8_t)copy[2];
    return index_of(*kind, *slot);
}
bool st_load(tc_controls *c) {
    next_slot = ST_SLOTS; records = 0u; sequence = 0u; live_at = 0u;
    memset(preset_at, 0, sizeof preset_at); memset(scale_at, 0, sizeof scale_at);
    for (uint32_t s = 0; s < ST_SLOTS; ++s) {
        const uint32_t *w = slot_words(s);
        if (erased(w)) { next_slot = s; break; }
        if (w[0] != ST_MAGIC || w[ST_WORDS - 1u] != crc32(w, ST_WORDS - 1u)) continue;
        records++;
        if (w[1] > sequence) sequence = w[1];
    }
    /* Appended in order, so walking back finds each kind/slot's newest record first;
     * one that fails validation falls back to an older one. Scales come first:
     * the live settings and presets may select one. */
    for (uint32_t s = next_slot; s-- > 0u;) {
        uint8_t kind, slot; char name[TC_USER_NAME + 1u];
        uint32_t *at = valid(s, &kind, &slot);
        if (at && kind == ST_SCALE && !*at && scale_ok(slot_words(s) + 3u, name)) {
            *at = s + 1u;
            (void)tc_set_user_scale(slot, (uint16_t)slot_words(s)[3], name);
        }
    }
    /* Presets are checked when loaded (one may name a user scale cleared since). */
    bool found = false;
    for (uint32_t s = next_slot; s-- > 0u;) {
        uint8_t kind, slot;
        uint32_t *at = valid(s, &kind, &slot);
        if (!at || kind == ST_SCALE || *at) continue;
        tc_controls t = *c;
        if (kind == ST_LIVE && !decode(slot_words(s) + 3u, &t)) continue;
        *at = s + 1u;
        if (kind == ST_LIVE) { *c = t; found = true; }
    }
    writable = true;
    if (next_slot >= ST_ERASE_AT) {
        /* Before USB: compact by erasing and rewriting the current records. */
        uint32_t presets[TC_PRESETS][ST_PAYLOAD], scales[TC_USER_SCALES][ST_PAYLOAD], live[ST_PAYLOAD];
        bool had_preset[TC_PRESETS], had_scale[TC_USER_SCALES];
        for (uint32_t i = 0; i < TC_PRESETS; ++i)
            if ((had_preset[i] = preset_at[i] != 0u)) memcpy(presets[i], payload_at(preset_at[i]), sizeof presets[i]);
        for (uint8_t i = 0; i < TC_USER_SCALES; ++i) { had_scale[i] = scale_at[i] != 0u; scale_payload(i, scales[i]); }
        encode(c, live);
        writable = fs_erase();
        next_slot = 0u; records = 0u; live_at = 0u;
        memset(preset_at, 0, sizeof preset_at); memset(scale_at, 0, sizeof scale_at);
        if (writable) {
            for (uint8_t i = 0; i < TC_USER_SCALES; ++i) if (had_scale[i]) (void)write_record(ST_SCALE, i, scales[i]);
            for (uint8_t i = 0; i < TC_PRESETS; ++i) if (had_preset[i]) (void)write_record(ST_PRESET, i, presets[i]);
            (void)write_record(ST_LIVE, 0, live);
        }
    }
    note_presets();
    saved_changes = seen_changes = c->changes;
    return found;
}
void st_service(tc_controls *c, uint32_t now) {
    if (c->changes != seen_changes) { seen_changes = c->changes; changed_at = now; }
    if (c->changes == saved_changes || now - changed_at < ST_SAVE_DELAY_MS) return;
    saved_changes = c->changes;
    uint32_t p[ST_PAYLOAD];
    encode(c, p);
    (void)write_record(ST_LIVE, 0, p);
}
/* A preset is the whole musical setup; brightness and the MIDI-in lights stay. */
static bool load_preset(tc_controls *c, tp_engine *e, uint8_t slot) {
    if (slot >= TC_PRESETS || !preset_at[slot]) return false;
    tc_controls t = *c;
    if (!decode(payload_at(preset_at[slot]), &t)) return false;
    t.brightness = c->brightness; t.midi_leds = c->midi_leds;
    *c = t;
    e->layout = tc_layout(c);
    c->changes++;
    return true;
}
void st_requests(tc_controls *c, tp_engine *e) {
    uint8_t kind = c->request, slot = c->request_slot;
    if (kind == TC_REQUEST_NONE) return;
    c->request = TC_REQUEST_NONE;
    if (kind == TC_REQUEST_LOAD) {
        if (load_preset(c, e, slot)) { c->preset = slot; c->last_action = TC_PRESET_LOADED; }
        else c->last_action = TC_PRESET_EMPTY;
    } else {
        uint32_t p[ST_PAYLOAD];
        encode(c, p);
        if (write_record(ST_PRESET, slot, p)) { c->preset = slot; c->last_action = TC_PRESET_SAVED; }
        else c->last_action = TC_REJECT;
        note_presets();
    }
    c->last_source = 0u; c->event_sequence++; /* redraw */
}
uint32_t st_get(uint8_t kind, uint8_t slot, const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    uint32_t *at = index_of(kind, slot);
    if (!at) return ST_BAD_VALUE;
    if (kind == ST_LIVE) { encode(c, p); return ST_OK; }
    if (kind == ST_SCALE) { scale_payload(slot, p); return tc_user_scale_get(slot)->mask ? ST_OK : ST_ABSENT; }
    if (!*at) return ST_ABSENT;
    memcpy(p, payload_at(*at), ST_PAYLOAD * 4u);
    return ST_OK;
}
uint32_t st_save_scale(uint8_t slot) {
    uint32_t p[ST_PAYLOAD];
    if (slot >= TC_USER_SCALES) return ST_BAD_VALUE;
    scale_payload(slot, p);
    return write_record(ST_SCALE, slot, p) ? ST_OK : ST_STORAGE;
}
uint32_t st_put(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD], tc_controls *c, tp_engine *e) {
    if (!index_of(kind, slot)) return ST_BAD_VALUE;
    if (kind == ST_SCALE) {
        char name[TC_USER_NAME + 1u];
        if (!scale_ok(p, name)) return ST_BAD_VALUE;
        tc_user_scale before = *tc_user_scale_get(slot);
        (void)tc_set_user_scale(slot, (uint16_t)p[0], name);
        /* The scale in use must still fit every pad (and cannot be removed). */
        if (c->scale == TC_SCALES + slot && !tc_set_layout(c, e, tc_layout(c))) {
            (void)tc_set_user_scale(slot, before.mask, before.name);
            return ST_BAD_VALUE;
        }
        return st_save_scale(slot);
    }
    tc_controls t = *c;
    if (!decode(p, &t)) return ST_BAD_VALUE;
    if (kind == ST_LIVE) {
        *c = t; e->layout = tc_layout(c); c->changes++;
        return ST_OK; /* st_service saves it */
    }
    uint32_t result = write_record(ST_PRESET, slot, p) ? ST_OK : ST_STORAGE;
    note_presets();
    return result;
}
uint32_t st_records(void) { return records; }
