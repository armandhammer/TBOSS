/* SPDX-License-Identifier: GPL-3.0-or-later - settings log in flash sector 11 (flash_store.c).
 * Record = 44 words: magic, sequence, kind<<8|slot, 40 payload words, CRC-32 (format 3,
 * 0.6.0; installing erases the sector, so older formats never meet this firmware).
 * Records are appended; for each kind/slot the newest valid one wins. A record
 * interrupted by power loss fails its CRC and is skipped. Runtime saves only
 * program; the sector is erased only by st_load at boot, before USB starts (an
 * erase stalls flash for 1-2 s). Kinds: live settings, presets 0..7, scales 0..11.
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
 * Sector budget (1.5.0): 744 record slots, compacted at boot once 372 are used. The current records
 * are at most 12 scales, 8 chord sets, the favourites, 8 progressions, 8 presets, the live settings and
 * 16 pattern records (one pattern of 4 lanes x 4 chunks): 54, leaving over 300 saves per session.
 * Eight patterns (later) would be 128 pattern records, 166 in all, still under half the sector. */
#include "settings.h"
#include "flash_store.h"
#include "sequence.h"
#include "lanes.h"
#include <string.h>
_Static_assert(LN_WORDS == ST_PAYLOAD, "a pattern record is a settings record");
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
/* Newest valid record per kind/slot as log index + 1 (0 = none). */
static uint32_t live_at, preset_at[TC_PRESETS], scale_at[TC_USER_SCALES], favourites_at, prog_at[SQ_SLOTS];
static uint32_t set_at[TC_CHORD_SETS], lane_at[LN_RECORDS], look_at[TC_LOOKS], feel_at[TC_LOOKS], names_at;
static uint32_t own_at[TC_LOOKS], chain_at, chance_at[SQ_SLOTS]; /* 2.0.0 */
static uint32_t lane_seen, lane_changed_at; /* ln_changes seen, and when it last changed */
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
    if (kind == ST_FAVOURITES && slot == 0u) return &favourites_at;
    if (kind == ST_PROG && slot < SQ_SLOTS) return &prog_at[slot];
    if (kind == ST_CHORDS && slot < TC_CHORD_SETS) return &set_at[slot];
    if (kind == ST_LANE && slot < LN_RECORDS) return &lane_at[slot];
    if (kind == ST_LOOK && slot < TC_LOOKS) return &look_at[slot];
    if (kind == ST_FEEL && slot < TC_LOOKS) return &feel_at[slot];
    if (kind == ST_NAMES && slot == 0u) return &names_at;
    if (kind == ST_OWN_SET && slot < TC_LOOKS) return &own_at[slot];
    if (kind == ST_CHAIN && slot == 0u) return &chain_at;
    if (kind == ST_CHANCE && slot < SQ_SLOTS) return &chance_at[slot];
    return NULL;
}
static uint8_t live_scheme(const tc_controls *c); /* the look (1.9.0), below */
static uint8_t chord_byte(tc_chord e) {
    return (uint8_t)(e.lower | (uint32_t)(e.sus / 2u) << 3 | (uint32_t)e.add << 5);
}
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
            (uint32_t)c->chord_prog << 16 | (uint32_t)c->melody_prog << 24; /* 1.7.0: Unstick off, Spread of, programs */
    p[20] = c->chord_voicing | (uint32_t)c->chord_spread << 8 | (uint32_t)c->chord_octaves << 16 |
            (uint32_t)c->live_update << 24;
    p[21] = (uint32_t)c->strum_off; p[22] = (uint32_t)(c->strum_off >> 32) & 0xFFFFu;
    p[23] = c->strum_repeat | (uint32_t)c->strum_cutoff << 8 | (uint32_t)c->arp_rhythm << 16 |
            (uint32_t)c->arp_swing << 24;
    p[24] = c->arp_sync | (uint32_t)c->arp_latch << 8 | (uint32_t)c->melody_layout << 16 | (uint32_t)c->bass_ch << 24;
    p[25] = c->drum_ch | (uint32_t)c->clock_out << 8 | (uint32_t)c->pedal_note << 16 | (uint32_t)c->pedal_mode << 24;
    p[26] = c->arrows | (uint32_t)c->family << 8 | (uint32_t)c->capo << 16 | (uint32_t)c->chords_right << 24;
    p[27] = c->feel_time | (uint32_t)c->feel_vel << 8 | (uint32_t)c->repeat_rhythm << 16 | (uint32_t)c->arp_rotate << 24;
    p[28] = c->live_ch | (uint32_t)c->live_in << 8 | (uint32_t)c->pedal_target << 16 |
            (uint32_t)(c->mpe | ((uint32_t)c->glide + TC_GLIDE_MODES - TC_GLIDE_DEFAULT) % TC_GLIDE_MODES << 2 |
                       ((c->glide_time + TC_GLIDE_TIMES - TC_GLIDE_TIME_DEFAULT) % TC_GLIDE_TIMES) << 4) << 24;
    p[29] = (uint32_t)c->chord_mute; p[30] = (uint32_t)(c->chord_mute >> 32) & 0xFFFFu;
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
/* Every field in range and the whole layout inside MIDI range, or nothing changes. */
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
static bool decode(const uint32_t p[ST_PAYLOAD], tc_controls *c) {
    tc_controls t = *c;
    t.unstick = 1u; /* a record before 1.7.0: on */
    t.tonic = byte(p[0], 0); t.scale = byte(p[0], 1); t.chord_base = byte(p[0], 2); t.melody_base = byte(p[0], 3);
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
        if (p[22] >> 16) return false;
        if (byte(p[18], 0) >= TC_HARMONIES || byte(p[18], 1) >= TC_ECHO_TIMES || byte(p[18], 2) > TC_ECHOES ||
            byte(p[18], 3) >= TC_CHANCES || (p[19] & 0xF000u) || (p[19] & 0xA00u) == 0xA00u || byte(p[19], 0) > 1u ||
            byte(p[19], 2) >= TC_PROGRAMS || byte(p[19], 3) >= TC_PROGRAMS) return false;
        t.harmony = byte(p[18], 0); t.echo = byte(p[18], 1); t.chance = byte(p[18], 3); t.mono = byte(p[19], 0);
        t.unstick = (uint8_t)(byte(p[19], 1) & 1u ? 0u : 1u); t.spread_chord = (uint8_t)(byte(p[19], 1) & 8u ? TC_SPREAD_OF_BASS : (byte(p[19], 1) >> 1) & 1u);
        t.step_silent = (uint8_t)((byte(p[19], 1) >> 2) & 1u);
        t.chord_prog = byte(p[19], 2); t.melody_prog = byte(p[19], 3);
        t.echoes = byte(p[18], 2) ? (uint8_t)(byte(p[18], 2) - 1u) : (uint8_t)TC_ECHOES_DEFAULT;
        if (byte(p[26], 3) > 1u || byte(p[26], 0) >= TC_ARROWS_MODES || byte(p[26], 1) >= TC_FAMILIES ||
            byte(p[26], 2) >= TC_CAPOS || byte(p[27], 0) >= TC_FEEL_TIMES || byte(p[27], 1) >= TC_FEEL_VELS ||
            byte(p[27], 2) > RH_EUCLIDS || byte(p[27], 3) >= TC_ROTATES) return false;
        if (byte(p[28], 3) > 127u || byte(p[28], 2) >= TC_PEDAL_TARGETS ||
            byte(p[28], 0) >= TC_LIVE_CHANNELS || byte(p[28], 1) >= TC_LIVE_INPUTS ||
            p[30] >> 16) return false;
        if (byte(p[31], 0) > 1u || byte(p[31], 1) > 1u || byte(p[31], 2) >= TC_NOTE_REPEATS) return false;
        if (byte(p[31], 3) && (!(byte(p[31], 3) & 0x80u) || (byte(p[31], 3) & 0x7Fu) >= TC_VEL_MODES)) return false;
        if (byte(p[33], 3) >= TC_PROGRAM_INPUTS || byte(p[33], 0) >= SQ_SLOTS || byte(p[33], 1) > 3u ||
            byte(p[33], 2) >= TC_PIVOTS) return false;
        t.program_in = byte(p[33], 3);
        if (byte(p[34], 3) > 1u || byte(p[34], 0) > TC_CHORDS_GRID || byte(p[34], 1) > 1u || byte(p[34], 2) > 16u) return false;
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
        t.live_ch = byte(p[28], 0); t.live_in = byte(p[28], 1); t.chord_mute = p[29] | (uint64_t)p[30] << 32;
        t.arrows = byte(p[26], 0); t.family = byte(p[26], 1); t.capo = byte(p[26], 2); t.chords_right = byte(p[26], 3);
        t.feel_time = byte(p[27], 0); t.feel_vel = byte(p[27], 1); t.repeat_rhythm = byte(p[27], 2); t.arp_rotate = byte(p[27], 3);
        t.chord_voicing = byte(p[20], 0); t.chord_spread = byte(p[20], 1); t.chord_octaves = byte(p[20], 2);
        t.live_update = byte(p[20], 3); t.strum_off = p[21] | (uint64_t)p[22] << 32;
        t.strum_repeat = byte(p[23], 0); t.strum_cutoff = byte(p[23], 1); t.arp_rhythm = byte(p[23], 2);
        t.arp_swing = byte(p[23], 3); t.arp_sync = byte(p[24], 0); t.arp_latch = byte(p[24], 1);
        t.melody_layout = byte(p[24], 2); t.bass_ch = byte(p[24], 3); t.drum_ch = byte(p[25], 0);
        t.clock_out = byte(p[25], 1); t.pedal_note = byte(p[25], 2); t.pedal_mode = byte(p[25], 3);
        if (t.chord_voicing >= TC_VOICINGS || t.chord_spread >= TC_SPREADS || !t.chord_octaves ||
            t.chord_octaves > 0x7Fu || t.live_update > 1u || t.strum_repeat >= TC_REPEATS ||
            t.strum_cutoff >= TC_CUTOFFS || t.arp_rhythm >= TC_RHYTHM_VALUES || t.arp_swing >= TC_SWINGS ||
            t.arp_sync > 1u || t.arp_latch > 1u || t.bass_ch > 15u || t.drum_ch > 15u || t.clock_out > 1u ||
            t.pedal_note > 127u || t.pedal_mode >= TC_PEDAL_MODES) return false;
    } else {
        for (uint32_t i = 20u; i < ST_PAYLOAD; ++i) if (p[i]) return false;
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
    w[ST_WORDS - 1u] = crc32(w, ST_WORDS - 1u);
    bool ok = fs_program(next_slot * ST_BYTES, w, ST_WORDS);
    next_slot++; /* a failed slot is never reused */
    if (ok) { sequence++; records++; *at = next_slot; }
    return ok;
}
static const uint32_t *payload_at(uint32_t at);
/* A preset stored and not cleared since (1.9.3: a record of zeros clears it). */
static bool preset_present(uint32_t slot) {
    if (slot >= TC_PRESETS || !preset_at[slot]) return false;
    const uint32_t *p = payload_at(preset_at[slot]);
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) if (p[i]) return true;
    return false;
}
static void note_presets(void) {
    tc_presets_present = 0u;
    for (uint32_t i = 0; i < TC_PRESETS; ++i) if (preset_present(i)) tc_presets_present |= (uint8_t)(1u << i);
    sq_present = 0u;
    for (uint32_t i = 0; i < SQ_SLOTS; ++i)
        if (prog_at[i] && slot_words(prog_at[i] - 1u)[3] >> 24) sq_present |= (uint8_t)(1u << i);
}
static const uint32_t *payload_at(uint32_t at) { return slot_words(at - 1u) + 3u; }
/* --- the feel (1.9.0; 1.9.3 presets' own, settings.h ST_FEEL) --- */
static bool feel_present(uint32_t slot) { return slot < TC_LOOKS && feel_at[slot] && payload_at(feel_at[slot])[0]; }
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
    if (feel_at[slot]) { memcpy(p, payload_at(feel_at[slot]), ST_PAYLOAD * 4u); return; }
    uint8_t plain[TC_FEEL_BYTES]; tc_feel_default(plain); feel_payload(plain, 0u, p);
}
/* The board's curves as stored (the defaults if never stored). */
static void board_feel(uint8_t feel[TC_FEEL_BYTES]) {
    uint8_t keep;
    if (!feel_at[0] || !feel_read(payload_at(feel_at[0]), feel, &keep)) tc_feel_default(feel);
}
/* With the settings: the curves in use where they belong (the board's, or the preset's whose they are) when
 * they differ from what is stored there; Keep feel always in the board's. */
