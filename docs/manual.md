# TBOSS user manual

TBOSS is open-source replacement firmware for the TheoryBoard THY333. This manual covers
every feature on the board and in the web editor. For installing the firmware see
[flashing.md](flashing.md). TBOSS is **alpha** software: see [Known issues](#known-issues)
and please report anything that doesn't work as described here.

**How each feature is explained:** what it does, then **Try it** (steps you can follow on
the board, with what you should see or hear), **Why use it** and, where it matters, **When
not to**. Features that go beyond the original TheoryBoard are marked **Optional**: they
are off until you turn them on.

Contents: [Coming from the original TheoryBoard](#coming-from-the-original-theoryboard) ·
[The board](#the-board) · [Getting started](#getting-started) · [Playing](#playing) ·
[Key and scale](#key-and-scale) · [Chord panel](#chord-panel) · [Chord edits](#chord-edits) ·
[Voice leading and fold](#voice-leading-and-fold) · [Octaves](#octaves) ·
[Right-half layouts](#right-half-layouts) · [Hold](#hold) · [Tempo](#tempo) · [Strum](#strum) ·
[Note repeat](#note-repeat) · [Arpeggiator](#arpeggiator) · [Progressions](#progressions) ·
[MIDI channels, pressure and velocity](#midi-channels-pressure-and-velocity) ·
[Restrike](#restrike) · [MIDI clock and transport](#midi-clock-and-transport) ·
[TRS MIDI out](#trs-midi-out) · [Sustain pedal](#sustain-pedal) · [Presets](#presets) ·
[MIDI in lights](#midi-in-lights) · [Pad sensitivity](#pad-sensitivity) · [Settings menu](#settings-menu) ·
[Screen and lights](#screen-and-lights) · [Web editor](#web-editor) ·
[Saved settings](#saved-settings) · [Troubleshooting](#troubleshooting) ·
[TBOSS and the original TheoryBoard](#tboss-and-the-original-theoryboard) ·
[Known issues](#known-issues) · [Feature check](#feature-check)

---

## Coming from the original TheoryBoard

TBOSS starts out playing like a stock TheoryBoard: chords of the key on the left, the
scale on the right, the root and mode buttons along the top, and the printed chord panel
in the centre doing what its icons say. If you know the original, you can play straight
away.

What is different from the first moment, because it makes the board easier to use:
- The **screen** names the chord you just played (`Dm7`, `C/E`) and what the last button
  did (`VOICING II`, `ROOT D`), so the board explains itself as you go.
- **Settings** are in a menu under the five buttons below the screen: press **● Centre**
  to open it. See [Settings menu](#settings-menu).
- **▲ / ▼** on the play screen step to the next or previous scale, and **▶ Right** turns
  [hold](#hold) on and off.
- Pads are lit in **rainbow** colours by note and turn white under your finger.
- Pads play from a **lighter touch**, and how light is up to you: Settings → Pads
  ([Pad sensitivity](#pad-sensitivity)).

Everything else (strum feel, voice leading, live notes, note repeat, the drum layout and
so on) is **optional**: nothing changes until you switch it on in the menu or the web
editor. If the board ever behaves unexpectedly, [Troubleshooting](#troubleshooting) lists
the usual causes.

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

- **Pads (96):** pressure-sensitive. The left half plays chords, the right half single notes.
- **Root buttons (12):** the key, C to B from left to right.
- **Mode buttons (12):** `5` major pentatonic, `12` chromatic, `M1`..`M7` the seven
  modes of the major scale, `U1`..`U3` your own scales (U4–U12 are in the menu).
- **Navigation buttons (5),** the row just below the screen, left to right:
  **◀ Left**, **▲ Up**, **● Centre**, **▼ Down**, **▶ Right**.
- **Chord panel (14 centre buttons):** voicing (I, II, III, guitar), spread (I, II, III)
  and the octave V (−3 to +3).
- **Jacks (back):** TRS MIDI out (Type A) and a sustain pedal input.

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
   they play one note at a time, so a second pad seems to cut off the first. That is the
   instrument, not the board.

**Try it:** play a few right-hand pads. You hear notes, and each pad lights white while
you touch it, brighter as you press harder.

## Playing

### Melody pads (right half)
- Each column is one step of the current scale, starting from the root on the left. With
  a 7-note scale, columns 1–7 are one octave and columns 8–12 continue upward.
- The **bottom row** starts on the root (C3 by default); each row up is one octave higher.
- **Velocity** comes from how hard you strike the pad.

Every pad is in the key, so there are no wrong notes: you can improvise freely.

**Try it:** with root C and mode M1, play the bottom row of the right half from left to
right: C D E F G A B C D E F G. The row above plays the same an octave higher.

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

- The chord's name appears large on the screen, for example `Dm` or `G7`. A name like
  `C/E` means a C chord with E as its lowest note.
- In pentatonic and chromatic scales, chords are built the same way (every other note of
  the scale), so they can be unusual; the screen names them, e.g. `Am/C`.

**Try it:** with root C and mode M1, play the bottom row from left to right. The screen
reads C, Dm, Em, F, G, Am, Bdim. Now play the C column from bottom to top: C, C/E, C/G,
C, each a little higher.

**Why the rows:** playing a progression on one row makes every chord jump to its own
root. Mixing rows lets you keep chords close together, e.g. C (bottom row), then F
(3rd row, C F A), then G (2nd row, B D G): this sounds smooth, like a pianist's
hands. [Voice leading](#voice-leading-and-fold) can do this for you.

## Key and scale

### Root
Press a root button: the whole board moves to the new key, both halves. Notes already
sounding keep playing until you lift your finger.

**Try it:** play the bottom-left chord (C), press **D**, play the same pad: `D`.

### Mode buttons
The mode buttons choose the scale: `5` major pentatonic, `12` chromatic, `M1`–`M7` the
modes, `U1`–`U3` your own scales.

**Try it:** press **D**, then **M2** (Dorian). The screen shows `D` and `DORIAN`, and the
bottom-left chord is `Dm`.

**Why:** a mode changes the mood without changing the key's root: M1 (major) bright,
M6 (minor) sad, M2 (Dorian) minor but hopeful, M5 (Mixolydian) bluesy major.

### Families *(optional)*
The **M1–M7** buttons can play the seven modes of other scale families, chosen in
Settings → Play → *Family* (default **Major**, the classic M1–M7):

| Family | M1 | M2 | M3 | M4 | M5 | M6 | M7 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Major (default) | Major | Dorian | Phrygian | Lydian | Mixolydian | Minor | Locrian |
| Melodic minor | Melodic minor | Dorian ♭2 | Lydian augmented | Lydian dominant | Mixolydian ♭6 | Locrian ♮2 | Altered |
| Harmonic minor | Harmonic minor | Locrian ♮6 | Ionian ♯5 | Dorian ♯4 | Phrygian dominant | Lydian ♯2 | Ultralocrian |
| Harmonic major | Harmonic major | Dorian ♭5 | Phrygian ♭4 | Lydian ♭3 | Mixolydian ♭2 | Lydian augmented ♯2 | Locrian ♭♭7 |

**Try it:** Settings → Play → *Family* → **Harm minor**, back to the play screen, press
**M5**: `PHRYGIAN DOM`, the "Spanish" or "Middle Eastern" sound. Set *Family* back to
Major afterwards.

**Why use it:** 28 of the most-used scales are one button away, grouped so that related
sounds sit together. Jazz players get the melodic minor modes (Lydian dominant, Altered)
on buttons.
**When not to:** if you only need the classic modes, leave it on Major; the printed
M1–M7 then mean what they always did.

### The scale library
86 built-in scales: the four families above, pentatonic scales (major, minor, Egyptian,
Yo, Hirajoshi, In sen, Iwato, Kumoi and more), six-note scales (blues, whole tone,
augmented, Prometheus…), other seven-note scales (Hungarian, Neapolitan, Persian, double
harmonic, Indian thaats…) and scales of eight notes or more (bebop, diminished,
Messiaen's modes, chromatic).

**Try it:** Settings → Play → *Scale*, ● Centre, ▲/▼ through the list, ● Centre on
**BLUES**. The chords and the right half now follow the blues scale.

The web editor's [scale library](#scale-library) is easier for browsing: it groups the
scales, plays them, and finds scales by the notes you play.

### Stepping through scales with ▲ / ▼
On the play screen, **▲ Up / ▼ Down** step to the next or previous scale (`SCALE DORIAN`).

**Try it:** on the play screen press ▲ Up: `SCALE MINOR`; ▼ Down: back.

**Why:** compare scales quickly without opening the menu.
If you'd rather use the arrows for something else (melody octave, presets or tempo),
change Settings → Play → *Arrows*.

### Favourites *(optional)*
Up to 128 scales you pick in the web editor, in your order. Once you have favourites,
▲ / ▼ step through them instead of through every scale (`FAV 3/12 BLUES`).

**Try it:** in the web editor's scale library, add Dorian, Blues and Hirajoshi to
favourites and click **Save favourites to the TB**. On the board, ▲ Up shows
`FAV 1/3 DORIAN`, then `FAV 2/3 BLUES`…

**Why use it:** a set list of scales for a song or a practice session, reachable with
one button.

### User scales (U1–U12) *(optional)*
Your own scales, made in the [web editor](#custom-scales). U1–U3 have buttons; all twelve
are in Settings → Play → *Scale*. Until a slot has a scale, pressing it shows `U1 IS EMPTY`.

Press only one root or mode button at a time; two at once shows `ONE BUTTON AT A TIME`.

## Chord panel

The 14 buttons in the centre do what the panel prints next to them. They shape **every
chord pad** at once and work on the play screen and in the settings alike. A held chord
keeps the notes it started with; the next touch plays the new shape. The bottom line of
the screen says what changed (`VOICING II`, `SPREAD I`, `OCTAVES -1 0 +1`). A panel button
is fully lit while its choice is on and dim otherwise, so the panel always shows what is
in use.

### Voicing I / II / III
Move every chord up one, two or three inversions: with **I** the bottom row of C plays
E G C instead of C E G, and the other rows move up with it. Press the lit button again to
go back. One voicing at a time.

**Try it:** play the bottom-left chord (C E G), press **I**, play it again: E G C,
brighter and higher. Press **I** again to turn it off.

**Why use it:** lift all chords into a brighter register without changing the octave
of the melody half, or to sit above a bass line.

### Guitar voicing
Plays each chord the way a guitar does: six strings in standard tuning (E A D G B E),
each string playing the lowest chord note under the hand, the strings below the chord's
root left out. The bottom row is the open position (C is x32010, G 320003, D xx0232,
Am x02210); the rows above move the hand 3, 5 and 7 frets up the neck.

**Try it:** press the guitar button, play the bottom-left chord in C: C E G C E, the
open C chord. With [Strum](#strum) Up at 20 ms it sounds strummed.

**Why use it:** wide, open chords that sit well on a guitar or piano sound; strumming
them sounds like a real guitar.

**Capo** (Settings → Strum → *Capo*, fret 1–11, default Off): the chords stay in the key
but take the shapes and brighter sound of a higher position, as with a real capo.
**Try it:** guitar voicing on, *Capo* fret 3, play C: the same chord, a different,
brighter shape.

### Spread I / II / III
Drops each chord's root one, two or three octaves: a deep bass note under the chord.
Press it again to turn it off.

**Try it:** play the bottom-left chord, press **Spread I**: a low C joins under the chord.

**Why use it:** a full, piano-like left hand from one pad. **When not to:** when a bass
player or a bass track already covers the low end; the extra root can muddy it.

### Octave V (−3 to +3)
**Each tap turns that octave on or off.** Every lit octave plays, so lighting several
stacks them. 0 is normal.
- To **move** the chords: tap the new octave on, then the old one off (for +1 alone from
  0: tap +1, then 0).
- Turning off the last lit octave goes back to 0.
- A stacked chord plays at most 16 notes; notes beyond the MIDI range are left out.

**Try it:** with 0 lit, tap **+1**: the chord plays in two octaves (`OCTAVES 0 +1`). Tap
**0**: only +1 is left, the chord is an octave up. Tap **0**, then **+1**: back to normal.

**Why use it:** stacked octaves make big, organ-like chords; moving the octave keeps
chords out of the melody's way.

## Chord edits

Chord edits change **one chord pad: the last one you played**. They stay on that pad
(and are saved) until you reset them. Edited chord pads light paler. They are in
Settings → **Chords**:

| Item | Edit | Default |
| --- | --- | --- |
| Add 7th / Add 9th / Add 6th | Off / On: add that note (from the scale) | Off |
| Sus | Off / SUS2 / SUS4: the 3rd becomes the 2nd or the 4th | Off |
| Strum pad | On / Off: this pad strums, or plays all at once (see [Strum](#strum)) | On |
| Sound | On / Off: Off makes the pad silent; it still picks the [live notes](#live-notes-optional) | On |
| Reset chord | ● clears this pad's edits (`CHORD EDITS RESET`) | |
| Reset all | ● clears the edits of every chord pad | |

*Voicing*, *Spread*, *Voice lead* and *Fold* on the same page apply to every chord.

**Try it:** play the bottom-left chord in C major (`C`), open Settings → Chords →
*Add 7th*, set On: the name becomes `Cmaj7` and the pad turns paler. Play the G column's
bottom pad, *Add 7th* On: `G7`. *Reset chord* takes the G back to `G`.

**Why use it:** richer chords exactly where a song needs them, e.g. a G7 that pulls back
to C, or a sus4 that resolves. Edits are saved with [presets](#presets), so each song can
have its own set.

If you have not played a chord pad yet, an edit shows `PLAY A CHORD 1ST`. An edit that
would take a note outside the MIDI range shows `OUT OF RANGE` and is not made.

## Voice leading and fold

Two ways to keep chords close together instead of jumping up and down the keyboard. Both
are in Settings → Chords and both are **Optional** (Off by default).

### Voice lead *(optional)*
Each chord you play picks the inversion that moves the least from the chord before, so
every voice moves by a step or stays put, the way a pianist or a string section plays a
progression. It stays near where you play: a chord pad in a higher row still plays
higher (within two inversions of its own position), and Voicing I–III still lift
everything.

Voice leading always starts from **the last chord you played** (the board remembers it
after you let go), so there's nothing to hold down.

If no inversion near a pad's own position fits inside the MIDI note range (possible only
with a very high chord octave plus added notes and Voicing III), that pad plays its row's
own chord instead, so every pad always sounds. *(0.9.1; before, such a pad could stop the
board playing until Voice lead was turned off.)*

**Try it:**
1. Root **C**, mode **M1**. Tap the bottom-left chord pad and let go: `C` (C E G). This is
   the chord the next one will lead from.
2. Settings → Chords → *Voice lead* → On, then ◀ Left back to the play screen.
3. Tap the bottom-row **F** pad (4th column): the screen shows `F/C`. It plays **C F A**
   instead of F A C: C stays, E moves up to F, G up to A.
4. Tap the bottom-row **G** pad (5th column): `G/D`, **D G B**: each note moves a step.
5. Tap **C** again: `C/E`, **E G C**.
6. Turn *Voice lead* Off and tap F and G again: plain `F` and `G`, each jumping to its own
   root.

**Why use it:** smooth, professional-sounding progressions with no theory needed;
especially good for pads, strings, choirs and electric piano. Combine it with **Spread I**
to keep a bass note on each chord's root underneath the smooth chords.
**When not to:** when you want the bass note to follow the chord's root by itself (rock,
pop power chords), when you choose inversions yourself with the rows, or with the guitar
voicing (which has its own shapes, so voice leading leaves it alone).

### Fold *(optional)*
Every chord's notes are moved into one fixed octave, G♯ to G, whatever the key: the
lowest note of a chord can then be its 3rd or 5th instead of its root. This is how the
original TheoryBoard's chords sounded, as its owners describe it (the campaign promised a
way to turn it off; in TBOSS it is off unless you turn it on). The rows still work:
each row up raises the chord's lowest note an octave.

**Try it:** root F, Settings → Play → *Scale* → **HARM MINOR**, Settings → Chords →
*Fold* → On. The bottom-left chord (Fm) plays **A♭ C F**: it starts on G♯/A♭. The 5th
column (C) plays **C E G**. Set *Fold* Off: Fm plays F A♭ C again.

**Why use it:** every chord sits in the same narrow range, so changing chords never jumps
far; it also matches the original board's sound if you're used to it.
**When not to:** if you want each chord to start on its root (as the screen's chord name
suggests), or want the higher columns: with Fold on, columns 8–12 repeat columns 1–5.
Voice lead is usually the better way to smooth a progression.

## Octaves

- **Melody half:** Settings → Play → *Melody oct*, −2 to +2 octaves. If you change it
  often, set Settings → Play → *Arrows* to *Melody octave*: then **▲ Up / ▼ Down** on the
  play screen move it (`MELODY OCT C4`).
- **Chord half:** the [chord panel](#chord-panel)'s octave buttons.

The screen line `OCT C2 C3` shows the lowest root of each half (chords, melody).

**Try it:** Settings → Play → *Melody oct* → +1: the right half's bottom row now starts on
C4, and the screen shows `OCT C2 C4`.

## Right-half layouts

Settings → Layout → *Layout* changes what the right-hand pads play. **Scale** is the
original layout and the default; the others are **Optional**.

| Layout | Right half |
| --- | --- |
| Scale (default) | each column one scale step, each row up an octave |
| Chromatic | each column one semitone from the root, each row up an octave |
| Fourths | each column one semitone, each row up a fourth (like a bass or guitar neck) |
| Drums | three 4×4 grids: scale notes, a bass line and General MIDI drums |
| Live notes | the scale, plus the notes of the chord you just played |

### Chromatic and fourths *(optional)*
**Chromatic** puts all 12 notes in each row, out-of-scale notes dimmed. **Fourths** lays
the notes out like a bass guitar: each row a fourth above the one below, so a scale shape
is the same in every key.

**Try it:** *Layout* → Chromatic: the dim pads are the notes outside the scale. Play a
blues lick that bends through them.
**Why use it:** passing notes and blue notes the Scale layout can't reach; Fourths if you
know bass or guitar fingerings. **When not to:** if you want every pad to be safe; with
Scale, no pad is ever out of key.

### Drums *(optional)*
The right half becomes three grids of 4 columns × 4 rows, each played from its
bottom-left pad, left to right, then up a row:
- **Left grid:** the scale from the root, on the *Melody ch*.
- **Middle grid:** the same notes an octave lower, on the *Bass ch* (default 2): a bass line.
- **Right grid:** 16 drum notes, General MIDI 36 to 51 (kick on the bottom-left pad, then
  snare, claps, hi-hats, toms, cymbals), on the *Drum ch* (default 10, the General MIDI
  drum channel). These pads light amber.

**Try it:** *Layout* → Drums, a General MIDI drum kit on channel 10 in your DAW: the
right grid's bottom-left pad plays a kick, the next one a side stick, then a snare; the
third pad of the row above is the closed hi-hat.
**Why use it:** a whole band from one board: chords on the left, bass and drums on the
right, each on its own channel and instrument.

### Live notes *(optional)*
The original TheoryBoard's *Live Note Update*: while you play chords on the left, part of
the right half offers exactly that chord's notes, so a melody or solo over the chord
can't clash.

- **Columns 1–7:** the scale, each row an octave up (like the Scale layout).
- **Columns 8–12:** the notes of the **live chord**, from its root upwards: a C chord
  gives C E G C E along a row, a Cmaj7 C E G B C. Each row is an octave above the one
  below. The chord's root lights brightest.
- The live chord is the **last chord pad you touched**, and stays when you let go.
  Before you play any chord it is the key's first chord.

**Try it:** root C, M1, *Layout* → Live notes. Play the G chord (5th column, bottom row)
and lift your hand. Columns 8–12 of the right half's bottom row now play G B D G B
(blue, pink, yellow in the rainbow colours). Play Am: they become A C E A C.
**Why use it:** solo or play arpeggios by hand over any chord without thinking about
which notes fit; great for learning which notes make a chord.
**When not to:** if you want the full 12 columns of the scale, use the Scale layout.

**Live ch** (Settings → Layout → *Live ch*, default *Melody*): give columns 8–12 their own
MIDI channel (1–16), so they can play a different instrument or be recorded on their own
track. **Try it:** *Live ch* 2, a second instrument on channel 2: columns 1–7 play the
melody instrument, columns 8–12 the second one. (Pressure as poly aftertouch follows them
there; channel pressure and the pedal's sustain stay on the Melody ch.)

**Live in** (Settings → Layout → *Live in*, default Off): chords your computer sends **to**
the board pick the live chord, on one MIDI channel (1–16) or All. The lowest note held is
the chord's root; two or more different notes make a chord. A chord stays until the next
one arrives (letting it go note by note never changes it); touching a chord pad takes
over again. The screen names the incoming chord.
**Try it:** in your DAW, send a chord track to the TheoryBoard MIDI port, *Live in* All,
play the song: columns 8–12 follow the song's chords while you solo over them.

**Silent chords:** Settings → Chords → *Sound* Off on a chord pad: the pad sets the live
chord but makes no sound. **Why:** pick the harmony for the live notes while another
instrument (or your DAW) plays the chords.

A held live pad keeps its note when the chord changes; with [Restrike](#restrike) on it
moves to the new chord's note at once.

## Hold

With hold on, a chord keeps sounding after you lift your hand, so the left hand is free.

- Turn it on or off with **▶ Right** while playing, or in Settings → Play → *Hold*. The
  screen shows `HOLD` on the octave line.
- Touching **another chord pad** replaces the held chord; touching the **held chord's pad
  again** stops it. Turning hold off stops it too.
- Melody pads are never held.

**Try it:** press ▶ Right (`HOLD ON`), play a chord and lift your hand: it keeps sounding.
Play another chord: it replaces the first. Touch that pad again: silence. ▶ Right again
turns hold off.
**Why use it:** play a long chord and use both hands on the melody, or hold chords
without a sustain pedal.

## Tempo

The board has its own tempo: Settings → Play → *Tempo*, 40 to 240 BPM in steps of 5
(default **120**). Everything timed in note values follows it: the
[arpeggiator](#arpeggiator), [note repeat](#note-repeat), strum times and strum repeat set
in note values, and MIDI clock out. Only plain chords and notes ignore it.

When a **MIDI clock comes in** (your DAW sending clock to TheoryBoard MIDI), the board
follows that instead and the screen shows `EXT` where the tempo was. A quarter of a
second after the clock stops, the board goes back to its own *Tempo*.

**Try it:** Settings → Arpeggiator → *Arp* Up, hold a chord: sixteenth notes at 120 BPM.
Settings → Play → *Tempo* → 80: the same pattern, slower. *Arp* Off afterwards.
**Quick changes:** set Settings → Play → *Arrows* to **Tempo**; then ▲ / ▼ on the play
screen make it 5 BPM faster or slower (`TEMPO 125 BPM`), handy for matching a song by ear.
(The arrows then no longer step through scales.)

## Strum

*(Optional; Off by default.)* Instead of all at once, a chord's notes start one after
another, like a strummed guitar or a harp.

- Settings → Strum → *Strum*: **Off**, **Up** (low to high), **Down** (high to low),
  **Up+Down** (alternates direction on each strike, like strumming up and down).
- *Strum time*: 10 to 120 ms between notes, or a note value at the tempo (1/96, 1/48,
  1/32, 1/24, 1/16), which follows MIDI clock too.
- If you let go before all notes have started, the remaining notes are skipped.

**Try it:** *Strum* Up, *Strum time* 80 ms, play a chord: you clearly hear the notes go
low to high. Set 20 ms: a quick, natural guitar strum.
**Why use it:** guitar and harp sounds come alive; slow strums make a chord into a
gentle arpeggio. **When not to:** tight, punchy stabs (synth brass, house chords) want
all notes at once.

**Repeat** (*Repeat*: Off, 1/4, 1/8, 1/16, 1/32): while you hold a chord pad it strums
again at that rate, louder or softer as you press harder or lighter.
**Try it:** *Repeat* 1/8, hold a chord: it strums every eighth note at the tempo.
**Why:** rhythm guitar or a pulsing synth chord from one held pad.

**Cutoff** (*Cutoff*: Off, 1/32 to 1/1): a strummed chord ends after that note value even
while held (with *Repeat*, until the next strike).
**Try it:** *Repeat* 1/8 and *Cutoff* 1/16: short, chopped chords.

**Strum pad** (Settings → Chords → *Strum pad*) turns strum off for one pad (the last one
played), so some chords strum and others hit all at once.

**Feel** (*Feel time* up to 5, 10, 20 or 40 ms late; *Feel vel* up to ±5, 10, 20 or 30
velocity): each strummed note varies a little at random, for a looser, human strum. The
notes always start in strum order.
**Try it:** *Feel time* 0–20 ms, *Feel vel* ±20, strike the same chord several times:
each strike sounds slightly different. **When not to:** tight electronic music, where
every hit should be identical.

Strum does not apply while the arpeggiator is on (except its *Chords* pattern).

## Note repeat

*(Optional; Off by default.)* Like the note repeat on an MPC or a Launchpad: while you
hold a right-half pad, it strikes again and again at a note value of the
[tempo](#tempo). Each repeat sounds for half the step and rests for the other half, so
you hear separate, even hits ("da-da-da-da"). Press harder or lighter while holding to
make the repeats louder or softer.

Settings → Play → *Note repeat*: **Off**, 1/4, 1/8, 1/16, 1/32. It follows Settings →
Play → *Tempo* or an incoming MIDI clock. While it is on, the play screen's bottom line
shows `REPEAT 1/16 120`. Chord pads are not affected (they have the strum *Repeat*).

**Try it:** *Note repeat* 1/16, hold a melody pad: a steady stream of sixteenth notes at
120 BPM. Lean in: they get louder. Then *Layout* → Drums and hold the closed hi-hat
(right grid, second row from the bottom, third pad) for a hi-hat roll; hold two drum pads
at once for a fill.
**Why use it:** hi-hat rolls, trap hats, snare build-ups, repeated synth notes, or a
rhythmic pulse you shape with pressure. **When not to:** normal melody playing: with it
on, every held note repeats. Turn it back Off when you're done.

## Arpeggiator

*(Optional; Off by default.)* While the arpeggiator is on, chord pads don't sound as
chords: their notes are played one at a time in a pattern. Melody pads still play
normally on top.

Turn it on by choosing a pattern in Settings → Arpeggiator → *Arp*, and off by choosing
*Off* there.

| Pattern | Order |
| --- | --- |
| Up | lowest to highest, repeat |
| Down | highest to lowest |
| Up-down | up then down, without repeating the top and bottom notes |
| Random | any note of the chord each step |
| As played | pads in the order you touched them, each chord low to high |
| Chords | the whole chord on every step (strummed if *Strum* is on) |

| Setting | What it does | Default |
| --- | --- | --- |
| Arp rate | note length: 1/4, 1/8, 1/8T (triplet), 1/16, 1/16T, 1/32 | 1/16 |
| Arp octaves | 1, 2 or 3: the pattern repeats in higher octaves | 1 |
| Arp gate | how long each note sounds: 25, 50, 75 or 100 % of a step | 50 % |
| Rhythm | eight 8-step patterns shown as `X` (play) and `.` (rest), e.g. `X.XX.XX.`; the pattern keeps moving through the chord on rests | `XXXXXXXX` |
| Swing | 0 to 50 %: the first note of each pair longer, the second shorter | 0 % |
| Sync start | On: a new arpeggio waits for the next step of the clock, so it starts in time | Off |
| Latch | On: the arpeggio keeps running after you lift your hand; touch another chord to change it, or the same pad to stop | Off |

- The speed comes from Settings → Play → [*Tempo*](#tempo) (default 120 BPM) or an
  incoming MIDI clock.
- Holding more than one chord pad combines their notes.
- Velocity follows how hard you struck the last chord pad.
- **Sync to your DAW:** if your DAW sends **MIDI clock** to TheoryBoard MIDI, the
  arpeggiator follows it and the screen shows `EXT` instead of the tempo. Pressing Play
  in the DAW restarts the pattern on the beat. When the clock stops, the board goes back
  to its own tempo after a quarter of a second. (In most DAWs: MIDI settings → enable
  "send MIDI clock" or "sync" for the TheoryBoard MIDI port.)

**Try it:** *Arp* Up, hold the bottom-left chord: C E G C E G… in sixteenths at 120 BPM.
*Arp rate* 1/8: half as fast. *Arp octaves* 2: the pattern climbs an octave further.
*Rhythm* `X.XX.XX.`: a syncopated pattern. *Latch* On, lift your hand: it keeps going.
**Why use it:** instant synth arpeggios, plucked patterns and rhythmic textures from
simple chords. **When not to:** when you want to hear the chords themselves; *Arp* Off.

## Progressions

*(0.11.0)* Record the chords of a song by tapping chord pads, then let the board play them
in time while your hands are free for the melody. Eight slots (P1–P8) keep eight songs.

**Walk-throughs:** [record and play Am F G](#try-progression) ·
[a song with borrowed chords, Am D Dm Am C G E7 Am](#try-borrowed) ·
[type a song in the web editor](#try-typed). Quick start: Settings → Play → *Arrows* →
**Sequencer**, then on the play screen ▼ records (tap chord pads, ▼ again saves) and
▲ plays or stops.

A progression remembers **pads, not notes**: which chord pad (and row), its edits (7th,
sus…) and how hard you tapped it. So the root buttons **transpose** a playing progression
to any key at once, and the mode buttons **re-colour** it (a progression recorded in A
minor, played with M1 in A major, turns Am F G into A F♯m G♯dim: whatever those pads play
in the new mode). Everything that shapes a held chord applies: the chord panel's voicing,
spread and octaves, strum (not its *Repeat* or *Cutoff*), voice leading and fold; with the
arpeggiator on, the progression feeds the arpeggiator instead of sounding as chords.

**How loud:** each chord plays as hard as you tapped it while recording (chords typed in the
web editor play at 100). Set *Chord vel* to **Fixed 100** and every progression chord plays at
100 instead, like the chords you play; *Light* and *Firm* don't change a progression, since
the curve was already applied when you tapped. ([Velocity](#velocity))

**Borrowed chords:** a chord you record while another mode is selected keeps that mode.
That is how a song with chords from outside one scale goes in: in A minor, `D` (major) is
not on the A minor pads, but it is on the A Dorian pads. Record it with M2 (Dorian) pressed
and it stays D major whatever mode is playing later.

### Record
1. Settings → Sequence → *Progression*: pick a slot, 1–8 (`EMPTY` shows a free one).
2. *Unit*: how long one tap lasts: **1 beat**, **½ bar**, **1 bar** (default) or **2 bars**
   (4/4, at the [tempo](#tempo) or the DAW's clock).
3. *Record* → ● Centre. The play screen shows `REC P1 0/64`. Recording replaces what the
   slot held.
4. Tap chord pads, one tap per unit. Tapping the **same chord again lengthens it** (two
   taps of Am = Am for two units); a different chord starts the next step. Chords sound as
   you tap them. The screen counts the units and names the last chord (`REC P1 5/64 Am`).
   - **◀ Left** takes back the last unit (`REC 4/64 UNDO`).
   - Change mode between taps: a chord tapped in another mode is borrowed. (Don't change
     the root while recording: every step plays from the root of the moment.) Chord
     edits (Add 7th…) are recorded with the chord: set them before you start, because
     ● Centre finishes the recording rather than opening the settings.
5. **● Centre** finishes and saves (`PROGRESSION 1 SAVED`).

Up to 64 units and 38 chord changes per slot; one chord lasts up to 32 units. When a slot
is full the screen says `PROGRESSION FULL`. Finishing with nothing recorded (▼ pressed
twice by mistake, or every unit taken back) keeps what the slot had: the screen says
`NOTHING RECORDED`. To empty a slot, use *Clear*.

### Play
- **▲ Up** with Settings → Play → *Arrows* set to **Sequencer** (then ▲ plays/stops and
  ▼ Down records/finishes, without opening the settings), the pedal in *Play/Stop* mode,
  or Settings → MIDI → *Transport*. The board sends Start and Stop to the computer as
  usual.
- **From a DAW:** when the DAW sends MIDI clock and Start to the board, the progression
  starts with the DAW and follows its tempo and its position in the song: Stop stops it,
  Continue carries on where it stopped, moving the DAW's playhead moves the progression
  (if the DAW sends Song Position Pointer, as most do), and Start goes back to the first
  chord. The progression's first chord falls on the song's bar 1.
- It loops until stopped. Each chord sounds for its length and ends as the next one
  starts; the last chord before a Stop ends with it.
- While it plays you can play the melody half, change the root (transposes), change the
  mode (re-colours), switch voicing or strum, or play other chord pads on top. A pad set
  silent (Settings → Chords → *Sound* Off) is silent in the progression too.
- Picking another slot while it plays starts that progression at once, from its first
  chord (with a DAW: at the song's position). When the web editor talks to the board, the
  progression pauses with everything else and carries on where it was.
- The play screen shows the playing chord's name and `P1 Am > F 3/8`: slot, the chord now,
  the next one, and the unit playing of all the units. Long chord names are left out of
  this line to keep the position on screen (the chord now is also the large name above).

**Guide lights** (Settings → Sequence → *Guide*, On by default): while a progression plays,
the playing chord's pad lights soft white and the next chord's pad pulses in its colour,
so you can see what's coming; on the right half the playing chord's notes are bright and
the others dim, which shows the safe melody notes for this moment.

*Clear* (Settings → Sequence → *Clear*, ●) empties the selected slot. Pressing ▲ (or
Transport) with an empty slot shows `NO PROGRESSION YET`.

<a id="try-progression"></a>**Try it (the board):**
1. Root A, mode M6 (A minor). Settings → Sequence → *Progression* 1, *Unit* 1 bar, *Record* ●.
2. Bottom row of chord pads: tap Am, F, F, G (the 1st, 6th, 6th and 7th pads). Screen:
   `REC P1 4/64 G`. Tap G once more by mistake and press ◀ Left: back to 4.
3. ● Centre: `PROGRESSION 1 SAVED`. Settings → Play → *Arrows* → Sequencer, back to play.
4. ▲ Up: Am for a bar, F for two, G for one, round and round at 120 BPM; the screen shows
   `P1 Am > F 1/4`, the pads guide you. Play a melody on the bright right-half pads.
5. While it plays, press root C: the same progression in C minor (Cm A♭ B♭). Press M1
   (C major): the same pads now play C, Am and Bdim, the progression re-coloured. Press
   root A and M6 again.
6. ▲ Up again: it stops.

<a id="try-borrowed"></a>**Try it (borrowed chords): Am D Dm Am C G E7 Am**, a song that mixes A minor, A Dorian and
A harmonic minor:
1. Root A, M6. First the 7th for E7: play the bottom row's 5th pad (`Em`) and turn
   Settings → Chords → *Add 7th* On (`Em7`). Then *Unit* 1 bar, *Record* ●.
2. Tap Am (bottom row, 1st pad). Press **M2** (Dorian) and tap the 4th pad: `D`. Press
   **M6** again and tap the 4th pad: `Dm`. Tap Am, then C (3rd pad) and G (7th pad).
3. Press **M1** (A major) and tap the 5th pad: `E7`. Press **M6**, tap Am.
4. ● Centre, ▲ Up: Am D Dm Am C G E7 Am, with D and E7 exactly as recorded.
5. Afterwards, play the 5th pad and Settings → Chords → *Reset chord*.

(Simpler: type it in the [web editor](#progressions-1), which finds the pads and the
borrowed scales for you.)

**Why use it:** a backing to practise a melody over; a song's changes always at hand on
stage; a loop to write a melody to; chords from outside the scale without leaving the
pads; transposing a whole song with one button.
**When not to:** for long, through-composed parts or exact rhythms within a bar, record
into your DAW instead. A progression is chords on a grid of units, not a note sequencer:
it does not record the melody half, pressure, or timing within a unit.

## MIDI channels, pressure and velocity

### Channels
Settings → MIDI → *Chord ch* and *Melody ch*: the MIDI channel (1–16) of each half. Both
default to channel 1.

**Try it:** *Chord ch* 2, *Melody ch* 3; in your DAW put a pad sound on channel 2 and a
lead on channel 3: the left half plays the pad, the right half the lead.
**Why use it:** two instruments from one board, or recording chords and melody on
separate tracks.

**Shared notes on one channel:** MIDI can't tell two copies of the same note on the same
channel apart, so when both halves share a channel, a note that is already sounding is
shared: a melody pad playing a note the held chord already plays doesn't strike it again,
and the note ends only when both pads are let go (so lifting one never cuts the other).
To have every melody pad strike its own note, give the melody its own channel (*Melody ch*
2) and set the synth to receive all channels, or use two tracks.

### Pressure (aftertouch)
Settings → Play → *Chord AT* and *Melody AT*: what pressing harder on a held pad sends,
set separately for each half.

| Setting | Sends |
| --- | --- |
| Off | nothing from that half |
| Poly AT (default) | polyphonic aftertouch for each note the pad plays. A chord pad sends it for each of its notes, so leaning into a chord swells all of it. A note that another held pad also plays gets none, to keep it unambiguous. |
| Channel AT | channel aftertouch: the strongest held pad of that half |
| CC1 mod | mod wheel, from the strongest held pad of that half |
| CC2 breath / CC11 expr / CC74 filter | that controller, the same way |
| Pitch bend | bends up as you press harder (the strongest held pad of that half), back to the centre on release |

**Try it:** *Chord AT* CC74 on a synth pad whose filter responds to CC74: hold a chord
and lean in, the sound opens up. *Melody AT* Pitch bend: lean into a melody note and it
bends up.
**Why use it:** expression without knobs: swells, vibrato (CC1), bends.
**When not to:** if your synth doesn't respond to pressure, or a sound changes
unexpectedly when you press harder: set that half to Off. Many synths respond only to
channel aftertouch or CCs: if leaning into pads does nothing with Poly AT, try Channel AT
or CC74.

Pressure runs from 0 at the pad's *Release* point to full at its *Full* point
([Pad sensitivity](#pad-sensitivity)), so lowering *Full* makes full pressure easier to
reach. Channel AT and the CC settings send one value per half on that half's channel; when both
halves share a channel and the same setting, they send one value together. Releasing all
pads always brings the value back to 0 (pitch bend back to exactly the centre), however
slowly you let go; so does anything that interrupts playing, such as the web editor
talking to the board while you hold a pad. *(0.9.1; before, a slow release or an
interruption could leave a bend or controller slightly or fully up until you pressed
again.)*

**Try it (rest after release):** *Melody AT* Pitch bend, a synth on the computer. Hold a
melody pad hard (the pitch bends up), then lift your finger very slowly. The pitch glides
back down and ends exactly in tune. Play a note from the computer's keyboard on the same
synth: it is in tune too.

### Velocity
Settings → Play → *Chord vel* (the chord pads) and *Melody vel* (the right half) decide how
loud each new note is (its MIDI velocity, 1–127) from how hard the pad is pressed at the
moment the note starts. *(0.11.0: one setting per half; before, one *Velocity* for both.
Settings saved by older versions give both halves their old curve.)*

| Setting | What you get |
| --- | --- |
| Normal (default) | velocity follows pressure evenly: just pressed = quiet, pressed fully = 127 |
| Light | soft touches already play fairly loud; for a light playing style |
| Firm | soft touches stay quiet, loud notes need a firm press; for a heavy hand or more control at the quiet end |
| Fixed 100 | every note at 100, however you play (*Chord vel*: progression chords too) |

**What velocity does not change: how light a touch can start a note.** A pad starts a
note once its reading reaches the *Trigger* point; the velocity setting only shapes how
loud the note is after that, from velocity 1 at *Trigger* to 127 at *Full*. A touch too
light to start a note stays silent with every velocity setting: lower *Trigger* instead
([Pad sensitivity](#pad-sensitivity)).

**Try it:**
1. A piano or synth sound on the computer, *Melody vel* Normal. Play one melody pad several
   times, from a gentle tap to a firm press: the notes go from quiet to loud.
2. Set *Light* and tap gently again: the gentle taps are now clearly louder.
3. Set *Firm*: the same gentle taps are very quiet; only firm presses get loud.
4. Set *Fixed 100*: every note sounds the same, however you play. Back to Normal.
5. *Chord vel* Fixed 100, *Melody vel* Normal: chords at one even level however you hit
   them, the melody as dynamic as your fingers. The play screen's bottom line shows both
   (`V FIX/DYN`).

**Why use it:** match the board to how hard you naturally play, so the quiet-to-loud range
of your sound is easy to reach. *Fixed 100* suits sounds that shouldn't vary (organ, synth
bass, drum machines that use velocity for accents you don't want). Separate settings let a
steady pad or comping part sit under an expressive lead.
**When not to:** don't use *Light* or *Firm* to make the pads react to lighter touches;
they don't (see above), *Trigger* does. If notes are generally too quiet or too loud, check
the synth's own velocity range first; if your firm presses never reach 127, lower *Full*.

## Restrike

*(Optional; Off by default.)* Settings → Play → *Restrike*. Normally a held pad keeps the
notes it started with when you change something; the change applies to the next touch.
With Restrike on, held pads are struck again with their new notes as soon as you change
the root, scale, layout or the chord panel, or (in
[Live notes](#live-notes-optional)) the chord. (Before 0.8.0 this was called *Live update*.)

**Try it:** *Restrike* On, hold a chord pad, press another root button: the held chord
moves to the new key at once. With it Off, it keeps sounding in the old key until you
lift and play again.
**Why use it:** change keys or voicings under a sustained chord without lifting your hand.
**When not to:** the re-strike is audible (a new attack); with plucked or percussive
sounds you may prefer the change on the next touch.

## MIDI clock and transport

*(Optional.)*
- Settings → MIDI → *Clock out* (default Off): On sends MIDI clock at the board's *Tempo*
  to the computer (and the TRS jack), so a DAW, drum machine or synth can follow the
  board. While a MIDI clock is coming in, the board follows it and sends none.
- Settings → MIDI → *Transport*: ● sends **Start** (`PLAY`) or **Stop** (`STOP`). With the
  pedal set to *Play/Stop*, the pedal does the same.

If playing is interrupted while the transport runs (the web editor talks to the board, or
USB drops), the board sends **Stop**, and **Continue** when it plays again a moment later,
so a following sequencer pauses and carries on from where it was instead of jumping back
to the start. *(0.9.1; before, it started again from bar one.)* Turning *Transport* off
and on again sends a fresh Start.

**Try it:** *Clock out* On, set your DAW to sync to external MIDI clock from TheoryBoard
MIDI: the DAW shows 120 BPM. *Transport* ● starts the DAW, ● again stops it. Start it
again and, while it runs, click something in the web editor that talks to the board (for
example *Download settings backup*): the DAW pauses for about a second and then continues
from where it stopped.
**Why use it:** the board is the band leader: its tempo drives the DAW or a hardware drum
machine on the TRS jack.
**When not to:** if your DAW is the leader (it sends clock to the board), leave *Clock out*
and *Transport* off and start and stop from the DAW.

## TRS MIDI out

Everything the board sends over USB also goes out of the **TRS MIDI out** jack, so it can
play a hardware synth directly. The jack is **TRS Type A** (confirmed on a board): use a
Type A TRS-to-DIN adapter (Type B adapters swap the wires and won't work).

- Settings → MIDI → *TRS out*: On (default) or Off.
- **Without a computer:** power the board from a USB charger or power bank with the TRS
  jack connected. After about 3 seconds without a computer it plays to the jack alone.
  Plugging into a computer later switches back to USB (held notes end first).
- **If the computer goes away mid-chord** (cable pulled, computer asleep) while the board
  keeps power (DC input or a charger), the synth on the jack gets its Note Offs a few
  seconds later, when the board switches to the jack alone, so no note is left droning.
  *(0.9.1; before, those notes kept sounding until you played them again.)*
- The jack carries the notes, aftertouch and controllers you play, at MIDI's standard
  speed. Very dense pressure streams are paced to what the jack can carry.

**Try it:** connect a synth through a Type A adapter and play pads: the synth plays.
Settings → About → *I/O test* also sends middle C to the jack every second.
**Why use it:** play hardware synths with no computer on stage.

## Sustain pedal

> **Not yet confirmed on a board.** Settings → About → *I/O test* shows whether the board
> sees the pedal (`PEDAL ... DOWN` while pressed); please report what it shows.

Plug a sustain pedal (footswitch) into the pedal jack **before switching the board on**.

- Settings → Pedal → *Mode* (**Off** by default until the pedal input is confirmed):
  - **Sustain:** sends sustain (CC64) on the chord and melody channels; the screen shows
    `SUS` while it is down.
  - **Hold:** the board's own [hold](#hold) while the pedal is down: chords keep sounding,
    and the arpeggiator keeps running.
  - **Sostenuto / Soft / Portamento:** sends CC66, CC67 or CC65 instead.
  - **Play/Stop:** each press sends Start, then Stop.
  - **Note:** plays a note while the pedal is down, on the *Drum ch*: Settings → Pedal →
    *Note* (default C2 = 36, a General MIDI kick).
  - **Off:** the pedal is ignored.
- Settings → Pedal → *Type*: **Auto** (default: whatever the pedal reads at power-up
  counts as "released", so both pedal types work; that's why it must be plugged in
  first), **Normal** or **Inverted** if the pedal acts backwards.
- Settings → Pedal → *Target* *(0.11.0)*: **Both** (default), **Chords** or **Melody**:
  which half's MIDI channel gets the pedal in the Sustain, Sostenuto, Soft and Portamento
  modes. It needs the halves on different channels (Settings → MIDI → *Chord ch* /
  *Melody ch*); when they share one channel, the pedal goes to that channel and so to both.

**Try it:** *Mode* Sustain, play a chord with the pedal down and let go of the pad: the
sound keeps ringing until you lift the pedal. Then *Melody ch* 2 (and a synth or two
tracks listening to channels 1 and 2), *Target* Chords: with the pedal down, chords ring on
after you let go, melody notes stop when you lift your finger.
**Why use the other modes:** *Hold* keeps the board's chords (and arpeggios) going with
your foot; *Note* gives you a kick drum on your foot while both hands play. *Target*
Chords keeps a pad sustained under a clean, unsmeared melody (*Melody* the reverse).
**When not to:** *Target* does nothing useful while both halves share a channel.

## Presets

A preset stores the whole musical setup: root, scale, octaves, every chord edit, the
panel, hold, strum, arpeggiator, tempo, layouts, channels, pressure and velocity. The
board settings (brightness, colours, MIDI in lights, TRS out, pedal, arrows) are not part
of presets. There are 8 slots.

- **Save:** Settings → Presets → *Save preset* → ● → ▲/▼ choose 1–8 → ●. Saving over a
  slot replaces it (`PRESET 3 SAVED`).
- **Load:** Settings → Presets → *Load preset* → ● → choose a slot → ●. Empty slots show
  `EMPTY`.
- **Quick switch while playing:** hold **◀ Left** and press **▲ Up / ▼ Down** to load the
  previous / next saved preset (`PRESET 3 LOADED`, and `P3` near the scale name).
- A preset that uses a user scale which has since been cleared will not load.

**Try it:** set up D Dorian with strum on and save it as preset 1. Change to G major,
strum off, save as preset 2. On the play screen hold ◀ Left and press ▲/▼: the board
flips between the two setups.
**Why use it:** one preset per song, switched between songs (or between verse and
chorus) without touching the menu.

## MIDI in lights

With *MIDI in LEDs* on (Settings → Lights, default On), notes your computer sends **to**
the board light the pads that play them: melody pads whose note is sounding, and chord
pads whose whole chord is sounding.

**Try it:** send a C major chord from the DAW to the TheoryBoard MIDI port: the C-major
chord pads and the C, E, G melody pads light up.
**Why use it:** see where a song's chords and melody sit on the board, to learn a song or
to play along. **When not to:** if your DAW echoes the board's own notes back, every pad
you play stays lit a moment; turn it Off.

## Pad sensitivity

*(0.10.0)* Settings → **Pads** decides how a pad's pressure turns into notes. Each pad
measures how hard it is pressed as a number: a low number when untouched (a few hundred on
a typical board), rising to about 4000 when pressed very hard. Three points on that scale
matter, each adjustable in steps of 100:

| Setting | Range | Default | What it does |
| --- | --- | --- | --- |
| *Trigger* | 500–3000 | 1200 | a note starts when a pad's reading reaches this. Lower = a lighter touch plays. |
| *Release* | 300–2500 | 600 | the note ends when the reading falls below this; pressure (aftertouch, CCs, pitch bend) counts from here. Lower = you can relax further while holding a note. |
| *Full* | 1500–4000 | 3500 | velocity 127 and full pressure. Lower = loud notes and full pressure come with less force. |

*Trigger* has to stay at least 200 above *Release* (so a finger hovering at the edge can't
make a note stutter), and *Full* at least 500 above *Trigger*; a value that breaks this
shows `OUT OF RANGE` and the old value stays.

Up to 0.9.1 these were fixed at 2500 / 1500 / 4000, so the board needed a firm press to
play. The 0.10.0 defaults play from a light but deliberate touch. To get close to the old
feel, set *Trigger* 2500, *Release* 1500, *Full* 4000.

**The board protects itself.** Each time it starts playing (at power-up, and after the web
editor has talked to it), it measures how high the untouched pads rest and keeps *Release*
at least 130 above that, raising *Trigger* and *Full* with it if needed. A pad that rests
above *Release* would otherwise never count as let go. So a board whose pads rest unusually
high (for example with a sensitivity modification) still works with the defaults; the
[Pad test](#pad-test) page then shows the raised values with a `!`.

**Try it (find your own Trigger):**
1. Settings → Pads → *Pad test* → ● Centre. Hands off: the three pad numbers are your
   resting levels, and the bottom line shows `REST`, the level the board measured.
2. Touch a pad as lightly as you'd like to play. Its number appears at the top of the
   list; `PEAK` shows how far your touch went (say `PEAK 900`).
3. ◀ Left back to the list, ▼ Down to *Trigger*, ● Centre, then ▼/▲ to a little below
   your light touch's peak (here 800) and ● Centre to set it. ▼ Down to *Pad test* and
   ● Centre to open it again.
4. Back in *Pad test*, touch lightly again: the `*` appears and the note plays, at a low
   velocity (`LAST VEL`).
5. Press a pad firmly and watch the second and third rows: those are its neighbours. They
   must stay below *Trigger*, or they would play by themselves. If they reach it, raise
   *Trigger* until they don't.
6. Press as hard as you'd normally play your loudest notes and read `PEAK`. Set *Full* a
   little below that, so your loud notes reach velocity 127.

**Why use it:** match the board to your touch: light and fast for melodies, firmer if you
rest your fingers on the pads or play with a heavy hand. Boards differ, and a soft overlay
or a hardware modification changes how a press spreads; these settings let every board
play its best. They belong to the board: presets don't change them.

**When not to:**
- Don't lower *Trigger* below where your neighbours read when you press firmly
  (step 5): pads next to the one you play would start sounding by themselves.
- Don't lower *Release* to near your resting level: notes may not end reliably (the board
  keeps it at least 130 above rest, but the extra margin helps).
- **With a sensitivity modification or a thick overlay**, start with *Trigger* around
  1900 and check step 5. A modification that presses the membrane closer also spreads each
  press to the neighbours.

### Pad test

Settings → Pads → *Pad test* shows what the pads measure, as numbers: how hard a touch
reads, how much a press spreads to the neighbouring pads, and the velocity a note got. It
changes nothing; the pads keep playing while the page is open. *(0.9.1; on the About page
until 0.10.0.)*

```
PAD 1200/600/3500          Trigger / Release / Full as the board plays them
                           (! after them = raised for this board's resting level)
L C3 R2   2734 *           the pad reading highest now, its reading; * = it is playing
L C4 R2    812             the second and third highest: a press spreading to its
L C2 R2    640             neighbours shows up here
PEAK 3120 L C3 R2          the highest reading of the current (or last) touch
LAST VEL 64 L C3 R2        the velocity the newest note got, and its pad
<BACK      REST 420        the resting level the board measured when it started playing
```

`L C3 R2` means left half, column 3, row 2 (columns 1–12 from the left, rows 1–4 from the
top; the right half is `R`). The numbers above are an example: every board, and every pad,
reads a little differently.

**Try it:**
1. Settings → Pads → *Pad test* → ● Centre.
2. Hands off: the three numbers are small (your pads' resting readings), close to `REST`.
3. Rest a fingertip very lightly on one pad without playing it: its number rises but stays
   below *Trigger*, no `*`, no sound. That's the gap between touching and playing.
4. Press the pad gently until it plays: the `*` appears, `PEAK` shows how far you went,
   and `LAST VEL` the velocity of that note.
5. Press it firmly: higher `PEAK`, higher `LAST VEL`. Watch the second and third rows: the
   neighbouring pads' numbers rise a little as well. As long as they stay below *Trigger*
   they never play.
6. ◀ Left or ● Centre leaves the page.

**Why use it:** to set the [Pad sensitivity](#pad-sensitivity) from your own numbers, to
compare pads (a pad that needs much more force than the others, or that reads high when
untouched, shows up straight away), to check how much a pad overlay or a hardware
modification spreads a press to the neighbours, and to tell us real readings when you
report a sensitivity problem.
**When not to:** it's a measuring page, not a setting: nothing you see here changes how the
board plays.

## Settings menu

Press **● Centre** on the play screen to open the settings. They are pages of items:

| Button | Page list | A page | Changing a value |
| --- | --- | --- | --- |
| ◀ Left | back to the play screen | back to the page list | cancel, keep the old value |
| ▲ Up / ▼ Down | previous / next page | previous / next item | higher / lower value |
| ● Centre | open the page | change the item (or start a test) | set the value |
| ▶ Right | open the page | change the item | set the value |

- **Leaving:** only ◀ Left leaves, one level per press. Shortcut: **hold ◀ Left and press
  ● Centre** to go straight to the play screen from anywhere.
- The settings reopen on the page you left. Up and Down wrap around in lists; values stop
  at their lowest and highest setting.
- The top line names the page and the position (`3/7`); the value being changed is shown
  inverted. The bottom line shows what the buttons do, or the result of the last action.
- While the settings are open the root and mode buttons and the chord panel still work.

**Try it:** ● Centre (page list), ▼ Down to *STRUM*, ● Centre (the Strum page), ● Centre on
*STRUM* (the value turns inverted), ▲ Up to *UP*, ● Centre (set). ◀ Left twice: back to
playing.

Every item, with its default (items marked *opt* are optional extras, off or neutral by
default):

| Page | Item | Values | Default |
| --- | --- | --- | --- |
| Play | Scale | the 86 built-in scales, then U1–U12 | Major |
| | Family *(opt)* | Major / Mel minor / Harm minor / Harm major: what M1–M7 play | Major |
| | Hold | Off / On | Off |
| | Chord vel | Normal / Fixed 100 / Light / Firm: the chord pads ([Velocity](#velocity)) | Normal |
| | Melody vel | the same, for the right half | Normal |
| | Chord AT | Off / Poly AT / Channel AT / CC1 / CC2 / CC11 / CC74 / Pitch bend | Poly AT |
| | Melody AT | the same | Poly AT |
| | Restrike *(opt)* | Off / On ([Restrike](#restrike)) | Off |
| | Note repeat *(opt)* | Off / 1/4 / 1/8 / 1/16 / 1/32 ([Note repeat](#note-repeat)) | Off |
| | Tempo | 40–240 BPM: arpeggiator, note repeat, strum in note values, clock out ([Tempo](#tempo)) | 120 |
| | Arrows | what ▲/▼ do on the play screen: Scale / Melody octave / Preset / Tempo / Sequencer (▲ plays or stops the [progression](#progressions), ▼ records or finishes) (a board setting: presets don't change it) | Scale |
| | Melody oct | −2 … +2 | 0 |
| Chords | Voicing | Off / I / II / III / Guitar (the panel's) | Off |
| | Spread | Off / I / II / III (the panel's) | Off |
| | Voice lead *(opt)* | Off / On ([Voice lead](#voice-lead-optional)) | Off |
| | Fold *(opt)* | Off / On ([Fold](#fold-optional)) | Off |
| | Add 7th, Add 9th, Add 6th | Off / On (last chord played) | Off |
| | Sus | Off / SUS2 / SUS4 (last chord played) | Off |
| | Strum pad | On / Off (last chord played) | On |
| | Sound *(opt)* | On / Off (last chord played): Off picks the live notes without a sound | On |
| | Reset chord / Reset all | ● resets | |
| Layout | Layout | Scale / Chromatic / Fourths / Drums / Live notes | Scale |
| | Live ch *(opt)* | Melody / 1–16: the live notes' channel | Melody |
| | Live in *(opt)* | Off / 1–16 / All: chords arriving over MIDI pick the live notes | Off |
| | Bass ch | 1–16 (drum layout's middle grid) | 2 |
| | Drum ch | 1–16 (drum layout's drums, pedal note) | 10 |
| Strum | Strum *(opt)* | Off / Up / Down / Up+Down | Off |
| | Strum time | 10, 15, 20, 30, 40, 60, 80, 120 ms, 1/96, 1/48, 1/32, 1/24, 1/16 | 20 ms |
| | Repeat | Off, 1/4, 1/8, 1/16, 1/32 | Off |
| | Cutoff | Off, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 | Off |
| | Feel time *(opt)* | Off, 0–5, 0–10, 0–20, 0–40 ms late at random | Off |
| | Feel vel *(opt)* | Off, ±5, ±10, ±20, ±30 velocity at random | Off |
| | Capo | Off, fret 1–11 (guitar voicing) | Off |
| Arpeggiator | Arp *(opt)* | Off / Up / Down / Up-down / Random / As played / Chords | Off |
| | Arp rate | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 | 1/16 |
| | Arp octaves | 1–3 | 1 |
| | Arp gate | 25 / 50 / 75 / 100 % | 50 % |
| | Rhythm | 8 patterns: `XXXXXXXX`, `X.X.X.X.`, `.X.X.X.X`, `XX.XX.X.`, `X..X..X.`, `XXX.XXX.`, `X.XX.XX.`, `X...X...` | `XXXXXXXX` |
| | Swing | 0, 10, 20, 30, 40, 50 % | 0 % |
| | Sync start | Off / On | Off |
| | Latch | Off / On | Off |
| MIDI | Chord ch | 1–16 | 1 |
| | Melody ch | 1–16 | 1 |
| | TRS out | Off / On | On |
| | Clock out *(opt)* | Off / On | Off |
| | Transport | ● Play / Stop (also plays the [progression](#progressions)) | Stopped |
| Pedal | Mode | Off / Sustain / Hold / Sostenuto / Soft / Portamento / Play/Stop / Note | Off |
| | Type | Auto / Normal / Inverted | Auto |
| | Note | any MIDI note | C2 (36) |
| | Target | Both / Chords / Melody: whose channel the pedal's controller goes to | Both |
| Presets | Load preset | 1–8 | |
| | Save preset | 1–8 | |
| Lights | Brightness | 1–8 | 4 |
| | Colours | Rainbow / Root+5th | Rainbow |
| | MIDI in LEDs | Off / On | On |
| | LED test | Start | |
| Sequence | Progression | 1–8 (`EMPTY` when nothing is recorded there): the slot ▲ plays and *Record* records ([Progressions](#progressions)) | 1 |
| | Unit | 1 beat / ½ bar / 1 bar / 2 bars: one tap's length | 1 bar |
| | Record | ● starts recording into the slot (replacing it); ● again finishes | |
| | Clear | ● empties the slot | |
| | Guide | Off / On: guide lights while a progression plays | On |
| Pads | Trigger | 500–3000 in steps of 100: a note starts here ([Pad sensitivity](#pad-sensitivity)) | 1200 |
| | Release | 300–2500: the note ends below this; pressure counts from here | 600 |
| | Full | 1500–4000: velocity 127 and full pressure | 3500 |
| | Pad test | Start: the pad readings, the peak of a touch, the last note's velocity ([Pad test](#pad-test)) | |
| About | Version | the TBOSS version, e.g. `0.11.0` | |
| | I/O test | Start: shows the pedal input and two alternative inputs live, sends middle C to the TRS jack every second, and shows pads that read as held | |

### Shortcuts

On the play screen:

| Button | Shortcut |
| --- | --- |
| ● Centre | Open the settings |
| ▶ Right | Hold on/off |
| ▲ Up / ▼ Down | Set in Settings → Play → *Arrows*: **Scale** (default): next / previous of your [favourite scales](#favourites-optional), or with none, of all scales including your U1–U12, skipping empty slots; **Melody octave**: melody half an octave up / down; **Preset**: next / previous saved preset; **Tempo**: 5 BPM faster / slower; **Sequencer**: ▲ plays / stops the [progression](#progressions), ▼ starts / finishes recording |
| Hold ◀ Left + ▲ Up / ▼ Down | Previous / next saved preset (whatever *Arrows* is set to; not while recording a progression) |
| ◀ Left | Nothing (so pressing it once too often on the way back is harmless); while recording a progression, takes back the last unit |
| ● Centre while recording | Finishes the progression and saves it |

## Screen and lights

Play screen, top to bottom:
1. Root (large), scale name, and the mode button and/or preset (`MODE M2  P3`).
2. Chord name of the last chord pad played, with its edits (`Dm7`, `G7sus4`, `C/E`). In
   Live notes following MIDI in, the incoming chord's name. While a progression plays, its
   chord.
3. The last thing that happened (`ROOT D`, `VOICING II`, `PRESET 2 LOADED`…).
4. Octaves (`OCT C2 C3`), `HOLD` when hold is on, `SUS` while the sustain pedal is down.
5. A progression recording (`REC P1 5/64 Am`) or playing (`P1 Am > F 3/8`), the
   arpeggiator (`ARP UP 1/16 120` or `EXT`), strum (`STRUM UP 20MS`), note repeat
   (`REPEAT 1/16 120`), or velocity and pressure for chords/melody (`V DYN/DYN A POLY/POLY`).
   The number is the tempo. `USB: WAITING` until a computer is connected.

Pad colours (Settings → Lights → *Colours*):

**Rainbow** (default): every pad shows the colour of its note, the same colour for the
same note everywhere on the board (C red, D yellow, E green, G blue, A violet, B pink and
the colours between). The colours move when you change root or mode. Root columns are
brightest.

| Pad | Meaning |
| --- | --- |
| White, brighter the harder you press | you are touching it |
| Soft white | sounding without your finger: held, latched, or feeding the arpeggiator |
| Paler colour | a chord pad with edits |
| Dim | a chord pad set silent (*Sound* Off) |
| White (not touched) | notes arriving over MIDI |
| Soft white / pulsing chord pad, bright and dim melody pads | a progression's guide lights: the chord now, the next chord, its notes ([Progressions](#progressions)) |

In the chromatic and fourths layouts, notes outside the scale are dimmed. In the drums
layout the drum grid is amber, its kick pad brightest. In Live notes, the live chord's
root is brightest.

**Root+5th**: root orange, fifth blue, other steps dim, edited chords purple, pressed
pads white, notes arriving over MIDI green. **Why:** some players find it easier to find
the root and fifth at a glance.

**While the web editor edits a scale**, the whole board becomes a keyboard of the scale:
both halves show 12 columns, one per semitone from the editor's root. The root column is
white, scale notes bright, other notes nearly dark. The screen shows `WEB EDITOR`, the
slot (`U2`), the root and the notes. Normal colours return 5 seconds after the editor
stops sending.

A pad that already reads as pressed when the board starts (held, or a pad stuck on your
board) lights white and stays silent until it has been released once; everything else
plays normally. Settings → About → *I/O test* shows how many pads read as held and where
(`PADS HELD 1 L C10 R2`: left half, column 10, row 2 from the top); *Pad test* shows the
readings themselves ([Pad test](#pad-test)).

*Brightness* (1–8) sets all lights. *LED test* lights every pad red, green and blue, then
each pad and each button light in turn: **Try it** if a pad or button light seems dead.

## Web editor

The web editor runs in **Chrome or Edge** on a computer (Safari and Firefox lack the MIDI
features it needs). It browses the scale library, keeps your favourite scales on the
board, makes custom scales, types in chord progressions, and backs up the board's settings.

### Start it
Open **https://armandhammer.github.io/TBOSS/** in Chrome or Edge. Nothing to install.

Click **Connect**. Chrome asks whether the page may "control and reprogram your MIDI
devices": choose **Allow**. The header turns to `Connected` and shows the firmware
version. With several MIDI devices, pick **TheoryBoard MIDI** in the *Board* list.

The board stops its notes while the page talks to it and plays again about a second
after the last command, so don't play while the page is saving.

To run the editor from your own copy instead (for example offline), from the folder you
downloaded TBOSS into: `python scripts/tboss_editor.py`. It starts a small web server on
your computer only and opens the editor at `http://localhost:8000`; keep its window open
(Ctrl+C stops it). A page opened straight from the file will not work: browsers allow
MIDI with SysEx only for `localhost` or `https` pages.

### Scale library
Every built-in scale, grouped into families.

- **Browse:** pick a **Group** (or All scales) and an **Order**: *As listed*, or
  *Bright → dark* (by how high the scale's notes sit above the root: Lydian is the
  brightest of the major modes, Locrian the darkest). Click a scale to see its notes in
  the board's colours in the chosen **Key**.
  - **▶ Play** plays it on the computer.
  - **Play it on the TB** switches the board to it, keeping the board's key.
  - **Add to favourites** puts it on your favourites list (below).
  - **Copy to my scales** opens it in a custom slot, to change it.
- **Find by notes:** click notes on the keyboard, or tick **Listen to MIDI keyboards** and
  play them on the TB or any MIDI keyboard: the list shows every scale and key that
  contains them, fewest notes first. Click one to select it in that key. **Clear notes**
  starts over.
- **One note away:** under the selected scale, every scale you get by moving one of its
  notes a semitone (Major: lower the 3rd → melodic minor, raise the 4th → Lydian, lower
  the 7th → Mixolydian…). Click to go there.

**Try it:** Find by notes, click C, E♭, F♯: the list shows the scales and keys with all
three (blues, diminished and others). Click *Blues* in C, **▶ Play**, then **Play it on
the TB**: the board shows `BLUES`.
**Why use it:** you hear a melody or a riff and want the scale that fits it: play its
notes and pick from the list. *One note away* is a quick way to hear what one note
changes, and *Bright → dark* to find a mood.

### Favourites
Up to 128 scales (built-in or your own U1–U12) in the order the board's ▲/▼ step through
them. Reorder by dragging a row by its ⠿ grip (or with its ▲ ▼ buttons), remove one with
✕ or all with **Remove all**, then **Save favourites to the TB**.

**Try it:** add Major, Dorian and Blues, drag Blues to the top, **Save favourites to the
TB**. On the board, ▲ Up: `FAV 1/3 BLUES`.
**Why use it:** the scales of a set list, in order, on one button. **When not to:** to
step through every scale again, **Remove all** and save.

### Custom scales
1. Choose a slot tab, **U1** to **U12**.
2. Click piano keys to add or remove notes. The root (orange) is always included; a scale
   needs **5 to 12 notes**. Below each key is its interval (`b3` = minor third, `#4` =
   augmented fourth…). **Start from** fills in a known scale to change, and its name. The
   summary line names a match if your scale is a known one.
3. **Root for listening** only changes what **▶ Play** sounds like; scales are stored
   relative to the root, so the board plays them in any key.
4. While you edit, the board's pads show the scale (root white, scale notes in colour,
   others dark) and its screen shows `WEB EDITOR`, the slot and the notes.
5. Type a **Name** (up to 12 characters) and click **Save to U1**.
6. Press **U1** on the board, or click **Use it on the TB** (U4–U12 have no button: choose
   them in Settings → Play → *Scale* or with that button). **Clear slot** empties it (not
   possible while the board uses it).

**Try it:** slot U5, *Start from* Blues, add the major 3rd (E in C), name it `BLUES MAJ`,
**Save to U5**, **Use it on the TB**: the board shows `BLUES MAJ` and its chords follow.
**Why use it:** a scale from a song, a world-music mode, or an exercise that isn't in the
library.

### Progressions
*(TBOSS 0.11.0 or later.)* Type a song's chords instead of tapping them in, and see what
each slot holds.

1. Choose a slot tab, **P1** to **P8** (the tab shows how many chords each holds). A
   stored progression appears as text.
2. Pick the **Key** and its scale (on connecting, the board's own key is selected) and how
   long **Each unit** lasts.
3. Type the chords, separated by spaces: `Am D Dm Am C G E7 Am`. Each chord lasts one
   unit; `Am*2` (or `Am Am`) lasts two. Commas and bar lines `|` are ignored. Below the
   text, each chord appears with its length; a chord outside the key's scale is outlined
   and names the scale it is borrowed from (`D · Dorian`, `E7 · Harmonic minor`).
4. **Save to P1**, then play it on the board ([Play](#play)) in the same key and scale.

The editor finds each chord on the chord pads: in the key's scale if it is there,
otherwise in the closest scale that has it (the fewest notes different, preferring the
major modes, harmonic and melodic minor and harmonic major). Chord types: major, `m`,
`dim`, `aug`, `sus2`, `sus4`, `6`, `m6`, `7`, `maj7`, `m7`, `mMaj7`, `m7b5`, `dim7`,
`7#5`, `7b5`, `maj7#5`, `7sus4`, `7sus2`, `add9`, `madd9`, `9`, `maj9`, `m9`, `6/9`,
`m6/9`, `9sus4`, `7b9`, `7#9`, `13`, `maj13`, `m13`, and slash chords whose bass is the
chord's third or fifth (`C/E`, `C/G`). A chord no scale can play on the pads (`C/F#`,
`C5`) is reported and nothing is saved until it is changed.

- The board stores pads, not notes, so the key here only names the chords: on the board,
  the root buttons transpose the progression and the key you pick here is the one where it
  sounds as written.
- **Copy as text** copies the chords, to paste into a note or share.
- **Clear P1** empties the slot.

<a id="try-typed"></a>**Try it:** Key A, Natural minor, Each unit 1 bar; type `Am D Dm Am C G E7 Am`; D and E7
are outlined (Dorian, Harmonic minor). **Save to P1**. On the board: root A, M6, Settings →
Sequence → *Progression* 1, ▲ (Arrows: Sequencer): the song plays. Change the key in the
editor to C: the text stays, and is placed again for C minor.
**Why use it:** a song from a chord chart in seconds, with the borrowed chords worked out
for you; and a readable list of what each slot holds. **When not to:** chords that need a
bass note outside the chord, or rhythms finer than the unit: record those in your DAW.

### Settings backup
- **Download settings backup** saves the current settings, all 8 presets, the user scales,
  your favourites and your progressions to a `.json` file.
- **Restore from file…** writes them back (user scales first, then favourites, presets,
  progressions, then the current settings). Into an older TBOSS, settings it doesn't have
  go back to their defaults and progressions are left out (before 0.11.0).

**Why use it:** before installing firmware with the SWD tool, before lending the board, or
to keep different setups for different gigs. (The browser installer saves and restores
your settings by itself.)

### External memory backup
The board has a 16 MB memory chip holding data from its original firmware. TBOSS only
ever **reads** it. **Back up external memory** copies the whole chip to a file, reading
it twice; the copy counts as verified only if both reads are identical. It takes several
minutes per read and the board does not play meanwhile (**Cancel** stops it). Save both
downloaded files (the `.bin` image and the `.txt` with its SHA-256).
**Why:** a safety copy of the original firmware's data before anyone ever experiments
with that chip. You only need it once.

## Saved settings

Every change is saved automatically 3 seconds after you stop changing things, and comes
back at power-up, together with presets, user scales, favourites and progressions. Installing firmware
clears them: the [browser installer](flashing.md) saves them first and puts them back
afterwards (step 2 on its page); with the SWD tool, use the web editor's settings backup
first.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Only one note at a time | The instrument is mono/legato: switch the preset or the synth to poly. |
| No sound | Check the DAW's MIDI input and the track's MIDI channel against *Chord ch* / *Melody ch*. |
| A chord stays on | Hold is on (`HOLD` on screen): touch the chord's pad, or press ▶ Right on the play screen. |
| Chord pads play single notes in a pattern | The arpeggiator is on (`ARP` on screen): Settings → Arpeggiator → *Arp* → Off. |
| A held melody pad keeps re-striking | Note repeat is on: Settings → Play → *Note repeat* → Off. |
| A melody pad doesn't retrigger a note the held chord is already playing | Both halves are on the same MIDI channel, so the note is shared (see [Channels](#channels)). Give the melody its own channel: Settings → MIDI → *Melody ch* → 2, and set the synth to receive all channels (or use two tracks). |
| Chords sound in the wrong octave or very wide | The chord panel's octave or spread: tap octave **0** and press the lit spread again; Settings → Chords shows both. |
| Chords don't start on their root / sound "inverted" | *Voice lead* or *Fold* is on (Settings → Chords), or a voicing button is lit. |
| A chord pad makes no sound but changes the live notes | Its *Sound* is Off: play it, then Settings → Chords → *Sound* On. |
| Right half plays odd notes in columns 8–12 | The Live notes layout: Settings → Layout → *Layout* → Scale. |
| ▲/▼ change the scale | That's the default; Settings → Play → *Arrows* chooses melody octave, preset, tempo or sequencer instead. Hold ◀ Left with ▲/▼ switches presets. |
| `NO PROGRESSION YET` | The selected slot is empty: record one, or pick another slot in Settings → Sequence → *Progression* (empty slots say `EMPTY`). |
| `NOTHING RECORDED` | A recording finished with no chords in it, so the slot kept what it had. Tap chord pads after starting to record. |
| `NOT SAVED: RESTART` | The board's settings memory filled up during a very long session (hundreds of saves). What you did still plays; switch the board off and on (it tidies the memory as it starts) and save again. |
| A progression plays the wrong chords | It plays in the board's current key and mode: set the root and mode it was recorded or typed in. Borrowed chords keep their own scale. |
| A progression doesn't start with the DAW | The DAW must send MIDI clock (with Start/Stop) to the TheoryBoard MIDI port. ▲ (Arrows: Sequencer) also starts it. |
| A progression makes no chords, only single notes | The arpeggiator is on: it arpeggiates the progression. Settings → Arpeggiator → *Arp* → Off. |
| The pedal sustains the wrong half, or both | Settings → Pedal → *Target*; it can only pick one half when *Chord ch* and *Melody ch* differ. |
| All lights dim or flicker together at random | The USB port isn't supplying steady power. Use the board's DC power input, or try another USB port, a short cable or a powered hub. |
| Pads play nothing for a second | The web editor just talked to the board; it resumes after 1 s. Held notes end, pressure and pitch bend return to rest, and a running transport pauses and continues. |
| Light touches don't play | Lower Settings → Pads → *Trigger* ([Pad sensitivity](#pad-sensitivity)); *Velocity* (Light/Firm) only changes how loud notes are. [Pad test](#pad-test) shows how far your light touches reach. |
| Neighbouring pads play by themselves when you press one | Raise Settings → Pads → *Trigger* above what the neighbours read in [Pad test](#pad-test) (around 1900 with a sensitivity modification or a thick overlay). |
| Notes don't end when you lift your finger, or a pad never plays | A pad resting above *Release*: Pad test shows `!` or a high `REST`; raise *Release* (and *Trigger*). |
| The pads feel different after updating to 0.10.0 | The default *Trigger* is lighter now. For the old feel: *Trigger* 2500, *Release* 1500, *Full* 4000. |
| A pad plays without being touched, or needs much more force than the others | Open [Pad test](#pad-test) with hands off: a pad reading high when untouched, or a pad whose numbers barely rise when pressed, is a hardware problem with that pad. |
| Notes kept sounding on a hardware synth after unplugging the computer | Update to 0.9.1 or later: with TRS out on and the board still powered, they end a few seconds after unplugging. |
| Editor says MIDI access refused | Chrome's site settings: allow MIDI devices, then Connect again. |
| Editor: "No TheoryBoard reply" | Pick TheoryBoard MIDI in *Board*; close other programs holding the port exclusively. |
| Arpeggiator or note repeat ignores the DAW tempo | Enable MIDI clock output to the TheoryBoard MIDI port in the DAW. |
| Arpeggiator, note repeat or strum too fast or slow | Settings → Play → *Tempo* (default 120 BPM). |
| `U1 IS EMPTY` | Make a scale in the web editor first. |
| `OUT OF RANGE` | That edit or octave would leave the MIDI note range; move the octave back first. |
| Everything seems different from yesterday | A preset was loaded (hold ◀ Left + ▲/▼ loads presets); load the one you want, or check the settings marked *opt* in the [Settings menu](#settings-menu). |

## TBOSS and the original TheoryBoard

How TBOSS (0.11.0) compares with the TheoryBoard's own firmware: the features it was sold
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
| Panel buttons light up to show what is on | ✅ |
| Live Note Update (the right half plays the notes of the chord you hold) | ✅ [Live notes](#live-notes-optional) layout (0.8.0) |
| Octave folding (chords kept inside one octave) | ✅ Settings → Chords → *Fold* (0.9.0), off by default |
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
| Performance mode (save chords in a designated area to recall for songs) | 🟡 [Progressions](#progressions) keep a song's chords in order (8 slots, borrowed chords included); [presets](#presets) save the whole setup |
| Change colour palettes | 🟡 Two schemes (rainbow, root+5th); your own colours are planned |
| Bass mode (doubles notes so they can be played with rapid fire in tandem) | ❌ Not yet |
| Arpeggiator | ✅ 6 patterns, rate, octaves, gate, 8-step rhythms, swing, sync start, latch, follows MIDI clock ([Arpeggiator](#arpeggiator)) |
| Step sequencer (chords and/or single notes) | 🟡 Chords: [progressions](#progressions) (0.11.0), recorded by tapping or typed in the editor, played in time with the transport or a DAW; single notes not yet |
| Program changes via MIDI CC, NRPN, or pads on their own MIDI channels | ❌ Not yet (each half, the live notes and the drum layout's three grids have their own MIDI channel) |
| Voicing, spread or octave for single chords instead of all (and saved) | 🟡 Per pad: add 7th/9th/6th, sus2/4, strum on/off, sound on/off, saved; voicing, spread and octave apply to all chords |
| Strum types besides up and down | ✅ Up+down, repeat while held, cutoff, per-pad on/off, random feel, an arpeggiator *Chords* pattern |
| Pitch / mod from the pads or the screen | ✅ From pad pressure: pitch bend or CC1 mod (also CC2/11/74) |
| Chords on both sides (same scale or not), mix and match single notes | ❌ Not yet (the right half: scale, chromatic, fourths, drums or live notes) |
| MIDI in over USB | ✅ MIDI clock in, [MIDI in lights](#midi-in-lights), live notes from MIDI in, the web editor |
| Alternative spread: the root note or the entire chord | 🟡 Root only; guitar voicing gives a wide open chord |
| Turn off octave folding | ✅ TBOSS doesn't fold unless you turn *Fold* on (0.9.0) |
| Live note update via MIDI in when the DAW plays chords | ✅ Settings → Layout → *Live in* (0.8.0) |
| Latch key: press a pad, latched, press again, unlatched | ✅ [Hold](#hold) (▶ Right), and the arpeggiator's latch |
| Drum kit mode | ✅ Drums layout: scale, bass and General MIDI drum grids, each on its own channel |
| Live note update on its own MIDI channel | ✅ Settings → Layout → *Live ch* (0.8.0) |
| A chord set to stay silent and still feed Live Note Update | ✅ Settings → Chords → *Sound* Off (0.8.0) |
| Note repeat mode (like an MPC) | ✅ Settings → Play → *Note repeat* for the right half (0.9.0); chord pads: strum *Repeat* |

### Only in TBOSS

| TBOSS extra | |
| --- | --- |
| Open source (GPL), reproducible builds, automatic tests | ✅ |
| Web editor: scale library, find scales by notes, favourites, custom scales, typed chord progressions, settings backup | ✅ |
| Chord progressions with borrowed chords, live transposition and guide lights | ✅ (0.11.0) |
| Browser installer that keeps your settings | ✅ |
| 8 presets of the whole setup, switched from the play screen | ✅ |
| Chromatic and fourths layouts for the melody half | ✅ |
| Chord names on the screen, per-pad chord edits (7th, 9th, 6th, sus) | ✅ |
| Auto voice leading (smooth chord changes) | ✅ (0.9.0) |
| MIDI clock out with Start/Stop; follows an external clock | ✅ |
| Velocity curves (normal, light, firm, fixed), set per half | ✅ (per half: 0.11.0) |
| Adjustable pad sensitivity: Trigger, Release and Full, with a self-measured resting level | ✅ (0.10.0) |
| Poly aftertouch from chord pads on each of their notes | ✅ |
| ▲/▼ shortcuts: step through your favourite scales, melody octave, presets or tempo | ✅ |
| Scale families on M1–M7 (melodic minor, harmonic minor, harmonic major modes) | ✅ |
| Strum feel (random timing and velocity) and a capo for the guitar voicing | ✅ |
| I/O test, LED test and Pad test (the pads' readings as numbers) pages | ✅ |

## Known issues

TBOSS is alpha software, tested on one board so far. Known gaps and limits:

- **Not in TBOSS yet:** a note sequencer/phrase recorder (chord progressions are in), a second arpeggiator, bass mode,
  per-pad sensitivity calibration, user colours, MPE. See the [roadmap](roadmap.md).
- The arpeggiator has no on/off shortcut button yet: use Settings → Arpeggiator → *Arp*.
- The chord edits that moved a chord's 3rd or 5th down an octave (0.5 and earlier) have
  no button any more; edits saved with them still play, and *Reset chord* clears them.
- The board appears with a test USB ID (`cafe:4069`) until TBOSS has its own.
- Windows may keep showing an older name for the MIDI port; it is the same device.
- Installing clears saved settings unless the browser installer's step 2 is used.
- Going back to an older TBOSS: the browser installer (and the editor's *Restore from
  file…*) put back what the older version has. Settings it doesn't have return to their
  defaults, and progressions are left out of a version before 0.11.0 (the installer says
  how many). Backups restored into versions before 0.10.0 may still be refused if they
  use settings those versions don't have.
- The external memory chip is only read, never used; the original firmware's data there
  stays untouched.
- On Linux the browser installer needs a one-time permission rule (see
  [Installing](flashing.md#with-a-browser-recommended)).

**Not yet tried on a board:** the sustain pedal input and its *Target* (the pin comes from
the board's previous firmware; the I/O test shows what the board sees), live notes
following MIDI in and on their own channel, progressions started by a DAW.

**Least tested so far** (reports especially welcome): installing from macOS, Linux and
ChromeOS (Windows is confirmed); the arpeggiator following MIDI clock from a DAW; the web
editor's external memory backup on a real board; boards that report a hardware version
other than 4 (the installer stops for those and asks for a report).

Please report what you find: a [test report](https://github.com/armandhammer/TBOSS/issues/new?template=test_report.yml)
after trying TBOSS (even "everything worked"), or a [bug report](https://github.com/armandhammer/TBOSS/issues/new/choose)
for a single problem, with the TBOSS version (Settings → About) and, for install problems,
the installer's *Details*.

## Feature check

A quick pass through everything, for example after installing a new version. Expected
results are in brackets. Turn each optional feature back off after its step.

1. Power up with USB → `TBOSS` and the version, then the play screen; the DAW sees
   TheoryBoard MIDI. [pads lit in rainbow colours; a touched pad turns white]
2. Melody pads, several at once, soft and hard. [polyphonic, louder when harder]
3. Root C, mode M1, bottom row of chord pads left to right. [C Dm Em F G Am Bdim on screen]
4. Press D, then M2. [`D DORIAN`, bottom-left chord `Dm`]
5. Bottom-left chord; Settings → Chords → *Add 7th* On, then *Reset chord*. [`Dm7`, paler
   pad; back to `Dm`]
6. Chord panel, top row: voicing I, II, III, guitar, each followed by a chord pad.
   [`VOICING I`…`GUITAR VOICING`; the chord moves up, guitar sounds wide]; press guitar again.
7. Spread I, II, III. [`SPREAD I`…, a deep root note]; press III again.
8. Octave V (0 lit): tap +1, then 0, then +1. [`OCTAVES 0 +1`: two octaves; `OCTAVES +1`:
   one octave up; `OCTAVES 0`: back]
9. Play screen ▲ Up, ▼ Down twice, ▲ Up. [`SCALE PHRYGIAN`, `SCALE DORIAN`, `SCALE MINOR`,
   `SCALE DORIAN`]. Settings → Play → *Family* Mel minor, press M4. [`LYDIAN DOM`];
   *Family* back to Major. Guitar voicing on, bottom-left chord in C. [C E G C E];
   Settings → Strum → *Capo* fret 3 [the same chord, a different shape]; Capo Off,
   guitar voicing off.
10. ▶ Right (hold), play a chord, lift, touch it again. [keeps sounding; then silence];
    ▶ Right again.
11. Settings → Strum: Strum Up, Strum time 80 ms, play a chord. [notes low to high];
    *Repeat* 1/8 and hold the pad [strums again every eighth]; *Cutoff* 1/16 [short
    stabs]; Repeat, Cutoff and Strum Off.
12. ◀ Left, ◀ Left. [page list, then the play screen; a third ◀ does nothing]
13. Settings → Arpeggiator → *Arp* Up; hold a chord. [single notes in a pattern;
    `ARP UP 1/16 120`]; *Rhythm* `X.X.X.X.` [every other step rests]; *Latch* On, lift
    the hand [keeps playing; touch the pad to stop]; *Arp* Off.
14. Root C, M1. Tap the bottom-left chord and let go (nothing to hold); Settings → Chords
    → *Voice lead* On; back on the play screen tap the bottom-row F, then G. [`F/C`,
    `G/D`]; *Voice lead* Off.
15. Root F, Settings → Play → *Scale* HARM MINOR, Settings → Chords → *Fold* On; bottom-left
    chord. [A♭ C F]; 5th column [C E G]; *Fold* Off; root C, M1.
16. Settings → Play → *Note repeat* 1/16; hold a melody pad and lean in. [repeats, louder
    as you press]; *Note repeat* Off.
17. Settings → Layout → *Layout* Chromatic, Fourths, Drums. [pad colours change; Drums:
    right grid amber, kick on its bottom-left pad on channel 10]; Live notes, play the G
    chord, then the right half's columns 8–12 [G B D G B along the bottom row; play Am and
    they become A C E A C]; back to Scale.
18. Settings → MIDI: Chord ch 2, Melody ch 3, a MIDI monitor. [chords on 2, melody on 3];
    both back to 1.
19. Settings → Play → *Chord AT* Poly AT, hold a chord pad and press harder. [poly
    aftertouch on each of its 3 notes]; *Melody AT* Pitch bend [bends up with pressure,
    back on release]; back to Poly AT.
20. *Melody vel* Light, then Firm. [the same touch is louder, then softer]; back to Normal.
    *Chord vel* Fixed 100. [chords all at 100, melody still dynamic; screen `V FIX/DYN`];
    back to Normal.
21. Settings → MIDI → *Clock out* On. [MIDI monitor: clock at 120 BPM]; *Transport* ●
    [Start, then Stop on the next press]; start it again and click *Download settings
    backup* in the web editor [Stop, then Continue about a second later]; *Transport* ●
    to stop; *Clock out* Off.
22. Save preset 1; change root; on the play screen hold ◀ Left and press ▼ Down.
    [`PRESET 1 LOADED`, setup restored]
22a. Root A, M6. Settings → Sequence → *Progression* 1, *Record* ●; tap Am, F, F, G
    (bottom row), ◀ Left once, tap G, ● Centre. [`PROGRESSION 1 SAVED`]. Settings → Play →
    *Arrows* Sequencer; ▲ Up. [Am, F for two bars, G, looping; `P1 Am > F 1/4`; the
    current pad white, the next pulsing]; press root C [the same pads in C]; root A.
    MIDI monitor: *Chord vel* Fixed 100 [from the next chord, every chord at velocity 100];
    back to Normal [the tapped velocities again]; ▲ Up [stops]. *Arrows* back to Scale.
23. Send a C major chord from the DAW to the board. [matching pads light up]
24. Web editor: Connect, scale library, Find by notes C E♭ F♯, pick Blues, *Play it on the
    TB*. [`BLUES` on the board]. Custom scales: U5, *Start from* Blues, Save, *Use it on
    the TB*. [while editing, the pads showed the scale and the screen said `WEB EDITOR`]
    Progressions: P2, Key A Natural minor, type `Am D Dm Am C G E7 Am`, Save to P2.
    [D and E7 outlined]; on the board root A, M6, *Progression* 2, ▲ Up [the song plays].
25. Web editor: Download settings backup; Restore from file. [`Restored n of n records`]
26. Settings → About. [`0.11.0`, the same version as the editor's header]. Settings → Pads →
    *Pad test*: rest a finger lightly on a pad, then press it. [`PAD 1200/600/3500`; its
    number rises; `*`, `PEAK` and `LAST VEL` appear once it plays]; *Trigger* 800 [lighter
    touches play]; *Trigger* 500 [`OUT OF RANGE`: too close to Release]; *Trigger* back to 1200.
27. TRS out to a synth (Type A adapter), play. [the synth plays]
28. Sustain pedal plugged in before power-up; Settings → About → *I/O test*, press it.
    [`PEDAL PD13 ... DOWN` while pressed]; then Settings → Pedal → *Mode* Sustain, play
    with it. [`SUS` on screen, notes sustain]; *Mode* Off.
29. Power off and on. [settings, presets, chord panel and U5 come back]
