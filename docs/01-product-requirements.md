# 01 — Product Requirements

## 1. Problem statement

Move ships Drift and Wavetable, both clean, modern engines. There is no gritty
vintage-digital wavetable voice on the device. The Waldorf Protein is a much-liked
modern take on the Microwave 1, but its own panel is criticised for "Select-knob
heavy" editing and hidden parameters (noise in the filter section, pan in the
envelope section, ADSR stages reached through a menu).

Amino brings the Protein's *sound and architecture* to Move, while making
it *easier* to edit than the original by mapping it onto Move's 8-encoder page model.

## 2. Target users

| Persona | Needs |
|---------|-------|
| **Move player** | Load a preset, tweak 8 obvious knobs, play pads with aftertouch. Never reads a manual. |
| **Sound designer** | Reach every parameter from the hardware, see envelopes/filters drawn, save user presets. |
| **Live performer** | Predictable knob positions per page, scene morphing, layers for round-robin variation, no audio dropouts. |

## 3. Reference feature inventory (Waldorf Protein)

Used as the parity checklist. Sources: Waldorf product page, Synth Anatomy review
(Feb 2026), firmware 1.02 notes.

| Area | Protein | Amino target |
|------|---------|--------------|
| Polyphony | 8 voices, shared by up to 4 layers | 8 voices default, `voices` up to 16 when CPU allows (gated by M0 measurements) |
| Layers | 4; modes: All (stack), Round Robin, Random RR, MIDI channel split | Same + **Key split** (Move-specific) |
| Oscillators | 2 per layer, Microwave ASIC wavetables, 8-bit, stepped | Same behaviour, **original wavetables** (see 07) |
| Osc controls | Table, Wave, Semi, Level, Env amount, Velocity, Link | Same + Fine tune |
| Pitch | Scales & chords, glide | Same |
| Dirt | Static, Crackle, Geiger, Click, Burst, Ring mod | Same |
| Filter | CEM-style multimode LP24/LP12/HP24/HP12, Cutoff, Res, Env amt, Keytrack, Velocity | Same |
| Drive | Transistor / Tube / Diode, alternative to filter, modulatable | Same (pre/post-filter switch) |
| Envelopes | 3 × ADSR (Wave, Filter, Amp) | Same |
| LFOs | 2, Sine/Tri/Pulse/Saw/Ramp/Random/S&H, free/sync, per-voice/global phase | Same |
| Mod matrix | 8 slots: Source, Target, Amount; ~25 sources, ~23 targets | Same + optional **Via** scaler per slot |
| Pan | Manual + random/animated modes | Same |
| Flavour | Micro-variation of wave position & pitch per note | Same |
| Effects | 2 slots, 9 types (Chorus, Flanger, Phaser, Tremolo, Drive, Comp, EQ, Delay, Reverb); 1 slot per layer | Same model |
| Arpeggiator | Directions, 1–4 octaves, rate, swing, gate, 16 patterns | **Out of scope** — use Schwung's `arp` MIDI FX in the slot chain |
| Sequencer | Mono 32-step | **Out of scope** — Move's own sequencer covers this |
| Expression | Poly aftertouch, MPE | Poly AT from pads; MPE from external controllers |
| Presets | 150 factory, 360 slots, Load Layer | 324 original factory presets: Nostalgia ([09](09-factory-presets.md)), Modern ([10](10-factory-presets-modern.md)), 4 genre banks ([13](13-genre-banks.md)), Expansion ([14](14-expansion-bank.md)), Trance Gate Showcase ([16](16-trance-gate-presets.md)), Expansion II ([17](17-expansion-bank-2.md)) + Schwung "My Presets", Layer copy & Load Layer |

## 4. Functional requirements

### FR-1 Sound engine
- FR-1.1 8-voice polyphony across up to 4 layers; voices dynamically allocated.
- FR-1.2 Two wavetable oscillators per layer reproducing 8-bit, 128-sample,
  64-position Microwave-style tables with stepped (non-crossfaded) wave switching.
