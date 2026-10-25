/* SPDX-License-Identifier: GPL-3.0-or-later - engine stress on the real engine/controls/layout code.
 * Seeded random play with board-like timing: 1 ms main-loop passes, a scan frame every
 * 12..18 ms (and outages long enough to make the scan stale), receipts delayed 0..50 ms,
 * refused writes, transfer-mode drains (as main.c's SysEx replies), TRS-only sessions on a
 * 31,250-baud queue that fills and starves, and injected faults on USB sessions (lost
 * session, a receipt that never comes, a receipt for another token, USB not ready).
 * After every pass: reference counts match the sources, exposed counts match, the receiver
 * hears exactly the exposed voices, every packet is well formed; every graceful drain and
 * every release-everything phase ends silent. Then timing checks across the millisecond
 * (2^32 ms) and microsecond (now * 1000 wraps every ~71.6 min) wrap points. 1.5.0: random step
 * patterns playing alongside (and the step screen, where the right half's pads are silent steps),
 * and a drum lane's 1/16 across the wraps. Faster pads (1.5.0): half the rounds read two scan
 * positions a pass and the engine takes their pads at once, every pass, as main.c now does.
 * The pass mirrors usb_main (src/tb/main.c). Fixed seeds: failures reproduce. */
#include "../../src/tb/controls.h"
#include "../../src/tb/serial_midi.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/sequence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now, seed;
static uint32_t rnd(uint32_t n) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return n ? seed % n : 0u;
}
/* What the receiver hears (the host on USB routes, the jack in a TRS-only session). */
static uint8_t sounding[16][128], sustained[16][128];
/* Voices a stopped session still owed: the next session sends their Note Off (again, if
 * one was in flight when it stopped). */
