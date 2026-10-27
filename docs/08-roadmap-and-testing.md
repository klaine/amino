# 08 — Implementation Roadmap & Test Plan

## 1. Proposed repository layout

```
amino/
  README.md
  LICENSE                   # MIT
  docs/                     # these design docs
  src/
    module.json
    help.json
    web_ui.html             # Remote UI (v0.3)
    assets/                 # Remote UI js/css
    ui/                     # canvas + card scripts (no main ui.js needed: host grid)
      wave.js
      cards.js
      wavetables.mjs        # generated
    dsp/
      amino.c               # plugin_api_v2 glue: create/destroy/midi/params/render
      engine.c/.h           # instance, layer manager, voice allocator
      voice.c/.h            # per-voice render
      osc_wt.c/.h           # wavetable oscillator (vintage + clean)
      decimator.c/.h        # half-band polyphase
      filter.c/.h           # TPT multimode
      env.c/.h  lfo.c/.h  modmatrix.c/.h  dirt.c/.h  drive.c/.h
      fx/ chorus.c flanger.c phaser.c tremolo.c drive.c comp.c eq.c delay.c reverb.c
      params.c/.h           # param_desc table, set/get, state (de)serialise
      contract.c            # generated chain_params + ui_hierarchy strings
      wavetables.c          # generated
      factory_presets.c     # generated
  presets/factory/*.json
  tools/
    wavegen/                # see 07
    contract/gen_contract.mjs  # builds chain_params/ui_hierarchy from params.yaml
    presets/build_presets.py
    params.yaml             # single source of truth for all parameters
  tests/
    host/                   # native (macOS/Linux) unit + golden tests
    device/                 # on-device scripts
  scripts/build.sh          # Docker cross-compile (aarch64), package tarball
  .github/workflows/release.yml
  release.json
```

**Single source of truth:** `tools/params.yaml` describes every parameter (key,
names, short name, type, range, default, unit, viz, page, flags). Generators emit:
`params.c` descriptor table, `contract.c` (`chain_params`, `ui_hierarchy` JSON),
`help.json` page leaves, and the parameter table in doc 04. This prevents the C
engine, UI contract and docs drifting apart.

## 2. Milestones

### M0 — Spike & measurements (de-risk)
- Build the Schwung "hello synth" template on device; confirm toolchain, `dsp.so`
  path, slot loading.
- Measure available core-3 headroom with a busy Move set.
- Prototype osc at 1×/2×/4× + decimator; measure µs per voice.
- Answer VERIFY items in 02 §7 (re-plan on `child_count`, shared
  `child_index_param`, `visible_if` equals on index vs name, poly AT delivery).
- **Exit:** CPU numbers recorded; go/no-go on 8 voices @ 4×.

### M1 — Single-layer engine (→ v0.1)
1. `params.yaml` + generators + `param_desc` table.
2. Wavegen with 16 tables.
3. Voice: osc ×2, dirt, drive, filter, 3 env, amp/pan.
4. LFO ×2, mod matrix, expression routes, flavour.
5. Voice allocator (poly/mono/legato, glide, stealing).
6. `state` (de)serialise, factory presets ×20.
7. `ui_hierarchy` for Main, Osc 1/2, Wave Env, Mixer, Filter, Amp, Play, Voice,
   LFO 1/2, Mod 1-8, Tools.
8. `help.json`.
- **Exit:** passes test suites §3.1–3.4; usability U1–U3, U5–U7.

### M2 — Layers & FX (→ v0.2)
1. Layer manager (Stack, RR, Random RR, Split, Channel), `edit_layer` focus.
2. Child-level hierarchy, Copy/Delete gestures, Load Layer.
3. FX engine: 9 types, 2 slots, routing/sends, serial chain, stable FX 1/FX 2 pages.
4. Layers / Layer Mix / Layer Route / Key Split pages.
5. 64 factory presets, 32+ wavetables.
- **Exit:** U4 passes; CPU ≤ budget with 4 layers + 2 FX.

### M3 — Expression & visuals (→ v0.3)
- Wave canvas page, mod card, `live` wave position.
- Scales & chords, MPE, Clean osc mode.
- `split_voices` + `move_plugin_render_split`.
- **Remote UI** (required): all views in 05 §9, `rui_poll` live sync.
- 16-voice option + CPU guard, if M0 numbers allow (03 §14.1).
- Modern preset bank (64–127).
- **Exit:** Remote UI edits every parameter and mirrors hardware edits within
  200 ms; 16-voice worst case ≤ 600 µs or option hidden.

### M4 — optional (→ v0.4)
- User wavetable import worker.
- Expansion wavetables 48–57 + Expansion preset bank 224–255 ([14](14-expansion-bank.md)).

## 3. Test plan

### 3.1 Native unit tests (host machine, CI)
- Oscillator: frequency accuracy (±0.5 cent over 8 octaves), wave index stepping,
  phase continuity on table change.
- Filter: cutoff accuracy, stability at max resonance + max modulation, no NaN/denormal
  (flush-to-zero enabled).
- Envelopes: segment times within 2 %, retrigger without discontinuity.
- Mod matrix: each source/target pair moves the target in the right direction.
- Voice allocator: stealing order, layer mode distribution (RR fairness, Random RR
  no-repeat).
