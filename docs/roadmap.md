# Roadmap

Ideas and agreed next steps, newest decisions first. Nothing here is built
unless marked. (Written so it can also go into the public TBOSS repository.)

## 2.0: Turing, chains, presets' own chords, U3 Shift, Sleep (approved 2026-10-28; 2.0 released 2026-10-30)

Built 2026-10-28 to 2026-10-30, checked on a board (2.0.8) and released as 2.0 2026-10-30, with 1.8 and 1.9:
- [x] A Turing Machine style arpeggiator pattern: a random loop of the held notes that changes a little each
      time round, with *Lock* (how much stays) and *Loop* (2-16 steps).
- [x] Copy, paste and shift in the web editor's Sequencer tab: a lane onto another, its steps one earlier or
      later, a range of steps; on the board, Shift + the step screen's buttons do the same for a lane.
- [x] Chained progressions (P1 > P1 > P2, up to 8), set in the editor, Shift + root button 9 or
      Settings → Progressions → *Chain* plays them; each chord's chance (rest, or the borrowed chord).
- [x] Chord sets stored per preset (*Keep chords*), in the editor's Presets tab too.
- [x] U3 as Shift (tap: the next press, two taps: latched) and Shift + the buttons under the screen on every
      screen, the bottom line saying what; the I/O test shows the buttons held.
- [x] Shut down (Settings → About: dark until ● Center) and *Sleep after* 5-60 minutes without a touch (dark,
      not low power).
- [x] The Settings menu rebalanced (approved 2026-10-28): 17 pages, each one kind of thing (new Expression, Effects,
      Chord edits; every MIDI channel on MIDI; Sequence split into Progressions and Steps).
- [x] Spread of Bass (a user's request, approved 2026-10-29): the chord as voiced, its root added under it, so the
      lowest note is always the root (Settings → Chords → *Spread of* → Bass).
- [x] Five sequencer ideas from the Deluge, OXI One and Hapax (approved 2026-10-29, built as 2.0.3): Fill and Not fill
      steps (a center button or the pedal), a step's nudge and each lane's swing, each lane's transpose, Mutate (and
      Randomize in the editor), and Stutter buttons (1/4, 1/8, 1/16) for the lanes and the progression.

## 1.9: Customize, the board set up from the web editor (approved 2026-10-27)

Built in stages, each tested on a board (1.9.2 to 1.9.4). Released inside 2.0.
- [x] Every setting in the web editor: a Settings tab built from the board's own menu (a setting added
      later shows there by itself), each change applied at once without pausing the board.
- [x] Tabs in the web editor: Settings, Lights, Feel, Scales, Chords, Progressions, Sequencer, Buttons, Presets, Backup.
- [x] Lights: four more color schemes (Piano, Triad, Function, Octaves), Color by note name or scale
      degree, every scheme's colors chosen from 16 in the editor's Lights tab with a picture of the board,
      and a preset's own look (Keep look).
- [x] *Full* up to +4000; *Glide* Roll by default.
- [x] Feel: velocity and pressure curves drawn in the editor (Chord vel / Melody vel → Custom), a
      calibration from soft, medium and hard presses, and a map of every pad's resting level.