static void save_feel(tc_controls *c) {
    uint32_t s = c->feel_slot % TC_LOOKS, p[ST_PAYLOAD], q[ST_PAYLOAD];
    if (s) {
        uint8_t board[TC_FEEL_BYTES]; board_feel(board); feel_payload(board, c->keep_feel, p); feel_stored(0, q);
        if (memcmp(p, q, sizeof p) && !write_record(ST_FEEL, 0, p)) c->last_action = TC_SAVE_FAILED;
    }
    feel_payload(c->feel, s ? 0u : c->keep_feel, p); feel_stored(s, q);
    if (memcmp(p, q, sizeof p) && !write_record(ST_FEEL, (uint8_t)s, p)) c->last_action = TC_SAVE_FAILED;
}
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
static bool own_present(uint32_t slot) { return slot && slot < TC_LOOKS && own_at[slot] && payload_at(own_at[slot])[0]; }
static uint8_t keep_chords_stored(void) { return own_at[0] ? byte(payload_at(own_at[0])[0], 2) : 0u; }
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
    if (!set_at[k] || !set_ok(payload_at(set_at[k]), out)) memset(out, 0, sizeof *out);
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
    if (!own_present(slot) || !own_read(payload_at(own_at[slot]), false, &set, &s)) return;
    (void)tc_chord_set_put(set, &s);
    c->chords_slot = slot; c->chords_cover = set;
}
/* The covered set saved where it belongs (its preset's own) when changed since, and Keep chords (with the
 * settings and before another preset's set replaces it). */
