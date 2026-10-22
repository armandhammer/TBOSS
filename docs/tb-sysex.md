# TheoryBoard SysEx protocol, version 1

Used by the web editor (`webapp/`). Firmware: `src/tb/sysex.c`; JavaScript:
`webapp/tb-protocol.js`; the two are tested against each other by
`tests/webapp/test_protocol.mjs`.

## Framing

```
request  F0 7D 54 42 cmd data... sum F7
reply    F0 7D 54 42 cmd|40 status data... sum F7
```

- `7D` is the MIDI manufacturer ID reserved for non-commercial use; `54 42`
  ("TB") marks this protocol. Messages without that header are ignored.
- `sum` makes (cmd + data [+ status] + sum) a multiple of 128.
- All bytes between `F0` and `F7` are 7-bit. Binary data is **7-bit packed**: each
  group of up to 7 bytes becomes one byte holding their top bits (bit i = byte i)
  followed by the 7 bytes' low 7 bits.
- Multi-byte numbers are sent low 7 bits first.
- One request at a time: wait for the reply (or 2 s) before the next.
- Replying stops the TB's notes (all Note Offs are sent first); playing resumes
  1 s after the last request. `PREVIEW` has no reply and does not interrupt.

## Status

| Value | Meaning |
| --- | --- |
| 0 | OK |
| 1 | Checksum error |
| 2 | Wrong length |
| 3 | Value not accepted (range, a scale with < 5 notes or no root, layout would leave MIDI range, scale in use cleared) |
| 4 | Unknown command |
| 5 | Storage (flash) error |
| 6 | External memory missing, read failed or out of range |
| 7 | Nothing stored there |

## Commands

| cmd | Name | Request data | Reply data (after status) |
| --- | --- | --- | --- |
| 01 | IDENTIFY | - | protocol 1, stage 4 (3 in 0.6.x), user scales 12 (3 before 0.6.0), presets 8, record words 40 (20 before 0.6.0), NOR present 0/1, packed[JEDEC 3 bytes, size 4 bytes LE], version 3 bytes |
| 02 | GET_SCALE | slot 0..11 | slot, mask low 7, mask high 5, name 12 bytes (0 padded) |
| 03 | SET_SCALE | slot, mask low 7, mask high 5, name 12 bytes | slot. Mask 0 clears the slot. Stored in flash at once. |
| 04 | PREVIEW | mask low 7, mask high 5, root 0..11 [, slot 0..11] | none. Shown on all pads and the display for 5 s; mask 0 ends it. The optional slot (firmware 0.5.0+; older firmware ignores 4-byte previews) names the user slot being edited on the display. |
| 05 | SELECT_SCALE | scale id 0..97 (12..23 = U1..U12, 24..36 more built-in scales, 37..97 the library from 0.7.0; webapp/tb-scales.js lists them) | - |
| 06 | GET_RECORD | kind, slot | kind, slot, packed[160 bytes = 40 words LE] (status 7: nothing stored) |
| 07 | SET_RECORD | kind, slot, packed[160 bytes, or 80 bytes from older firmware] | kind, slot. Validated; live settings apply at once. A 20-word record is padded with zeros (the 0.6.0 settings keep their current values). |
| 08 | NOR_READ | address 4 x 7 bits, length 2 x 7 bits (1..2048) | the 6 request bytes, packed[data, CRC-32 of data LE] |

Scale mask: bit n = the note n semitones above the root; bit 0 (root) required,
5..12 notes. Names: up to 12 characters, shown in capitals.

Record kinds: 1 live settings (slot 0), 2 preset (slots 0..7), 3 user scale
(slots 0..11), 4 favorite scales (slot 0, 0.7.0: up to 128 scale ids as bytes in words
0..31, in order, 0xFF after the last, no id twice, words 32..39 zero; older firmware
answers status 3 for kind 4), 5 progression (slots 0..7, 0.11.0; older firmware answers
status 3), 6 chord set (slots 0..7, 1.0.0; older firmware answers status 3). A record is
40 words (20 before 0.6.0), the same payload as in flash sector 11 (`src/tb/settings.c`):
treat it as opaque for backup and restore.
Restore scales first, then chord sets, favorites, presets, progressions, then the live
settings (they may select a user scale or a chord set).

Live settings, 1.0.0 additions (each 0 at its default, so a 0.11.0 record reads the same):
word 34 = chord set (0 = the scale's chords, 1..8; 9 = the Full chord layout, 10 = the chord grid, 1.3.0) | hints (0/1) << 8 | scale lock channel
(0 off, 1..16) << 16; words 35..38 = one byte per center button (button 0 first, in the
order Voicing I, II, III, Guitar, Spread I, II, III, Octave -3 .. +3): 0 = its printed
panel function, else action + 1 (`src/tb/controls.h` TC_ACT_: 0..13 the panel functions,
14 Shift, then Hold, Arp, Strum, Voice lead, Fold, Hints, Sustain, All notes off, Tap tempo,
Next/Previous preset, Next/Previous scale, Melody octave up/down, Play, Record, Add 7th,
Add 9th, Add 6th, Sus2, Sus4, Next chord set, Nothing = 38); bytes 14 and 15 are 0. Presets
carry the chord set and hints; the board keeps its own buttons and scale lock when one loads.
A live settings record that changes the chord set is refused (status 3) while the board records a
progression (a progression keeps one set).

Chord set record (1.0.0, `src/tb/settings.c` set_payload): word 0 = 1; word 1 + i =
column i's chord: tones | root << 12, where tones has bit n for the note n semitones above
the chord's root (bit 0 required, 1..6 notes) and root is the chord's root in semitones
above the key's root (0..23); 0 = no chord on that column. Words 13..15: the name (12
bytes, as a scale's). Every other word 0. A set without any chord is absent (GET answers
status 7); SET_RECORD with one clears the slot, refused (status 3) while the board plays
that set, as is a set that would put a note outside the MIDI range at the current octave.

Progression record (`src/tb/sequence.c`; the editor's `webapp/tb-protocol.js`
progressionToWords): word 0 = 1 | unit << 8 | scale << 16 | count << 24 (unit 0..3: 1 beat,
1/2 bar, 1 bar, 2 bars; scale: the id it was recorded in; count: steps, up to 38). Word
1 + i is step i: chord pad (0..47, 4 x column + row, row 3 the bottom) | (units - 1) << 6
(1..32 units, at most 64 in all) | velocity / 2 << 11 | edits << 17 | scale << 25. Edits:
lowered members (bits 0..2: root, third, fifth) | sus / 2 << 3 (0, 2 or 4) | added 6th, 7th,
9th (bits 5..7). Step scale 127: the progression's own, which is the board's current scale
when it plays (the mode buttons re-color it); any other id is a borrowed chord that keeps
its scale. Word 39 (1.0.0): the chord set it was recorded with (0 = the scale's chords,
1..8; 9 = the Full chord layout, 10 = the chord grid, 1.3.0); it plays that set's chords while the set has any. Every other word after the last
step is 0. A SET_RECORD to the selected slot plays from then on; a record with no steps
empties the slot.