- [x] Content editors: the step pattern as a grid (step options, direction, Euclid, play and stop), every
      chord pad's edits at once with its chord's name, preset names (shown by the board's Load preset) and a
      preset manager (load, copy, swap, clear, compare setting by setting, a preset's own colors and curves,
      Keep feel).

## 1.8: MPE and Glide (approved 2026-10-26)

Built 2026-10-26 as 1.8.0 alpha; 1.8.5 passed its board tests (2026-10-27). Released inside 2.0.
- [x] MPE (Settings → MIDI → *MPE*: Melody, Chords or Both): each pad on a channel of its own (the
      melody half the Lower Zone, channels 2–8 with 1 as master; the chord half the Upper Zone, 9–15 with
      16), each pad's pressure on its channel only, the MPE configuration message at the start, 3 s
      later and at a DAW's Start.
- [x] Each pad's pressure as channel pressure, with a controller (CC74 "timbre") or a bend of up to 2
      semitones, from the half's pressure setting.
- [x] Glide (Settings → MIDI → *Glide*): rolling from a held melody pad onto the pad beside it slides
      the note there, and the note carries on when the first pad lifts. Three ways (1.8.5): Roll (by which
      pad is pressed harder), Pad (by the new pad's pressure) and Time (portamento over a *Glide time*).
- [x] Vel full (Settings → Pads) and the Firmer and Firmest velocity curves: velocity can reach 127 at a
      harder press than *Full*, so medium presses keep their range.
- [x] Play and stop on the step screen (asked by a user, 2026-10-27): tap ● plays/stops, hold ●
      for step record; Settings → Sequence opens on *Play*; the manual says how to play the steps.

## 1.7: step sequencer, round 2 (approved 2026-10-25; 1.7 released 2026-10-26)

The step fields below are already in 1.5's pattern record, so none of this changes the format.
Built 2026-10-25 and 2026-10-26, checked on a board and released as 1.7 2026-10-26:
- [x] Direction per lane: forward, reverse, ping-pong, random (random repeats each time it plays).
- [x] Probability and ratchets per step.
- [x] Trig conditions per step (1:2 … 4:4, first, not first: play on the A-th of every B loops).
- [x] Euclid fill and rotate per lane.
- [x] Gate, ties and velocity editing on the step screen (hold steps, ▲/▼ choose, ◀/▶ change).
- [x] Held chord shapes (Add 7th etc. on a center button) change a running arpeggio at once
      (1.0: they apply to the next chord pad touched; found in board testing, 2026-10-21).
- [x] Pedal *Mode* defaults to Sustain (the pedal input was confirmed with 1.5.0).
- [x] Settings → Pads → *Rescan pads*, for a board that was moved and a pad plays by itself.
- [x] `EXT` on the play screen whenever a MIDI clock comes in.
- [x] Pads counted from each pad's own resting level, a neighbor's bleed kept silent, and *Unstick*
      for pads that settle higher after a press (added 2026-10-26 from a warped board's readings).

Three more of the original's 2020 promises (approved 2026-10-26, checked on a board, in 1.7):
- [x] Bass layout (the promised *bass mode*): each scale note on two side-by-side pads, each hit strikes
      the note again, for fast two-finger bass lines (Settings → Layout → *Layout* → Bass).
- [x] Program Change: each half's program sent with its preset (*Chord prog*, *Melody prog*), and
      programs 1–8 from a DAW load presets 1–8 (*Program in*). NRPN: not planned (a channel per pad: MPE, 1.8).
- [x] Spread of Chord: the Spread buttons open the whole chord, not only drop its root
      (Settings → Chords → *Spread of*).

## 1.6: four effects from the Hapax / Square Pyramid list (approved 2026-10-24; 1.6.0 beta released 2026-10-25)

A user asked for the MIDI effects of the Square Pyramid and Hapax sequencers (18 of them). Many were
already in TBOSS (arpeggiator, Euclidean rhythms, swing, scale quantizing and scale lock, random
strum feel, velocity curves, channel routing); per-step chance and timing quantize are planned for
the step sequencer. To keep the menus short, four were chosen, each one to three settings on an
existing page. Built 2026-10-24, checked on a board and released as 1.6.0 beta 2026-10-25:
- [x] Harmonizer (Settings → Play → *Harmony*): a diatonic 3rd, 4th, 5th, 6th, octave, 3rd+5th, 3rd
      or 6th below on every melody pad, in the current scale.
- [x] Echo (Settings → Play → *Echo*, *Echoes*): a MIDI delay on the clock, 1/16 to 1/2, 1–8 echoes,
      each quieter.
- [x] Mono (Settings → Layout → *Mono*): the melody half one note at a time, the newest wins, legato
      overlap, the note still held comes back.
- [x] Chance (Settings → Arpeggiator → *Chance*): arpeggiator steps and note repeats at 100–10 %.

Not planned: envelope, LFO and slew (they may come as CC lanes in the step sequencer), Arpoly (the
step sequencer covers it), a MIDI-in note filter.

## 1.5: a step sequencer, foundation first (approved 2026-10-23; 1.5.0 beta released 2026-10-24)

A multi-lane step sequencer (ideas from OXI One and Deluge), built so later features are
small additions, not rewrites:
- A pure core (`lanes.c`: data model, record format, which step plays where) tested natively;
  each lane's step comes from the clock position, as progressions' do, so lanes of different
  lengths and rates are polymeter for free and a DAW's Start, Continue and Song Position
  land correctly; randomness from a hash of seed, lane, loop and step, so playback repeats.
- Every step field has 0 = default and exists from the start (gate, probability, ratchet,
  condition, nudge, tie), so later features need no format change.
- A chord lane's step stores a pad, its edits and scale as progressions do (the root buttons
  transpose a pattern); melody lanes scale steps, drum lanes notes. Lanes play through the
  engine's own sources (strum, channels, safe Note Offs come with it).
- A new record kind for patterns, so backups and *Save settings* include them; 8 patterns of
  4 lanes x 64 steps fit the settings sector.

- [x] 1.5.0 (beta, released 2026-10-24 with 1.4 after board tests): one pattern of 4 lanes (kind, channel, length
      1–64, any 1.4 rate including tuplets, mute); steps on/off with a value and velocity; plays
      with the board's transport or a DAW next to the progression; a step screen (the right half
      shows 12 steps a row in reading order, the left half what to play; ▲▼ choose the lane) and
      step recording. The pattern is in backups; every later field is already in its record.
- [x] Chords on the right (user request, built 2026-10-23 into 1.5.0): Settings → Layout →
      *Chords on* Right swaps the halves (chords right, melody left) for left-handed playing.
      Two halves of the same kind (chords–chords, melody–melody) was considered and not planned.
- [x] Rainbow colors (user request, built 2026-10-23 into 1.5.0): Settings → Lights → *Colors*
      → Rainbow lights both halves in seven colors whatever plays (red, orange, yellow, green,
      dark blue, light blue, magenta, then again; touched pads white; seven instead of twelve
      from 2026-10-24, as the requester asked); the colors by note are now called Default.
- [x] Faster pads (user report, built 2026-10-23 into 1.5.0): the engine takes each scan
      position's pads as they are read and the ADC converts faster (a frame about every 4 ms, was
      15): notes about 6 ms after the touch on average (was about 16), fast repeats no longer
      merge. One USB transfer per chord: only if this is not enough.