- State: round-trip, fuzz, migration (06 §7).
- Contract: `chain_params`/`ui_hierarchy` parse as JSON, < 64 KB, every knob key
  exists, no selector key in `knobs`, every viz group contiguous in one row,
  ≤ 4 distinct `visible_if` params, every enum ≤ 128 options.

### 3.2 Golden audio tests
- **Previews (implemented):** `tools/emulator/render.py` renders every factory
  preset through the compiled C engine (`build/libamino_host`, built by
  `scripts/build.sh`) and writes WAVs + `levels.csv` + the gallery page; see
  docs/12. The C engine and its tests are the source of truth.
- **Reference emulator (historical, frozen):** `tools/emulator/` is a pure-Python model of
  docs 03/04/11, including both FX slots (all 11 types, routing, send, Parallel/Serial).
  `extract_presets.py` turns docs 09/10 (including FX notes such as
  `FX1 Delay .45 (1/4d, Fdbk 65 %, Tone dark)`) into `presets/factory/*.json`;
  `render.py` plays a per-category demo phrase (incl. pressure swells) through each
  preset and writes WAVs + `levels.csv` + `index.html`, warning on near-silent or
  clipping presets (`--dry` bypasses FX). All 128 render in ≈ 70 s. It was the
  behavioural spec while the C engine was written and is no longer maintained;
  previews now come from the C engine. Its documented
  APPROX points: boxcar instead of half-band decimation, Clean mode without
  mip-maps, control rate 16 samples, linear-interpolated FX delay reads, no Drive
   oversampling, one shared Shimmer pitch shifter, the Python emulator's Scale
   "Set" fallback is C minor (the C module reads Move's set), 120 BPM.
- Render fixed MIDI sequences for every factory preset to WAV (native build).
- Compare against committed goldens (spectral + RMS tolerance) to catch
  regressions.
- Silence test: no output > −120 dBFS 1 s after all notes off (except FX tails
  bounded by reverb decay).

### 3.3 Realtime safety
- Static check: CI greps `dsp/` for `malloc|free|fopen|printf|fprintf|pthread_mutex`
  outside `create_instance`/`destroy_instance` (allow-list).
- Runtime: native test harness with `malloc` interposer that aborts if called
  inside `render_block`/`set_param`/`get_param`/`on_midi`.
- Worst-case timing: 8 and 16 voices, all layers, max-resonance, reverb + delay,
  every param modulated by scenes each block; measure p99.9 block time on device.
- CPU guard test: force overload, verify no voice above 8 is granted and no
  sounding voice is cut.

### 3.4 Schwung integration (device)
- Load in each of 4 shadow slots simultaneously; CPU and dropouts.
- Autosave → reboot → state restored.
- My Presets save/recall; scenes morph; chain LFO on cutoff; automation lane record
  and playback.
- `validate.mjs amino` clean; `preview_knob_card.mjs` reviewed for every page.
- `help.json` width test.

### 3.5 Sound character validation
- Blind A/B listening against public Protein/Microwave demo recordings for:
  wave stepping, aliasing texture, filter drive, dirt types.
- Checklist owner: sound designer. Pass = "recognisably in the family".

### 3.6 Usability tests
- 5 Move users, scenarios U1–U7 (05 §10), think-aloud, timed.
- Fix any page where a user needs > 2 navigation steps or misreads a label.

## 4. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| CPU too high at 4× osc | Fewer voices | 2× osc, cheaper decimator, voice cap per layer |
| Host doesn't re-plan on `child_count` change | Layer UI shows 4 layers in simple mode | Keep `child_count: 4`, clamp `edit_layer`, hide multi-layer pages via gates |
| `chain_params` near 64 KB | Contract fails to load | Template keys if supported; trim metadata; generator enforces limit |
| 4-gate `visible_if` cap | Can't hide more conditional UI | Design already fits exactly 4; new gates need a trade |
| Sound not "Protein-like" | Product misses its point | M0 listening spike; sound designer sign-off per milestone |
| Trademark/IP complaint | Takedown | 07 guard-rails, original tables, neutral naming |
| Schwung API changes | Breakage | Pin `min_host_version`; CI against latest Schwung headers |

## 5. Decisions & remaining questions

Decided (2026-10-21):
- Name / id: **Amino** / `amino`.
- Voices: 8 default, up to 16 if CPU allows (03 §14.1).
- Arpeggiator: not built in.
- Licence: MIT (`LICENSE` at repo root; generated tables and presets are MIT too).
- Factory presets: 324 — Nostalgia [09](09-factory-presets.md), Modern [10](10-factory-presets-modern.md), genre banks [13](13-genre-banks.md), Expansion [14](14-expansion-bank.md), Trance Gate Showcase [16](16-trance-gate-presets.md), Expansion II [17](17-expansion-bank-2.md).
- Effects: Protein's 9 types + Amino extensions, [11](11-effects.md).
- Remote UI: required for v0.3.

Still open:
1. Should the Main page be user-assignable ("macro" knobs mapped per preset)?
2. Who signs off sound character and wavetable recipes (listening reviewer)?
