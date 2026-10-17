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
| 01 | IDENTIFY | - | protocol 1, stage 3, user scales 12 (3 before 0.6.0), presets 8, record words 40 (20 before 0.6.0), NOR present 0/1, packed[JEDEC 3 bytes, size 4 bytes LE], version 3 bytes |
| 02 | GET_SCALE | slot 0..11 | slot, mask low 7, mask high 5, name 12 bytes (0 padded) |
| 03 | SET_SCALE | slot, mask low 7, mask high 5, name 12 bytes | slot. Mask 0 clears the slot. Stored in flash at once. |
| 04 | PREVIEW | mask low 7, mask high 5, root 0..11 [, slot 0..11] | none. Shown on all pads and the display for 5 s; mask 0 ends it. The optional slot (firmware 0.5.0+; older firmware ignores 4-byte previews) names the user slot being edited on the display. |
| 05 | SELECT_SCALE | scale id 0..36 (12..23 = U1..U12, 24..36 the extra built-in scales) | - |
| 06 | GET_RECORD | kind, slot | kind, slot, packed[160 bytes = 40 words LE] (status 7: nothing stored) |
| 07 | SET_RECORD | kind, slot, packed[160 bytes, or 80 bytes from older firmware] | kind, slot. Validated; live settings apply at once. A 20-word record is padded with zeros (the 0.6.0 settings keep their current values). |
| 08 | NOR_READ | address 4 x 7 bits, length 2 x 7 bits (1..2048) | the 6 request bytes, packed[data, CRC-32 of data LE] |

Scale mask: bit n = the note n semitones above the root; bit 0 (root) required,
5..12 notes. Names: up to 12 characters, shown in capitals.

Record kinds: 1 live settings (slot 0), 2 preset (slots 0..7), 3 user scale
(slots 0..11). A record is 40 words (20 before 0.6.0), the same payload as in flash sector 11
(`src/tb/settings.c`): treat it as opaque for backup and restore. Restore scales
first, then presets, then the live settings (they may select a user scale).
