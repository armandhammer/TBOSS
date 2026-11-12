# Roadmap

Ideas and agreed next steps, newest decisions first. Nothing here is built
unless marked. (Written so it can also go into the public TBOSS repository.)

## 2.6: planned next

- [ ] **Named functions for the MIDI map** (stage 2 of 2.5's controller work): about 70 functions that do the same
      thing from any screen ('Mute lane 3', 'Key D', 'Pattern next', 'Load preset 5', 'Step option: Gate'), set up in
      the web editor. 2.5's TB-button slots already reach every button, as it acts on the screen shown.
- [ ] **The step screen's loop rate with a pattern playing back to 2.4.6's** (0.47 passes a millisecond; 2.5 reads
      0.457): the two Mono lines and a few per-pad checks All Melody added.

## 2.5: steadier timing, All Melody, the step sequencer from the board and a controller (approved 2026-11-10; released 2026-11-11)

Released 2026-11-11 (build 2.5.0, board-tested over the alpha builds 2.4.1 to 2.4.13). It began as a review of the
whole firmware: its timing and reliability fixes came first, then All Melody and the step sequencer items first
proposed for 2.6.

- [x] **Steadier timing:** the MIDI clock out no longer loses time while notes play (48.0 clocks a second at 120 BPM;
      2.4 lost about 1 %); pads are read about every 15 ms instead of about every 40 ms; starting a session, or a
      write from the web editor, no longer holds everything else up; screens redraw in a third of the time. *(2.4.1-2.4.3)*
- [x] **Sturdier:** the touch buttons keep working however long the board stays on (they stopped after about 248
      days); a board that halts on a fault can still be reached by the debugger; music software that stops in the
      middle of a SysEx message no longer leaves the editor's port deaf; a computer that sleeps or is unplugged no
      longer leaves a DAW's Start or its held notes behind; a pasted lane keeps its loop window and follow after a
      restart; a save and a load asked for at the same moment both happen; the step screen's `PLAY 9/16` line no
      longer jumps sideways at step 10. *(2.4.4)*
- [x] **Storage:** a save after an edit no longer makes playing hiccup (one record written per pass); the flash cache
      set by the firmware itself; every save read back (`NOT SAVED: RESTART` if one didn't take); less stack at boot
      and a faster start with a full settings log; the start-up tidy waits 1.5 s for steady power before it erases.
      *(2.4.5-2.4.7)*
- [x] **Tests and fences** (nothing the player notices): the native suites built as the board builds them, a random
      run of the real main loop, random flash failures, the screen and lights' DMA buffers fenced off from the
      pattern memory, the stack's high-water mark for the inspect tool. *(2.4.8)*
- [x] **All Melody** ([design](all-melody.md)): Settings → Layout → *Pads* → All Melody, both halves one melody range,
      joined *Stacked*, in *Wide rows* or as a *Kalimba*; *Right octave*; *Split* keeps the left half on the chord
      half's channel and settings. *(2.4.9, 2.4.10)*
- [x] **Lane settings on the step screen** (a user's request: "U1 / U2 and U3 length / direction and swing"): U1
      held is the lane's *Length* (tap a step: the last one), U2 *Direction*, U3 or Shift + U1 *Swing*, Shift + U2
      *Rate*, changed with ◀ ▶; the step screen's line 4 no longer repeats PLAY / STOP. *(2.4.11, 2.4.12)*
- [x] **Pitch bend per step:** BEND, the eighth step option (Shift + M7), in eighths of a semitone of Settings →
      Steps → *Bend range* (2, 12, 24 or 48), on a lane with a MIDI channel of its own; sent just before the step's
      note and back to the middle at a rest, stop, mute and All notes off; in the web editor's Sequencer tab too.
      *(2.4.12)*
- [x] **The MIDI map grows** (set up in the web editor; no new menus on the board): 32 controls, each a CC, a note or
      pitch bend on its own channel; a control can be **one of the board's 43 buttons**, pressed and held as the
      real one, so every function of the top row can be played from a controller, or set **a step option** on the
      steps held (the pitch wheel bends them exactly). *(2.4.13)*
- [x] **External MIDI control tested on a board** (2.1's MIDI map: a setting on a knob, a button action, pickup).
- **Waiting:** a fix for hard strikes that now and then played almost silent. A trace of the last touches stays in the
  firmware to catch one; none has shown up in the 2.4.x rounds.
- **Going back** to a TBOSS before 2.5 after using a step's bend or 2.5's MIDI map additions loses those steps or the
  map (the older firmware can't read them).

Gates for every build: the clock test with Pattern 1 stays at 48.0 clocks a second and the loop rate does not fall
below 2.4.3's (play screen 0.55 passes a ms, Pattern 1 playing on the step screen 0.46), read with the inspect tool;
back up with the web editor before each install and restore after. 2.5 read 0.565 and 0.457 (accepted).

## 2.4: the sequencer grows up, and the lights' background (approved 2026-11-08; released 2026-11-09)

Released 2026-11-09 (build 2.4.0, board-tested over five alpha builds).

"The sequencer grows up" (all seven proposed items, approved 2026-11-08):
- [x] 8 lanes a pattern (4 before), the records backward-compatible: pattern 1's lanes 1-4 keep the slots 2.3 reads
      (kind 7, slots 0..15), lanes 5-8 in kind 20; 8 lanes' notes due on the same step all go out within a few ms
      (burst-safe); the web editor's Sequencer tab with 8 rows.
- [x] Lane mute and solo on the root buttons from the step screen (1-8 mute, Shift + 1-8 solo, 10 every lane again);
      Settings > Steps > *Solo*.
- [x] M1-M7 choose the step option on the step screen (velocity, gate, chance, ratchet, condition, nudge, tie: the
      2.2.1 user's request), no scrolling with up/down.
- [x] 8 patterns and a pattern chain (song mode): Settings > Patterns (*Pattern*, *Song*, *Copy to*, *Clear
      pattern*); a pattern chosen while playing takes over at the end of the playing one's time round; 5 held + root
      buttons choose a pattern (one) or a song (several, in turn); 11 / 12 the previous / next pattern; the editor
      shows and edits any pattern, plays it, copies it and edits the song.
- [x] Live transpose of lanes from the chord pads: a lane set to *Follow* moves with the chord pad struck last, by its
      degree in the scale (the nearer way); drum lanes stay. On the step screen Shift + a left-half pad (silent, nothing
      picked: board test S8; on a melody lane each row up an octave up); a moved lane's steps light as they sound.
- [x] A loop window per lane: hold the first and last step and press 12 (12 alone: the whole lane), or Settings >
      Steps > *Loop from* / *Loop to*; steps outside it light dimmer (off ones dark).
- [x] Fill: the Fill and Not fill conditions and the Fill center-button action came in 2.0.3; 2.4 adds Fill on the
      step screen's root button 9 (while held), so it is at hand while editing.

The lights (a user's request, decided 2026-11-08): the user asked for the notes outside the scale to show white on the
Chromatic and Fourths melody layouts (the only layouts with out-of-scale pads). Built as two general Settings > Lights
items instead, both board settings (like *Brightness*: a preset doesn't change them), both `AUTO` by default, which
looks exactly as before:
- [x] *Out of scale*: `AUTO` (as before), `WHITE`, `WARM` (warm white, chosen on the board from five), `COLOR` (the note's own color in every
      scheme), `DARK` (as an unlit pad), `OFF` (not lit at all: board test L1). How bright they show follows *Dim*.
- [x] *Dim*: how bright the background lights are, as a share of *Brightness*: `AUTO` (as before: out-of-scale notes
      10 %, silent chord pads 15 %, Modes' shared chords, the chords hints don't suggest and the progression
      guide's other notes 25 %), then 5, 10, 15, 20, 30, 40, 50 %. The panel's function buttons that are off follow
      it too, gentler (decided on the board 2026-11-09): AUTO 15 % as before, then 2 % a step, 15 % at Dim 15 % (11 … 23 %).
- [x] The pads' colors worked out in sixteenths of the LEDs' steps, so *Dim* works at every *Brightness*: the pad LEDs
      take 8 bits a color (only the button lights' driver has 12), so at *Brightness* 1 (4 of 255) a 10 % light used
      to round to dark; now a background light keeps at least the faintest step (only *Out of scale* `OFF` is dark).
- [x] *Brightness* 5 by default (4 before; decided on the board 2026-11-09). A board that saved its own keeps it.
- [x] Stored in a new small record kind (17): older firmware ignores it (going back to 2.3 forgets only these two), in
      the settings backup; the editor's Settings tab shows both (it reads the board's menu), with help; the manual.

From the board tests: ▲ on the step screen goes to the next lane; a held button outlasts missed readings of its
touch chip (Fill on root 9 ended at the first one); root 9 holds Fill at most about 5 seconds (the touch chip's
limit: a pedal or a controller button mapped to Fill for longer); the editor's Sequencer tab no longer shifts.

Next, after 2.4 (proposed with it): parameter locks and CC lanes (they need a storage design of their own), then
MIDI control Build B (mutes and patterns give it more to control).

## 2.3: Modes, modal interchange (a user's request; all 9 proposed items approved 2026-11-01; 2.1 and 2.2.1 in it)

The user's idea: the top row the selected key and mode, each other row a different mode of that key with its chords.
Released in 2.3 (2026-11-07, build 2.3.6, tested on a board with 2.1 and 2.2):
- [x] Settings > Chords > *Chord set* `MODES` (also on the chord-set button: Scale, Grid, Full, Modes, sets).
- [x] *Row 2*, *Row 3*, *Row 4*: any scale (default Minor, Mixolydian, Dorian), saved with settings and presets.
- [x] Every pad its own chord in root position (Voicing I-III and voice leading invert); pad edits and held shapes
      apply.
- [x] *Mel follows* `MAIN` / `LAST ROW`: the melody pads, scale lock and harmonizer in the last row's mode (the user's
      "chords, notes, or both"; per-row melody rows would have put each octave in a different scale).
- [x] Rows 2-4: the chords the key's scale also has dim, the borrowed ones lit.
- [x] The screen: the chord and the row's mode in three or four letters (`D# AEO`, `Edim MIX`, `G HMIN`).
- [x] Hold a pad of rows 2-4 + a mode button: that row's mode.
- [x] Editor (items, help, progressions named by row), manual section with a pad explorer and a Try it.
- [x] Firmer pad defaults (*Trigger* +1000, *Release* +600, *Full* +3200; were +700 / +300 / +2500), so a press near
      a pad's edge is less likely to play its neighbor too.

The ninth approved item, Modes in the microtonal tunings, went with the tunings in 2.3.3 (below).

## 2.1: external MIDI control (planned 2026-10-29; released in 2.3, 2026-11-07)

Any MIDI controller's knobs, faders and buttons drive TBOSS: knobs and faders set any Settings value,
buttons do any center-button action. The controller reaches the board through the computer (routed in
a DAW) or a USB MIDI host box; the board reads it on its existing USB MIDI input. Two builds, the
second only after the first has been tried on a board.
- [x] Build A, the core (released in 2.3, tested on a board): a *Control channel* (Settings → Control; Off by
      default) whose controllers only control the board; a map of 16 controls, each to a setting (with its own range,
      backwards if wanted) or a button action (press and release, "while held" as long as held); MIDI learn (move a
      control while a setting is open; Shift + right for one already mapped; *Map button* for actions); pickup, so a
      knob doesn't jump a value after a preset load; steady stepped settings; the value shown on the screen while it
      moves; the map saved, in backups and in the web editor's Buttons tab (with Learn).
- [ ] Build B, after Build A is checked: values and on/off states sent back to the controller's lights;
      a second layer with Shift; performance targets (chord pads, progressions, presets, root, scale,
      lane mutes, a velocity fader); controllers passed on to the synth (also out of the TRS jack); notes
      as triggers; endless encoders.

## Declined: microtonal tunings (a user's request; built in 2.2.0, removed in 2.3.3, decided 2026-11-01)

"Microtonal? 24-EDO (quarter-tones), 19-EDO, and 31-EDO." Built in 2.2.0 (19/24/31-EDO and Scala tunings, Tuning
out Bend / Map / Map + MTS, the editor's Scala import and export) and then dropped before any release: **out of scope
for the TheoryBoard.** The board is built around 12-tone harmony (its 12 root buttons, the mode buttons, chord
naming, theory hints, chord sets), so every tuning other than 12 needed its own scales, names, layouts and MIDI output
paths beside the 12-tone ones, a large amount of code and testing for a feature outside what the board is for. Not
planned. Record kinds 15/16 and scale ids 98..148 are retired (docs/tb-sysex.md).

## 2.2.1: steady timing and the step grid (a user's sequencer report, approved 2026-11-01; released in 2.3)
- [x] Steady timing: notes due together go out in one USB transfer (two or more lanes on the same 16ths were a few
      ms apart, the second the latest); a DAW's Start and clock read before the notes each pass.
- [x] *Step grid* (Settings → Steps): the step screen in 4 × 4 blocks of 16, steps 1–4 on the top row (default) or
      on the bottom row (MPC), or rows of 12 as before (triplets, 3/4).

Asked by the same user, not yet decided: the mode buttons choosing a step option (velocity, gate, chance …) and
**5** entering steps; a lane's length, channel and rate set on the step screen (hold *Length*, tap a step); a
controller's CC (and pitch wheel) setting the chosen option on held steps; a pitch bend per step (free pitch, as
on an analog step sequencer).
(2026-11-10: the mode buttons came in 2.4; the rest is approved for 2.5, at the top of this file.)

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

- [ ] More for the step sequencer, roughly in order (per-step nudge came in 2.0.3; 8 patterns and songs in 2.4):
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