static void save_chords(tc_controls *c) {
    uint32_t p[ST_PAYLOAD];
    if (c->keep_chords != keep_chords_stored()) {
        memset(p, 0, sizeof p); p[0] = 1u | (uint32_t)(c->keep_chords ? 1u : 0u) << 16;
        if (!write_record(ST_OWN_SET, 0, p)) c->last_action = TC_SAVE_FAILED;
    }
    if (!c->chords_slot) return;
    own_payload(c->chords_cover, p);
    if (own_present(c->chords_slot) && !memcmp(p, payload_at(own_at[c->chords_slot]), sizeof p)) return;
    if (!write_record(ST_OWN_SET, c->chords_slot, p)) c->last_action = TC_SAVE_FAILED;
}
/* --- the chain and each progression's chances (2.0.0) --- */
/* Slot's progression as stored, with its chances, into sq_slots (the chain plays from there). */
static void bank(uint8_t slot) {
    sq_prog *p = &sq_slots[slot];
    if (!prog_at[slot] || !sq_decode(payload_at(prog_at[slot]), p)) sq_clear(p, SQ_UNIT_BAR, 0u);
    if (chance_at[slot]) (void)sq_chances_decode(payload_at(chance_at[slot]), p);
}
/* sq's chances stored beside its progression when they differ from what is stored there. */
static void save_chances(tc_controls *c, uint8_t slot) {
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    sq_chances_encode(&sq, p);
    if (chance_at[slot]) memcpy(q, payload_at(chance_at[slot]), sizeof q); else memset(q, 0, sizeof q);
    if (memcmp(p, q, sizeof p) && !write_record(ST_CHANCE, slot, p)) c->last_action = TC_SAVE_FAILED;
}
/* --- the look (1.9.0, settings.h ST_LOOK) --- */
static bool look_present(uint32_t slot) { return slot < TC_LOOKS && look_at[slot] && payload_at(look_at[slot])[0]; }
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
/* What the live settings' two bits say of the board's scheme: the first three as they are, the rest Default. */
static uint8_t live_scheme(const tc_controls *c) {
    uint8_t scheme = c->look_slot && look_present(0) ? byte(payload_at(look_at[0])[0], 1) : c->led_scheme;
    return scheme < TC_LED_SCHEMES_V1 ? scheme : 0u;
}
static void save_look(tc_controls *c);
/* The look showing stored where it belongs before another covers it: the board's even when the live bits
 * could say it (its scheme may not fit them), and (1.9.3) changes not saved yet, the board's or a preset's. */
