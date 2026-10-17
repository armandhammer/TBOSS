/* SPDX-License-Identifier: GPL-3.0-or-later - SysEx protocol v1 and USB-MIDI input on the real
 * sysex.c/settings.c/midi_in.c with a fake sector 11 and a fake 16 MiB NOR. */
#include "../../src/tb/sysex.h"
#include "../../src/tb/version.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
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
static bool nor_here = true;
static uint8_t nor_byte(uint32_t a) { return (uint8_t)(a * 7u + (a >> 8) * 13u + (a >> 16)); }
bool nr_present(void) { return nor_here; }
void nr_id(uint8_t id[3]) { id[0] = 0xEF; id[1] = 0x40; id[2] = 0x18; }
uint32_t nr_size(void) { return nor_here ? 1u << 24 : 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) {
    if (!nor_here || !n || n > NR_MAX_READ || a >= nr_size() || n > nr_size() - a) return false;
    for (uint32_t i = 0; i < n; ++i) out[i] = nor_byte(a + i);
    return true;
}
static tp_engine engine;
static uint8_t msg[300];
static uint32_t now = 1000u;
/* F0 7D 54 42 cmd data sum F7 */
static uint32_t request(uint8_t cmd, const uint8_t *data, uint32_t n) {
    uint32_t len = 0, sum = cmd;
    msg[len++] = 0xF0; msg[len++] = 0x7D; msg[len++] = 0x54; msg[len++] = 0x42; msg[len++] = cmd;
    for (uint32_t i = 0; i < n; ++i) { msg[len++] = data[i]; sum += data[i]; }
    msg[len++] = (uint8_t)((128u - sum % 128u) % 128u); msg[len++] = 0xF7;
    return sx_handle(msg, len, &tc_state, &engine, now);
}
/* A well formed reply to cmd; returns its status, data at sx_out[6..]. */
static uint8_t reply(uint32_t len, uint8_t cmd) {
    assert(len >= 8u && sx_out[0] == 0xF0 && sx_out[1] == 0x7D && sx_out[2] == 0x54 && sx_out[3] == 0x42);
    assert(sx_out[4] == (cmd | 0x40u) && sx_out[len - 1u] == 0xF7);
    uint32_t sum = 0;
    for (uint32_t i = 4; i < len - 1u; ++i) { assert(sx_out[i] < 0x80u); sum += sx_out[i]; }
    assert(sum % 128u == 0u);
    return sx_out[5];
}
static void packing(void) {
    uint8_t in[200], packed[240], out[200];
    for (uint32_t n = 0; n <= 200u; ++n) {
        for (uint32_t i = 0; i < n; ++i) in[i] = (uint8_t)(i * 37u + n);
        uint32_t p = sx_pack(in, n, packed);
        assert(p == n + (n + 6u) / 7u);
        for (uint32_t i = 0; i < p; ++i) assert(packed[i] < 0x80u);
        assert(sx_unpack(packed, p, out) == n && !memcmp(in, out, n));
    }
}
static void framing(void) {
    uint8_t d[1] = {0};
    assert(!sx_handle((const uint8_t *)"\xF0\x7E\x00\x06\x01\xF7", 6, &tc_state, &engine, now)); /* not ours */
    uint32_t len = request(SX_IDENTIFY, NULL, 0);
    assert(reply(len, SX_IDENTIFY) == SX_OK);
    assert(sx_out[6] == SX_VERSION && sx_out[8] == TC_USER_SCALES && sx_out[9] == TC_PRESETS && sx_out[11] == 1u);
    uint8_t info[7]; assert(sx_unpack(&sx_out[12], 8, info) == 7u);
    assert(info[0] == 0xEF && info[1] == 0x40 && info[2] == 0x18 && info[6] == 1u); /* 16 MiB */
    assert(sx_out[7] == 3u && sx_out[20] == TB_VERSION_MAJOR && sx_out[21] == TB_VERSION_MINOR &&
           sx_out[22] == TB_VERSION_PATCH && sx_out[24] == 0xF7u); /* stage 3, then the version */
    len = request(SX_IDENTIFY, NULL, 0); msg[5] ^= 1u;
    assert(reply(sx_handle(msg, 7, &tc_state, &engine, now), SX_IDENTIFY) == SX_CHECKSUM);
    assert(reply(request(SX_IDENTIFY, d, 1), SX_IDENTIFY) == SX_LENGTH);
    assert(reply(request(0x30, NULL, 0), 0x30) == SX_UNKNOWN);
    assert(!request(SX_IDENTIFY | 0x40u, NULL, 0)); /* a reply echoed back: ignored */
}
static void scales(void) {
    uint8_t d[15] = {0, 0x55, 0x15, 'L','y','d','i','a','n',' ','2', 0,0,0,0}; /* 0xAD5: Lydian */
    assert(reply(request(SX_SET_SCALE, d, 15), SX_SET_SCALE) == SX_OK && sx_out[6] == 0u);
    assert(tc_user_scale_get(0)->mask == 0xAD5u && !strcmp(tc_user_scale_get(0)->name, "LYDIAN 2"));
    assert(tc_state.last_action == TC_REMOTE);
    uint8_t slot = 0;
    uint32_t len = request(SX_GET_SCALE, &slot, 1);
    assert(reply(len, SX_GET_SCALE) == SX_OK && sx_out[6] == 0 && sx_out[7] == 0x55 && sx_out[8] == 0x15);
    assert(!memcmp(&sx_out[9], "LYDIAN 2\0\0\0\0", 12));
    d[1] = 0x03; d[2] = 0; /* two notes */
    assert(reply(request(SX_SET_SCALE, d, 15), SX_SET_SCALE) == SX_BAD_VALUE);
    d[0] = TC_USER_SCALES; d[1] = 0x55; d[2] = 0x15;
    assert(reply(request(SX_SET_SCALE, d, 15), SX_SET_SCALE) == SX_BAD_VALUE); /* no slot 13 */
    d[0] = 3;
    assert(reply(request(SX_SET_SCALE, d, 14), SX_SET_SCALE) == SX_LENGTH);
    /* Select it like pressing U1; an empty slot is refused. */
    slot = TC_SCALES;
    assert(reply(request(SX_SELECT_SCALE, &slot, 1), SX_SELECT_SCALE) == SX_OK);
    assert(tc_state.scale == TC_SCALES && engine.layout.mode == TC_SCALES);
    slot = TC_SCALES + 2u;
    assert(reply(request(SX_SELECT_SCALE, &slot, 1), SX_SELECT_SCALE) == SX_BAD_VALUE && tc_state.scale == TC_SCALES);
    /* Persisted: a fresh boot restores it. */
    (void)tc_set_user_scale(0, 0, NULL); tc_controls c; tc_init(&c);
    assert(st_load(&c) || 1); assert(tc_user_scale_get(0)->mask == 0xAD5u);
}
static void records(void) {
    uint8_t ks[2] = {ST_LIVE, 0};
    uint32_t len = request(SX_GET_RECORD, ks, 2);
    enum { PACKED = 160u + 23u }; /* 40 words, 7-bit packed */
    assert(reply(len, SX_GET_RECORD) == SX_OK && len == 8u + 2u + PACKED);
    uint8_t packed[PACKED]; memcpy(packed, &sx_out[8], PACKED);
    uint8_t set[2 + PACKED] = {ST_PRESET, 4};
    memcpy(&set[2], packed, PACKED);
    assert(reply(request(SX_SET_RECORD, set, sizeof set), SX_SET_RECORD) == SX_OK && (tc_presets_present & 0x10u));
    ks[0] = ST_PRESET; ks[1] = 4;
    len = request(SX_GET_RECORD, ks, 2);
    assert(reply(len, SX_GET_RECORD) == SX_OK && !memcmp(&sx_out[8], packed, PACKED));
    { /* A 20-word record from firmware before 0.6.0 is accepted (the new settings keep their values). */
        uint8_t bytes[160], old_packed[92], v2[2 + 92] = {ST_PRESET, 6};
        assert(sx_unpack(packed, PACKED, bytes) == 160u && sx_pack(bytes, 80u, old_packed) == 92u);
        memcpy(&v2[2], old_packed, 92);
        assert(reply(request(SX_SET_RECORD, v2, sizeof v2), SX_SET_RECORD) == SX_OK && (tc_presets_present & 0x40u));
        assert(reply(request(SX_SET_RECORD, v2, sizeof v2 - 1u), SX_SET_RECORD) == SX_LENGTH);
    }
    ks[1] = 5; assert(reply(request(SX_GET_RECORD, ks, 2), SX_GET_RECORD) == SX_ABSENT);
    ks[0] = 9; assert(reply(request(SX_GET_RECORD, ks, 2), SX_GET_RECORD) == SX_BAD_VALUE);
    /* A live record with tonic 12 is refused; tonic 5 takes effect at once. */
    uint8_t bytes[80]; assert(sx_unpack(packed, 92, bytes) == 80u);
    bytes[0] = 12; set[0] = ST_LIVE; set[1] = 0; (void)sx_pack(bytes, 80, &set[2]);
    assert(reply(request(SX_SET_RECORD, set, sizeof set), SX_SET_RECORD) == SX_BAD_VALUE && tc_state.tonic != 12u);
    bytes[0] = 5; (void)sx_pack(bytes, 80, &set[2]);
    assert(reply(request(SX_SET_RECORD, set, sizeof set), SX_SET_RECORD) == SX_OK && tc_state.tonic == 5u);
    assert(engine.layout.tonic == 5u);
}
static void preview(void) {
    uint8_t d[4] = {0x55, 0x15, 2, 0};
    assert(!request(SX_PREVIEW, d, 3));
    uint8_t root = 0; assert(sx_preview(now, &root) == 0xAD5u && root == 2u);
    assert(sx_preview(now + SX_PREVIEW_MS - 1u, NULL) && !sx_preview(now + SX_PREVIEW_MS, NULL));
    assert(sx_preview_slot() == 0xFFu); /* 3 bytes: no slot */
    d[3] = 2u; assert(!request(SX_PREVIEW, d, 4) && sx_preview(now, NULL) == 0xAD5u && sx_preview_slot() == 2u);
    d[3] = TC_USER_SCALES; assert(!request(SX_PREVIEW, d, 4) && sx_preview_slot() == 0xFFu); /* not a slot */
    d[0] = d[1] = 0; assert(!request(SX_PREVIEW, d, 3) && !sx_preview(now, NULL));
}
static void nor(void) {
    uint32_t a = 0x123456u, n = 2048u;
    uint8_t d[6] = {(uint8_t)(a & 127u), (uint8_t)(a >> 7 & 127u), (uint8_t)(a >> 14 & 127u), (uint8_t)(a >> 21),
                    (uint8_t)(n & 127u), (uint8_t)(n >> 7)};
    uint32_t len = request(SX_NOR_READ, d, 6);
    assert(reply(len, SX_NOR_READ) == SX_OK && !memcmp(&sx_out[6], d, 6));
    static uint8_t data[2052];
    uint32_t packed = len - 14u;
    assert(packed == 2052u + (2052u + 6u) / 7u && sx_unpack(&sx_out[12], packed, data) == 2052u);
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; ++i) {
        assert(data[i] == nor_byte(a + i));
        crc ^= data[i];
        for (uint32_t k = 0; k < 8u; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    assert(~crc == (data[n] | (uint32_t)data[n + 1] << 8 | (uint32_t)data[n + 2] << 16 | (uint32_t)data[n + 3] << 24));
    d[4] = 1; d[5] = 16; /* 2049 */ assert(reply(request(SX_NOR_READ, d, 6), SX_NOR_READ) == SX_BAD_VALUE);
    d[4] = 0; d[5] = 0; assert(reply(request(SX_NOR_READ, d, 6), SX_NOR_READ) == SX_BAD_VALUE);
    d[0] = d[1] = d[2] = 127; d[3] = 7; d[4] = 2; d[5] = 0; /* last byte + 1 */
    assert(reply(request(SX_NOR_READ, d, 6), SX_NOR_READ) == SX_NOR);
    nor_here = false; d[3] = 0; assert(reply(request(SX_NOR_READ, d, 6), SX_NOR_READ) == SX_NOR); nor_here = true;
}
static void in(uint8_t cin, uint8_t a, uint8_t b, uint8_t c) { uint8_t p[4] = {cin, a, b, c}; tm_packet(p, now); }
static void midi_input(void) {
    tm_reset();
    /* SysEx across packets, every ending form. */
    in(4, 0xF0, 0x7D, 0x54); in(4, 0x42, 0x01, 0x7F); in(5, 0xF7, 0, 0);
    assert(tm.sysex_ready && tm.sysex_len == 7u && tm.sysex[6] == 0xF7);
    /* A second message while the first is unconsumed is dropped, the first kept. */
    in(4, 0xF0, 0x01, 0x02); in(6, 0x03, 0xF7, 0); assert(tm.sysex_dropped == 1u && tm.sysex_len == 7u);
    assert(sx_handle(tm.sysex, tm.sysex_len, &tc_state, &engine, now)); tm_sysex_consumed();
    in(4, 0xF0, 0x01, 0x02); in(7, 0x03, 0x04, 0xF7); assert(tm.sysex_ready && tm.sysex_len == 6u);
    tm_sysex_consumed();
    /* Too long: dropped whole; an unterminated message is dropped at the next F0. */
    in(4, 0xF0, 0, 0); for (uint32_t i = 0; i < 90u; ++i) in(4, 1, 2, 3); /* > TM_SYSEX_MAX */
    in(5, 0xF7, 0, 0); assert(!tm.sysex_ready && tm.sysex_dropped == 2u);
    in(4, 0xF0, 1, 2); in(4, 0xF0, 3, 4); in(6, 5, 0xF7, 0);
    assert(tm.sysex_dropped == 3u && tm.sysex_ready && tm.sysex_len == 5u); tm_sysex_consumed();
    /* Other cables are ignored. */
    in(0x14, 0xF0, 1, 2); in(0x16, 3, 0xF7, 0); assert(!tm.sysex_ready);
    /* Notes from any channel; velocity-0 On is Off; all-notes-off clears. */
    in(9, 0x90, 60, 100); in(9, 0x93, 60, 90); in(9, 0x91, 64, 1);
    assert(tm.notes[60] == 2u && tm.notes[64] == 1u);
    in(8, 0x80, 60, 0); in(9, 0x93, 60, 0); assert(!tm.notes[60]);
    in(0xB, 0xB1, 123, 0); assert(!tm.notes[64]);
    /* Clock: period smoothed over USB frame jitter; Start resets the count. */
    for (uint32_t i = 0; i < 48u; ++i) { in(0xF, 0xF8, 0, 0); now += (i & 1u) ? 21u : 20u; }
    assert(tm.ticks == 48u && tm.tick_us >= 20000u && tm.tick_us <= 21000u && tm_clock_active(now));
    in(0xF, 0xFA, 0, 0); assert(tm.ticks == 0u && tm.starts == 1u);
    now += TM_CLOCK_TIMEOUT_MS; assert(!tm_clock_active(now));
}
int main(void) {
    memset(flash, 0xFF, sizeof flash);
    tc_init(&tc_state); (void)st_load(&tc_state); engine.layout = tc_layout(&tc_state);
    packing(); framing(); scales(); records(); preview(); nor(); midi_input();
    puts("PASS: SysEx v1: 7-bit packing 0..200 bytes, framing/checksum/length/unknown/echo, identify, user scales set/get/validate/select/persist, records get/set/validate (live applies, presets stored), preview expiry and editing slot, NOR read 2 KiB with CRC and range/absence errors; MIDI in: SysEx assembly/drops, cables, notes, all-notes-off, clock period and Start");
    return 0;
}
