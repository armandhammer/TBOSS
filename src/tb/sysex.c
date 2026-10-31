/* SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard SysEx protocol v1 (see sysex.h). */
#include "sysex.h"
#include "nor.h"
#include "ui.h"
#include "version.h"
#include "lanes.h"
#include "chord_name.h"
#include "sequence.h"
#include <string.h>
uint8_t sx_out[SX_OUT_MAX];
const ts_scan *sx_scanner;
static uint16_t preview_mask;
static uint8_t preview_root, preview_slot = 0xFFu;
static uint32_t preview_at;
typedef struct { uint32_t n; uint32_t sum; } reply_t;
static void put(reply_t *r, uint8_t b) {
    if (r->n < SX_OUT_MAX) sx_out[r->n++] = b & 0x7Fu;
    r->sum += b & 0x7Fu;
}
static void begin(reply_t *r, uint8_t cmd, uint8_t status) {
    r->n = 0; r->sum = 0;
    sx_out[r->n++] = 0xF0u; sx_out[r->n++] = 0x7Du; sx_out[r->n++] = 0x54u; sx_out[r->n++] = 0x42u;
    put(r, (uint8_t)(cmd | 0x40u)); put(r, status);
}
static uint32_t finish(reply_t *r) {
    put(r, (uint8_t)((128u - r->sum % 128u) % 128u));
    if (r->n < SX_OUT_MAX) sx_out[r->n++] = 0xF7u;
    return r->n;
}
static void put_packed(reply_t *r, const uint8_t *in, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 7u) {
        uint32_t m = n - i < 7u ? n - i : 7u, top = 0;
        for (uint32_t j = 0; j < m; ++j) top |= (uint32_t)(in[i + j] >> 7) << j;
        put(r, (uint8_t)top);
        for (uint32_t j = 0; j < m; ++j) put(r, in[i + j]);
    }
}
uint32_t sx_pack(const uint8_t *in, uint32_t n, uint8_t *out) {
    uint32_t o = 0;
    for (uint32_t i = 0; i < n; i += 7u) {
        uint32_t m = n - i < 7u ? n - i : 7u, top = 0;
        for (uint32_t j = 0; j < m; ++j) top |= (uint32_t)(in[i + j] >> 7) << j;
        out[o++] = (uint8_t)top;
        for (uint32_t j = 0; j < m; ++j) out[o++] = in[i + j] & 0x7Fu;
    }
    return o;
}
uint32_t sx_unpack(const uint8_t *in, uint32_t n, uint8_t *out) {
    uint32_t o = 0;
    for (uint32_t i = 0; i < n; i += 8u) {
        uint32_t m = n - i - 1u < 7u ? n - i - 1u : 7u;
        for (uint32_t j = 0; j < m; ++j) out[o++] = (uint8_t)(in[i + 1u + j] | ((in[i] >> j) & 1u) << 7);
    }
    return o;
}
static uint32_t packed_size(uint32_t n) { return n + (n + 6u) / 7u; }
static uint32_t crc32(const uint8_t *b, uint32_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; ++i) {
        crc ^= b[i];
        for (uint32_t k = 0; k < 8u; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static uint32_t status_only(uint8_t cmd, uint32_t status) {
    reply_t r; begin(&r, cmd, (uint8_t)status); return finish(&r);
}
static void words_to_bytes(const uint32_t *w, uint32_t n, uint8_t *b) {
    for (uint32_t i = 0; i < n; ++i) for (uint32_t k = 0; k < 4u; ++k) b[4u * i + k] = (uint8_t)(w[i] >> (8u * k));
}
static void remote(tc_controls *c) { c->last_action = TC_REMOTE; c->last_source = 0u; c->event_sequence++; }
static void put2(reply_t *r, uint32_t v) { put(r, (uint8_t)(v & 0x7Fu)); put(r, (uint8_t)((v >> 7) & 0x7Fu)); }
static void put_text(reply_t *r, const char *t) { while (*t) put(r, (uint8_t)*t++); }
bool sx_inline(const uint8_t *msg, uint32_t len) {
    if (len < 7u) return false;
    uint8_t cmd = msg[4];
    return cmd == SX_IDENTIFY || cmd == SX_GET_SCALE || cmd == SX_GET_RECORD || cmd == SX_MENU || cmd == SX_ITEM ||
           cmd == SX_PADS || cmd == SX_VALUES || cmd == SX_CHORD_NAMES || cmd == SX_STATE;
}
/* 1.9.4, STATE: which items the menu shows now (bit i = item i). */
static void shown_items(const tc_controls *c, bool shown[TC_MENU_ITEMS]) {
    memset(shown, 0, TC_MENU_ITEMS * sizeof shown[0]);
    uint8_t items[TC_GROUP_MAX];
    for (uint8_t g = 0; g < TC_GROUPS; ++g)
        for (uint8_t i = 0, k = tc_page_items(c, g, items); i < k; ++i) if (items[i] < TC_MENU_ITEMS) shown[items[i]] = true;
}
static void put32(reply_t *r, uint32_t v) { for (uint32_t k = 0; k < 5u; ++k) put(r, (uint8_t)((v >> (7u * k)) & 0x7Fu)); }
uint32_t sx_handle(const uint8_t *msg, uint32_t len, tc_controls *c, tp_engine *e, uint32_t now) {
    if (len < 7u || msg[0] != 0xF0u || msg[1] != 0x7Du || msg[2] != 0x54u || msg[3] != 0x42u ||
        msg[len - 1u] != 0xF7u) return 0u;
    uint8_t cmd = msg[4];
    const uint8_t *d = &msg[5];
    uint32_t n = len - 7u, sum = cmd; /* data bytes between cmd and the checksum */
    for (uint32_t i = 5; i < len - 1u; ++i) { if (msg[i] > 0x7Fu) return status_only(cmd, SX_LENGTH); sum += msg[i]; }
    if (cmd > 0x3Fu) return 0u; /* replies (ours echoed back) and other traffic */
    if (sum % 128u) return status_only(cmd, SX_CHECKSUM);
    reply_t r;
    switch (cmd) {
    case SX_IDENTIFY: {
        if (n) return status_only(cmd, SX_LENGTH);
        uint8_t info[7]; uint32_t size = nr_size();
        nr_id(info); for (uint32_t k = 0; k < 4u; ++k) info[3u + k] = (uint8_t)(size >> (8u * k));
        begin(&r, cmd, SX_OK);
        put(&r, SX_VERSION); put(&r, 4u); put(&r, TC_USER_SCALES); put(&r, TC_PRESETS); put(&r, ST_PAYLOAD);
        put(&r, nr_present() ? 1u : 0u);
        put_packed(&r, info, 7u);
        put(&r, TB_VERSION_MAJOR); put(&r, TB_VERSION_MINOR); put(&r, TB_VERSION_PATCH);
        return finish(&r);
    }
    case SX_GET_SCALE: {
        if (n != 1u) return status_only(cmd, SX_LENGTH);
        const tc_user_scale *u = tc_user_scale_get(d[0]);
        if (!u) return status_only(cmd, SX_BAD_VALUE);
        begin(&r, cmd, SX_OK);
        put(&r, d[0]); put(&r, (uint8_t)(u->mask & 0x7Fu)); put(&r, (uint8_t)(u->mask >> 7));
        for (uint32_t i = 0; i < TC_USER_NAME; ++i) put(&r, (uint8_t)u->name[i]);
        return finish(&r);
    }
    case SX_SET_SCALE: {
        if (n != 3u + TC_USER_NAME) return status_only(cmd, SX_LENGTH);
        uint32_t p[ST_PAYLOAD] = {0};
        p[0] = d[1] | (uint32_t)d[2] << 7;
        for (uint32_t i = 0; i < TC_USER_NAME; ++i) p[1u + i / 4u] |= (uint32_t)d[3u + i] << (8u * (i % 4u));
        uint32_t status = st_put(ST_SCALE, d[0], p, c, e);
        if (status == ST_OK) remote(c);
        begin(&r, cmd, (uint8_t)status); put(&r, d[0]);
        return finish(&r);
    }
    case SX_PREVIEW:
        if (n == 3u || n == 4u) { /* optional 4th byte (0.5.0): the user slot being edited */
            preview_mask = (uint16_t)((d[0] | (uint32_t)d[1] << 7) & 0xFFFu);
            preview_root = d[2] % 12u; preview_at = now;
            preview_slot = n == 4u && d[3] < TC_USER_SCALES ? d[3] : 0xFFu;
        }
        return 0u;
    case SX_SELECT_SCALE: {
        if (n != 1u) return status_only(cmd, SX_LENGTH);
        tp_layout layout = tc_layout(c);
        layout.mode = d[0];
        bool ok = d[0] < TC_ALL_SCALES && tc_set_layout(c, e, layout);
        if (ok) remote(c);
        return status_only(cmd, ok ? SX_OK : SX_BAD_VALUE);
    }
    case SX_GET_RECORD: {
        if (n != 2u) return status_only(cmd, SX_LENGTH);
        uint32_t p[ST_PAYLOAD]; uint8_t bytes[4u * ST_PAYLOAD];
        uint32_t status = st_get(d[0], d[1], c, p);
        begin(&r, cmd, (uint8_t)status); put(&r, d[0]); put(&r, d[1]);
        if (status == ST_OK) { words_to_bytes(p, ST_PAYLOAD, bytes); put_packed(&r, bytes, sizeof bytes); }
        return finish(&r);
    }
    case SX_SET_RECORD: {
        /* 40 words, or 20 from firmware before 0.6.0 (the rest then reads as zeros); 1.9.0: 40 words
         * after a flags byte, SX_QUIET for no reply. */
        bool flagged = n == 3u + packed_size(4u * ST_PAYLOAD);
        uint32_t words = flagged || n == 2u + packed_size(4u * ST_PAYLOAD) ? ST_PAYLOAD :
                         n == 2u + packed_size(4u * ST_PAYLOAD_V2) ? ST_PAYLOAD_V2 : 0u;
        if (!words) return status_only(cmd, SX_LENGTH);
        if (flagged && d[2] & ~SX_QUIET) return status_only(cmd, SX_BAD_VALUE);
        bool quiet = flagged && (d[2] & SX_QUIET);
        uint8_t bytes[4u * ST_PAYLOAD] = {0}; uint32_t p[ST_PAYLOAD];
        (void)sx_unpack(&d[flagged ? 3 : 2], packed_size(4u * words), bytes);
        for (uint32_t i = 0; i < ST_PAYLOAD; ++i)
            p[i] = bytes[4u * i] | (uint32_t)bytes[4u * i + 1u] << 8 | (uint32_t)bytes[4u * i + 2u] << 16 |
                   (uint32_t)bytes[4u * i + 3u] << 24;
        uint32_t status = quiet ? st_put_quiet(d[0], d[1], p, c, e) : st_put(d[0], d[1], p, c, e);
        if (status == ST_OK) remote(c);
        if (quiet) return 0u;
        begin(&r, cmd, (uint8_t)status); put(&r, d[0]); put(&r, d[1]);
        return finish(&r);
    }
    case SX_MENU: { /* a page: its items, in the board's order, then its name */
        if (n != 1u) return status_only(cmd, SX_LENGTH);
        if (d[0] >= TC_GROUPS) return status_only(cmd, SX_BAD_VALUE);
        uint8_t count = tc_group_size(d[0]);
        begin(&r, cmd, SX_OK); put(&r, d[0]); put(&r, TC_GROUPS); put(&r, count);
        for (uint8_t i = 0; i < count; ++i) put2(&r, tc_group_items[d[0]][i]);
        put_text(&r, ui_page_name(d[0]));
        return finish(&r);
    }
    case SX_ITEM: { /* an item, its value and its values' names from first on, as many as fit */
        if (n != 4u) return status_only(cmd, SX_LENGTH);
        uint32_t item = d[0] | (uint32_t)d[1] << 7, first = d[2] | (uint32_t)d[3] << 7;
        if (item >= TC_MENU_ITEMS) return status_only(cmd, SX_BAD_VALUE);
        uint8_t id = (uint8_t)item, items[TC_GROUP_MAX];
        uint32_t values = tc_menu_values(id), shown = 0;
        for (uint8_t g = 0; g < TC_GROUPS && !shown; ++g)
            for (uint8_t i = 0, k = tc_page_items(c, g, items); i < k; ++i) shown |= items[i] == id;
        begin(&r, cmd, SX_OK); put2(&r, item); put(&r, tc_item_kind(id)); put(&r, (uint8_t)shown);
        put2(&r, values); put2(&r, tc_menu_value(c, id)); put2(&r, first);
        uint32_t at = r.n; put(&r, 0); /* how many names follow, set below */
        put_text(&r, ui_item_name(id)); put(&r, 0);
        uint32_t names = 0;
        for (uint32_t v = first; v < values && names < 127u && r.n + UI_TEXT_MAX + 1u <= SX_ITEM_FILL; ++v, ++names) {
            put_text(&r, ui_value_text(id, (uint8_t)v)); put(&r, 0);
        }
        r.sum += names; sx_out[at] = (uint8_t)names;
        return finish(&r);
    }
    case SX_SET_ITEM: { /* no reply: the board shows CHANGED BY EDITOR, or the menu's refusal */
        if (n != 4u) return 0u;
        uint32_t item = d[0] | (uint32_t)d[1] << 7, value = d[2] | (uint32_t)d[3] << 7;
        if (item < TC_MENU_ITEMS && tc_set_item(c, e, (uint8_t)item, value) &&
            c->last_action == TC_SETTING) remote(c);
        return 0u;
    }
    case SX_PADS: { /* each pad by number (not where it lights): its resting level, its reading now */
        if (n) return status_only(cmd, SX_LENGTH);
        const ts_scan *s = sx_scanner;
        begin(&r, cmd, SX_OK);
        put(&r, TS_KEYS); put(&r, (uint8_t)((e->rested ? 1u : 0u) | (s && s->valid ? 2u : 0u)));
        put(&r, c->pad_trigger); put(&r, c->pad_release); put(&r, c->pad_full); put(&r, c->pad_vel_full);
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            uint32_t p = tc_physical(c, k), v = s && s->valid ? s->values[p] : 0u;
            put2(&r, e->rested ? e->rest[p] : 0u); put2(&r, v > 16383u ? 16383u : v);
        }
        return finish(&r);
    }
    case SX_STATE: { /* 1.9.4: the menu check (FNV-1a of every value and where it shows), ln/sq/saved changes, shown */
        if (n) return status_only(cmd, SX_LENGTH);
        bool shown[TC_MENU_ITEMS];
        shown_items(c, shown);
        uint32_t check = 2166136261u;
        for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i) {
            uint32_t v = (uint32_t)tc_menu_value(c, (uint8_t)i) << 1 | (shown[i] ? 1u : 0u);
            for (uint32_t k = 0; k < 4u; ++k) { check ^= (v >> (8u * k)) & 255u; check *= 16777619u; }
        }
        /* 2.0.7: which presets there are and their names: the value lists of Load, Save and Clear preset (board test A2) */
        check ^= tc_presets_present; check *= 16777619u;
        for (uint32_t i = 0; i < TC_PRESETS; ++i)
            for (uint32_t k = 0; k < TC_PRESET_NAME; ++k) { check ^= (uint8_t)tc_preset_names[i][k]; check *= 16777619u; }
        begin(&r, cmd, SX_OK);
        /* 2.0.6: progressions = the live one's changes and the slots' (sq_chain_changes bumps once a recording or a
         * clear is in flash; the live one changes before the recording is saved: the editor read too soon) */
        put32(&r, check); put32(&r, ln_changes); put32(&r, sq_changes + sq_chain_changes); put32(&r, c->changes);
        put2(&r, TC_MENU_ITEMS);
        for (uint32_t i = 0; i < TC_MENU_ITEMS; i += 7u) {
            uint8_t b = 0;
            for (uint32_t k = 0; k < 7u && i + k < TC_MENU_ITEMS; ++k) b |= (uint8_t)((shown[i + k] ? 1u : 0u) << k);
            put(&r, b);
        }
        return finish(&r);
    }
    case SX_VALUES: { /* 1.9.3: every item's value (as ITEM's) in preset slot, or the live settings */
        if (n != 2u) return status_only(cmd, SX_LENGTH);
        static tc_controls t;
        uint32_t status = st_values(d[0], d[1], c, &t);
        begin(&r, cmd, (uint8_t)status); put(&r, d[0]); put(&r, d[1]);
        if (status != ST_OK) return finish(&r);
        put2(&r, TC_MENU_ITEMS);
        for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i) put2(&r, tc_menu_value(&t, (uint8_t)i));
        return finish(&r);
    }
    case SX_SET_EDITS: { /* 1.9.3, no reply: the chord pads' edits, strum off and sound off, all or nothing */
        if (n != packed_size(SX_EDIT_BYTES)) return 0u;
        uint8_t b[SX_EDIT_BYTES];
        (void)sx_unpack(d, n, b);
        tc_chord chord[48];
        uint64_t strum = 0u, mute = 0u;
        for (uint32_t k = 0; k < 6u; ++k) { strum |= (uint64_t)b[48u + k] << (8u * k); mute |= (uint64_t)b[54u + k] << (8u * k); }
        tp_layout layout = tc_layout(c);
        for (uint32_t k = 0; k < 48u; ++k) {
            tp_notes notes;
            if (((b[k] >> 3) & 3u) == 3u) return 0u;
            chord[k] = ln_edit(b[k]);
            if (!tc_notes(layout, k, chord[k], &notes)) return 0u; /* every pad still in MIDI range */
        }
        memcpy(c->chord, chord, sizeof chord); c->strum_off = strum; c->chord_mute = mute;
        c->changes++; remote(c);
        return 0u;
    }
    case SX_CHORD_NAMES: { /* 1.9.3: each chord pad's chord, with its edits, as the play screen names it */
        if (n) return status_only(cmd, SX_LENGTH);
        tp_layout layout = tc_layout(c);
        begin(&r, cmd, SX_OK); put(&r, 48u);
        for (uint32_t k = 0; k < 48u; ++k) { char name[TN_MAX]; (void)tn_pad(layout, k, name); put_text(&r, name); put(&r, 0); }
        return finish(&r);
    }
    case SX_NOR_READ: {
        /* address: 4 x 7 bits (low first); length: 2 x 7 bits, 1..2048. */
        if (n != 6u) return status_only(cmd, SX_LENGTH);
        uint32_t address = d[0] | (uint32_t)d[1] << 7 | (uint32_t)d[2] << 14 | (uint32_t)d[3] << 21;
        uint32_t count = d[4] | (uint32_t)d[5] << 7;
        static uint8_t data[NR_MAX_READ + 4u];
        if (!count || count > NR_MAX_READ) return status_only(cmd, SX_BAD_VALUE);
        if (!nr_read(address, data, count)) return status_only(cmd, SX_NOR);
        uint32_t crc = crc32(data, count);
        for (uint32_t k = 0; k < 4u; ++k) data[count + k] = (uint8_t)(crc >> (8u * k));
        begin(&r, cmd, SX_OK);
        for (uint32_t k = 0; k < 6u; ++k) put(&r, d[k]);
        put_packed(&r, data, count + 4u); /* data then its CRC-32 (little endian) */
        return finish(&r);
    }
    default: return status_only(cmd, SX_UNKNOWN);
    }
}
uint8_t sx_preview_slot(void) { return preview_slot; }
uint16_t sx_preview(uint32_t now, uint8_t *root) {
    if (!preview_mask || now - preview_at >= SX_PREVIEW_MS) return 0u;
    if (root) *root = preview_root;
    return preview_mask;
}