static void keep_board_look(tc_controls *c) {
    if (c->look_slot || look_present(0)) { save_look(c); return; }
    uint32_t p[ST_PAYLOAD]; look_payload(c, true, p); (void)write_record(ST_LOOK, 0, p);
}
/* The look showing, saved where it belongs (with the live settings): the board's only once it says more
 * than the live bits; Keep look always in the board's. */
static void save_look(tc_controls *c) {
    uint32_t s = c->look_slot % TC_LOOKS, p[ST_PAYLOAD];
    if (s && look_present(0) && byte(payload_at(look_at[0])[0], 3) != c->keep_look) {
        uint32_t b[ST_PAYLOAD]; memcpy(b, payload_at(look_at[0]), sizeof b);
        b[0] = (b[0] & 0x00FFFFFFu) | (uint32_t)c->keep_look << 24;
        if (!write_record(ST_LOOK, 0, b)) c->last_action = TC_SAVE_FAILED;
    }
    look_payload(c, s == 0u, p);
    bool plain = byte(p[0], 1) < TC_LED_SCHEMES_V1 && !(p[0] >> 16);
    for (uint32_t i = 1u; i < 10u; ++i) plain = plain && !p[i];
    if (look_present(s) ? !memcmp(p, payload_at(look_at[s]), sizeof p) : (!s && plain)) return;
    if (!write_record(ST_LOOK, (uint8_t)s, p)) c->last_action = TC_SAVE_FAILED;
}
/* The selected slot's progression into sq (empty when there is none). */
static void load_progression(uint8_t slot, uint8_t scale) {
    if (slot < SQ_SLOTS && prog_at[slot]) { sq = sq_slots[slot]; sq_changes++; return; } /* 2.0.0: with its chances */
    sq_clear(&sq, SQ_UNIT_BAR, scale);
}
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
    next_slot = ST_SLOTS; records = 0u; sequence = 0u; live_at = 0u; favourites_at = 0u; tc_favourite_count = 0u;
    memset(preset_at, 0, sizeof preset_at); memset(scale_at, 0, sizeof scale_at); memset(prog_at, 0, sizeof prog_at);
    memset(set_at, 0, sizeof set_at); memset(lane_at, 0, sizeof lane_at); memset(look_at, 0, sizeof look_at);
    memset(feel_at, 0, sizeof feel_at); names_at = 0u; memset(tc_preset_names, 0, sizeof tc_preset_names);
    memset(own_at, 0, sizeof own_at); chain_at = 0u; memset(chance_at, 0, sizeof chance_at); memset(&sq_chain, 0, sizeof sq_chain);
    ln_clear(&ln); ln_dirty = 0u; lane_seen = ln_changes;
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
        if (at && kind == ST_FAVOURITES && !*at && favourites_set(slot_words(s) + 3u)) *at = s + 1u;
        sq_prog check;
        if (at && kind == ST_PROG && !*at && sq_decode(slot_words(s) + 3u, &check)) *at = s + 1u;
        tc_chord_set set;
        if (at && kind == ST_CHORDS && !*at && set_ok(slot_words(s) + 3u, &set)) {
            *at = s + 1u;
            (void)tc_chord_set_put(slot, &set);
        }
        if (at && kind == ST_LANE && !*at && ln_decode(slot_words(s) + 3u, slot, &ln)) *at = s + 1u;
        if (at && kind == ST_LOOK && !*at && look_ok(slot_words(s) + 3u, slot == 0u)) *at = s + 1u; /* 1.9.0 */
        uint8_t feel[TC_FEEL_BYTES], keep;
        if (at && kind == ST_FEEL && !*at && feel_read(slot_words(s) + 3u, feel, slot ? NULL : &keep)) *at = s + 1u;
        if (at && kind == ST_NAMES && !*at && names_read(slot_words(s) + 3u, tc_preset_names)) *at = s + 1u; /* 1.9.3 */
        if (at && kind == ST_OWN_SET && !*at && own_ok(slot_words(s) + 3u, slot)) *at = s + 1u; /* 2.0.0 */
        if (at && kind == ST_CHAIN && !*at && sq_chain_decode(slot_words(s) + 3u, &sq_chain)) *at = s + 1u;
        if (at && kind == ST_CHANCE && !*at && sq_chances_decode(slot_words(s) + 3u, &check)) *at = s + 1u;
    }
    /* Presets are checked when loaded (one may name a user scale cleared since). */
    bool found = false;
    for (uint32_t s = next_slot; s-- > 0u;) {
        uint8_t kind, slot;
        uint32_t *at = valid(s, &kind, &slot);
        if (!at || (kind != ST_LIVE && kind != ST_PRESET) || *at) continue;
        tc_controls t = *c;
        if (kind == ST_LIVE && !decode(slot_words(s) + 3u, &t)) continue;
        *at = s + 1u;
        if (kind == ST_LIVE) { *c = t; found = true; }
    }
    c->look_slot = 0u;
    if (look_present(0)) look_apply(payload_at(look_at[0]), c, true); /* the board's look (1.9.0) */
    c->feel_slot = 0u;
    if (feel_at[0]) (void)feel_read(payload_at(feel_at[0]), c->feel, &c->keep_feel); /* the curves (1.9.0) */
    c->chords_slot = 0u; c->keep_chords = keep_chords_stored(); /* 2.0.0 */
    writable = true;
    if (next_slot >= ST_ERASE_AT) {
        /* Before USB: compact by erasing and rewriting the current records. */
        uint32_t presets[TC_PRESETS][ST_PAYLOAD], scales[TC_USER_SCALES][ST_PAYLOAD], live[ST_PAYLOAD];
        uint32_t favourites[ST_PAYLOAD], progs[SQ_SLOTS][ST_PAYLOAD]; bool had_favourites = favourites_at != 0u;
        bool had_prog[SQ_SLOTS], had_set[TC_CHORD_SETS], had_lane[LN_RECORDS];
        for (uint32_t i = 0; i < LN_RECORDS; ++i) had_lane[i] = lane_at[i] != 0u; /* rewritten from ln */
        uint32_t sets[TC_CHORD_SETS][ST_PAYLOAD], looks[TC_LOOKS][ST_PAYLOAD], feels[TC_LOOKS][ST_PAYLOAD], names[ST_PAYLOAD];
        bool had_look[TC_LOOKS], had_feel[TC_LOOKS], had_names = names_any();
        uint32_t owns[TC_LOOKS][ST_PAYLOAD], chain[ST_PAYLOAD], chances[SQ_SLOTS][ST_PAYLOAD]; /* 2.0.0 */
        bool had_own[TC_LOOKS], had_chance[SQ_SLOTS], had_chain = sq_chain.count != 0u;
        for (uint32_t i = 0; i < TC_LOOKS; ++i) {
            if ((had_look[i] = look_present(i))) memcpy(looks[i], payload_at(look_at[i]), sizeof looks[i]);
            if ((had_feel[i] = feel_present(i))) memcpy(feels[i], payload_at(feel_at[i]), sizeof feels[i]);
            if ((had_own[i] = own_at[i] && payload_at(own_at[i])[0])) memcpy(owns[i], payload_at(own_at[i]), sizeof owns[i]);
        }
        for (uint32_t i = 0; i < SQ_SLOTS; ++i)
            if ((had_chance[i] = chance_at[i] && payload_at(chance_at[i])[0])) memcpy(chances[i], payload_at(chance_at[i]), sizeof chances[i]);
        sq_chain_encode(&sq_chain, chain);
        names_payload(names);
        for (uint8_t i = 0; i < TC_CHORD_SETS; ++i) { had_set[i] = set_at[i] != 0u; set_payload(i, sets[i]); }
        for (uint32_t i = 0; i < SQ_SLOTS; ++i)
            if ((had_prog[i] = prog_at[i] != 0u)) memcpy(progs[i], payload_at(prog_at[i]), sizeof progs[i]);
        favourites_payload(favourites);
        bool had_preset[TC_PRESETS], had_scale[TC_USER_SCALES];
        for (uint32_t i = 0; i < TC_PRESETS; ++i)
            if ((had_preset[i] = preset_present(i))) memcpy(presets[i], payload_at(preset_at[i]), sizeof presets[i]);
        for (uint8_t i = 0; i < TC_USER_SCALES; ++i) { had_scale[i] = scale_at[i] != 0u; scale_payload(i, scales[i]); }
        encode(c, live);
        writable = fs_erase();
        next_slot = 0u; records = 0u; live_at = 0u; favourites_at = 0u;
        memset(preset_at, 0, sizeof preset_at); memset(scale_at, 0, sizeof scale_at); memset(prog_at, 0, sizeof prog_at);
        memset(set_at, 0, sizeof set_at); memset(lane_at, 0, sizeof lane_at); memset(look_at, 0, sizeof look_at);
        memset(feel_at, 0, sizeof feel_at); names_at = 0u;
        memset(own_at, 0, sizeof own_at); chain_at = 0u; memset(chance_at, 0, sizeof chance_at);
        if (writable) {
            for (uint8_t i = 0; i < TC_LOOKS; ++i) if (had_own[i]) (void)write_record(ST_OWN_SET, i, owns[i]);
            for (uint8_t i = 0; i < SQ_SLOTS; ++i) if (had_chance[i]) (void)write_record(ST_CHANCE, i, chances[i]);
            if (had_chain) (void)write_record(ST_CHAIN, 0, chain);
            for (uint8_t i = 0; i < TC_LOOKS; ++i) if (had_feel[i]) (void)write_record(ST_FEEL, i, feels[i]);
            if (had_names) (void)write_record(ST_NAMES, 0, names);
            for (uint8_t i = 0; i < TC_LOOKS; ++i) if (had_look[i]) (void)write_record(ST_LOOK, i, looks[i]);
            for (uint8_t i = 0; i < TC_CHORD_SETS; ++i) if (had_set[i]) (void)write_record(ST_CHORDS, i, sets[i]);
            for (uint8_t i = 0; i < TC_USER_SCALES; ++i) if (had_scale[i]) (void)write_record(ST_SCALE, i, scales[i]);
            if (had_favourites) (void)write_record(ST_FAVOURITES, 0, favourites);
            for (uint8_t i = 0; i < SQ_SLOTS; ++i) if (had_prog[i]) (void)write_record(ST_PROG, i, progs[i]);
            for (uint8_t i = 0; i < LN_RECORDS; ++i) if (had_lane[i]) {
                uint32_t p[ST_PAYLOAD];
                ln_encode(&ln, i, p); (void)write_record(ST_LANE, i, p);
            }
            for (uint8_t i = 0; i < TC_PRESETS; ++i) if (had_preset[i]) (void)write_record(ST_PRESET, i, presets[i]);
            (void)write_record(ST_LIVE, 0, live);
        }
    }
    note_presets();
    for (uint8_t i = 0; i < SQ_SLOTS; ++i) bank(i);
    load_progression(c->prog_slot, c->scale);
    saved_changes = seen_changes = c->changes;
    return found;
}
/* The pattern's changed chunks, each as one record; a chunk of defaults that was never stored stays
 * unstored. A failed write is not retried (as the live settings: a failed slot is never reused). */
