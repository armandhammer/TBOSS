/* SPDX-License-Identifier: GPL-3.0-or-later - controls stress on the real i2c/input/controls/settings code.
 * 1. tc_i2c_read against a register fake that follows the F407 master-receiver sequence
 *    but injects NACKs, bus errors, arbitration loss, stalls, garbage flags and a frozen
 *    clock at random: every read ends within the poll bound, reports a valid status, has
 *    data only when OK, writes nothing but the four addresses and register 0xAA (no client
 *    configuration), leaves interrupts unmasked, and once latched touches no register.
 * 2. tc_bus_clear with SDA/SCL held at random: at most nine clocks, never a START (SDA
 *    never falls while SCL is driven high), always returns.
 * 3. tc_poll with random button bitmaps, single-sample glitches, invalid/NACK/stale
 *    samples and 10..30 ms poll spacing: never a press that did not happen; with clean
 *    samples every press held 40 ms counts exactly once.
 * 4. Random presses on all 43 buttons drive the real menus, panel, presets (settings.c on a
 *    fake sector 11): every setting stays in range and the layout always fits.
 * 5. The buttons keep working when tb_millis wraps (2^32 ms, 49.7 days of uptime).
 * Fixed seeds. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t seed = 0x6A09E667u;
static uint32_t rnd(uint32_t n) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return n ? seed % n : 0u;
}
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    assert(!(offset & 3u) && offset + 4u * count <= FS_SIZE);
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
static uint32_t i2c_ok, i2c_nack, i2c_latched, menu_presses, menu_records;
/* ---- 1. the transaction engine on a faulty bus ---- */
enum { F_IDLE, F_START, F_ADDR, F_TX, F_BTF, F_RESTART, F_RADDR, F_RX, F_DATA, F_STOPPING };
static struct {
    uint32_t phase, cr1, time, accesses, faults, nack_rate, fault_rate, data_left;
    uint8_t address;
    bool frozen, locked, read;
} f;
static uint32_t fake_read(void *ctx, enum tc_reg r) {
    (void)ctx; f.accesses++;
    if (f.fault_rate && !rnd(f.fault_rate)) { /* garbage, an error flag, or no progress */
        f.faults++;
        switch (rnd(4u)) {
        case 0: return rnd(65536u);
        case 1: return r == TC_SR1 ? (uint32_t)(TC_BERR << rnd(3u)) : rnd(4u);
        case 2: return r == TC_SR1 ? TC_AF : 0u;
        default: return 0u;
        }
    }
    switch (r) {
    case TC_CR1: return f.cr1;
    case TC_SR1:
        switch (f.phase) {
        case F_START: case F_RESTART: return TC_SB;
        case F_ADDR: case F_RADDR: return f.nack_rate && !rnd(f.nack_rate) ? TC_AF : TC_ADDR;
        case F_TX: return TC_TXE;
        case F_BTF: return TC_TXE | TC_BTF;
        case F_RX: f.phase = F_DATA; return TC_BTF;
        case F_DATA: return TC_BTF;
        default: return 0u;
        }
    case TC_SR2: {
        uint32_t v = f.phase == F_IDLE ? 0u : TC_BUSY | TC_MSL;
        if (f.phase == F_ADDR) f.phase = F_TX;
        else if (f.phase == F_RADDR) f.phase = F_RX;
        else if (f.phase == F_STOPPING) { f.phase = F_IDLE; v = 0u; f.cr1 &= ~(uint32_t)(TC_START | TC_STOP); }
        return v;
    }
    case TC_DR:
        if (f.data_left && !--f.data_left) f.phase = F_STOPPING;
        return rnd(16u) ? rnd(256u) : 256u + rnd(256u);
    }
    return 0u;
}
static void fake_write(void *ctx, enum tc_reg r, uint32_t v) {
    (void)ctx; f.accesses++;
    if (r == TC_CR1) {
        assert(!(v & ~(uint32_t)(TC_PE | TC_ACK | TC_POS | TC_STOP | TC_START)));
        f.cr1 = v & ~(uint32_t)TC_START;
        if (v & TC_START) f.phase = f.phase == F_BTF ? F_RESTART : F_START;
        if (v & TC_STOP) { if (f.phase == F_DATA || f.phase == F_RX) f.data_left = 2u; else f.phase = F_STOPPING; }
    } else if (r == TC_DR) {
        /* Only the client address (write, then read) and the button register: never a
         * configuration write to the touch controllers. */
        uint8_t a = (uint8_t)(v >> 1);
        bool address = v != 0xAAu && v <= 0xFFu && (a == 0x35u || a == 0x36u || a == 0x38u || a == 0x39u);
        assert(address || v == 0xAAu);
        if (address) { f.address = a; f.read = v & 1u; f.phase = f.read ? F_RADDR : F_ADDR; }
        else f.phase = F_BTF;
    } else if (r == TC_SR1) {
        assert(v == (0xFFFFu & ~(uint32_t)TC_AF)); /* clearing AF only */
    } else assert(false);
}
static uint32_t fake_micros(void *ctx) { (void)ctx; if (!f.frozen) f.time += rnd(40u); return f.time; }
static uint32_t fake_lock(void *ctx) { (void)ctx; assert(!f.locked); f.locked = true; return 0u; }
static void fake_unlock(void *ctx, uint32_t m) { (void)ctx; (void)m; assert(f.locked); f.locked = false; }
static void transactions(void) {
    static const uint8_t clients[4] = {0x35, 0x36, 0x38, 0x39};
    uint32_t ok = 0, latched = 0, statuses[8] = {0};
    for (uint32_t round = 0; round < 20000u; ++round) {
        tc_bus b; memset(&f, 0, sizeof f);
        f.cr1 = TC_PE; f.fault_rate = rnd(3u) ? 0u : 4u + rnd(200u); f.nack_rate = rnd(4u) ? 0u : 1u + rnd(5u);
        f.frozen = !rnd(20u);
        tc_bus_init(&b, (tc_io){0, fake_read, fake_write, fake_micros, fake_lock, fake_unlock});
        for (uint32_t i = 0; i < 8u; ++i) {
            uint32_t before = f.accesses;
            uint8_t a = clients[rnd(4u)];
            bool was_latched = b.latched;
            struct bi_reply r = tc_i2c_read(&b, a, 0xAA, 7u, 9u, BI_BUDGET_US);
            uint32_t used = f.accesses - before;
            assert(!f.locked);                                  /* interrupts restored */
            assert(r.status <= BI_BLOCKED && r.generation == 7u && r.timestamp_ms == 9u);
            assert(r.status == BI_OK ? r.count == 2u : (r.count == 0u && !r.data[0] && !r.data[1]));
            assert(used <= 3u * 4096u + 64u);                   /* bounded even with a frozen clock */
            if (was_latched) assert(!used && r.status == BI_BLOCKED);
            if (r.status == BI_OK) assert(!f.frozen || used < 4096u);
            statuses[r.status]++;
            ok += r.status == BI_OK; latched += b.latched && !was_latched;
            if (!b.latched && f.phase != F_IDLE) { f.phase = F_IDLE; f.cr1 = TC_PE; } /* a fresh bus for the next read */
        }
    }
    assert(ok >= 1000u && latched >= 100u && statuses[BI_NACK] >= 100u && statuses[BI_TIMEOUT] >= 10u);
    i2c_ok = ok; i2c_nack = statuses[BI_NACK]; i2c_latched = latched;
}
/* ---- 2. bus clear with stuck lines ---- */
static struct { bool scl_out, sda_out; uint32_t calls, clocks, sda_hold, scl_hold; } pins;
static void pin_scl(void *c, bool high) {
    (void)c; pins.calls++;
    if (high && !pins.scl_out) { pins.clocks++; if (pins.sda_hold) pins.sda_hold--; }
    pins.scl_out = high;
}
static void pin_sda(void *c, bool high) {
    (void)c; pins.calls++;
    if (!high && pins.sda_out) assert(!pins.scl_out); /* SDA falls only with SCL low: never a START */
    pins.sda_out = high;
}
static bool read_scl(void *c) { (void)c; pins.calls++; if (pins.scl_hold) { pins.scl_hold--; return false; } return pins.scl_out; }
static bool read_sda(void *c) { (void)c; pins.calls++; return pins.sda_out && !pins.sda_hold; }
static void delay(void *c, uint32_t us) { (void)c; (void)us; pins.calls++; }
static void bus_clear(void) {
    const tc_pins p = {0, pin_scl, pin_sda, read_scl, read_sda, delay};
    for (uint32_t round = 0; round < 20000u; ++round) {
        memset(&pins, 0, sizeof pins);
        pins.scl_out = pins.sda_out = true;
        pins.sda_hold = rnd(3u) ? rnd(12u) : (rnd(2u) ? 1000u : 0u);  /* client holds SDA for n clocks (or for good) */
        pins.scl_hold = rnd(4u) ? 0u : rnd(2u) ? rnd(300u) : 100000u;   /* stretching, or SCL stuck low */
        uint32_t result = tc_bus_clear(&p);
        assert((result & 0xFFu) <= 9u && pins.clocks <= 11u && pins.calls < 2000u);
        if (!(result & (TC_CLEAR_SCL_STUCK | TC_CLEAR_FAILED))) assert(pins.sda_out && pins.scl_out); /* bus released */
    }
}
/* ---- 3. debounce: no phantom presses; clean presses count once ---- */
static tp_engine engine;
static uint16_t truth[4], glitch[4];
static uint32_t client_status[4];
static bool stale_reply;
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; assert(reg == 0xAAu && budget == BI_BUDGET_US);
    uint32_t i = a == 0x35u ? 0u : a == 0x36u ? 1u : a == 0x38u ? 2u : 3u;
    uint16_t raw = (uint16_t)(truth[i] ^ glitch[i]);
    struct bi_reply r = {client_status[i], client_status[i] == BI_OK ? 2u : 0u, stale_reply ? gen - 1u : gen, stamp, 300u,
                         {(uint8_t)raw, (uint8_t)(raw >> 8)}};
    return r;
}
static uint32_t logical_of(uint32_t client, uint32_t bit) {
    static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
    uint32_t source, index, logical, aliases;
    return tc_decode(addresses[client], (uint8_t)bit, &source, &index, &logical, &aliases) ? logical : UINT32_MAX;
}
static void quiet_dispatch(void) {
    /* Keep presses from changing anything that matters for this part. */
    tc_state.ui = TC_UI_PLAY; tc_state.arrows = TC_ARROWS_TEMPO;
}
static void debounce(bool faults) {
    uint32_t now = 1000u, edges[43] = {0}, held_since[4][16];
    tc_init(&tc_state); quiet_dispatch();
    engine.layout = tc_layout(&tc_state);
    memset(truth, 0, sizeof truth); memset(glitch, 0, sizeof glitch); memset(client_status, 0, sizeof client_status);
    for (uint32_t c = 0; c < 4u; ++c) for (uint32_t b = 0; b < 16u; ++b) held_since[c][b] = now;
    stale_reply = false;
    uint32_t before[43];
    for (uint32_t step = 0; step < 60000u; ++step) {
        /* True button changes: each state lasts at least 40 ms. */
        for (uint32_t c = 0; c < 4u; ++c)
            for (uint32_t b = 0; b < 16u; ++b)
                if (now - held_since[c][b] >= 40u && !rnd(30u)) {
                    truth[c] ^= (uint16_t)(1u << b); held_since[c][b] = now;
                    uint32_t l = logical_of(c, b);
                    if ((((uint32_t)truth[c] >> b) & 1u) != 0u && l != UINT32_MAX) edges[l]++;
                }
        if (faults) {
            for (uint32_t c = 0; c < 4u; ++c) {
                /* One sample wrong, never two in a row: two wrong samples 20 ms apart are a
                 * real press as far as anyone can tell. */
                /* Nor right after a bad sample: the sample after one is trusted as the new
                 * baseline, so a wrong value there would be trusted too. */
                bool after_bad = client_status[c] != BI_OK || stale_reply;
                glitch[c] = !glitch[c] && !after_bad && !rnd(10u) ? (uint16_t)rnd(65536u) : 0u;
            }
            bool was_stale = stale_reply;
            stale_reply = !rnd(50u) && !was_stale;
            for (uint32_t c = 0; c < 4u; ++c) {
                client_status[c] = rnd(12u) ? BI_OK : 1u + rnd(BI_BLOCKED);
                if (glitch[c] || was_stale) client_status[c] = BI_OK; /* a glitch is a good read with a wrong value */
            }
            if (stale_reply) memset(glitch, 0, sizeof glitch);
        }
        memcpy(before, tc_state.presses, sizeof before);
        (void)tc_poll(&tc_state, &engine, reader, NULL, now);
        quiet_dispatch();
        for (uint32_t l = 0; l < 43u; ++l) {
            if (tc_state.presses[l] > edges[l])
                fprintf(stderr, "phantom: faults %d step %u logical %u presses %u edges %u\n", faults, step, l,
                        tc_state.presses[l], edges[l]);
            assert(tc_state.presses[l] <= edges[l]); /* never a phantom press */
        }
        now += faults ? 10u + rnd(21u) : 10u;
    }
    if (!faults) {
        /* Clean samples: every press counted exactly once (the edges of the last 40 ms may still be pending). */
        uint32_t missing = 0;
        for (uint32_t l = 0; l < 43u; ++l) missing += edges[l] - tc_state.presses[l];
        assert(missing <= 43u);
    }
}
/* ---- 4. button mashing through the real menus ---- */
static uint32_t step_screen_polls; /* 1.5.0: polls spent on the step screen */
static void settings_in_range(void) {
    const tc_controls *c = &tc_state;
    assert(c->ui <= TC_UI_STEPS && c->menu < TC_MENU_ITEMS && c->group < TC_GROUPS);
    /* 1.5.0: the step screen's lane, page, cursor and held step; every lane's settings. */
    assert(c->lane < LN_LANES && c->step_page < 2u && c->step_cursor < LN_STEPS && (c->step_held == LN_NONE || c->step_held < LN_STEPS));
    for (uint32_t i = 0; i < LN_LANES; ++i)
        assert(ln.lanes[i].kind < LN_KINDS && ln.lanes[i].channel < LN_CHANNELS && ln_length(&ln.lanes[i]) >= 1u &&
               ln_rate(&ln.lanes[i]) < LN_RATES && ln.lanes[i].mute <= 1u);
    if (c->ui == TC_UI_STEPS) step_screen_polls++;
    if (c->ui == TC_UI_EDIT) assert(c->edit_value < tc_menu_values(c->menu));
    for (uint8_t item = 0; item < TC_MENU_ITEMS; ++item)
        if (item != TC_MENU_SCALE && item != TC_MENU_LOAD && item != TC_MENU_SAVE)
            assert(tc_menu_value(c, item) < tc_menu_values(item));
    assert(c->tonic < 12u && tc_scale_defined(c->scale) && c->chord_octaves && c->chord_octaves <= 0x7Fu);
    /* 1.4.0: a pivot target in range; capturing and recording never both; at most 12 captured. */
    uint8_t roots[12]; uint16_t tones[12];
    assert(c->pivot < TC_PIVOTS && c->capture_slot < TC_CHORD_SETS && !(c->capturing && c->recording));
    assert(tc_captured(roots, tones) <= 12u);
    tp_layout l = tc_layout(c);
    assert(!memcmp(&l, &engine.layout, sizeof l)); /* the engine plays what the controls show */
    tp_notes n;
    for (uint32_t k = 0; k < TS_KEYS; ++k) assert(tc_notes(l, k, c->chord[k % 48u], &n));
}
static void mash(void) {
    static const uint8_t nav_bits[5] = {8, 7, 6, 1, 3}; /* left, up, centre, down, right */
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
    memset(&engine, 0, sizeof engine); engine.layout = tc_layout(&tc_state);
    memset(truth, 0, sizeof truth); memset(glitch, 0, sizeof glitch); memset(client_status, 0, sizeof client_status);
    stale_reply = false;
    uint32_t now = 1000u, presses = 0, presets = 0;
    for (uint32_t step = 0; step < 200000u; ++step) {
        /* One button at a time (sometimes two) for 50 ms, mostly the navigation buttons;
         * every 15 s a 4 s pause (the settings save 3 s after the last change). */
        if (step % 1500u >= 1100u) memset(truth, 0, sizeof truth);
        else if (step % 8u == 0u) {
            memset(truth, 0, sizeof truth);
            uint32_t c = rnd(2u) ? 1u : rnd(4u);
            truth[c] = (uint16_t)(1u << (c == 1u && rnd(4u) ? nav_bits[rnd(5u)] : rnd(16u)));
            if (!rnd(10u)) truth[rnd(4u)] |= (uint16_t)(1u << rnd(16u));
        } else if (step % 8u == 5u) memset(truth, 0, sizeof truth);
        /* 1.5.0: now and then a player goes to Settings > Sequence > Steps (a random walk seldom gets
         * that deep): the mashing goes on from there, on the step screen and back out. */
        if (step % 9000u == 4000u) { tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_SEQUENCE; tc_state.menu = TC_MENU_STEPS; }
        presets |= tc_presets_present;
        (void)tc_poll(&tc_state, &engine, reader, NULL, now);
        st_requests(&tc_state, &engine);
        st_service(&tc_state, now);
        settings_in_range();
        now += 10u;
    }
    for (uint32_t l = 0; l < 43u; ++l) presses += tc_state.presses[l];
    assert(presses >= 10000u && st_records() >= 10u && presets && step_screen_polls >= 100u);
    menu_presses = presses; menu_records = st_records();
}
/* ---- 5. across the millisecond wrap ---- */
static void wrap(void) {
    tc_init(&tc_state); memset(&engine, 0, sizeof engine); engine.layout = tc_layout(&tc_state);
    memset(truth, 0, sizeof truth); memset(glitch, 0, sizeof glitch); memset(client_status, 0, sizeof client_status);
    stale_reply = false;
    uint32_t now = UINT32_MAX - 5000u; /* 5 s before the wrap, 10 s of polls */
    for (uint32_t i = 0; i < 1000u; ++i, now += 10u) {
        truth[3] = (i / 10u) % 2u ? (uint16_t)(1u << 3) : 0u; /* root C: 100 ms down, 100 ms up */
        (void)tc_poll(&tc_state, &engine, reader, NULL, now);
    }
    assert(!tc_state.sampler.stopped && tc_state.presses[19] == 50u && tc_state.releases[19] >= 49u);
}
int main(void) {
    wrap();
    transactions();
    bus_clear();
    debounce(false);
    debounce(true);
    mash();
    printf("PASS: controls stress: 160000 I2C reads under random NACK/bus error/arbitration/stall/garbage/frozen "
           "clock (%u OK, %u NACK, %u latched) are bounded, valid, write only the client addresses and 0xAA, then "
           "touch nothing once latched; bus clear with stuck lines: <= 9 clocks, never a START; debounce under "
           "glitches, invalid/stale samples and 10..30 ms polls: no phantom press, clean presses counted once; "
           "%u random presses through every menu (%u records saved, presets used) keep every setting (the step screen and every lane too) in range and "
           "the layout valid; buttons keep working across the 2^32 ms wrap\n", i2c_ok, i2c_nack, i2c_latched, menu_presses, menu_records);
    return 0;
}
