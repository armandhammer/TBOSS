# TBOSS user manual

TBOSS is open-source replacement firmware for the TheoryBoard THY333. This manual
covers every feature: what it does, how to use it, and a quick check that it works.
For installing the firmware see [flashing.md](flashing.md).

Contents: [The board](#the-board) · [Getting started](#getting-started) ·
[Playing](#playing) · [Key and scale](#key-and-scale) · [Chord edits](#chord-edits) ·
[Octaves](#octaves) · [Hold](#hold) · [Strum](#strum) · [Arpeggiator](#arpeggiator) ·
[MIDI channels and pressure](#midi-channels-and-pressure) · [Presets](#presets) ·
[MIDI in lights](#midi-in-lights) · [The menu](#the-menu) · [Screen and lights](#screen-and-lights) ·
[Web editor](#web-editor) · [Saved settings](#saved-settings) ·
[Troubleshooting](#troubleshooting) · [Feature check](#feature-check)

---

## The board

```
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  C  C# D  D# E  F  F# G  G# A  A# B │ 5 12 M1 M2 M3 M4 M5 M6 M7 U1 U2 U3  │  top row: 24 buttons
 │            root buttons             │            mode buttons             │
 ├────────────────────────┬────────────┴────────────┬────────────────────────┤
 │                        │        [ screen ]       │                        │
 │   CHORD PADS           │   5 navigation buttons  │   MELODY PADS          │
 │   12 columns x 4 rows  │   14 function buttons   │   12 columns x 4 rows  │
 │   (left half)          │   (centre panel)        │   (right half)         │
 └────────────────────────┴─────────────────────────┴────────────────────────┘
```

- **Pads (96):** pressure-sensitive. Left half plays chords, right half plays single notes.
- **Root buttons (12):** the key, C to B from left to right.
- **Mode buttons (12):** `5` major pentatonic, `12` chromatic, `M1`..`M7` the seven
  modes of the major scale, `U1`..`U3` your own scales.
- **Navigation buttons (5):** *Menu*, *Previous*, *Next*, *Select* and *Back*. They
  open and move through the menu; while playing, *Select*, *Back*, *Previous* and
  *Next* are shortcuts (see [Shortcuts](#shortcuts)).
- **Function buttons (14):** chord edits and octave shifts, numbered F0..F13.
  Press any of them and the screen shows its number and job (for example
  `F7 ADD 7TH`), so you can learn them without a chart.

## Getting started

1. Connect the TheoryBoard to the computer with USB. About 1.5 seconds after power-up
   (the board's start-up loader waits that long) the screen shows the play screen and
   the pads light up.
2. The computer sees a USB MIDI device called **TheoryBoard MIDI**. No driver is needed
   on Windows, macOS or Linux. (Windows may show an older name it remembers for the
   same port; it is the same device.)
3. In your music software (DAW), enable **TheoryBoard MIDI** as a MIDI input and put a
   software instrument on a track that listens to it.
4. **Make sure the instrument is polyphonic.** Many synth presets are mono or legato:
   they play only one note at a time, so a second pad seems to cut off the first.
   That is the instrument, not the board.

**Check:** play a few right-hand pads; you hear notes, and the pressed pads light white.

## Playing

### Melody pads (right half)
- Each column is one step of the current scale, from the root on the left.
  With a 7-note scale, columns 1–7 are one octave and columns 8–12 continue upward.
- The **bottom row** starts on the root (C3 by default); each row up is one octave higher.
- **Velocity** comes from how hard the pad is struck (or a fixed 100, see the menu).

### Chord pads (left half)
- Each column is the chord built on that step of the scale (column 1 = I, column 2 = ii
  and so on in a major key), using only notes of the scale.
- The four rows are four voicings of that chord:

| Row | Voicing | C major example |
| --- | --- | --- |
| bottom | root position | C E G |
| 2nd from bottom | first inversion | E G C |
| 3rd from bottom | second inversion | G C E |
| top | root position, an octave up | C E G (higher) |

- The chord name appears large on the screen, for example `Dm` or `G7`. A name like
  `C/E` means a C chord with E as the lowest note.
- In pentatonic and chromatic scales, chords are built the same way (every other note of
  the scale), so they can be unusual; the screen names them, e.g. `Am/C`.

**Check:** with root C and mode M1 (major), the bottom-row chords read
C, Dm, Em, F, G, Am, Bdim from left to right.

## Key and scale

- **Root:** press a root button. The whole board moves to the new key. Notes that are
  already sounding keep playing until you lift your finger.
- **Scale:** press a mode button, or choose any scale in the menu (*Scale*), including
  natural minor, harmonic minor, melodic minor and minor pentatonic.
- **User scales (U1–U3):** made in the [web editor](#web-editor). Until a slot has a
  scale, pressing it shows `U1 IS EMPTY`.
- Press only one root or mode button at a time; two at once shows `ONE BUTTON AT A TIME`.

**Check:** press D, then M2 (Dorian): the screen shows `D` and `DORIAN`, and the
bottom-left chord is `Dm`.

## Chord edits

Chord edits change the **last chord pad you played**. They stay on that pad (and are
saved) until you reset them. Edited chord pads light purple.

| Button | Edit |
| --- | --- |
| F0 / F1 / F2 | Move the chord's root / 3rd / 5th down an octave (press again to undo) |
| F7 | Add the 7th (from the scale) |
| F8 | Add the 9th |
| F11 | Add the 6th |
| F9 | sus2: the 3rd becomes the 2nd (press again to undo) |
| F10 | sus4: the 3rd becomes the 4th |
| F12 | Reset this chord |
| F13 | Reset all chords |

If you have not played a chord pad yet, an edit shows `PLAY A CHORD 1ST`. An edit that
would take a note outside the MIDI range shows `OUT OF RANGE` and is not made.

**Check:** play the bottom-left chord in C major (`C`), press F7: the name becomes
`Cmaj7` and the pad turns purple. Press F12: back to `C`.

## Octaves

| Button | Effect |
| --- | --- |
| F3 / F4 | Chord half one octave down / up |
| F5 / F6 | Melody half one octave down / up |

Each half moves up to two octaves either way. The screen line `OCT C2 C3` shows the
lowest root of each half (chords, melody).

## Hold

With hold on, a chord keeps sounding after you lift your hand, so the left hand is free.

- Turn it on or off with **Select** while playing, or in the menu (*Hold*).
  The screen shows `HOLD` on the octave line.
- Touching **another chord pad** replaces the held chord.
- Touching the **held chord's pad again** stops it.
- Melody pads are never held.
- Turning hold off stops a held chord.

**Check:** hold on, play a chord and lift your hand: it keeps sounding. Touch the same
pad: silence.

## Strum

Instead of all at once, a chord's notes start one after another, like a strummed guitar.

- Menu → *Strum*: **Off**, **Up** (low to high), **Down** (high to low),
  **Up+Down** (alternates direction on each strike).
- Menu → *Strum time*: 10 to 120 ms between notes.
- If you let go before all notes have started, the remaining notes are skipped.
- Strum does not apply while the arpeggiator is on.

**Check:** Strum Up, Strum time 80 ms: a chord pad clearly plays low-to-high.

## Arpeggiator

While the arpeggiator is on, chord pads don't sound as chords. Their notes are played
one at a time in a pattern. Melody pads still play normally on top.

- Turn it on or off with **Back** while playing (it remembers your last pattern), or
  choose a pattern in the menu (*Arp*):

| Pattern | Order |
| --- | --- |
| Up | lowest to highest, repeat |
| Down | highest to lowest |
| Up-down | up then down, without repeating the top and bottom notes |
| Random | any note of the chord each step |
| As played | pads in the order you touched them, each chord low to high |

- **Arp rate:** note length: 1/4, 1/8, 1/8T (triplet), 1/16, 1/16T, 1/32 of the tempo.
- **Arp octaves:** 1, 2 or 3: the pattern repeats in higher octaves.
- **Arp gate:** how long each note sounds: 25, 50, 75 or 100 % of a step.
- **Tempo:** 40 to 240 BPM.
- Holding more than one chord pad combines their notes.
- **Latch:** turn [Hold](#hold) on and the arpeggio keeps running after you lift your
  hand; touch another chord to change it, or the same pad to stop.
- Velocity follows how hard you struck the last chord pad.
- **Sync to your DAW:** if your music software sends **MIDI clock** to TheoryBoard MIDI,
  the arpeggiator follows it and the screen shows `EXT` instead of the tempo. Pressing
  Play/Start in the DAW restarts the pattern on the beat. When the clock stops, the
  board goes back to its own tempo after a quarter of a second. (In most DAWs: MIDI
  settings → enable "send MIDI clock" or "sync" output for the TheoryBoard MIDI port.)

**Check:** press Back, hold the bottom-left chord: C E G C E G… at 120 BPM in
sixteenths. Change *Arp rate* to 1/8: half as fast.

## MIDI channels and pressure

- Menu → *Chord ch* and *Melody ch*: the MIDI channel (1–16) of each half. Put
  them on different channels to play two instruments, e.g. pads on the left and lead on
  the right. Both default to channel 1.
- Menu → *Pressure*: what pressing harder on a held pad sends.

| Setting | Sends |
| --- | --- |
| Off | nothing |
| Poly AT | polyphonic aftertouch for single melody notes |
| Channel AT | channel aftertouch: the strongest held pad of each half |
| CC1 mod | mod wheel, from the strongest held pad of each half |
| CC2 breath / CC11 expr / CC74 filter | that controller, the same way |

Channel AT and the CC settings send one value per half on that half's channel (one value
for both if they share a channel). CC74 often opens a synth's filter; CC1 usually adds
vibrato. Releasing all pads sends 0.

- Menu → *Velocity*: **Dynamic** (from strike force) or **Fixed 100**.

**Check:** Chord ch 2, Melody ch 3, a MIDI monitor shows chord notes on channel 2 and
melody notes on channel 3. Pressure CC74: pressing harder on a held pad moves CC74.

## Presets

A preset stores the whole musical setup: root, scale, octaves, every chord edit, hold,
strum, arpeggiator settings, tempo, channels, pressure and velocity. Brightness and the
MIDI-in lights are not part of presets. There are 8 slots.

- **Save:** Menu → *Save preset* → choose 1–8 → Select. Saving over a slot replaces it.
- **Load:** Menu → *Load preset* → choose a slot → Select. Empty slots show `EMPTY`.
- **Quick switch while playing:** **Previous** / **Next** load the previous / next saved
  preset. The screen shows `PRESET 3 LOADED` and `P3` near the scale name.
- A preset that uses a user scale which has since been cleared will not load.

**Check:** save preset 1, change root and scale, press Next: the saved setup returns.

## MIDI in lights

With *MIDI in LEDs* on (the default), notes your computer sends **to** the board light
the pads that play them, in green: melody pads whose note is sounding, and chord pads
whose whole chord is sounding. Play a MIDI file or a chord progression in your DAW into
the TheoryBoard MIDI output port to see where it sits on the board.

**Check:** send a C major chord from the DAW: the C-major chord pads and the C, E, G
melody pads light green.

## The menu

Open and close with **Menu**. **Previous / Next** move; **Select** starts changing the
highlighted item (its value is shown inverted); Previous / Next change the value;
**Select** keeps it; **Back** cancels. The list scrolls.

| Item | Values | Default |
| --- | --- | --- |
| Scale | 12 built-in scales, U1–U3 | Major |
| Hold | Off / On | Off |
| Strum | Off / Up / Down / Up+Down | Off |
| Strum time | 10, 15, 20, 30, 40, 60, 80, 120 ms | 20 ms |
| Arp | Off / Up / Down / Up-down / Random / As played | Off |
| Arp rate | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 | 1/16 |
| Arp octaves | 1–3 | 1 |
| Arp gate | 25 / 50 / 75 / 100 % | 50 % |
| Tempo | 40–240 BPM | 120 |
| Chord ch | 1–16 | 1 |
| Melody ch | 1–16 | 1 |
| Pressure | Off / Poly AT / Channel AT / CC1 / CC2 / CC11 / CC74 | Poly AT |
| Velocity | Dynamic / Fixed 100 | Dynamic |
| Load preset | 1–8 | |
| Save preset | 1–8 | |
| MIDI in LEDs | Off / On | On |
| Brightness | 1–8 | 4 |
| LED test | Start | |

While the menu is open, function buttons do nothing (`CLOSE THE MENU FIRST`).

### Shortcuts

While playing (menu closed):

| Button | Shortcut |
| --- | --- |
| Select | Hold on/off |
| Back | Arpeggiator on/off |
| Previous / Next | Previous / next saved preset |

## Screen and lights

Play screen, top to bottom:
1. Root (large), scale name, and the mode button and/or preset (`MODE M2  P3`).
2. Chord name of the last chord pad played, with its edits (`Dm7`, `G7sus4`, `C/E`).
3. The last thing that happened (`ROOT D`, `F7 ADD 7TH`, `PRESET 2 LOADED`…).
4. Octaves (`OCT C2 C3`) and `HOLD` when hold is on.
5. Arpeggiator (`ARP UP 1/16 120` or `EXT`), strum (`STRUM UP 20MS`), or velocity
   and pressure (`VEL DYN PRESS POLY`). `USB: WAITING` until a computer is connected.

Pad colours:

| Colour | Meaning |
| --- | --- |
| Orange | the root of the scale |
| Blue | the fifth |
| Dim | other scale steps |
| Purple | a chord pad with edits |
| White | sounding (pressed, held, or feeding the arpeggiator) |
| Green | notes arriving over MIDI |
| Cyan (right half) | the scale being edited in the web editor |

The centre buttons are backlit; *Brightness* sets all lights. *LED test* lights every
pad red, green and blue, then each pad and each button light in turn.

## Web editor

The web editor runs in **Chrome or Edge** on a computer (Safari and Firefox lack the
MIDI features it needs). It makes custom scales and backs up the board's settings.

### Start it
From the folder you downloaded TBOSS into:
```
python scripts/tboss_editor.py
```
From anywhere else, give the full path, e.g. on Windows
`python "$HOME\Documents\TBOSS\scripts\tboss_editor.py"`. It starts a small web
server on your computer only and opens the editor at `http://localhost:8000`. Keep the window that runs it open; Ctrl+C stops
it. (A page opened straight from the file, or served from another folder, will not
work: browsers allow MIDI with SysEx only for `localhost` or `https` pages.)

Click **Connect**. Chrome asks whether the page may "control and reprogram your MIDI
devices": choose **Allow**. The header turns to `Connected` and shows the firmware and
the external memory found. With several MIDI devices, pick **TheoryBoard MIDI** in the
*Board* list.

The board stops its notes while the page talks to it and plays again about a second
after the last command, so don't play while saving.

### Custom scales
1. Choose a slot tab: **U1**, **U2** or **U3**.
2. Click piano keys to add or remove notes. The root (orange) is always included; a scale
   needs **5 to 12 notes**. Below each key is its interval (`b3` = minor third, `#4` =
   augmented fourth…). *Start from* fills in a known scale to modify (blues, whole tone,
   Hungarian minor, Hirajoshi and more). The summary line names a match if your scale
   is a known one.
3. *Root for listening* only changes what **Play** sounds like; scales are stored relative
   to the root, so the board can play them in any key.
4. While you edit, the board's right-hand pads show the scale as 12 columns of
   semitones from the left: orange root, cyan notes, dark gaps.
5. Type a **name** (up to 12 characters, capitals) and click **Save to U1**.
6. Press **U1** on the board (or click **Use it on the TB**). The chords, pads and lights
   follow the new scale. **Clear slot** empties it (not possible while it is in use).

### Settings backup
- **Download settings backup** saves the current settings, all presets and user scales
  to a `.json` file.
- **Restore from file** writes them back (user scales first, then presets, then the
  current settings). Installing new firmware clears the settings, so back up first.

### External memory backup
The board has a 16 MB memory chip holding data from its original firmware. TBOSS only
ever **reads** it. **Back up external memory** copies the whole chip to a file, reading
it twice; the copy counts as verified only if both reads are identical. It takes several
minutes per read and the board does not play meanwhile. Save both downloaded files
(the `.bin` image and the `.txt` with its SHA-256).

## Saved settings

Every change is saved automatically 3 seconds after you stop changing things, and comes
back at power-up, together with presets and user scales. Installing or restoring firmware
clears everything, so use the web editor's settings backup first.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Only one note at a time | The instrument is mono/legato: switch the preset or the synth to poly. |
| No sound | Check the DAW's MIDI input and the track's MIDI channel against *Chord ch* / *Melody ch*. |
| A chord stays on | Hold is on (`HOLD` on screen): touch the chord's pad, or press Select. |
| Chord pads play single notes in a pattern | The arpeggiator is on (`ARP` on screen): press Back. |
| Pads play nothing for a second | The web editor just talked to the board; it resumes after 1 s. |
| Editor shows "404 File not found" | Start it with `python scripts/tboss_editor.py` from the TBOSS folder (not `http.server` from another folder). |
| Editor says MIDI access refused | Chrome's site settings for localhost: allow MIDI devices, then Connect again. |
| Editor: "No TheoryBoard reply" | Pick TheoryBoard MIDI in *Board*; close other programs holding the port exclusively. |
| Arpeggiator ignores the DAW tempo | Enable MIDI clock output to the TheoryBoard MIDI port in the DAW. |
| `U1 IS EMPTY` | Make a scale in the web editor first. |
| `OUT OF RANGE` | That edit or octave would leave the MIDI note range; move the octave back first. |

## Feature check

A quick pass through everything, for example after installing a new version. Expected
results are in brackets.

1. Power up with USB → after ~1.5 s the play screen; DAW sees TheoryBoard MIDI. [pads lit]
2. Melody pads, several at once, soft and hard. [polyphonic, louder when harder]
3. Root C, mode M1, bottom row of chord pads left to right. [C Dm Em F G Am Bdim on screen]
4. Press D, then M2. [`D DORIAN`, bottom-left chord `Dm`]
5. Bottom-left chord, F7, then F12. [`Dm7`, purple pad; back to `Dm`]
6. F3 twice, F4 twice. [`OCT` line moves down and back]
7. Select (hold), play a chord, lift, touch it again. [keeps sounding; then silence]
8. Menu → Strum Up, Strum time 80 ms, play a chord. [notes low to high]
9. Strum Off; Back (arp), hold a chord. [single notes in a pattern; `ARP UP 1/16 120`]
10. Arp rate 1/8, Arp octaves 2, Arp gate 25 %. [slower, two octaves, short notes]
11. DAW sends MIDI clock, press Play. [`EXT` on screen, pattern follows the DAW]
12. Back (arp off). Chord ch 2, Melody ch 3, MIDI monitor. [chords on 2, melody on 3]
13. Pressure CC74, hold a pad and press harder. [CC74 values rise, 0 on release]
14. Save preset 1; change root; press Next. [`PRESET 1 LOADED`, setup restored]
15. Send a C major chord from the DAW to the board. [matching pads light green]
16. Web editor: Connect, U1, *Start from* Blues, name `BLUES`, Save, press U1. [`BLUES` on screen; right pads showed the scale while editing]
17. Web editor: Download settings backup; Restore from file. [`Restored n of n records`]
18. Power off and on. [settings, presets and U1 come back]
