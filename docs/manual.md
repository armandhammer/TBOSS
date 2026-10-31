# TBOSS user manual

TBOSS is open-source replacement firmware for the TheoryBoard THY333. This manual covers
every feature on the board and in the web editor. For installing the firmware see
[Installing](flashing.md). This is **TBOSS 2.0**: the Turing arpeggiator, chained progressions with each
chord's chance, presets with their own chord sets, U3 as Shift, Sleep and Shut down, the web editor's own USB port and
the rebalanced Settings menu are new in 2.0. It also brings 1.9 (the web editor's Settings, Lights, Feel, Sequencer and
Presets tabs, the chord pad edits, the new pad colors, curves and preset names) and 1.8 (MPE, Glide, the step screen's
play button, Vel full), which were not released on their own. Everything in 1.4 to 2.0 passed its board tests on one
board. See
[Known issues](#known-issues) and please report anything that doesn't work as described here.

**How each feature is explained:** what it does, then **Try it** (steps you can follow on
the board, with what you should see or hear), **Why use it** and, where it matters, **When
not to**. Features that go beyond the original TheoryBoard are marked **Optional**: they
are off until you turn them on.

Contents: [Coming from the original TheoryBoard](#coming-from-the-original-theoryboard) ·
[New in 2.0](#new-in-20) · [New in 1.9](#new-in-19) · [New in 1.8](#new-in-18) · [New in 1.7](#new-in-17) · [New in 1.6](#new-in-16) · [New in 1.5](#new-in-15) · [New in 1.4](#new-in-14) · [New in 1.3](#new-in-13) · [New in 1.0](#new-in-10) · [The board](#the-board) · [Getting started](#getting-started) · [Playing](#playing) ·
[Key and scale](#key-and-scale) · [Chord panel](#chord-panel) ·
[Center buttons and Shift](#center-buttons-and-shift-optional) · [Chord edits](#chord-edits) ·
[Chord sets](#chord-sets-optional) · [Chord capture](#chord-capture-optional) · [Chord grid](#chord-grid-optional) ·
[Full chord layout](#full-chord-layout-optional) ·
[Theory hints](#theory-hints-optional) · [Pivot chords](#pivot-chords-optional) ·
[Voice leading and fold](#voice-leading-and-fold) · [Octaves](#octaves) ·
[Right-half layouts](#right-half-layouts) · [Hold](#hold) · [Tempo](#tempo) · [Strum](#strum) ·
[Note repeat](#note-repeat) · [Harmony, echo and mono](#harmony-echo-and-mono-optional) ·
[Arpeggiator](#arpeggiator) · [Progressions](#progressions) ·
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

## New in 2.0

*Tested on a board (2.0.8). 2.0 also brings [1.9](#new-in-19) and [1.8](#new-in-18), not released on their own.*
Four things from the roadmap and two you asked for, then more:

- **[Turing pattern](#turing-pattern-optional)** (Settings → Arpeggiator → *Arp* → Turing): the arpeggiator
  plays a loop of the held notes drawn at random. *Lock* sets how much of it stays each time round (100 %: the
  same loop over and over; 0 %: new every time) and *Loop* its length (2–16 steps). Other notes, a new loop.
- **[Chained progressions](#chained-progressions-optional)**: up to 8 progressions played one after another,
  for example P1 > P1 > P2 > P3, set in the web editor's Progressions tab. Settings → Progressions → *Chain* On
  (or Shift + root button 9) and Play plays the chain; the play screen shows where it is (`C2/4 P1 …`).
- **[Chord chance](#chord-chance-optional)**: each chord of a progression plays as written only so often
  (100 % down to 10 %); the other times it rests, or plays the borrowed chord from the parallel major or minor
  key. Set in the web editor.
- **[Presets with their own chord sets](#presets-with-their-own-chord-sets-optional)** (Settings → Presets →
  *Keep chords*): *Save preset* also saves a copy of the chord set the preset plays, and loading the preset
  brings that copy back, so two presets can use different versions of chord set 1. The editor's Presets tab
  sets, removes, copies and compares them.
- **[Copy, paste and move lanes](#sequencer)** in the editor's Sequencer tab: copy a lane onto another, move its
  steps one earlier or later, copy and paste a range of steps. On the board the
  [step screen](#moving-copying-and-clearing-a-lane) does the same with Shift.
- **[U3 as Shift](#u3-as-shift-optional)** (Settings → Buttons → *U3 button*): the top right button becomes a
  Shift button, so all 14 center buttons keep their printed functions. A tap is Shift for the next press, two
  quick taps latch it. **[Shift + the buttons under the screen](#shift--the-buttons-under-the-screen)** do more
  on every screen (tempo, play, arpeggiator, home, jump to the end, defaults), and the bottom line says what.
- **[Sleep and Shut down](#sleep-and-shut-down-optional)**: Settings → About → *Shut down* darkens the board
  until you press ● Center (as the original's shutdown did); Settings → Lights → *Sleep after* darkens it after
  5–60 minutes without a touch. Settings → About → *I/O test* now also shows the buttons you hold.
- **[The Settings menu, rebalanced](#settings-menu)** *(2.0.1)*: 17 pages, each one kind of thing, the longest 13
  items (before: 13 pages, three of 16–17). What moved: **Expression** (new): *Chord AT*, *Melody AT* (from Play),
  *MPE*, *Glide*, *Glide time* (from MIDI). **Effects** (new): *Note repeat*, *Repeat rhythm*, *Harmony*, *Echo*,
  *Echoes* (from Play), *Mono* (from Layout). **Chord edits** (new): *Add 7th/9th/6th*, *Sus*, *Strum pad*,
  *Sound*, *Reset chord*, *Reset all edits* (from Chords). **MIDI**: every channel now, *Bass ch*, *Drum ch*,
  *Live ch*, *Live in* (from Layout). **Layout**: *Melody oct* (from Play). **Sequence** is now two pages,
  **Progressions** (*Play*, *Chain*, *Progression*, *Unit*, *Record*, *Clear*, *Guide*) and **Steps** (*Steps*,
  *Lane* … *Rotate*, *Step pads*). Your settings stay as they were; each page's *Reset options* resets what is on
  it now. *(2.0.4)* The pages in a new order: what the pads play (Play, Layout, Chords, Chord edits), how they're
  played (Strum, Expression, Arpeggiator, Effects; Expression after Strum since 2.0.5), sequencing (Progressions, Steps), Presets, then the setup pages
  by how often they're changed (MIDI, Pedal, Buttons, Lights, Pads) and About. ▲ Up from Play goes to the last.
- **[Spread of Bass](#spread-i--ii--iii)** *(2.0.2)* (Settings → Chords → *Spread of* → Bass): every chord keeps
  its root as the lowest note. The chord stays as the pad, the voicing buttons or *Voice lead* make it, and the
  Spread buttons add its root under it (E G C becomes C E G C); Spread II and III put that root one or two
  octaves lower. From a user's request.
- **[More for the step sequencer](#step-options)** *(2.0.3)*, ideas from the Deluge, OXI One and Hapax:
  - **Fill** steps play only while a center button set to *Fill* (or the pedal, *Mode* Fill) is held, and *Not fill*
    steps rest then: hold it for a drum fill, let go and the beat is back.
  - **Nudge** moves a step up to half a step early or late. Each lane gets its own **Swing** and **Transpose**.
  - **Mutate** (Settings → Steps, and the editor) changes a few steps of a lane a little, differently each time. The
    editor also has **Randomize**.
  - **Stutter** (a center button, 1/4, 1/8 or 1/16): while you hold it, the lanes and the progression repeat the
    beat, eighth or sixteenth the press fell in. Let go and they carry on in time.
- **[The web editor's own USB port](#getting-started)** *(2.0.6)*: the board shows a second MIDI port,
  **TheoryBoard Editor**, and the web editor talks to it, so your DAW keeps hearing the board while the editor is
  connected. Leave that port off in the DAW.
- **[Clear preset](#presets)** *(2.0.7)*: Settings → Presets → *Clear preset* empties a preset, with its own colors,
  curves, chord set and name.
- **The web editor follows the board** *(2.0.5–2.0.8)*: a progression recorded on the board, settings changed there,
  a preset saved, cleared or loaded show in the editor within a second or two, without Reconnect; Reconnect no longer
  pauses what the board plays. Each message appears next to what you changed and goes after 3 seconds.
- **[Curves of](#your-own-feel-optional)** *(2.0.8)*: the Feel tab edits the board's curves or any preset's own
  without loading it, and sets the preset's own Chord vel to Custom so its curve is heard when it loads.

## New in 1.9

*Tested on a board (1.9.4); not released on its own: it comes with 2.0.* The board, customized from the web editor:

- **[Every setting in the web editor](#settings)**: its new **Settings** tab shows every page and item of
  the board's Settings menu, read from the board itself, with what each does and its default. A change
  goes to the board at once, and the board keeps playing while you change it.
- **[Your own colors](#your-own-colors-optional)**: the editor's **Lights** tab recolors every pad color
  scheme from a palette of 16, with a picture of the board; a preset can keep a look of its own
  (Settings → Presets → *Keep look*).
- **[Your own feel](#your-own-feel-optional)**: the editor's **Feel** tab draws each half's velocity
  curve (Settings → Play → *Chord vel* / *Melody vel* **Custom**) and pressure curve, calibrates the pads
  from a few presses, and maps every pad's resting level.
- *(1.9.4)* **Changes on the board show in the editor within a second** (the Settings tab, and the
  Sequencer tab's steps), and the editor no longer pauses the board's notes when it reads it: only a
  restore, a write that answers, or the external memory's backup still does.
- **Four more color schemes** ([Screen and lights](#screen-and-lights)): *Piano*, *Triad*, *Function*
  and *Octaves*, and **Color by** note name or scale degree (Settings → Lights).
- **[The step sequencer in the web editor](#sequencer)** *(1.9.3)*: its **Sequencer** tab shows the four lanes
  of 64 steps as a grid: click to add steps, set each step's options and each lane's kind, length, rate,
  direction and Euclid fill, and start and stop it. The board plays each change at once.
- **[Every chord pad's edits at once](#chord-pad-edits)** *(1.9.3)*: the **Chords** tab shows all 48 chord
  pads with their chord names; add the 7th, 9th or 6th, sus2 or sus4, or turn a pad's strum or sound off.
- **[Presets](#presets-tab)** *(1.9.3)*: the editor's **Presets** tab names presets (the board's
  *Load preset* shows the names), loads, copies, swaps and clears them, compares two setting by setting,
  and gives a preset its own colors or curves; a preset can keep its own velocity and pressure curves
  (Settings → Presets → *Keep feel*).
- *Full* (Settings → Pads) goes up to +4000, and *Glide* is Roll by default (it was Off).
- The editor has **tabs** along the top: Settings, Lights, Feel, Scales, Chords, Progressions, Sequencer,
  Buttons, Presets, Backup.

## New in 1.8

*Tested on a board (1.8.5); not released on its own: it comes with 2.0.* Expression per pad, and a play button for the step sequencer:

- **[MPE](#mpe-optional)** (Settings → Expression → *MPE*): every pad you play gets a MIDI channel of its
  own, so how hard you press each one shapes only its notes, also inside a chord and when two pads
  play the same note. For synths made for MPE (in Ableton Live 12: Drift, Wavetable, Meld; also Vital,
  Surge XT, Pigments, Equator and MPE hardware), many of which ignore poly aftertouch. The melody half,
  the chord half or both; each half's pressure setting chooses what a pad sends (channel pressure,
  with a controller or a bend).
- **[Glide](#glide-optional)** (Settings → Expression → *Glide*): with MPE on the melody half, rolling a
  finger from a held melody pad onto the pad beside it slides the note there instead of striking a
  new one, as you lean onto the new pad (lean clearly harder and it lands exactly on it). Lift the first pad and the note carries on; roll on
  to the next pad to keep sliding. Three ways: Roll (by which pad you press harder), Pad (by the new
  pad's pressure) and Time (portamento, over a set time).
- **[Play and stop on the step screen](#the-step-screen)**: tap **● Center** to start and stop the
  sequencer (the screen says `O PLAY` / `O STOP`); hold ● for half a second for step record (`HOLD O:
  REC`). Settings → Sequence (2.0: Progressions) now opens on *Play*, which does the same. You no longer need a pedal, a
  center button or the MIDI page to hear your steps.
- **[Vel full](#pad-sensitivity)** (Settings → Pads) and the **Firmer** and **Firmest** velocity curves
  (Settings → Play → *Chord vel*, *Melody vel*): velocity can now reach 127 only at a harder press than
  *Full*, so a medium press no longer plays every note at full velocity. *Full* itself now goes up to
  +4000, so full pressure can need a harder press too.

## New in 1.7

*Tested on a board.* The step sequencer's
second round, three more of the original TheoryBoard's 2020 promises, and three small things you asked for:

- **[Bass layout](#bass-optional)** (Settings → Layout → *Layout* → Bass): the original's promised
  *bass mode*. Each scale note sits on two pads side by side, so you can play fast repeated notes
  with two fingers, like a bass player; every hit strikes the note again, at its own velocity.
- **[Program Change](#program-change-optional)** (Settings → MIDI): each half can send a program
  number to its synth (*Chord prog*, *Melody prog*, saved in presets), and a DAW can load presets
  1–8 by sending programs 1–8 (*Program in*).
- **[Spread of Chord](#spread-i--ii--iii)** (Settings → Chords → *Spread of*): the Spread buttons
  open the whole chord across a wider range instead of only dropping its root.

- **[Step options](#step-options)** (hold steps on the step screen, ▲/▼ choose, ◀/▶ change):
  each step's own velocity, gate (10–100 % of the step), tie (it holds until the next step that
  plays), chance (90 % down to 10 %), ratchet (2–8 even strikes inside the step) and condition
  (1:2 … 4:4, first time round, every time but the first). Hold several steps to change them together.
- **[Direction](#step-options)** (Settings → Steps; before 2.0, Sequence): forward, reverse, ping-pong or random
  (every step once a time round, in an order that repeats when you play it again).
- **[Euclid and Rotate](#step-options)** (Settings → Steps; before 2.0, Sequence): fill a lane with a number of hits
  spread as evenly as they go over its length, and turn the pattern round.
- **Chord shapes and the arpeggiator:** a held *Add 7th*, *Sus4* … changes a running arpeggio at
  once (before, only the next chord pad you touched).
- **[Pads measured from where each one rests](#pad-sensitivity)** (Settings → Pads): *Trigger*,
  *Release* and *Full* now count from each pad's own resting level, so a pad that rests high (a dent,
  a warped case) plays and lets go like the others. A firm press no longer sets off the pads next to
  it, and the new *Unstick* ends a note whose pad settles back higher than it rested.
  Your settings are converted when you update.
- **[Rescan pads](#pad-sensitivity)** (Settings → Pads): measures every pad's resting level again,
  for a board that was moved and a pad that plays by itself.
- **The sustain pedal works out of the box:** Settings → Pedal → *Mode* is Sustain on a fresh
  install and after resetting the Pedal page (boards that already saved a setting keep it).
- **`EXT` on the play screen** whenever a DAW's MIDI clock comes in.

## New in 1.6

Four effects from
a user's request (ideas from the Hapax and Square Pyramid sequencers), each one to three settings
on a page you already know, all off until you turn them on:

- **[Harmony](#harmony)** (Settings → Play): every melody pad also plays a 3rd, 4th, 5th, 6th or
  octave above, a 3rd and 5th, or a 3rd or 6th below, always in the current scale, so the
  harmony follows the key as the chords do.
- **[Echo](#echo)** (Settings → Play): a MIDI delay locked to the tempo or your DAW, 1/16 to 1/2,
  1–8 echoes, each one quieter.
- **[Mono](#mono)** (Settings → Layout): the melody half plays one note at a time; the newest wins
  and the one you still hold comes back when you let go. For mono synths and bass lines.
- **[Chance](#chance)** (Settings → Arpeggiator): the arpeggiator and note repeat skip steps at
  random, from 100 % (every step) down to 10 %.

## New in 1.5

**[A step sequencer](#step-sequencer-optional)** *(optional)*: four lanes of steps
that play with the tempo or your DAW, next to a progression. A lane plays chords (chord pads, as a
progression does, so the root buttons transpose it), a melody (the scale by octaves) or drums
(MIDI notes), each with its own length (1–64 steps), rate (any arpeggiator rate, triplets and 5 or
7 per beat too) and channel, so lanes of different lengths drift against each other and meet again.
You edit it on the board: Settings → Steps → *Steps* turns the right half into the lane's
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
halves in seven fixed colors, red, orange, yellow, green, dark blue, light blue and magenta from the
left, then again, whatever you play (user request).
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
   same port; it is the same device.) Since TBOSS 2.0 the board has a second port,
   **TheoryBoard Editor** (Ableton Live lists it as `TheoryBoard MIDI (Port 2)`, Chrome on Windows as
   `MIDIIN2 (TheoryBoard MIDI)` and `MIDIOUT2 (TheoryBoard MIDI)`). It is the web editor's own: leave it off in
   your DAW.
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

**Spread of** *(1.7)* (Settings → Chords → *Spread of*, default *Root*): with **Chord**, the
Spread buttons open the whole chord instead. **Spread I** moves every second note, counted from the
bottom, up an octave (C E G becomes C G E, a Cmaj7 C G E B: the wide "open" voicing of a string
section or a piano's two hands). **Spread II** and **III** also drop the chord's root one or two
octaves under it. The screen says `SPREAD I CHORD`.
**Try it:** *Spread of* Chord, play the bottom-left chord, press **Spread I**: the same chord,
airier and wider; **Spread III**: a deep root under it.
**When not to:** fast chord changes in a low register; an open chord spans almost two octaves.

With **Bass** *(2.0.2)*, the chord stays exactly as it is and the Spread buttons add its root under it,
so the lowest note is always the root while the voicing changes only the notes above it, as a
pianist's left hand holds the root. **Spread I** puts the root just under the chord's lowest note
(nothing changes when that note is already the root), **Spread II** and **III** one or two octaves
lower. The screen says `SPREAD I BASS`.

| Pad plays | Spread I with Bass |
|---|---|
| C E G (root position) | C E G (the root is already lowest) |
| E G C (Voicing I) | **C** E G C |
| G C E (Voicing II) | **C** G C E |
| C F A (F, after C with *Voice lead*) | **F** C F A |

**Try it:** *Spread of* Bass, turn on *Voice lead* (Settings → Chords), press **Spread I**, and play
C, F, G and Am: the chords above move as little as they can, and each one has its own root in the bass.
**Why use it:** smooth inversions without losing a firm bass, when you play alone. **When not
to:** with a bass player or bass track, which already covers the root.

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
| All notes off | ends everything the board plays: held and latched chords, the arpeggiator, a playing progression (one following a DAW comes back at its next chord), [scale lock](#scale-lock-optional) notes, a Sustain button that is on, [echoes](#echo) still to come |
| Tap tempo | tap twice: the time between the taps sets the tempo (40–240 BPM) |
| Next / Previous preset, Next / Previous scale, Melody octave up / down | as the ▲/▼ shortcuts do |
| Play/stop, Record/finish | the [progression](#progressions): as ▲ and ▼ with *Arrows* Sequencer (Record on the [step screen](#the-step-screen): step record) |
| Nothing | the button does nothing |
| Pivot *(1.4)* | hold it and press a root button: the [pivot](#pivot-chords-optional) target is that key (major, or minor when the current scale has a minor 3rd); tap it alone: Pivot off. Lit while a target is set |
| Fill *(2.0.3)* (while held) | the step sequencer's [Fill](#step-options) steps play, *Not fill* steps rest (`FILL` on the Buttons page) |
| Stutter 1/4, 1/8, 1/16 *(2.0.3)* (while held) | the lanes and the progression repeat the beat, eighth or sixteenth the press fell in, again and again; let go and they carry on in time (`STUT 1/8` on the Buttons page, `STUTTER 1/8` when pressed) |

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

### U3 as Shift *(optional)*
*(2.0.)* Settings → Buttons → **U3 button** → **Shift** turns the top right button (U3, otherwise your user scale
U3) into a Shift button, so every center button can keep its printed function.

- **A tap:** Shift for the next press only (`SHIFT: NEXT BUTTON`): a root, mode or center button, a chord pad
  or a button under the screen. Tap U3 again to let go.
- **Two quick taps:** Shift latched (`SHIFT LATCHED`) until the next tap.
- **Held:** Shift while you hold it, for root buttons, chord pads, center buttons and the buttons under the
  screen. The touch chip lets go of a button held for about 5 seconds, and while U3 is held the other mode
  buttons don't answer, so for Shift + mode button (load a preset) tap U3 first.

Everything Shift does works with it (above), plus Shift + root button 9 for the
[chain](#chained-progressions-optional). U3's scale is still in Settings → Play → *Scale* and on the arrows.
*Reset all* (Buttons) sets it back to the scale.

### Shift + the buttons under the screen
*(2.0.)* With Shift on (any Shift: U3 or a center button), the five buttons under the screen do something else,
and the screen's bottom line shows what:

| Screen | ◀ | ▲ / ▼ | ● | ▶ | Bottom line |
|---|---|---|---|---|---|
| Play screen | play / stop | tempo ± 5 BPM (next / previous scale if *Arrows* is Tempo) | tap tempo | arpeggiator on / off | `<PLAY ^v120 >ARP OTAP` |
| Settings pages | home | first / last page | | | `<HOME ^vEND` |
| A page's items | home | first / last item | the item back to its default | | `<HOME ^vEND ODEFAULT` |
| Changing a value | home | 10 values up / down | its default | | `<HOME ^v10 ODEFAULT` |
| Step screen | move the lane's steps earlier / later (◀ ▶) | copy / paste the lane | clear the lane (asks first) | | `<>MOVE ^CPY vPST OCLR` |

While recording a progression or capturing chords the buttons do what they always do there.

**Try it:** U3 as Shift. Tap U3, ▲: `TEMPO 125 BPM`. Tap U3, ▶: the arpeggiator on. ● Center into Settings, tap
U3, ▼: the last page (About). Tap U3, ◀: home.

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
2. Settings → Play → *Arrows* → Sequencer. Settings → Progressions → *Progression*: an `EMPTY`
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

- **Melody half:** Settings → Layout → *Melody oct*, −2 to +2 octaves. If you change it
  often, set Settings → Play → *Arrows* to *Melody octave*: then **▲ Up / ▼ Down** on the
  play screen move it (`MELODY OCT C4`).
- **Chord half:** the [chord panel](#chord-panel)'s octave buttons.

The screen line `OCT C2 C3` shows the lowest root of each half (chords, melody).

**Try it:** Settings → Layout → *Melody oct* → +1: the right half's bottom row now starts on
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
| Bass *(1.7)* | each scale note on two pads side by side, two octaves down, for fast two-finger bass lines |

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

### Bass *(optional)*
*1.7.* The original TheoryBoard's promised **bass mode**. A single pad can't be struck again
very fast: it has to come back up between two hits. A bass player gets round that by plucking with
two fingers in turn, and this layout gives you the same: **each scale note is on two pads side by
side**, so your index and middle finger can take turns on one note.

- **The pads:** 6 notes to a row, as 6 pairs. The bottom row's left pair is the key's root two
  octaves below the melody half (C1 with the default *Melody oct*), and the scale climbs on pair by
  pair, left to right, then on into the next row up: 24 notes, about 3½ octaves of a 7-note scale.
  *Melody oct* moves it all an octave at a time.
- **Every hit sounds:** touching a pad while its twin still holds the note ends the note and strikes
  it again, at the new touch's velocity. The pad under the other finger stays quiet until you lift it
  and touch it again. Two different notes play together as usual.
- It plays on the *Melody ch*; the pedal, pressure, *Melody prog* and *Mono* work as on the other
  layouts.
- The board ignores a touch that rises less than half as far as a held pad right next to it
  (a firm press lifts its neighbors' readings: 1.7's bleed check). In a roll, strike each hit about
  as firmly as the last; a much softer touch on the twin, while the other finger still presses hard,
  is taken for bleed and doesn't sound.

**Try it:** a bass sound on the melody channel, *Layout* → Bass. Rest your index finger on the
bottom-left pad and your middle finger on the pad to its right, and tap them in turn as fast as you
can: a steady stream of low Cs, each hit as loud as you strike it. Move one pair to the right for
the next note of the scale.
**Why use it:** fast repeated bass notes and runs played by hand, with your own timing and accents.
Note repeat does it for you, on the clock (Settings → Effects → *Note repeat*). **When not to:** for
melodies over many octaves, the Scale layout has more range.

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

**Live ch** (Settings → MIDI → *Live ch*, default *Melody*): give columns 8–12 their own
MIDI channel (1–16), so they can play a different instrument or be recorded on their own
track. **Try it:** *Live ch* 2, a second instrument on channel 2: columns 1–7 play the
melody instrument, columns 8–12 the second one. (Pressure as poly aftertouch follows them
there; channel pressure and the pedal's sustain stay on the Melody ch.)

**Live in** (Settings → MIDI → *Live in*, default Off): chords your computer sends **to**
the board pick the live chord, on one MIDI channel (1–16) or All. The lowest note held is
the chord's root; two or more different notes make a chord. A chord stays until the next
one arrives (letting it go note by note never changes it); touching a chord pad takes
over again. The screen names the incoming chord.
**Try it:** in your DAW, send a chord track to the TheoryBoard MIDI port, *Live in* All,
play the song: columns 8–12 follow the song's chords while you solo over them.

**Silent chords:** Settings → Chord edits → *Sound* Off on a chord pad: the pad sets the live
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

**Strum pad** (Settings → Chord edits → *Strum pad*) turns strum off for one pad (the last one
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

Settings → Effects → *Note repeat*: **Off**, 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T (triplets),
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
- **[Chance](#chance)** (Settings → Arpeggiator) skips strikes at random; the
  first strike, when you touch the pad, always sounds.

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

## Harmony, echo and mono *(optional)*

*(All Off by default.)* Three ways to change what a melody pad plays. They work on the
right half's pads (the left half with [Chords on the right](#chords-on-the-right-optional)) in
every layout but Drums. Chord pads, the arpeggiator, progressions, the step sequencer and notes
arriving over MIDI are not changed. While one of them is on (and the arpeggiator, strum and note
repeat are off), the play screen's bottom line shows them: `HARM 3RD ECHO 1/8 MONO`.

### Harmony

Settings → Effects → *Harmony*: every melody pad also plays a second note (or two) a fixed number
of **scale steps** away, so the harmony stays in the key: in C major a 3rd above C is E (major), above
D it is F (minor), above B it is D. Change the root or mode and the harmony follows.

| Harmony | Adds | C major, pad C |
| --- | --- | --- |
| Off | nothing | C |
| 3rd | 2 scale steps up | C + E |
| 4th | 3 steps up | C + F |
| 5th | 4 steps up | C + G |
| 6th | 5 steps up | C + A |
| Octave | the same note an octave up | C + C |
| 3rd+5th | 2 and 4 steps up: a triad on every note | C + E + G |
| 3rd below | 2 steps down | C + A below |
| 6th below | 5 steps down (a 3rd above, an octave lower) | C + E below |

- In scales with more or fewer than 7 notes the same number of that scale's own steps is
  used: in a pentatonic scale a "3rd" is two steps of the pentatonic.
- A note outside the scale (the Chromatic layout, live notes) is harmonized from the scale
  note just below it.
- The harmony notes play with the pad's velocity and pressure, and on the melody channel.

**Try it:** root C, M1, *Harmony* 3rd. Play C D E F G on the melody pads: C+E, D+F, E+G, F+A,
G+B: parallel thirds, some major, some minor, all in C. Press root A, M6: the same pads play
thirds in A minor. *Harmony* 6th below: a lower second voice, like a duet.
**Why use it:** instant two-part melodies, thicker leads, triad stabs (3rd+5th) from one
finger. **When not to:** fast solos where every note should be single; Off.

### Echo

Settings → Effects → *Echo*: each melody note comes back after a note value of the
[tempo](#tempo), or of the DAW's clock when one comes in: **Off**, 1/16, 1/8T, 1/8, 1/8.
(dotted), 1/4, 1/4. or 1/2. *Echoes* sets how many times: 1–8 (default 3). Each echo is quieter:
with 3 echoes they play at 75, 50 and 25 % of the note's velocity; each one sounds for half the
echo time.

- Harmony notes echo with their pad; with [note repeat](#note-repeat) every strike echoes.
- A note that is still sounding when its echo comes (you are still holding the pad, or another
  pad plays the same note) is not struck again, so hold notes shorter than the echo time to hear
  every echo.
- Up to 8 notes echo at once; a 9th note takes over the echoes of the oldest one.
- *All notes off* (a [center button](#center-buttons-and-shift-optional)) stops the echoes still to
  come, as does setting *Echo* Off.

**Try it:** *Echo* 1/8., *Echoes* 4, 120 BPM. Tap short notes on the melody pads: each one
comes back four times, quieter and quieter, dotted eighths apart: the classic "dotted delay"
rhythm against the beat. With a DAW sending MIDI clock it locks to the DAW's tempo.
**Why use it:** rhythmic delay on synths that have none, echoes that are real notes you can
record, quantize and edit in the DAW. **When not to:** with a synth that has its own delay
already set, or for dense chords of notes: the echoes pile up.

### Mono

Settings → Effects → *Mono*: **On** makes the melody half play one pad at a time. The newest pad
you touch sounds; the one before it goes quiet, and comes back when you lift the newer one (as on
a monophonic synth's keyboard). The new note starts just before the old one ends, so a mono synth
set to legato or glide slides between them instead of starting its envelope again.

- Not in the Drums layout (every drum pad keeps sounding).
- With Harmony on, a pad's harmony notes go with it.

**Try it:** *Mono* On, a synth with glide (portamento) on. Hold C, then touch G: the synth slides
up to G. Lift G: it slides back to C. Trills: hold one pad and tap another.
**Why use it:** bass lines and leads on mono synths, trills, and "last note wins" playing.
**When not to:** chords or two-handed melodies on the right half.

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
| Chance | 100 % down to 10 % in steps of 10: how likely each step is to play ([Chance](#chance)) | 100 % |
| Swing | 0 to 50 %: the first note of each pair longer, the second shorter | 0 % |
| Sync start | On: a new arpeggio waits for the next step of the clock, so it starts in time | Off |
| Latch | On: the arpeggio keeps running after you lift your hand; touch another chord to change it, or the same pad to stop | Off |

<a id="chance"></a>**Chance**: each step the [rhythm](#arpeggiator) plays sounds only
with this probability, decided at random each time; skipped steps still move the pattern on, so
the notes that do play stay in their places. The same setting thins out [note repeat](#note-repeat)
(the strike when you touch the pad always sounds). **Try it:** *Arp* Up, *Arp rate* 1/16, *Chance*
60 %, hold a chord: the same arpeggio, with gaps that change every bar. **Why use it:** patterns
that breathe and never repeat exactly, ghost-note hi-hats with note repeat. **When not to:** a part
that must be the same every time (a recorded loop); 100 %.

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

### Turing pattern *(optional)*
*(2.0.)* Like the Turing Machine module: *Arp* **Turing** plays a loop of steps, each a held note chosen at
random (from every note the other patterns would play, *Arp octaves* included). Each time round, each step is
drawn again or kept:

- **Lock** (0–100 %, 80 % to start): how likely a step is kept. 100 %: the loop repeats exactly, as long as you
  hold the same notes; 90 %: it changes slowly; 0 %: a new random note every step.
- **Loop** (2–16 steps, 8 to start): how many steps before it comes round.

*Lock* and *Loop* show on the Arpeggiator page only while *Arp* is Turing. The loop belongs to the notes you
hold: other notes (another chord, an added pad, another octave setting) start a new loop. *Rhythm*, *Rate*,
*Gate*, *Swing* and *Chance* work as for the other patterns; a silent rhythm step still moves the loop on.

**Try it:** *Arp* Turing, *Lock* 100 %, *Loop* 8, hold a chord: an eight-note phrase that repeats. *Lock* 80 %:
it drifts, a note now and then. Change chord: a new phrase in the new chord. *Lock* 100 % again to keep a
phrase you like.
**Why use it:** melodies and basslines you didn't write, that stay with the chords you play and repeat until you
let them change.

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
1. Settings → Progressions → *Progression*: pick a slot, 1–8 (`EMPTY` shows a free one).
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
  silent (Settings → Chord edits → *Sound* Off) is silent in the progression too.
- Picking another slot while it plays starts that progression at once, from its first
  chord (with a DAW: at the song's position). When the web editor saves something to the board
  (or reads its whole menu, on Connect), the progression pauses with everything else and carries on
  where it was. *(1.9.4: reading settings, steps or a backup no longer pauses it.)*
- The play screen shows the playing chord's name and `P1 Am > F 3/8`: slot, the chord now,
  the next one, and the unit playing of all the units. Long chord names are left out of
  this line to keep the position on screen (the chord now is also the large name above).

**Guide lights** (Settings → Progressions → *Guide*, On by default): while a progression plays,
the playing chord's pad lights soft white and the next chord's pad pulses in its color,
so you can see what's coming; on the right half the playing chord's notes are bright and
the others dim, which shows the safe melody notes for this moment.

*Clear* (Settings → Progressions → *Clear*, ●) empties the selected slot. Pressing ▲ (or
Transport) with an empty slot shows `NO PROGRESSION YET`.

<a id="try-progression"></a>**Try it (the board):**
1. Root A, mode M6 (A minor). Settings → Progressions → *Progression* 1, *Unit* 1 bar, *Record* ●.
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
   Settings → Chord edits → *Add 7th* On (`Em7`). Then *Unit* 1 bar, *Record* ●.
2. Tap Am (bottom row, 1st pad). Press **M2** (Dorian) and tap the 4th pad: `D`. Press
   **M6** again and tap the 4th pad: `Dm`. Tap Am, then C (3rd pad) and G (7th pad).
3. Press **M1** (A major) and tap the 5th pad: `E7`. Press **M6**, tap Am.
4. ● Center, ▲ Up: Am D Dm Am C G E7 Am, with D and E7 exactly as recorded.
5. Afterward, play the 5th pad and Settings → Chord edits → *Reset chord*.

(Simpler: type it in the [web editor](#progressions-1), which finds the pads and the
borrowed scales for you.)

**Why use it:** a backing to practice a melody over; a song's changes always at hand on
stage; a loop to write a melody to; chords from outside the scale without leaving the
pads; transposing a whole song with one button.
**When not to:** for long, through-composed parts or exact rhythms within a bar, record
into your DAW instead. A progression is chords on a grid of units, not a note sequencer:
it does not record the melody half, pressure, or timing within a unit. For rhythms, a melody
or drums, use the [step sequencer](#step-sequencer-optional).

### Chained progressions *(optional)*
*(2.0.)* A chain plays up to 8 progressions one after another and then starts again from the first: P1 > P1 > P2 >
P3 for verse, verse, chorus, bridge. A progression can be in the chain more than once; empty slots are skipped.

- **Set the chain** in the web editor's [Progressions tab](#progressions-1), under *Chain*: choose P1–P8 for each
  link, **Save chain**.
- **Play it:** Settings → Progressions → **Chain** On, or **Shift + root button 9** (the G♯ button; `CHAIN ON: 1123`
  shows the chain, `CHAIN OFF`). While *Chain* is On, everything that plays the progression plays the chain: ▲
  (Arrows: Sequencer), a Play button, the pedal's Play/Stop, Settings → Progressions → *Play*, and a DAW's Start,
  Continue and song position. Shift + root buttons 1–8 still play one progression, and turn *Chain* off.
- **Where it is:** the play screen's bottom line starts `C2/4 P1`: link 2 of 4, playing P1, then the chord now,
  the next and the unit as before.

*Chain* is a board setting: presets don't change it. Recording still records the selected progression.

**Try it:** record Am F C G into P1 and Dm G C into P2 (or type them in the editor). In the editor, chain P1 >
P1 > P2, **Save chain**. On the board, Shift + root button 9 (`CHAIN ON: 112`), ▲ with Arrows on Sequencer:
`C1/3 P1`, then `C2/3 P1`, then `C3/3 P2`, and round again.
**Why use it:** a whole song's chords, not one loop. **When not to:** for arrangements with many parts, use your DAW.

### Chord chance *(optional)*
*(2.0.)* Each chord of a progression has a chance: how often it plays as written, 100 % (always) down to 10 %.
The other times it either **rests** (nothing sounds for its length: the chord before ends, an arpeggio stops)
or plays the **borrowed** chord: the same chord pad in the parallel key, natural minor for a scale with a major
3rd and major for one with a minor 3rd (C becomes Cm in C major, Am becomes A in A minor). The dice are thrown
once each time round, and the play screen names what plays (`REST`, or the borrowed chord).

Set it in the web editor's [Progressions tab](#progressions-1): under each chord, a chance and *else rest* or
*else borrow*. For a saved progression the board takes each change at once. Recording a progression again on the
board sets its chords back to 100 %.

**Try it:** Am F C G a bar each; in the editor give G 50 % *else borrow*. Play it: half the time the fourth bar is
G, half the time Gm.
**Why use it:** a loop that doesn't repeat exactly; a borrowed chord now and then for color.

## Step sequencer *(optional)*

*(1.5.0)* Four lanes of steps, L1 to L4, that play in time with the [tempo](#tempo) or
your DAW, at the same time as a [progression](#progressions). Each lane has its own kind, length,
rate and MIDI channel, so a 5-step bass line in eighths turns against a 16-step drum beat and
lines up with it again every 5 bars (polymeter).

**Walk-throughs:** [a drum beat and a bass line](#try-steps) · [chords by step record](#try-step-record) ·
[with a DAW](#try-steps-daw).

**How to play it:** on the step screen, tap **● Center**: the lanes play; tap it again: they stop
*(1.8)*. Elsewhere: Settings → Progressions → *Play* ●, or ▲ on the play screen with Settings → Play →
*Arrows* Sequencer, a center button set to *Play*, the pedal in *Play/Stop* mode, or play in a DAW
that sends the board its clock. All of these start and stop the same transport, so the lanes and any
progression play together.

### Lanes

Settings → Steps. *Lane* picks the lane the items after it set (and the one the step screen
opens on):

| Item | Values | Default |
| --- | --- | --- |
| Lane | L1–L4 (the value shows the lane's kind: `L2 MELODY`) | L1 |
| Kind | Chords / Melody / Drums / Off | Chords |
| Length | 1–64 steps | 16 |
| Rate | a step's length: any [arpeggiator](#arpeggiator) rate, 1/4 … 1/16 … 1/32, triplets, 5/BEAT, 7/BEAT, dotted | 1/16 |
| Channel | AUTO (chords: the chord channel, melody: the melody channel, drums: the drum channel; the value shows which) or 1–16 | AUTO |
| Mute | Off / On | Off |
| Direction *(1.7)* | Forward / Reverse / Ping-pong / Random ([Step options](#step-options)) | Forward |
| Euclid *(1.7)* | Off / 1 … the length: fills the lane with that many hits ([Step options](#step-options)) | Off |
| Rotate *(1.7)* | 0 … the length − 1: turns the Euclid fill (not stored) | 0 |

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
for melody and drum lanes), unless its own [options](#step-options) *(1.7)* say otherwise. Held chord shapes (Add 7th … on a center button) don't change a
step; the chord panel does.

### The step screen

Settings → Steps → *Steps*, ●. The screen shows the lane (`L1 CHORDS 16 1/16`: lane, kind,
length, rate), the page and channel (`PAGE 1-48 CH1`), the step you hold or the value picked last
(`PICKED: Am`), what just happened (`STEP 5 ON`), and where the lane is (`PLAYING 5/16`).

- **The right half is the lane's steps,** 12 to a row in reading order: steps 1–12 on the top
  row, then 13–24, 25–36, and 37–48 on the bottom row. **▶ Right** shows steps 49–64 (the top
  row and the first four pads of the second), **◀ Left** goes back. Steps that are on light in the
  color of what they play, the playing step is white, steps past the lane's length are dark, and
  of the steps that are off every fourth (1, 5, 9 …: the beats at 1/16) is a little brighter. The steps
  make no sound.
- **The left half is what to play,** and you hear it: chord pads on a chord lane, scale notes
  on a melody lane, drum notes on a drum lane. *(1.9)* With Settings → Steps → **Step pads**
  **Silent** they choose without a sound (to fill steps while a song plays, say).
- **Tap a step:** it goes on with what you touched last on the left (until then: the first
  chord, the root, or the kick), or off.
- **Hold a step and touch a left-half pad:** that step plays it.
- **▲ / ▼:** the previous / next lane.
- *(1.7)* **Hold steps and use the arrows:** ▲ / ▼ choose an option, ◀ / ▶ change it on
  every held step ([Step options](#step-options)).
- **◀ Left** on steps 1–48: back to the Steps page.
- *(1.8)* **Play / stop:** tap **● Center**. The bottom line says what a tap does next
  (`<BACK  O PLAY  >49-64`, `O STOP` while it plays); the line above shows the lane's step
  (`PLAY 5/16`) and the reminder `HOLD O: REC`. ● acts when you let go, so a hold can do something
  else.
- **Step record:** **hold ● Center** for half a second *(1.8; before, a tap)*, or press a center
  button set to *Record*. The screen says `REC: PLAY THE PADS` as soon as the hold is long enough
  and the bottom line `REC 1/16 >REST O END`. Each left-half pad you tap fills the step at the cursor
  (it pulses white) and moves on, round the lane; **▶ Right** enters a rest (that step off);
  **◀ Left** takes the last one back. A tap on ● ends it.

### Step options

*(1.7)* Each step can have its own velocity, gate, tie, chance, ratchet and condition, and
each lane a direction; *Euclid* fills a lane with evenly spread hits. Every one of them starts at
the value 1.5 played (velocity as touched, half the step, no tie, always, one strike, every time
round, forward), so patterns made with 1.5 play as they did.

**On the step screen, hold a step** (or several: hold one, then touch more):

- **▲ / ▼** choose what to change: *Velocity*, *Gate*, *Tie*, *Chance*, *Ratchet*, *Condition*, *Nudge* *(2.0.3)*.
  The screen shows it and the held step's value (`CHANCE 50%`); the bottom line says
  `<- >+  UP/DN: FIELD`.
- **◀ / ▶** change it on every held step: velocity in fives (1–127), gate in tens (10–100 % of the
  step; 50 % is the default), tie Off / On, chance Always, 90 % … 10 %, ratchet 1–8 strikes,
  condition Always, 1:2, 2:2, 1:3, 2:3, 3:3, 1:4, 2:4, 3:4, 4:4, First, Not first, Fill, Not fill, nudge
  −8/16 … +7/16 of a step (`NUDGE +3/16`, `ON TIME`).
- A held step that is off goes on when you change it, with the value picked last.
- Touching a left-half pad gives every held step that chord, note or drum, at the touch's velocity;
  their other options stay.
- Steps you held to change aren't switched off when you let go. ▲ / ▼ choose the lane again and
  ◀ / ▶ the page once no step is held.

What they do:

- **Gate:** how long the step sounds, as a part of its length; 100 % runs into the next step.
- **Tie:** the note holds over the steps after it that don't play, until a step plays (or the
  lane stops, is muted, or *All notes off*). A tie over a rest makes a long note.
- **Chance:** the step plays with that probability. The draw comes from the pattern, the lane,
  the step and how many times the lane has gone round, so a pattern played again from the start
  (or from the same song position) makes the same choices.
- **Ratchet:** the step strikes 2–8 times, evenly inside its length, each strike for its gate's
  share. With a tie, the last strike holds.
- **Condition:** A:B plays the A-th of every B times round the lane (1:2 every other time, from the
  first; 2:2 the others; 4:4 every fourth time, from the fourth). *First* plays the first time
  round only, *Not first* every time but the first. A ping-pong lane goes round once it is back
  at its first step. *(2.0.3)* *Fill* plays only while Fill is held (a center button set to *Fill*, or the pedal
  with *Mode* Fill), *Not fill* only while it isn't. Firmware before 2.0.3 plays both every time.
- **Nudge** *(2.0.3)*: the step starts up to half a step early or late, in sixteenths of a step: a snare a
  little behind the beat, a push before it. Its gate and ratchets go with it.
- **Velocity:** the step's own, 1–127. *Chord vel* / *Melody vel* Fixed 100 still win.

Settings → Steps, for the lane *Lane* chose:

- **Direction:** *Forward*, *Reverse* (last step first), *Ping-pong* (there and back, the end
  steps once) or *Random* (every step once a time round, in an order of its own each time round
  that repeats when the pattern is played again).
- **Euclid:** *Off*, or a number of hits up to the lane's length (`5/16`). Choosing it fills the
  lane: the hits go on, spread as evenly as they go, the first on step 1; a hit that was already on
  keeps what it plays, a new one gets the value picked last; every other step goes off. The steps
  are ordinary steps afterwards: edit them as you like. *Off* leaves them as they are.
- **Rotate:** turns the Euclid pattern that many steps later (0 up to the length − 1) and fills
  again. It is not stored: after a restart it shows 0, and the steps stay as they were.
- **Swing** *(2.0.3)*: every second step of the lane this much of a step later (Off, 5 % … 50 %), each lane its
  own. A step's nudge adds to it, and a step never moves more than half a step.
- **Transpose** *(2.0.3)*: the whole lane up or down, −24 … +24. A chord lane moves by steps of the scale (+1: C
  becomes Dm in C major), a melody or drum lane by semitones (`+2 SEMIS`). A melody lane can leave the scale this
  way; a drum lane plays other drums.
- **Mutate** *(2.0.3)*: an action, not a setting (so it has no Off): nothing changes until you press ● the second
  time. ● opens it, ▲▼ choose how many of the lane's steps may change (`10% OF STEPS` … `50% OF STEPS`: 10 % touches
  one step in ten, 50 % every second one), ● changes them once (`LANE 1 MUTATED`). A chord or melody step moves one or two pads left or right (its row stays) and some change
  velocity. A drum step goes off, or comes on as the nearest step before it. Each ● differs; press it until you like
  it. The editor's Sequencer tab does the same (the editor can't set this item).

<a id="try-fill"></a>**Try it (2.0.3: fill, swing, stutter):**
1. Make the drum beat of [the first walk-through](#try-steps). Settings → Buttons → *Octave +3* → `FILL`,
   *Octave +2* → `STUT 1/8`.
2. L1's step screen: hold the empty steps 15 and 16 (the second row's 3rd and 4th pads), touch the snare (38) on the
   left half, then ▲/▼ to `CONDITION` and ◀/▶ to `FILL`; let go. *Transport* ●. [the beat as before]
3. Hold the *Octave +3* button [`FILL WHILE HELD`: two snares end each bar]; let go [gone again].
4. Settings → Steps → *Lane* L2, *Swing* 25%. [the hi-hats swing]
5. Hold the *Octave +2* button [`STUTTER 1/8`: the same eighth over and over]; let go [the beat carries on in time].

<a id="try-step-options"></a>**Try it (a drum beat with options):**
1. Make the drum beat of [the first walk-through](#try-steps) (L1 kicks, L2 hi-hats).
2. On L2's step screen hold the hi-hat on step 3, ▲ Up until `CHANCE`, ◀ Left five times.
   [`CHANCE 50%`]; let go. *Transport* ●. [that hi-hat comes and goes, the same way each time you
   start again]
3. Hold the kick on step 9, ▼ Down to `RATCHET`, ▶ Right twice. [`RATCHET 3 STRIKES`: a triplet
   roll on beat 3]
4. Settings → Steps → *Lane* L3, *Kind* Drums, *Euclid* 5. [`LANE FILLED`: five hits over 16];
   *Rotate* 2 [the five move two steps later].
5. *Lane* L2, *Direction* Reverse, then Random. [the hi-hats backwards, then shuffled]
6. Settings → MIDI → *Transport* ● to stop.

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
1. Settings → Steps → *Lane* L1, *Kind* Drums, then *Steps*, ●. [`L1 DRUMS 16 1/16`]
2. Tap the top row's 1st and 9th pads: the kick on steps 1 and 9. [they light amber]
3. Touch the left half's first column, second pad from the top: a snare (note 38). [`PICKED: D2
   38`] Tap the top row's 5th pad and the second row's 1st (step 13): snares there.
4. ◀ Left. *Lane* L2, *Kind* Drums, *Steps* ●. Touch the left half's second column, second pad
   from the top (42, a closed hi-hat), then tap every other step: the top row's 1st, 3rd, 5th,
   7th, 9th, 11th and the second row's 1st and 3rd.
5. ◀ Left. *Lane* L3, *Kind* Melody, *Length* 5, *Rate* 1/8, *Steps* ●. Hold ● Center (step
   record: `REC: PLAY THE PADS`), then tap the bottom row's 1st, 1st, 5th, 4th and 3rd pads (in
   C major: C C G F E). [the cursor goes round to step 1] Tap ● to end.
6. Tap ● Center. [the beat, and a 5-step bass line that turns against it and lines up again every
   5 bars]
7. While it plays: press root D (everything but the drums moves up a tone), then tap ● again to
   stop. ◀ Left to the Steps page, *Lane* L2, *Mute* On, then *Steps* ● and ● Center
   [the same, without the hi-hats]; ● again to stop, then *Mute* Off.

<a id="try-step-record"></a>**Try it (chords by step record):**
1. Settings → Steps → *Lane* L4 (Chords), *Length* 4, *Rate* 1/4, *Steps* ●.
2. Hold ● Center until `REC: PLAY THE PADS`. Tap the bottom row's 6th, 4th, 1st and 5th pads (in
   C major: Am F C G), one chord per step; tap ● to end. [steps 1–4 light in their chords' colors]
3. Hold step 2 (the top row's 2nd pad) and touch the 2nd pad of the bottom row (Dm): step 2 is
   Dm now. Tap step 4: off.
4. Tap ● Center. [Am Dm C, a beat each, then a beat of rest, round and round] Press *Voicing* II
   on the chord panel: the steps play the new voicing. Tap ● to stop.

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

### Moving, copying and clearing a lane
*(2.0.)* On the step screen, with [Shift](#shift--the-buttons-under-the-screen) on (U3 tapped, or a Shift button
held), the buttons under the screen work on the lane shown, and the bottom line says so (`<>MOVE ^CPY vPST OCLR`):

- **◀ / ▶** move every step one earlier or later; the one that falls off the end comes round to the start. Only
  the steps within the lane's length move.
- **▲** copies the lane; **▼** pastes it onto the lane shown: its steps and its settings (kind, channel, length,
  rate, direction, mute), so the two play alike.
- **●** asks first (`SHIFT+O AGAIN: CLEAR`); Shift + ● again within 3 seconds takes every step away (the lane's
  settings stay).

The web editor's [Sequencer](#sequencer) tab does the same, and also copies and pastes a range of steps.

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

### Program Change *(optional)*
*1.7.* A **Program Change** is the MIDI message that picks a synth's sound (its "patch" or
"preset"), by number. Hardware synths, and every DAW, understand it. TBOSS uses it both ways:

- **Sending** (Settings → MIDI → *Chord prog*, *Melody prog*, default Off): 1–128, sent on that
  half's channel when you set it, when you load a preset, when the board starts (a synth on the TRS
  jack switches at once), again 3 seconds later (by then a DAW has usually opened the board's USB
  port, which it can only do after the computer has found the board), and whenever a DAW sends Start,
  just before its first notes. Each preset
  keeps its own two programs, so loading a preset also changes your synths' sounds. When both halves
  share a channel, only *Chord prog* goes (or *Melody prog* when *Chord prog* is Off).
- **Receiving** (Settings → MIDI → *Program in*, default Off): programs 1–8 arriving on that channel
  (or *All* channels) load presets 1–8, as if you chose them on the board (`PRESET 3 LOADED`;
  `NO PRESET THERE` for an empty slot). Higher numbers are ignored. *Program in* belongs to the board:
  loading a preset doesn't change it.

**Try it (DAW → board, Ableton Live 12):** save two different presets in slots 1 and 2. Set
*Program in* to CH 16. In Live, make a MIDI track, set its *MIDI To* to the TheoryBoard and channel
16, and make two empty MIDI clips (double-click two empty slots). In each clip's *Clip View*, set its
Program Change box (*Pgm*) to 1 in the first and 2 in the second. Launching a clip loads that preset
on the board.
**Try it (board → synth):** *Melody prog* 5 with a General MIDI synth (or a hardware synth) on the
melody channel: it changes to sound 5. Save preset 3; set *Melody prog* 33 and save preset 4;
switching between presets 3 and 4 now switches the synth's sound too.
**Why use it:** a song's whole setup, the board and the sounds, changes in one step; a DAW can switch
the board between verse and chorus setups at the right bar.
**When not to:** don't send the board's own programs back into it on the *Program in* channel (a DAW
track that echoes everything it receives): each program it sends would load a preset. Keep *Program
in* on a channel of its own, such as 16.

What the 2020 campaign also listed: NRPN messages aren't planned; a MIDI channel for every pad came
in 1.8, as [MPE](#mpe-optional).

### MPE *(optional)*
*1.8.* **MPE** (MIDI Polyphonic Expression) gives every pad you play a MIDI channel of its own, so
pressing one pad harder shapes only that pad's notes: one note of a melody, one chord of two held
chords, also when two pads play the same note (with one channel, the synth can't tell them apart).
Synths made for MPE read pressure this way; many of them ignore poly aftertouch.

Settings → Expression → *MPE*, default Off:

| Setting | What plays where |
| --- | --- |
| Melody | the melody half is an MPE *Lower Zone*: each melody pad on one of channels 2–8; the half's own messages (sustain, *Melody prog*, the zone's setup) on channel 1 |
| Chords | the chord half is an *Upper Zone*: each chord pad on one of channels 9–15, all of its notes on that one channel; sustain, *Chord prog* and the setup on channel 16 |
| Both | both zones: two MPE synths, or one that takes both zones |

- While MPE is on, the halves play on channels 1 and 16 whatever *Chord ch* and *Melody ch* say (the
  Expression page shows `MPE 1` and `MPE 16`); a half that isn't in MPE plays plainly on its one.
- Up to seven pads at once get a channel each; an eighth shares the channel with the fewest pads (the
  strongest of their pressures goes out). A pad takes the channel unused the longest, so a note that
  just ended can ring out undisturbed.
- Whatever else plays in a zone takes channels of it too: the arpeggiator, echoes, progressions,
  lanes on the half's channel, the scale lock, the pedal's note.
- Not with the Drums layout (the melody half then plays as without MPE).
- The board tells the synth about its zones (the MPE configuration message) when it starts, again
  3 seconds later and whenever a DAW sends Start; turning MPE off tells it the zones are gone.

What a pad sends follows its half's pressure setting ([Pressure](#pressure-aftertouch)):

| *Chord AT*, *Melody AT* | With MPE, on the pad's own channel |
| --- | --- |
| Off | no pressure (the pads still get channels of their own, for [Glide](#glide-optional)) |
| Poly AT, Channel AT | channel pressure |
| CC1, CC2, CC11, CC74 | channel pressure and that controller (CC74 is MPE's "timbre") |
| Pitch bend | channel pressure, and the pad's notes bend up as you press, up to 2 semitones |

Set the synth's MPE pitch bend range to 48 semitones (the MPE default, and Live's) so that bends and
glides land in tune.

**Try it (Ableton Live 12):** Options → Settings → *Link, Tempo & MIDI*: for the `TheoryBoard MIDI`
input, turn on **MPE**. Make a MIDI track with an MPE sound (in Live's browser, search `MPE`; Drift
and Wavetable presets that respond to pressure work), **MIDI From** `TheoryBoard MIDI`, **Monitor**
`In`. On the board, Settings → Expression → *MPE* Melody, Settings → Expression → *Melody AT* Channel AT. Hold two
melody pads and lean into one: only its note changes.
**Why use it:** each finger its own expression, as on an MPE keyboard: swell one note of a chord,
lean into a melody note while the others stay put.
**When not to:** with a synth that doesn't do MPE: it may hear only one channel, or play every
channel with the same sound and no per-note pressure. Use Poly AT or Channel AT then.

### Glide *(optional)*
*1.8.* Settings → Expression → *Glide*, default Roll *(1.8.6; Off before)*. With *MPE* Melody or Both, in the Scale, Chromatic,
Fourths or Live notes layout (not Bass or Drums) and with *Note repeat* Off, touching the pad beside a
held melody pad (left or right, in the same row) doesn't strike a new note: the held note slides to
the new pad's. *Glide* chooses how far, and how fast:

| *Glide* | The note slides … |
| --- | --- |
| Roll | by which pad you press harder, like a finger rolled across them: evenly pressed, halfway; the new pad clearly harder (about half of full pressure more), exactly on its note, even with a finger still on the first. *(1.8.4)* |
| Pad *(1.8.5)* | as far as you press the new pad, whatever the first one does: a resting touch doesn't bend, full pressure is exactly the new note. The new pad works like a pitch wheel. |
| Time *(1.8.5)* | all the way, over *Glide time* (25 ms to 1 s, default 100 ms; shown only with Time): classic portamento. Pressure doesn't matter. |

- Lift the first pad and the note carries on at the new pad's note, without a new attack (with Time,
  it keeps going there). Roll on to the next pad along and it slides on; lift the last pad and the
  note ends.
- Lift the new pad instead and the note slides back (with Time, over the same time).
- Every glide comes to rest exactly on the note, in tune with the synth's MPE bend range at 48.
- Pads further away strike their own notes as usual; with [Mono](#mono) the gliding pair stays.

**Try it:** set up MPE as above, *Glide* Roll. Hold the bottom-left melody pad (C) and roll your finger
onto the pad to its right (D) and back: the pitch slides between C and D. Roll onto D, lift C, roll
on to E: one note, sliding C, D, E. *Glide* Pad: hold C, press D slowly harder: the pitch follows D's
pressure. *Glide* Time: hold C, tap D: the note slides up and back, at the same speed each time.
**Why use it:** slides and bends between scale notes, like a fretless string or a ribbon.
**When not to:** fast trills between neighboring pads become slides; turn Glide Off for those.

### Pressure (aftertouch)
Settings → Expression → *Chord AT* and *Melody AT*: what pressing harder on a held pad sends,
set separately for each half.

| Setting | Sends |
| --- | --- |
| Off | nothing from that half |
| Poly AT (default) | polyphonic aftertouch for each note the pad plays. A chord pad sends it for each of its notes, so leaning into a chord swells all of it. A note that another held pad also plays gets none, to keep it unambiguous. |
| Channel AT | channel aftertouch: the strongest held pad of that half |
| CC1 mod | mod wheel, from the strongest held pad of that half |
| CC2 breath / CC11 expr / CC74 filter | that controller, the same way |
| Pitch bend | bends up as you press harder (the strongest held pad of that half), back to the center on release |

With [MPE](#mpe-optional) on for a half, each pad sends its own pressure on its own channel instead.

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
| Firmer *(1.8)* | quieter still: a press halfway to full plays at about an eighth of full velocity (Firm: a quarter) |
| Firmest *(1.8)* | halfway plays at about a sixteenth: the widest quiet and middle range, only your hardest presses get loud |
| Custom *(1.9)* | a curve you draw in the web editor's **Feel** tab, with your own softest and loudest velocity ([Your own feel](#your-own-feel-optional)); until then about Normal |
| Fixed 100 | every note at 100, however you play (*Chord vel*: progression chords too) |

**What velocity does not change: how light a touch can start a note.** A pad starts a
note once its reading reaches the *Trigger* point; the velocity setting only shapes how
loud the note is after that, from velocity 1 at *Trigger* to 127 at *Full* (or at *Vel full*, 1.8). A touch too
light to start a note stays silent with every velocity setting: lower *Trigger* instead
([Pad sensitivity](#pad-sensitivity)).

**Try it:**
1. A piano or synth sound on the computer, *Melody vel* Normal. Play one melody pad several
   times, from a gentle tap to a firm press: the notes go from quiet to loud.
2. Set *Light* and tap gently again: the gentle taps are now clearly louder.
3. Set *Firm*: the same gentle taps are very quiet; only firm presses get loud.
   *Firmer* and *Firmest* (1.8) go further. If even those play your medium presses at full
   velocity, the press is reaching *Vel full*: raise it (Settings → Pads, [Pad sensitivity](#pad-sensitivity)).
4. Set *Fixed 100*: every note sounds the same, however you play. Back to Normal.
5. *Chord vel* Fixed 100, *Melody vel* Normal: chords at one even level however you hit
   them, the melody as dynamic as your fingers. The play screen's bottom line shows both
   (`V FIX/DYN`; Custom shows as `CUS`).

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
again and, while it runs, save something to the board from the web editor (for example a user
scale): the DAW pauses for about a second and then continues from where it stopped.
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

- Settings → Pedal → *Mode* (**Sustain** by default from 1.7: a fresh install, *Reset options* on
  the Pedal page or *Factory settings*; a board that saved a mode before keeps it, Off included):
  - **Sustain:** sends sustain (CC64) on the chord and melody channels; the screen shows
    `SUS` while it is down.
  - **Hold:** the board's own [hold](#hold) while the pedal is down: chords keep sounding,
    and the arpeggiator keeps running.
  - **Sostenuto / Soft / Portamento:** sends CC66, CC67 or CC65 instead.
  - **Play/Stop:** each press sends Start, then Stop.
  - **Fill** *(2.0.3)*: the step sequencer's [Fill](#step-options) steps play while the pedal is down, *Not fill*
    steps rest.
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
panel, hold, strum, arpeggiator, tempo, layouts, channels, pressure and velocity, and (1.7) each
half's [Program Change](#program-change-optional). The board settings (brightness, colors, MIDI in
lights, TRS out, pedal, arrows, *Program in*) are not part of presets. There are 8 slots. A DAW can
also load presets with Program Changes (Settings → MIDI → *Program in*).

- **Save:** Settings → Presets → *Save preset* → ● → ▲/▼ choose 1–8 → ●. Saving over a
  slot replaces it (`PRESET 3 SAVED`).
- **Clear** *(2.0)*: Settings → Presets → *Clear preset* → ● → ▲/▼ choose 1–8 → ● (`PRESET 3 CLEARED`).
  It empties the slot along with the preset's own colors, curves, chord set and name. If it was the preset in use, the
  board's own colors, curves and chords come back; the settings playing now stay as they are.
- **Load:** Settings → Presets → *Load preset* → ● → choose a slot → ●. Empty slots show
  `EMPTY`; *(1.9.3)* a preset named in the web editor shows its name (`3 FUNK`; a name too long to fit
  beside the item fills the line while you choose).
- **Quick switch while playing:** hold **◀ Left** and press **▲ Up / ▼ Down** to load the
  previous / next saved preset (`PRESET 3 LOADED`, and `P3` near the scale name).
- A preset that uses a user scale which has since been cleared will not load.
- *(1.9)* A preset can have its own pad colors (*Keep look*, [Your own colors](#your-own-colors-optional)) and,
  from 1.9.3, its own velocity and pressure curves (*Keep feel*, [Your own feel](#your-own-feel-optional)).
- *(1.9.3)* The web editor's [Presets tab](#presets-tab) names, copies, swaps, clears and compares presets.

**Try it:** set up D Dorian with strum on and save it as preset 1. Change to G major,
strum off, save as preset 2. On the play screen hold ◀ Left and press ▲/▼: the board
flips between the two setups.
**Why use it:** one preset per song, switched between songs (or between verse and
chorus) without touching the menu.

### Presets with their own chord sets *(optional)*
*(2.0.)* The 8 [chord sets](#chord-sets-optional) belong to the board: every preset that plays chord set 1 plays the
same chord set 1. With Settings → Presets → **Keep chords** On, *Save preset* also saves a copy of the chord set the
preset plays (1–8) with the preset. Loading the preset puts that copy in its set's place; loading a preset without
one brings the board's set back. While a preset's copy plays, changing that set (Chord capture, or the editor's
Presets tab) changes the preset's copy, and the change is saved before another preset's set replaces it.

A preset that plays the scale's chords, Full or the chord grid has no chord set to keep. *Keep chords* is a board
setting. Saving with *Keep chords* Off takes a preset's own copy away.

The web editor's [Presets tab](#presets-tab) shows which presets have their own set, gives one a copy of its set
as it plays now (**Set own chords**) or takes it away (**Remove own chords**); Copy, Swap and Clear take it along,
and Compare lists it. Backups include them (restoring into TBOSS 1.9 or older leaves them out).

**Try it:** chord set 1 with your verse chords, *Keep chords* On, save preset 1. Change chord set 1 (in the editor
or by capturing) to other chords, save preset 2. Load preset 1: the first chords; preset 2: the second.
**Why use it:** one chord set slot, a different version per song.

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

*(0.10.0; 1.7: counted from each pad's resting level)* Settings → **Pads** decides how a pad's
pressure turns into notes. Each pad measures how hard it is pressed as a number: a low number when
untouched (a few hundred on a typical board, more on a pad that is dented or bent), rising to about
4000 when pressed very hard. The board measures where each pad rests, and the settings count from
there, in steps of 100:

| Setting | Range | Default | What it does |
| --- | --- | --- | --- |
| *Trigger* | +500 to +2000 | +700 | a note starts when a pad's reading rises this far above where it rests. Lower = a lighter touch plays. |
| *Release* | +100 to +1500 | +300 | the note ends when the reading falls back below this; pressure (aftertouch, CCs, pitch bend) counts from here. Lower = you can relax further while holding a note. |
| *Full* | +1000 to +4000 *(+4000 from 1.8.6)* | +2500 | velocity 127 and full pressure (at most 4095). Lower = loud notes and full pressure come with less force. |
| *Vel full* *(1.8)* | = Full, +1000 to +4000 | = Full | velocity 127 here instead of at *Full* (at most 4095); pressure still ends at *Full*. Higher = loud notes need a harder press, and the quiet and middle range is wider. |
| *Unstick* | On / Off | On | a note also ends when its pad's reading stays still (within 30: 0.3 s below *Trigger*, 1 s above), as a stuck pad does and a finger never does ([below](#pad-sensitivity)). |

With the defaults, a pad resting at 300 plays from 1000 and lets go below 600, and a pad resting
at 1000 plays from 1700 and lets go below 1300: every pad takes the same press above where it rests.
*Trigger* has to stay at least 200 above *Release* (so a finger hovering at the edge can't
make a note stutter), and *Full* at least 500 above *Trigger*; a value that breaks this
shows `OUT OF RANGE` and the old value stays.

**Updating from 1.6 or earlier:** your settings are converted. The old defaults (*Trigger* 1200,
*Release* 600, *Full* 3500) become the new ones, and a value you changed keeps its distance from
them (*Trigger* 1500 becomes +1000). Going back to 1.6 converts them back. Up to 0.9.1 the levels
were fixed at 2500 / 1500 / 4000; the closest now is *Trigger* +2000, *Release* +1200, *Full* +3000.

**The board follows each pad.** Each time it starts playing (at power-up, and after the web
editor has talked to it), it measures how high each untouched pad rests. While a pad rests, the
board keeps following its level, so a board that flexes (moved from a desk to your lap) keeps
working; a press is never taken for rest. It follows a pad up within a few seconds and down within
about two (1.7; before, down at once, so a moment of low readings could leave a pad playing by
itself). The [Pad test](#pad-test) page shows each pad's resting
level. *(1.5 measured each pad but only raised the thresholds of pads resting high, so they needed
less force than the others to set off; 1.7 counts every threshold from each pad's rest.)*

**Bleed** *(1.7)*: a firm press also lifts the pads around it a little. A pad next to one
you press that rises less than half as far is taken for that press's bleed and stays silent; a pad
you really press rises further and plays. A very light touch right next to a much harder press can
be taken for bleed too: press it a little more.

**Unstick** *(1.7)*: on a bent case or a worn membrane, some pads don't come all the way
back after a press. They settle higher than they rested, sometimes above *Release*, and the note
would never end. With *Unstick* on (the default), a note also ends once its pad's reading has stayed
within 30 of where it is: for 0.3 s below *Trigger*, or for 1 s at *Trigger* or above. A pad left
reading high holds within a few steps; a finger always wanders more (50 to 75 or more), so held
notes and their aftertouch stay, however long you hold them. The level the pad stopped at becomes
its resting level, so it stays quiet and the next press lets go too. A reading of 2000 or more is
always a press, never a stuck pad. Turn *Unstick* off only if you hold notes so still that they end
by themselves: a note then ends only below *Release*.

**Rescan pads** *(1.7)*: Settings → Pads → *Rescan pads* → ● measures every pad's resting
level again at once (`PADS RESCANNED`), as at power-up. Use it when a pad plays by itself or
skips after the board was moved or flexed: take your hands off the pads first. A pad held while
you press ● gets the board's middle level, then follows its own as usual.

**Try it (find your own Trigger):**
1. Settings → Pads → *Pad test* → ● Center. Hands off: the three pad numbers are your
   resting readings, the last number on each row that pad's resting level, and the bottom line
   `REST` the lowest and highest the board measured.
2. Touch a pad as lightly as you'd like to play. Its number appears at the top of the
   list; `PEAK` shows how far your touch went (say `PEAK 900`, on a pad resting at 300: it rose 600).
3. ◀ Left back to the list, ▼ Down to *Trigger*, ● Center, then ▼/▲ to a little below
   how far your light touch rose (here +500) and ● Center to set it. ▼ Down to *Pad test* and
   ● Center to open it again.
4. Back in *Pad test*, touch lightly again: the `*` appears and the note plays, at a low
   velocity (`LAST VEL`).
5. Press a pad firmly and watch the second and third rows: those are its neighbors. They
   stay silent unless they rise past *Trigger* and more than half as far as your press. If
   one still plays by itself, raise *Trigger* until it doesn't.
6. Press as hard as you'd normally play your loudest notes and read `PEAK`. Set *Full* a
   little below how far that rose, so full pressure is easy to reach.
7. *(1.8)* If your medium presses already play at velocity 127 (`LAST VEL 127`), set *Vel full*
   to about how far your hardest press rose (`PEAK` minus the pad's resting level, the number at
   the end of its row): with a pad resting at 300 and `PEAK 3800`, *Vel full* +3500. Then
   velocity keeps rising past *Full*, and *Firm*, *Firmer* or *Firmest* spread the quiet and middle
   range further.

**Why use it:** match the board to your touch: light and fast for melodies, firmer if you
rest your fingers on the pads or play with a heavy hand. Boards differ, and a soft overlay
or a hardware modification changes how a press spreads; these settings let every board
play its best. They belong to the board: presets don't change them.

**When not to:**
- Don't lower *Trigger* below how far your neighbors rise when you press firmly
  (step 5): pads next to the one you play could start sounding by themselves.
- Don't set *Release* to +100 or +200 on a board whose resting readings jitter: notes may
  not end reliably.
- **With a sensitivity modification or a thick overlay**, start with *Trigger* around
  +1000 and check step 5. A modification that presses the membrane closer also spreads each
  press to the neighbors.

### Pad test

Settings → Pads → *Pad test* shows what the pads measure, as numbers: how hard a touch
reads, how much a press spreads to the neighboring pads, and the velocity a note got. It
changes nothing; the pads keep playing while the page is open. *(0.9.1; on the About page
until 0.10.0.)*

```
PAD 1120/720/2920          Trigger / Release / Full of the highest pad: the settings
                           above its resting level (here 420)
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
   neighboring pads' numbers rise a little as well. Below their *Trigger*, or rising less than
   half as far as your press, they never play.
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
progressions and the step pattern stay. The chord edits stay (*Reset all edits* on the Chord edits page
clears them), and on the Progressions and Steps pages the progressions and the step pattern's lanes do
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
|  | Family *(opt)* | Major / Mel minor / Harm minor / Harm major: what M1–M7 play | Major |
|  | Hold | Off / On | Off |
|  | Chord vel | Normal / Fixed 100 / Light / Firm / Firmer / Firmest *(1.8)* / Custom *(1.9)*: the chord pads ([Velocity](#velocity)) | Normal |
|  | Melody vel | the same, for the right half | Normal |
|  | Tempo | 40–240 BPM: arpeggiator, note repeat, strum in note values, clock out ([Tempo](#tempo)) | 120 |
|  | Arrows | what ▲/▼ do on the play screen: Scale / Melody octave / Preset / Tempo / Sequencer (▲ plays or stops the [progression](#progressions), ▼ records or finishes) (a board setting: presets don't change it) | Scale |
|  | Restrike *(opt)* | Off / On ([Restrike](#restrike)) | Off |
| Layout | Layout | Scale / Chromatic / Fourths / Drums / Live notes / Bass ([Bass](#bass-optional), 1.7) | Scale |
|  | Chords on | Left / Right: which half the chord pads are on ([Chords on the right](#chords-on-the-right-optional); a board setting: presets don't change it) | Left |
|  | Melody oct | −2 … +2 | 0 |
| Chords | Chord set *(opt)* | Scale / 1–8 (`EMPTY` until typed in the web editor) ([Chord sets](#chord-sets-optional)) / Full ([Full chord layout](#full-chord-layout-optional)) / Grid ([Chord grid](#chord-grid-optional)) | Scale |
|  | Capture *(opt)* | Set 1–8: ● starts capturing chords into that set ([Chord capture](#chord-capture-optional)) | |
|  | Voicing | Off / I / II / III / Guitar (the panel's) | Off |
|  | Spread | Off / I / II / III (the panel's) | Off |
|  | Spread of *(opt)* | Root / Chord / Bass: what Spread spreads ([Spread](#spread-i--ii--iii)) (1.7; Bass 2.0) | Root |
|  | Voice lead *(opt)* | Off / On ([Voice lead](#voice-lead-optional)) | Off |
|  | Fold *(opt)* | Off / On ([Fold](#fold-optional)) | Off |
|  | Hints *(opt)* | Off / On ([Theory hints](#theory-hints-optional)) | Off |
|  | Pivot to *(opt)* | Off / C major … B major / C minor … B minor ([Pivot chords](#pivot-chords-optional)) | Off |
| Chord edits | Add 7th, Add 9th, Add 6th | Off / On (last chord played) | Off |
|  | Sus | Off / SUS2 / SUS4 (last chord played) | Off |
|  | Strum pad | On / Off (last chord played) | On |
|  | Sound *(opt)* | On / Off (last chord played): Off picks the live notes without a sound | On |
|  | Reset chord / Reset all edits | ● asks, then clears the last chord's / every chord pad's edits | |
| Strum | Strum *(opt)* | Off / Up / Down / Up+Down | Off |
|  | Strum time | 10, 15, 20, 30, 40, 60, 80, 120 ms, 1/96, 1/48, 1/32, 1/24, 1/16 | 20 ms |
|  | Repeat | Off, 1/4, 1/8, 1/16, 1/32 | Off |
|  | Cutoff | Off, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 | Off |
|  | Feel time *(opt)* | Off, 0–5, 0–10, 0–20, 0–40 ms late at random | Off |
|  | Feel vel *(opt)* | Off, ±5, ±10, ±20, ±30 velocity at random | Off |
|  | Capo | Off, fret 1–11 (guitar voicing) | Off |
| Expression | Chord AT | Off / Poly AT / Channel AT / CC1 / CC2 / CC11 / CC74 / Pitch bend | Poly AT |
|  | Melody AT | the same | Poly AT |
|  | MPE *(opt)* | Off / Melody / Chords / Both: a channel per pad ([MPE](#mpe-optional), 1.8; the halves on 1 and 16) | Off |
|  | Glide *(opt)* | Off / Roll / Pad / Time: a melody pad beside a held one slides its note ([Glide](#glide-optional), 1.8) | Roll |
|  | Glide time *(opt)* | 25, 50, 100, 150, 250, 400, 600 ms, 1 s: how long a Time glide takes (only with *Glide* Time) | 100 ms |
| Arpeggiator | Arp *(opt)* | Off / Up / Down / Up-down / Random / As played / Chords / Turing *(2.0)* | Off |
|  | Lock *(2.0)* | 0–100 %, in steps of 10: how much of the Turing loop stays each time round (100 %: the same loop); only with *Arp* Turing ([Turing pattern](#turing-pattern-optional)) | 80 % |
|  | Loop *(2.0)* | 2–16 steps: the Turing loop's length; only with *Arp* Turing | 8 steps |
|  | Arp rate | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/4T, 1/32T, 5/beat, 7/beat, 1/8., 1/16. | 1/16 |
|  | Arp octaves | 1–3 | 1 |
|  | Arp gate | 25 / 50 / 75 / 100 % | 50 % |
|  | Rhythm | 8 patterns: `XXXXXXXX`, `X.X.X.X.`, `.X.X.X.X`, `XX.XX.X.`, `X..X..X.`, `XXX.XXX.`, `X.XX.XX.`, `X...X...`; then the Euclidean `1/2` … `15/16` (up to 8 steps shown with their pattern) | `XXXXXXXX` |
|  | Rotate | 0–15 | 0 |
|  | Chance *(opt)* | 100 / 90 / … / 10 %: arpeggiator steps and note repeats ([Chance](#chance)) | 100 % |
|  | Swing | 0, 10, 20, 30, 40, 50 % | 0 % |
|  | Sync start | Off / On | Off |
|  | Latch | Off / On | Off |
| Effects | Note repeat *(opt)* | Off / 1/4 / 1/8 / 1/16 / 1/32 / 1/8T / 1/16T / 5/beat / By row ([Note repeat](#note-repeat)) | Off |
|  | Repeat rhythm *(opt)* | All / the Euclidean rhythms `1/2` … `15/16` | All |
|  | Harmony *(opt)* | Off / 3rd / 4th / 5th / 6th / Octave / 3rd+5th / 3rd below / 6th below ([Harmony](#harmony)) | Off |
|  | Echo *(opt)* | Off / 1/16 / 1/8T / 1/8 / 1/8. / 1/4 / 1/4. / 1/2 ([Echo](#echo)) | Off |
|  | Echoes | 1–8 | 3 |
|  | Mono *(opt)* | Off / On: the melody half one note at a time ([Mono](#mono)) | Off |
| Progressions | Play | ● plays / stops: the lanes and the progression, as *Transport* ([How to play it](#step-sequencer-optional), 1.8) | |
|  | Chain *(2.0)* | Off / On: Play plays the chain (set in the web editor) instead of the selected progression; Shift + root button 9 too ([Chained progressions](#chained-progressions-optional)) (a board setting) | Off |
|  | Progression | 1–8 (`EMPTY` when nothing is recorded there): the slot ▲ plays and *Record* records ([Progressions](#progressions)) | 1 |
|  | Unit | 1 beat / ½ bar / 1 bar / 2 bars: one tap's length | 1 bar |
|  | Record | ● starts recording into the slot (replacing it); ● again finishes | |
|  | Clear | ● empties the slot | |
|  | Guide | Off / On: guide lights while a progression plays | On |
| Steps | Steps | ● opens the step screen ([Step sequencer](#step-sequencer-optional)) | |
|  | Lane | L1–L4: the lane the next items and the step screen work on | L1 |
|  | Kind | Chords / Melody / Drums / Off | Chords |
|  | Length | 1–64 steps | 16 |
|  | Rate | a step's length: the arpeggiator's rates | 1/16 |
|  | Direction *(1.7)* | Forward / Reverse / Ping-pong / Random | Forward |
|  | Channel | AUTO (the kind's channel) / 1–16 | AUTO |
|  | Mute | Off / On | Off |
|  | Swing *(2.0)* | Off / 5 % … 50 %: every second step later ([Step options](#step-options)) (2.0.3) | Off |
|  | Transpose *(2.0)* | −24 … +24: a chord lane in scale steps, the others in semitones (2.0.3) | 0 |
|  | Euclid *(1.7)* | Off / 1 … the length: fills the lane ([Step options](#step-options)) | Off |
|  | Rotate *(1.7)* | 0 … the length − 1 (not stored) | 0 |
|  | Mutate *(2.0)* | An action: ●, ▲▼ `10% OF STEPS` … `50% OF STEPS`, ● changes about that many steps of the lane a little, once (2.0.3; on the board only) | 30 % |
|  | Step pads *(1.9)* | Sound / Silent: the step screen's left half plays what you touch, or only chooses it ([The step screen](#the-step-screen)) (a board setting) | Sound |
| Presets | Load preset | 1–8, with each preset's name *(1.9)* | |
|  | Save preset | 1–8, with each preset's name *(1.9)* | |
|  | Clear preset *(2.0)* | 1–8: ● empties the preset, with its own colors, curves, chord set and name | |
|  | Keep look *(1.9)* | Off / On: *Save preset* also saves the pad colors with the preset ([Your own colors](#your-own-colors-optional)) (a board setting) | Off |
|  | Keep feel *(1.9)* | Off / On: *Save preset* also saves the velocity and pressure curves with the preset ([Your own feel](#your-own-feel-optional)) (a board setting) | Off |
|  | Keep chords *(2.0)* | Off / On: *Save preset* also saves a copy of the chord set the preset plays ([Presets with their own chord sets](#presets-with-their-own-chord-sets-optional)) (a board setting) | Off |
| MIDI | Chord ch | 1–16 | 1 |
|  | Melody ch | 1–16 | 1 |
|  | Bass ch | 1–16 (drum layout's middle grid) | 2 |
|  | Drum ch | 1–16 (drum layout's drums, pedal note) | 10 |
|  | Live ch *(opt)* | Melody / 1–16: the live notes' channel | Melody |
|  | Live in *(opt)* | Off / 1–16 / All: chords arriving over MIDI pick the live notes | Off |
|  | Chord prog *(opt)* | Off / 1–128: the chord half's Program Change ([Program Change](#program-change-optional), 1.7) | Off |
|  | Melody prog *(opt)* | Off / 1–128: the melody half's | Off |
|  | Program in *(opt)* | Off / CH 1–16 / All: programs 1–8 arriving there load presets 1–8 (a board setting) | Off |
|  | TRS out | Off / On | On |
|  | Clock out *(opt)* | Off / On | Off |
|  | Transport | ● Play / Stop (also plays the [progression](#progressions)) | Stopped |
|  | Scale lock *(opt)* | Off / CH 1–16: notes arriving on that channel play in the key ([Scale lock](#scale-lock-optional)) (a board setting) | Off |
| Pedal | Mode | Off / Sustain / Hold / Sostenuto / Soft / Portamento / Play/Stop / Note / Fill (2.0.3) | Sustain (1.7; Off before) |
|  | Type | Auto / Normal / Inverted | Auto |
|  | Note | any MIDI note; shown only while *Mode* is Note | C2 (36) |
|  | Target | Both / Chords / Melody: whose channel the pedal's controller goes to | Both |
| Buttons | Voicing I … Octave +3 *(opt)* | what that center button does ([Center buttons and Shift](#center-buttons-and-shift-optional)) (board settings: presets don't change them) | its printed function |
|  | U3 button *(2.0)* | U3 scale / Shift: the top right button plays user scale U3, or is Shift ([U3 as Shift](#u3-as-shift-optional)) (a board setting) | U3 scale |
|  | Reset all | ● asks, then every center button back to its printed function (2.0: and U3 to its scale) | |
| Lights | Brightness | 1–8 | 4 |
|  | Colors | Default / Root+5th / Rainbow / Piano / Triad / Function / Octaves *(the last four 1.9)* ([Screen and lights](#screen-and-lights)) | Default |
|  | Color by *(1.9)* | Note / Degree: Default's colors by note name (C always red) or by scale degree (the root always red) | Note |
|  | MIDI in LEDs | Off / On | On |
|  | Sleep after *(2.0)* | Off / 5 / 15 / 30 / 60 MIN: the screen and lights dark after that long without a touch ([Sleep and Shut down](#sleep-and-shut-down-optional)) (a board setting) | Off |
|  | LED test | Start | |
| Pads | Trigger | +500 to +2000 above each pad's resting level, in steps of 100: a note starts here ([Pad sensitivity](#pad-sensitivity)) | +700 |
|  | Release | +100 to +1500: the note ends below this; pressure counts from here | +300 |
|  | Full | +1000 to +4000: velocity 127 and full pressure | +2500 |
|  | Vel full *(1.8)* | = Full / +1000 to +4000: velocity 127 there instead ([Pad sensitivity](#pad-sensitivity)) | = Full |
|  | Unstick *(1.7)* | Off / On: a note also ends when its pad's reading stays still, as a stuck pad's does ([Pad sensitivity](#pad-sensitivity)) | On |
|  | Pad test | Start: the pad readings, the peak of a touch, the last note's velocity ([Pad test](#pad-test)) | |
|  | Rescan pads *(1.7)* | ●: every pad's resting level measured again, hands off ([Pad sensitivity](#pad-sensitivity)) | |
| About | Version | the TBOSS version, e.g. `2.0` | |
|  | I/O test | Start: shows the pedal input and two alternative inputs live, sends middle C to the TRS jack every second, and shows the buttons held *(2.0)* and pads that read as held | |
|  | Shut down *(2.0)* | ●: every note stops, the screen and lights go dark, pads and buttons do nothing until ● Center ([Sleep and Shut down](#sleep-and-shut-down-optional)) | |
|  | Factory settings | ● asks, then every option back to its default; saved presets, scales, chord sets and progressions stay ([Resetting options](#resetting-options)) | |
| Every page but Chord edits, Presets, Steps, Buttons, About | Reset options | ● asks, then that page's items back to their defaults | |

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
4. Octaves (`OCT C2 C3`), `HOLD` when hold is on, `SUS` while the sustain pedal is down, `EXT`
   *(1.7)* while a DAW's MIDI clock comes in (`OCT` is left out when they need its room).
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

**Rainbow** *(1.5.0)*: seven colors across both halves, the same on every row: red, orange,
yellow, green, dark blue, light blue, magenta, then red, orange, yellow, green and dark blue again
in the last five columns, the first red column brightest, whatever root, mode or layout you play.
(Until 2026-10-24 the 1.5.0 download had twelve colors, red to pink; download it again for the
seven.) A pad
you touch still turns white, and a sounding pad (held, latched) soft white. Nothing else
lights: no theory hints, pivot chords, progression guide or MIDI in. The step screen and the
web editor's preview keep their own colors. **Why:** just for fun, a board that looks the
same whatever you play.

*1.9:* four more:

- **Piano**: white keys light, black keys dark, wherever they are on the board, and the root in
  its own color (orange). **Why:** find your way on the chromatic and fourths layouts as on a
  keyboard.
- **Triad**: the key's root orange, its 3rd gold and its 5th blue; every other note dim.
  **Why:** the chord tones of the key at a glance, for melodies that land on them.
- **Function**: each chord by what it does in the key: tonic chords (I, iii, vi) green,
  subdominant (ii, IV) gold, dominant (V, vii) red; the melody pads by the same degrees.
  **Why:** see which chords lead home and which move away.
- **Octaves**: every root bright orange, everything else dim blue. **Why:** a calm board for the
  dark, with the roots as landmarks.

Pressed and sounding pads, chord edits, silent pads, MIDI in, hints and the progression guide show as
in Default.

**Color by** *(1.9, Settings → Lights)*: *Note* (the default) gives each note name its color
everywhere (C red in every key, as before); *Degree* colors by scale degree instead: the root is
always red, the 5th always blue-ish, whatever key you play in. It applies to Default and to the
colors of the hints and the progression guide.

### Your own colors *(optional)*
*1.9.* In the web editor's **Lights** tab every scheme's colors can be changed: each of its roles
(Default's 12 notes, Root+5th's root, 5th, other notes, edited chords and MIDI in, Rainbow's 7
columns, …) takes one of 16 colors: red, orange, gold, lime, green, turquoise, sky blue, blue,
dark blue, violet, purple, pink, white, warm white, gray or off. **Built-in** gives a role its
original color back. The pads follow at once while you choose.

- The colors you pick are the board's own, for every preset, unless a preset has its own look.
- **A preset's own look:** set Settings → Presets → *Keep look* On, then *Save preset*: the preset
  keeps the scheme, Color by and colors showing then. Loading it shows them; loading a preset
  without a look of its own brings the board's back. While a preset's look shows, changing the
  colors changes that preset's look. Saving the preset again with *Keep look* Off removes its look.
- The colors are in the settings backup (firmware 1.9 and later).

**Try it:** web editor → Lights → *Root+5th*, set its **Root** to white and **5th** to green: the
roots light white on the board as you click.

### Your own feel *(optional)*
*1.9.* The web editor's **Feel** tab shapes how the pads respond, for each half on its own:

- **Velocity:** a curve from a strike just past *Trigger* (left) to one at *Vel full* (right), and the
  softest and loudest velocity it plays. It applies where Settings → Play → *Chord vel* or *Melody vel*
  is **Custom** (the tab's **Use the curve** button sets that). Start from one of the built-in curves (Normal,
  Light, Firm, Firmer, Firmest) and drag its 8 points.
- **Pressure:** what a held pad sends as you press on (aftertouch, a controller or a bend, and with
  [MPE](#mpe-optional) each pad's pressure), from *Release* (left) to *Full* (right). It always applies;
  the default straight line is exactly as before 1.9. Bend it to make pressure come in late, early or
  all at once, and set where it starts and where it is full.
- The board takes each change at once and keeps playing; the curves are the board's own, for every preset,
  and are in the settings backup.
- **A preset's own curves** *(1.9.3)*: set Settings → Presets → *Keep feel* On, then *Save preset*: the
  preset keeps the curves in use then. Loading it uses them; loading a preset without curves of its own
  brings the board's back. While a preset's curves are in use, the Feel tab says so and changes go to that
  preset. Saving the preset again with *Keep feel* Off removes its curves. The Presets tab does the same
  without saving the preset (**Set own curves**, **Remove own curves**).
- **Curves of** *(2.0.8)*: the Feel tab's list of whose curves you edit: the board's, or any preset's own,
  without loading it. When the board loads a preset, the list goes to the curves it plays (*· playing now*).
  A preset without its own is shown dimmed, with **Give preset N its own curves**.
- **Each preset keeps its own Chord vel and Melody vel**, saved with it, and loading it brings them: a preset
  saved while Chord vel wasn't Custom plays its curve only once that changes. With a preset chosen under
  *Curves of*, **Use the curve in preset N** sets Custom in the preset itself, without loading it; giving a
  preset its own curves sets its Chord vel to Custom too.
- **Calibrate:** the tab asks you to keep your hands off, then to press pads softly, at your usual
  strength and as hard as you ever play (about 30 seconds, the board silent meanwhile). It then proposes
  *Trigger*, *Release*, *Full*, *Vel full* and a velocity curve that plays your soft presses at about 35
  and your usual ones at about 85, beside the values you have now. **Apply** sets them all (and Custom
  on both halves); nothing changes before that.
- **The pads at rest:** a map of every pad's resting level (what it reads untouched), dark to bright.
  A pad far above the others (outlined) needs a harder press: Settings → Pads → *Rescan pads* with
  your hands off measures them again.

**Try it:** Feel tab → *Chord velocity* → **Firm**, then **Use it**; play a chord pad softly and
firmly. Then drag the rightmost point down to 100: even your hardest strike now plays at most about 100.

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

### Sleep and Shut down *(optional)*
*(2.0.)* **Shut down** (Settings → About → *Shut down*, ●), as on the original TheoryBoard: every note stops, the
transport stops, the screen and every light go dark, and the pads and buttons do nothing, until you press
**● Center**, which only wakes the board (it doesn't open Settings). The board stays connected over USB; MIDI
coming in still reaches it.

**Sleep after** (Settings → Lights: Off, 5, 15, 30 or 60 minutes): the screen and lights go dark after that long
without a touch. Everything else carries on (a progression or an arpeggio keeps playing). Any button or pad wakes
it: a pad also plays, a button only wakes the board.

Both only darken the board: the processor keeps running (the safe low-power modes the chip has would cut the
debugger off), so the board draws a little less current, not none.

**Try it:** Settings → About → *Shut down*, ●: dark. Touch pads, press ▲: nothing. ● Center: the play screen.

## Web editor

The web editor runs in **Chrome or Edge** on a computer (Safari and Firefox lack the MIDI
features it needs). It changes every setting, browses the scale library, keeps your favorite
scales on the board, makes custom scales, types in chord progressions, and backs up the board's
settings. *(1.9)* Tabs along the top pick one area at a time: **Settings**, **Lights** (the pad
colors, see [Your own colors](#your-own-colors-optional)), **Feel** (the curves and calibration, see [Your own
feel](#your-own-feel-optional)), **Scales** (the library, favorites and custom scales), **Chords** (chord sets and,
1.9.3, every chord pad's edits), **Progressions**, **Sequencer** (1.9.3, the step pattern), **Buttons**, **Presets**
(1.9.3) and **Backup**; the page remembers the last one.

### Start it
Open **https://armandhammer.github.io/TBOSS/editor.html** in Chrome or Edge. Nothing to install.

Click **Connect**. Chrome asks whether the page may "control and reprogram your MIDI
devices": choose **Allow**. The header turns to `Connected` and shows the firmware
version. With several MIDI devices, pick **TheoryBoard Editor** in the *Board* list (the editor
picks it by itself). The board answers the editor on that port, so your DAW, listening to
TheoryBoard MIDI, never sees the editor's messages; boards before TBOSS 2.0 have only
TheoryBoard MIDI.

The board stops its notes while the page reads from it or saves to it and plays again about a
second after the last command, so don't play then. Changing a setting on the Settings tab, colors,
curves, steps or a chord pad's edits doesn't stop it.

To run the editor from your own copy instead (for example offline), from the folder you
downloaded TBOSS into: `python scripts/tboss_editor.py`. It starts a small web server on
your computer only and opens the editor at `http://localhost:8000`; keep its window open
(Ctrl+C stops it). A page opened straight from the file will not work: browsers allow
MIDI with SysEx only for `localhost` or `https` pages.

### Settings
*(1.9; needs TBOSS 1.9 on the board.)* Every page of the board's Settings menu (Play, Chords,
Layout … About), with every item, its value as the board has it now, what it does and its default.
Pick a value and the board takes it at once, as if you had set it on the board: it keeps playing
and its screen shows `CHANGED BY EDITOR` (or why it refused, as the menu would). Each item also
names where it is on the board (`On the TB: Settings → Play → Chord vel`).

- **Play / Stop** (MIDI → *Transport*, Progressions → *Play*) start and stop the board.
- **Load** and **Save** (Presets) load or save the preset you pick; Save asks first.
- Some items are **on the TB only**: the resets, the tests, *Capture*, *Record* and *Clear*,
  *Steps*, *Rescan pads*, and the last chord's edits (*Add 7th* … *Sound*). The page shows their
  value.
- Items the board hides just now (Pedal → *Note* unless *Mode* is Note, *Glide time* unless
  *Glide* is Time) are dimmed.
- Changed something on the board? *(1.9.4)* It shows here within a second, dimmed items too, while the
  board keeps playing (before 1.9.4: when you come back to the tab or window, with a pause of the board's
  notes, or with **Read from the TB**).
- After changing Steps → *Lane*, press **Read from the TB** to see that lane's *Euclid* and
  *Rotate* values.

**Try it:** Settings tab → **Play** → *Chord vel* `Firm`, hold a chord pad while you change it:
the chord keeps sounding, and the next press plays softer.

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
- *(2.0)* **Each chord's chance**, under its name: how often it plays as written, and *else rest* or *else borrow*
  ([Chord chance](#chord-chance-optional)). For the progression saved in the slot the board takes each change at
  once; for one you are typing, with **Save**.
- *(2.0)* **Chain**: up to 8 links, each P1–P8, then **Save chain** ([Chained progressions](#chained-progressions-optional)).

<a id="try-typed"></a>**Try it:** Key A, Natural minor, Each unit 1 bar; type `Am D Dm Am C G E7 Am`; D and E7
are outlined (Dorian, Harmonic minor). **Save to P1**. On the board: root A, M6, Settings →
Progressions → *Progression* 1, ▲ (Arrows: Sequencer): the song plays. Change the key in the
editor to C: the text stays, and is placed again for C minor.
**Why use it:** a song from a chord chart in seconds, with the borrowed chords worked out
for you; and a readable list of what each slot holds. **When not to:** chords that need a
bass note outside the chord, or rhythms finer than the unit: record those in your DAW.

### Chord sets
*(TBOSS 1.0 or later.)* Type the 12 chords of a [chord set](#chord-sets-optional).

1. Choose a set tab, **1** to **8** (the tab shows its name). A stored set appears as text.
2. Pick the key the chords are **Written in** (it only names them: on the board the root
   buttons transpose the set). *Start from* offers six examples (pop, jazz, blues, minor, and the circle of fifths: the 12
   major or the 12 minor chords in circle order, C G D A E B F♯ D♭ A♭ E♭ B♭ F).
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

### Chord pad edits
*(1.9.3; needs TBOSS 1.9.3 on the board.)* On the **Chords** tab, below the chord sets: all 48 chord pads as
they sit on the board (12 columns, 4 rows), each with the name of its chord as the board's screen shows it,
and its [chord edits](#chord-edits):

- **7**, **9**, **6** add the 7th, 9th or 6th; **sus** makes it sus2, then sus4, then plain again.
- **strum** off: the pad plays at once when [Strum](#strum) is on; **sound** off: the pad stays silent.
- **All plain** takes every edit away (asks first).
- The board takes each change at once and keeps playing; the names follow about a second later (before
  1.9.4 the board paused its notes for that, once). While a chord set, the chord grid or the Full layout plays, its chords
  are as typed, so only **strum** and **sound** apply.

**Try it:** in C major, Chords tab → the bottom-left pad (C) → **7**: its name turns into C major 7th; play it.
**Why use it:** set up a song's chord colors in one view instead of playing each pad and editing it.

### Sequencer
*(1.9.3; needs TBOSS 1.9.3 on the board.)* The [step sequencer's](#step-sequencer-optional) pattern as a grid:
four lanes of up to 64 steps (16 to a row).

- **Click a step** to add it (it plays what the lane's chosen step played, or the bottom-left pad, or C2
  (the kick) on a drum lane), or to choose it; **click the chosen step** again to take it away. Steps past the lane's
  length are dimmed.
- **The chosen step's options**, below the lanes: what it **plays** (a chord pad, a melody pad of the Scale
  layout, or a drum note), **Velocity**, **Gate**, **Tie to the next**, **Chance**, **Ratchet**,
  **Condition**, and on a chord lane its **Add 7th / 9th / 6th** and **Sus** ([Step options](#step-options)).
- **Each lane:** kind (Chords, Melody, Drums, Off), channel, number of **Steps**, rate, direction, **Mute**,
  **Clear**, and **Euclid**: a number of hits and a rotation, then **Fill** (as Settings → Steps → *Euclid*
  and *Rotate*: hits already on keep what they play).
- **▶ Play / ■ Stop** start and stop the board's transport (Settings → Progressions → *Play*).
- The board plays each change at once and keeps playing; it saves the steps 3 s after the last change.
  Changed steps on the board? The tab reads them again when you come back to it, or press **Read from
  the TB**.

- *(2.0)* **◀ ▶** beside a lane move its steps one earlier or later (the end comes round to the start; steps past the
  lane's length stay). **Copy** a lane and **Paste** it onto another: the steps and the lane's settings, so the pasted
  lane takes the copied one's length too. **Shift-click** a second step to choose a range; below the lanes **Copy
  steps** copies it, and **Paste … steps from step n** puts it over the steps from the chosen one on (as far as step
  64; only onto a lane of the same kind, since a chord step's value is a chord pad and a drum step's a note).
- *(2.0.3)* Each lane's **Swing** and **Transpose**, a step's **Nudge**, and the *Fill* and *Not fill* conditions
  ([Step options](#step-options)). **Randomize** fills the lane with new steps at random (25–100 % of them on: chord
  lanes the scale's chords, melody lanes its first octave, drum lanes the drums already there), after asking.
  **Mutate** changes about 10–50 % of the steps a little, as Settings → Steps → *Mutate* does, and says beside the
  button how many steps changed (2.0.5).

**Try it:** Sequencer tab → Lane 2 → kind **Drums**, **Euclid** 5, rotate 0, **Fill**; choose one of its steps
and set **Plays** to `D2 Snare`; **▶ Play**. Then **Copy** lane 2, **Paste** on lane 3 and press lane 3's **▶** twice:
the same rhythm two sixteenths later.
**Why use it:** see the whole pattern at once and set step options without holding pads.

### Presets tab
*(1.9.3; needs TBOSS 1.9.3 on the board.)* The board's 8 [presets](#presets), one row each:

- **Name** them (up to 12 characters) and **Save names**: the board's *Load preset* and *Save preset* show
  the names.
- **Load on the TB** loads that preset now.
- **Copy** (over the preset chosen beside it), **Swap** and **Clear** take the preset's colors, curves and
  name along. Copy and Clear ask first.
- **Set own colors** / **Set own curves**: the colors showing or the curves in use become the preset's own
  (loading it uses them); **Remove own colors** / **Remove own curves** gives it the board's again.
  **Keep look** and **Keep feel** are the Presets page's items: with them on, *Save preset* on the board
  also saves the colors and curves.
- **Compare** two presets, or one with the board as it is now, setting by setting, by the names the
  board's menu uses (needs the Settings tab read: it is read when you connect). Only the differences show,
  or every setting with the tick box.

- *(2.0)* **Set own chords** gives the preset a copy of the chord set it plays, as it plays now; **Remove own chords**
  takes it away ([Presets with their own chord sets](#presets-with-their-own-chord-sets-optional)). **Keep chords** is
  the Presets page's item. Copy, Swap and Clear take the copy along; Compare shows each preset's.

**Try it:** name preset 1 `VERSE` and preset 2 `CHORUS`, **Save names**; on the board, Settings → Presets →
*Load preset*: `1 VERSE`, `2 CHORUS`. Compare 1 with 2 to see what differs.

### Center buttons
*(TBOSS 1.0 or later; 2.0: the U3 button too.)* Choose each center button's action from a list, named by what is
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
| Every note is loud, even medium presses | Your presses reach *Vel full* (= *Full* by default). Settings → Pads → *Vel full*: set it near your hardest press (Pad test: `PEAK` minus the pad's rest), then try *Chord vel* / *Melody vel* Firm, Firmer or Firmest ([Velocity](#velocity)), or let the web editor's Feel tab calibrate them ([Your own feel](#your-own-feel-optional)). |
| I entered steps but nothing plays | The steps play while the transport runs. On the step screen tap ● Center (`O PLAY` on the bottom line), or Settings → Progressions → *Play* ● ([How to play it](#step-sequencer-optional)). A lane whose *Kind* is Off, or *Mute* On, stays silent. |
| A held melody pad keeps re-striking | Note repeat is on: Settings → Effects → *Note repeat* → Off. |
| A melody pad doesn't retrigger a note the held chord is already playing | Both halves are on the same MIDI channel, so the note is shared (see [Channels](#channels)). Give the melody its own channel: Settings → MIDI → *Melody ch* → 2, and set the synth to receive all channels (or use two tracks). |
| Chords sound in the wrong octave or very wide | The chord panel's octave or spread: tap octave **0** and press the lit spread again; Settings → Chords shows both. |
| Chords don't start on their root / sound "inverted" | *Voice lead* or *Fold* is on (Settings → Chords), or a voicing button is lit. |
| A chord pad makes no sound but changes the live notes | Its *Sound* is Off: play it, then Settings → Chord edits → *Sound* On. |
| Right half plays odd notes in columns 8–12 | The Live notes layout: Settings → Layout → *Layout* → Scale. |
| ▲/▼ change the scale | That's the default; Settings → Play → *Arrows* chooses melody octave, preset, tempo or sequencer instead. Hold ◀ Left with ▲/▼ switches presets. |
| `NO PROGRESSION YET` | The selected slot is empty: record one, or pick another slot in Settings → Progressions → *Progression* (empty slots say `EMPTY`). |
| `NOT WHILE RECORDING` | A progression is recording, and it keeps one chord set: finish it first (▼ or ● Center), then change the set or load the preset. |
| `NOTHING RECORDED` | A recording finished with no chords in it, so the slot kept what it had. Tap chord pads after starting to record. |
| `NOT SAVED: RESTART` | The board's settings memory filled up during a very long session (hundreds of saves). What you did still plays; switch the board off and on (it tidies the memory as it starts) and save again. |
| A progression plays the wrong chords | It plays in the board's current key and mode: set the root and mode it was recorded or typed in. Borrowed chords keep their own scale. |
| A progression doesn't start with the DAW | The DAW must send MIDI clock (with Start/Stop) to the TheoryBoard MIDI port. ▲ (Arrows: Sequencer) also starts it. |
| A progression makes no chords, only single notes | The arpeggiator is on: it arpeggiates the progression. Settings → Arpeggiator → *Arp* → Off. |
| The pedal sustains the wrong half, or both | Settings → Pedal → *Target*; it can only pick one half when *Chord ch* and *Melody ch* differ. |
| All lights dim or flicker together at random | The USB port isn't supplying steady power. Use the board's DC power input, or try another USB port, a short cable or a powered hub. |
| Pads play nothing for a second | The web editor just saved something to the board, or connected (1.9.4; before, any request); it resumes after 1 s. Held notes end, pressure and pitch bend return to rest, and a running transport pauses and continues. |
| Light touches don't play | Lower Settings → Pads → *Trigger* ([Pad sensitivity](#pad-sensitivity)); *Velocity* (Light/Firm) only changes how loud notes are. [Pad test](#pad-test) shows how far your light touches reach. |
| Neighboring pads play by themselves when you press one | 1.7 keeps a neighbor's bleed silent; if one still plays, raise Settings → Pads → *Trigger* above how far the neighbors rise in [Pad test](#pad-test) (around +1000 with a sensitivity modification or a thick overlay). |
| Notes don't end when you lift your finger | Keep Settings → Pads → *Unstick* On (the default), and *Rescan pads* with hands off after moving the board. With *Unstick* on, a note ends about a second after its pad stops moving. If one still doesn't, send its [Pad test](#pad-test) row (its reading, `*`, its resting level) in a [bug report](https://github.com/armandhammer/TBOSS/issues/new/choose); *Rescan pads* frees it. With *Unstick* off, raise *Release*. |
| The pads feel different after updating to 1.7 | Every pad now counts from where it rests, so pads that rest high need the same press as the others (before, less). Your settings were converted; *Trigger* +100 or +200 makes it firmer. |
| The pads feel different after updating to 0.10.0 | The default *Trigger* is lighter now. For the old feel: *Trigger* +2000, *Release* +1200, *Full* +3000 (1.7). |
| A pad plays without being touched, or needs much more force than the others | Open [Pad test](#pad-test) with hands off: a pad reading high when untouched, or a pad whose numbers barely rise when pressed, is a hardware problem with that pad. |
| Notes kept sounding on a hardware synth after unplugging the computer | Update to 0.9.1 or later: with TRS out on and the board still powered, they end a few seconds after unplugging. |
| Editor says MIDI access refused | Chrome's site settings: allow MIDI devices, then Connect again. |
| Editor: "No TheoryBoard reply" | Pick TheoryBoard Editor (before TBOSS 2.0: TheoryBoard MIDI) in *Board*; close other programs holding the port exclusively. |
| The DAW stops hearing the board when the editor connects | In the DAW, turn the **TheoryBoard Editor** port off (Port 2, MIDIIN2 / MIDIOUT2 on Windows); only TheoryBoard MIDI on. |
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
| Bass mode (doubles notes so they can be played with rapid fire in tandem) | ✅ The [Bass layout](#bass-optional): every scale note on two pads, each hit strikes again (1.7) |
| Arpeggiator | ✅ 6 patterns, rate, octaves, gate, 8-step rhythms, swing, sync start, latch, follows MIDI clock ([Arpeggiator](#arpeggiator)) |
| Step sequencer (chords and/or single notes) | ✅ [Progressions](#progressions) (0.11.0), and the [step sequencer](#step-sequencer-optional) (1.5): four lanes of chords, single notes or drums, played in time with the transport or a DAW |
| Program changes via MIDI CC, NRPN, or pads on their own MIDI channels | ✅ [Program Change](#program-change-optional) out per half and in to load presets (1.7); each half, the live notes and the drum layout's three grids have their own MIDI channel; NRPN not planned |
| Voicing, spread or octave for single chords instead of all (and saved) | 🟡 Per pad: add 7th/9th/6th, sus2/4, strum on/off, sound on/off, saved; voicing, spread and octave apply to all chords |
| Strum types besides up and down | ✅ Up+down, repeat while held, cutoff, per-pad on/off, random feel, an arpeggiator *Chords* pattern |
| Pitch / mod from the pads or the screen | ✅ From pad pressure: pitch bend or CC1 mod (also CC2/11/74) |
| Chords on both sides (same scale or not), mix and match single notes | ❌ Not yet (the right half: scale, chromatic, fourths, drums or live notes) |
| MIDI in over USB | ✅ MIDI clock in, [MIDI in lights](#midi-in-lights), live notes from MIDI in, the web editor |
| Alternative spread: the root note or the entire chord | ✅ Settings → Chords → [*Spread of*](#spread-i--ii--iii) Root or Chord (1.7), or a root added under the chord, Bass (2.0) |
| Turn off octave folding | ✅ TBOSS doesn't fold unless you turn *Fold* on (0.9.0) |
| Live note update via MIDI in when the DAW plays chords | ✅ Settings → MIDI → *Live in* (0.8.0) |
| Latch key: press a pad, latched, press again, unlatched | ✅ [Hold](#hold) (▶ Right), and the arpeggiator's latch |
| Drum kit mode | ✅ Drums layout: scale, bass and General MIDI drum grids, each on its own channel |
| Live note update on its own MIDI channel | ✅ Settings → MIDI → *Live ch* (0.8.0) |
| A chord set to stay silent and still feed Live Note Update | ✅ Settings → Chord edits → *Sound* Off (0.8.0) |
| Note repeat mode (like an MPC) | ✅ Settings → Effects → *Note repeat* for the right half (0.9.0); chord pads: strum *Repeat* |

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

TBOSS 2.0: everything new in 1.4 to 2.0 passed its board tests on one board, with Ableton Live as the DAW.
Known gaps and limits:

- **Not in TBOSS yet:** a phrase recorder and the step sequencer's later parts (per-step nudge, more patterns
  and songs; planned), a second arpeggiator. See the [roadmap](roadmap.md).
- *(2.0)* While U3 is held, the other mode buttons don't answer (they share its touch chip): for Shift + a mode
  button, tap U3 first instead of holding it ([U3 as Shift](#u3-as-shift-optional)).
- *(2.0)* *Sleep after* and *Shut down* only darken the board; it still draws power from USB.
- The arpeggiator has no on/off button of its own: set a [center button](#center-buttons-and-shift-optional)
  to *Arpeggiator on/off*, or use Settings → Arpeggiator → *Arp*.
- A center button held for more than about 5 seconds lets go by itself (the touch buttons'
  own limit), so Shift can be latched and Sustain is tapped on and off. A held chord shape
  (Add 7th …) applies to the next chord pad you touch, not to a chord already sounding (a
  running arpeggio takes it at once from 1.7).
- The chord edits that moved a chord's 3rd or 5th down an octave (0.5 and earlier) have
  no button any more; edits saved with them still play, and *Reset chord* clears them.
- The board appears with a test USB ID (`cafe:4069`) until TBOSS has its own.
- Windows may keep showing an older name for the MIDI port; it is the same device.
- If the MIDI monitor MidiView is running, the web editor's *Connect* can crash Windows (a fault in
  MidiView's own driver while it captures the editor's messages); close MidiView before connecting.
- Installing clears saved settings unless the browser installer's step 2 is used.
- Going back to an older TBOSS: the browser installer (and the editor's *Restore from
  file…*) put back what the older version has. Settings it doesn't have return to their
  defaults, progressions are left out of a version before 0.11.0 and chord sets of a
  version before 1.0.0 (the installer says how many); a version before 1.3.0 plays the scale's
  chords instead of the chord grid or Full, and a version before 1.4.0 gets the arpeggiator's
  and note repeat's 1.0 rates and rhythms (1/16, the first rhythm, note repeat off where a
  new value was set), no pivot target, and *Pivot* buttons back to their printed function; the
  step pattern is left out of a version before 1.5.0, and a version before 1.6.0 gets Harmony,
  Echo, Mono and Chance off (Echoes 3); a version before 1.7.0 plays a step pattern forward, each
  step once at half its length and every time round (the step options are kept for later), and a
  version before 1.8.0 gets MPE and Glide off; preset names and the presets' own curves are left out of a
  version before 1.9.3. Backups restored into versions before 0.10.0 may still be refused if they
  use settings those versions don't have.
- An echo that comes while you still hold its note is not heard (see [Echo](#echo)).
- The external memory chip is only read, never used; the original firmware's data there
  stays untouched.
- On Linux the browser installer needs a one-time permission rule (see
  [Installing](flashing.md#with-a-browser-recommended)).

**Not yet tried on a board:** the web editor's [Settings](#settings) and Lights tabs, [your own colors](#your-own-colors-optional), the Piano, Triad, Function and Octaves schemes and
[your own feel](#your-own-feel-optional) (1.9); the pedal's *Target* and its Hold, Sostenuto, Soft, Portamento
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
5. Bottom-left chord; Settings → Chord edits → *Add 7th* On, then *Reset chord*. [`Dm7`, paler
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
16. Settings → Effects → *Note repeat* 1/16; hold a melody pad and lean in. [repeats, louder
    as you press]; *Note repeat* Off.
17. Settings → Layout → *Layout* Chromatic, Fourths, Drums. [pad colors change; Drums:
    right grid amber, kick on its bottom-left pad on channel 10]; Live notes, play the G
    chord, then the right half's columns 8–12 [G B D G B along the bottom row; play Am and
    they become A C E A C]; back to Scale.
    Settings → Lights → *Colors* Rainbow, then Drums again. [every row of both halves red, orange,
    yellow, green, dark blue, light blue, magenta, then red to dark blue again, whatever the layout; a
    touched pad white]; back to Default and Scale.
18. Settings → MIDI: Chord ch 2, Melody ch 3, a MIDI monitor. [chords on 2, melody on 3];
    both back to 1.
19. Settings → Expression → *Chord AT* Poly AT, hold a chord pad and press harder. [poly
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
22a. Root A, M6. Settings → Progressions → *Progression* 1, *Record* ●; tap Am, F, F, G
    (bottom row), ◀ Left once, tap G, ● Center. [`PROGRESSION 1 SAVED`]. Settings → Play →
    *Arrows* Sequencer; ▲ Up. [Am, F for two bars, G, looping; `P1 Am > F 1/4`; the
    current pad white, the next pulsing]; press root C [the same pads in C]; root A.
    MIDI monitor: *Chord vel* Fixed 100 [from the next chord, every chord at velocity 100];
    back to Normal [the tapped velocities again]; ▲ Up [stops]. *Arrows* back to Scale.
22b. Web editor → Chord sets: set 1, *Start from* Pop, Save to set 1, Use it on the TB.
    [bottom row left to right: C Dm Em F G Am B♭ A♭ Fm D7 E7 G7; `S1` on the play screen];
    root A [A Bm C♯m D E F♯m G F Dm B7 C♯7 E7]; Settings → Chord edits → *Add 7th* On
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
22j. Settings → Steps → *Lane* L1, *Kind* Drums, *Steps* ●. [`L1 DRUMS 16 1/16`]; tap the
    top row's 1st and 9th pads [amber]; tap ● Center. [a kick on beats 1 and 3, the playing step
    white, `<BACK  O STOP  >49-64`]; tap ● again [it stops]; hold ● [`REC: PLAY THE PADS`], tap ● [it
    ends]; tap both pads off.
22k. Settings → Layout → *Chords on* Right. [the right half lit as chords, the left as the
    scale; the right half's bottom-left pad plays C, the left half's bottom row C D E …];
    *Chords on* Left.
22l. Root C, M1. Settings → Effects → *Harmony* 3rd; play C, then D on the melody pads.
    [C+E, then D+F; `HARM 3RD` on the play screen]; *Harmony* Off. *Echo* 1/8, tap C. [C four times,
    each quieter, eighths apart]; *Echo* Off. Settings → Effects → *Mono* On; hold C, touch G, lift G.
    [G replaces C, then C comes back]; *Mono* Off. Settings → Arpeggiator → *Arp* Up, *Chance* 50 %,
    hold a chord. [about half the steps play, different each time]; *Chance* 100 %, *Arp* Off.
22m. *(1.7)* L1 Drums with the kick on steps 1 and 9 (22j), *Steps* ●: hold step 9, ▼ Down
    to `RATCHET`, ▶ Right twice, let go; *Transport* ●. [a roll of three on beat 3]; ◀ Left,
    *Direction* Reverse [backwards: the roll now comes before the plain kick]; *Euclid* 5 [`LANE FILLED`]; *Transport* ● again.
23. Send a C major chord from the DAW to the board. [matching pads light up]
23a. Settings → MIDI → *Scale lock* CH 16; from the DAW send C♯, then F♯ on channel 16.
    [C, then F, on the melody channel]; *Scale lock* Off.
24. Web editor: Connect, scale library, Find by notes C E♭ F♯, pick Blues, *Play it on the
    TB*. [`BLUES` on the board]. Custom scales: U5, *Start from* Blues, Save, *Use it on
    the TB*. [while editing, the pads showed the scale and the screen said `WEB EDITOR`]
    Progressions: P2, Key A Natural minor, type `Am D Dm Am C G E7 Am`, Save to P2.
    [D and E7 outlined]; on the board root A, M6, *Progression* 2, ▲ Up [the song plays].
25. Web editor: Download settings backup; Restore from file. [`Restored n of n records`]
26. Settings → About. [`2.0`; the editor's header says TBOSS 2.0.8, its build number]. Settings → Pads →
    *Pad test*: rest a finger lightly on a pad, then press it. [`PAD a/b/c`: that pad's resting
    level + 700 / + 300 / + 2500; each row ends with its pad's resting level, the bottom line
    `REST lo-hi`; its number rises; `*`, `PEAK` and `LAST VEL` appear once it plays]; *Trigger* +500
    [lighter touches play]; *Release* +400 [`OUT OF RANGE`: too close to Trigger]; *Trigger* back to +700.
26a. Hammer one melody pad as fast as you can, with the DAW recording. [every hit is a note, none
    stays on]; a light tap and a hard strike [the hard one much louder].
26b. *(1.7)* Settings → Layout → *Layout* Bass; tap the bottom-left pad and the one to its right in
    turn, fast. [a low C on every hit, each as loud as struck, none stays on]; Settings → Chords →
    *Spread of* Chord, a chord pad, **Spread I** [`SPREAD I CHORD`, the chord wider]; *Spread of* Root.
26c. *(1.7)* Settings → MIDI → *Melody prog* 5 [the synth on the melody channel changes sound];
    *Program in* CH 16, a DAW clip sending program 1 on channel 16 [`PRESET 1 LOADED`]; both back to Off.
26d. *(1.8)* Settings → Expression → *MPE* Melody, Settings → Expression → *Melody AT* Channel AT, an MPE sound on
    the computer ([MPE](#mpe-optional)); hold two melody pads and lean into one [only its note
    changes]; *Glide* Roll, hold the bottom-left melody pad and roll onto the pad to its right [the note
    slides up to it, no new note]; *Glide* Off, *MPE* Off.
27. TRS out to a synth (Type A adapter), play. [the synth plays]
28. Sustain pedal plugged in before power-up; Settings → About → *I/O test*, press it.
    [`PEDAL PA10 ... DOWN` while pressed]; then Settings → Pedal → *Mode* Sustain, play
    with it. [`SUS` on screen, notes sustain]; *Mode* Off.
29. Power off and on. [settings, presets, chord panel and U5 come back]
