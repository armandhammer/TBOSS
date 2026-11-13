/* SPDX-License-Identifier: GPL-3.0-or-later - settings log in flash sector 11 (flash_store.c).
 * Record = 44 words: magic, sequence, kind<<8|slot, 40 payload words, CRC-32 (format 3,
 * 0.6.0; installing erases the sector, so older formats never meet this firmware).
 * Records are appended; for each kind/slot the newest valid one wins. A record
 * interrupted by power loss fails its CRC and is skipped. Runtime saves only
 * program; the sector is erased only by st_load at boot, before USB starts (an
 * erase stalls flash for 1-2 s). Kinds: settings.h ST_LIVE..ST_MIDI_MAP.
 * Settings payload: tonic/scale/bases, modes, strum/arp, arp/tempo/LEDs,
 * channels (+ jack settings in the top byte), then 48 chord-edit bytes. Scale payload:
 * mask, 12 name bytes. Device byte: bit 7 present (records before 0.4.0 have 0 there and
 * keep the defaults), bit 0 TRS out, bits 1-2 pedal mode, bits 3-4 pedal type, bit 5 pad
 * colors (0.5.0; 0 = rainbow, which 0.4.x records therefore get), bit 6 reserved.
 * Pressure: p[1] byte 0 is the chord side; p[17] = 0x80 | melody side (0.5.0). Older
 * records have p[17] = 0 and one pressure setting, which then applies to both sides.
 * 0.6.0, words 20..25, valid when word 39 is ST_V3 (a 20-word payload from earlier
 * firmware, padded with zeros, keeps the current values instead):
 *   20: voicing | spread << 8 | octaves << 16 | live update << 24
 *   21, 22: chord pads whose strum is off (bits 0..31, 32..47)
 *   23: strum repeat | cutoff << 8 | arp rhythm << 16 | swing << 24
 *   24: arp sync | arp latch << 8 | right-half layout << 16 | bass channel << 24
 *   25: drum channel | clock out << 8 | pedal note << 16 | pedal mode << 24
 *   26: what up/down do on the play screen (0.6.1; 0 = scale, so 0.6.0 records get that)
 *       | mode family << 8 | capo << 16 (0.7.0) | chords on the right << 24 (1.5.0; 0 = left)
 *   27: strum feel: timing | velocity << 8 (0.7.0) | note repeat's rhythm << 16 | the arp rhythm's
 *       rotation << 24 (1.4.0)
 *   28: live notes: channel | MIDI-in follow << 8 (0.8.0; 0 = melody channel, off)
 *   29, 30: silent chord pads (bits 0..31, 32..47) (0.8.0)
 *   31: fold | voice leading << 8 | note repeat << 16 (0.9.0; 0 = off)
 *   32: pad Trigger | Release << 8 | Full << 16, in hundreds (0.10.0; 0 = the defaults,
 *       also written as 0 while they are the defaults, so older firmware still reads it;
 *       1.8.6: Full up to +4000, stored up to 50, which firmware before 1.8 refuses)
 *   28 byte 2: the pedal's target (0.11.0; 0 = both halves)
 *   31 byte 3: 0x80 | the melody half's velocity curve (0.11.0; 0 = the same as the chords)
 *   33: progression slot | guide lights off << 8 (0.11.0; 0 = slot 1, guide on) | pivot target << 16
 *       (1.4.0; 0 = off)
 *   34: chord set | hints << 8 | scale lock channel << 16 (1.0.0; 0 = scale chords, off, off)
 *   35..38: the centre buttons' actions, a byte each from button 0: 0 = its panel function,
 *       else action + 1 (1.0.0); bytes 14 and 15 are 0
 *   18: harmonizer | echo time << 8 | echo count << 16 (0 = the default, 3) | chance << 24 (1.6.0)
 *   19: mono melody (1.6.0); 18 and 19 are 0 in every earlier record, and with these options off
 *       | Unstick off << 8 | Spread of Chord << 9 | chord program << 16 | melody program << 24 (1.7.0;
 *       0 = Unstick on, Spread of Root, no programs)
 *   33 byte 3: Program in (1.7.0; 0 = off)
 *   24 byte 2 (the right-half layout) may be TP_MELODY_BASS (1.7.0)
 *   28 byte 3: MPE | Glide << 2 (1.8.0; 0 = off, off; 1.8.5: Off/Roll/Pad/Time, 1.8.0's On = Roll;
 *       1.8.6: Glide stored as (mode + 3) % 4, so 0 = the default, Roll)
 *       | Glide time << 4 (1.8.5; stored as (index + 6) % 8, so 0 = the default, 100 ms)
 *   32 byte 3: Vel full (1.8.3; 0 = Full; word 32 then written whenever Vel full is set)
 *   17 byte 1: Sleep after (2.0.0; 0 = off) | byte 2: the Turing pattern's Lock | Loop << 4, each stored as
 *       (value + count - default) % count, so 0 = the defaults (80 %, 8 steps)
 *   33 byte 1 bit 1: Chain (2.0.0; 0 = off); 34 byte 3: the U3 button (2.0.0; 0 = its scale, 1 = Shift)
 *   2 byte 2 and 4 byte 2 may be TC_ARP_TURING (2.0.0)
 * 1.4.0 also widens the arp rate (word 2 byte 3), the arp rhythm (23 byte 2) and note repeat (31
 * byte 2): the web editor sets values an older version doesn't have back to their defaults.
 * Each 0.11.0, 1.0.0 and 1.4.0 value is 0 while it has its default, so older firmware keeps reading
 * the record. Chord sets are kind ST_CHORDS, step patterns (1.5.0) kind ST_LANE (settings.h).
 * Sector budget: 744 record slots, compacted at boot once 372 are used. The current records are at
 * most 12 scales, 8 chord sets, the favourites, 8 progressions, 8 presets, the live settings, 16 pattern
 * records (one pattern of 4 lanes x 4 chunks), 9 looks, 9 feels, the names, 9 own chord sets, the chain,
 * 8 chances and the MIDI map (2.1.0): 92, leaving over 280 saves per session. 2.4.0: up to 256 pattern records (8 of
 * 8 lanes x 4 chunks; only those not all defaults are stored), 8 pattern records, the song and the lights: up to 342,
 * so the compaction waits until at least ST_SLOTS / 4 records can be reclaimed (st_load), and every session keeps
 * room for 186 saves or more. */
#include "settings.h"
#include "flash_store.h"
#include "crc.h"
#include "sequence.h"
#include "lanes.h"
#include "midi_in.h"
#include <string.h>
_Static_assert(LN_WORDS == ST_PAYLOAD, "a pattern record is a settings record");
_Static_assert(LN_RECORDS == 256u && LN_LANES == 2u * LN_LANES_V1, "lanes 1-4 in kind 7, 5-8 in kind 20, 128 slots each");
/* 2.4.0: pattern record g (pattern * 32 + lane * 4 + chunk: ln_bank, idx.lane) as stored, and back (settings.h). */
static uint8_t lane_kind(uint32_t g) { return (g % LN_PATTERN_RECORDS) / 16u ? (uint8_t)ST_LANE_HI : (uint8_t)ST_LANE; }
static uint8_t lane_slot(uint32_t g) { return (uint8_t)(g / LN_PATTERN_RECORDS * 16u + g % 16u); }
static uint32_t lane_index(uint8_t kind, uint8_t slot) {
    return (uint32_t)slot / 16u * LN_PATTERN_RECORDS + (kind == ST_LANE_HI ? 16u : 0u) + slot % 16u;
}
static bool lane_record(uint8_t kind, uint8_t slot) { return (kind == ST_LANE || kind == ST_LANE_HI) && slot < 128u; }
#define ST_MAGIC 0x33534254u /* "TBS3" */
#define ST_V3 0x56330000u
#define ST_WORDS (4u + ST_PAYLOAD)
#define ST_BYTES (ST_WORDS * 4u)
#define ST_SLOTS (FS_SIZE / ST_BYTES)
/* Compacted at boot once half full: every session has room for at least 372 saves, half an
 * hour of a settings change every 5 s (each root, mode, hold or panel press counts). */
#define ST_ERASE_AT (ST_SLOTS / 2u)
static uint32_t next_slot, saved_changes, seen_changes, changed_at, sequence, records;
static bool writable;
/* Newest valid record per kind/slot as log index + 1 (0 = none). One struct, so one memset clears every kind
 * (st_load, compact): a new kind added here can't be missed there. */
static struct {
    uint32_t live, preset[TC_PRESETS], scale[TC_USER_SCALES], favourites, prog[SQ_SLOTS];
    uint32_t set[TC_CHORD_SETS], lane[LN_RECORDS], look[TC_LOOKS], feel[TC_LOOKS], names;
    uint32_t own[TC_LOOKS], chain, chance[SQ_SLOTS]; /* 2.0.0 */
    uint32_t map, map2; /* 2.1.0; 2.4.13: slots 17-32 */
    uint32_t lights, pattern[LN_PATTERNS], song; /* 2.4.0 */
    uint32_t extra; /* 2.5.1 */
} idx;
void (*st_before_erase)(void);
static uint32_t lane_seen, lane_changed_at; /* ln_changes seen, and when it last changed */
/* 2.4.5 (review TBR-007): saving writes one record a pass. lane_failed: the chunks (bit n of a pattern, as ln_dirty),
 * windows (extra) and song whose write failed since the last edit, skipped until the next; tail_step: the next of
 * st_service's savers to run, ST_TAIL_STEPS when none is due. */
static uint32_t lane_failed[LN_PATTERNS];
static uint8_t extra_failed;
static bool song_failed;
static uint8_t tail_step;
#define ST_TAIL_STEPS 10u /* 2.5.1: the extras */
/* 2.4.6 (review TBR-027): the log slots whose CRC st_load found good, so its two walks back need not compute it
 * again (each CRC was computed three times at every start). Only st_load reads it. */
static uint8_t crc_good[(ST_SLOTS + 7u) / 8u];
static uint32_t *index_of(uint8_t kind, uint8_t slot) {
    if (kind == ST_LIVE && slot == 0u) return &idx.live;
    if (kind == ST_PRESET && slot < TC_PRESETS) return &idx.preset[slot];
    if (kind == ST_SCALE && slot < TC_USER_SCALES) return &idx.scale[slot];
    if (kind == ST_FAVOURITES && slot == 0u) return &idx.favourites;
    if (kind == ST_PROG && slot < SQ_SLOTS) return &idx.prog[slot];
    if (kind == ST_CHORDS && slot < TC_CHORD_SETS) return &idx.set[slot];
    if (lane_record(kind, slot)) return &idx.lane[lane_index(kind, slot)]; /* 2.4.0: lanes 5-8 in ST_LANE_HI */
    if (kind == ST_LOOK && slot < TC_LOOKS) return &idx.look[slot];
    if (kind == ST_FEEL && slot < TC_LOOKS) return &idx.feel[slot];
    if (kind == ST_NAMES && slot == 0u) return &idx.names;
    if (kind == ST_OWN_SET && slot < TC_LOOKS) return &idx.own[slot];
    if (kind == ST_CHAIN && slot == 0u) return &idx.chain;
    if (kind == ST_CHANCE && slot < SQ_SLOTS) return &idx.chance[slot];
    if (kind == ST_MIDI_MAP && slot == 0u) return &idx.map;
    if (kind == ST_MIDI_MAP && slot == 1u) return &idx.map2; /* 2.4.13 */
    if (kind == ST_LIGHTS && slot == 0u) return &idx.lights;
    if (kind == ST_EXTRA && slot == 0u) return &idx.extra; /* 2.5.1 */
    if (kind == ST_PATTERN && slot < LN_PATTERNS) return &idx.pattern[slot];
    if (kind == ST_SONG && slot == 0u) return &idx.song;
    return NULL;
}
static uint8_t live_scheme(const tc_controls *c); /* the look (1.9.0), below */
static uint8_t chord_byte(tc_chord e) {
    return (uint8_t)(e.lower | (uint32_t)(e.sus / 2u) << 3 | (uint32_t)e.add << 5);
}
/* 2.3.0: a Modes row's scale as stored, a byte: 0 its default (so a record with the defaults reads on firmware before
 * 2.3), else the scale id + 1. */