The step options, direction and Euclid fill came in 1.7; the step sequencer's further plans are
under Later.

## Later

- [ ] More for the step sequencer, roughly in order (per-step nudge came in 2.0.3): 8 patterns chained into a song;
      live recording with quantize (the phrase recorder); parameter lanes with their own lengths; CC
      lanes. (The lane editor in the web editor came in 1.9, copy, paste and shift in 2.0.)
- [ ] A second arpeggiator (deferred from 0.6.0: it needs a second source pool and its own menu page).
- [ ] Own USB product ID 1209:B333 from pid.codes (requested 2026-10-16, pidcodes PR #1319),
      replacing the test ID `cafe:4069`.

## 1.4.0 beta (built 2026-10-23, released inside 1.5.0 beta 2026-10-24): rhythm, pivot chords, chord capture

From "copy useful things from Scaler 3" and "a polyrhythmic sequencer" (approved 2026-10-23):
- [x] Euclidean rhythms (every k of n up to 16 steps, with rotation) and tuplet rates (5 and 7
      per beat, 1/4T, 1/32T, dotted) for the arpeggiator; note repeat with triplets, 5 per
      beat, its own rhythm, By row (each row its own rate: 2:3 and 3:4 with two fingers),
      and strikes on the clock's grid so they stay with each other and the DAW.
- [x] Pivot chords: a target key lights the chords both keys share (ii and IV of the target
      pulsing), the screen names the way in (`ii IN G: D7>G`), the target's root button
      arrives; a *Pivot* center button.
- [x] Chord capture (like Scaler's Detect): chords played on the pads or over MIDI in become a
      chord set in the key they fit best, and the board moves to it.

## 1.3.0 beta (released 2026-10-22): two new chord layouts

- [x] Full chord layout, the original TheoryBoard's default (requested in
      [discussion #8](https://github.com/armandhammer/TBOSS/discussions/8)), built 2026-10-22: the scale's 7 chords in order, each with a run of variations that stay in the
      scale (C major: C, Cadd2, C6, C6(add9), Cmaj7, Cmaj7(9), Csus2, Csus4), poured column by
      column from the top across the 48 chord pads (runs in C major: 8, 8, 5, 9, 8, 7, 3). The
      chords and voicings are the original firmware's own lists, read from the board's memory
      chip (with the developer's permission); the original's typing slips are corrected; scales
      it didn't have play a parent mode's list or a layout built from common chord types.
      Settings → Chords → Chord set → Full.
- [x] Chord grid (built 2026-10-22): the scale's chords with a chord type per row (bottom up: triad, seventh, sus, ninth), columns the scale steps as
      usual, so sevenths, sus chords and ninths are always in the same place without setting
      up a center button. Settings → Chords → Chord set → Grid.

## 1.0.0 beta (built as 0.12.0, released 2026-10-21): chord sets, hints, scale lock, center buttons (approved 2026-10-19)

From a user's request (custom chord sets) and the ideas below (customizable center buttons
and Shift, theory hints, MIDI in scale lock).

- [x] Chord sets: 8 sets of 12 chords (up to 6 notes each, any root, a column may be empty)
      typed in the web editor; Settings → Chords → Chord set; the root buttons transpose a
      set, the rows play its inversions, the panel, voice leading and fold apply; a
      progression keeps the set it was recorded with.
- [x] Theory hints: after a chord, the usual next chords pulse and glow (by scale step in
      seven-note scales, by root movement otherwise).
- [x] Scale lock: notes arriving on one MIDI channel play on the melody channel, moved to the
      nearest note of the key and scale.
- [x] Center buttons: each of the 14 does any of 39 actions (its printed function by
      default; toggles (sustain too), chord shapes while held, tap tempo, presets, scales,
      play/record, next chord set, all notes off); a Shift button (held or latched) with
      Shift + root = progression, Shift + mode = preset, Shift + chord pad = pick for edits.
      Settings → Buttons and the web editor.
- [x] Checked on a board.

## 0.11.0: chord progressions (approved 2026-10-19)

From user requests: songs whose chords are not all in one scale, velocity set per half,
and the sustain pedal on one half only.

- [x] Progressions: record by tapping chord pads (1 beat to 2 bars per tap, the same chord
      again lengthens it, ◀ takes back), 8 slots; play with ▲ (Arrows: Sequencer), the
      pedal, Transport or a DAW's Start, on the MIDI clock.
- [x] A step keeps its pad, edits and velocity; chords recorded in another mode keep that
      scale (borrowed chords). Root buttons transpose a playing progression; mode buttons
      re-color it.
- [x] Everything that shapes a held chord applies (panel, strum, voice leading, fold,
      arpeggiator). Guide lights: the chord now, the next one, its notes on the right half.
- [x] Web editor: type a progression (`Am D Dm Am C G E7 Am`) in a key; the editor places
      each chord on the pads, borrowing from the closest scale where needed.
- [x] Chord vel and Melody vel (velocity curve per half); pedal Target (both, chords or
      melody).
- [x] Validation before release: a DAW's Continue and Song Position Pointer land on the right
      chord, the editor's pauses keep the place, recording nothing keeps the slot, the play
      screen's line fits; restores into older TBOSS leave out what they don't have.
- [x] Checked on a board and released (2026-10-19).
- [x] Decided: strum *Repeat* and *Cutoff* stay off for progression chords; a
      progression plays its recorded velocities unless *Chord vel* is Fixed 100 (then 100);
      a typed or Fixed 100 plays 100 after a restart (was 101).

Later sequencer ideas (from the Deluge and OXI One comparison) wait for approval: a note
phrase recorder, chained progressions, per-step probability.

## 0.10.0: pad sensitivity (approved 2026-10-18)

From Pad test readings on a stock board (rest ~290–470, lightest touch ~600, neighbors
under ~500, firm press ~3500) and the same board with a sensitivity modification.

- [x] Settings → Pads: Trigger (500–3000, default 1200), Release (300–2500, default 600),
      Full (1500–4000, default 3500) in steps of 100; Pad test moves to this page.
- [x] Release kept at least 130 above the resting level measured when playing starts, so
      boards whose pads rest high still release.
- [x] Released as the default (2026-10-18).

- [x] 1.5.0: per-pad calibration: each pad its own resting level, measured when playing
      starts and followed while it rests (uneven or flexing boards: pads stuck on and
      skipped notes).

Later, if needed: ignoring a pad that only reacts to a much harder-pressed neighbor (for
modified boards).

## 0.9.1: pad test and reliability fixes (2026-10-18)

From a code review with long randomized stress tests of the engine, layouts, controls and
settings (now part of the test suites).

- [x] Settings → About → Pad test: raw pad readings, the peak of a touch, the velocity a
      note got, and how much a press spreads to neighboring pads.
- [x] Voice leading never stops the board: a pad with no inversion in range plays its
      row's chord.
- [x] Notes held when a session is lost (computer unplugged or asleep) end in the next
      session, so a synth on the TRS jack doesn't drone.
- [x] Pressure, controllers and pitch bend always return to rest (after a slow release,
      and when the web editor interrupts playing).
- [x] A running transport pauses and continues (Stop, Continue) instead of restarting
      after an interruption.
- [x] Buttons keep working after 49.7 days of continuous power.
- [x] Next: an adjustable pad sensitivity: 0.10.0.

## 0.9.0: voice leading, note repeat, fold (approved 2026-10-17)

Defaults stay those of a regular TheoryBoard: all three are off until turned on.

- [x] Auto voice leading (Settings → Chords → Voice lead): each chord takes the inversion
      that moves least from the chord before, within two inversions of its pad.
- [x] Note repeat for the right half (Settings → Play → Note repeat), MPC-style, velocity
      from pressure.
- [x] Fold (Settings → Chords → Fold): every chord inside one octave, G♯ to G, as the
      original is described; the campaign's "turn off octave folding" is TBOSS's default.
- [x] Manual: every feature with a walk-through, why to use it and when not to.
- [x] Checked on a board and released as the default (2026-10-17).

Bass mode (each note on two pads for fast alternating repeats): built in 1.7 as the Bass layout.

## 0.8.0: the original's promised Live Note Update (approved 2026-10-17)

The 2020 campaign's Live Note Update was a pad area: while a chord is held on the left, the
right half offers that chord's notes (press of the time: 28 scale pads, 20 chord pads). TBOSS
had used the name for re-striking held pads (now *Restrike*).

- [x] Live notes layout: columns 1–7 the scale, columns 8–12 the live chord's notes, rows
      an octave apart; the chord stays after release.
- [x] Live notes on their own MIDI channel (Settings → Layout → Live ch).
- [x] Live notes follow chords arriving over MIDI in (Settings → Layout → Live in).
- [x] Silent chord pads that only set the live chord (Settings → Chords → Sound).
- [x] Checked on a board (2026-10-17; following MIDI in and the own channel not yet tried).

Chords on both sides: not for now.

## 0.7.0: Stage 4, the scale library and strum feel (approved 2026-10-17)

- [x] Scale families on M1–M7: the modes of major, melodic minor, harmonic minor and
      harmonic major (Settings → Play → Family).
- [x] A built-in library: 86 scales in all (61 new), one list for firmware and editor
      (scripts/gen_scales.py).
- [x] Up to 128 favorite scales on the board; ▲/▼ step through them.
- [x] Editor: browse by family or bright to dark, find scales by notes (clicked or played),
      scales one note away, favorites.
- [x] Strum feel: random timing and velocity (user request).
- [x] Capo (user request): the guitar voicing became six real strings in standard tuning,
      with a capo and positions up the neck.
- [x] Checked on a board and released as the default (2026-10-17).

Stage 5 (not approved yet): sequencers/phrase recorder, sets and user colors.

## 0.6.0: Stage 3, the original TheoryBoard's features (approved 2026-10-17)

- [x] Center buttons do what the printed panel says: voicing I/II/III/guitar, spread
      I/II/III, octave V with tap and hold-to-stack; per-pad edits moved to Settings → Chords.
- [x] Drum mode: the right half as scale / bass / General MIDI drum grids, own channels.
- [x] 13 more built-in scales; user slots raised to 12.
- [x] More layouts: chromatic and fourths.
- [x] Strum: repeat while held, cutoff, tempo-based strum times, strum on/off per pad.
- [x] Arpeggiator: 8-step rhythm, swing, sync start, a Chords pattern (arp-to-strum), latch.
- A second arpeggiator was deferred (it needs a second source pool and its own menu page); it is
  under Later.
- [x] Expression: pitch bend from pressure, Light/Firm velocity curves, live update of
      held pads.
- [x] Pedal: sostenuto/soft/portamento, play/stop, a note on the drum channel.
- [x] MIDI clock out at the board's tempo; transport Start/Stop.
- [x] Violet and pink pad colors moved further apart (A, A#, B).
- [x] Checked on a board: voicing and spread right; the octave V was out of order (fixed in 0.6.1).

## 0.6.1: panel fixes from the board check (2026-10-17)

- [x] Octave V buttons in their real order (−3 … +3).
- [x] 0.6.2: octave buttons toggle (tap on / tap off, lit octaves stack), as the original
      firmware does; quick taps had stacked by accident with hold-to-stack.
- [x] Panel lights: full while a choice is on, dim otherwise (30 %, 15 % from 0.6.2) (channels from the original
      firmware's light tables).
- [x] ▲/▼ on the play screen: Settings → Play → *Arrows* (Scale by default, or melody
      octave, preset, tempo); melody octave also as Settings → Play → *Melody oct*.
- [x] 0.6.2 checked on a board (panel, its lights, arrows) and released as the default (2026-10-17).
- [x] 0.6.3 (navigation buttons at 70 %) checked on a board and released as the default (2026-10-17).

## 0.5.x: lights, a robust start, aftertouch per half (2026-10-17)

- [x] A pad that reads pressed when the board starts no longer silences the whole board
      (tester report: no pad reaction, no MIDI); it stays silent until released once.
- [x] Rainbow pad colors by note (default), touched pads white with pressure; the 0.4
      look stays as Settings → Lights → Colors → Root+5th.
- [x] Web editor preview: the whole board shows the scale; the display names the slot and notes.
- [x] I/O test page shows pads that read as held.
- [x] Aftertouch per half (Chord AT, Melody AT), and poly aftertouch from chord pads on each
      of their notes that no other held pad shares.
- [x] Editor keys in the board's colors.
- [x] 0.5.1 checked on a board and released as the default (2026-10-17).

## 0.4.0: the jacks (2026-10-16)

- [x] TRS MIDI out (Type A): everything sent over USB also goes to the jack; plays the jack
      alone when no computer is attached. Settings → MIDI → TRS out.
- [x] Sustain pedal: CC64 per side, or the board's hold; auto/normal/inverted pedal types.
- [x] I/O test page (Settings → About) showing the pedal input live and sending a test note.
- [x] 0.4.0 installed and checked on a board without the jacks (USB playing, menu, settings,
      editor, no-computer start); released as the default 2026-10-16.
- [x] TRS MIDI out (PB6 USART1 TX, Type A) confirmed on a board: messages read correctly
      by a MIDI interface (2026-10-17).
- [x] Sustain pedal input on a board: the pin is PA10, the jack's tip (I/O test, 2026-10-24; PD13
      was wrong); sustain and note modes confirmed with 1.5.0.

## Alpha launch: 0.3.0 (agreed 2026-10-16)

Launch with a basic feature set; users test and report what breaks.

- [x] Browser installer through the board's own update mode (WebHID); verified on hardware.
- [x] Settings, presets and user scales carried across an install (saved over MIDI first,
      restored after the restart).
- [x] Hosted installer and editor on GitHub Pages, with the newest release's firmware.
- [x] Firmware version on the start-up screen, in Settings → About, reported to the editor
      and the installer, and in the USB device release number.
- [x] Settings menu as a tree driven by the five buttons below the display: left back,
      up/down move, center enter, right deeper; only left leaves (hold left + center: home).
- [x] Manual: browser install first, real button names, known issues; SWD guide kept as advanced.
- [x] Alpha disclaimer; Discussions for feedback.
- [x] Installed on hardware with the browser installer, settings kept (2026-10-16); released as v0.3.0.
- [x] Going back with the manufacturer's update utility after TBOSS: confirmed (the utility
      hangs at "finalizing" although the board has finished; close it and replug).
- Own USB product ID: requested from pid.codes 2026-10-16 (pidcodes PR #1319); under Later.

Not in TBOSS yet (candidates for the next stages): sequencer/phrase recorder (chord
progressions came in 0.11.0),
a second arpeggiator, an arpeggiator on/off button, per-pad sensitivity, customizable
center buttons (below). Drum mode and 12 user slots came in 0.6.0.

## TBOSS public release (agreed 2026-10-16, after Stage 2 passes on hardware)

Public GitHub repository **TBOSS** under `armandhammer`, GPL-3.0-or-later.
Latest known-working firmware, web editor, tools and documentation; tagged releases.

- [x] Source comments neutral; headers GPL-3.0-or-later; standalone build
      `scripts/tboss_build.py` reproduces the Stage 2 image byte for byte.
- [x] Public flash tool (backup / install / restore over SWD) with simulated-OpenOCD tests.
- [x] Documentation: user guide (every feature, step by step), hardware reference
      (MCU, flash map, loader, pads, touch controllers, LEDs, OLED, NOR, USB, SWD),
      flashing, building, firmware architecture, SysEx protocol.
- [x] Issue templates (bug report, feature request, hardware finding), CONTRIBUTING.
- Own USB product ID instead of the test ID `cafe:4069`: under Later.

## Install from Chrome, no debugger (done in 0.3.0)

The board's resident loader has a built-in USB update mode (hold the chord half's top-left
and bottom-left pads while plugging in). The TBOSS installer page talks to it over WebHID
and installs to flash sectors 6-11 only; the protocol is in
[installer-protocol.md](installer-protocol.md). Verified on hardware. The SWD tools stay as
the expert and recovery path (they are also the only way to back up a board's whole
internal flash).

## Customizable center buttons and a Shift layer (idea, 2026-10-16; built in 1.0.0)

Let users choose, in the web editor, what each of the 14 center buttons does, with
the current functions as the default. One button (or a user-slot button such as U3)
can be made **Shift**: held, it gives every other button and pad a second meaning;
double-tapped, it could latch.

Possible actions to choose from:
- Chord shaping: lower root/3rd/5th; add 6th/7th/9th/11th/13th; sus2/sus4; force
  major/minor/dim/7th while held (quality override); cycle inversion; drop-2 voicing;
  smooth voice leading on/off; add the root an octave down (bass note).
- Performance: hold; arpeggiator on/off; tap tempo; strum on/off and direction;
  sustain pedal (CC64) while held; mod wheel or pitch bend while held; all notes off
  (panic); mute the chord or melody half; octave and semitone transpose.
- Navigation: next/previous preset; save to the current preset; next/previous scale;
  open a menu item directly.
- Looping (needs a recorder): record/play/overdub/clear a phrase.
- MIDI: send a chosen CC or program change; switch the melody half's channel.

Shift ideas: Shift + root button = transpose without changing the layout name;
Shift + mode button = save the current setup to that preset slot; Shift + chord pad =
edit that pad directly instead of "the last chord played"; Shift + melody pad =
choose the arpeggiator's root note or rate.

Button lights could show state (hold, arpeggiator, Shift, a latched function). That
needs the backlight channel of each button, which the LED test can measure.

Stored as a new settings record, so presets and the settings backup carry it.

## Other ideas

- Use the external memory (after it is backed up): many more presets and scales.
- Dual boot (low priority): firmware A starts normally, firmware B while two right-half
  pads are held at power-up. Easy with two TBOSS builds; with the original firmware it
  needs a check that the original never writes sectors 10-11 and that the loader does not
  verify its checksum at boot (the original must stay at 0x08040000).
