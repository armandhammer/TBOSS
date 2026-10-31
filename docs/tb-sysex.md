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
- Since TBOSS 2.0.6 the board has two USB MIDI ports (cables): 0, TheoryBoard MIDI, and 1,
  TheoryBoard Editor, which carries SysEx only. A reply goes out on the cable its request came
  in on, so music software on cable 0 never sees the editor's traffic. Either cable works.
- One request at a time: wait for the reply (or 2 s) before the next.
- Replying stops the TB's notes (all Note Offs are sent first); playing resumes
  1 s after the last request. `PREVIEW`, `SET_ITEM` and a quiet `SET_RECORD` (1.9.0) have no
  reply and do not interrupt.

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
| 07 | SET_RECORD, quiet (1.9.0) | kind, slot, flags 1, packed[160 bytes] | none: applied and stored as SET_RECORD, but the board keeps playing; read the record back to see whether it was taken. Older firmware answers status 2. |
| 09 | MENU (1.9.0) | page 0..pages-1 | page, pages, count, count item ids (2 x 7 bits each), the page's name (ASCII, to the end) |
| 0A | ITEM (1.9.0) | item id 2 x 7 bits, first value 2 x 7 bits | item id (2), kind, flags, values (2), current value (2), first (2), n, the item's name, then the names of values first .. first + n - 1 (each ASCII, 0 after each) |
| 0C | PADS (1.9.0) | - | 96, flags (bit 0: resting levels measured, bit 1: the scanner reads), Trigger, Release, Full, Vel full (the settings' values), then for each pad by number (0..47 the chord pads, 4 per column from the top; not where it lights): its resting level (2 x 7 bits), its reading now (2 x 7 bits). Before 1.9.4 a reply paused playing; the editor's calibration asks it several times a second while it measures. |
| 0B | SET_ITEM (1.9.0) | item id 2 x 7 bits, value 2 x 7 bits | none: set as the board's menu sets it (its checks and side effects, saved as usual); the board shows `CHANGED BY EDITOR`, or why it refused |
| 0D | VALUES (1.9.3) | kind, slot (kind 2 preset 0..7, or kind 1 slot 0 the live settings) | kind, slot, count (2 x 7 bits) = the number of menu items, then every item's value (2 x 7 bits each, by item id, as ITEM gives it) in that preset as loading it would set it: the board's own settings (brightness, pedal, pads ...) as they are. Status 7 for an empty preset, 3 for one that can't load now (it names a user scale cleared since) or another kind. The editor's Presets tab compares two presets with it, named by MENU and ITEM. |
| 0E | SET_EDITS (1.9.3) | packed[60 bytes]: the 48 chord pads' edit bytes (pad 0 first; lower \| sus / 2 << 3 \| add << 5, as in the live settings), strum off (6 bytes, bit n = pad n, low byte first), sound off (6 bytes) | none: all of them set at once (live settings words 5..16, 21, 22, 29, 30; saved with the settings), or none when one is sus 3 or a pad would leave MIDI range; the board keeps playing and shows `CHANGED BY EDITOR` |
| 0F | CHORD_NAMES (1.9.3) | - | 48, then each chord pad's chord with its edits as the play screen names it (ASCII, 0 after each; `-` for a chord set's empty column) |
| 10 | STATE (1.9.4) | - | check (5 x 7 bits, low first: FNV-1a over every menu item's value << 1 \| shown, 4 bytes each, by item id), the step pattern's change count, the progressions' change count (2.0.6: the one playing and the saved slots), the saved changes' count (5 x 7 bits each, 32-bit counts that wrap), items (2 x 7 bits), then which items the menu shows now (7 a byte, bit k of byte i = item 7i + k). The editor asks it once a second and reads again what changed (VALUES, the lanes); only differences matter, never the numbers. |

**Replies between notes (1.9.4):** a reply to a request that only reads (IDENTIFY, GET_SCALE, GET_RECORD, MENU,
ITEM, PADS, VALUES, CHORD_NAMES, STATE) of up to 640 bytes goes out between two of the board's own MIDI messages:
nothing stops, notes wait a few milliseconds while it streams. Other replies (a write that answers, NOR_READ, a long
ITEM) use the pause as before: every note ends, the transport sends Stop, the reply, a second of quiet, Continue.

The menu (1.9.0, `src/tb/controls_menu.c`, `sysex.c`): the board's Settings pages and their items,
as the board shows them, so an editor can show every setting, also ones added later. MENU lists a
page's items in the board's order (every item, also one the board hides just now). ITEM gives one
item: kind 0 = a value (SET_ITEM sets it), 1 = start/stop (Transport, Play: value 1 starts, 0
stops), 2 = pick and do (Load preset, Save preset: SET_ITEM with a preset number loads or saves it),
3 = on the board only (resets, tests, capture, recording, the last chord's edits, Version); flags
bit 0 = the board's page shows it now (Pedal's Note only with Mode Note, Glide time only with
Glide Time). Value names are the board's own, in capitals; ITEM sends as many as fit in a reply
(n), and the editor asks again from first + n for the rest. Some items' values depend on others
(Euclid and Rotate on the lane's length): read them again after changing the lane.

Scale mask: bit n = the note n semitones above the root; bit 0 (root) required,
5..12 notes. Names: up to 12 characters, shown in capitals.

Record kinds: 1 live settings (slot 0), 2 preset (slots 0..7), 3 user scale
(slots 0..11), 4 favorite scales (slot 0, 0.7.0: up to 128 scale ids as bytes in words
0..31, in order, 0xFF after the last, no id twice, words 32..39 zero; older firmware
answers status 3 for kind 4), 5 progression (slots 0..7, 0.11.0; older firmware answers
status 3), 6 chord set (slots 0..7, 1.0.0; older firmware answers status 3), 7 step pattern
(slots 0..15, 1.5.0; older firmware answers status 3), 8 look (slots 0..9, 1.9.0; older firmware answers
status 3), 9 feel (slot 0, 1.9.0; slots 0..9 from 1.9.3; older firmware answers status 3), 10 preset names (slot 0,
1.9.3; older firmware answers status 3), 11 presets' own chord sets (slots 0..9, 2.0.0), 12 the chain (slot 0,
2.0.0), 13 a progression's chances (slots 0..7, 2.0.0; firmware before 2.0 answers status 3 for 11..13). A record is
40 words (20 before 0.6.0), the same payload as in flash sector 11 (`src/tb/settings.c`):
treat it as opaque for backup and restore.
Restore scales first, then chord sets, favorites, presets, progressions, the step pattern,
then the live settings (they may select a user scale or a chord set), then the looks, the feels and the
preset names. 2.0: each progression's chances and the chain after the progressions, the presets' own chord sets
last. Restoring a 2.0 backup into firmware before 2.0 leaves kinds 11..13 out and clears the 2.0 settings below.

Live settings and presets, 2.0.0 (`src/tb/settings.c`; each 0 at its default, so a record from before reads the
same): word 2 byte 2 (Arp) and word 4 byte 2 (the last pattern) may be 7, Turing; word 17 byte 1 = Sleep after
(0 off, 1..4: 5, 15, 30, 60 minutes), byte 2 = the Turing pattern's Lock | Loop << 4, each stored as (value +
count - default) % count (Lock: 0..10 = 0..100 %, default 8; Loop: 0..14 = 2..16 steps, default 6), both only
with word 17's 0x80 bit; word 33 bit 9 = Chain (0 off); word 34 byte 3 = the U3 button (0 its scale, 1 Shift).
Firmware before 2.0 refuses a record with any of these set: the editor clears them for it.