/* The pattern's changed chunks; false when one couldn't be written (the sector is full until the next
 * start compacts it, or a write failed): those stay marked and are tried again after the next edit. */
static bool save_lanes(void) {
    uint16_t failed = 0u;
    for (uint8_t i = 0; i < LN_RECORDS; ++i) {
        if (!(((uint32_t)ln_dirty >> i) & 1u) || (!lane_at[i] && ln_chunk_default(&ln, i))) continue;
        uint32_t p[ST_PAYLOAD];
        ln_encode(&ln, i, p);
        if (!write_record(ST_LANE, i, p)) failed = (uint16_t)(failed | 1u << i);
    }
    ln_dirty = failed;
    return !failed;
}
void st_service(tc_controls *c, uint32_t now) {
    static uint32_t lane_tried = UINT32_MAX;
    if (ln_changes != lane_seen) { lane_seen = ln_changes; lane_changed_at = now; }
    if (ln_dirty && ln_changes != lane_tried && now - lane_changed_at >= ST_SAVE_DELAY_MS) {
        lane_tried = ln_changes;
        if (!save_lanes()) c->last_action = TC_SAVE_FAILED; /* NOT SAVED: RESTART (1.7.0; was silent) */
    }
    if (c->changes != seen_changes) { seen_changes = c->changes; changed_at = now; }
    if (c->changes == saved_changes || now - changed_at < ST_SAVE_DELAY_MS) return;
    saved_changes = c->changes;
    uint32_t p[ST_PAYLOAD];
    encode(c, p);
    if (!write_record(ST_LIVE, 0, p)) c->last_action = TC_SAVE_FAILED; /* 1.7.0: was silent */
    save_look(c);
    save_feel(c);
    save_chords(c); /* 2.0.0 */
}
/* A preset is the whole musical setup (1.7.0: each half's program too); brightness, the MIDI-in lights,
 * pad colors, the jack settings (TRS out, pedal and its note), clock out, the arrows, the pad
 * thresholds, which half the chords are on and Program in belong to the board and stay. */
