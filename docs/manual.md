# TBOSS user manual

TBOSS is open-source replacement firmware for the TheoryBoard THY333. This manual
covers every feature: what it does, how to use it, and a quick check that it works.
For installing the firmware see [flashing.md](flashing.md). TBOSS is **alpha** software:
see [Known issues](#known-issues) and please report anything that doesn't work as described.

Contents: [The board](#the-board) · [Getting started](#getting-started) ·
[Playing](#playing) · [Key and scale](#key-and-scale) · [Chord panel](#chord-panel) ·
[Chord edits](#chord-edits) · [Octaves](#octaves) · [Melody layouts and drums](#melody-layouts-and-drums) ·
[Hold](#hold) · [Strum](#strum) · [Arpeggiator](#arpeggiator) ·
[MIDI channels and pressure](#midi-channels-and-pressure) · [MIDI clock and transport](#midi-clock-and-transport) ·
[TRS MIDI out](#trs-midi-out) · [Sustain pedal](#sustain-pedal) · [Presets](#presets) ·
[MIDI in lights](#midi-in-lights) · [Settings menu](#settings-menu) · [Screen and lights](#screen-and-lights) ·
[Web editor](#web-editor) · [Saved settings](#saved-settings) ·
[Troubleshooting](#troubleshooting) · [TBOSS and the original TheoryBoard](#tboss-and-the-original-theoryboard) ·
[Known issues](#known-issues) · [Feature check](#feature-check)

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
  modes of the major scale, `U1`..`U3` your own scales (U4–U12 are in the menu).
- **Navigation buttons (5),** the row just below the screen, left to right:
  **◀ Left**, **▲ Up**, **● Centre**, **▼ Down**, **▶ Right**. Centre opens the
  [settings](#settings-menu); while playing, Up/Down step through the scales (or what
  you choose in Settings → Play → *Arrows*) and Right turns hold on and off (see
  [Shortcuts](#shortcuts)).
- **Jacks (back):** TRS MIDI out (Type A) and a sustain pedal input.
- **Chord panel (14 centre buttons):** the printed panel: voicing (I, II, III, guitar),
  spread (I, II, III) and the octave V. They do what their icons say: see
  [Chord panel](#chord-panel).

## Getting started

1. Connect the TheoryBoard to the computer with USB. About 1.5 seconds after power-up
   (the board's start-up loader waits that long) the screen briefly shows `TBOSS` and the
   version, then the play screen, and the pads light up.
2. The computer sees a USB MIDI device called **TheoryBoard MIDI**. No driver is needed
   on Windows, macOS or Linux. (Windows may show an older name it remembers for the
   same port; it is the same device.)
3. In your music software (DAW), enable **TheoryBoard MIDI** as a MIDI input and put a
   software instrument on a track that listens to it.
4. **Make sure the instrument is polyphonic.** Many synth presets are mono or legato:
   they play only one note at a time, so a second pad seems to cut off the first.
   That is the instrument, not the board.

**Check:** play a few right-hand pads; you hear notes, and each pad lights white while
you touch it (brighter as you press harder).

## Playing

### Melody pads (right half)
- Each column is one step of the current scale, from the root on the left.
  With a 7-note scale, columns 1–7 are one octave and columns 8–12 continue upward.
- The **bottom row** starts on the root (C3 by default); each row up is one octave higher.
- **Velocity** comes from how hard the pad is struck (or a fixed 100, see the menu).

### Chord pads (left half)
- Each column is the chord built on that step of the scale (column 1 = I, column 2 = ii
  and so on in a major key), using only notes of the scale.
- The four rows are four voicings of that chord (with the [chord panel](#chord-panel)
  in its normal state):

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
- **Scale:** press a mode button, or choose any scale in the menu (Settings → Play →
  *Scale*).
- **Families (0.7.0):** the **M1–M7** buttons play the seven modes of a scale family,
  chosen in Settings → Play → *Family*:
  | Family | M1 | M2 | M3 | M4 | M5 | M6 | M7 |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | Major (default) | Major | Dorian | Phrygian | Lydian | Mixolydian | Minor | Locrian |
  | Melodic minor | Melodic minor | Dorian ♭2 | Lydian augmented | Lydian dominant | Mixolydian ♭6 | Locrian ♮2 | Altered |
  | Harmonic minor | Harmonic minor | Locrian ♮6 | Ionian ♯5 | Dorian ♯4 | Phrygian dominant | Lydian ♯2 | Ultralocrian |
  | Harmonic major | Harmonic major | Dorian ♭5 | Phrygian ♭4 | Lydian ♭3 | Mixolydian ♭2 | Lydian augmented ♯2 | Locrian ♭♭7 |

  These four are the seven-note scales with no two half steps in a row and no gap
  wider than a minor third: together, 28 of the most-used scales are one press away.
- **The scale library (0.7.0):** 86 built-in scales in all: the four families above,
  pentatonic scales (major, minor, Egyptian, Yo, Hirajoshi, In sen, Iwato, Kumoi,
  Hamsadhwani and more), six-note scales (blues, whole tone, augmented, Prometheus…),
  other seven-note scales (Hungarian, Neapolitan, Persian, double harmonic, Indian
  thaats…) and scales of eight notes and more (bebop, diminished, Messiaen's modes,
  chromatic). All are in Settings → Play → *Scale*; the web editor's
  [scale library](#scale-library) lists them by family and finds scales by their notes.
- **Favourites (0.7.0):** up to 128 scales you pick in the web editor, in your order.
  With Settings → Play → *Arrows* on *Scale*, **▲ / ▼** on the play screen step through
  them (`FAV 3/12 BLUES`); with no favourites they step through every scale.
- **User scales (U1–U12):** made in the [web editor](#web-editor). U1–U3 have buttons;
  all twelve are in Settings → Play → *Scale*. Until a slot has a scale, pressing it
  shows `U1 IS EMPTY`.
- Press only one root or mode button at a time; two at once shows `ONE BUTTON AT A TIME`.

**Check:** press D, then M2 (Dorian): the screen shows `D` and `DORIAN`, and the
bottom-left chord is `Dm`.

## Chord panel

The 14 buttons in the centre do what the panel prints next to them. They shape **every
chord pad** (not just one) and work on the play screen and in the settings alike. A
held chord keeps the notes it started with; the next touch plays the new shape (or turn
on *Live update*, see [Settings menu](#settings-menu)). The bottom line of the screen
says what changed (`VOICING II`, `SPREAD I`, `OCTAVES -1 0 +1`).

**Voicing** (top row: I, II, III and the guitar): "voice chords in different inversions
or like a guitar".
- **I / II / III** move every chord up one, two or three inversions: with I the bottom
  row of C plays E G C instead of C E G, and the other rows move up with it.
- **Guitar** (0.7.0) plays each chord the way a guitar does: six strings in standard
  tuning (E A D G B E), each string the lowest chord note it has under the hand, and the
  strings below the chord's root left out. The bottom row is the open position (C is
  x32010, G 320003, D xx0232, Am x02210); the rows above move the hand 3, 5 and 7 frets
  up the neck. Settings → Strum → *Capo* puts a capo on fret 1 to 11: the chords stay in
  the key but take the shapes and brighter sound of the higher position.
- Press the lit choice again to go back to normal. One voicing at a time.

**Spread** (second row: I, II, III): "drop root note of chords down an octave". I drops
the root one octave, II two octaves, III three. Press it again to turn it off.

**Octave** (the V of seven buttons, marked −3 to +3): "raise, lower, or stack octaves of
chords". 0 is normal.
- **Each tap turns that octave on or off.** Every lit octave plays, so lighting several
  stacks them: with 0 lit, tap +1 for a doubled, wider chord.
- **To move** the chords to one octave: tap the new one on, then the old one off (for
  +1 alone from 0: tap +1, then 0).
- Turning off the last lit octave goes back to 0. (0.6.0 and 0.6.1 used tap = that
  octave alone, hold one and tap others = stack.)
- A stacked chord plays at most 16 notes; notes beyond the MIDI range are left out.

**Lights:** a panel button is fully lit while its choice is on and dim (15 %) otherwise,
so the panel always shows the voicing, spread and octaves in use (0.6.1).

**Check:** play the bottom-left chord in C major (C E G), press voicing **I** and play it
again: E G C. Press **I** again, then tap **+1**: the chord plays in two octaves. Tap
**+1** again to go back.

## Chord edits

Chord edits change the **last chord pad you played**; they stay on that pad (and are
saved) until you reset them. Edited chord pads light paler. Find them in Settings →
**Chords**:

| Item | Edit |
| --- | --- |
| Add 7th / Add 9th / Add 6th | Off / On: add that note (from the scale) |
| Sus | Off / SUS2 / SUS4: the 3rd becomes the 2nd or the 4th |
| Strum pad | On / Off: this pad strums or plays all at once (see [Strum](#strum)) |
| Reset chord | ● resets this pad (`CHORD EDITS RESET`) |
| Reset all | ● resets every chord pad |

Voicing and Spread on the same page are the panel's settings (for every chord).

If you have not played a chord pad yet, an edit shows `PLAY A CHORD 1ST`. An edit that
would take a note outside the MIDI range shows `OUT OF RANGE` and is not made.

**Check:** play the bottom-left chord in C major (`C`), open Settings → Chords → *Add 7th*,
set On: the name becomes `Cmaj7` and the pad turns paler. *Reset chord*: back to `C`.

## Octaves

- **Melody half:** Settings → Play → *Melody oct*, −2 to +2 octaves. If you change it
  often, set Settings → Play → *Arrows* to *Melody octave*: then **▲ Up** / **▼ Down** on
  the play screen move it (`MELODY OCT C4`).
- **Chord half:** the [chord panel](#chord-panel)'s octave buttons.

The screen line `OCT C2 C3` shows the lowest root of each half (chords, melody).

## Melody layouts and drums

Settings → Layout → *Layout* changes what the right-hand pads play:

| Layout | Right half |
| --- | --- |
| Scale (default) | each column one scale step, each row up an octave (as above) |
| Chromatic | each column one semitone from the root, each row up an octave: every note, out-of-scale notes dimmed |
| Fourths | each column one semitone, each row up a fourth (like a bass or guitar neck) |
| Drums | three 4×4 grids, see below |

**Drums** splits the right half into three grids of 4 columns × 4 rows, each played from
the bottom-left pad, left to right, then up a row:
- **Left grid:** the scale from the root, on the *Melody ch*.
- **Middle grid:** the same notes an octave lower, on the *Bass ch* (default 2): a bass
  line.
- **Right grid:** 16 drum notes, General MIDI 36 to 51 (kick on the bottom-left pad,
  then snare, claps, hi-hats, toms, cymbals), on the *Drum ch* (default 10, the General
  MIDI drum channel). These pads light amber.

**Check:** *Layout* Drums, a General MIDI drum kit on channel 10: the bottom-left pad
of the right grid plays a kick.
## Hold

With hold on, a chord keeps sounding after you lift your hand, so the left hand is free.

- Turn it on or off with **▶ Right** while playing, or in Settings → Play → *Hold*.
  The screen shows `HOLD` on the octave line.
- Touching **another chord pad** replaces the held chord.
- Touching the **held chord's pad again** stops it.
- Melody pads are never held.
- Turning hold off stops a held chord.

**Check:** hold on, play a chord and lift your hand: it keeps sounding. Touch the same
pad: silence.

## Strum

Instead of all at once, a chord's notes start one after another, like a strummed guitar.

- Settings → Strum → *Strum*: **Off**, **Up** (low to high), **Down** (high to low),
  **Up+Down** (alternates direction on each strike).
- Settings → Strum → *Strum time*: 10 to 120 ms between notes, or a note value at the
  tempo (1/96, 1/48, 1/32, 1/24, 1/16), which follows MIDI clock too.
- Settings → Strum → *Repeat*: Off, 1/4, 1/8, 1/16, 1/32. While you hold a chord pad it
  strums again at that rate, louder or softer as you press harder or lighter.
- Settings → Strum → *Cutoff*: Off, 1/32 to 1/1. A strummed chord ends after that note
  value even while held (with *Repeat*, until the next strike): short, rhythmic stabs.
- Settings → Chords → *Strum pad* turns strum off for one pad (the last one played),
  so some chords strum and others hit all at once.
- **Feel (0.7.0):** Settings → Strum → *Feel time* lets each strummed note come up to 5,
  10, 20 or 40 ms late, and *Feel vel* varies each note's velocity by up to 5, 10, 20
  or 30, at random, for a looser, human strum. The notes always start in strum order.
- Settings → Strum → *Capo*: see [Guitar voicing](#chord-panel). Strum with the guitar
  voicing for a strummed guitar.
- If you let go before all notes have started, the remaining notes are skipped.
- Strum does not apply while the arpeggiator is on (except *Chords*, see below).

**Check:** Strum Up, Strum time 80 ms: a chord pad clearly plays low-to-high. *Repeat*
1/8: holding the pad strums it again every eighth note. *Feel time* 40 ms and *Feel vel*
30: each strike sounds slightly different.

## Arpeggiator

While the arpeggiator is on, chord pads don't sound as chords. Their notes are played
one at a time in a pattern. Melody pads still play normally on top.

- Turn it on by choosing a pattern in Settings → Arpeggiator → *Arp*, and off by
  choosing *Off* there. The patterns:

| Pattern | Order |
| --- | --- |
| Up | lowest to highest, repeat |
| Down | highest to lowest |
| Up-down | up then down, without repeating the top and bottom notes |
| Random | any note of the chord each step |
| As played | pads in the order you touched them, each chord low to high |
| Chords | the whole chord on every step (strummed if *Strum* is on) |

- **Arp rate:** note length: 1/4, 1/8, 1/8T (triplet), 1/16, 1/16T, 1/32 of the tempo.
- **Arp octaves:** 1, 2 or 3: the pattern repeats in higher octaves.
- **Arp gate:** how long each note sounds: 25, 50, 75 or 100 % of a step.
- **Tempo:** 40 to 240 BPM.
- **Rhythm:** eight 8-step patterns, shown as `X` (play) and `.` (rest), for example
  `X.XX.XX.`. The pattern keeps moving through the chord on rests.
- **Swing:** 0 to 50 %: every first note of a pair is longer, the second shorter.
- **Sync start:** On: a new arpeggio waits for the next step of the clock, so it starts
  in time with the tempo or the DAW.
- Holding more than one chord pad combines their notes.
- **Latch:** On: the arpeggio keeps running after you lift your hand; touch another
  chord to change it, or the same pad to stop. ([Hold](#hold) does the same.)
- Velocity follows how hard you struck the last chord pad.
- **Sync to your DAW:** if your music software sends **MIDI clock** to TheoryBoard MIDI,
  the arpeggiator follows it and the screen shows `EXT` instead of the tempo. Pressing
  Play/Start in the DAW restarts the pattern on the beat. When the clock stops, the
  board goes back to its own tempo after a quarter of a second. (In most DAWs: MIDI
  settings → enable "send MIDI clock" or "sync" output for the TheoryBoard MIDI port.)

**Check:** set *Arp* to Up, hold the bottom-left chord: C E G C E G… at 120 BPM in
sixteenths. Change *Arp rate* to 1/8: half as fast.

## MIDI channels and pressure

- Settings → MIDI → *Chord ch* and *Melody ch*: the MIDI channel (1–16) of each half. Put
  them on different channels to play two instruments, e.g. pads on the left and lead on
  the right. Both default to channel 1.
- Settings → Play → *Chord AT* and *Melody AT*: what pressing harder on a held pad sends,
  set separately for each half (so you can, say, swell chords with CC74 and bend melody
  notes with poly aftertouch, or switch one half off).

| Setting | Sends |
| --- | --- |
| Off | nothing from that half |
| Poly AT (default) | polyphonic aftertouch for each note the pad plays. A chord pad sends it for each of its notes, so leaning into a chord swells all of it. A note that another held pad also plays gets none, to keep it unambiguous. |
| Channel AT | channel aftertouch: the strongest held pad of that half |
| CC1 mod | mod wheel, from the strongest held pad of that half |
| CC2 breath / CC11 expr / CC74 filter | that controller, the same way |
| Pitch bend | bends up as you press harder (the strongest held pad of that half), back to the centre on release |

Channel AT and the CC settings send one value per half on that half's channel; when both
halves share a channel and the same setting, they send one value together. CC74 often
opens a synth's filter; CC1 usually adds vibrato. Releasing all pads sends 0. Poly
aftertouch needs a synth that responds to it (many respond only to channel aftertouch or
CCs: if leaning into pads does nothing, try Channel AT or CC74).

- Settings → Play → *Velocity*: **Normal** (from strike force), **Light** (louder from a
  light touch), **Firm** (needs a harder strike for loud notes) or **Fixed 100**.

**Check:** Chord ch 2, Melody ch 3, a MIDI monitor shows chord notes on channel 2 and
melody notes on channel 3. Chord AT CC74: pressing harder on a held chord pad moves CC74.

## MIDI clock and transport

- Settings → MIDI → *Clock out*: On sends MIDI clock at the board's *Tempo* to the
  computer (and the TRS jack), so a DAW or synth can follow the board. While a MIDI
  clock is coming in, the board follows it and sends none.
- Settings → MIDI → *Transport*: ● sends **Start** (`PLAY`) or **Stop** (`STOP`). With
  the pedal set to *Play/Stop*, the pedal does the same.

**Check:** *Clock out* On, the DAW synced to external MIDI clock from TheoryBoard MIDI:
the DAW shows 120 BPM; *Transport* ● starts and stops it.

## TRS MIDI out

> **New in 0.4.0, not yet confirmed on a board.** If it doesn't work for you, please
> send a [test report](https://github.com/armandhammer/TBOSS/issues/new?template=test_report.yml)
> with what Settings → About → *I/O test* shows.

Everything the board sends over USB also goes out of the **TRS MIDI out** jack, so it can
play a hardware synth directly. The jack is **TRS Type A**: use a Type A TRS-to-DIN
adapter (Type B adapters swap the wires and won't work).

- Settings → MIDI → *TRS out*: On (default) or Off.
- **Without a computer:** power the board from a USB charger or power bank with the TRS
  jack connected. After about 3 seconds without a computer it plays to the jack alone.
  Plugging into a computer later switches back to USB (held notes end first).
- The jack carries the notes, aftertouch and controllers you play, at MIDI's standard
  speed. Very dense pressure streams are paced to what the jack can carry.

**Check:** connect a synth through a Type A adapter; play pads: the synth plays.

## Sustain pedal

> **New in 0.4.0, not yet confirmed on a board.** Settings → About → *I/O test* shows
> whether the board sees the pedal (`PEDAL ... DOWN` while pressed); please report what
> it shows if the pedal does nothing.

Plug a sustain pedal (footswitch) into the pedal jack **before switching the board on**.

- Settings → Pedal → *Mode* (**Off** by default until the pedal input is confirmed on a
  board: switch it on to use a pedal):
  - **Sustain:** sends sustain (CC64) on the chord and melody channels. The
    screen shows `SUS` on the octave line while it is down.
  - **Hold:** the board's own [hold](#hold) while the pedal is down: chords keep
    sounding, and the arpeggiator keeps running.
  - **Sostenuto / Soft / Portamento:** sends CC66, CC67 or CC65 instead of sustain.
  - **Play/Stop:** each press sends Start, then Stop (see
    [MIDI clock and transport](#midi-clock-and-transport)).
  - **Note:** plays a note while the pedal is down, on the *Drum ch*: Settings → Pedal →
    *Note* (default C2 = 36, a General MIDI kick).
  - **Off** (default): the pedal is ignored.
- Settings → Pedal → *Type*:
  - **Auto** (default): whatever the pedal reads at power-up counts as "released", so both
    normally-open and normally-closed pedals work. That's why the pedal must be plugged
    in first.
  - **Normal / Inverted:** choose one if the pedal acts backwards, for example because
    it was plugged in after power-up.

**Check:** set *Mode* to Sustain, play a chord with the pedal down and let go of the pad:
the synth keeps sounding until you lift the pedal.

## Presets

A preset stores the whole musical setup: root, scale, octaves, every chord edit, hold,
strum, arpeggiator settings, tempo, channels, pressure and velocity. Brightness and the
MIDI-in lights are not part of presets. There are 8 slots.

- **Save:** Settings → Presets → *Save preset* → ● → ▲/▼ choose 1–8 → ●. Saving over a
  slot replaces it. The bottom line shows `PRESET 3 SAVED`.
- **Load:** Settings → Presets → *Load preset* → ● → choose a slot → ●. Empty slots show
  `EMPTY`.
- **Quick switch while playing:** hold **◀ Left** and press **▲ Up** / **▼ Down** to load
  the previous / next saved preset. The screen shows `PRESET 3 LOADED` and `P3` near the
  scale name.
- A preset that uses a user scale which has since been cleared will not load.

**Check:** save preset 1, change root and scale, go back to the play screen, hold ◀ Left
and press ▼ Down: the saved setup returns.

## MIDI in lights

With *MIDI in LEDs* on (the default), notes your computer sends **to** the board light
the pads that play them, in green: melody pads whose note is sounding, and chord pads
whose whole chord is sounding. Play a MIDI file or a chord progression in your DAW into
the TheoryBoard MIDI output port to see where it sits on the board.

**Check:** send a C major chord from the DAW: the C-major chord pads and the C, E, G
melody pads light green.

## Settings menu

Press **● Centre** on the play screen to open the settings. They are pages of items:

| Button | Page list | A page | Changing a value |
| --- | --- | --- | --- |
| ◀ Left | back to the play screen | back to the page list | cancel, keep the old value |
| ▲ Up / ▼ Down | previous / next page | previous / next item | higher / lower value |
| ● Centre | open the page | change the item (or start the LED test) | set the value |
| ▶ Right | open the page | change the item | set the value |

- **Leaving:** only ◀ Left leaves, one level per press; press it until the play screen
  shows. An extra press on the play screen does nothing. Shortcut: **hold ◀ Left and
  press ● Centre** to go straight to the play screen from anywhere.
- The settings reopen on the page you left. Up and Down wrap around in lists; values stop
  at their lowest and highest setting.
- The top line names the page and the position (`3/7`); the value being changed is shown
  inverted. The bottom line shows what the buttons do, or the result of the last action
  (`PRESET 3 SAVED`, `OUT OF RANGE`…).
- While the settings are open the root and mode buttons and the chord panel still work.

| Page | Item | Values | Default |
| --- | --- | --- | --- |
| Play | Scale | the 86 built-in scales (classic, more, the library), then U1–U12 | Major |
| | Family | Major / Mel minor / Harm minor / Harm major: what M1–M7 play | Major |
| | Hold | Off / On | Off |
| | Velocity | Normal / Fixed 100 / Light / Firm | Normal |
| | Chord AT | Off / Poly AT / Channel AT / CC1 / CC2 / CC11 / CC74 / Pitch bend | Poly AT |
| | Melody AT | the same | Poly AT |
| | Live update | Off / On: held pads move to the new notes when the root, scale, layout or chord panel changes | Off |
| | Arrows | what ▲/▼ do on the play screen: Scale / Melody octave / Preset / Tempo (a board setting: presets don't change it) | Scale |
| | Melody oct | −2 … +2: the melody half's octave | 0 |
| Chords | Voicing | Off / I / II / III / Guitar (the panel's) | Off |
| | Spread | Off / I / II / III (the panel's) | Off |
| | Add 7th, Add 9th, Add 6th | Off / On (last chord played) | Off |
| | Sus | Off / SUS2 / SUS4 (last chord played) | Off |
| | Strum pad | On / Off (last chord played) | On |
| | Reset chord / Reset all | ● resets | |
| Layout | Layout | Scale / Chromatic / Fourths / Drums | Scale |
| | Bass ch | 1–16 | 2 |
| | Drum ch | 1–16 | 10 |
| Strum | Strum | Off / Up / Down / Up+Down | Off |
| | Strum time | 10, 15, 20, 30, 40, 60, 80, 120 ms, 1/96, 1/48, 1/32, 1/24, 1/16 | 20 ms |
| | Repeat | Off, 1/4, 1/8, 1/16, 1/32 | Off |
| | Cutoff | Off, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 | Off |
| | Feel time | Off, 0–5, 0–10, 0–20, 0–40 ms late at random | Off |
| | Feel vel | Off, ±5, ±10, ±20, ±30 velocity at random | Off |
| | Capo | Off, fret 1–11 (guitar voicing) | Off |
| Arpeggiator | Arp | Off / Up / Down / Up-down / Random / As played / Chords | Off |
| | Arp rate | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 | 1/16 |
| | Arp octaves | 1–3 | 1 |
| | Arp gate | 25 / 50 / 75 / 100 % | 50 % |
| | Tempo | 40–240 BPM | 120 |
| | Rhythm | 8 patterns: `XXXXXXXX`, `X.X.X.X.`, `.X.X.X.X`, `XX.XX.X.`, `X..X..X.`, `XXX.XXX.`, `X.XX.XX.`, `X...X...` | `XXXXXXXX` |
| | Swing | 0, 10, 20, 30, 40, 50 % | 0 % |
| | Sync start | Off / On | Off |
| | Latch | Off / On | Off |
| MIDI | Chord ch | 1–16 | 1 |
| | Melody ch | 1–16 | 1 |
| | TRS out | Off / On | On |
| | Clock out | Off / On | Off |
| | Transport | ● Play / Stop | Stopped |
| Pedal | Mode | Off / Sustain / Hold / Sostenuto / Soft / Portamento / Play/Stop / Note | Off |
| | Type | Auto / Normal / Inverted | Auto |
| | Note | any MIDI note | C2 (36) |
| Presets | Load preset | 1–8 | |
| | Save preset | 1–8 | |
| Lights | Brightness | 1–8 | 4 |
| | Colours | Rainbow / Root+5th | Rainbow |
| | MIDI in LEDs | Off / On | On |
| | LED test | Start | |
| About | Version | the TBOSS version, e.g. `0.7.0` | |
| | I/O test | Start: shows the pedal input and two alternative inputs live, sends middle C to the TRS jack every second, and shows pads that read as held | |

### Shortcuts

On the play screen:

| Button | Shortcut |
| --- | --- |
| ● Centre | Open the settings |
| ▶ Right | Hold on/off |
| ▲ Up / ▼ Down | Set in Settings → Play → *Arrows*: **Scale** (default): next / previous of your [favourite scales](#key-and-scale) (`FAV 2/9 DORIAN`), or with none, of all scales including your U1–U12, skipping empty slots (`SCALE BLUES`); **Melody octave**: melody half an octave up / down; **Preset**: next / previous saved preset; **Tempo**: 5 BPM faster / slower (`TEMPO 125 BPM`) |
| Hold ◀ Left + ▲ Up / ▼ Down | Previous / next saved preset (whatever *Arrows* is set to) |
| ◀ Left | Nothing (so pressing it once too often on the way back is harmless) |

## Screen and lights

Play screen, top to bottom:
1. Root (large), scale name, and the mode button and/or preset (`MODE M2  P3`).
2. Chord name of the last chord pad played, with its edits (`Dm7`, `G7sus4`, `C/E`).
3. The last thing that happened (`ROOT D`, `VOICING II`, `PRESET 2 LOADED`…).
4. Octaves (`OCT C2 C3`), `HOLD` when hold is on, `SUS` while the sustain pedal is down.
5. Arpeggiator (`ARP UP 1/16 120` or `EXT`), strum (`STRUM UP 20MS`), or velocity
   and pressure (`VEL DYN AT POLY/POLY`). `USB: WAITING` until a computer is connected.
   A strum time at the tempo shows as a note value (`STRUM UP 1/16`).

Pad colours (Settings → Lights → *Colours*):

**Rainbow** (default): every pad shows the colour of its note, the same colour for the
same note everywhere on the board (C red, then orange, yellow, green, cyan, blue and violet
round to B). The colours move when you change root or mode. Root columns are brightest.

| Pad | Meaning |
| --- | --- |
| White, brighter the harder you press | you are touching it (shows the board senses you) |
| Soft white | sounding without your finger: held, latched, or feeding the arpeggiator |
| Paler colour | a chord pad with edits |
| White (not touched) | notes arriving over MIDI |

In the chromatic and fourths layouts, notes outside the scale are dimmed. In the drums
layout the drum grid is amber, its kick pad brightest.

**Root+5th**: root orange, fifth blue, other steps dim, edited chords purple, pressed pads
white, notes arriving over MIDI green.

**While the web editor edits a scale** (either setting), the whole board becomes a
keyboard of the scale: both halves show 12 columns, one per semitone from the editor's
root. The root column is white, scale notes are bright in their colour, other notes are
nearly dark. The screen shows `WEB EDITOR`, the slot (`U2`), the root and the notes.
Normal colours return 5 seconds after the editor stops sending.

A pad that already reads as pressed when the board starts (held, or a pad stuck on your
board) lights white and stays silent until it has been released once; everything else
plays normally. Settings → About → *I/O test* shows how many pads read as held and where
(`PADS HELD 1 L C10 R2`: left half, column 10, row 2 from the top).

The centre buttons are backlit; *Brightness* sets all lights. *LED test* lights every
pad red, green and blue, then each pad and each button light in turn.

## Web editor

The web editor runs in **Chrome or Edge** on a computer (Safari and Firefox lack the
MIDI features it needs). It browses the scale library and keeps your favourite scales on
the board, makes custom scales, and backs up the board's settings.

### Start it
Open **https://armandhammer.github.io/TBOSS/** in Chrome or Edge. Nothing to install.

To run it from your own copy instead (for example offline), from the folder you
downloaded TBOSS into:
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

### Scale library
The editor's first section lists every built-in scale, grouped into families.
- **Browse:** pick a group (or all scales) and an order: as listed, or **bright → dark**
  (by the average height of the notes above the root: Lydian is the brightest of the
  major modes, Locrian the darkest). Click a scale to see its notes in the board's
  colours in the chosen key, **Play** it, **Play it on the TB** (the board switches to
  it, keeping its key), **Add to favourites**, or **Copy to my scales** to change it in a
  user slot.
- **Find by notes:** click notes, or tick *Listen to MIDI keyboards* and play them on the
  TB or any MIDI keyboard: the list shows every scale and key that contains them, fewest
  notes first. Click one to select it in that key.
- **One note away:** under the selected scale, every listed scale you get by moving one
  of its notes a semitone (Major: lower 3 → melodic minor, raise 4 → Lydian, lower 7 →
  Mixolydian…). Click to go there: a quick way to hear what one note changes.
- **Favourites:** up to 128 scales (built-in or your own U1–U12) in the order the
  board's ▲/▼ step through them. Reorder with ▲ ▼, remove with ✕, then **Save favourites
  to the TB**. Needs TBOSS 0.7.0 or later.

### Custom scales
1. Choose a slot tab, **U1** to **U12** (U1 to U3 with firmware before 0.6.0).
2. Click piano keys to add or remove notes. The root (orange) is always included; a scale
   needs **5 to 12 notes**. Below each key is its interval (`b3` = minor third, `#4` =
   augmented fourth…). *Start from* fills in a known scale to modify (blues, whole tone,
   Hungarian minor, Hirajoshi and more). The summary line names a match if your scale
   is a known one.
3. *Root for listening* only changes what **Play** sounds like; scales are stored relative
   to the root, so the board can play them in any key.
4. While you edit, the board's pads show the scale on both halves as 12 columns of
   semitones from the root (root white, scale notes in colour, others dark), and its
   screen shows `WEB EDITOR`, the slot and the notes.
5. Type a **name** (up to 12 characters, capitals) and click **Save to U1**.
6. Press **U1** on the board (or click **Use it on the TB**; U4–U12 have no button,
   choose them in Settings → Play → *Scale* or with that button). The chords, pads and
   lights follow the new scale. **Clear slot** empties it (not possible while it is in use).

### Settings backup
- **Download settings backup** saves the current settings, all presets, user scales and
  favourites to a `.json` file.
- **Restore from file** writes them back (user scales first, then favourites, presets,
  then the current settings). Installing new firmware clears the settings, so back up first.

### External memory backup
The board has a 16 MB memory chip holding data from its original firmware. TBOSS only
ever **reads** it. **Back up external memory** copies the whole chip to a file, reading
it twice; the copy counts as verified only if both reads are identical. It takes several
minutes per read and the board does not play meanwhile. Save both downloaded files
(the `.bin` image and the `.txt` with its SHA-256).

## Saved settings

Every change is saved automatically 3 seconds after you stop changing things, and comes
back at power-up, together with presets and user scales. Installing firmware clears them:
the [browser installer](flashing.md) saves them first and puts them back afterwards
(step 2 on its page); with the SWD tool, use the web editor's settings backup first.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Only one note at a time | The instrument is mono/legato: switch the preset or the synth to poly. |
| No sound | Check the DAW's MIDI input and the track's MIDI channel against *Chord ch* / *Melody ch*. |
| A chord stays on | Hold is on (`HOLD` on screen): touch the chord's pad, or press ▶ Right on the play screen. |
| Chord pads play single notes in a pattern | The arpeggiator is on (`ARP` on screen): Settings → Arpeggiator → *Arp* → Off. |
| Chords sound in the wrong octave or very wide | The chord panel's octave or spread: tap octave **0** and press the lit spread again; Settings → Chords shows both. |
| ▲/▼ change the scale | That is the default since 0.6.1; Settings → Play → *Arrows* chooses melody octave, preset or tempo instead. Hold ◀ Left with ▲/▼ always switches presets. |
| All lights dim or flicker together at random | The USB port isn't supplying steady power. Use the board's DC power input (checked: the lights are then perfectly steady), or try another USB port, a short cable or a powered hub. |
| Pads play nothing for a second | The web editor just talked to the board; it resumes after 1 s. |
| Editor shows "404 File not found" | Start it with `python scripts/tboss_editor.py` from the TBOSS folder (not `http.server` from another folder). |
| Editor says MIDI access refused | Chrome's site settings for localhost: allow MIDI devices, then Connect again. |
| Editor: "No TheoryBoard reply" | Pick TheoryBoard MIDI in *Board*; close other programs holding the port exclusively. |
| Arpeggiator ignores the DAW tempo | Enable MIDI clock output to the TheoryBoard MIDI port in the DAW. |
| `U1 IS EMPTY` | Make a scale in the web editor first. |
| `OUT OF RANGE` | That edit or octave would leave the MIDI note range; move the octave back first. |

## TBOSS and the original TheoryBoard

How TBOSS (0.7.0) compares with the TheoryBoard's own firmware: the features it was sold
with, the features promised during the 2020 Kickstarter campaign that never arrived, and
what TBOSS adds. ✅ in TBOSS · 🟡 partly · ❌ not yet. The original's features come from its
2020 description and the printed panel; the promised list is the campaign's update, as
reported by [Synthtopia](https://www.synthtopia.com/content/2020/09/09/theoryboard-midi-controller-kickstarter-raises-1000000/)
(September 2020). Suggestions and corrections are welcome in the issues.

### The original's features

| Original TheoryBoard | TBOSS |
| --- | --- |
| Chords of the scale on the left half, melody notes in key on the right | ✅ The same, plus the chord's name on the screen ([Playing](#playing)) |
| 12 root and 12 mode buttons, change key and mode on the fly | ✅ ([Key and scale](#key-and-scale)) |
| 3 user scales (U1–U3) | ✅ 12 user scales (U1–U12), made in the [web editor](#web-editor) |
| "840 scales" (as advertised) | 🟡 86 built-in scales plus your 12, each in all 12 keys; families on M1–M7, favourites, and the editor finds scales by their notes |
| Voicing I / II / III and guitar voicing | ✅ Guitar voicing on six real strings, with a capo ([Chord panel](#chord-panel)) |
| Spread I / II / III (root down 1–3 octaves) | ✅ |
| Octave V: raise, lower or stack octaves | ✅ Tap to turn octaves on and off; lit octaves stack |
| Panel buttons light up to show what is on | ✅ (0.6.1) |
| Live note update (held notes follow changes) | ✅ Settings → Play → *Live update* |
| Strum up / down | ✅ Plus up+down, strum times in ms or note values ([Strum](#strum)) |
| 96 velocity-sensitive pads with polyphonic aftertouch | ✅ Aftertouch set per half: poly, channel, CC1/2/11/74 or pitch bend; velocity curves |
| RGB pad lights | ✅ Colour by note (rainbow) or root+5th; touched pads light white |
| USB MIDI | ✅ |
| TRS MIDI out (Type A) | ✅ Also without a computer ([TRS MIDI out](#trs-midi-out)) |
| Sustain pedal input | 🟡 Built (sustain, sostenuto, soft, portamento, hold, play/stop, note); not yet confirmed on a board |
| Firmware updates over USB with the update utility | ✅ From the browser, keeping your settings ([flashing.md](flashing.md)) |

### Promised in 2020, never delivered

| Promised for the TheoryBoard | TBOSS |
| --- | --- |
| Performance mode (save chords in a designated area to recall for songs) | ❌ Not yet; [presets](#presets) save the whole setup, including every pad's chord edits |
| Change colour palettes | 🟡 Two schemes (rainbow, root+5th); your own colours are planned |
| Bass mode (doubles notes so they can be played with rapid fire in tandem) | ❌ Not yet |
| Arpeggiator | ✅ 6 patterns (up, down, up-down, random, as played, chords), rate, octaves, gate, 8-step rhythms, swing, sync start, latch, follows MIDI clock ([Arpeggiator](#arpeggiator)) |
| Step sequencer (chords and/or single notes) | ❌ Planned ([roadmap](roadmap.md)) |
| Program changes via MIDI CC, NRPN, or pads on their own MIDI channels | ❌ Not yet (each half, and the drum layout's three grids, have their own MIDI channel) |
| Voicing, spread or octave for single chords instead of all (and saved) | 🟡 Per pad: add 7th/9th/6th, sus2/4 and strum on/off, saved; voicing, spread and octave apply to all chords |
| Strum types besides up and down | ✅ Up+down, repeat while held, cutoff, per-pad on/off, random feel (timing and velocity), an arpeggiator *Chords* pattern |
| Pitch / mod from the pads or the screen | ✅ From pad pressure: pitch bend or CC1 mod (also CC2/11/74) |
| Chords on both sides (same scale or not), mix and match single notes | ❌ Not yet (the right half: scale, chromatic, fourths or drums) |
| MIDI in over USB | ✅ MIDI clock in, [MIDI in lights](#midi-in-lights), the web editor |
| Alternative spread: the root note or the entire chord | 🟡 Root only; guitar voicing gives a wide open chord |
| Turn off octave folding | ❌ Not yet |
| Live note update via MIDI in when the DAW plays chords | 🟡 MIDI in lights show where the DAW's chords and notes are on the board; the board doesn't change its notes |
| Latch key: press a pad, latched, press again, unlatched | ✅ [Hold](#hold) (▶ Right), and the arpeggiator's latch |
| Drum kit mode | ✅ Drums layout: scale, bass and General MIDI drum grids, each on its own channel |
| Live note update on its own MIDI channel | ❌ Not yet |
| Note repeat mode (like an MPC) | 🟡 Chord pads: strum repeat while held, at the tempo; not yet for melody or drum pads |

### Only in TBOSS

| TBOSS extra | |
| --- | --- |
| Open source (GPL), reproducible builds, automatic tests | ✅ |
| Web editor: make and play your own scales, back up and restore all settings | ✅ |
| Browser installer that keeps your settings | ✅ |
| 8 presets of the whole setup, switched from the play screen | ✅ |
| Chromatic and fourths layouts for the melody half | ✅ |
| Chord names on the screen, per-pad chord edits (7th, 9th, 6th, sus) | ✅ |
| MIDI clock out with Start/Stop; follows an external clock | ✅ |
| Velocity curves (normal, light, firm, fixed) | ✅ |
| Poly aftertouch from chord pads on each of their notes | ✅ |
| ▲/▼ shortcuts: step through your favourite scales, melody octave, presets or tempo | ✅ |
| Scale families on M1–M7 (melodic minor, harmonic minor, harmonic major modes) | ✅ |
| Editor: find scales by their notes, scales one note away, bright-to-dark order | ✅ |
| Strum feel (random timing and velocity) and a capo for the guitar voicing | ✅ |
| I/O test and LED test pages | ✅ |

## Known issues

TBOSS is alpha software, tested on one board so far. Known gaps and limits:

- **Not in TBOSS yet** (some were in the original firmware): sequencer/phrase recorder,
  a second arpeggiator, per-pad sensitivity settings, user colours, MPE. See the
  [roadmap](roadmap.md).
- The arpeggiator has no on/off shortcut button yet: use Settings → Arpeggiator → *Arp*.
- The chord edits that move a chord's 3rd or 5th down an octave (0.5 and earlier, F1/F2)
  have no button any more; edits saved with them still play, and *Reset chord* clears
  them. *Spread* moves the root down for every chord.
- **New in 0.7.0, please check:** the guitar voicing (does it sound like a guitar?), the
  capo, strum feel, the families on M1–M7 and favourites on ▲/▼.
- The board appears with a test USB ID (`cafe:4069`) until TBOSS has its own.
- Windows may keep showing an older name for the MIDI port; it is the same device.
- Installing clears saved settings unless the browser installer's step 2 is used.
- The external memory chip is only read, never used; the original firmware's data there
  stays untouched.
- On Linux the browser installer needs a one-time permission rule (see
  [Installing](flashing.md#with-a-browser-recommended)).

**Confirmed on a board (2026-10-17):** TRS MIDI out, **Type A**; a MIDI interface reads
its messages correctly. Settings → About → *I/O test* sends middle C every second, handy
with a MIDI monitor. **Not yet tried on a board:** the sustain pedal input. Its pin comes from the board's
previous firmware; the I/O test shows what the board sees, so a report with that screen
tells us quickly what to fix.

**Least tested so far** (reports especially welcome): installing from macOS, Linux and
ChromeOS (Windows is confirmed); the arpeggiator following MIDI clock from a DAW; the web
editor's external memory backup on a real board (how long it takes); boards that report a
hardware version other than 4 (the installer stops for those and asks for a report).

Please report what you find: a [test report](https://github.com/armandhammer/TBOSS/issues/new?template=test_report.yml)
after trying TBOSS (even "everything worked"), or a [bug report](https://github.com/armandhammer/TBOSS/issues/new/choose)
for a single problem, with the TBOSS version (Settings → About) and, for install problems,
the installer's *Details*.

## Feature check

A quick pass through everything, for example after installing a new version. Expected
results are in brackets.

1. Power up with USB → `TBOSS` and the version, then the play screen; DAW sees
   TheoryBoard MIDI. [pads lit in rainbow colours; a touched pad turns white]
2. Melody pads, several at once, soft and hard. [polyphonic, louder when harder]
3. Root C, mode M1, bottom row of chord pads left to right. [C Dm Em F G Am Bdim on screen]
4. Press D, then M2. [`D DORIAN`, bottom-left chord `Dm`]
5. Bottom-left chord; Settings → Chords → *Add 7th* On, then *Reset chord*. [`Dm7`, paler
   pad; back to `Dm`]
6. Chord panel, top row: voicing I, II, III, guitar, each followed by a chord pad.
   [`VOICING I`…`GUITAR VOICING`; the chord moves up, guitar sounds wide]; press guitar again.
7. Spread I, II, III. [`SPREAD I`…, a deep root note]; press III again.
8. Octave V (0 lit): tap +1, then 0, then +1. [`OCTAVES 0 +1`: the chord in two octaves,
   0 and +1 lit; `OCTAVES +1`: one octave up, only +1 lit; `OCTAVES 0`: back, only 0 lit]
9. Play screen ▲ Up, ▼ Down twice, ▲ Up. [`SCALE PHRYGIAN`, `SCALE DORIAN`,
   `SCALE MINOR`, `SCALE DORIAN`]
   Settings → Play → *Family* Mel minor, press M4. [`LYDIAN DOM`]; *Family* back to Major.
   Guitar voicing on, bottom-left chord in C. [C E G C E, like an open C on a guitar];
   Settings → Strum → *Capo* fret 3 [the same chord, a different shape]; Capo Off,
   guitar voicing off.
10. ▶ Right (hold), play a chord, lift, touch it again. [keeps sounding; then silence];
    ▶ Right again.
11. ● Centre, ▼ to *Strum*, ● Centre; Strum Up, Strum time 80 ms, play a chord. [notes
    low to high]; *Repeat* 1/8 and hold the pad [strums again every eighth]; *Cutoff*
    1/16 [short stabs]; Repeat and Cutoff Off.
12. ◀ Left, ◀ Left. [page list, then the play screen; a third ◀ does nothing]
13. Strum Off; Settings → Arpeggiator → *Arp* Up; hold a chord. [single notes in a
    pattern; `ARP UP 1/16 120`]; *Rhythm* `X.X.X.X.` [every other step rests];
    *Swing* 50 % [long-short]; *Latch* On, lift the hand [keeps playing; touch the pad to
    stop]; *Arp* Chords [whole chords each step].
14. DAW sends MIDI clock, press Play. [`EXT` on screen, pattern follows the DAW]
15. *Arp* Off. Hold ◀ Left and press ● Centre. [straight back to the play screen]
16. Settings → Layout → *Layout* Chromatic, Fourths, Drums. [pad colours change; Drums:
    right grid amber, kick on its bottom-left pad on channel 10]; back to Scale.
17. Settings → MIDI: Chord ch 2, Melody ch 3, MIDI monitor. [chords on 2, melody on 3]
18. Settings → Play → *Chord AT* Poly AT, hold a chord pad and press harder. [poly
    aftertouch on each of its 3 notes]; *Melody AT* Pitch bend [bends up with pressure,
    back on release].
19. *Velocity* Light, then Firm. [the same touch is louder, then softer]
20. Settings → MIDI → *Clock out* On. [MIDI monitor: clock at 120 BPM]; *Transport* ●
    [Start, then Stop on the next press]
21. Save preset 1; change root; on the play screen hold ◀ Left and press ▼ Down.
    [`PRESET 1 LOADED`, setup restored]
22. Send a C major chord from the DAW to the board. [matching pads light green]
23. Web editor: Connect [12 slot tabs], U5, *Start from* Blues, name `BLUES`, Save,
    *Use it on the TB*. [`BLUES` on screen; while editing, all pads showed the scale and
    the screen said `WEB EDITOR` and `U5`]
24. Web editor: Download settings backup; Restore from file. [`Restored n of n records`]
25. Settings → About. [`0.6.0`, the same version as the editor's header]
26. TRS out to a synth (Type A adapter), play. [the synth plays]
27. Sustain pedal plugged in before power-up; Settings → About → *I/O test*, press it.
    [`PEDAL PD13 ... DOWN` while pressed]; then Settings → Pedal → *Mode* Sustain, play
    with it. [`SUS` on screen, notes sustain]; *Mode* Note [a kick on channel 10].
28. Power off and on. [settings, presets, chord panel and U5 come back]
