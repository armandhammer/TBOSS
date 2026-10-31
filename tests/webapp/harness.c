/* SPDX-License-Identifier: GPL-3.0-or-later - the real firmware SysEx handler (sysex.c, settings.c,
 * layout.c ...) behind stdin/stdout for tests/webapp/test_protocol.mjs: each input line
 * is one request in hex, each output line the reply in hex (empty: no reply).
 * Fake sector 11 and a fake 16 MiB NOR whose byte at a is (a*7 + (a>>8)*13 + (a>>16)). */
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/settings.h"
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
bool nr_present(void) { return true; }
void nr_id(uint8_t id[3]) { id[0] = 0xEF; id[1] = 0x40; id[2] = 0x18; }
uint32_t nr_size(void) { return 1u << 24; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) {
    if (!n || n > NR_MAX_READ || a >= nr_size() || n > nr_size() - a) return false;
    for (uint32_t i = 0; i < n; ++i) { uint32_t x = a + i; out[i] = (uint8_t)(x * 7u + (x >> 8) * 13u + (x >> 16)); }
    return true;
}
int main(void) {
    static char line[1024];
    static uint8_t msg[512];
    static tp_engine engine;
    memset(flash, 0xFF, sizeof flash);
    tc_init(&tc_state); (void)st_load(&tc_state); engine.layout = tc_layout(&tc_state);
    uint32_t now = 1000u;
    while (fgets(line, sizeof line, stdin)) {
        uint32_t n = 0;
        for (char *p = line; p[0] && p[1] && n < sizeof msg; p += 2) {
            unsigned v;
            if (sscanf(p, "%2x", &v) != 1) break;
            msg[n++] = (uint8_t)v;
        }
        uint32_t len = sx_handle(msg, n, &tc_state, &engine, now);
        st_requests(&tc_state, &engine); /* 2.0.6: Load / Save preset by SET_ITEM, as main.c's loop runs them */
        for (uint32_t i = 0; i < len; ++i) printf("%02x", sx_out[i]);
        printf("\n");
        now += 10u;
    }
    return 0;
}