static const uint8_t row_default[TC_MODE_ROWS] = TC_MODE_ROWS_DEFAULT;
static uint32_t row_code(const tc_controls *c, uint32_t i) {
    return c->mode_rows[i] == row_default[i] ? 0u : c->mode_rows[i] + 1u;
}
static bool row_read(uint8_t code, uint32_t i, uint8_t *out) {
    if (code > TC_RETIRED_SCALES_END) return false;
    *out = code && code - 1u < TC_ALL_SCALES ? (uint8_t)(code - 1u) : row_default[i]; /* 2.3.3: a retired tuned scale: the default */
    return true;
}
/* 2.5.1: the arpeggiator's swing in 2.5's terms (0..5: 0..50 % late in tens; earlier swing as straight). */
static uint32_t coarse_swing(uint8_t v) { return v <= TC_SWING_STRAIGHT ? 0u : (v - TC_SWING_STRAIGHT + 2u) / 5u > 5u ? 5u : (v - TC_SWING_STRAIGHT + 2u) / 5u; }
static void encode(const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = c->tonic | (uint32_t)c->scale << 8 | (uint32_t)c->chord_base << 16 | (uint32_t)c->melody_base << 24;
    p[1] = c->at_chord | (uint32_t)c->vel_mode << 8 | (uint32_t)c->brightness << 16 | (uint32_t)c->hold << 24;
    p[2] = c->strum | (uint32_t)c->strum_speed << 8 | (uint32_t)c->arp << 16 | (uint32_t)c->arp_rate << 24;
    p[3] = c->arp_octaves | (uint32_t)c->arp_gate << 8 | (uint32_t)c->tempo << 16 | (uint32_t)c->midi_leds << 24;
    uint32_t jacks = 0x80u | (c->trs_out & 1u) | (uint32_t)(c->pedal_mode & 3u) << 1 | (uint32_t)c->pedal_type << 3 |
                     (uint32_t)live_scheme(c) << 5; /* 1.5.0: Rainbow sets bit 6; 1.9.0: the rest in ST_LOOK */
    p[4] = c->chord_ch | (uint32_t)c->melody_ch << 8 | (uint32_t)c->arp_last << 16 | jacks << 24;
    for (uint32_t i = 0; i < 12u; ++i)
        for (uint32_t b = 0; b < 4u; ++b) p[5u + i] |= (uint32_t)chord_byte(c->chord[4u * i + b]) << (8u * b);
    p[17] = 0x80u | c->at_melody | (uint32_t)c->sleep << 8 |
            (uint32_t)((c->arp_lock + TC_ARP_LOCKS - TC_ARP_LOCK_DEFAULT) % TC_ARP_LOCKS |
                       ((c->arp_loop + TC_ARP_LOOPS - TC_ARP_LOOP_DEFAULT) % TC_ARP_LOOPS) << 4) << 16; /* 2.0.0 */
    uint32_t echoes = c->echoes == TC_ECHOES_DEFAULT ? 0u : c->echoes + 1u; /* 0: the default */
    p[18] = c->harmony | (uint32_t)c->echo << 8 | echoes << 16 | (uint32_t)c->chance << 24;
    p[19] = c->mono | (uint32_t)(c->unstick ? 0u : 1u) << 8 | (uint32_t)(c->spread_chord == TC_SPREAD_OF_CHORD) << 9 |
            (uint32_t)(c->step_silent & 1u) << 10 | (uint32_t)(c->spread_chord == TC_SPREAD_OF_BASS) << 11 | /* 1.9.4, 2.0.1 */
            (uint32_t)(c->bend_range & 3u) << 12 | /* 2.4.12 (bits 12-13 were 2.2's Tuning out, retired in 2.3.3) */
            (uint32_t)(c->step_grid & 3u) << 14 | /* 2.2.0 */
            (uint32_t)c->chord_prog << 16 | (uint32_t)c->melody_prog << 24; /* 1.7.0: Unstick off, Spread of, programs */
    p[20] = c->chord_voicing | (uint32_t)c->chord_spread << 8 | (uint32_t)c->chord_octaves << 16 |
            (uint32_t)c->live_update << 24;
    p[21] = (uint32_t)c->strum_off; p[22] = ((uint32_t)(c->strum_off >> 32) & 0xFFFFu) | row_code(c, 0u) << 16 | row_code(c, 1u) << 24;
    p[23] = c->strum_repeat | (uint32_t)c->strum_cutoff << 8 | (uint32_t)c->arp_rhythm << 16 |
            coarse_swing(c->arp_swing) << 24; /* 2.5.1: 2.5's six steps here (going back), the exact one in ST_EXTRA */
    p[24] = c->arp_sync | (uint32_t)c->arp_latch << 8 | (uint32_t)c->melody_layout << 16 | (uint32_t)c->bass_ch << 24;
    p[25] = c->drum_ch | (uint32_t)c->clock_out << 8 | (uint32_t)c->pedal_note << 16 | (uint32_t)c->pedal_mode << 24;
    p[26] = c->arrows | (uint32_t)c->family << 8 | (uint32_t)c->capo << 16 | (uint32_t)c->chords_right << 24;
    p[27] = c->feel_time | (uint32_t)c->feel_vel << 8 | (uint32_t)c->repeat_rhythm << 16 | (uint32_t)c->arp_rotate << 24;
    p[28] = c->live_ch | (uint32_t)c->live_in << 8 | (uint32_t)c->pedal_target << 16 |
            (uint32_t)(c->mpe | ((uint32_t)c->glide + TC_GLIDE_MODES - TC_GLIDE_DEFAULT) % TC_GLIDE_MODES << 2 |
                       ((c->glide_time + TC_GLIDE_TIMES - TC_GLIDE_TIME_DEFAULT) % TC_GLIDE_TIMES) << 4) << 24;
    p[29] = (uint32_t)c->chord_mute; p[30] = ((uint32_t)(c->chord_mute >> 32) & 0xFFFFu) | row_code(c, 2u) << 16 |
                                             (uint32_t)(c->melody_follow & 1u) << 24 | /* 2.3.0 */
                                             (uint32_t)(c->whole_join & 3u) << 25 | (uint32_t)(c->whole_split & 1u) << 27 |
                                             (uint32_t)(c->right_octave & 15u) << 28; /* 2.4.9: All Melody */
    p[31] = c->fold | (uint32_t)c->voice_lead << 8 | (uint32_t)c->note_repeat << 16;
    if (c->melody_vel != c->vel_mode) p[31] |= (0x80u | c->melody_vel) << 24;
    p[33] = c->prog_slot | (uint32_t)((c->seq_guide ? 0u : 1u) | (c->chain ? 2u : 0u)) << 8 | (uint32_t)c->pivot << 16 |
            (uint32_t)c->program_in << 24;
    if (c->pad_trigger != TC_PAD_TRIGGER_DEFAULT || c->pad_release != TC_PAD_RELEASE_DEFAULT ||
        c->pad_full != TC_PAD_FULL_DEFAULT || c->pad_vel_full) /* as levels (controls.h): 1.6 reads them too */
        p[32] = (c->pad_trigger + TC_PAD_TRIGGER_STORED) | (uint32_t)(c->pad_release + TC_PAD_RELEASE_STORED) << 8 |
                (uint32_t)(c->pad_full + TC_PAD_FULL_STORED) << 16 | (uint32_t)c->pad_vel_full << 24;
    p[34] = c->chord_set | (uint32_t)c->hints << 8 | (uint32_t)c->lock << 16 | (uint32_t)c->u3_shift << 24;
    for (uint32_t i = 0; i < TC_FUNCTIONS; ++i)
        if (c->buttons[i] != i) p[35u + i / 4u] |= (uint32_t)(c->buttons[i] + 1u) << (8u * (i % 4u));
    p[39] = ST_V3;
}
static uint8_t byte(uint32_t w, uint32_t n) { return (uint8_t)(w >> (8u * n)); }
/* The pad thresholds stored as levels (settings word 32, 0.10.0's ranges and gaps) as 1.7.0's
 * settings above each pad's rest: less each level's offset, in range, the gaps kept. A record
 * written by 1.7.0 reads back exactly. */