static uint8_t carried[16][128];
enum { USB, USB_TRS, JACK };
static uint32_t route;
static bool usb_ready = true;
/* One engine packet in flight on USB; its receipt arrives `delay` passes later. */
static struct { bool busy; uint32_t token, delay; } wire;
static uint32_t refuse, serial_token, uart_starve, frame_gap;
static bool stall_receipt, wrong_receipt, fault;
enum { TX_IDLE, TX_DRAIN, TX_SEND, TX_LINGER }; /* main.c transfer mode */
static uint32_t transfer, transfer_left;
static uint32_t packets, ons, offs, faults, owed, owed_trs, drains, settles, unexplained, outages, seq_chords, seq_order;
static uint32_t lane_strikes, lane_order[LN_LANES];
static void check_packet(const uint8_t p[4]) {
    uint32_t cin = p[0], status = p[1], ch = status & 15u;
    packets++;
    if (cin == 0x0Fu) {
        assert((status == 0xF8u || status == 0xFAu || status == 0xFBu || status == 0xFCu) && !p[2] && !p[3]);
        return;
    }
    assert(cin == TK_ON || cin == TK_OFF || cin == TK_PRESSURE || cin == TK_CC || cin == TK_CHANNEL || cin == TK_BEND);
    assert(status >> 4 == cin && p[2] < 128u && p[3] < 128u);
    if (cin == TK_ON) { assert(p[3] >= 1u && !sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++; }
    if (cin == TK_OFF) {
        assert((sounding[ch][p[2]] || carried[ch][p[2]]) && !p[3]);
        sounding[ch][p[2]] = carried[ch][p[2]] = 0u; offs++;
    }
    if (cin == TK_PRESSURE) assert(sounding[ch][p[2]]);
    if (cin == TK_CHANNEL) assert(!p[3]);
    if (cin == TK_CC) {
        assert(p[2] == 1u || p[2] == 2u || p[2] == 11u || p[2] == 74u || (p[2] >= 64u && p[2] <= 67u));
        if (p[2] >= 64u && p[2] <= 67u) { assert(p[3] == 0u || p[3] == 127u); sustained[ch][p[2]] = p[3] != 0u; }
    }
}
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && !e.pending_id);
    uint8_t bytes[3]; uint32_t n = sm_packet_bytes(p, bytes);
    assert(n >= 1u);
    if (route == JACK) { /* main.c write_packet in a TRS-only session */
        if (!sm_push(&sm_out, bytes, n)) return false;
        check_packet(p); serial_token = token; return true;
    }
    assert(!wire.busy); /* one packet in flight */
    if (!usb_ready || refuse) return false;
    if (route == USB_TRS && sm_space(&sm_out) < n) return false; /* waits for room on the jack */
    check_packet(p);
    if (route == USB_TRS) assert(sm_push(&sm_out, bytes, n));
    wire.busy = true; wire.token = token; wire.delay = rnd(8u) ? rnd(4u) : rnd(50u);
    return true;
}
static void quiet_model(void) {
    memset(sounding, 0, sizeof sounding); memset(sustained, 0, sizeof sustained); memset(carried, 0, sizeof carried);
}
static uint32_t still_carried(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += carried[c][p];
    return n;
}
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += sounding[c][p] + sustained[c][p];
    return n;
}
/* The engine's bookkeeping against the sources it serves and what the receiver heard. */
static void invariants(void) {
    static uint32_t refs[16][128];
    memset(refs, 0, sizeof refs);
    for (uint32_t k = 0; k < TP_SOURCES; ++k) {
        const tp_key *key = &e.keys[k];
        assert(key->captured.count <= TP_MAX_NOTES && key->channel < 16u);
        if (!key->admitted || key->silent) continue;
        for (uint32_t i = 0; i < key->captured.count; ++i) {
            assert(key->captured.notes[i] < 128u);
            refs[key->channel][key->captured.notes[i]]++;
        }
    }
    /* Mono: a lifted pad's notes kept until the one back has started. */
    assert(e.mono_hold.count <= TP_MAX_NOTES && e.mono_hold_ch < 16u);
    for (uint32_t i = 0; i < e.mono_hold.count; ++i) refs[e.mono_hold_ch][e.mono_hold.notes[i] & 127u]++;
    uint32_t exposed = 0;
    for (uint32_t c = 0; c < 16u; ++c) {
        uint32_t n = 0;
        for (uint32_t p = 0; p < 128u; ++p) {
            const tp_voice *v = &e.voices[c][p];
            assert(refs[c][p] < 256u && v->refs == refs[c][p]); /* never wrapped below zero, never leaked */
            n += v->exposed;
            if (!fault && e.state != TK_STOPPED) {
                if (sounding[c][p]) assert(v->exposed);
                if (v->exposed && !sounding[c][p] && !carried[c][p]) /* only the Off in flight */
                    assert(e.pending_id && e.pending_kind == TK_OFF && e.pending_channel == c && e.pending_pitch == p);
            }
        }
        assert(e.exposed[c] == n); exposed += n;
    }
    assert(tp_obligations(&e) == exposed + e.sustain_on[0] + e.sustain_on[1]);
    assert(e.state <= TK_STOPPED);
    if (e.state == TK_IDLE || e.state == TK_DONE) assert(!e.pending_id);
    if (e.state == TK_IDLE) for (uint32_t k = 0; k < TP_SOURCES; ++k) assert(!e.keys[k].admitted);
}
/* Pad readings: resting, around the Pads thresholds at their defaults (600 / 1200 / 3500) and
 * at the old fixed ones (1500 / 2500 / 4000), firm, full. */
static const uint32_t readings[] = {0u, 300u, 470u, 590u, 610u, 900u, 1150u, 1250u, 1500u, 1501u, 1800u, 2499u,
                                    2500u, 2600u, 3000u, 3500u, 4000u, 4095u};