- FR-1.3 Selectable oscillator character: **Vintage** (aliased, stepped) and
  **Clean** (band-limited). Vintage is default.
- FR-1.4 Multimode resonant filter (LP24, LP12, HP24, HP12, BP12, Off) with drive.
- FR-1.5 Three ADSR envelopes, two LFOs, 8-slot mod matrix per layer.
- FR-1.6 Two global FX slots (9 types each, any combination); each layer routes to
  Dry / FX1 / FX2; slots run in Parallel or Serial (FX1 → FX2). See [11](11-effects.md).
- FR-1.7 Flavour: deterministic-per-note micro randomisation.

### FR-2 Control & usability
- FR-2.1 Every sound parameter is reachable from Move hardware (knobs + jog) with no
  external editor.
- FR-2.2 The first page shown after loading is a **Main** page of the 8 most
  musically useful controls for the focused layer.
- FR-2.3 No parameter is more than **2 jog steps** from the Main page via the
  section picker (Shift+click).
- FR-2.4 Envelopes, filter, LFO shape and current wave are drawn graphically.
- FR-2.5 With 1 active layer the UI hides all multi-layer complexity.
- FR-2.6 Layer copy/clear with Move's Copy/Delete buttons.
- FR-2.7 All labels readable in the ~5-character knob cell (see 04 short names).
- FR-2.8 On-device help (`help.json`).

### FR-3 Integration
- FR-3.1 Runs as a Schwung Signal Chain `sound_generator` (`plugin_api_v2`,
  multi-instance).
- FR-3.2 Full `state` round-trip: autosave, Schwung "My Presets", scenes, LFOs and
  automation lanes all work with no module-specific glue.
- FR-3.3 Tempo sync from the host transport (`get_beat_position`, BPM).
- FR-3.4 Optional: `split_voices` publishes the 4 layers as separate voices so
  each layer can go to its own Schwung bus/FX chain.
- FR-3.5 Remote UI `web_ui.html` (required in v0.3): mod matrix editor, wavetable
  view, layer overview, all pages mirrored. See 05 §9.

## 5. Non-functional requirements

| ID | Requirement |
|----|-------------|
| NFR-1 | Realtime safe: no allocation, file I/O, locks, logging on any plugin entry point. |
| NFR-2 | CPU: ≤ 20 % of one A72 core at 8 voices, all layers, both FX active (≈ 580 µs per 128-frame block). Stretch goal ≤ 12 %. |
| NFR-3 | No audible zipper noise on any knob turn (per-sample or sub-block smoothing). |
| NFR-4 | Module load (`create_instance`) < 50 ms; no file I/O required for factory content. |
| NFR-5 | Memory < 8 MB per instance. |
| NFR-6 | Deterministic: same state + same MIDI = same audio (except Random sources, which are seeded). |
| NFR-7 | No Waldorf code, ROM data or trademarks shipped. |

## 6. Release scope

| Release | Contents |
|---------|----------|
| **v0.1 "Single cell"** | 1 layer, 2 osc, filter, 3 env, 2 LFO, 8 mod slots, dirt, flavour, pan, 16 wavetables, factory presets ×20, full page UI, state. |
| **v0.2 "Organism"** | 4 layers, layer modes (stack/RR/random RR/key split/channel), layer copy, Load Layer, FX1/FX2 with 9 types + serial chain, 32+ wavetables, Nostalgia bank (64 presets). |
| **v0.3 "Expression"** | Wave canvas page, scales & chords, MPE, `split_voices`, **Remote UI (required)**, Clean osc mode, 16-voice option if CPU allows, **Modern bank (64 presets)**. |
| **v0.4** (optional) | User wavetable import. |

## 7. Out of scope
- Step sequencer (Move provides one).
- Arpeggiator (Schwung's `arp` MIDI FX covers it; presets tagged "ARP-friendly" in
  09 are voiced for it).
- Bit-exact emulation of the Protein or Microwave (we target character, not clone).
- SysEx compatibility with Protein patches.