static uint8_t within(uint32_t v, uint32_t lo, uint32_t hi) { return (uint8_t)(v < lo ? lo : v > hi ? hi : v); }
static bool pad_levels(uint32_t w, tc_controls *t) {
    uint32_t trigger = byte(w, 0), release = byte(w, 1), full = byte(w, 2);
    if (byte(w, 3) >= TC_PAD_VEL_FULLS || trigger < 5u || trigger > 30u || release < 3u || release > 25u || full < 15u || full > TC_PAD_FULL_MAX + TC_PAD_FULL_STORED ||
        trigger < release + 2u || full < trigger + 5u) return false;
    t->pad_trigger = within(trigger - TC_PAD_TRIGGER_STORED, TC_PAD_TRIGGER_MIN, TC_PAD_TRIGGER_MAX);
    t->pad_release = within(release - TC_PAD_RELEASE_STORED, TC_PAD_RELEASE_MIN, t->pad_trigger - 2u);
    t->pad_full = within(full - TC_PAD_FULL_STORED, t->pad_trigger + 5u, TC_PAD_FULL_MAX);
    t->pad_vel_full = (uint8_t)byte(w, 3); /* 1.8.3 */
    return true;
}
/* Every field in range and the whole layout inside MIDI range, or nothing changes. */
static bool decode(const uint32_t p[ST_PAYLOAD], tc_controls *c) {
    tc_controls t = *c;
    t.unstick = 1u; /* a record before 1.7.0: on */
    t.tonic = byte(p[0], 0); t.scale = byte(p[0], 1); t.chord_base = byte(p[0], 2); t.melody_base = byte(p[0], 3);
    /* 2.3.3: a retired tuned scale (2.2.0-2.3.2's tunings, layout.h): 12's C Major; the root was a step of that tuning */
    if (t.scale >= TC_ALL_SCALES && t.scale < TC_RETIRED_SCALES_END) { t.scale = 0u; t.tonic = 0u; }
    t.at_chord = byte(p[1], 0); t.vel_mode = byte(p[1], 1); t.brightness = byte(p[1], 2); t.hold = byte(p[1], 3);
    t.strum = byte(p[2], 0); t.strum_speed = byte(p[2], 1); t.arp = byte(p[2], 2); t.arp_rate = byte(p[2], 3);
    t.arp_octaves = byte(p[3], 0); t.arp_gate = byte(p[3], 1); t.tempo = byte(p[3], 2); t.midi_leds = byte(p[3], 3);
    t.chord_ch = byte(p[4], 0); t.melody_ch = byte(p[4], 1); t.arp_last = byte(p[4], 2);
    uint8_t jacks = byte(p[4], 3);
    if (jacks & 0x80u) {
        if (((jacks >> 5) & 3u) >= TC_LED_SCHEMES_V1 || ((jacks >> 1) & 3u) >= TC_PEDAL_MODES ||
            ((jacks >> 3) & 3u) >= TC_PEDAL_TYPES) return false;
        /* 1.9.0: bits that say what they would of the scheme on show leave it (Piano and later read 0) */
        if (((jacks >> 5) & 3u) != live_scheme(&t)) t.led_scheme = (uint8_t)((jacks >> 5) & 3u);
        t.trs_out = (uint8_t)(jacks & 1u); t.pedal_mode = (uint8_t)((jacks >> 1) & 3u);
        t.pedal_type = (uint8_t)((jacks >> 3) & 3u);
    } else if (jacks) return false;
    if (p[17] >> 24 || byte(p[17], 1) >= TC_SLEEPS || (byte(p[17], 2) & 15u) >= TC_ARP_LOCKS ||
        (byte(p[17], 2) >> 4) >= TC_ARP_LOOPS || (byte(p[17], 0) && (p[17] & 0x7Fu) >= TC_AT_MODES) ||
        (p[17] >> 8 && !(p[17] & 0x80u)) || (byte(p[17], 0) && !(p[17] & 0x80u))) return false;
    t.at_melody = byte(p[17], 0) ? (uint8_t)(p[17] & 0x7Fu) : t.at_chord;
    t.sleep = byte(p[17], 1); /* 2.0.0 */
    t.arp_lock = (uint8_t)(((byte(p[17], 2) & 15u) + TC_ARP_LOCK_DEFAULT) % TC_ARP_LOCKS);
    t.arp_loop = (uint8_t)(((byte(p[17], 2) >> 4) + TC_ARP_LOOP_DEFAULT) % TC_ARP_LOOPS);
    if (t.at_chord >= TC_AT_MODES || t.vel_mode >= TC_VEL_MODES || t.brightness >= TC_BRIGHTNESS_LEVELS ||
        t.hold > 1u || t.strum >= TC_STRUM_MODES || t.strum_speed >= TC_STRUM_SPEEDS ||
        t.arp >= TC_ARP_MODES || t.arp_rate >= TC_ARP_RATES || t.arp_octaves >= TC_ARP_OCTAVES ||
        t.arp_gate >= TC_ARP_GATES || t.tempo >= TC_TEMPOS || t.midi_leds > 1u ||
        t.chord_ch > 15u || t.melody_ch > 15u || t.arp_last >= TC_ARP_MODES) return false;
    if (p[39] != ST_V3) for (uint32_t i = 18u; i < 20u; ++i) if (p[i]) return false;
    if (p[39] == ST_V3) {
        if (!row_read(byte(p[22], 2), 0u, &t.mode_rows[0]) || !row_read(byte(p[22], 3), 1u, &t.mode_rows[1]) ||
            !row_read(byte(p[30], 2), 2u, &t.mode_rows[2]) || (byte(p[30], 3) & 1u) >= TC_FOLLOWS) return false; /* 2.3.0 */
        t.melody_follow = (uint8_t)(byte(p[30], 3) & 1u); t.follow_row = 0u;
        /* 2.4.9, All Melody: its join (bits 25-26), Split (27), the right half's octave (28-31) */
        if (((p[30] >> 25) & 3u) >= TP_JOINS || (p[30] >> 28) >= TP_RIGHT_OCTAVES) return false;
        t.whole_join = (uint8_t)((p[30] >> 25) & 3u); t.whole_split = (uint8_t)((p[30] >> 27) & 1u);
        t.right_octave = (uint8_t)(p[30] >> 28);
        if (byte(p[18], 0) >= TC_HARMONIES || byte(p[18], 1) >= TC_ECHO_TIMES || byte(p[18], 2) > TC_ECHOES ||
            byte(p[18], 3) >= TC_CHANCES || ((p[19] >> 14) & 3u) >= TC_STEP_GRIDS || /* bits 12-13: retired, ignored */
            (p[19] & 0xA00u) == 0xA00u || byte(p[19], 0) > 1u ||
            byte(p[19], 2) >= TC_PROGRAMS || byte(p[19], 3) >= TC_PROGRAMS) return false;
        t.harmony = byte(p[18], 0); t.echo = byte(p[18], 1); t.chance = byte(p[18], 3); t.mono = byte(p[19], 0);
        t.unstick = (uint8_t)(byte(p[19], 1) & 1u ? 0u : 1u); t.spread_chord = (uint8_t)(byte(p[19], 1) & 8u ? TC_SPREAD_OF_BASS : (byte(p[19], 1) >> 1) & 1u);
        t.step_silent = (uint8_t)((byte(p[19], 1) >> 2) & 1u);
        t.step_grid = (uint8_t)((p[19] >> 14) & 3u); /* 2.2.0 */
        t.bend_range = (uint8_t)((p[19] >> 12) & 3u); /* 2.4.12: Bend range (0 = 2 semitones; firmware before reads past it) */
        t.chord_prog = byte(p[19], 2); t.melody_prog = byte(p[19], 3);
        t.echoes = byte(p[18], 2) ? (uint8_t)(byte(p[18], 2) - 1u) : (uint8_t)TC_ECHOES_DEFAULT;
        if (byte(p[26], 3) >= TC_PADS_SIDES || byte(p[26], 0) >= TC_ARROWS_MODES || byte(p[26], 1) >= TC_FAMILIES ||
            byte(p[26], 2) >= TC_CAPOS || byte(p[27], 0) >= TC_FEEL_TIMES || byte(p[27], 1) >= TC_FEEL_VELS ||
            byte(p[27], 2) > RH_EUCLIDS || byte(p[27], 3) >= TC_ROTATES) return false;
        if (byte(p[28], 3) > 127u || byte(p[28], 2) >= TC_PEDAL_TARGETS ||
            byte(p[28], 0) >= TC_LIVE_CHANNELS || byte(p[28], 1) >= TC_LIVE_INPUTS) return false;
        if (byte(p[31], 0) > 1u || byte(p[31], 1) > 1u || byte(p[31], 2) >= TC_NOTE_REPEATS) return false;
        if (byte(p[31], 3) && (!(byte(p[31], 3) & 0x80u) || (byte(p[31], 3) & 0x7Fu) >= TC_VEL_MODES)) return false;
        if (byte(p[33], 3) >= TC_PROGRAM_INPUTS || byte(p[33], 0) >= SQ_SLOTS || byte(p[33], 1) > 3u ||
            byte(p[33], 2) >= TC_PIVOTS) return false;
        t.program_in = byte(p[33], 3);
        if (byte(p[34], 3) > 1u || byte(p[34], 0) > TC_CHORDS_MODES || byte(p[34], 1) > 1u || byte(p[34], 2) > 16u) return false;
        t.chord_set = byte(p[34], 0); t.hints = byte(p[34], 1); t.lock = byte(p[34], 2); t.u3_shift = byte(p[34], 3);
        for (uint32_t i = 0; i < 16u; ++i) {
            uint8_t b = byte(p[35u + i / 4u], i % 4u);
            if (i >= TC_FUNCTIONS ? b != 0u : b > TC_ACTIONS) return false;
            if (i < TC_FUNCTIONS) t.buttons[i] = b ? (uint8_t)(b - 1u) : (uint8_t)i;
        }
        t.shift_latched = false; t.button_sustain = false;
        t.pedal_target = byte(p[28], 2); t.prog_slot = byte(p[33], 0); t.seq_guide = (uint8_t)(byte(p[33], 1) & 1u ? 0u : 1u);
        t.chain = (uint8_t)((byte(p[33], 1) >> 1) & 1u); /* 2.0.0 */
        t.pivot = byte(p[33], 2);
        if (p[32] && !pad_levels(p[32], &t)) return false;
        if (!p[32]) { t.pad_trigger = TC_PAD_TRIGGER_DEFAULT; t.pad_release = TC_PAD_RELEASE_DEFAULT; t.pad_full = TC_PAD_FULL_DEFAULT;
                      t.pad_vel_full = 0u; }
        t.fold = byte(p[31], 0); t.voice_lead = byte(p[31], 1); t.note_repeat = byte(p[31], 2);
        t.melody_vel = byte(p[31], 3) ? (uint8_t)(byte(p[31], 3) & 0x7Fu) : t.vel_mode;
        t.mpe = byte(p[28], 3) & 3u; t.glide = (uint8_t)((((byte(p[28], 3) >> 2) & 3u) + TC_GLIDE_DEFAULT) % TC_GLIDE_MODES); /* 1.8.0; 1.8.6 */
        t.glide_time = (uint8_t)(((byte(p[28], 3) >> 4) + TC_GLIDE_TIME_DEFAULT) % TC_GLIDE_TIMES); /* 1.8.5 */
        t.live_ch = byte(p[28], 0); t.live_in = byte(p[28], 1); t.chord_mute = p[29] | (uint64_t)(p[30] & 0xFFFFu) << 32;
        t.arrows = byte(p[26], 0); t.family = byte(p[26], 1); t.capo = byte(p[26], 2); t.chords_right = byte(p[26], 3);
        t.feel_time = byte(p[27], 0); t.feel_vel = byte(p[27], 1); t.repeat_rhythm = byte(p[27], 2); t.arp_rotate = byte(p[27], 3);
        t.chord_voicing = byte(p[20], 0); t.chord_spread = byte(p[20], 1); t.chord_octaves = byte(p[20], 2);
        t.live_update = byte(p[20], 3); t.strum_off = p[21] | (uint64_t)(p[22] & 0xFFFFu) << 32;
        t.strum_repeat = byte(p[23], 0); t.strum_cutoff = byte(p[23], 1); t.arp_rhythm = byte(p[23], 2);
        t.arp_swing = (uint8_t)(TC_SWING_STRAIGHT + 5u * byte(p[23], 3)); /* 2.5.1: 2.5's 0..50 % in tens (ST_EXTRA: exact) */
        t.arp_sync = byte(p[24], 0); t.arp_latch = byte(p[24], 1);
        t.melody_layout = byte(p[24], 2); t.bass_ch = byte(p[24], 3); t.drum_ch = byte(p[25], 0);
        t.clock_out = byte(p[25], 1); t.pedal_note = byte(p[25], 2); t.pedal_mode = byte(p[25], 3);
        if (t.chord_voicing >= TC_VOICINGS || t.chord_spread >= TC_SPREADS || !t.chord_octaves ||
            t.chord_octaves > 0x7Fu || t.live_update > 1u || t.strum_repeat >= TC_REPEATS ||
            t.strum_cutoff >= TC_CUTOFFS || t.arp_rhythm >= TC_RHYTHM_VALUES || byte(p[23], 3) >= 6u ||
            t.arp_sync > 1u || t.arp_latch > 1u || t.bass_ch > 15u || t.drum_ch > 15u || t.clock_out > 1u ||
            t.pedal_note > 127u || t.pedal_mode >= TC_PEDAL_MODES) return false;
    } else {
        for (uint32_t i = 20u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
        memcpy(t.mode_rows, row_default, sizeof row_default); t.melody_follow = 0u; t.follow_row = 0u; /* 2.3.0 */
    }
    for (uint32_t k = 0; k < 48u; ++k) {
        uint8_t b = byte(p[5u + k / 4u], k % 4u);
        uint32_t sus = (b >> 3) & 3u;
        if (sus == 3u) return false;
        t.chord[k] = (tc_chord){(uint8_t)(b & 7u), (uint8_t)(2u * sus), (uint8_t)(b >> 5)};
    }
    tp_layout layout = tc_layout(&t);
    if (!tc_layout_ok(layout) || layout.chord_base + TP_OCTAVE_RANGE < TP_CHORD_BASE ||
        layout.chord_base > TP_CHORD_BASE + TP_OCTAVE_RANGE ||
        (layout.whole ? layout.melody_base > TP_WHOLE_BASE_MAX : /* 2.4.9: All Melody, wider */
         layout.melody_base + TP_OCTAVE_RANGE < TP_MELODY_BASE || layout.melody_base > TP_MELODY_BASE + TP_OCTAVE_RANGE)) return false;
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
static void favourites_payload(uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    for (uint32_t i = 0; i < TC_FAVOURITES; ++i)
        p[i / 4u] |= (uint32_t)(i < tc_favourite_count ? tc_favourites[i] : 0xFFu) << (8u * (i % 4u));
}
/* Valid favourites payload: sets them (and returns true), else changes nothing. */
static bool favourites_set(const uint32_t p[ST_PAYLOAD]) {
    uint8_t ids[TC_FAVOURITES]; uint32_t n = 0; bool seen[256] = {false}, ended = false;
    for (uint32_t i = TC_FAVOURITES / 4u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    for (uint32_t i = 0; i < TC_FAVOURITES; ++i) {
        uint8_t id = byte(p[i / 4u], i % 4u);
        if (id == 0xFFu) { ended = true; continue; }
        if (ended || id >= TC_ALL_SCALES || seen[id]) return false;
        seen[id] = true; ids[n++] = id;
    }
    memcpy(tc_favourites, ids, n); tc_favourite_count = (uint8_t)n;
    return true;
}
static void set_words(const tc_chord_set *s, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = 1u;
    for (uint32_t i = 0; i < 12u; ++i) p[1u + i] = s->column[i].mask | (uint32_t)s->column[i].root << 12;
    for (uint32_t i = 0; i < TC_USER_NAME; ++i) p[13u + i / 4u] |= (uint32_t)(uint8_t)s->name[i] << (8u * (i % 4u));
}
static void set_payload(uint8_t slot, uint32_t p[ST_PAYLOAD]) { set_words(tc_chord_set_get(slot), p); }
/* A valid chord set payload as a set (true), else false. */
static bool set_ok(const uint32_t p[ST_PAYLOAD], tc_chord_set *out) {
    memset(out, 0, sizeof *out);
    if (p[0] != 1u) return false;
    for (uint32_t i = 16u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    for (uint32_t i = 0; i < 12u; ++i) {
        uint32_t w = p[1u + i];
        tc_set_chord ch = {(uint16_t)(w & 0xFFFu), (uint8_t)(w >> 12)};
        if (w >> 17 || !tc_set_chord_ok(ch)) return false;
        out->column[i] = ch;
    }
    for (uint32_t i = 0; i < TC_USER_NAME; ++i) {
        char ch = (char)byte(p[13u + i / 4u], i % 4u);
        if (ch && (ch < ' ' || ch > '~')) return false;
        out->name[i] = ch;
    }
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
    w[ST_WORDS - 1u] = tb_crc32_words(w, ST_WORDS - 1u);
    bool ok = fs_program(next_slot * ST_BYTES, w, ST_WORDS);
    /* 2.4.6 (review TBR-018): read back (flash_store.c has flushed the data cache); a word that did not take is a failed
     * write, so the index keeps the older record and the caller says NOT SAVED. */
    const uint32_t *back = slot_words(next_slot);
    for (uint32_t i = 0; ok && i < ST_WORDS; ++i) ok = back[i] == w[i];
    next_slot++; /* a failed slot is never reused */
    if (ok) { sequence++; records++; *at = next_slot; }
    return ok;
}
static const uint32_t *payload_at(uint32_t at);
/* A preset stored and not cleared since (1.9.3: a record of zeros clears it). */
static bool preset_present(uint32_t slot) {
    if (slot >= TC_PRESETS || !idx.preset[slot]) return false;
    const uint32_t *p = payload_at(idx.preset[slot]);
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) if (p[i]) return true;
    return false;
}
static void note_presets(void) {
    tc_presets_present = 0u;
    for (uint32_t i = 0; i < TC_PRESETS; ++i) if (preset_present(i)) tc_presets_present |= (uint8_t)(1u << i);
    sq_present = 0u;
    for (uint32_t i = 0; i < SQ_SLOTS; ++i)
        if (idx.prog[i] && slot_words(idx.prog[i] - 1u)[3] >> 24) sq_present |= (uint8_t)(1u << i);
}
static const uint32_t *payload_at(uint32_t at) { return slot_words(at - 1u) + 3u; }
/* A record stored (at, an index entry) and not cleared since (a cleared one's first word is 0). */
static bool stored(uint32_t at) { return at && payload_at(at)[0]; }
/* --- the feel (1.9.0; 1.9.3 presets' own, settings.h ST_FEEL) --- */
static bool feel_present(uint32_t slot) { return slot < TC_LOOKS && stored(idx.feel[slot]); }
static void feel_payload(const uint8_t feel[TC_FEEL_BYTES], uint8_t keep, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = 1u | (uint32_t)keep << 8;
    for (uint32_t i = 0; i < TC_FEEL_BYTES; ++i) p[1u + i / 4u] |= (uint32_t)feel[i] << (8u * (i % 4u));
}
/* A feel record's curves into feel (all zero: the defaults) and Keep feel into keep (NULL: a preset's own,
 * which has none); false if it isn't one. */
static bool feel_read(const uint32_t p[ST_PAYLOAD], uint8_t feel[TC_FEEL_BYTES], uint8_t *keep) {
    for (uint32_t i = 1u + TC_FEEL_BYTES / 4u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    if (!p[0]) {
        for (uint32_t i = 1u; i <= TC_FEEL_BYTES / 4u; ++i) if (p[i]) return false;
        tc_feel_default(feel);
        if (keep) *keep = 0u;
        return true;
    }
    uint8_t t[TC_FEEL_BYTES];
    for (uint32_t i = 0; i < TC_FEEL_BYTES; ++i) t[i] = (uint8_t)(p[1u + i / 4u] >> (8u * (i % 4u)));
    if (byte(p[0], 0) != 1u || p[0] >> 16 || byte(p[0], 1) > (keep ? 1u : 0u) || !tc_feel_ok(t)) return false;
    memcpy(feel, t, sizeof t);
    if (keep) *keep = byte(p[0], 1);
    return true;
}
/* Slot's record as stored, or what never storing it means (the default curves, Keep feel off). */
static void feel_stored(uint32_t slot, uint32_t p[ST_PAYLOAD]) {
    if (idx.feel[slot]) { memcpy(p, payload_at(idx.feel[slot]), ST_PAYLOAD * 4u); return; }
    uint8_t plain[TC_FEEL_BYTES]; tc_feel_default(plain); feel_payload(plain, 0u, p);
}
/* The board's curves as stored (the defaults if never stored). */
static void board_feel(uint8_t feel[TC_FEEL_BYTES]) {
    uint8_t keep;
    if (!idx.feel[0] || !feel_read(payload_at(idx.feel[0]), feel, &keep)) tc_feel_default(feel);
}
/* With the settings: the curves in use where they belong (the board's, or the preset's whose they are) when
 * they differ from what is stored there; Keep feel always in the board's. */
static void save_feel_keep(tc_controls *c) { /* 2.4.5: save_feel's two records, one a pass from st_service */
    uint32_t s = c->feel_slot % TC_LOOKS, p[ST_PAYLOAD], q[ST_PAYLOAD];
    if (!s) return;
    uint8_t board[TC_FEEL_BYTES]; board_feel(board); feel_payload(board, c->keep_feel, p); feel_stored(0, q);
    if (memcmp(p, q, sizeof p) && !write_record(ST_FEEL, 0, p)) c->last_action = TC_SAVE_FAILED;
}
static void save_feel_slot(tc_controls *c) {
    uint32_t s = c->feel_slot % TC_LOOKS, p[ST_PAYLOAD], q[ST_PAYLOAD];
    feel_payload(c->feel, s ? 0u : c->keep_feel, p); feel_stored(s, q);
    if (memcmp(p, q, sizeof p) && !write_record(ST_FEEL, (uint8_t)s, p)) c->last_action = TC_SAVE_FAILED;
}
static void save_feel(tc_controls *c) { save_feel_keep(c); save_feel_slot(c); }
/* --- preset names (1.9.3, settings.h ST_NAMES) --- */
#define ST_NAME_WORDS (TC_PRESETS * TC_PRESET_NAME / 4u)
static void names_payload(uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = 1u;
    for (uint32_t i = 0; i < TC_PRESETS * TC_PRESET_NAME; ++i)
        p[1u + i / 4u] |= (uint32_t)(uint8_t)tc_preset_names[i / TC_PRESET_NAME][i % TC_PRESET_NAME] << (8u * (i % 4u));
}
/* A names record into names (all zero: none named), false if it isn't one. */
static bool names_read(const uint32_t p[ST_PAYLOAD], char names[TC_PRESETS][TC_PRESET_NAME + 1u]) {
    char t[TC_PRESETS][TC_PRESET_NAME + 1u];
    for (uint32_t i = 1u + ST_NAME_WORDS; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    if (p[0] > 1u) return false;
    for (uint32_t i = 0; i < TC_PRESETS * TC_PRESET_NAME; ++i) {
        uint32_t n = i / TC_PRESET_NAME, at = i % TC_PRESET_NAME;
        char ch = (char)byte(p[1u + i / 4u], i % 4u);
        if (ch && (ch < ' ' || ch > '~' || !p[0] || (at && !t[n][at - 1u]))) return false; /* 0 after the last */
        t[n][at] = ch; t[n][TC_PRESET_NAME] = 0;
    }
    memcpy(names, t, sizeof t);
    return true;
}
static bool names_any(void) {
    for (uint32_t i = 0; i < TC_PRESETS; ++i) if (tc_preset_names[i][0]) return true;
    return false;
}
/* --- presets' own chord sets (2.0.0, settings.h ST_OWN_SET) --- */
static bool own_present(uint32_t slot) { return slot && slot < TC_LOOKS && stored(idx.own[slot]); }
static uint8_t keep_chords_stored(void) { return idx.own[0] ? byte(payload_at(idx.own[0])[0], 2) : 0u; }
/* A record of a preset's own (or slot 9's) set: the set slot it covers and its chords; false if it isn't one. */
static bool own_read(const uint32_t p[ST_PAYLOAD], bool now, uint8_t *set, tc_chord_set *out) {
    uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q);
    if (byte(p[0], 0) != 1u || byte(p[0], 1) >= TC_CHORD_SETS || byte(p[0], 2) > (now ? 1u : 0u) || p[0] >> 24) return false;
    q[0] = 1u; if (now) q[ST_PAYLOAD - 1u] = 0u;
    if (!set_ok(q, out)) return false;
    *set = byte(p[0], 1);
    return true;
}
static bool own_ok(const uint32_t p[ST_PAYLOAD], uint8_t slot) {
    bool zeros = true;
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) zeros = zeros && !p[i];
    if (zeros) return true; /* cleared */
    if (!slot) { for (uint32_t i = 1u; i < ST_PAYLOAD; ++i) if (p[i]) return false; return (p[0] & ~0x10000u) == 1u; }
    uint8_t set; tc_chord_set s;
    return own_read(p, false, &set, &s);
}
/* Chord set k as the board keeps it (empty if never stored), also while a preset's own covers it. */
static void board_set(uint8_t k, tc_chord_set *out) {
    if (!idx.set[k] || !set_ok(payload_at(idx.set[k]), out)) memset(out, 0, sizeof *out);
}
static void own_payload(uint8_t set, uint32_t p[ST_PAYLOAD]) { set_payload(set, p); p[0] = 1u | (uint32_t)set << 8; }
/* The board's set back where a preset's own covers it. */
static void uncover(tc_controls *c) {
    if (!c->chords_slot) return;
    tc_chord_set s; board_set(c->chords_cover, &s); (void)tc_chord_set_put(c->chords_cover, &s);
    c->chords_slot = 0u;
}
/* Preset slot - 1's own set in its place. */
static void cover(tc_controls *c, uint8_t slot) {
    uint8_t set; tc_chord_set s;
    if (!own_present(slot) || !own_read(payload_at(idx.own[slot]), false, &set, &s)) return;
    (void)tc_chord_set_put(set, &s);
    c->chords_slot = slot; c->chords_cover = set;
}
/* The covered set saved where it belongs (its preset's own) when changed since, and Keep chords (with the
 * settings and before another preset's set replaces it). */
static void save_chords_keep(tc_controls *c) { /* 2.4.5: save_chords' two records, one a pass from st_service */
    uint32_t p[ST_PAYLOAD];
    if (c->keep_chords == keep_chords_stored()) return;
    memset(p, 0, sizeof p); p[0] = 1u | (uint32_t)(c->keep_chords ? 1u : 0u) << 16;
    if (!write_record(ST_OWN_SET, 0, p)) c->last_action = TC_SAVE_FAILED;
}
static void save_chords_own(tc_controls *c) {
    uint32_t p[ST_PAYLOAD];
    if (!c->chords_slot) return;
    own_payload(c->chords_cover, p);
    if (own_present(c->chords_slot) && !memcmp(p, payload_at(idx.own[c->chords_slot]), sizeof p)) return;
    if (!write_record(ST_OWN_SET, c->chords_slot, p)) c->last_action = TC_SAVE_FAILED;
}
static void save_chords(tc_controls *c) { save_chords_keep(c); save_chords_own(c); }
/* --- the chain and each progression's chances (2.0.0) --- */
/* Slot's progression as stored, with its chances, into sq_slots (the chain plays from there). */
static void bank(uint8_t slot) {
    sq_prog *p = &sq_slots[slot];
    if (!idx.prog[slot] || !sq_decode(payload_at(idx.prog[slot]), p)) sq_clear(p, SQ_UNIT_BAR, 0u);
    if (idx.chance[slot]) (void)sq_chances_decode(payload_at(idx.chance[slot]), p);
}
/* sq's chances stored beside its progression when they differ from what is stored there. */
static void save_chances(tc_controls *c, uint8_t slot) {
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    sq_chances_encode(&sq, p);
    if (idx.chance[slot]) memcpy(q, payload_at(idx.chance[slot]), sizeof q); else memset(q, 0, sizeof q);
    if (memcmp(p, q, sizeof p) && !write_record(ST_CHANCE, slot, p)) c->last_action = TC_SAVE_FAILED;
}
/* --- the MIDI map (2.1.0, settings.h ST_MIDI_MAP; 2.4.13: two records, slot 0 the Control channel and map slots 1-16,
 * slot 1 slots 17-32). Word 1 + i: kind | cc << 8 | target << 16 | jump << 24 | channel << 25 (2.4.13: 0 the Control
 * channel, 1..16) | source << 30 (2.4.13: TM_SRC_); word 17 + i: lo | hi << 8. Firmware before 2.4.13 refuses a record
 * with a channel or a source (word 0 byte 1 of slot 1 is 0). --- */
static void map_payload(const tc_controls *c, uint32_t part, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    bool any = !part && c->control_ch != 0u;
    for (uint32_t i = 0; i < TC_CC_PART; ++i) {
        const tc_cc_slot *s = &c->cc[part * TC_CC_PART + i];
        p[1u + i] = s->kind | (uint32_t)s->cc << 8 | (uint32_t)s->target << 16 | (uint32_t)s->jump << 24 |
                    (uint32_t)(s->channel & 31u) << 25 | (uint32_t)(s->src & 3u) << 30;
        p[1u + TC_CC_PART + i] = s->lo | (uint32_t)s->hi << 8;
        any = any || s->kind != TC_CC_EMPTY;
    }
    if (any) p[0] = 1u | (uint32_t)(part ? 0u : c->control_ch) << 8;
}
/* A map record (word 39 aside) of part 0 or 1 into c's map (part 0: and Control channel; c NULL: only checked); false if
 * it isn't one. */
static bool map_read(const uint32_t p[ST_PAYLOAD], uint32_t part, tc_controls *c) {
    tc_cc_slot slots[TC_CC_PART];
    for (uint32_t i = 1u + 2u * TC_CC_PART; i < ST_PAYLOAD - 1u; ++i) if (p[i]) return false;
    if (p[0] && (byte(p[0], 0) != 1u || byte(p[0], 1) >= TC_CONTROL_CHANNELS || (part && byte(p[0], 1)) || p[0] >> 16)) return false;
    for (uint32_t i = 0; i < TC_CC_PART; ++i) {
        uint32_t w = p[1u + i], r = p[1u + TC_CC_PART + i];
        slots[i] = (tc_cc_slot){byte(w, 0), byte(w, 1), byte(w, 2), byte(r, 0), byte(r, 1), (uint8_t)((w >> 24) & 1u),
                                (uint8_t)(w >> 30), (uint8_t)((w >> 25) & 31u)};
        if (r >> 16 || (!p[0] && (w || r)) || !tc_cc_slot_ok(&slots[i])) return false;
    }
    if (!c) return true;
    for (uint32_t i = 0; i < TC_CC_PART; ++i) tc_cc_restart(c, part * TC_CC_PART + i);
    memcpy(&c->cc[part * TC_CC_PART], slots, sizeof slots);
    if (!part) c->control_ch = byte(p[0], 1);
    tc_cc_watch(c);
    return true;
}
/* The map stored where it differs from what is (with the settings, and at once when the editor sends one). */
static bool save_map(const tc_controls *c) {
    for (uint32_t part = 0; part < 2u; ++part) {
        uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD], at = part ? idx.map2 : idx.map;
        map_payload(c, part, p);
        if (at) memcpy(q, payload_at(at), sizeof q); else memset(q, 0, sizeof q);
        if (memcmp(p, q, sizeof p) && !write_record(ST_MIDI_MAP, (uint8_t)part, p)) return false;
    }
    return true;
}
/* --- the extras (2.5.1, settings.h ST_EXTRA): part 0 from the live settings, the presets' parts kept here --- */
static uint32_t extra_presets[TC_PRESETS][ST_EXTRA_WORDS];
static void extra_part(const tc_controls *c, uint32_t w[ST_EXTRA_WORDS]) {
    memset(w, 0, ST_EXTRA_WORDS * 4u);
    if (c->arp_swing != TC_SWING_STRAIGHT + 5u * coarse_swing(c->arp_swing)) w[0] = 1u | (uint32_t)c->arp_swing << 8;
}
static bool extra_part_ok(const uint32_t w[ST_EXTRA_WORDS]) {
    for (uint32_t i = 1u; i < ST_EXTRA_WORDS; ++i) if (w[i]) return false;
    return !w[0] || ((w[0] & 0xFFu) == 1u && byte(w[0], 1) < TC_SWINGS && !(w[0] >> 16));
}
static void extra_part_apply(const uint32_t w[ST_EXTRA_WORDS], tc_controls *t) { if (w[0]) t->arp_swing = byte(w[0], 1); }
static void extra_payload(const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    extra_part(c, p);
    memcpy(&p[ST_EXTRA_WORDS], extra_presets, sizeof extra_presets);
}
/* A valid extras record (c NULL: only checked) into the live settings and the presets' parts. */
static bool extra_read(const uint32_t p[ST_PAYLOAD], tc_controls *c) {
    for (uint32_t i = ST_EXTRA_WORDS * (1u + TC_PRESETS); i < ST_PAYLOAD; ++i) if (p[i]) return false;
    for (uint32_t part = 0; part <= TC_PRESETS; ++part) if (!extra_part_ok(&p[part * ST_EXTRA_WORDS])) return false;
    if (!c) return true;
    extra_part_apply(p, c);
    memcpy(extra_presets, &p[ST_EXTRA_WORDS], sizeof extra_presets);
    return true;
}
static bool save_extra(const tc_controls *c) {
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    extra_payload(c, p);
    if (idx.extra) memcpy(q, payload_at(idx.extra), sizeof q); else memset(q, 0, sizeof q);
    return !memcmp(p, q, sizeof p) || write_record(ST_EXTRA, 0, p);
}
/* --- the lights (2.4.0, settings.h ST_LIGHTS) --- */
static void lights_payload(const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    if (c->out_scale || c->dim) p[0] = 1u | (uint32_t)c->out_scale << 8 | (uint32_t)c->dim << 16;
}
/* A valid lights record into c (when given); all zeros reads as AUTO, AUTO. */
static bool lights_read(const uint32_t p[ST_PAYLOAD], tc_controls *c) {
    for (uint32_t i = 1u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    if (p[0] && ((p[0] & 0xFFu) != 1u || byte(p[0], 1) >= TC_OUT_SCALES || byte(p[0], 2) >= TC_DIMS || byte(p[0], 3))) return false;
    if (c) { c->out_scale = byte(p[0], 1); c->dim = byte(p[0], 2); }
    return true;
}
static bool save_lights(const tc_controls *c) {
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    lights_payload(c, p);
    if (idx.lights) memcpy(q, payload_at(idx.lights), sizeof q); else memset(q, 0, sizeof q);
    return !memcmp(p, q, sizeof p) || write_record(ST_LIGHTS, 0, p);
}
/* --- the look (1.9.0, settings.h ST_LOOK) --- */
static bool look_present(uint32_t slot) { return slot < TC_LOOKS && stored(idx.look[slot]); }
static void look_payload(const tc_controls *c, bool board, uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u);
    p[0] = 1u | (uint32_t)c->led_scheme << 8 | (uint32_t)c->color_by << 16 | (uint32_t)(board ? c->keep_look : 0u) << 24;
    for (uint32_t i = 0; i < TC_LOOK_ROLES; ++i) p[1u + i / 4u] |= (uint32_t)c->look_roles[i] << (8u * (i % 4u));
}
static bool look_ok(const uint32_t p[ST_PAYLOAD], bool board) {
    for (uint32_t i = 10u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
    if (!p[0]) { for (uint32_t i = 1u; i < 10u; ++i) if (p[i]) return false; return true; } /* cleared */
    if (byte(p[0], 0) != 1u || byte(p[0], 1) >= TC_LED_SCHEMES || byte(p[0], 2) >= TC_COLOR_BYS || byte(p[0], 3) > (board ? 1u : 0u))
        return false;
    for (uint32_t i = 0; i < TC_LOOK_ROLES; ++i) if (byte(p[1u + i / 4u], i % 4u) > TC_PALETTE) return false;
    return true;
}
/* A look on show (a cleared one: the built-in look); the board's also brings Keep look. */
static void look_apply(const uint32_t p[ST_PAYLOAD], tc_controls *c, bool board) {
    c->led_scheme = byte(p[0], 1); c->color_by = byte(p[0], 2);
    if (board) c->keep_look = byte(p[0], 3);
    for (uint32_t i = 0; i < TC_LOOK_ROLES; ++i) c->look_roles[i] = byte(p[1u + i / 4u], i % 4u);
}
/* The board's look back on show: as stored, or the built-in look when it has no record (its write failed). */
static void board_look(tc_controls *c) {
    if (idx.look[0]) { look_apply(payload_at(idx.look[0]), c, false); return; }
    c->led_scheme = 0u; c->color_by = 0u; memset(c->look_roles, 0, sizeof c->look_roles);
}
/* What the live settings' two bits say of the board's scheme: the first three as they are, the rest Default. */
static uint8_t live_scheme(const tc_controls *c) {
    uint8_t scheme = c->look_slot && look_present(0) ? byte(payload_at(idx.look[0])[0], 1) : c->led_scheme;
    return scheme < TC_LED_SCHEMES_V1 ? scheme : 0u;
}
static void save_look(tc_controls *c);
/* The look showing stored where it belongs before another covers it: the board's even when the live bits
 * could say it (its scheme may not fit them), and (1.9.3) changes not saved yet, the board's or a preset's. */
static void keep_board_look(tc_controls *c) {
    if (c->look_slot || look_present(0)) { save_look(c); return; }
    uint32_t p[ST_PAYLOAD]; look_payload(c, true, p);
    if (!write_record(ST_LOOK, 0, p)) c->last_action = TC_SAVE_FAILED; /* was silent before the 2.3 review */
}
/* The look showing, saved where it belongs (with the live settings): the board's only once it says more
 * than the live bits; Keep look always in the board's. */
static void save_look_keep(tc_controls *c) { /* 2.4.5: save_look's two records, one a pass from st_service */
    uint32_t s = c->look_slot % TC_LOOKS;
    if (!s || !look_present(0) || byte(payload_at(idx.look[0])[0], 3) == c->keep_look) return;
    uint32_t b[ST_PAYLOAD]; memcpy(b, payload_at(idx.look[0]), sizeof b);
    b[0] = (b[0] & 0x00FFFFFFu) | (uint32_t)c->keep_look << 24;
    if (!write_record(ST_LOOK, 0, b)) c->last_action = TC_SAVE_FAILED;
}
static void save_look_slot(tc_controls *c) {
    uint32_t s = c->look_slot % TC_LOOKS, p[ST_PAYLOAD];
    look_payload(c, s == 0u, p);
    bool plain = byte(p[0], 1) < TC_LED_SCHEMES_V1 && !(p[0] >> 16);
    for (uint32_t i = 1u; i < 10u; ++i) plain = plain && !p[i];
    if (look_present(s) ? !memcmp(p, payload_at(idx.look[s]), sizeof p) : (!s && plain)) return;
    if (!write_record(ST_LOOK, (uint8_t)s, p)) c->last_action = TC_SAVE_FAILED;
}
static void save_look(tc_controls *c) { save_look_keep(c); save_look_slot(c); }
/* The selected slot's progression into sq (empty when there is none). */
static void load_progression(uint8_t slot, uint8_t scale) {
    if (slot < SQ_SLOTS && idx.prog[slot]) { sq = sq_slots[slot]; sq_changes++; return; } /* 2.0.0: with its chances */
    sq_clear(&sq, SQ_UNIT_BAR, scale);
}
/* CRC-valid record in log slot s (st_load's first walk found it so: crc_good): its kind/slot index pointer, else NULL. */
static uint32_t *valid(uint32_t s, uint8_t *kind, uint8_t *slot) {
    if (!(((uint32_t)crc_good[s / 8u] >> (s % 8u)) & 1u)) return NULL;
    uint32_t head = slot_words(s)[2];
    if (head >> 16) return NULL;
    *kind = (uint8_t)(head >> 8); *slot = (uint8_t)head;
    return index_of(*kind, *slot);
}
/* 2.4.0: the records the log holds now (the newest of each kind/slot; cleared ones too). */
static uint32_t current_records(void) {
    const uint32_t *all = (const uint32_t *)&idx;
    uint32_t n = 0u;
    for (uint32_t i = 0; i < sizeof idx / sizeof *all; ++i) if (all[i]) n++;
    return n;
}
/* 2.4.6 (review TBR-009): the records compaction copies from flash (they exist nowhere else) and the live one (its
 * encoding reads the look index the erase clears), held here across the erase; each kind's slots in order. Before,
 * all of compaction's copies were locals: 12.9 KB of stack, 84.5 % of it on the deepest chain. The rest are rebuilt
 * from RAM after the erase. */
#define ST_STAGED (2u + 3u * TC_LOOKS + 2u * SQ_SLOTS + TC_PRESETS + 1u) /* map (2), own/feel/look, chance/prog, presets, live */
static struct { uint8_t kind, slot; uint32_t p[ST_PAYLOAD]; } staged[ST_STAGED];
static uint32_t staged_count;
static bool rewrite_failed;
static uint32_t *stage(uint8_t kind, uint8_t slot) {
    staged[staged_count].kind = kind; staged[staged_count].slot = slot;
    return staged[staged_count++].p;
}
static void stage_copy(uint8_t kind, uint8_t slot, uint32_t at) { memcpy(stage(kind, slot), payload_at(at), ST_PAYLOAD * 4u); }
/* 2.4.6 (review TBR-018): a record rewritten after the erase, tried once more in the next slot if it fails. */
static void rewrite(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD]) {
    if (!write_record(kind, slot, p) && !write_record(kind, slot, p)) rewrite_failed = true;
}
static void rewrite_staged(uint8_t kind) {
    for (uint32_t i = 0; i < staged_count; ++i) if (staged[i].kind == kind) rewrite(kind, staged[i].slot, staged[i].p);
}
/* Before USB (st_load, once half the sector is used): erase it and rewrite the current records, in the same order as
 * before 2.4.6. False when the erase or a record failed (2.4.6, review TBR-018: st_load then shows NOT SAVED). */
static __attribute__((noinline)) bool compact(const tc_controls *c) {
    staged_count = 0u;
    if (stored(idx.map)) stage_copy(ST_MIDI_MAP, 0, idx.map); /* 2.1.0 */
    if (stored(idx.map2)) stage_copy(ST_MIDI_MAP, 1, idx.map2); /* 2.4.13 */
    for (uint8_t i = 0; i < TC_LOOKS; ++i) {
        if (stored(idx.own[i])) stage_copy(ST_OWN_SET, i, idx.own[i]); /* 2.0.0 */
        if (feel_present(i)) stage_copy(ST_FEEL, i, idx.feel[i]);
        if (look_present(i)) stage_copy(ST_LOOK, i, idx.look[i]);
    }
    for (uint8_t i = 0; i < SQ_SLOTS; ++i) {
        if (stored(idx.chance[i])) stage_copy(ST_CHANCE, i, idx.chance[i]);
        if (idx.prog[i]) stage_copy(ST_PROG, i, idx.prog[i]);
    }
    for (uint8_t i = 0; i < TC_PRESETS; ++i) if (preset_present(i)) stage_copy(ST_PRESET, i, idx.preset[i]);
    encode(c, stage(ST_LIVE, 0));
    /* Which of the records rebuilt from RAM the log held (it is about to be cleared). */
    uint32_t had_set = 0u, had_scale = 0u, had_lane[LN_PATTERNS];
    uint32_t had_pattern = 0u;
    bool had_favourites = idx.favourites != 0u, had_song = idx.song != 0u;
    for (uint32_t i = 0; i < TC_CHORD_SETS; ++i) if (idx.set[i]) had_set |= 1u << i;
    for (uint32_t i = 0; i < TC_USER_SCALES; ++i) if (idx.scale[i]) had_scale |= 1u << i;
    memset(had_lane, 0, sizeof had_lane); /* rewritten from ln_bank */
    for (uint32_t i = 0; i < LN_RECORDS; ++i) if (idx.lane[i]) had_lane[i / LN_PATTERN_RECORDS] |= 1u << (i % LN_PATTERN_RECORDS);
    for (uint32_t i = 0; i < LN_PATTERNS; ++i) if (stored(idx.pattern[i])) had_pattern |= 1u << i; /* 2.4.0 */
    if (st_before_erase) st_before_erase(); /* SAVING SETTINGS: losing power from here to the rewrite loses them all */
    writable = fs_erase();
    next_slot = 0u; records = 0u; memset(&idx, 0, sizeof idx);
    if (!writable) return false;
    rewrite_failed = false;
    uint32_t p[ST_PAYLOAD];
    rewrite_staged(ST_MIDI_MAP);
    lights_payload(c, p); if (p[0]) rewrite(ST_LIGHTS, 0, p); /* 2.4.0 */
    extra_payload(c, p); /* 2.5.1: from RAM, as the lights */
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) if (p[i]) { rewrite(ST_EXTRA, 0, p); break; }
    rewrite_staged(ST_OWN_SET);
    rewrite_staged(ST_CHANCE);
    if (sq_chain.count) { sq_chain_encode(&sq_chain, p); rewrite(ST_CHAIN, 0, p); }
    rewrite_staged(ST_FEEL);
    if (names_any()) { names_payload(p); rewrite(ST_NAMES, 0, p); }
    rewrite_staged(ST_LOOK);
    for (uint8_t i = 0; i < TC_CHORD_SETS; ++i) if ((had_set >> i) & 1u) { set_payload(i, p); rewrite(ST_CHORDS, i, p); }
    for (uint8_t i = 0; i < TC_USER_SCALES; ++i) if ((had_scale >> i) & 1u) { scale_payload(i, p); rewrite(ST_SCALE, i, p); }
    if (had_favourites) { favourites_payload(p); rewrite(ST_FAVOURITES, 0, p); }
    rewrite_staged(ST_PROG);
    for (uint32_t i = 0; i < LN_RECORDS; ++i) if ((had_lane[i / LN_PATTERN_RECORDS] >> (i % LN_PATTERN_RECORDS)) & 1u) {
        ln_encode(&ln_bank[i / LN_PATTERN_RECORDS], i % LN_PATTERN_RECORDS, p); rewrite(lane_kind(i), lane_slot(i), p);
    }
    for (uint8_t i = 0; i < LN_PATTERNS; ++i) if ((had_pattern >> i) & 1u) { ln_extras_encode(&ln_bank[i], p); rewrite(ST_PATTERN, i, p); }
    if (had_song) { ln_song_encode(p); rewrite(ST_SONG, 0, p); }
    rewrite_staged(ST_PRESET);
    rewrite_staged(ST_LIVE);
    return !rewrite_failed;
}
bool st_load(tc_controls *c) {
    records = 0u; sequence = 0u; memset(&idx, 0, sizeof idx); tc_favourite_count = 0u;
    memset(tc_preset_names, 0, sizeof tc_preset_names); memset(&sq_chain, 0, sizeof sq_chain);
    for (uint32_t i = 0; i < LN_PATTERNS; ++i) ln_clear(&ln_bank[i]); /* 2.4.0: core-coupled RAM isn't zeroed at start */
    memset(ln_dirty, 0, sizeof ln_dirty); ln_extra_dirty = 0u; ln_song_dirty = false; lane_seen = ln_changes;
    memset(lane_failed, 0, sizeof lane_failed); extra_failed = 0u; song_failed = false; tail_step = ST_TAIL_STEPS;
    memset(&ln_song, 0, sizeof ln_song); ln_current = 0u; ln_queued = LN_NO_PATTERN;
    /* Every slot: one whose write failed before programming anything stays erased, with records after it (2.3 review;
     * the scan stopped there, so those were lost and later overwritten). New records go after the last written. */
    next_slot = 0u; memset(crc_good, 0, sizeof crc_good);
    for (uint32_t s = 0; s < ST_SLOTS; ++s) {
        const uint32_t *w = slot_words(s);
        if (erased(w)) continue;
        next_slot = s + 1u;
        if (w[0] != ST_MAGIC || w[ST_WORDS - 1u] != tb_crc32_words(w, ST_WORDS - 1u)) continue;
        crc_good[s / 8u] = (uint8_t)((uint32_t)crc_good[s / 8u] | 1u << (s % 8u));
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
        if (at && kind == ST_FAVOURITES && !*at && favourites_set(slot_words(s) + 3u)) *at = s + 1u;
        sq_prog check;
        if (at && kind == ST_PROG && !*at && sq_decode(slot_words(s) + 3u, &check)) *at = s + 1u;
        tc_chord_set set;
        if (at && kind == ST_CHORDS && !*at && set_ok(slot_words(s) + 3u, &set)) {
            *at = s + 1u;
            (void)tc_chord_set_put(slot, &set);
        }
        if (at && lane_record(kind, slot) && !*at && ln_decode(slot_words(s) + 3u, lane_index(kind, slot) % LN_PATTERN_RECORDS,
                                                               &ln_bank[lane_index(kind, slot) / LN_PATTERN_RECORDS])) *at = s + 1u;
        if (at && kind == ST_PATTERN && !*at && ln_extras_decode(slot_words(s) + 3u, &ln_bank[slot])) *at = s + 1u; /* 2.4.0 */
        if (at && kind == ST_SONG && !*at && ln_song_decode(slot_words(s) + 3u, &ln_song, &ln_current)) *at = s + 1u;
        if (at && kind == ST_LOOK && !*at && look_ok(slot_words(s) + 3u, slot == 0u)) *at = s + 1u; /* 1.9.0 */
        uint8_t feel[TC_FEEL_BYTES], keep;
        if (at && kind == ST_FEEL && !*at && feel_read(slot_words(s) + 3u, feel, slot ? NULL : &keep)) *at = s + 1u;
        if (at && kind == ST_NAMES && !*at && names_read(slot_words(s) + 3u, tc_preset_names)) *at = s + 1u; /* 1.9.3 */
        if (at && kind == ST_OWN_SET && !*at && own_ok(slot_words(s) + 3u, slot)) *at = s + 1u; /* 2.0.0 */
        if (at && kind == ST_CHAIN && !*at && sq_chain_decode(slot_words(s) + 3u, &sq_chain)) *at = s + 1u;
        if (at && kind == ST_CHANCE && !*at && sq_chances_decode(slot_words(s) + 3u, &check)) *at = s + 1u;
        if (at && kind == ST_MIDI_MAP && !*at && map_read(slot_words(s) + 3u, slot, NULL)) *at = s + 1u; /* 2.1.0 */
        if (at && kind == ST_LIGHTS && !*at && lights_read(slot_words(s) + 3u, NULL)) *at = s + 1u; /* 2.4.0 */
        if (at && kind == ST_EXTRA && !*at && extra_read(slot_words(s) + 3u, NULL)) *at = s + 1u; /* 2.5.1 */
    }
    /* Presets are checked when loaded (one may name a user scale cleared since). */
    bool found = false;
    for (uint32_t s = next_slot; s-- > 0u;) {
        uint8_t kind, slot;
        uint32_t *at = valid(s, &kind, &slot);
        if (!at || (kind != ST_LIVE && kind != ST_PRESET) || *at) continue;
        if (kind == ST_LIVE && !decode(slot_words(s) + 3u, c)) continue; /* decode writes c only when valid */
        *at = s + 1u;
        if (kind == ST_LIVE) found = true;
    }
    c->look_slot = 0u;
    if (look_present(0)) look_apply(payload_at(idx.look[0]), c, true); /* the board's look (1.9.0) */
    c->feel_slot = 0u;
    if (idx.feel[0]) (void)feel_read(payload_at(idx.feel[0]), c->feel, &c->keep_feel); /* the curves (1.9.0) */
    c->chords_slot = 0u; c->keep_chords = keep_chords_stored(); /* 2.0.0 */
    if (idx.map) (void)map_read(payload_at(idx.map), 0u, c); /* 2.1.0 */
    if (idx.map2) (void)map_read(payload_at(idx.map2), 1u, c); /* 2.4.13 */
    if (idx.lights) (void)lights_read(payload_at(idx.lights), c); /* 2.4.0 */
    memset(extra_presets, 0, sizeof extra_presets);
    if (idx.extra) (void)extra_read(payload_at(idx.extra), c); /* 2.5.1: after the live settings */
    writable = true;
    /* 2.4.0: compacted once half the sector is used and a quarter of it can be reclaimed (with 8 patterns of 8 full
     * lanes the current records alone are up to 342: compacting at half full would erase the sector at every start) */
    if (next_slot >= ST_ERASE_AT && next_slot >= current_records() + ST_SLOTS / 4u && !compact(c))
        c->last_action = TC_SAVE_FAILED; /* 2.4.6 (review TBR-018): NOT SAVED: RESTART on the play screen */
    note_presets();
    for (uint8_t i = 0; i < SQ_SLOTS; ++i) bank(i);
    load_progression(c->prog_slot, c->scale);
    saved_changes = seen_changes = c->changes;
    return found;
}
/* One record of the patterns' changes (2.4.5, review TBR-007: one a pass; before, every changed chunk of every
 * pattern went in one pass, about 1 ms each, and held playing up by tens of ms after a pattern copy). The order is
 * as before: each pattern's chunks, then its windows, solos and follows; the song last. A chunk of defaults that was
 * never stored stays unstored. True when a record was written or tried, false when nothing is left to try. A write
 * that fails (the sector is full until the next start compacts it, or a write failed; a failed slot is never reused)
 * sets *failed and stays marked, tried again after the next edit. */
static bool save_lane_step(bool *failed) {
    for (uint32_t pattern = 0; pattern < LN_PATTERNS; ++pattern) {
        const ln_pattern *pat = &ln_bank[pattern];
        for (uint32_t i = 0; i < LN_PATTERN_RECORDS; ++i) {
            uint32_t slot = pattern * LN_PATTERN_RECORDS + i, bit = 1u << i;
            if (!(ln_dirty[pattern] & bit) || (lane_failed[pattern] & bit)) continue;
            if (!idx.lane[slot] && ln_chunk_default(pat, i)) { ln_dirty[pattern] &= ~bit; continue; }
            uint32_t p[ST_PAYLOAD];
            ln_encode(pat, i, p);
            if (write_record(lane_kind(slot), lane_slot(slot), p)) ln_dirty[pattern] &= ~bit;
            else { lane_failed[pattern] |= bit; *failed = true; }
            return true;
        }
        uint8_t bit = (uint8_t)(1u << pattern); /* 2.4.0: its windows, solos, follows */
        if (!(ln_extra_dirty & bit) || (extra_failed & bit)) continue;
        uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
        ln_extras_encode(pat, p);
        if (idx.pattern[pattern]) memcpy(q, payload_at(idx.pattern[pattern]), sizeof q); else memset(q, 0, sizeof q);
        if (!memcmp(p, q, sizeof p)) { ln_extra_dirty = (uint8_t)(ln_extra_dirty & ~bit); continue; }
        if (write_record(ST_PATTERN, (uint8_t)pattern, p)) ln_extra_dirty = (uint8_t)(ln_extra_dirty & ~bit);
        else { extra_failed = (uint8_t)(extra_failed | bit); *failed = true; }
        return true;
    }
    if (!ln_song_dirty || song_failed) return false; /* 2.4.0 */
    uint32_t p[ST_PAYLOAD];
    ln_song_encode(p);
    if (idx.song && !memcmp(p, payload_at(idx.song), sizeof p)) { ln_song_dirty = false; return false; }
    if (write_record(ST_SONG, 0, p)) ln_song_dirty = false;
    else { song_failed = true; *failed = true; }
    return true;
}
static bool lanes_dirty(void) {
    for (uint32_t i = 0; i < LN_PATTERNS; ++i) if (ln_dirty[i]) return true;
    return ln_extra_dirty || ln_song_dirty;
}
/* st_service's savers, one a pass (2.4.5), in the order they all ran in one pass before; each writes at most one
 * record. */
static void save_live(tc_controls *c) {
    uint32_t p[ST_PAYLOAD];
    encode(c, p);
    if (!write_record(ST_LIVE, 0, p)) c->last_action = TC_SAVE_FAILED; /* 1.7.0: was silent */
}
static void store_map(tc_controls *c) { if (!save_map(c)) c->last_action = TC_SAVE_FAILED; } /* 2.1.0 */
static void store_lights(tc_controls *c) { if (!save_lights(c)) c->last_action = TC_SAVE_FAILED; } /* 2.4.0 */
static void store_extra(tc_controls *c) { if (!save_extra(c)) c->last_action = TC_SAVE_FAILED; } /* 2.5.1 */
static void (*const savers[])(tc_controls *) = {
    save_live, save_look_keep, save_look_slot, save_feel_keep, save_feel_slot, save_chords_keep, save_chords_own,
    store_map, store_lights, store_extra,
};
_Static_assert(sizeof savers / sizeof savers[0] == ST_TAIL_STEPS, "one step per saver");
bool st_service(tc_controls *c, uint32_t now) {
    static uint32_t lane_tried = UINT32_MAX;
    if (ln_changes != lane_seen) {
        lane_seen = ln_changes; lane_changed_at = now;
        memset(lane_failed, 0, sizeof lane_failed); extra_failed = 0u; song_failed = false; /* tried again */
    }
    if (lanes_dirty() && ln_changes != lane_tried && now - lane_changed_at >= ST_SAVE_DELAY_MS) {
        bool failed = false;
        if (save_lane_step(&failed)) {
            if (failed) c->last_action = TC_SAVE_FAILED; /* NOT SAVED: RESTART (1.7.0; was silent) */
            return true;
        }
        lane_tried = ln_changes; /* only failed ones left: they wait for the next edit */
    }
    if (c->changes != seen_changes) { seen_changes = c->changes; changed_at = now; tail_step = ST_TAIL_STEPS; }
    if (tail_step >= ST_TAIL_STEPS) {
        if (c->changes == saved_changes || now - changed_at < ST_SAVE_DELAY_MS) return false;
        saved_changes = c->changes; tail_step = 0u;
    }
    while (tail_step < ST_TAIL_STEPS) { /* a saver with nothing to write costs no pass */
        uint32_t before = next_slot;
        savers[tail_step++](c);
        if (next_slot != before) return true;
    }
    return false;
}
/* A preset is the whole musical setup (1.7.0: each half's program too); brightness, the MIDI-in lights,
 * pad colors, the jack settings (TRS out, pedal and its note), clock out, the arrows, the pad
 * thresholds, which half the chords are on and Program in belong to the board and stay. */
static void keep_board(tc_controls *t, const tc_controls *c) {
    t->brightness = c->brightness; t->midi_leds = c->midi_leds;
    t->trs_out = c->trs_out; t->pedal_mode = c->pedal_mode; t->pedal_type = c->pedal_type; t->led_scheme = c->led_scheme;
    t->clock_out = c->clock_out; t->pedal_note = c->pedal_note; t->arrows = c->arrows; t->chords_right = c->chords_right;
    t->whole_join = c->whole_join; t->right_octave = c->right_octave; t->whole_split = c->whole_split; /* 2.4.9 */
    t->pad_trigger = c->pad_trigger; t->pad_release = c->pad_release; t->pad_full = c->pad_full; t->unstick = c->unstick;
    t->pad_vel_full = c->pad_vel_full; /* 1.8.3: the board's, like the thresholds */
    t->step_silent = c->step_silent;   /* 1.9.4: the board's, like Unstick */
    t->pedal_target = c->pedal_target; t->prog_slot = c->prog_slot; t->seq_guide = c->seq_guide;
    t->lock = c->lock; memcpy(t->buttons, c->buttons, sizeof t->buttons); t->shift_latched = c->shift_latched; t->button_sustain = c->button_sustain;
    t->program_in = c->program_in; /* 1.7.0: the channel presets are chosen from stays */
    t->chain = c->chain; t->u3_shift = c->u3_shift; t->sleep = c->sleep; /* 2.0.0: the board's */
    t->step_grid = c->step_grid;   /* 2.2.0: how the step screen lays out the steps */
    t->bend_range = c->bend_range; /* 2.4.12: the synth's */
    t->out_scale = c->out_scale; t->dim = c->dim; /* 2.4.0: the lights' */
    t->program_again = 3u;        /* each half's program goes out again, also when it is the same */
}
/* The preset's settings, the board's own as they are (false: empty, or it can't load now). */
static bool preset_controls(const tc_controls *c, uint8_t slot, tc_controls *t) {
    if (!preset_present(slot)) return false;
    *t = *c;
    if (!decode(payload_at(idx.preset[slot]), t)) return false;
    extra_part_apply(extra_presets[slot], t); /* 2.5.1: its exact arpeggiator swing */
    keep_board(t, c);
    /* 2.4.9: in All Melody (the board's) the preset's melody octave comes down as far as all 96 pads need */
    tp_layout l = tc_layout(t);
    while (l.whole && !tc_layout_allowed(t, l) && l.melody_base >= 12u) l.melody_base = (uint8_t)(l.melody_base - 12u);
    if (!tc_layout_allowed(t, l)) return false;
    t->melody_base = l.melody_base;
    return true;
}
static bool load_preset(tc_controls *c, tp_engine *e, uint8_t slot) {
    tc_controls t;
    if (!preset_present(slot)) return false;
    /* 2.0.0: the preset's own chord set in its set's place, else the board's (back, if another preset's covered
     * it); the covered set's changes saved first. Its settings are checked with the sets it brings. */
    uint8_t own_set = own_present(slot + 1u) ? (uint8_t)(slot + 1u) : 0u, was = c->chords_slot;
    if (own_set || was) { save_chords(c); uncover(c); cover(c, own_set); }
    if (!preset_controls(c, slot, &t)) {
        if (own_set || was) { uncover(c); cover(c, was); }
        return false;
    }
    t.chords_slot = c->chords_slot; t.chords_cover = c->chords_cover;
    *c = t;
    /* 1.9.0: the preset's own look, else the board's (back, if another preset's showed) */
    uint8_t own = look_present(slot + 1u) ? (uint8_t)(slot + 1u) : 0u;
    if (own || c->look_slot) {
        keep_board_look(c);
        if (own) look_apply(payload_at(idx.look[own]), c, false); else board_look(c);
        c->look_slot = own;
    }
    /* 1.9.3: the preset's own curves, else the board's (back, if another preset's were in use) */
    uint8_t own_feel = feel_present(slot + 1u) ? (uint8_t)(slot + 1u) : 0u;
    if (own_feel || c->feel_slot) {
        save_feel(c); /* the curves in use, changes not saved yet too, before others cover them */
        if (own_feel) (void)feel_read(payload_at(idx.feel[own_feel]), c->feel, NULL); else board_feel(c->feel);
        c->feel_slot = own_feel;
    }
    e->layout = tc_layout(c);
    c->changes++;
    return true;
}
/* 2.0.7, Clear preset: preset slot, and its own look, curves and chord set, emptied (only those there: no flash
 * written for nothing), its name too; any of them in use now gives way to the board's. */
static void clear_preset(tc_controls *c, uint8_t slot) {
    uint32_t zero[ST_PAYLOAD], own = (uint32_t)slot + 1u;
    bool ok = slot < TC_PRESETS;
    memset(zero, 0, sizeof zero);
    if (ok && preset_present(slot)) ok = write_record(ST_PRESET, slot, zero);
    if (ok && slot < TC_PRESETS) { memset(extra_presets[slot], 0, sizeof extra_presets[slot]); ok = save_extra(c); } /* 2.5.1 */
    if (ok && look_present(own)) ok = write_record(ST_LOOK, (uint8_t)own, zero);
    if (ok && feel_present(own)) ok = write_record(ST_FEEL, (uint8_t)own, zero);
    if (ok && own_present(own)) ok = write_record(ST_OWN_SET, (uint8_t)own, zero);
    if (ok && tc_preset_names[slot][0]) {
        uint32_t p[ST_PAYLOAD];
        memset(tc_preset_names[slot], 0, sizeof tc_preset_names[slot]);
        names_payload(p); ok = write_record(ST_NAMES, 0, p);
    }
    if (ok && c->look_slot == own) { board_look(c); c->look_slot = 0u; }
    if (ok && c->feel_slot == own) { board_feel(c->feel); c->feel_slot = 0u; }
    if (ok && c->chords_slot == own) uncover(c);
    note_presets();
    if (ok) { c->cleared = slot; c->last_action = TC_PRESET_CLEARED; c->changes++; }
    else c->last_action = TC_SAVE_FAILED;
}
static void carry_out(tc_controls *c, tp_engine *e, uint8_t kind, uint8_t slot) {
    if (kind == TC_REQUEST_LOAD && c->recording) c->last_action = TC_SEQ_BUSY; /* it may bring another chord set */
    else if (kind == TC_REQUEST_LOAD) {
        /* A save on the way (the look, curves or chord set in use, stored before the preset's cover them) that failed
         * says NOT SAVED, not the load's message over it (2.3 review). */
        c->last_action = TC_NONE;
        bool loaded = load_preset(c, e, slot);
        if (loaded) c->preset = slot;
        if (c->last_action != TC_SAVE_FAILED) c->last_action = loaded ? TC_PRESET_LOADED : TC_PRESET_EMPTY;
    } else if (kind == TC_REQUEST_PROG_LOAD) {
        load_progression(slot, c->scale);
    } else if (kind == TC_REQUEST_PROG_SAVE) {
        uint32_t p[ST_PAYLOAD];
        sq_encode(&sq, p);
        if (!write_record(ST_PROG, slot, p)) c->last_action = TC_SAVE_FAILED;
        if (slot < SQ_SLOTS) { save_chances(c, slot); bank(slot); sq_chain_changes++; } /* 2.0.0: a new recording plays every chord */
        note_presets();
    } else if (kind == TC_REQUEST_CLEAR) { /* 2.0.7: the preset, its own look, curves and chord set, its name */
        if (c->recording) { c->last_action = TC_SEQ_BUSY; c->last_source = 0u; c->event_sequence++; return; }
        clear_preset(c, slot);
    } else if (kind == TC_REQUEST_SET_SAVE) { /* a captured chord set (1.4.0) */
        uint32_t p[ST_PAYLOAD];
        if (slot >= TC_CHORD_SETS) c->last_action = TC_SAVE_FAILED; /* 2.4.8 (review TBR-029): tested before it is read */
        else if (c->chords_slot && slot == c->chords_cover) save_chords(c); /* 2.0.0: covered: into the preset's own */
        else { set_payload(slot, p); if (!write_record(ST_CHORDS, slot, p)) c->last_action = TC_SAVE_FAILED; }
    } else {
        uint32_t p[ST_PAYLOAD];
        encode(c, p);
        if (write_record(ST_PRESET, slot, p)) { c->preset = slot; c->last_action = TC_PRESET_SAVED; }
        else c->last_action = TC_SAVE_FAILED;
        if (slot < TC_PRESETS) { extra_part(c, extra_presets[slot]); if (!save_extra(c)) c->last_action = TC_SAVE_FAILED; } /* 2.5.1 */
        /* 1.9.0, Keep look: the look showing goes with the preset; else the preset has none (any it had goes) */
        uint32_t lp[ST_PAYLOAD];
        if (c->keep_look) look_payload(c, false, lp); else memset(lp, 0, sizeof lp);
        if ((c->keep_look || look_present(slot + 1u)) && slot < TC_PRESETS) {
            keep_board_look(c);
            if (!write_record(ST_LOOK, (uint8_t)(slot + 1u), lp)) c->last_action = TC_SAVE_FAILED;
            if (!c->keep_look && c->look_slot == slot + 1u) { board_look(c); c->look_slot = 0u; }
        }
        /* 1.9.3, Keep feel: the curves in use go with the preset; else the preset has none (any it had goes) */
        if ((c->keep_feel || feel_present(slot + 1u)) && slot < TC_PRESETS) {
            uint32_t fp[ST_PAYLOAD];
            if (c->keep_feel) feel_payload(c->feel, 0u, fp); else memset(fp, 0, sizeof fp);
            save_feel(c);
            if (!write_record(ST_FEEL, (uint8_t)(slot + 1u), fp)) c->last_action = TC_SAVE_FAILED;
            if (!c->keep_feel && c->feel_slot == slot + 1u) { board_feel(c->feel); c->feel_slot = 0u; }
        }
        /* 2.0.0, Keep chords: a copy of the chord set in use goes with the preset; else it has none (any it had goes) */
        if ((c->keep_chords || own_present(slot + 1u)) && slot < TC_PRESETS) {
            uint32_t op[ST_PAYLOAD];
            bool keep = c->keep_chords && c->chord_set && c->chord_set <= TC_CHORD_SETS && tc_chord_set_defined((uint8_t)(c->chord_set - 1u));
            if (keep) own_payload((uint8_t)(c->chord_set - 1u), op); else memset(op, 0, sizeof op);
            save_chords(c);
            if (!write_record(ST_OWN_SET, (uint8_t)(slot + 1u), op)) c->last_action = TC_SAVE_FAILED;
            if (c->chords_slot == slot + 1u) { uncover(c); cover(c, (uint8_t)(slot + 1u)); } /* as now saved */
        }
        note_presets();
    }
    c->last_source = 0u; c->event_sequence++; /* redraw */
}
/* 2.4.4 (review TBR-028): every request waiting (controls.c tc_request), oldest first, then the newest. Bounded: one
 * a request makes while carried out waits for the next pass. */
void st_requests(tc_controls *c, tp_engine *e) {
    for (uint32_t n = 0; n <= TC_REQUEST_QUEUE && (c->queued || c->request != TC_REQUEST_NONE); ++n) {
        uint8_t kind, slot;
        if (c->queued) {
            kind = c->queue[0].kind; slot = c->queue[0].slot; c->queued--;
            for (uint32_t i = 0; i < c->queued; ++i) c->queue[i] = c->queue[i + 1u];
        } else { kind = c->request; slot = c->request_slot; c->request = TC_REQUEST_NONE; }
        carry_out(c, e, kind, slot);
    }
}
uint32_t st_get(uint8_t kind, uint8_t slot, const tc_controls *c, uint32_t p[ST_PAYLOAD]) {
    if (kind == ST_LOOK && slot == ST_LOOK_NOW) { /* the look showing, with Keep look, and whose it is */
        look_payload(c, true, p); p[ST_PAYLOAD - 1u] = c->look_slot; return ST_OK;
    }
    if (kind == ST_LOOK && slot < TC_LOOKS && !look_present(slot)) return ST_ABSENT;
    if (kind == ST_FEEL && slot == ST_FEEL_NOW) { /* the curves in use, with Keep feel, and whose they are (1.9.3) */
        feel_payload(c->feel, c->keep_feel, p); p[ST_PAYLOAD - 1u] = c->feel_slot; return ST_OK;
    }
    if (kind == ST_FEEL && slot == 0u) { /* the board's curves (the defaults too), with Keep feel */
        uint8_t board[TC_FEEL_BYTES];
        if (c->feel_slot) board_feel(board); else memcpy(board, c->feel, sizeof board);
        feel_payload(board, c->keep_feel, p); return ST_OK;
    }
    if (kind == ST_FEEL && slot < TC_LOOKS && !feel_present(slot)) return ST_ABSENT;
    if (kind == ST_NAMES && slot == 0u) { names_payload(p); return names_any() ? ST_OK : ST_ABSENT; } /* 1.9.3 */
    if (kind == ST_OWN_SET && slot == ST_OWN_SET_NOW) { /* 2.0.0: the covered set as it plays, Keep chords, whose */
        if (c->chords_slot) own_payload(c->chords_cover, p); else { memset(p, 0, ST_PAYLOAD * 4u); p[0] = 1u; }
        p[0] |= (uint32_t)(c->keep_chords ? 1u : 0u) << 16; p[ST_PAYLOAD - 1u] = c->chords_slot; return ST_OK;
    }
    if (kind == ST_OWN_SET && slot == 0u) { memset(p, 0, ST_PAYLOAD * 4u); p[0] = 1u | (uint32_t)(c->keep_chords ? 1u : 0u) << 16; return ST_OK; }
    if (kind == ST_OWN_SET && slot < TC_LOOKS && !own_present(slot)) return ST_ABSENT;
    if (kind == ST_CHAIN && slot == 0u) { sq_chain_encode(&sq_chain, p); return sq_chain.count ? ST_OK : ST_ABSENT; }
    if (kind == ST_MIDI_MAP && slot <= 1u) { /* 2.1.0: as it is now, with the last controller in (MIDI learn) */
        map_payload(c, slot, p);
        p[ST_PAYLOAD - 1u] = tm.cc_seen ? tm.cc_last | (uint32_t)tm.cc_last_value << 8 | (uint32_t)tm.cc_last_channel << 16 |
                                          (tm.cc_seen & 0xFFu) << 24 : 0u;
        return ST_OK;
    }
    if (kind == ST_LIGHTS && slot == 0u) { lights_payload(c, p); return p[0] ? ST_OK : ST_ABSENT; } /* 2.4.0: as they are */
    if (kind == ST_CHANCE && slot < SQ_SLOTS && !stored(idx.chance[slot])) return ST_ABSENT;
    if (kind == ST_CHORDS && c->chords_slot && slot == c->chords_cover) { /* covered: the board's own, as stored */
        tc_chord_set s; board_set(slot, &s); set_words(&s, p);
        for (uint32_t i = 0; i < 12u; ++i) if (s.column[i].mask) return ST_OK;
        return ST_ABSENT;
    }
    if (kind == ST_PRESET && slot < TC_PRESETS && !preset_present(slot)) return ST_ABSENT;
    uint32_t *at = index_of(kind, slot);
    if (!at) return ST_BAD_VALUE;
    if (kind == ST_LIVE) { encode(c, p); return ST_OK; }
    if (kind == ST_EXTRA) { /* 2.5.1: as it is now */
        extra_payload(c, p);
        for (uint32_t i = 0; i < ST_PAYLOAD; ++i) if (p[i]) return ST_OK;
        return ST_ABSENT;
    }
    if (kind == ST_SCALE) { scale_payload(slot, p); return tc_user_scale_get(slot)->mask ? ST_OK : ST_ABSENT; }
    if (kind == ST_FAVOURITES) { favourites_payload(p); return *at ? ST_OK : ST_ABSENT; }
    if (kind == ST_PROG && !*at) return ST_ABSENT;
    if (kind == ST_CHORDS) { set_payload(slot, p); return tc_chord_set_defined(slot) ? ST_OK : ST_ABSENT; }
    if (lane_record(kind, slot)) { /* as it plays now */
        uint32_t g = lane_index(kind, slot);
        const ln_pattern *pat = &ln_bank[g / LN_PATTERN_RECORDS];
        ln_encode(pat, g % LN_PATTERN_RECORDS, p); return ln_chunk_default(pat, g % LN_PATTERN_RECORDS) ? ST_ABSENT : ST_OK;
    }
    if (kind == ST_PATTERN) { ln_extras_encode(&ln_bank[slot], p); return p[0] ? ST_OK : ST_ABSENT; } /* 2.4.0 */
    if (kind == ST_SONG) { ln_song_encode(p); return ST_OK; }
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
/* 2.0.0, ST_OWN_SET: slot 9 changes the covered set as it plays (saved in its preset's own at once) or only Keep
 * chords; 0 Keep chords; 1..8 store a preset's own (the one covering: in place at once). A set in use must still
 * fit every pad. */
static uint32_t put_own(uint8_t slot, const uint32_t p[ST_PAYLOAD], tc_controls *c, tp_engine *e) {
    if (slot == ST_OWN_SET_NOW) {
        uint8_t set, keep = byte(p[0], 2); tc_chord_set s;
        if (p[ST_PAYLOAD - 1u] >= TC_LOOKS || keep > 1u) return ST_BAD_VALUE;
        if (!c->chords_slot) {
            for (uint32_t i = 1u; i < ST_PAYLOAD - 1u; ++i) if (p[i]) return ST_BAD_VALUE;
            if ((p[0] & ~0x10000u) != 1u) return ST_BAD_VALUE;
            c->keep_chords = keep; c->changes++;
            return ST_OK;
        }
        if (!own_read(p, true, &set, &s) || set != c->chords_cover) return ST_BAD_VALUE;
        tc_chord_set before = *tc_chord_set_get(set);
        if (!tc_chord_set_put(set, &s)) return ST_BAD_VALUE;
        if (c->chord_set == set + 1u && !tc_set_layout(c, e, tc_layout(c))) { (void)tc_chord_set_put(set, &before); return ST_BAD_VALUE; }
        c->keep_chords = keep; c->changes++;
        uint32_t q[ST_PAYLOAD]; own_payload(set, q);
        return write_record(ST_OWN_SET, c->chords_slot, q) ? ST_OK : ST_STORAGE;
    }
    if (slot >= TC_LOOKS || !own_ok(p, slot)) return ST_BAD_VALUE;
    if (!slot) {
        if (!write_record(ST_OWN_SET, 0, p)) return ST_STORAGE;
        c->keep_chords = byte(p[0], 2);
        return ST_OK;
    }
    if (slot == c->chords_slot) { /* in place: as stored (cleared: the board's again) */
        uint8_t was = c->chords_cover, set = 0u; tc_chord_set s, before = *tc_chord_set_get(was);
        if (p[0]) (void)own_read(p, false, &set, &s);
        uncover(c);
        if (p[0]) (void)tc_chord_set_put(set, &s);
        if (!tc_set_layout(c, e, tc_layout(c))) { /* doesn't fit: as it was */
            tc_chord_set board; board_set(set, &board); (void)tc_chord_set_put(set, &board);
            (void)tc_chord_set_put(was, &before); c->chords_slot = slot; c->chords_cover = was;
            return ST_BAD_VALUE;
        }
        if (p[0]) { c->chords_slot = slot; c->chords_cover = set; }
    }
    return write_record(ST_OWN_SET, slot, p) ? ST_OK : ST_STORAGE;
}
uint32_t st_put(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD], tc_controls *c, tp_engine *e) {
    if (kind == ST_LOOK) { /* 1.9.0: slot 9 changes the look showing (saved with the settings), 0..8 store */
        uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q);
        if (slot == ST_LOOK_NOW) {
            if (q[ST_PAYLOAD - 1u] >= TC_LOOKS) return ST_BAD_VALUE;
            q[ST_PAYLOAD - 1u] = 0u;
            if (!q[0] || !look_ok(q, true)) return ST_BAD_VALUE;
            look_apply(q, c, true); c->changes++;
            return ST_OK;
        }
        if (slot >= TC_LOOKS || !look_ok(q, slot == 0u)) return ST_BAD_VALUE;
        if (slot && !look_present(0)) keep_board_look(c);
        if (!write_record(ST_LOOK, slot, q)) return ST_STORAGE;
        if (!slot) c->keep_look = byte(q[0], 3);
        if (slot == c->look_slot) { /* showing: shown as stored (a preset's cleared: the board's again) */
            if (slot && !q[0]) { board_look(c); c->look_slot = 0u; }
            else look_apply(q, c, false);
        }
        return ST_OK;
    }
    if (kind == ST_FEEL) { /* 1.9.0; 1.9.3: slot 9 the curves in use, 0 the board's, 1..8 presets' own */
        uint8_t feel[TC_FEEL_BYTES], keep = 0u;
        if (slot == ST_FEEL_NOW) { /* in use at once, saved with the settings where they belong */
            uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q);
            if (q[ST_PAYLOAD - 1u] >= TC_LOOKS) return ST_BAD_VALUE;
            q[ST_PAYLOAD - 1u] = 0u;
            if (!q[0] || !feel_read(q, feel, &keep)) return ST_BAD_VALUE;
            memcpy(c->feel, feel, sizeof feel); c->keep_feel = keep; c->changes++;
            return ST_OK;
        }
        if (slot >= TC_LOOKS || !feel_read(p, feel, slot ? NULL : &keep)) return ST_BAD_VALUE;
        if (!slot) {
            c->keep_feel = keep;
            if (!c->feel_slot) { memcpy(c->feel, feel, sizeof feel); c->changes++; return ST_OK; } /* in use: as 1.9.0 */
            return write_record(ST_FEEL, 0, p) ? ST_OK : ST_STORAGE;
        }
        if (!write_record(ST_FEEL, slot, p)) return ST_STORAGE;
        if (slot == c->feel_slot) { /* in use: as stored (cleared: the board's again) */
            if (p[0]) memcpy(c->feel, feel, sizeof feel);
            else { board_feel(c->feel); c->feel_slot = 0u; }
        }
        return ST_OK;
    }
    if (kind == ST_NAMES) { /* 1.9.3 */
        char names[TC_PRESETS][TC_PRESET_NAME + 1u];
        if (slot || !names_read(p, names)) return ST_BAD_VALUE;
        if (!write_record(ST_NAMES, 0, p)) return ST_STORAGE;
        memcpy(tc_preset_names, names, sizeof names); c->event_sequence++;
        return ST_OK;
    }
    if (kind == ST_OWN_SET) return put_own(slot, p, c, e); /* 2.0.0 */
    if (kind == ST_MIDI_MAP) { /* 2.1.0: in use and stored at once */
        uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q); q[ST_PAYLOAD - 1u] = 0u;
        if (slot > 1u || !map_read(q, slot, NULL)) return ST_BAD_VALUE;
        (void)map_read(q, slot, c); c->changes++;
        return save_map(c) ? ST_OK : ST_STORAGE;
    }
    if (kind == ST_EXTRA) { /* 2.5.1: in use at once, stored with the live settings (st_service) */
        if (slot || !extra_read(p, NULL)) return ST_BAD_VALUE;
        (void)extra_read(p, c); c->changes++;
        return ST_OK;
    }
    if (kind == ST_LIGHTS) { /* 2.4.0: in use and stored at once */
        if (slot || !lights_read(p, NULL)) return ST_BAD_VALUE;
        (void)lights_read(p, c); c->changes++;
        return save_lights(c) ? ST_OK : ST_STORAGE;
    }
    if (kind == ST_CHAIN) {
        sq_chain_list chain;
        if (slot || !sq_chain_decode(p, &chain)) return ST_BAD_VALUE;
        if (!write_record(ST_CHAIN, 0, p)) return ST_STORAGE;
        sq_chain = chain; sq_chain_changes++;
        return ST_OK;
    }
    if (kind == ST_CHANCE) { /* plays at once, without starting the progression again */
        sq_prog check;
        if (slot >= SQ_SLOTS || !sq_chances_decode(p, &check)) return ST_BAD_VALUE;
        if (!write_record(ST_CHANCE, slot, p)) return ST_STORAGE;
        bank(slot);
        if (slot == c->prog_slot && !c->recording) (void)sq_chances_decode(p, &sq);
        return ST_OK;
    }
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
    if (kind == ST_PROG) { /* the selected slot also plays it at once (and ends a recording into it) */
        sq_prog t;
        if (!sq_decode(p, &t)) return ST_BAD_VALUE;
        if (!write_record(ST_PROG, slot, p)) return ST_STORAGE;
        bank(slot); sq_chain_changes++; /* 2.0.0: with its chances as stored */
        if (slot == c->prog_slot) { sq = sq_slots[slot]; sq_changes++; c->recording = false; }
        note_presets();
        return ST_OK;
    }
    if (kind == ST_CHORDS) { /* the set in use must still fit every pad (and cannot be removed) */
        tc_chord_set set, before = *tc_chord_set_get(slot);
        if (!set_ok(p, &set)) return ST_BAD_VALUE;
        if (c->chords_slot && slot == c->chords_cover) { /* 2.0.0: covered by a preset's own: the board's, stored */
            uint32_t stored[ST_PAYLOAD];
            if (!tc_chord_set_put(slot, &set)) return ST_BAD_VALUE;
            set_payload(slot, stored); (void)tc_chord_set_put(slot, &before);
            return write_record(ST_CHORDS, slot, stored) ? ST_OK : ST_STORAGE;
        }
        (void)tc_chord_set_put(slot, &set);
        if (c->chord_set == slot + 1u && !tc_set_layout(c, e, tc_layout(c))) {
            (void)tc_chord_set_put(slot, &before);
            return ST_BAD_VALUE;
        }
        uint32_t stored[ST_PAYLOAD];
        set_payload(slot, stored);
        return write_record(ST_CHORDS, slot, stored) ? ST_OK : ST_STORAGE;
    }
    if (lane_record(kind, slot)) { /* plays at once; stored now, so a later save leaves it alone */
        uint32_t g = lane_index(kind, slot), pattern = g / LN_PATTERN_RECORDS, local = g % LN_PATTERN_RECORDS;
        if (!ln_decode(p, local, &ln_bank[pattern])) return ST_BAD_VALUE;
        ln_dirty[pattern] &= ~(1u << local); ln_changes++; lane_seen = ln_changes;
        return write_record(kind, slot, p) ? ST_OK : ST_STORAGE;
    }
    if (kind == ST_PATTERN) { /* 2.4.0: plays at once; stored now */
        if (!ln_extras_decode(p, &ln_bank[slot])) return ST_BAD_VALUE;
        ln_extra_dirty = (uint8_t)(ln_extra_dirty & ~(1u << slot)); ln_changes++; lane_seen = ln_changes;
        return write_record(ST_PATTERN, slot, p) ? ST_OK : ST_STORAGE;
    }
    if (kind == ST_SONG) { /* 2.4.0: the pattern playing (as a pattern chosen: at the end of the one playing), Song */
        ln_song_list song; uint8_t current;
        if (!ln_song_decode(p, &song, &current)) return ST_BAD_VALUE;
        ln_song = song; if (current != ln_current) ln_queued = current;
        ln_song_changed(); lane_seen = ln_changes;
        uint32_t q[ST_PAYLOAD]; memcpy(q, p, sizeof q);
        if (!write_record(ST_SONG, 0, q)) return ST_STORAGE;
        ln_song_dirty = current != ln_current; /* stored again once the pattern playing has changed */
        return ST_OK;
    }
    if (kind == ST_FAVOURITES) {
        uint8_t before[TC_FAVOURITES], n = tc_favourite_count;
        memcpy(before, tc_favourites, n);
        if (!favourites_set(p)) return ST_BAD_VALUE;
        if (write_record(ST_FAVOURITES, 0, p)) return ST_OK;
        memcpy(tc_favourites, before, n); tc_favourite_count = n;
        return ST_STORAGE;
    }
    bool zeros = true;
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) zeros = zeros && !p[i];
    if (kind == ST_PRESET && zeros) { /* 1.9.3: clears the preset */
        uint32_t result = write_record(ST_PRESET, slot, p) ? ST_OK : ST_STORAGE;
        note_presets();
        return result;
    }
    tc_controls t = *c;
    if (!decode(p, &t)) return ST_BAD_VALUE;
    if (kind == ST_LIVE) {
        if (c->recording && t.chord_set != c->chord_set) return ST_BAD_VALUE; /* one set per progression */
        *c = t; e->layout = tc_layout(c); c->changes++;
        if (e->state == TK_ACTIVE) e->config = tc_pad_config(c);
        return ST_OK; /* st_service saves it */
    }
    uint32_t result = write_record(ST_PRESET, slot, p) ? ST_OK : ST_STORAGE;
    note_presets();
    return result;
}
uint32_t st_put_quiet(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD], tc_controls *c, tp_engine *e) {
    if (kind == ST_PATTERN) { /* 2.4.0: as lanes, saved 3 s after the last */
        if (slot >= LN_PATTERNS || !ln_extras_decode(p, &ln_bank[slot])) return ST_BAD_VALUE;
        ln_extra_changed(slot); return ST_OK;
    }
    if (!lane_record(kind, slot)) return st_put(kind, slot, p, c, e);
    uint32_t g = lane_index(kind, slot), pattern = g / LN_PATTERN_RECORDS, local = g % LN_PATTERN_RECORDS;
    if (!ln_decode(p, local, &ln_bank[pattern])) return ST_BAD_VALUE;
    ln_changed_in(pattern, local / LN_CHUNKS, (local % LN_CHUNKS) * LN_CHUNK); /* saved by st_service, 3 s after the last */
    return ST_OK;
}
uint32_t st_values(uint8_t kind, uint8_t slot, const tc_controls *c, tc_controls *out) {
    *out = *c;
    if (kind == ST_LIVE && !slot) return ST_OK;
    if (kind != ST_PRESET || slot >= TC_PRESETS) return ST_BAD_VALUE;
    if (!preset_present(slot)) return ST_ABSENT;
    return preset_controls(c, slot, out) ? ST_OK : ST_BAD_VALUE;
}
uint32_t st_records(void) { return records; }