The chain (2.0.0, kind 12, slot 0; `src/tb/sequence.c` sq_chain_encode; the editor's `webapp/tb-protocol.js`
chainToWords): up to 8 progression slots played one after another and round again, while Settings → Sequence →
*Chain* is On. Word 0 = 1 | count (1..8) << 8; word 1 = link i's slot (0..7) in bits 4i..4i+2, the rest of
word 1 and words 2..39 0. A record of zeros: no chain (GET answers status 7). Links to empty slots are skipped.

A progression's chances (2.0.0, kind 13, slot = the progression's slot 0..7; sq_chances_encode): word 0 = 1
(0 with every byte 0: every chord at 100 %), then a byte per chord, chord i in byte i from word 1 (words 1..10,
the two bytes past chord 38 and words 11..39 0): bits 0-3 the chance it plays as written, as the arpeggiator's
Chance (0 = 100 %, 1 = 90 % .. 9 = 10 %), bit 4 = otherwise the borrowed chord (the same chord pad in the parallel
key: natural minor for a scale with a major 3rd, else major) instead of a rest, bits 5-7 0. Kept beside the
progression record (whose format is unchanged); SET plays at once without starting the progression again;
recording the progression again on the board sets it back to zeros. GET answers status 7 while all are 100 %.

Presets' own chord sets (2.0.0, kind 11, *Keep chords*; `src/tb/settings.h` ST_OWN_SET): slots 1..8 are presets
1..8's own copy of a chord set. Word 0 = 1 | the chord set slot it is a copy of (0..7) << 8; words 1..15 as a chord
set record's (the 12 columns, the name); the rest 0; a record of zeros clears it. Loading the preset puts the copy
in that set's place; loading one without brings the board's set back; changes to a covered set (chord capture,
slot 9) go to the preset's copy. Slot 0: word 0 = 1 | Keep chords << 16, the rest 0 (always answers). Slot 9: the
set a preset's own covers, as it plays (word 0 = 1 | set << 8 | Keep chords << 16, words 1..15 the set, word 39 =
whose: n preset n's, 0 none, words 1..15 then 0); SET changes it at once and saves it in that preset's copy, or
with word 39 and words 1..15 0 only Keep chords. Kind 6 (chord sets) is always the board's own sets, also while
one is covered: SET then stores the board's without changing what plays.