static void keep_board(tc_controls *t, const tc_controls *c) {
    t->brightness = c->brightness; t->midi_leds = c->midi_leds;
    t->trs_out = c->trs_out; t->pedal_mode = c->pedal_mode; t->pedal_type = c->pedal_type; t->led_scheme = c->led_scheme;
    t->clock_out = c->clock_out; t->pedal_note = c->pedal_note; t->arrows = c->arrows; t->chords_right = c->chords_right;
    t->pad_trigger = c->pad_trigger; t->pad_release = c->pad_release; t->pad_full = c->pad_full; t->unstick = c->unstick;
    t->pad_vel_full = c->pad_vel_full; /* 1.8.3: the board's, like the thresholds */
    t->step_silent = c->step_silent;   /* 1.9.4: the board's, like Unstick */
    t->pedal_target = c->pedal_target; t->prog_slot = c->prog_slot; t->seq_guide = c->seq_guide;
    t->lock = c->lock; memcpy(t->buttons, c->buttons, sizeof t->buttons); t->shift_latched = c->shift_latched; t->button_sustain = c->button_sustain;
    t->program_in = c->program_in; /* 1.7.0: the channel presets are chosen from stays */
    t->chain = c->chain; t->u3_shift = c->u3_shift; t->sleep = c->sleep; /* 2.0.0: the board's */
    t->program_again = 3u;        /* each half's program goes out again, also when it is the same */
}
/* The preset's settings, the board's own as they are (false: empty, or it can't load now). */
static bool preset_controls(const tc_controls *c, uint8_t slot, tc_controls *t) {
    if (!preset_present(slot)) return false;
    *t = *c;
    if (!decode(payload_at(preset_at[slot]), t)) return false;
    keep_board(t, c);
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
        look_apply(payload_at(look_at[own]), c, false);
        c->look_slot = own;
    }
    /* 1.9.3: the preset's own curves, else the board's (back, if another preset's were in use) */
    uint8_t own_feel = feel_present(slot + 1u) ? (uint8_t)(slot + 1u) : 0u;
    if (own_feel || c->feel_slot) {
        save_feel(c); /* the curves in use, changes not saved yet too, before others cover them */
        if (own_feel) (void)feel_read(payload_at(feel_at[own_feel]), c->feel, NULL); else board_feel(c->feel);
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
    if (ok && look_present(own)) ok = write_record(ST_LOOK, (uint8_t)own, zero);
    if (ok && feel_present(own)) ok = write_record(ST_FEEL, (uint8_t)own, zero);
    if (ok && own_present(own)) ok = write_record(ST_OWN_SET, (uint8_t)own, zero);
    if (ok && tc_preset_names[slot][0]) {
        uint32_t p[ST_PAYLOAD];
        memset(tc_preset_names[slot], 0, sizeof tc_preset_names[slot]);
        names_payload(p); ok = write_record(ST_NAMES, 0, p);
    }
    if (ok && c->look_slot == own) { look_apply(payload_at(look_at[0]), c, false); c->look_slot = 0u; }
    if (ok && c->feel_slot == own) { board_feel(c->feel); c->feel_slot = 0u; }
    if (ok && c->chords_slot == own) uncover(c);
    note_presets();
    if (ok) { c->cleared = slot; c->last_action = TC_PRESET_CLEARED; c->changes++; }
    else c->last_action = TC_SAVE_FAILED;
}
void st_requests(tc_controls *c, tp_engine *e) {
    uint8_t kind = c->request, slot = c->request_slot;
    if (kind == TC_REQUEST_NONE) return;
    c->request = TC_REQUEST_NONE;
    if (kind == TC_REQUEST_LOAD && c->recording) c->last_action = TC_SEQ_BUSY; /* it may bring another chord set */
    else if (kind == TC_REQUEST_LOAD) {
        if (load_preset(c, e, slot)) { c->preset = slot; c->last_action = TC_PRESET_LOADED; }
        else c->last_action = TC_PRESET_EMPTY;
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
        set_payload(slot, p);
        if (c->chords_slot && slot == c->chords_cover) save_chords(c); /* 2.0.0: covered: into the preset's own */
        else if (slot >= TC_CHORD_SETS || !write_record(ST_CHORDS, slot, p)) c->last_action = TC_SAVE_FAILED;
    } else {
        uint32_t p[ST_PAYLOAD];
        encode(c, p);
        if (write_record(ST_PRESET, slot, p)) { c->preset = slot; c->last_action = TC_PRESET_SAVED; }
        else c->last_action = TC_SAVE_FAILED;
        /* 1.9.0, Keep look: the look showing goes with the preset; else the preset has none (any it had goes) */
        uint32_t lp[ST_PAYLOAD];
        if (c->keep_look) look_payload(c, false, lp); else memset(lp, 0, sizeof lp);
        if ((c->keep_look || look_present(slot + 1u)) && slot < TC_PRESETS) {
            keep_board_look(c);
            if (!write_record(ST_LOOK, (uint8_t)(slot + 1u), lp)) c->last_action = TC_SAVE_FAILED;
            if (!c->keep_look && c->look_slot == slot + 1u) { look_apply(payload_at(look_at[0]), c, false); c->look_slot = 0u; }
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
    if (kind == ST_CHANCE && slot < SQ_SLOTS && !(chance_at[slot] && payload_at(chance_at[slot])[0])) return ST_ABSENT;
    if (kind == ST_CHORDS && c->chords_slot && slot == c->chords_cover) { /* covered: the board's own, as stored */
        tc_chord_set s; board_set(slot, &s); set_words(&s, p);
        for (uint32_t i = 0; i < 12u; ++i) if (s.column[i].mask) return ST_OK;
        return ST_ABSENT;
    }
    if (kind == ST_PRESET && slot < TC_PRESETS && !preset_present(slot)) return ST_ABSENT;
    uint32_t *at = index_of(kind, slot);
    if (!at) return ST_BAD_VALUE;
    if (kind == ST_LIVE) { encode(c, p); return ST_OK; }
    if (kind == ST_SCALE) { scale_payload(slot, p); return tc_user_scale_get(slot)->mask ? ST_OK : ST_ABSENT; }
    if (kind == ST_FAVOURITES) { favourites_payload(p); return *at ? ST_OK : ST_ABSENT; }
    if (kind == ST_PROG && !*at) return ST_ABSENT;
    if (kind == ST_CHORDS) { set_payload(slot, p); return tc_chord_set_defined(slot) ? ST_OK : ST_ABSENT; }
    if (kind == ST_LANE) { ln_encode(&ln, slot, p); return ln_chunk_default(&ln, slot) ? ST_ABSENT : ST_OK; } /* as it plays now */
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
            if (slot && !q[0]) { look_apply(payload_at(look_at[0]), c, false); c->look_slot = 0u; }
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
    if (kind == ST_LANE) { /* plays at once; stored now, so a later save leaves it alone */
        if (!ln_decode(p, slot, &ln)) return ST_BAD_VALUE;
        ln_dirty = (uint16_t)(ln_dirty & ~(1u << slot)); ln_changes++; lane_seen = ln_changes;
        return write_record(ST_LANE, slot, p) ? ST_OK : ST_STORAGE;
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
    if (kind != ST_LANE) return st_put(kind, slot, p, c, e);
    if (!ln_decode(p, slot, &ln)) return ST_BAD_VALUE;
    ln_changed(slot / LN_CHUNKS, (slot % LN_CHUNKS) * LN_CHUNK); /* saved by st_service, 3 s after the last */
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
