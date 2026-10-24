# TBOSS user manual

TBOSS is open-source replacement firmware for the TheoryBoard THY333. This manual covers
every feature on the board and in the web editor. For installing the firmware see
[Installing](flashing.md). This is the **1.5 beta**: 1.4 and 1.5 come out together, and
everything new in both passed its board tests on one board. See
[Known issues](#known-issues) and please report anything that doesn't work as described here.

**How each feature is explained:** what it does, then **Try it** (steps you can follow on
the board, with what you should see or hear), **Why use it** and, where it matters, **When
not to**. Features that go beyond the original TheoryBoard are marked **Optional**: they
are off until you turn them on.

Contents: [Coming from the original TheoryBoard](#coming-from-the-original-theoryboard) ·
[New in 1.5](#new-in-15) · [New in 1.4](#new-in-14) · [New in 1.3](#new-in-13) · [New in 1.0](#new-in-10) · [The board](#the-board) · [Getting started](#getting-started) · [Playing](#playing) ·
[Key and scale](#key-and-scale) · [Chord panel](#chord-panel) ·
[Center buttons and Shift](#center-buttons-and-shift-optional) · [Chord edits](#chord-edits) ·
[Chord sets](#chord-sets-optional) · [Chord capture](#chord-capture-optional) · [Chord grid](#chord-grid-optional) ·
[Full chord layout](#full-chord-layout-optional) ·
[Theory hints](#theory-hints-optional) · [Pivot chords](#pivot-chords-optional) ·
[Voice leading and fold](#voice-leading-and-fold) · [Octaves](#octaves) ·
[Right-half layouts](#right-half-layouts) · [Hold](#hold) · [Tempo](#tempo) · [Strum](#strum) ·
[Note repeat](#note-repeat) · [Arpeggiator](#arpeggiator) · [Progressions](#progressions) ·
[Step sequencer](#step-sequencer-optional) ·
[MIDI channels, pressure and velocity](#midi-channels-pressure-and-velocity) ·
[Restrike](#restrike) · [MIDI clock and transport](#midi-clock-and-transport) ·
[TRS MIDI out](#trs-midi-out) · [Sustain pedal](#sustain-pedal) · [Presets](#presets) ·
[MIDI in lights](#midi-in-lights) · [Scale lock](#scale-lock-optional) · [Pad sensitivity](#pad-sensitivity) ·
[Settings menu](#settings-menu) ·
[Screen and lights](#screen-and-lights) · [Web editor](#web-editor) ·
[Saved settings](#saved-settings) · [Troubleshooting](#troubleshooting) ·
[TBOSS and the original TheoryBoard](#tboss-and-the-original-theoryboard) ·
[Known issues](#known-issues) · [Feature check](#feature-check)

---

## Coming from the original TheoryBoard

TBOSS starts out playing like a stock TheoryBoard: chords of the key on the left, the
scale on the right, the root and mode buttons along the top, and the printed chord panel
in the center doing what its icons say. If you know the original, you can play straight
away.

What is different from the first moment, because it makes the board easier to use:
- The **screen** names the chord you just played (`Dm7`, `C/E`) and what the last button
  did (`VOICING II`, `ROOT D`), so the board explains itself as you go.
- **Settings** are in a menu under the five buttons below the screen: press **● Center**
  to open it. See [Settings menu](#settings-menu).
- **▲ / ▼** on the play screen step to the next or previous scale, and **▶ Right** turns
  [hold](#hold) on and off.
- Pads are lit in **rainbow** colors by note and turn white under your finger.
- Pads play from a **lighter touch**, and how light is up to you: Settings → Pads
  ([Pad sensitivity](#pad-sensitivity)).

The original's **Full** chord layout (each chord of the scale with its variations: C, Cadd2,
C6 …) is there too, from 1.3: Settings → Chords → *Chord set* → Full. See
[Full chord layout](#full-chord-layout-optional). For sevenths, sus chords and ninths always
in the same place, try the [Chord grid](#chord-grid-optional) (*Chord set* → Grid).

Everything else (strum feel, voice leading, live notes, note repeat, the drum layout and
so on) is **optional**: nothing changes until you switch it on in the menu or the web
editor. If the board ever behaves unexpectedly, [Troubleshooting](#troubleshooting) lists
the usual causes.

## New in 1.5

**[A step sequencer](#step-sequencer-optional)** *(optional)*: four lanes of steps
that play with the tempo or your DAW, next to a progression. A lane plays chords (chord pads, as a
progression does, so the root buttons transpose it), a melody (the scale by octaves) or drums
(MIDI notes), each with its own length (1–64 steps), rate (any arpeggiator rate, triplets and 5 or
7 per beat too) and channel, so lanes of different lengths drift against each other and meet again.
You edit it on the board: Settings → Sequence → *Steps* turns the right half into the lane's
steps, 12 to a row, and the left half into what they play; tap steps on and off, or record them
one after another. Walk-throughs: [a drum beat and a bass line](#try-steps) ·
[chords by step record](#try-step-record) · [with a DAW](#try-steps-daw).

This is the foundation: a step is on or off and has a value and a velocity, and sounds for half
its length. Gate, ties, probability, ratchets, more patterns and the rest come in later updates,
on the same stored pattern (see the [roadmap](roadmap.md)).

Also new: **[Chords on the right](#chords-on-the-right-optional)**: Settings → Layout → *Chords
on* → Right puts the chord pads on the right half and the melody on the left (for left-handed
playing, or if you'd rather have the melody under your left hand).

Also new: **[Rainbow colors](#screen-and-lights)**: Settings → Lights → *Colors* → Rainbow lights both
halves as one fixed rainbow, red to pink from left to right, whatever you play (user request).
The old *Rainbow* look, the colors by note, is now called *Default*.

Fixed: **pads that stuck on or skipped notes** on boards whose pads rest unevenly. Each pad now
has its own resting level, measured when the board starts playing and followed while it rests
([Pad sensitivity](#pad-sensitivity); the [Pad test](#pad-test) shows it).

Faster: **notes sound sooner after a touch** (user report: 10–30 ms later than with the original
firmware). The board reads its pads about four times as often and plays each pad as soon as it is
read, instead of waiting for all 96: in a simulation of the board, about 6 ms on average instead of
about 16, at most about 8 instead of about 29. Fast repeated hits on one pad now each play (a quick
lift between two hits could be missed before). A note's velocity comes from the higher of the pad's
first two readings, a few milliseconds apart ([Velocity](#velocity)).

Fixed: **the [sustain pedal](#sustain-pedal) did nothing** (user report). Earlier versions listened
on the wrong pin; the board's I/O test showed the right one, the jack's tip. Settings → Pedal →
*Note* now shows only when *Mode* is Note, the only mode that uses it.

## New in 1.4

Three optional features, each with a walk-through you can follow on the board:

- **[Rhythms for the arpeggiator and note repeat](#arpeggiator):** new rates (5 and 7 notes
  per beat, 1/4 and 1/32 triplets, dotted 1/8 and 1/16) and **Euclidean rhythms** (`3/8`:
  3 hits spread as evenly as possible over 8 steps, and every other mix up to 16 steps),
  with a *Rotate* setting. Note repeat gets triplets, 5 per beat, a rhythm of its own and
  **By row**: each row of the melody half repeats at its own speed, so two fingers play a
  2:3 or 3:4 polyrhythm. Walk-throughs: [a 5-against-8 arpeggio](#try-euclid) ·
  [polyrhythms with two fingers](#try-by-row).
- **[Pivot chords](#pivot-chords-optional):** pick a key to move to and the chord pads light
  the chords the two keys share, the best way in pulsing. The screen tells you the chord's
  role in the new key and the way there (`ii IN G: D7>G`). Walk-through:
  [from C to G](#try-pivot).
- **[Chord capture](#chord-capture-optional):** play chords (on the pads or from your DAW or
  a keyboard over MIDI) and the board makes a [chord set](#chord-sets-optional) of them, in
  the key they fit best, ready to play. Walk-through: [capture Am F C G](#try-capture).

Also new: **[Reset options](#resetting-options)** at the end of every settings page puts that
page back to its defaults, and Settings → About → **Factory settings** does every page at once
(presets, scales, chord sets and progressions stay). Every reset asks first and starts on
CANCEL.

Note repeat now keeps to the beat: the first strike sounds at your touch, the next ones
land on the tempo's grid (or the DAW's clock), so repeats on several pads stay together.
A center button can be set to *Pivot*. Going back to an older version, the installer and
the web editor put the new values back to what that version has.

## New in 1.3

Two new ways to lay out the chord pads, both optional: Settings → Chords → *Chord set*.

- **[Chord grid](#chord-grid-optional):** one chord type on each row, the same in every key
  and scale. The bottom row has the scale's triads, then the sevenths, the sus chords, and
  the ninths on top; each column is a step of the scale. A ii–V–I is columns 2, 5, 1 on the
  sevenths row, with no button to hold. Walk-through: [the chord grid in C major](#try-grid).
- **[Full chord layout](#full-chord-layout-optional):** the original TheoryBoard's layout,
  every pad a different chord: each chord of the scale with its variations (C, Cadd9, C6 …),
  lit in its root's color. Walk-through: [Full in C major and A minor](#try-full).

The web editor has **Chord grid on the TB** and **Full layout on the TB** buttons, a
progression recorded on either keeps playing its chords, and backups keep the choice. A
center button set to *Next chord set* steps through the scale's chords, the grid, Full and
your chord sets.

## New in 1.0

Four optional features, each with a short walk-through you can follow on the board:

- **[Chord sets](#chord-sets-optional):** any 12 chords on the chord pads, typed in the web
  editor, for example the key's chords with their borrowed neighbors, or a song's chords
  in order. Walk-throughs: [a set of borrowed chords](#try-chord-set) ·
  [a song's chords, then a progression](#try-set-song).
- **[Theory hints](#theory-hints-optional):** after each chord, the chord pads light the
  chords that usually come next. Walk-through: [hints in C major](#try-hints).
- **[Center buttons and Shift](#center-buttons-and-shift-optional):** give the 14 center
  buttons your own jobs (arpeggiator, sustain, a 7th while held, tap tempo, next preset …),
  and make one a Shift button: Shift + a root button plays a progression, Shift + a mode
  button loads a preset. Walk-through: [set up five buttons](#try-buttons).
- **[Scale lock](#scale-lock-optional):** notes your computer sends come out in the board's
  key and scale. Walk-through: [scale lock from a DAW](#try-scale-lock).

The web editor has a panel for each of the first and third. Backups and the installer's
*Save settings* include chord sets and the buttons.

## The board

```
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  C  C# D  D# E  F  F# G  G# A  A# B │ 5 12 M1 M2 M3 M4 M5 M6 M7 U1 U2 U3  │  top row: 24 buttons
 │            root buttons             │            mode buttons             │
 ├────────────────────────┬────────────┴────────────┬────────────────────────┤
 │                        │        [ screen ]       │                        │
 │   CHORD PADS           │   5 navigation buttons  │   MELODY PADS          │
 │   12 columns x 4 rows  │   14 function buttons   │   12 columns x 4 rows  │
 │   (left half)          │   (center panel)        │   (right half)         │
 └────────────────────────┴─────────────────────────┴────────────────────────┘
```

- **Pads (96):** pressure-sensitive. The left half plays chords, the right half single notes.
- **Root buttons (12):** the key, C to B from left to right.
- **Mode buttons (12):** `5` major pentatonic, `12` chromatic, `M1`..`M7` the seven
  modes of the major scale, `U1`..`U3` your own scales (U4–U12 are in the menu).
- **Navigation buttons (5),** the row just below the screen, left to right:
  **◀ Left**, **▲ Up**, **● Center**, **▼ Down**, **▶ Right**.
- **Chord panel (14 center buttons):** voicing (I, II, III, guitar), spread (I, II, III)
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

The left half plays chords and the right half the melody. To have them the other way round,
see [Chords on the right](#chords-on-the-right-optional); everything in this manual then applies
with "left" and "right" swapped for the pads (not for the buttons).

### Chords on the right *(optional)*

*(1.5.0)* Settings → Layout → *Chords on* → **Right**: the chord pads move to the right half and
the melody pads to the left, with their lights. Everything else stays as it was: the root and mode
buttons along the top, the chord panel and the buttons below the screen don't move, and every
setting keeps working on the half it belongs to (*Chord vel* and *Chord AT* on the chords,
wherever they are). On the step screen the steps are then on the left and what to play on the
right. It's a board setting, like *Arrows*: loading a preset doesn't change it. A pad held while
you switch ends, as if lifted.

**Try it:** Settings → Layout → *Chords on* → Right. Play the right half's bottom-left pad: the C
chord. The left half's bottom row: C D E F G … Back with *Chords on* → Left.

**Why use it:** left-handed playing; a melody under the hand you lead with.

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
Major afterward.

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

**Try it:** Settings → Play → *Scale*, ● Center, ▲/▼ through the list, ● Center on
**BLUES**. The chords and the right half now follow the blues scale.

The web editor's [scale library](#scale-library) is easier for browsing: it groups the
scales, plays them, and finds scales by the notes you play.

### Stepping through scales with ▲ / ▼
On the play screen, **▲ Up / ▼ Down** step to the next or previous scale (`SCALE DORIAN`).

**Try it:** on the play screen press ▲ Up: `SCALE MINOR`; ▼ Down: back.

**Why:** compare scales quickly without opening the menu.
If you'd rather use the arrows for something else (melody octave, presets or tempo),
change Settings → Play → *Arrows*.

### Favorites *(optional)*
Up to 128 scales you pick in the web editor, in your order. Once you have favorites,
▲ / ▼ step through them instead of through every scale (`FAV 3/12 BLUES`).

**Try it:** in the web editor's scale library, add Dorian, Blues and Hirajoshi to
favorites and click **Save favorites to the TB**. On the board, ▲ Up shows
`FAV 1/3 DORIAN`, then `FAV 2/3 BLUES`…

**Why use it:** a set list of scales for a song or a practice session, reachable with
one button.

### User scales (U1–U12) *(optional)*
Your own scales, made in the [web editor](#custom-scales). U1–U3 have buttons; all twelve
are in Settings → Play → *Scale*. Until a slot has a scale, pressing it shows `U1 IS EMPTY`.

Press only one root or mode button at a time; two at once shows `ONE BUTTON AT A TIME`.

## Chord panel

The 14 buttons in the center do what the panel prints next to them. They shape **every
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

## Center buttons and Shift *(optional)*

*(TBOSS 1.0 or later.)* Each of the 14 center buttons can do something other than what
is printed next to it, and one of them can be a **Shift** button that gives the others a
second meaning. Set them in Settings → **Buttons** (one item per button, named by what is
printed next to it; pressing a center button while that page is open jumps to its item) or
in the [web editor](#center-buttons). *Reset all* on that page (confirmed first) puts every button back.
Until you change one, every button does what is printed.

| Action | What the button does |
| --- | --- |
| Voicing I–III, Guitar, Spread I–III, Octave −3…+3 | the panel function of that name (each button's default is its own) |
| Shift | held (or latched): the other buttons, the root and mode buttons and the chord pads do something else (below) |
| Hold, Arpeggiator, Strum, Voice leading, Fold, Hints on/off | turns that on or off; lit while on (the arpeggiator comes back with its last pattern, strum with Up) |
| Next chord set | steps to the [Chord grid](#chord-grid-optional), the [Full chord layout](#full-chord-layout-optional), then through the [chord sets](#chord-sets-optional) that have chords, then back to the scale's own chords; lit while the grid, Full or a set plays |
| Sustain on/off | tap: sends sustain (CC64) on, to the halves set in Settings → Pedal → *Target*; lit while on; tap again: off. A toggle rather than held because the board's touch buttons let go of a touch held for about 5 seconds. *All notes off* also ends it |
| Add 7th, Add 9th, Add 6th, Sus2, Sus4 (while held) | every chord you play while holding it gets that note (or sus); a recorded [progression](#progressions) keeps it |
| All notes off | ends everything the board plays: held and latched chords, the arpeggiator, a playing progression (one following a DAW comes back at its next chord), [scale lock](#scale-lock-optional) notes, a Sustain button that is on |
| Tap tempo | tap twice: the time between the taps sets the tempo (40–240 BPM) |
| Next / Previous preset, Next / Previous scale, Melody octave up / down | as the ▲/▼ shortcuts do |
| Play/stop, Record/finish | the [progression](#progressions): as ▲ and ▼ with *Arrows* Sequencer (Record on the [step screen](#the-step-screen): step record) |
| Nothing | the button does nothing |
| Pivot *(1.4)* | hold it and press a root button: the [pivot](#pivot-chords-optional) target is that key (major, or minor when the current scale has a minor 3rd); tap it alone: Pivot off. Lit while a target is set |

**Held for about 5 seconds at most.** The board's touch buttons treat a touch that lasts
longer as something resting on them and let go of it, so a held action (Shift, the chord
shapes) ends after about 5 seconds even with your finger still there; lift and touch again.
That is why Sustain is tapped on and off, and Shift can be latched.

**Shift.** Hold a Shift button, or tap it twice quickly (within 0.4 s) to latch it (`SHIFT
LATCHED`; it stays lit); tap it once more to let go. While Shift is on:

| With Shift | |
| --- | --- |
| Another center button | its printed panel function, whatever it is set to |
| Root button 1–8 (C … G) | plays progression P1–P8 (`PLAY`); the same one again stops it |
| Mode button 1–8 (5, 12, M1 … M6) | loads preset 1–8 (`NO PRESET THERE` if empty) |
| A chord pad | picks it for Settings → Chords without playing it (`CHOSEN FOR EDITING`) |

A Shift button's own light is on while Shift is. The "while held" chord shapes are paused
while Shift is on.

<a id="try-buttons"></a>**Try it:**
1. Settings → Buttons. Press the **Octave +3** button: the cursor jumps to its item. Set it
   to **Shift**; *Octave +2* → **Add 7th (while held)**; *Spread III* → **Arpeggiator
   on/off**; *Spread II* → **Sustain on/off**; *Octave −2* → **Tap tempo**. Back to the play
   screen.
2. Hold the Octave +2 button and play the bottom-left chord in C: `Cmaj7`. Let go and play
   it again: `C`.
3. Tap Spread III and hold a chord: the arpeggiator runs, the button stays lit. Tap again: off.
4. Tap Spread II: `SUSTAIN ON`, the button lit, chords ring on after you lift. Tap again:
   `SUSTAIN OFF`.
5. Tap Octave −2 twice, about half a second apart: `TEMPO 120 BPM` (or what you tapped).
6. Hold Octave +3 and press Spread III: `SPREAD III`, its printed function. Again to undo.
7. Tap Octave +3 twice quickly: `SHIFT LATCHED`. Press root C♯ (the 2nd root button):
   progression P2 plays (or `NO PROGRESSION YET`); press it again to stop. Mode button `5`
   loads preset 1 (or `NO PRESET THERE`). Tap Octave +3 once: Shift off.
8. Settings → Buttons → *Reset all*, ▲ to RESET, ●.


**Why use it:** the functions you use while playing (arpeggiator, sustain, a 7th on
demand, tap tempo) under your fingers instead of in the menu, and with Shift, presets and
progressions one press away. **When not to:** if you rely on the printed panel (for
example, sharing the board), keep the defaults; *Reset all* brings them back.

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
| Reset chord | ● then confirm: clears this pad's edits (`CHORD EDITS RESET`) | |
| Reset all edits | ● then confirm: clears the edits of every chord pad | |

*Voicing*, *Spread*, *Voice lead* and *Fold* on the same page apply to every chord.

**Try it:** play the bottom-left chord in C major (`C`), open Settings → Chords →
*Add 7th*, set On: the name becomes `Cmaj7` and the pad turns paler. Play the G column's
bottom pad, *Add 7th* On: `G7`. *Reset chord* takes the G back to `G`.

**Why use it:** richer chords exactly where a song needs them, e.g. a G7 that pulls back
to C, or a sus4 that resolves. Edits are saved with [presets](#presets), so each song can
have its own set.

If you have not played a chord pad yet, an edit shows `PLAY A CHORD 1ST`. An edit that
would take a note outside the MIDI range shows `OUT OF RANGE` and is not made. To edit a
chord without playing it, hold [Shift](#center-buttons-and-shift-optional) and touch its
pad. On a [chord set](#chord-sets-optional)'s pads, Add and Sus don't apply (`SET CHORDS: NO
EDITS`): type the chord you want into the set instead. *Strum pad* and *Sound* still work.

## Chord sets *(optional)*

*(TBOSS 1.0 or later.)* Normally the chord pads play the scale's own chords. A **chord
set** puts any 12 chords on the 12 columns of chord pads instead, for example a song's
chords, borrowed chords next to the key's, or jazz sevenths. A column can also be empty
(silent and dark). You type the sets in the [web editor](#chord-sets) (8 sets); on the
board, Settings → Chords → **Chord set** picks one (*Scale* = the scale's chords again), or a
[center button](#center-buttons-and-shift-optional) set to *Next chord set* steps through
them.

- A set is stored relative to the key: the **root buttons transpose it**. A set typed in C
  as `C F G Am` plays `D G A Bm` after pressing root D.
- The **mode buttons** still change the scale of the melody half, the hints and the
  [scale lock](#scale-lock-optional); the chord pads keep the set.
- The rows play the chord's **inversions**: root position at the bottom, then the first,
  second (and third, for four-note chords) inversion upward. The chord panel (voicing,
  spread, octaves, guitar), [voice leading and fold](#voice-leading-and-fold), strum, hold
  and the arpeggiator work as with the scale's chords; the screen names each chord.
- Chords with up to 6 notes; each root sits in the octave above the key's root, or the one
  above that.
- A [progression](#progressions) recorded while a set is on keeps playing that set's chords,
  even after you go back to the scale's chords (until the set is cleared; then it plays the
  pads' scale chords). While a progression records, the set can't change (`NOT WHILE
  RECORDING`), and neither can a preset load; an empty column's pads record nothing.
- The play screen shows `S2` (set 2) next to the mode and preset.

<a id="try-chord-set"></a>**Try it (a set of borrowed chords):**
1. In the [web editor](#web-editor): Chord sets → tab **1** → *Start from* "Pop: the key and
   its neighbors" (`C Dm Em F G Am Bb Ab Fm D7 E7 G7`). **Save to set 1**, then **Use it on
   the TB**. The play screen shows `S1`.
2. Root C. Play the bottom row from left to right: C, Dm, Em, F, G, Am, B♭ (the screen says
   `A#`), A♭ (`G#`), Fm, D7, E7, G7.
3. Play one column from the bottom up: the chord in root position, then its inversions.
4. Press root A: the same set in A (`A Bm C#m D E F#m G F Dm B7 C#7 E7`).
5. Settings → Chords → *Chord set* → Scale: the scale's own chords again.

<a id="try-set-song"></a>**Try it (a song's chords in order, then a progression):**
1. Web editor → Chord sets → tab **2**, *Written in* C, type `C G Am F - Dm7 G7 Cmaj7`, name
   it `SONG`, **Save to set 2**, **Use it on the TB**. Root C: columns 1–4 play C, G, Am, F;
   column 5 is dark and silent; then Dm7, G7, Cmaj7.
2. Settings → Play → *Arrows* → Sequencer. Settings → Sequence → *Progression*: an `EMPTY`
   slot, *Unit* 1 bar. Back to the play screen.
3. ▼ Down, tap the bottom pads of columns 1, 2, 3 and 4, ▼ Down: `PROGRESSION n SAVED`.
4. ▲ Up: C G Am F, a bar each, round and round. Press root G: the same song in G. Root C.
5. Settings → Chords → *Chord set* → Scale: the pads play the scale's chords again, but the
   progression keeps the set's chords. ▲ Up to stop.

**Why use it:** borrowed chords (B♭, A♭, Fm in C) and secondary dominants (D7, E7) right
next to the key's own, without switching modes; one song's chords in the order you play
them. **When not to:** to learn which chords belong to a key, the scale's chords show that
better.

## Chord capture *(optional)*

*(TBOSS 1.4 or later.)* Like *Detect* in chord software: play chords and the board turns
them into a [chord set](#chord-sets-optional). Settings → Chords → **Capture** picks the set
to fill (`SET 2`, or `SET 2 EMPTY`); ● Center starts (`CAPTURE: PLAY CHORDS`). Then:

- **Chords from the pads** count, in any layout, with their edits and held chord shapes, so
  you can build a song's set by playing it on the board. The pads sound as usual.
- **Chords from MIDI in** count too: notes your DAW or a keyboard sends, on any channel. A
  chord is taken at the first note you lift, with all the notes that were down together,
  so a rolled chord counts whole; it needs at least two different notes. Single notes
  (a melody) are ignored. An inversion is named by its root: E G C is C.
- Each new chord fills the next column and the screen shows it (`CAPTURE 3/12 Am7`); the
  same chord twice in a row counts once. The bottom line shows `3/12 <UNDO O SAVE`.
- **◀ Left** takes the last chord back. After 12 chords the set is full (`12 CHORDS: O
  SAVES`).
- **● Center** saves. The board works out the key the chords fit best (major or natural
  minor; a first or last chord on the key's root counts extra) and stores the set relative
  to that key, named `CAPTURE 2`. Then it switches to that key (major on M1, minor on M6)
  and to the set, so the pads play the chords as you played them: `SET 2 IN A MINOR`.
- Saving replaces what the set had. ● with nothing captured just stops (`NOTHING
  CAPTURED`).
- Capture and progression recording can't run at the same time (`NOT WHILE RECORDING`).

<a id="try-capture"></a>**Try it (capture Am F C G from a DAW):**
1. Settings → Chords → *Capture* → `SET 2` (or any set you don't need), ● Center:
   `CAPTURE: PLAY CHORDS`.
2. From the DAW (or a keyboard into it, sent on to TheoryBoard MIDI) play A minor, F major,
   C major and G major, one after another, each held for a moment. The screen counts
   `CAPTURE 1/12 Am` … `CAPTURE 4/12 G`.
3. ● Center: `SET 2 IN A MINOR`. The board is now on root A, M6, set 2.
4. Play the bottom row from the left: Am, F, C, G. Press root D: the same chords in D minor.
5. Try it from the pads too: Settings → Chords → *Capture* → another set, ●, tap a few chord
   pads (an edited one counts with its edit), ◀ Left to drop the last, ● to save.

**Why use it:** a song's chords from a DAW project or a chord chart played in, ready to
play, transpose, arpeggiate or record as a [progression](#progressions); or keep the chords
you found while improvising. **When not to:** to type exact chords (and names), the web
editor's chord sets are quicker.

## Chord grid *(optional)*

*(TBOSS 1.3 or later.)* The scale's chords with a **chord type on each row**, so a pad's
place tells you what it plays, in any key and any scale. The columns are the scale's steps,
as with the scale's own chords. The rows, from the bottom up, are the **triad**, the
**seventh**, the **sus** chord and the **ninth**:

| Row | Columns 1–7 in C major |
|---|---|
| Top: ninths | Cmaj9, Dm9, Em7♭9, Fmaj9, G9, Am9, Bm7♭5♭9 |
| Sus | Csus4, Dsus4, Esus4, Fsus2, Gsus4, Asus4, B–E–F |
| Sevenths | Cmaj7, Dm7, Em7, Fmaj7, G7, Am7, Bm7♭5 |
| Bottom: triads | C, Dm, Em, F, G, Am, Bdim |

Columns 8–12 are columns 1–5 again, an octave higher. Every chord uses the scale's own notes,
so the seventh on the fifth step is G7 in C major and Em7 in A minor, with no theory needed:
bottom row for plain chords, one row up for the jazzier seventh, top row for the richest.

- **Sus:** sus4 (the 4th instead of the 3rd). Where the scale's 4th above the chord's root is
  not a perfect fourth (F in C major has B), the row plays sus2 instead (`Fsus2`). On the
  diminished chord the sus4 keeps its ♭5: B–E–F in C major, shown as `B(11,#11)`.
- **Voicing:** every chord in root position, from the chord octave. Voicing I–III invert
  it, and [voice leading](#voice-lead-optional), [Fold](#fold-optional), guitar, spread,
  octaves, strum, hold and the arpeggiator work as usual.
- **Edits:** a pad's own Add 7/9/6 and Sus don't apply on the grid
  (`CHORD GRID: NO EDITS`); they stay for the scale's chords. A
  [center button](#center-buttons-and-shift-optional) set to Add 9th, Sus2 and so on still
  works while held: hold Add 9th and touch a bottom-row pad for `Cadd9`.
- The **root buttons** change the key and the **mode buttons** the scale; the rows stay the
  same chord types. [Theory hints](#theory-hints-optional) light whole columns, by scale step.
- A [progression](#progressions) recorded on the grid keeps playing the grid's chords. The
  play screen shows `GRID` next to the mode and preset.

<a id="try-grid"></a>**Try it:**
1. Root C, mode M1 (major). Settings → Chords → *Chord set* → **Grid**. The screen says
   `CHORD GRID`.
2. Play the bottom row from left to right: `C`, `Dm`, `Em`, `F`, `G`, `Am`, `Bdim`. Then the
   row above it: `Cmaj7`, `Dm7`, `Em7` …
3. Play column 5 from the bottom up: `G`, `G7`, `Gsus4`, `G9`.
4. A ii–V–I on the second row from the bottom: column 2 (`Dm7`), column 5 (`G7`), column 1
   (`Cmaj7`).
5. Press mode M6 (minor, root C): the same rows in C minor (`Cm`, `Cm7` …), the same places.
   Mode M1 again.
6. Settings → Chords → *Chord set* → Scale: the scale's chords in inversions again.

**Why use it:** sevenths, sus chords and ninths at a touch, with nothing to set up or hold,
and always in the same place. **When not to:** to play one chord in several inversions, the
scale's chords do that (or Voicing I–III here).

## Full chord layout *(optional)*

*(TBOSS 1.3 or later.)* The original TheoryBoard's **Full** layout: instead of the scale's
chords in inversions, every chord pad plays a different chord. Each chord of the scale gets
a run of its variations, and the runs follow the scale from the top-left pad down each
column, then on to the next column. In C major:

| Columns | Chords (top to bottom, column by column) |
|---|---|
| 1–2 | C, Cadd9, C6, C6/9, Cmaj7, Cmaj9, Csus2, Csus4 |
| 3–4 | Dm, Dm6, Dm7, Dm6/9, Dm9, Dsus2, Dsus4, D7sus4 |
| 5–6 | Em, Em7, Em7♯5, Esus4, E7sus4, then F … |
| 6–8 | F, F(♯11), Fadd9, F6, Fmaj7, F6/9, Fmaj9, F6♭5, Fsus2 |
| 8–10 | G, Gadd9, G6, G7, G6/9, Gsus2, Gsus4, G7sus4 |
| 10–12 | Am, Am7, Am9, Am7♯5, Asus2, Asus4, A7sus4, then Bdim, Bm7♭5, Bm7♯5 |

Each run lights in its root's color, so the chords of the scale show as blocks. The other
modes of a scale play the same chords starting from their own: A minor starts with the Am
run, then B, C, D, E, F and G's. The screen names each chord by its notes, so most show
their lowest note too (`Dm/A`), and a few read as another chord with the same notes (Em7♯5
is C D E G: `Cadd9`).

- **Where the chords come from:** the original firmware's own chord lists and voicings,
  still on the TheoryBoard's memory chip. TBOSS only reads them (it never writes that
  chip). A few of the original's entries have a wrong note in one key or another; TBOSS
  plays those as the same chord sounds in the other keys.
- **Scales:** the original had lists for the modes of seven scale families (major, melodic
  minor, harmonic minor, harmonic major, double harmonic, Neapolitan major and minor) and
  for pentatonic, Japanese, blues and bebop scales. A scale it didn't have plays the list of
  a seven-note mode that contains it. Scales with no such mode (the symmetric scales, most
  eight-note scales, some others) get a layout TBOSS builds the same way from common chord
  types; if a scale has fewer chords than pads, they repeat, an inversion higher. A board
  whose memory chip doesn't answer builds every scale's layout this way.
- **Voicing:** each chord sounds in one octave, G♯ to G above the chord octave's C, as with
  [Fold](#fold-optional), and each pad keeps its own voicing (the rows are different chords,
  not inversions). Voicing I–III, guitar, spread, octaves, [voice leading](#voice-lead-optional),
  strum, hold and the arpeggiator work as usual.
- **Edits:** Add 7/9/6 and Sus don't apply to Full chords (`FULL CHORDS: NO EDITS`);
  *Strum pad* and *Sound* still work.
- The **root buttons** change the key; the **mode buttons** change the scale and its Full
  layout. Theory hints follow the chords' roots.
- A [progression](#progressions) recorded in Full keeps playing Full chords. The play
  screen shows `FULL` next to the mode and preset.

<a id="try-full"></a>**Try it:**
1. Root C, mode M1 (major). Settings → Chords → *Chord set* → **Full**. The screen says
   `FULL CHORDS`.
2. Play column 1 from the top down: `C`, `Cadd9`, `C6/A`, `C6/9/A`. Then column 2: `Cmaj7`,
   `Cmaj9`, `Csus2`, `Csus4`.
3. Play the top pad of each column from left to right: the colors show where each chord's
   run starts.
4. Press root A, then mode M6 (minor): A minor's Full layout, the same chords as C major's
   starting from the Am run (`Am`, `Am7`, `Am9` in column 1). Root C, mode M1.
5. Settings → Chords → *Chord set* → Scale: the scale's chords again.

**Why use it:** the colors of a song's chords at hand without editing anything; the
original's layout if that's what your hands know. **When not to:** to play the same chord in
different inversions, the scale's chords do that better.

## Theory hints *(optional)*

*(TBOSS 1.0 or later.)* With Settings → Chords → **Hints** On, after each chord you play
the board lights the chords that usually come next: the most usual one **pulses**, the
other common ones light **steadily**, and the rest of the chord pads dim. The melody half
doesn't change.

In a seven-note scale with its own chords the hints follow the scale steps (in C major:
after C → **F**, G, Am; after Dm → **G**, Bdim; after Em → **Am**, F; after F → **G**, C, Dm;
after G → **C**, Am; after Am → **Dm**, F; after Bdim → **C**, Em). With a [chord
set](#chord-sets-optional) or a scale of another size they follow the root: a fifth down
pulses, a fifth up and a third down light (in a pentatonic key, where there is no chord a
fifth down, those two pulse). While a [progression](#progressions)'s guide lights show,
the hints wait.

<a id="try-hints"></a>**Try it:**
1. Root C, M1, Settings → Chords → *Hints* On.
2. Play the bottom-left C: the F column pulses, G and Am glow, the other chord pads dim. The
   melody half doesn't change.
3. Play Dm: G pulses, Bdim glows. Play F: G pulses, C and Dm glow.
4. *Hints* Off.

**Why use it:** to find your way around a key, write a progression or improvise one; with a
chord set, to see which of its chords lead where. **When not to:** if you know where you
are going, the dimmed pads hide the colors of the scale.

## Pivot chords *(optional)*

*(TBOSS 1.4 or later.)* To change key smoothly (modulate), you go through a chord the two
keys share: a **pivot chord**. Set Settings → Chords → **Pivot to** to the key you want to
reach (Off, C major … B major, C minor … B minor), or hold a center button set to *Pivot*
and press that key's root button. Then:

- The chord pads that are also chords of the new key light up: the ones that lead best
  there (its **ii and IV**, in a minor key its ii and iv) **pulse**, the other shared chords
  glow, and the rest dim. It works in any scale and with the [chord grid](#chord-grid-optional),
  [Full](#full-chord-layout-optional) and [chord sets](#chord-sets-optional), counting each
  pad's edits.
- The play screen shows the target (`PIVOT TO G MAJOR`); after you play a shared chord, its
  role in the new key and the way in, through the new key's V7: `ii IN G: D7>G`.
- **Arriving:** press the target's root button. From M1 or M6 the board also changes to the
  target's major or minor; Pivot turns off: `NOW IN G MAJOR`.
- The pivot lights come before the [hints](#theory-hints-optional); a playing
  [progression](#progressions)'s guide lights come before both.

<a id="try-pivot"></a>**Try it (from C to G):**
1. Root C, M1. Settings → Chords → *Pivot to* → `G MAJOR`. The play screen says `PIVOT TO G
   MAJOR`.
2. The C and Am columns pulse (IV and ii in G), Em and G glow, Dm, F and Bdim dim.
3. Play Am: `ii IN G: D7>G`, the way in: Am, then G's V7 (D7), then G. Play Em: `vi IN G:
   D7>G`. Play F: just `PIVOT TO G MAJOR` (F isn't in G).
4. Press root G: `NOW IN G MAJOR`, the lights go back to normal. Play the 5th column (D) with
   a held *Add 7th* button for the D7, then the bottom-left G: you have changed key.
5. With a center button set to *Pivot*: hold it and press root E while in C major: the
   target is E major (from A minor it would be E minor). Tap it alone: Pivot off.

**Why use it:** to write a key change, or improvise one, without working out which chords
the keys have in common. **When not to:** while you play in one key, the dimmed pads hide
the colors of the scale; set *Pivot to* Off.

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
- **Columns 8–12:** the notes of the **live chord**, from its root upward: a C chord
  gives C E G C E along a row, a Cmaj7 C E G B C. Each row is an octave above the one
  below. The chord's root lights brightest.
- The live chord is the **last chord pad you touched**, and stays when you let go.
  Before you play any chord it is the key's first chord.

**Try it:** root C, M1, *Layout* → Live notes. Play the G chord (5th column, bottom row)
and lift your hand. Columns 8–12 of the right half's bottom row now play G B D G B
(blue, pink, yellow in the rainbow colors). Play Am: they become A C E A C.
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
follows that instead; while the arpeggiator or note repeat is on, the play screen's bottom line
shows `EXT` where the tempo was (`ARP UP 1/16 EXT`). A quarter of a
second after the clock stops, the board goes back to its own *Tempo*.

**Try it:** Settings → Arpeggiator → *Arp* Up, hold a chord: sixteenth notes at 120 BPM.
Settings → Play → *Tempo* → 80: the same pattern, slower. *Arp* Off afterward.
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

Settings → Play → *Note repeat*: **Off**, 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T (triplets),
5/beat, **By row**. It follows Settings → Play → *Tempo* or an incoming MIDI clock. While it
is on, the play screen's bottom line shows `REPEAT 1/16 120`. Chord pads are not affected
(they have the strum *Repeat*).

- *(1.4)* The first strike sounds when you touch the pad; the ones after it land on the
  beat's grid for the rate (half a step later at the earliest), so repeats on several pads,
  and the DAW, stay together.
- *(1.4)* **By row:** each row of the melody half repeats at its own speed: from the bottom
  row up 2, 3, 4 and 5 per beat. Hold pads on two rows for a polyrhythm: bottom and second
  row 2:3, second and third 3:4.
- *(1.4)* **Repeat rhythm** (Settings → Play): *All* strikes on every step; a Euclidean
  rhythm such as `3/8` strikes on its hits and rests on the other steps (see
  [Arpeggiator](#arpeggiator) for what they are).

<a id="try-by-row"></a>**Try it (polyrhythms with two fingers):** *Note repeat* By row. Hold
a pad on the bottom row: two notes per beat. Add a pad on the second row: three per beat
against the two, a 2:3 polyrhythm that meets on every beat. Swap the second for a pad on the
third row: 2:4; third and top: 4:5. Then *Note repeat* 1/16 and *Repeat rhythm* `3/8`: a
tresillo pattern (X..X..X.) under your finger.

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
| Arp rate | note length: 1/4, 1/8, 1/8T (triplet), 1/16, 1/16T, 1/32; *(1.4)* 1/4T, 1/32T, 5/beat, 7/beat, 1/8. and 1/16. (dotted) | 1/16 |
| Arp octaves | 1, 2 or 3: the pattern repeats in higher octaves | 1 |
| Arp gate | how long each note sounds: 25, 50, 75 or 100 % of a step | 50 % |
| Rhythm | eight 8-step patterns shown as `X` (play) and `.` (rest), e.g. `X.XX.XX.`; the pattern keeps moving through the chord on rests. *(1.4)* Then the Euclidean rhythms `1/2` … `15/16`: so many hits over so many steps, spread as evenly as possible (`3/8` = `X..X..X.`) | `XXXXXXXX` |
| Rotate *(1.4)* | 0–15: the rhythm starts that many steps later (`3/8` rotated by 1: `.X..X..X`) | 0 |
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

<a id="try-euclid"></a>**Try it (1.4, a 5-against-8 arpeggio):** *Arp* Up, *Arp rate* 5/beat,
*Rhythm* `3/8`. Hold a chord: five steps to each beat, of which every 8 play 3, so the
accents drift across the beat and come back together every 8 beats. Change *Rotate* to
move where the hits fall. With a DAW sending MIDI clock it stays locked to the DAW's
tempo: *Arp rate* 7/beat against a 4/4 drum loop gives 7 against 4.
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
to any key at once, and the mode buttons **re-color** it (a progression recorded in A
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
3. *Record* → ● Center. The play screen shows `REC P1 0/64`. Recording replaces what the
   slot held.
4. Tap chord pads, one tap per unit. Tapping the **same chord again lengthens it** (two
   taps of Am = Am for two units); a different chord starts the next step. Chords sound as
   you tap them. The screen counts the units and names the last chord (`REC P1 5/64 Am`).
   - **◀ Left** takes back the last unit (`REC 4/64 UNDO`).
   - Change mode between taps: a chord tapped in another mode is borrowed. (Don't change
     the root while recording: every step plays from the root of the moment.) Chord
     edits (Add 7th…) are recorded with the chord: set them before you start, because
     ● Center finishes the recording rather than opening the settings.
5. **● Center** finishes and saves (`PROGRESSION 1 SAVED`).

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
  mode (re-colors), switch voicing or strum, or play other chord pads on top. A pad set
  silent (Settings → Chords → *Sound* Off) is silent in the progression too.
- Picking another slot while it plays starts that progression at once, from its first
  chord (with a DAW: at the song's position). When the web editor talks to the board, the
  progression pauses with everything else and carries on where it was.
- The play screen shows the playing chord's name and `P1 Am > F 3/8`: slot, the chord now,
  the next one, and the unit playing of all the units. Long chord names are left out of
  this line to keep the position on screen (the chord now is also the large name above).

**Guide lights** (Settings → Sequence → *Guide*, On by default): while a progression plays,
the playing chord's pad lights soft white and the next chord's pad pulses in its color,
so you can see what's coming; on the right half the playing chord's notes are bright and
the others dim, which shows the safe melody notes for this moment.

*Clear* (Settings → Sequence → *Clear*, ●) empties the selected slot. Pressing ▲ (or
Transport) with an empty slot shows `NO PROGRESSION YET`.

<a id="try-progression"></a>**Try it (the board):**
1. Root A, mode M6 (A minor). Settings → Sequence → *Progression* 1, *Unit* 1 bar, *Record* ●.
2. Bottom row of chord pads: tap Am, F, F, G (the 1st, 6th, 6th and 7th pads). Screen:
   `REC P1 4/64 G`. Tap G once more by mistake and press ◀ Left: back to 4.
3. ● Center: `PROGRESSION 1 SAVED`. Settings → Play → *Arrows* → Sequencer, back to play.
4. ▲ Up: Am for a bar, F for two, G for one, round and round at 120 BPM; the screen shows
   `P1 Am > F 1/4`, the pads guide you. Play a melody on the bright right-half pads.
5. While it plays, press root C: the same progression in C minor (Cm A♭ B♭). Press M1
   (C major): the same pads now play C, Am and Bdim, the progression re-colored. Press
   root A and M6 again.
6. ▲ Up again: it stops.

<a id="try-borrowed"></a>**Try it (borrowed chords): Am D Dm Am C G E7 Am**, a song that mixes A minor, A Dorian and
A harmonic minor:
1. Root A, M6. First the 7th for E7: play the bottom row's 5th pad (`Em`) and turn
   Settings → Chords → *Add 7th* On (`Em7`). Then *Unit* 1 bar, *Record* ●.
2. Tap Am (bottom row, 1st pad). Press **M2** (Dorian) and tap the 4th pad: `D`. Press
   **M6** again and tap the 4th pad: `Dm`. Tap Am, then C (3rd pad) and G (7th pad).
3. Press **M1** (A major) and tap the 5th pad: `E7`. Press **M6**, tap Am.
4. ● Center, ▲ Up: Am D Dm Am C G E7 Am, with D and E7 exactly as recorded.
5. Afterward, play the 5th pad and Settings → Chords → *Reset chord*.

(Simpler: type it in the [web editor](#progressions-1), which finds the pads and the
borrowed scales for you.)

**Why use it:** a backing to practice a melody over; a song's changes always at hand on
stage; a loop to write a melody to; chords from outside the scale without leaving the
pads; transposing a whole song with one button.
**When not to:** for long, through-composed parts or exact rhythms within a bar, record
into your DAW instead. A progression is chords on a grid of units, not a note sequencer:
it does not record the melody half, pressure, or timing within a unit. For rhythms, a melody
or drums, use the [step sequencer](#step-sequencer-optional).

## Step sequencer *(optional)*

*(1.5.0)* Four lanes of steps, L1 to L4, that play in time with the [tempo](#tempo) or
your DAW, at the same time as a [progression](#progressions). Each lane has its own kind, length,
rate and MIDI channel, so a 5-step bass line in eighths turns against a 16-step drum beat and
lines up with it again every 5 bars (polymeter).

**Walk-throughs:** [a drum beat and a bass line](#try-steps) · [chords by step record](#try-step-record) ·
[with a DAW](#try-steps-daw).

### Lanes

Settings → Sequence. *Lane* picks the lane the items after it set (and the one the step screen
opens on):

| Item | Values | Default |
| --- | --- | --- |
| Lane | L1–L4 (the value shows the lane's kind: `L2 MELODY`) | L1 |
| Kind | Chords / Melody / Drums / Off | Chords |
| Length | 1–64 steps | 16 |
| Rate | a step's length: any [arpeggiator](#arpeggiator) rate, 1/4 … 1/16 … 1/32, triplets, 5/BEAT, 7/BEAT, dotted | 1/16 |
| Channel | AUTO (chords: the chord channel, melody: the melody channel, drums: the drum channel; the value shows which) or 1–16 | AUTO |
| Mute | Off / On | Off |

What a step plays:
- **Chords:** a chord pad and its edits (7th, sus …), as a progression keeps them. The root
  buttons transpose a chord lane and the mode buttons re-color it; the chord panel, strum and
  voice leading apply.
- **Melody:** a scale note, as the right half's Scale layout has it: columns are scale steps,
  rows octaves, the bottom row from the root at the melody octave. That holds whatever the right
  half shows, and the root and mode buttons move the melody too.
- **Drums:** a MIDI note: 36 (the kick on a General MIDI kit; the screen says `C2 36`) at the
  bottom left, up each column of four and on to the right, to 83.

A step sounds for half its length, as hard as you touched the pad that set it (100 for steps
tapped on before any pad; always 100 with *Chord vel* at Fixed 100 for chord lanes, *Melody vel*
for melody and drum lanes). Held chord shapes (Add 7th … on a center button) don't change a
step; the chord panel does.

### The step screen

Settings → Sequence → *Steps*, ●. The screen shows the lane (`L1 CHORDS 16 1/16`: lane, kind,
length, rate), the page and channel (`PAGE 1-48 CH1`), the step you hold or the value picked last
(`PICKED: Am`), what just happened (`STEP 5 ON`), and where the lane is (`PLAYING 5/16`).

- **The right half is the lane's steps,** 12 to a row in reading order: steps 1–12 on the top
  row, then 13–24, 25–36, and 37–48 on the bottom row. **▶ Right** shows steps 49–64 (the top
  row and the first four pads of the second), **◀ Left** goes back. Steps that are on light in the
  color of what they play, the playing step is white, steps past the lane's length are dark, and
  of the steps that are off every fourth (1, 5, 9 …: the beats at 1/16) is a little brighter. The steps
  make no sound.
- **The left half is what to play,** and you hear it: chord pads on a chord lane, scale notes
  on a melody lane, drum notes on a drum lane.
- **Tap a step:** it goes on with what you touched last on the left (until then: the first
  chord, the root, or the kick), or off.
- **Hold a step and touch a left-half pad:** that step plays it.
- **▲ / ▼:** the previous / next lane.
- **◀ Left** on steps 1–48: back to the Sequence page.
- **Step record:** **● Center** (or a center button set to *Record*). The screen says
  `REC: PLAY THE PADS` and the bottom line `REC 1/16 >REST O END`. Each left-half pad you tap
  fills the step at the cursor (it pulses white) and moves on, round the lane; **▶ Right** enters
  a rest (that step off); **◀ Left** takes the last one back. ● again ends it.

### Playing

The lanes play whenever a progression would: while the transport runs (▲ with *Arrows*
Sequencer, a center button set to *Play*, the pedal in *Play/Stop* mode, or Settings → MIDI →
*Transport*) or while a DAW plays and sends its clock. They start together with the progression,
at step 1. With a DAW, each lane's step comes from the song position: Start begins at step 1,
Continue carries on, and moving the DAW's playhead (Song Position Pointer) puts every lane on the
step it would have reached. The lanes stop while a progression is being recorded.

The pattern is saved by itself 3 seconds after your last change, goes into the [settings
backup](#settings-backup), and stays when you reset options or use *Factory settings*. Lanes
sound as themselves: they don't feed the arpeggiator. 1.5.0 has one pattern.

<a id="try-steps"></a>**Try it (a drum beat and a bass line):** the board's chord and melody
channels to a synth, the drum channel (10) to a General MIDI drum kit (in a DAW, a track
listening to the board on all channels with a GM kit, or two tracks).
1. Settings → Sequence → *Lane* L1, *Kind* Drums, then *Steps*, ●. [`L1 DRUMS 16 1/16`]
2. Tap the top row's 1st and 9th pads: the kick on steps 1 and 9. [they light amber]
3. Touch the left half's first column, second pad from the top: a snare (note 38). [`PICKED: D2
   38`] Tap the top row's 5th pad and the second row's 1st (step 13): snares there.
4. ◀ Left. *Lane* L2, *Kind* Drums, *Steps* ●. Touch the left half's second column, second pad
   from the top (42, a closed hi-hat), then tap every other step: the top row's 1st, 3rd, 5th,
   7th, 9th, 11th and the second row's 1st and 3rd.
5. ◀ Left. *Lane* L3, *Kind* Melody, *Length* 5, *Rate* 1/8, *Steps* ●. ● Center (step
   record), then tap the bottom row's 1st, 1st, 5th, 4th and 3rd pads (in C major: C C G F E).
   [the cursor goes round to step 1] ● ends.
6. Settings → MIDI → *Transport* ● (or ▲ with *Arrows* Sequencer). [the beat, and a 5-step bass
   line that turns against it and lines up again every 5 bars]
7. While it plays: press root D (everything but the drums moves up a tone), *Mute* L2 On (the
   hi-hats stop), then *Transport* ● again to stop.

<a id="try-step-record"></a>**Try it (chords by step record):**
1. Settings → Sequence → *Lane* L4 (Chords), *Length* 4, *Rate* 1/4, *Steps* ●.
2. ● Center. Tap the bottom row's 6th, 4th, 1st and 5th pads (in C major: Am F C G), one chord
   per step; ● ends. [steps 1–4 light in their chords' colors]
3. Hold step 2 (the top row's 2nd pad) and touch the 2nd pad of the bottom row (Dm): step 2 is
   Dm now. Tap step 4: off.
4. *Transport* ●. [Am Dm C, a beat each, then a beat of rest, round and round] Press *Voicing* II on
   the chord panel: the steps play the new voicing.

<a id="try-steps-daw"></a>**Try it (with a DAW):** the DAW sending MIDI clock to the board (in
Ableton Live: the board's *Out* port with *Sync* on), a pattern as above.
1. Start the DAW at bar 1. [the lanes start at step 1 with it, in its tempo]
2. Stop, move the playhead to bar 3 beat 2, start again. [each lane is where it would have been:
   with 16 sixteenths, step 5]
3. Stop the DAW. [the lanes stop, nothing left sounding]

**Why use it:** a drum beat or a bass line under what you play on the pads; rhythms that a
progression can't make (a chord every eighth, rests); patterns in odd lengths and rates that
change against each other; a sketch that follows your DAW's song position.
**When not to:** for parts with exact lengths, slides or many notes at once, use your DAW. This
first version has one gate length (half a step), no ties or probability, and one pattern.

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
| Pitch bend | bends up as you press harder (the strongest held pad of that half), back to the center on release |

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
pads always brings the value back to 0 (pitch bend back to exactly the center), however
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
loud each new note is (its MIDI velocity, 1–127) from how hard the pad is pressed as the note
starts (1.5.0: the higher of its first two readings, a few milliseconds apart). *(0.11.0: one setting per half; before, one Velocity setting for both.
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

> **Fixed in 1.5.** Before 1.5 the board listened on the wrong pin and the pedal did nothing.
> Settings → About → *I/O test* shows whether the board sees the pedal (`PEDAL PA10 ... DOWN`
> while pressed); if your pedal doesn't work, please report what it shows.

Plug a sustain pedal (footswitch) into the pedal jack **before switching the board on**.

- Settings → Pedal → *Mode* (**Off** by default):
  - **Sustain:** sends sustain (CC64) on the chord and melody channels; the screen shows
    `SUS` while it is down.
  - **Hold:** the board's own [hold](#hold) while the pedal is down: chords keep sounding,
    and the arpeggiator keeps running.
  - **Sostenuto / Soft / Portamento:** sends CC66, CC67 or CC65 instead.
  - **Play/Stop:** each press sends Start, then Stop.
  - **Note:** plays a note while the pedal is down, on the *Drum ch*: Settings → Pedal →
    *Note* (default C2 = 36, a General MIDI kick: a kick drum on your foot). *Note* is on the
    page only while *Mode* is Note.
  - **Off:** the pedal is ignored.
- Settings → Pedal → *Type*: **Auto** (default: whatever the pedal reads at power-up
  counts as "released", so both pedal types work; that's why it must be plugged in
  first), **Normal** or **Inverted** if the pedal acts backward.
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
board settings (brightness, colors, MIDI in lights, TRS out, pedal, arrows) are not part
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

## Scale lock *(optional)*

*(TBOSS 1.0 or later.)* Notes your computer sends to the board on one MIDI channel come
back out of the board **moved to the nearest note of the board's key and scale**, on the
melody channel (and the TRS jack). Set the channel in Settings → MIDI → **Scale lock** (Off
by default). A note exactly between two scale notes goes to the lower one. Each note keeps
the pitch it was given until it ends, even if you change the key meanwhile; the next note
follows the new key. Up to 16 notes at once; the velocity is the incoming one.

<a id="try-scale-lock"></a>**Try it** (not yet confirmed on a board: a [test report](https://github.com/armandhammer/TBOSS/issues/new?template=test_report.yml)
is very welcome): Settings → MIDI → *Scale lock* CH 16. In the DAW, a MIDI track whose output is
the TheoryBoard on channel 16, and an instrument track listening to the TheoryBoard. Root
C, M1: play C♯ on your keyboard (or in a clip): C sounds; F♯ plays F. Press M6 (A minor):
the same keys now play the notes of A minor.
**Why use it:** play any keyboard, an arpeggiator or a generative MIDI tool and stay in the
key the board shows; change key and scale on the board while the music plays.
**When not to:** if the DAW sends the board's own output back to it on the lock channel,
notes repeat; use a channel nothing else sends on (16 is a good choice).

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
editor has talked to it), it measures how high each untouched pad rests and keeps that pad's
*Release* at least 130 above it, raising its *Trigger* and *Full* with it if needed. A pad that
rests above *Release* would otherwise never count as let go. While a pad rests, the board keeps
following its level, so a board that flexes (moved from a desk to your lap) keeps working; a
press is never taken for rest. Your settings are the least every pad gets: pads that rest low
keep your light touch, and only the pads that rest high (a dent, a sensitivity modification, a
thick overlay) need a firmer one. The [Pad test](#pad-test) page shows each pad's resting level,
and a `!` when the pad you touch plays with raised values. *(1.5.0; before, one level for the
whole board, so a few pads resting high could stick on or skip notes.)*

**Try it (find your own Trigger):**
1. Settings → Pads → *Pad test* → ● Center. Hands off: the three pad numbers are your
   resting readings, the last number on each row that pad's resting level, and the bottom line
   `REST` the lowest and highest the board measured.
2. Touch a pad as lightly as you'd like to play. Its number appears at the top of the
   list; `PEAK` shows how far your touch went (say `PEAK 900`).
3. ◀ Left back to the list, ▼ Down to *Trigger*, ● Center, then ▼/▲ to a little below
   your light touch's peak (here 800) and ● Center to set it. ▼ Down to *Pad test* and
   ● Center to open it again.
4. Back in *Pad test*, touch lightly again: the `*` appears and the note plays, at a low
   velocity (`LAST VEL`).
5. Press a pad firmly and watch the second and third rows: those are its neighbors. They
   must stay below *Trigger*, or they would play by themselves. If they reach it, raise
   *Trigger* until they don't.
6. Press as hard as you'd normally play your loudest notes and read `PEAK`. Set *Full* a
   little below that, so your loud notes reach velocity 127.

**Why use it:** match the board to your touch: light and fast for melodies, firmer if you
rest your fingers on the pads or play with a heavy hand. Boards differ, and a soft overlay
or a hardware modification changes how a press spreads; these settings let every board
play its best. They belong to the board: presets don't change them.

**When not to:**
- Don't lower *Trigger* below where your neighbors read when you press firmly
  (step 5): pads next to the one you play would start sounding by themselves.
- Don't lower *Release* to near your resting level: notes may not end reliably (the board
  keeps it at least 130 above rest, but the extra margin helps).
- **With a sensitivity modification or a thick overlay**, start with *Trigger* around
  1900 and check step 5. A modification that presses the membrane closer also spreads each
  press to the neighbors.

### Pad test

Settings → Pads → *Pad test* shows what the pads measure, as numbers: how hard a touch
reads, how much a press spreads to the neighboring pads, and the velocity a note got. It
changes nothing; the pads keep playing while the page is open. *(0.9.1; on the About page
until 0.10.0.)*

```
PAD 1200/600/3500          Trigger / Release / Full as the highest pad plays them
                           (! after them = raised for that pad's resting level)
L C3 R2  2734 *  420       the pad reading highest now, its reading, * = it is playing,
                           and its resting level
L C4 R2   812    385       the second and third highest: a press spreading to its
L C2 R2   640    402       neighbors shows up here
PEAK 3120 L C3 R2          the highest reading of the current (or last) touch
LAST VEL 64 L C3 R2        the velocity the newest note got, and its pad
<BACK  REST 290-470        the lowest and highest resting level of the pads
```

`L C3 R2` means left half, column 3, row 2 (columns 1–12 from the left, rows 1–4 from the
top; the right half is `R`). The numbers above are an example: every board, and every pad,
reads a little differently.

**Try it:**
1. Settings → Pads → *Pad test* → ● Center.
2. Hands off: the three readings are small, each close to its pad's resting level at the end
   of the row.
3. Rest a fingertip very lightly on one pad without playing it: its number rises but stays
   below *Trigger*, no `*`, no sound. That's the gap between touching and playing.
4. Press the pad gently until it plays: the `*` appears, `PEAK` shows how far you went,
   and `LAST VEL` the velocity of that note.
5. Press it firmly: higher `PEAK`, higher `LAST VEL`. Watch the second and third rows: the
   neighboring pads' numbers rise a little as well. As long as they stay below *Trigger*
   they never play.
6. ◀ Left or ● Center leaves the page.

**Why use it:** to set the [Pad sensitivity](#pad-sensitivity) from your own numbers, to
compare pads (a pad that needs much more force than the others, or that reads high when
untouched, shows up straight away), to check how much a pad overlay or a hardware
modification spreads a press to the neighbors, and to tell us real readings when you
report a sensitivity problem.
**When not to:** it's a measuring page, not a setting: nothing you see here changes how the
board plays.

## Settings menu

Press **● Center** on the play screen to open the settings. They are pages of items:

| Button | Page list | A page | Changing a value |
| --- | --- | --- | --- |
| ◀ Left | back to the play screen | back to the page list | cancel, keep the old value |
| ▲ Up / ▼ Down | previous / next page | previous / next item | higher / lower value |
| ● Center | open the page | change the item (or start a test) | set the value |
| ▶ Right | open the page | change the item | set the value |

- **Leaving:** only ◀ Left leaves, one level per press. Shortcut: **hold ◀ Left and press
  ● Center** to go straight to the play screen from anywhere.
- The settings reopen on the page you left. Up and Down wrap around in lists; values stop
  at their lowest and highest setting.
- The top line names the page and the position (`3/7`); the value being changed is shown
  inverted. The bottom line shows what the buttons do, or the result of the last action.
- While the settings are open the root and mode buttons and the chord panel still work.

**Try it:** ● Center (page list), ▼ Down to *STRUM*, ● Center (the Strum page), ● Center on
*STRUM* (the value turns inverted), ▲ Up to *UP*, ● Center (set). ◀ Left twice: back to
playing.

<a id="resetting-options"></a>**Resetting options** *(1.4)*. The last item of every page that
has options (all but Presets, Buttons and About) is **Reset options**: it puts every item on
that page back to its default (the table below). Settings → About → **Factory settings** does
all the pages at once, and also the key, the panel's octaves, the chord edits and the center
buttons. Neither touches what you saved: presets, user scales, favorites, chord sets,
progressions and the step pattern stay. On the Chords page the chord edits stay (*Reset all edits*
clears them), and on the Sequence page the progressions and the step pattern's lanes do
(*Progression* goes back to slot 1).

Every reset asks first, so none happens by accident:
1. ● on the item opens a confirmation, for example `RESET PLAY OPTIONS?`, `13 SETTINGS BACK
   TO THEIR DEFAULTS. YOUR CHANGES ARE LOST`, with **CANCEL** selected.
2. ● now (or ◀ Left, or ▶ Right) changes nothing: `NOTHING WAS RESET`.
3. To reset, press **▲ Up** to select **RESET** (`O RESETS   <CANCEL` at the bottom), then **●**:
   `PLAY OPTIONS RESET`.

The same confirmation is on *Reset chord*, *Reset all edits* (Chords) and *Reset all*
(Buttons). *Reset options* and *Factory settings* wait while a progression is recording (`NOT
WHILE RECORDING`).

**Try it:** Settings → Arpeggiator: *Arp* Up, *Arp rate* 1/8. ▼ to the last item, *RESET
OPTIONS*, ● (`RESET ARP OPTIONS?`, CANCEL selected), ● again: `NOTHING WAS RESET`, still Up
at 1/8. Once more: ●, ▲ (RESET), ●: `ARP OPTIONS RESET`, *Arp* Off, *Arp rate* 1/16.

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
| | Note repeat *(opt)* | Off / 1/4 / 1/8 / 1/16 / 1/32 / 1/8T / 1/16T / 5/beat / By row ([Note repeat](#note-repeat)) | Off |
| | Repeat rhythm *(opt)* | All / the Euclidean rhythms `1/2` … `15/16` | All |
| | Tempo | 40–240 BPM: arpeggiator, note repeat, strum in note values, clock out ([Tempo](#tempo)) | 120 |
| | Arrows | what ▲/▼ do on the play screen: Scale / Melody octave / Preset / Tempo / Sequencer (▲ plays or stops the [progression](#progressions), ▼ records or finishes) (a board setting: presets don't change it) | Scale |
| | Melody oct | −2 … +2 | 0 |
| Chords | Chord set *(opt)* | Scale / 1–8 (`EMPTY` until typed in the web editor) ([Chord sets](#chord-sets-optional)) / Full ([Full chord layout](#full-chord-layout-optional)) / Grid ([Chord grid](#chord-grid-optional)) | Scale |
| | Capture *(opt)* | Set 1–8: ● starts capturing chords into that set ([Chord capture](#chord-capture-optional)) | |
| | Voicing | Off / I / II / III / Guitar (the panel's) | Off |
| | Spread | Off / I / II / III (the panel's) | Off |
| | Voice lead *(opt)* | Off / On ([Voice lead](#voice-lead-optional)) | Off |
| | Fold *(opt)* | Off / On ([Fold](#fold-optional)) | Off |
| | Hints *(opt)* | Off / On ([Theory hints](#theory-hints-optional)) | Off |
| | Pivot to *(opt)* | Off / C major … B major / C minor … B minor ([Pivot chords](#pivot-chords-optional)) | Off |
| | Add 7th, Add 9th, Add 6th | Off / On (last chord played) | Off |
| | Sus | Off / SUS2 / SUS4 (last chord played) | Off |
| | Strum pad | On / Off (last chord played) | On |
| | Sound *(opt)* | On / Off (last chord played): Off picks the live notes without a sound | On |
| | Reset chord / Reset all edits | ● asks, then clears the last chord's / every chord pad's edits | |
| Layout | Layout | Scale / Chromatic / Fourths / Drums / Live notes | Scale |
| | Chords on | Left / Right: which half the chord pads are on ([Chords on the right](#chords-on-the-right-optional); a board setting: presets don't change it) | Left |
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
| | Arp rate | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/4T, 1/32T, 5/beat, 7/beat, 1/8., 1/16. | 1/16 |
| | Arp octaves | 1–3 | 1 |
| | Arp gate | 25 / 50 / 75 / 100 % | 50 % |
| | Rhythm | 8 patterns: `XXXXXXXX`, `X.X.X.X.`, `.X.X.X.X`, `XX.XX.X.`, `X..X..X.`, `XXX.XXX.`, `X.XX.XX.`, `X...X...`; then the Euclidean `1/2` … `15/16` (up to 8 steps shown with their pattern) | `XXXXXXXX` |
| | Rotate | 0–15 | 0 |
| | Swing | 0, 10, 20, 30, 40, 50 % | 0 % |
| | Sync start | Off / On | Off |
| | Latch | Off / On | Off |
| MIDI | Chord ch | 1–16 | 1 |
| | Melody ch | 1–16 | 1 |
| | TRS out | Off / On | On |
| | Clock out *(opt)* | Off / On | Off |
| | Transport | ● Play / Stop (also plays the [progression](#progressions)) | Stopped |
| | Scale lock *(opt)* | Off / CH 1–16: notes arriving on that channel play in the key ([Scale lock](#scale-lock-optional)) (a board setting) | Off |
| Pedal | Mode | Off / Sustain / Hold / Sostenuto / Soft / Portamento / Play/Stop / Note | Off |
| | Type | Auto / Normal / Inverted | Auto |
| | Note | any MIDI note; shown only while *Mode* is Note | C2 (36) |
| | Target | Both / Chords / Melody: whose channel the pedal's controller goes to | Both |
| Presets | Load preset | 1–8 | |
| | Save preset | 1–8 | |
| Lights | Brightness | 1–8 | 4 |
| | Colors | Default / Root+5th / Rainbow | Default |
| | MIDI in LEDs | Off / On | On |
| | LED test | Start | |
| Sequence | Progression | 1–8 (`EMPTY` when nothing is recorded there): the slot ▲ plays and *Record* records ([Progressions](#progressions)) | 1 |
| | Unit | 1 beat / ½ bar / 1 bar / 2 bars: one tap's length | 1 bar |
| | Record | ● starts recording into the slot (replacing it); ● again finishes | |
| | Clear | ● empties the slot | |
| | Guide | Off / On: guide lights while a progression plays | On |
| | Steps | ● opens the step screen ([Step sequencer](#step-sequencer-optional)) | |
| | Lane | L1–L4: the lane the next items and the step screen work on | L1 |
| | Kind | Chords / Melody / Drums / Off | Chords |
| | Length | 1–64 steps | 16 |
| | Rate | a step's length: the arpeggiator's rates | 1/16 |
| | Channel | AUTO (the kind's channel) / 1–16 | AUTO |
| | Mute | Off / On | Off |
| Pads | Trigger | 500–3000 in steps of 100: a note starts here ([Pad sensitivity](#pad-sensitivity)) | 1200 |
| | Release | 300–2500: the note ends below this; pressure counts from here | 600 |
| | Full | 1500–4000: velocity 127 and full pressure | 3500 |
| | Pad test | Start: the pad readings, the peak of a touch, the last note's velocity ([Pad test](#pad-test)) | |
| Buttons | Voicing I … Octave +3 *(opt)* | what that center button does ([Center buttons and Shift](#center-buttons-and-shift-optional)) (board settings: presets don't change them) | its printed function |
| | Reset all | ● asks, then every center button back to its printed function | |
| About | Version | the TBOSS version, e.g. `1.5.0 BETA` | |
| | I/O test | Start: shows the pedal input and two alternative inputs live, sends middle C to the TRS jack every second, and shows pads that read as held | |
| | Factory settings | ● asks, then every option back to its default; saved presets, scales, chord sets and progressions stay ([Resetting options](#resetting-options)) | |
| Every page but Presets, Buttons, About | Reset options | ● asks, then that page's items back to their defaults | |

### Shortcuts

On the play screen:

| Button | Shortcut |
| --- | --- |
| ● Center | Open the settings |
| ▶ Right | Hold on/off |
| ▲ Up / ▼ Down | Set in Settings → Play → *Arrows*: **Scale** (default): next / previous of your [favorite scales](#favorites-optional), or with none, of all scales including your U1–U12, skipping empty slots; **Melody octave**: melody half an octave up / down; **Preset**: next / previous saved preset; **Tempo**: 5 BPM faster / slower; **Sequencer**: ▲ plays / stops the [progression](#progressions), ▼ starts / finishes recording |
| Hold ◀ Left + ▲ Up / ▼ Down | Previous / next saved preset (whatever *Arrows* is set to; not while recording a progression) |
| ◀ Left | Nothing (so pressing it once too often on the way back is harmless); while recording a progression, takes back the last unit |
| ● Center while recording | Finishes the progression and saves it |
| ◀ Left / ● Center while capturing | Takes back the last chord / saves the [captured set](#chord-capture-optional) |
| Shift + root button 1–8 | Plays progression P1–P8; again: stops ([Shift](#center-buttons-and-shift-optional)) |
| Shift + mode button 1–8 | Loads preset 1–8 |
| Shift + chord pad | Picks the chord for Settings → Chords without playing it |

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
   The number is the tempo, `EXT` while a DAW's MIDI clock comes in. `USB: WAITING` until a computer is connected.

Pad colors (Settings → Lights → *Colors*):

**Default** (called *Rainbow* before 1.5): every pad shows the color of its note, the same color for the
same note everywhere on the board (C red, D yellow, E green, G blue, A violet, B pink and
the colors between). The colors move when you change root or mode. Root columns are
brightest.

| Pad | Meaning |
| --- | --- |
| White, brighter the harder you press | you are touching it |
| Soft white | sounding without your finger: held, latched, or feeding the arpeggiator |
| Paler color | a chord pad with edits |
| Dim | a chord pad set silent (*Sound* Off) |
| White (not touched) | notes arriving over MIDI |
| Soft white / pulsing chord pad, bright and dim melody pads | a progression's guide lights: the chord now, the next chord, its notes ([Progressions](#progressions)) |

In the chromatic and fourths layouts, notes outside the scale are dimmed. In the drums
layout the drum grid is amber, its kick pad brightest. In Live notes, the live chord's
root is brightest.

**Root+5th**: root orange, fifth blue, other steps dim, edited chords purple, pressed
pads white, notes arriving over MIDI green. **Why:** some players find it easier to find
the root and fifth at a glance.

**Rainbow** *(1.5.0)*: both halves always look like C in mode 12: every row runs red, orange,
orange-yellow, yellow, green, green-blue, light blue, blue, dark blue, purple, magenta, pink
from left to right, the red column brightest, whatever root, mode or layout you play. A pad
you touch still turns white, and a sounding pad (held, latched) soft white. Nothing else
lights: no theory hints, pivot chords, progression guide or MIDI in. The step screen and the
web editor's preview keep their own colors. **Why:** just for fun, a board that looks the
same whatever you play.

**While the web editor edits a scale**, the whole board becomes a keyboard of the scale:
both halves show 12 columns, one per semitone from the editor's root. The root column is
white, scale notes bright, other notes nearly dark. The screen shows `WEB EDITOR`, the
slot (`U2`), the root and the notes. The editor sends this only while you work in its
scale library or custom scales: using any other panel, switching away from the page, or 2
minutes without touching those two panels ends it, and the play screen and normal colors
return (within 5 seconds if the editor was closed).

A pad that already reads as pressed when the board starts (held, or a pad stuck on your
board) lights white and stays silent until it has been released once; everything else
plays normally. Settings → About → *I/O test* shows how many pads read as held and where
(`PADS HELD 1 L C10 R2`: left half, column 10, row 2 from the top); *Pad test* shows the
readings themselves ([Pad test](#pad-test)).

*Brightness* (1–8) sets all lights. *LED test* lights every pad red, green and blue, then
each pad and each button light in turn: **Try it** if a pad or button light seems dead.

## Web editor

The web editor runs in **Chrome or Edge** on a computer (Safari and Firefox lack the MIDI
features it needs). It browses the scale library, keeps your favorite scales on the
board, makes custom scales, types in chord progressions, and backs up the board's settings.

### Start it
Open **https://armandhammer.github.io/TBOSS/editor.html** in Chrome or Edge. Nothing to install.

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
  the board's colors in the chosen **Key**.
  - **▶ Play** plays it on the computer.
  - **Play it on the TB** switches the board to it, keeping the board's key.
  - **Add to favorites** puts it on your favorites list (below).
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

### Favorites
Up to 128 scales (built-in or your own U1–U12) in the order the board's ▲/▼ step through
them. Reorder by dragging a row by its ⠿ grip (or with its ▲ ▼ buttons), remove one with
✕ or all with **Remove all**, then **Save favorites to the TB**.

**Try it:** add Major, Dorian and Blues, drag Blues to the top, **Save favorites to the
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
4. While you edit, the board's pads show the scale (root white, scale notes in color,
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

- A progression recorded on the board with a [chord set](#chord-sets-optional) is listed
  from that set below the text box, but not as text: typed chords can only use the scale's
  chords, so typing replaces it (and Save is off until you type).
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

### Chord sets
*(TBOSS 1.0 or later.)* Type the 12 chords of a [chord set](#chord-sets-optional).

1. Choose a set tab, **1** to **8** (the tab shows its name). A stored set appears as text.
2. Pick the key the chords are **Written in** (it only names them: on the board the root
   buttons transpose the set). *Start from* offers four examples (pop, jazz, blues, minor).
3. Type up to 12 chords separated by spaces, one per column from left to right: `C Dm Em F
   G Am Bb Ab Fm D7 E7 G7`. `-` leaves a column empty. A chord's root sits in the octave
   above the key's root; `'` after a chord (`G7'`) puts it an octave higher. The chord
   types are the [progressions'](#progressions); slash chords are not taken (the rows play
   the inversions). Below, each column shows its chord.
4. **Name** it (up to 12 characters, shown on the board), **Save to set 1**, then **Use it on
   the TB** (or Settings → Chords → *Chord set*). **Back to the scale's chords** turns it off;
   **Chord grid on the TB** plays the [Chord grid](#chord-grid-optional) and **Full layout on
   the TB** the [Full chord layout](#full-chord-layout-optional) instead.
   **Clear set 1** empties the slot (not while the board plays that set).

**Why use it:** a song's chords, a jazz palette or a key with its borrowed chords on the
pads in a minute, kept with the board's settings.

### Center buttons
*(TBOSS 1.0 or later.)* Choose each center button's action from a list, named by what is
printed next to the button (see [Center buttons and Shift](#center-buttons-and-shift-optional)
for every action and for Shift). Changed buttons are highlighted. **Save to the TB** sends
them (the board uses them at once); **Printed functions** sets every button back (then
save). On the board the same list is Settings → Buttons. The panel shows the buttons as
they were when you clicked *Connect*; saving changes only the ones you changed on the page,
so a button changed on the board meanwhile keeps its action (and the panel then shows it).

### Settings backup
- **Download settings backup** saves the current settings, all 8 presets, the user scales,
  your favorites, your progressions and your chord sets to a `.json` file.
- **Restore from file…** writes them back (user scales first, then chord sets, favorites,
  presets, progressions, then the current settings). Into an older TBOSS, settings it
  doesn't have go back to their defaults, progressions are left out before 0.11.0 and chord
  sets before 1.0.0.

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
back at power-up, together with presets, user scales, favorites, progressions, chord sets and the
step pattern (each change to the pattern is saved 3 seconds after the last one, too). Installing firmware
clears them: the [browser installer](flashing.md) saves them first and puts them back
afterward (step 2 on its page); with the SWD tool, use the web editor's settings backup
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
| `NOT WHILE RECORDING` | A progression is recording, and it keeps one chord set: finish it first (▼ or ● Center), then change the set or load the preset. |
| `NOTHING RECORDED` | A recording finished with no chords in it, so the slot kept what it had. Tap chord pads after starting to record. |
| `NOT SAVED: RESTART` | The board's settings memory filled up during a very long session (hundreds of saves). What you did still plays; switch the board off and on (it tidies the memory as it starts) and save again. |
| A progression plays the wrong chords | It plays in the board's current key and mode: set the root and mode it was recorded or typed in. Borrowed chords keep their own scale. |
| A progression doesn't start with the DAW | The DAW must send MIDI clock (with Start/Stop) to the TheoryBoard MIDI port. ▲ (Arrows: Sequencer) also starts it. |
| A progression makes no chords, only single notes | The arpeggiator is on: it arpeggiates the progression. Settings → Arpeggiator → *Arp* → Off. |
| The pedal sustains the wrong half, or both | Settings → Pedal → *Target*; it can only pick one half when *Chord ch* and *Melody ch* differ. |
| All lights dim or flicker together at random | The USB port isn't supplying steady power. Use the board's DC power input, or try another USB port, a short cable or a powered hub. |
| Pads play nothing for a second | The web editor just talked to the board; it resumes after 1 s. Held notes end, pressure and pitch bend return to rest, and a running transport pauses and continues. |
| Light touches don't play | Lower Settings → Pads → *Trigger* ([Pad sensitivity](#pad-sensitivity)); *Velocity* (Light/Firm) only changes how loud notes are. [Pad test](#pad-test) shows how far your light touches reach. |
| Neighboring pads play by themselves when you press one | Raise Settings → Pads → *Trigger* above what the neighbors read in [Pad test](#pad-test) (around 1900 with a sensitivity modification or a thick overlay). |
| Notes don't end when you lift your finger, or a pad never plays | The board raises each pad's *Release* above where that pad rests (1.5.0), so this is rare: [Pad test](#pad-test) with hands off shows a high rest at the end of that pad's row, or `!` while you press it; raise *Release* (and *Trigger*). |
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

How TBOSS (1.0) compares with the TheoryBoard's own firmware: the features it was sold
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
| "840 scales" (as advertised) | 🟡 86 built-in scales plus your 12, each in all 12 keys; families on M1–M7, favorites, and the editor finds scales by their notes |
| Voicing I / II / III and guitar voicing | ✅ Guitar voicing on six real strings, with a capo ([Chord panel](#chord-panel)) |
| Spread I / II / III (root down 1–3 octaves) | ✅ |
| Octave V: raise, lower or stack octaves | ✅ Tap to turn octaves on and off; lit octaves stack |
| Panel buttons light up to show what is on | ✅ |
| Live Note Update (the right half plays the notes of the chord you hold) | ✅ [Live notes](#live-notes-optional) layout (0.8.0) |
| Octave folding (chords kept inside one octave) | ✅ Settings → Chords → *Fold* (0.9.0), off by default |
| Strum up / down | ✅ Plus up+down, strum times in ms or note values ([Strum](#strum)) |
| 96 velocity-sensitive pads with polyphonic aftertouch | ✅ Aftertouch set per half: poly, channel, CC1/2/11/74 or pitch bend; velocity curves |
| RGB pad lights | ✅ Color by note (rainbow), root+5th or a fixed rainbow; touched pads light white |
| USB MIDI | ✅ |
| TRS MIDI out (Type A) | ✅ Also without a computer ([TRS MIDI out](#trs-midi-out)) |
| Sustain pedal input | ✅ Sustain, sostenuto, soft, portamento, hold, play/stop or a note (1.5: sustain and note confirmed on a board) |
| Firmware updates over USB with the update utility | ✅ From the browser, keeping your settings ([flashing.md](flashing.md)) |

### Promised in 2020, never delivered

| Promised for the TheoryBoard | TBOSS |
| --- | --- |
| Performance mode (save chords in a designated area to recall for songs) | ✅ [Chord sets](#chord-sets-optional) put any chords on the pads (1.0); [progressions](#progressions) keep a song's chords in order; [presets](#presets) save the whole setup |
| Change color palettes | 🟡 Three schemes (by note, root+5th, fixed rainbow); your own colors are planned |
| Bass mode (doubles notes so they can be played with rapid fire in tandem) | ❌ Not yet |
| Arpeggiator | ✅ 6 patterns, rate, octaves, gate, 8-step rhythms, swing, sync start, latch, follows MIDI clock ([Arpeggiator](#arpeggiator)) |
| Step sequencer (chords and/or single notes) | ✅ [Progressions](#progressions) (0.11.0), and the [step sequencer](#step-sequencer-optional) (1.5): four lanes of chords, single notes or drums, played in time with the transport or a DAW |
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
| Web editor: scale library, find scales by notes, favorites, custom scales, typed chord progressions, settings backup | ✅ |
| Chord progressions with borrowed chords, live transposition and guide lights | ✅ (0.11.0) |
| Chord sets: any chords on the chord pads, typed in the web editor | ✅ (1.0) |
| Theory hints: the usual next chords light up | ✅ (1.0) |
| Scale lock: MIDI from the computer played in the board's key | ✅ (1.0) |
| Center buttons you assign (arpeggiator, sustain on/off, chord shapes while held, tap tempo …) and a Shift button | ✅ (1.0) |
| Euclidean rhythms and tuplet rates (5 and 7 per beat) for the arpeggiator and note repeat; polyrhythms by row | ✅ (1.4) |
| Pivot chords: the way to a new key lit on the pads | ✅ (1.4) |
| A step sequencer of four lanes (chords, melody, drums), each with its own length and rate, edited on the pads | ✅ (1.5) |
| Chord capture: a chord set from chords played on the pads or over MIDI, in the key they fit | ✅ (1.4) |
| Browser installer that keeps your settings | ✅ |
| 8 presets of the whole setup, switched from the play screen | ✅ |
| Chromatic and fourths layouts for the melody half | ✅ |
| Chord names on the screen, per-pad chord edits (7th, 9th, 6th, sus) | ✅ |
| Auto voice leading (smooth chord changes) | ✅ (0.9.0) |
| MIDI clock out with Start/Stop; follows an external clock | ✅ |
| Velocity curves (normal, light, firm, fixed), set per half | ✅ (per half: 0.11.0) |
| Adjustable pad sensitivity: Trigger, Release and Full, with a self-measured resting level | ✅ (0.10.0) |
| Poly aftertouch from chord pads on each of their notes | ✅ |
| ▲/▼ shortcuts: step through your favorite scales, melody octave, presets or tempo | ✅ |
| Scale families on M1–M7 (melodic minor, harmonic minor, harmonic major modes) | ✅ |
| Strum feel (random timing and velocity) and a capo for the guitar voicing | ✅ |
| I/O test, LED test and Pad test (the pads' readings as numbers) pages | ✅ |

## Known issues

TBOSS 1.5 is a beta: everything new in 1.4 and 1.5 passed its board tests on one board, with
Ableton Live as the DAW. Known gaps and limits:

- **Not in TBOSS yet:** a phrase recorder, chained progressions and the step sequencer's later
  parts (gate, probability, ratchets, more patterns; planned for 1.5.x), a second arpeggiator, bass mode,
  user colors, MPE. See the [roadmap](roadmap.md).
- The arpeggiator has no on/off button of its own: set a [center button](#center-buttons-and-shift-optional)
  to *Arpeggiator on/off*, or use Settings → Arpeggiator → *Arp*.
- A center button held for more than about 5 seconds lets go by itself (the touch buttons'
  own limit), so Shift can be latched and Sustain is tapped on and off. A held chord shape
  (Add 7th …) applies to the next chord pad you touch, not to a chord already sounding or
  feeding the arpeggiator.
- The chord edits that moved a chord's 3rd or 5th down an octave (0.5 and earlier) have
  no button any more; edits saved with them still play, and *Reset chord* clears them.
- The board appears with a test USB ID (`cafe:4069`) until TBOSS has its own.
- Windows may keep showing an older name for the MIDI port; it is the same device.
- Installing clears saved settings unless the browser installer's step 2 is used.
- Going back to an older TBOSS: the browser installer (and the editor's *Restore from
  file…*) put back what the older version has. Settings it doesn't have return to their
  defaults, progressions are left out of a version before 0.11.0 and chord sets of a
  version before 1.0.0 (the installer says how many); a version before 1.3.0 plays the scale's
  chords instead of the chord grid or Full, and a version before 1.4.0 gets the arpeggiator's
  and note repeat's 1.0 rates and rhythms (1/16, the first rhythm, note repeat off where a
  new value was set), no pivot target, and *Pivot* buttons back to their printed function; the
  step pattern is left out of a version before 1.5.0. Backups restored into versions before 0.10.0 may still be refused if they
  use settings those versions don't have.
- The external memory chip is only read, never used; the original firmware's data there
  stays untouched.
- On Linux the browser installer needs a one-time permission rule (see
  [Installing](flashing.md#with-a-browser-recommended)).

**Not yet tried on a board:** the pedal's *Target* and its Hold, Sostenuto, Soft, Portamento
and Play/Stop modes; live notes following MIDI in and on their own channel; progressions
started by a DAW; [scale lock](#scale-lock-optional).

**Least tested so far** (reports especially welcome): installing from macOS, Linux and
ChromeOS (Windows is confirmed); DAWs other than Ableton Live; the web
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
   TheoryBoard MIDI. [pads lit in rainbow colors; a touched pad turns white]
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
17. Settings → Layout → *Layout* Chromatic, Fourths, Drums. [pad colors change; Drums:
    right grid amber, kick on its bottom-left pad on channel 10]; Live notes, play the G
    chord, then the right half's columns 8–12 [G B D G B along the bottom row; play Am and
    they become A C E A C]; back to Scale.
    Settings → Lights → *Colors* Rainbow, then Drums again. [both halves red to pink from left
    to right on every row, whatever the layout; a touched pad white]; back to Default and Scale.
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
    (bottom row), ◀ Left once, tap G, ● Center. [`PROGRESSION 1 SAVED`]. Settings → Play →
    *Arrows* Sequencer; ▲ Up. [Am, F for two bars, G, looping; `P1 Am > F 1/4`; the
    current pad white, the next pulsing]; press root C [the same pads in C]; root A.
    MIDI monitor: *Chord vel* Fixed 100 [from the next chord, every chord at velocity 100];
    back to Normal [the tapped velocities again]; ▲ Up [stops]. *Arrows* back to Scale.
22b. Web editor → Chord sets: set 1, *Start from* Pop, Save to set 1, Use it on the TB.
    [bottom row left to right: C Dm Em F G Am B♭ A♭ Fm D7 E7 G7; `S1` on the play screen];
    root A [A Bm C♯m D E F♯m G F Dm B7 C♯7 E7]; Settings → Chords → *Add 7th* On
    [`SET CHORDS: NO EDITS`]; *Chord set* Scale; root C.
22c. Settings → Chords → *Hints* On; play the bottom-left C. [F pulses, G and Am glow, the
    other chord pads dim]; play Dm [G pulses]; *Hints* Off.
22d. Settings → Buttons: *Octave +3* Shift, *Octave +2* Add 7th (while held), *Spread III*
    Arpeggiator on/off. Hold Octave +2 and play C [`Cmaj7`]; tap Spread III [arpeggiator
    on, button lit; tap again: off]; hold Octave +3 and touch the Dm pad [no sound,
    `CHOSEN FOR EDITING`, `Dm` on screen]; tap Octave +3 twice [`SHIFT LATCHED`], press mode
    button 1 [`PRESET 1 LOADED`], tap Octave +3 [Shift off]. Settings → Buttons → *Reset all*.
22e. Root C, M1. Settings → Chords → *Chord set* Grid; column 5 from the bottom up. [`G`,
    `G7`, `Gsus4`, `G9`]; column 4, second row from the top [`Fsus2`]. *Chord set* Full;
    column 1 from the top down [`C`, `Cadd9`, `C6/A`, `C6/9/A`]. *Chord set* Scale.
22f. Settings → Arpeggiator → *Arp* Up, *Arp rate* 5/beat, *Rhythm* `3/8`; hold a chord.
    [five steps a beat, three of every eight sounding]; *Rotate* 1 [the hits move]; *Arp*
    Off, *Rhythm* back to `XXXXXXXX`, *Rotate* 0, *Arp rate* 1/16. Settings → Play → *Note
    repeat* By row; hold a bottom-row and a second-row melody pad. [2 against 3 per beat];
    *Note repeat* Off.
22g. Root C, M1. Settings → Chords → *Pivot to* G MAJOR. [`PIVOT TO G MAJOR`; C and Am
    columns pulse, Em and G glow, F dims]; play Am [`ii IN G: D7>G`]; press root G
    [`NOW IN G MAJOR`, lights normal]; root C.
22h. Settings → Chords → *Capture* SET 8, ●. [`CAPTURE: PLAY CHORDS`]; from the DAW send A
    minor, F, C, G as chords. [`CAPTURE 4/12 G`]; ● Center. [`SET 8 IN A MINOR`, root A, M6,
    the bottom row plays Am F C G]; *Chord set* Scale, root C, M1.
22i. Settings → Strum → *Strum* Up; last item *Reset options*, ●. [`RESET STRUM OPTIONS?`,
    CANCEL selected]; ●. [`NOTHING WAS RESET`, Strum still Up]; ●, ▲, ●. [`STRUM OPTIONS RESET`,
    Strum Off].
22j. Settings → Sequence → *Lane* L1, *Kind* Drums, *Steps* ●. [`L1 DRUMS 16 1/16`]; tap the
    top row's 1st and 9th pads [amber]; ◀ Left, Settings → MIDI → *Transport* ●. [a kick on beats
    1 and 3; on the step screen the playing step white]; *Transport* ● again; *Steps*: tap both
    pads off.
22k. Settings → Layout → *Chords on* Right. [the right half lit as chords, the left as the
    scale; the right half's bottom-left pad plays C, the left half's bottom row C D E …];
    *Chords on* Left.
23. Send a C major chord from the DAW to the board. [matching pads light up]
23a. Settings → MIDI → *Scale lock* CH 16; from the DAW send C♯, then F♯ on channel 16.
    [C, then F, on the melody channel]; *Scale lock* Off.
24. Web editor: Connect, scale library, Find by notes C E♭ F♯, pick Blues, *Play it on the
    TB*. [`BLUES` on the board]. Custom scales: U5, *Start from* Blues, Save, *Use it on
    the TB*. [while editing, the pads showed the scale and the screen said `WEB EDITOR`]
    Progressions: P2, Key A Natural minor, type `Am D Dm Am C G E7 Am`, Save to P2.
    [D and E7 outlined]; on the board root A, M6, *Progression* 2, ▲ Up [the song plays].
25. Web editor: Download settings backup; Restore from file. [`Restored n of n records`]
26. Settings → About. [`1.5.0 BETA`; the editor's header says TBOSS 1.5.0]. Settings → Pads →
    *Pad test*: rest a finger lightly on a pad, then press it. [`PAD 1200/600/3500`; each row
    ends with its pad's resting level, the bottom line `REST lo-hi`; its number rises; `*`, `PEAK` and `LAST VEL` appear once it plays]; *Trigger* 800 [lighter
    touches play]; *Trigger* 500 [`OUT OF RANGE`: too close to Release]; *Trigger* back to 1200.
26a. Hammer one melody pad as fast as you can, with the DAW recording. [every hit is a note, none
    stays on]; a light tap and a hard strike [the hard one much louder].
27. TRS out to a synth (Type A adapter), play. [the synth plays]
28. Sustain pedal plugged in before power-up; Settings → About → *I/O test*, press it.
    [`PEDAL PA10 ... DOWN` while pressed]; then Settings → Pedal → *Mode* Sustain, play
    with it. [`SUS` on screen, notes sustain]; *Mode* Off.
29. Power off and on. [settings, presets, chord panel and U5 come back]