A preset record of zeros (1.9.3) clears the preset: from then on GET answers status 7 and loading it says
`PRESET 3 EMPTY` (the live settings can't be cleared: status 3).

Preset names (1.9.3, `src/tb/settings.h` ST_NAMES; the editor's `webapp/tb-presets.js`): slot 0. Word 0 =
1; words 1..24 = 8 names of 12 bytes, preset 1's from byte 0 of word 1, preset n's from byte 12 (n - 1)
on; each a character ' '..'~' (the editor sends capitals, digits and the symbols the font has) and 0 after
its last; words 25..39 are 0. GET answers status 7 while no preset is named; a record of zeros names none.
Settings → Presets → *Load preset* and *Save preset* show a named, stored preset as `3 FUNK`.

Look record (1.9.0, `src/tb/settings.h` ST_LOOK; the editor's `webapp/tb-lights.js`): the pads'
color scheme and colors. Slot 0 is the board's look; slots 1..8 are presets 1..8's own (present:
loading that preset shows it; absent: the board's look shows); slot 9 is whichever look shows now
(GET: word 39 says whose, 0 the board's or n preset n's; SET: changes it, saved with the settings
where it belongs; send it quietly while choosing colors). Word 0 = 1 | scheme << 8 (0 Default,
1 Root+5th, 2 Rainbow, 3 Piano, 4 Triad, 5 Function, 6 Octaves) | Color by << 16 (0 note name,
1 scale degree) | Keep look << 24 (0/1, slots 0 and 9 only). Words 1..9: 36 role bytes from role 0
(Default's notes C..B, or degrees 0..11 with Color by degree, = roles 0..11; Root+5th's root, 5th,
other, edited, MIDI in = 12..16; Rainbow's columns 17..23; Piano's white keys, black keys, root
24..26; Triad's root, 3rd, 5th, other 27..30; Function's tonic, subdominant, dominant 31..33;
Octaves' root, other 34..35): 0 = the role's built-in color, 1..16 = palette color n - 1 (red,
orange, gold, lime, green, turquoise, sky blue, blue, dark blue, violet, purple, pink, white, warm
white, gray, off). Words 10..38 are 0. A record of zeros clears a slot (absent again). The live
settings' scheme bits (word 4, bits 29-30) hold only the first three schemes; from Piano on they
read 0 (Default) and the look record holds the scheme.

Feel record (1.9.0, `src/tb/settings.h` ST_FEEL; the editor's `webapp/tb-feel.js`): the velocity and
pressure curves. 1.9.0 had slot 0 only; from 1.9.3, as the look: slot 0 is the board's curves, slots 1..8
presets 1..8's own (present: loading that preset uses them; absent or cleared with a record of zeros: the
board's), slot 9 the curves in use now (GET: word 39 says whose, 0 the board's or n preset n's; SET: in use
at once and saved with the settings where they belong: send it quietly while dragging). Slot 0 always
answers (the defaults when none is stored); while it is in use, SET there is in use at once and saved with
the settings 3 s after the last change, as in 1.9.0. Word 0 = 1 | Keep feel << 8 (0/1, 1.9.3, slots 0 and
9 only: *Save preset* also saves the curves in use with the preset);
words 1..10 = 40 bytes in four parts of 10: chord velocity, melody velocity, chord pressure, melody
pressure. Each part: 8 curve points (0..127, outputs at inputs 0, 127/7, ... 127, straight between),
then for velocity the softest and loudest velocity (1..127, softest <= loudest; used where Chord vel /
Melody vel is Custom, value 6, 1.9.0), for pressure where it starts and where it is full (0..127 on the
way from Release to Full, start < full). The rest 0; a record of zeros sets the defaults (a straight
line, velocity 1..127, pressure 0..127: pressure exactly as before 1.9.0). Live settings: Chord vel
(word 1 byte 1) and Melody vel (word 31 byte 3, 0x80 | value) may be 6 (Custom) from 1.9.0.

Step pattern record (1.5.0, `src/tb/lanes.h` ln_encode): slot = lane × 4 + chunk (lane 0..3,
chunk 0..3 = steps 1-16, 17-32, 33-48, 49-64; slots 16 and up are for more patterns later and
answer status 3 now). Word 0 = 1 (the format). Words 1 + 2i and 2 + 2i = step chunk × 16 + i as
64 bits, low word first: bit 0 on, 1-7 value (chord lane: chord pad 0..47; melody lane: the
right half's Scale layout position 4 × column + row, 0..47; drum lane: the MIDI note), 8-14
velocity (0 = 100), 15-22 the chord edits (as a progression's: lower | sus / 2 << 3 | add << 5;
sus 3 refused), 23-29 scale (0 = the scale playing, n = scale id n - 1), 30-33 gate, 34-37
probability, 38-40 ratchet, 41-45 condition, 46-49 nudge, 50 tie (all 0 = default, kept but not
played in 1.5.0), 51-63 must be 0. Chunk 0 only (other chunks 0 there): word 33 = kind (0
chords, 1 melody, 2 drums, 3 off) | channel (0 = the kind's own, 1..16) << 8 | length (0 = 16,
1..64) << 16 | rate (0 = 1/16, n = arpeggiator rate n - 1, 1..12) << 24; word 34 = direction
(0..3) | swing (0..50) << 8 | mute (0/1) << 16 | transpose (signed byte, -48..48) << 24; word 35
= Euclid fill (0..64); direction, swing, transpose and Euclid fill are kept but not played in
1.5.0. Lane 0 chunk 0 only: word 36 = the pattern's seed (16 bits). Words 37..39 are 0. A chunk
holding only defaults is absent (status 7); SET_RECORD plays it at once and stores it. 1.9.3: a quiet
SET_RECORD of a step pattern record plays it at once and stores it as the board's own step edits are, 3 s
after the last change, so the editor's step grid writes each changed record once per burst of clicks.
Firmware 1.7.0 and later play the step options (gate, probability, ratchet, condition, tie), direction and
Euclid fill.

Live settings, 1.0.0 additions (each 0 at its default, so a 0.11.0 record reads the same):
word 34 = chord set (0 = the scale's chords, 1..8; 9 = the Full chord layout, 10 = the chord grid, 1.3.0) | hints (0/1) << 8 | scale lock channel
(0 off, 1..16) << 16; words 35..38 = one byte per center button (button 0 first, in the
order Voicing I, II, III, Guitar, Spread I, II, III, Octave -3 .. +3): 0 = its printed
panel function, else action + 1 (`src/tb/controls.h` TC_ACT_: 0..13 the panel functions,
14 Shift, then Hold, Arp, Strum, Voice lead, Fold, Hints, Sustain, All notes off, Tap tempo,
Next/Previous preset, Next/Previous scale, Melody octave up/down, Play, Record, Add 7th,
Add 9th, Add 6th, Sus2, Sus4, Next chord set, Nothing = 38, then Pivot = 39 from 1.4.0); bytes
14 and 15 are 0. Presets
carry the chord set and hints; the board keeps its own buttons and scale lock when one loads.
A live settings record that changes the chord set is refused (status 3) while the board records a
progression (a progression keeps one set).

Live settings and presets, 1.5.0 addition: word 26 byte 3 = chords on the right (0 = the chord pads
on the left, as before; 1 = on the right). A board setting: loading a preset keeps the board's.
Firmware before 1.5.0 refuses records with it set; `recordsFor` clears it for them.

Live settings and presets, 1.4.0 additions (each 0 at its default, so a 1.3.0 record reads the
same): word 27 byte 2 = note repeat's rhythm (0 every step, n = Euclidean pattern n - 1, up to
120), byte 3 = the arpeggiator rhythm's rotation (0..15); word 33 byte 2 = the pivot target (0
off, 1..12 the major key on C..B, 13..24 the minor key). Wider ranges in existing bytes: arp
rate (word 2 byte 3) 0..11 (6..11 = 1/4T, 1/32T, 5 per beat, 7 per beat, dotted 1/8, dotted
1/16), arp rhythm (word 23 byte 2) 0..127 (8..127 = the Euclidean patterns: steps 2..16 in
order, hits 1..steps-1 within each), note repeat (word 31 byte 2) 0..8 (5..8 = 1/8T, 1/16T,
5 per beat, By row). Firmware before 1.4.0 refuses records with these values; the web editor
(`recordsFor` in `webapp/tb-protocol.js`) sets them back to the defaults for it. Captured
chord sets are ordinary chord set records (kind 6) named `CAPTURE n`.

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
