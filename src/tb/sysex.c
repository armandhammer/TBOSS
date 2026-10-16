/* SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard SysEx protocol v1 (see sysex.h). */
#include "sysex.h"
#include "nor.h"
#include "version.h"
#include <string.h>
uint8_t sx_out[SX_OUT_MAX];
static uint16_t preview_mask;
static uint8_t preview_root;
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
        put(&r, SX_VERSION); put(&r, 3u); put(&r, TC_USER_SCALES); put(&r, TC_PRESETS); put(&r, ST_PAYLOAD);
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
        if (n == 3u) {
            preview_mask = (uint16_t)((d[0] | (uint32_t)d[1] << 7) & 0xFFFu);
            preview_root = d[2] % 12u; preview_at = now;
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
        if (n != 2u + packed_size(4u * ST_PAYLOAD)) return status_only(cmd, SX_LENGTH);
        uint8_t bytes[4u * ST_PAYLOAD]; uint32_t p[ST_PAYLOAD];
        (void)sx_unpack(&d[2], packed_size(4u * ST_PAYLOAD), bytes);
        for (uint32_t i = 0; i < ST_PAYLOAD; ++i)
            p[i] = bytes[4u * i] | (uint32_t)bytes[4u * i + 1u] << 8 | (uint32_t)bytes[4u * i + 2u] << 16 |
                   (uint32_t)bytes[4u * i + 3u] << 24;
        uint32_t status = st_put(d[0], d[1], p, c, e);
        if (status == ST_OK) remote(c);
        begin(&r, cmd, (uint8_t)status); put(&r, d[0]); put(&r, d[1]);
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
uint16_t sx_preview(uint32_t now, uint8_t *root) {
    if (!preview_mask || now - preview_at >= SX_PREVIEW_MS) return 0u;
    if (root) *root = preview_root;
    return preview_mask;
}