static uint32_t pressed_limit = 10u;
static void scan_frame(bool release_all) {
    uint32_t down = 0;
    for (uint32_t k = 0; k < TS_KEYS; ++k) down += scan.values[k] > 1500u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        if (release_all) { scan.values[k] = rnd(4u) ? 0u : 300u; continue; }
        if (rnd(60u)) continue;
        uint32_t v = readings[rnd(sizeof readings / sizeof readings[0])];
        if (v > 1500u && scan.values[k] <= 1500u && down >= pressed_limit) continue;
        scan.values[k] = v;
    }
    scan.frame++; scan.last_ms = now; scan.valid = true;
}
/* Faster pads (1.5.0): one scan position's 6 pads read; true at the end of a frame. */
static bool positional;
static bool scan_position(bool release_all) {
    uint32_t down = 0, p = scan.position;
    for (uint32_t k = 0; k < TS_KEYS; ++k) down += scan.values[k] > 1500u;
    for (uint32_t b = 0; b < 6u; ++b) {
        uint32_t k = 16u * b + p;
        if (release_all) { scan.values[k] = rnd(4u) ? 0u : 300u; continue; }
        if (rnd(60u)) continue;
        uint32_t v = readings[rnd(sizeof readings / sizeof readings[0])];
        if (v > 1500u && scan.values[k] <= 1500u && down >= pressed_limit) continue;
        scan.values[k] = v;
    }
    scan.last = (uint8_t)p; scan.reads++; scan.position = (uint8_t)((p + 1u) & 15u);
    if (scan.position) return false;
    scan.frame++; scan.last_ms = now; scan.valid = true;
    return true;
}
static void random_layout(void) {
    tp_layout l = tc_layout(&tc_state);
    switch (rnd(7u)) {
    case 5: l.chords = (uint8_t)rnd(TC_CHORDS_GRID + 1u); break; /* a chord set (refused while it is empty), Full or the grid */
    case 0: l.tonic = (uint8_t)rnd(12u); break;
    case 1: do l.mode = (uint8_t)rnd(TC_ALL_SCALES); while (!tc_scale_defined(l.mode)); break;
    case 2: l.melody_layout = (uint8_t)rnd(TP_MELODY_LAYOUTS); break;
    case 3: l.fold = (uint8_t)rnd(2u); break;
    case 4: l.chord_base = (uint8_t)(TP_CHORD_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u)); break;
    default: l.melody_base = (uint8_t)(TP_MELODY_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u)); break;
    }
    (void)tc_set_layout(&tc_state, &e, l);
}
/* What the menu, panel, pedal and editor change while playing, in range, as controls.c does. */
static void random_control(void) {
    tc_controls *c = &tc_state;
    switch (rnd(40u)) {
    case 0: random_layout(); break;
    case 39: /* 1.5.0: a random step pattern; the step screen open or closed; the board's transport */
        for (uint32_t i = 0; i < LN_LANES; ++i) {
            ln_lane *l = &ln.lanes[i];
            l->kind = (uint8_t)rnd(LN_KINDS); l->channel = (uint8_t)(rnd(2u) ? 0u : rnd(LN_CHANNELS));
            ln_set_length(l, 1u + rnd(LN_STEPS)); ln_set_rate(l, rnd(LN_RATES)); l->mute = !rnd(5u);
            for (uint32_t s = 0; s < LN_STEPS; ++s) {
                memset(&l->steps[s], 0, sizeof l->steps[s]);
                if (rnd(2u)) continue;
                l->steps[s].on = true; l->steps[s].value = (uint8_t)rnd(rnd(4u) ? 48u : 128u); l->steps[s].velocity = (uint8_t)rnd(128u);
                l->steps[s].edit = (uint8_t)(rnd(4u) ? 0u : rnd(8u) | rnd(3u) << 3 | rnd(8u) << 5);
                l->steps[s].scale = (uint8_t)(rnd(5u) ? 0u : 1u + rnd(TC_ALL_SCALES));
            }
        }
        c->lane = (uint8_t)rnd(LN_LANES); c->ui = rnd(3u) ? (uint8_t)TC_UI_PLAY : (uint8_t)TC_UI_STEPS;
        if (rnd(2u)) c->transport = true;
        break;
    case 34: { /* 1.0.0: a chord set redefined (the editor), kept only while the layout still fits */
        uint8_t slot = (uint8_t)rnd(TC_CHORD_SETS);
        tc_chord_set set, before = *tc_chord_set_get(slot); memset(&set, 0, sizeof set);
        for (uint32_t i = 0; i < 12u; ++i) {
            if (!rnd(4u)) continue;
            uint16_t mask = 1u;
            for (uint32_t n = rnd(6u); n; --n) mask |= (uint16_t)(1u << (1u + rnd(11u)));
            set.column[i] = (tc_set_chord){mask, (uint8_t)rnd(TC_SET_ROOTS)};
        }
        assert(tc_chord_set_put(slot, &set));
        if (c->chord_set == slot + 1u && !tc_set_layout(c, &e, tc_layout(c))) (void)tc_chord_set_put(slot, &before);
        break;
    }
    case 35: c->lock = (uint8_t)rnd(17u); break; /* scale lock: MIDI in notes on that channel (case 28) */
    case 36: c->panic = true; c->transport = false; break;
    case 38: /* 1.4.0: a pivot target; chord capture on or off (pads and MIDI in add chords) */
        c->pivot = (uint8_t)rnd(TC_PIVOTS);
        if (!rnd(3u)) { c->capturing = !c->capturing && !c->recording; c->capture_slot = (uint8_t)rnd(TC_CHORD_SETS); }
        break;
    case 37: /* centre buttons held: chord shapes and sustain while held, Shift */
        for (uint32_t i = 0; i < TC_FUNCTIONS; ++i) c->buttons[i] = (uint8_t)rnd(TC_ACTIONS);
        c->fn_held = (uint16_t)(rnd(3u) ? 0u : rnd(1u << TC_FUNCTIONS)); c->shift_latched = !rnd(8u);
        break;
    case 30: { /* a new progression (0.11.0), sometimes with borrowed chords and edits */
        sq_clear(&sq, (uint8_t)rnd(SQ_UNIT_KINDS), c->scale);
        for (uint32_t i = 0, n = rnd(12u); i < n; ++i) {
            uint8_t sc = rnd(3u) ? c->scale : (uint8_t)rnd(TC_ALL_SCALES);
            tc_chord ed = rnd(2u) ? (tc_chord){0, 0, 0} : (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
            (void)sq_tap(&sq, (uint8_t)rnd(48u), (uint8_t)(1u + rnd(127u)), sc, ed);
        }
        break;
    }
    case 31: c->recording = !c->recording && !rnd(3u); break; /* recording: chord pad touches become steps */
    case 32: { /* a DAW's Start, Continue or Stop */
        static const uint8_t msg[3] = {0xFAu, 0xFBu, 0xFCu};
        const uint8_t p[4] = {0x0Fu, msg[rnd(3u)], 0u, 0u};
        tm_packet(p, now);
        break;
    }
    case 33: c->seq_guide = (uint8_t)rnd(2u); c->melody_vel = (uint8_t)rnd(TC_VEL_MODES); c->pedal_target = (uint8_t)rnd(TC_PEDAL_TARGETS);
        if (!rnd(3u)) c->chords_right = !c->chords_right; /* 1.5.0: the halves trade places, pads held or not */
        break;
    case 29: { /* Settings > Pads, as the menu applies it: valid values only, the session follows */
        uint32_t t = TC_PAD_TRIGGER_MIN + rnd(TC_PAD_TRIGGER_MAX - TC_PAD_TRIGGER_MIN + 1u);
        uint32_t r = TC_PAD_RELEASE_MIN + rnd(TC_PAD_RELEASE_MAX - TC_PAD_RELEASE_MIN + 1u);
        uint32_t f = TC_PAD_FULL_MIN + rnd(TC_PAD_FULL_MAX - TC_PAD_FULL_MIN + 1u);
        if (!tc_pad_settings_ok(t, r, f)) break;
        c->pad_trigger = (uint8_t)t; c->pad_release = (uint8_t)r; c->pad_full = (uint8_t)f;
        if (e.state == TK_ACTIVE) e.config = tc_pad_config(c);
        break;
    }
    case 1: c->hold = (uint8_t)!c->hold; break;
    case 2: c->arp = (uint8_t)(rnd(3u) ? 0u : rnd(TC_ARP_MODES)); break;
    case 3: c->arp_rate = (uint8_t)rnd(TC_ARP_RATES); c->arp_octaves = (uint8_t)rnd(TC_ARP_OCTAVES);
            c->arp_gate = (uint8_t)rnd(TC_ARP_GATES); break;
    case 4: c->arp_rhythm = (uint8_t)rnd(TC_RHYTHM_VALUES); c->arp_rotate = (uint8_t)rnd(TC_ROTATES); c->arp_swing = (uint8_t)rnd(TC_SWINGS);
            c->arp_sync = (uint8_t)rnd(2u); c->arp_latch = (uint8_t)rnd(2u); break;
    case 5: c->strum = (uint8_t)rnd(TC_STRUM_MODES); c->strum_speed = (uint8_t)rnd(TC_STRUM_SPEEDS); break;
    case 6: c->strum_repeat = (uint8_t)rnd(TC_REPEATS); c->strum_cutoff = (uint8_t)rnd(TC_CUTOFFS); break;
    case 7: c->feel_time = (uint8_t)rnd(TC_FEEL_TIMES); c->feel_vel = (uint8_t)rnd(TC_FEEL_VELS); break;
    case 8: c->note_repeat = (uint8_t)rnd(TC_NOTE_REPEATS); c->repeat_rhythm = (uint8_t)(rnd(2u) ? 0u : rnd(RH_EUCLIDS + 1u));
            if (!rnd(2u)) { /* 1.6.0: harmonizer, echo, mono, chance, also while pads are held and echoes run */
                c->harmony = (uint8_t)rnd(TC_HARMONIES); c->echo = (uint8_t)(rnd(2u) ? 0u : rnd(TC_ECHO_TIMES));
                c->echoes = (uint8_t)rnd(TC_ECHOES); c->mono = (uint8_t)rnd(2u); c->chance = (uint8_t)(rnd(3u) ? 0u : rnd(TC_CHANCES));
            }
            break;
    case 9: c->live_update = (uint8_t)rnd(2u); if (c->live_update && rnd(2u)) c->relayout = true; break;
    case 10: c->chord_ch = (uint8_t)rnd(16u); break;
    case 11: c->melody_ch = (uint8_t)rnd(16u); break;
    case 12: c->bass_ch = (uint8_t)rnd(16u); c->drum_ch = (uint8_t)rnd(16u); c->live_ch = (uint8_t)rnd(TC_LIVE_CHANNELS); break;
    case 13: c->at_chord = (uint8_t)rnd(TC_AT_MODES); c->at_melody = (uint8_t)rnd(TC_AT_MODES); break;
    case 14: c->vel_mode = (uint8_t)rnd(TC_VEL_MODES); break;
    case 15: c->pedal_mode = (uint8_t)rnd(TC_PEDAL_MODES); break;
    case 16: { /* the pedal moves (tc_pedal's outputs) */
        bool down = rnd(2u) != 0u;
        c->sustain = tc_pedal_cc(c->pedal_mode) != 0u && down;
        c->pedal_down_note = c->pedal_mode == TC_PEDAL_NOTE && down;
        if (c->pedal_mode == TC_PEDAL_HOLD) c->hold = down;
        break;
    }
    case 17: c->pedal_note = (uint8_t)rnd(128u); break;
    case 18: c->clock_out = (uint8_t)rnd(2u); break;
    case 19: c->transport = !c->transport; break;
    case 20: c->tempo = (uint8_t)rnd(TC_TEMPOS); break;
    case 21: c->chord_mute ^= (uint64_t)1u << rnd(48u); break;
    case 22: c->strum_off ^= (uint64_t)1u << rnd(48u); break;
    case 23: c->chord_voicing = (uint8_t)rnd(TC_VOICINGS); if (c->live_update) c->relayout = true; break;
    case 24: c->chord_spread = (uint8_t)rnd(TC_SPREADS); c->chord_octaves = (uint8_t)(1u + rnd(127u)); break;
    case 25: c->capo = (uint8_t)rnd(TC_CAPOS); c->voice_lead = (uint8_t)rnd(2u); break;
    case 26: { /* a chord edit, refused like edit_chord when it leaves the MIDI range */
        uint32_t k = rnd(48u); tc_chord edit = {(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
        tp_notes n;
        if (tc_notes(tc_layout(c), k, edit, &n)) c->chord[k] = edit;
        break;
    }
    case 27: c->live_in = (uint8_t)rnd(TC_LIVE_INPUTS); break;
    case 28: { /* MIDI in: notes for the lights and the live chord */
        uint8_t on = (uint8_t)rnd(2u);
        uint8_t p[4] = {(uint8_t)(on ? 0x09u : 0x08u), (uint8_t)((on ? 0x90u : 0x80u) | rnd(16u)), (uint8_t)rnd(128u),
                        (uint8_t)rnd(128u)};
        tm_packet(p, now);
        break;
    }
    default: c->arrows = (uint8_t)rnd(TC_ARROWS_MODES); break;
    }
}
static uint32_t clock_period, clock_next; /* incoming MIDI clock, 0 = none */
static void midi_clock(void) {
    if (!clock_period || --clock_next) return;
    const uint8_t tick[4] = {0x0Fu, 0xF8u, 0u, 0u};
    tm_packet(tick, now);
    clock_next = clock_period - 2u + rnd(5u); /* USB frames: +-2 ms of jitter */
}
static void inject_fault(void) {
    fault = true; faults++;
    switch (rnd(4u)) {
    case 0: tp_loss(&e); break;              /* bus reset / unplug / suspend */
    case 1: stall_receipt = true; break;     /* no receipt: 2 s pending -> stop */
    case 2: wrong_receipt = true; break;     /* a receipt for another token */
    default: usb_ready = false; break;       /* not ready: loss */
    }
}
/* One 1 ms main-loop pass. */
static void pass(bool release_all, bool chaos) {
    if (wire.busy && !stall_receipt && !wire.delay--) { /* tud_task: the receipt */
        wire.busy = false;
        tp_complete(&e, wrong_receipt ? wire.token + 1u : wire.token, 4u, true, now);
    }
    if (route != JACK && !usb_ready) tp_loss(&e); /* observe_loss (USB events never end a TRS-only session) */
    midi_clock();
    bool fresh = false;
    if (positional) { /* two positions a pass; frame_gap: an outage */
        if (frame_gap) frame_gap--;
        else for (uint32_t i = 0; i < 2u && !fresh; ++i) fresh = scan_position(release_all);
    } else if (!frame_gap--) { scan_frame(release_all); fresh = true; frame_gap = 12u + rnd(7u); }
    bool ready = route == JACK || usb_ready;
    if (e.state == TK_IDLE) { if (fresh && transfer == TX_IDLE) (void)tc_arm(&e, &scan, now, ready); }
    else if (fresh || positional) (void)tc_frame(&e, &scan, now);
    uint32_t errors = e.errors;
    tc_step(&e, now, ready, writer);
    if (serial_token) { tp_complete(&e, serial_token, 4u, true, now); serial_token = 0u; }
    if (e.errors != errors) { /* the session stopped: the next one owes its Note Offs */
        if (!fault) unexplained++;
        uint32_t left = 0;
        for (uint32_t c = 0; c < 16u; ++c) for (uint32_t q = 0; q < 128u; ++q)
            if (e.voices[c][q].exposed) { carried[c][q] = 1u; left++; }
        owed += left;
        if (route == USB_TRS) owed_trs += left;
    }
    if (e.state == TK_IDLE && !e.issued) { /* between sessions: faults end, the route may change */
        if (fault) { fault = stall_receipt = wrong_receipt = false; wire.busy = false; usb_ready = true; }
        if (fresh && transfer == TX_IDLE && !rnd(6u)) route = rnd(3u);
    }
    /* The serial interrupt sends ~3.1 bytes per ms, unless starved. */
    uint32_t rate = uart_starve ? (uart_starve--, 0u) : 3u + (rnd(8u) == 0u);
    for (uint8_t b; rate && sm_pop(&sm_out, &b); --rate) {}
    if (refuse) refuse--;
    if (transfer == TX_DRAIN && e.state != TK_ACTIVE && e.state != TK_DRAINING && !e.pending_id && !wire.busy) {
        if (!fault && !tp_obligations(&e)) assert(!heard()); /* every owed Note Off and pedal off went out first */
        drains++; transfer = TX_SEND; transfer_left = 20u + rnd(400u);
    } else if (transfer == TX_SEND && !transfer_left--) { transfer = TX_LINGER; transfer_left = 1000u; }
    else if (transfer == TX_LINGER && !transfer_left--) transfer = TX_IDLE;
    if (chaos && !release_all) {
        if (!rnd(40u)) random_control();
        if (!rnd(3000u)) refuse = rnd(300u);
        if (!rnd(4000u)) uart_starve = rnd(1500u);
        if (!rnd(9000u) && transfer == TX_IDLE) { transfer = TX_DRAIN; tp_disable(&e); }
        if (!rnd(15000u)) { frame_gap = 150u + rnd(100u); outages++; } /* scan stale: graceful drain */
        if (!rnd(7000u)) { clock_period = rnd(2u) ? 0u : 15u + rnd(30u); clock_next = 1u; }
        if (route == JACK && e.state == TK_ACTIVE && !rnd(20000u)) tp_disable(&e); /* a computer appeared */
        if (route != JACK && !fault && !rnd(12000u)) inject_fault();
    }
    if (e.keys[TP_SEQ].admitted && e.keys[TP_SEQ].order != seq_order) { seq_order = e.keys[TP_SEQ].order; seq_chords++; }
    for (uint32_t i = 0; i < LN_LANES; ++i) {
        const tp_key *q = &e.keys[TP_LANE0 + i];
        if (q->admitted && q->order != lane_order[i]) { lane_order[i] = q->order; lane_strikes++; }
    }
    invariants();
    now++;
}
static void settle(void) {
    /* Hands off, hold/latch/arp off, pedal up: nothing may still sound once the strum,
     * cutoff and repeat timers have run out. */
    tc_state.hold = 0u; tc_state.arp_latch = 0u; tc_state.arp = TC_ARP_OFF;
    tc_state.sustain = false; tc_state.pedal_down_note = false;
    tc_state.transport = false; tc_state.recording = false; tm.running = false; /* the progression stops too */
    tc_state.echo = 0u; /* 1.6.0: echoes still to come end (8 echoes of 1/2 at 40 BPM would take 24 s) */
    tc_state.lock = 0u; tc_state.fn_held = 0u; tc_state.shift_latched = false; /* MIDI in let go, buttons up (1.0.0) */
    refuse = 0u; uart_starve = 0u;
    for (uint32_t i = 0; i < 3000u; ++i) pass(true, true);
    /* A fault just before may have stopped a session late; its successor still owes Offs. */
    for (uint32_t i = 0; i < 5000u && (still_carried() || tp_obligations(&e) || e.state != TK_ACTIVE); ++i)
        pass(true, true);
    if (!fault && (e.state == TK_ACTIVE || e.state == TK_DRAINING)) {
        assert(!heard() && !tp_obligations(&e) && !still_carried()); /* owed notes ended too */
        for (uint32_t k = 0; k < TP_SOURCES; ++k) assert(!e.keys[k].admitted || e.keys[k].silent);
        settles++;
    }
}
static void reset_world(uint32_t start, uint32_t s) {
    seed = s; now = start;
    tc_init(&tc_state); tm_reset(); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); sq_clear(&sq, SQ_UNIT_BAR, 0u);
    ln_clear(&ln);
    memset(&sm_out, 0, sizeof sm_out); memset(&wire, 0, sizeof wire); quiet_model();
    route = USB; usb_ready = true; refuse = serial_token = uart_starve = frame_gap = 0u; transfer = TX_IDLE;
    stall_receipt = wrong_receipt = fault = false; clock_period = 0u;
}
static void fuzz(uint32_t start, uint32_t s, uint32_t rounds) {
    reset_world(start, s);
    for (uint32_t r = 0; r < rounds; ++r) {
        pressed_limit = rnd(4u) ? 1u + rnd(10u) : 96u;
        positional = rnd(2u) != 0u;
        for (uint32_t i = 0; i < 4000u; ++i) pass(false, true);
        settle();
    }
}
/* ---- steady timing across the wrap points ---- */
static uint32_t on_at[2048], on_count, clock_at[2048], clock_count, gate_lo, gate_hi;
static bool timing_writer(const uint8_t p[4], uint32_t token) {
    (void)token; check_packet(p);
    if (p[0] == TK_ON && on_count < 2048u) on_at[on_count++] = now;
    if (p[0] == TK_OFF && on_count > 3u) { /* gate: Note On to its Note Off */
        uint32_t d = now - on_at[on_count - 1u];
        if (d < gate_lo) gate_lo = d;
        if (d > gate_hi) gate_hi = d;
    }
    if (p[0] == 0x0Fu && p[1] == 0xF8u && clock_count < 2048u) clock_at[clock_count++] = now;
    return true;
}
static void timing_pass(void) {
    if (!frame_gap--) { scan.frame++; scan.last_ms = now; scan.valid = true; frame_gap = 14u; }
    midi_clock();
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, timing_writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
    invariants();
    now++;
}
typedef struct { uint32_t lo, hi, n, gate_lo, gate_hi; } span;
static span spacing(const uint32_t *at, uint32_t n) {
    span s = {UINT32_MAX, 0u, n, 0u, 0u};
    for (uint32_t i = 3u; i < n; ++i) { /* after warm-up */
        uint32_t d = at[i] - at[i - 1u];
        if (d < s.lo) s.lo = d;
        if (d > s.hi) s.hi = d;
    }
    return s;
}
/* One pad held for 12 s from 6 s before `wrap`. */
static span timing(uint32_t wrap, uint32_t which) {
    reset_world(wrap - 7000u, 7u);
    on_count = clock_count = 0u; gate_lo = UINT32_MAX; gate_hi = 0u;
    tc_state.tempo = 16u; /* 120 BPM: one MIDI clock = 20.833 ms */
    if (which <= 1u || which == 4u) { tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; } /* 1/16 */
    if (which == 1u) tc_state.arp_sync = 1u;
    if (which == 2u) tc_state.note_repeat = 2u; /* 1/8 */
    if (which == 3u) tc_state.clock_out = 1u;
    if (which == 4u) { clock_period = 21u; clock_next = 1u; }
    if (which == 5u) { /* 1.5.0: a drum lane, every 1/16 step on, with the board's transport */
        ln.lanes[0].kind = LN_DRUMS;
        for (uint32_t s = 0; s < LN_LENGTH_DEFAULT; ++s) { ln.lanes[0].steps[s].on = true; ln.lanes[0].steps[s].value = 36u; }
    }
    for (uint32_t i = 0; i < 1000u; ++i) timing_pass();
    assert(e.state == TK_ACTIVE);
    uint32_t pad = which == 2u ? 51u : 3u;
    if (which == 5u) tc_state.transport = true; else scan.values[pad] = 3000u;
    for (uint32_t i = 0; i < 12000u; ++i) timing_pass();
    span s = which == 3u ? spacing(clock_at, clock_count) : spacing(on_at, on_count);
    s.gate_lo = gate_lo; s.gate_hi = gate_hi;
    scan.values[pad] = 0u; tc_state.transport = false;
    for (uint32_t i = 0; i < 1000u; ++i) timing_pass();
    assert(!heard());
    return s;
}
int main(void) {
    /* Ordinary start, ~30 s before now * 1000 wraps, ~45 s before the ms counter wraps. */
    static const uint32_t starts[3] = {1000u, 4294967u - 30000u, UINT32_MAX - 45000u};
    for (uint32_t i = 0; i < 3u; ++i) fuzz(starts[i], 0x2545F491u + 977u * i, 40u);
    for (uint32_t s = 1u; s <= 4u; ++s) fuzz(starts[s % 3u], 0x9E3779B9u * s, 20u);
    assert(!unexplained); /* every stop came from an injected fault */
    assert(drains >= 10u && settles >= 40u && faults >= 5u && outages >= 5u && seq_chords >= 200u && lane_strikes >= 200u);
    /* Steady patterns either side of each wrap: 1/16 arp at 120 BPM = 125 ms; synced = 6
     * clocks (124..126 ms); note repeat 1/8 = 12 clocks (249..251 ms: on the clock's grid since
     * 1.4.0); clock out every 20..21 ms; arp on an
     * incoming clock of 21 +-2 ms = 6 of them. Gates: arp 50 % of the step; note repeat
     * sounds half its step and rests the other half. */
    static const uint32_t wraps[3] = {10000u, 4294968u, 0u}; /* 0: UINT32_MAX + 1 */
    static const uint32_t lo[6] = {125u, 124u, 249u, 20u, 114u, 124u}, hi[6] = {125u, 126u, 251u, 21u, 138u, 126u};
    static const uint32_t gate_min[6] = {62u, 62u, 125u, 0u, 55u, 62u}, gate_max[6] = {63u, 63u, 125u, UINT32_MAX, 70u, 63u};
    for (uint32_t w = 0; w < 6u; ++w)
        for (uint32_t i = 0; i < 3u; ++i) {
            span s = timing(wraps[i], w);
            if (s.lo < lo[w] || s.hi > hi[w] || s.n < 40u ||
                (w != 3u && (s.gate_lo < gate_min[w] || s.gate_hi > gate_max[w]))) {
                printf("timing %u at wrap %u: %u..%u ms over %u, gate %u..%u ms\n", w, wraps[i], s.lo, s.hi, s.n,
                       s.gate_lo, s.gate_hi);
                fflush(stdout); assert(false);
            }
        }
    printf("PASS: engine stress: %u packets (%u Note On, %u Note Off) from seeded random play, controls, MIDI "
           "in/clock, 12..18 ms frames, %u scan outages, delayed/refused receipts, %u transfer drains, TRS-only "
           "sessions with a full queue, %u progression chords, %u lane steps, %u silent settles; %u injected USB faults (%u notes owed there, %u of them "
           "on a mirrored TRS jack, all ended by the next session); arp/sync/repeat/clock-out spacing steady "
           "and a 1/16 drum lane across the ms and us wraps\n", packets, ons, offs, outages, drains, seq_chords, lane_strikes, settles, faults, owed, owed_trs);
    return 0;
}
