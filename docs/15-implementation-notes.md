# 15 - Implementation Notes

This document records where the implementation had to make a decision the
design docs left open, or where the platform contract forced a small deviation.
It is not a second specification: docs 01-14 remain authoritative for sound and
UX unless a note here says otherwise.

## 1. Source layout

```
src/module.json              module metadata (id: amino, api_version: 2)
src/ui_chain.js              custom hardware UI; binds Schwung's shared page controller
src/mod_matrix_ui.mjs        Pad Matrix drawing and pure interaction helpers
src/ui_pages.json            generated page contract for the custom controller
src/help.json                on-device help tree
src/web_ui.html              Remote UI shell (vanilla ES modules, no CDN)
src/web/                     Remote UI modules + style, packaged as assets/ui/ (§7.2)
src/cards.js                 lazy on-device wave preview cards
src/dsp/
  plugin_api.h               Schwung v1/v2 ABI (vendored)
  amino.h                    instance, voice, layer, FX structs
  params_gen.h/.c            generated parameter descriptors
  factory_presets.c/.h       generated bank + Load Layer source list
  wavetables.c/.h            generated 58 x 64 x 128 int8 tables
  contract_gen.c             generated chain_params / ui_hierarchy / ui_pages strings
  params.c                   lookup, get/set, state, preset and layer ops
  voice.c                    voice allocator and per-voice DSP
  fx.c                       two FX slots, eleven algorithms, one arena per slot
  amino.c                    v2 entry point, MIDI, routing and master
tools/gen/
  params.py                  single source of truth for parameters
  gen.py                     emits C/JS artefacts and fills missing help-page guides
  dump_contract.py           decodes the contract strings for inspection
tests/host/test_main.c       native smoke test
tests/golden/golden_main.c   fixed-phrase renderer for all factory presets
tests/golden/check_golden.py regression check against tests/golden/golden.json
tests/contract/validate_contract.py   contract limits and page checks
```

The design docs propose `tools/params.yaml`; the implementation uses
`tools/gen/params.py` instead. It carries the same fields and keeps generated C
files, packaged Remote UI and wave-preview assets reproducible with one command:

```
py -3 tools/gen/gen.py
```

Everything under `src/dsp/` that is generated says so in its first line. The
generator also emits per-table browser previews into the installed module's
`assets/wave-previews/` directory.

The hardware UI intentionally refuses `get_param("ui_hierarchy")` so Schwung
loads Amino's `ui_chain.js`, which owns the pad surface.
The generated hierarchy is published as `ui_pages`; Amino's custom UI feeds it
to the shared page controller, preserving the existing pages and appending the
new Pad Matrix page.

`release.json` and `.github/workflows/release.yml` from the docs/08 layout are
distribution-time artefacts: they carry a real `download_url` once the module has
a public repository. They are deliberately not invented here, and they are not
part of the installed module folder (`dsp.so`, `module.json`, `help.json`,
`web_ui.html`, `assets/ui/`, `assets/ui_meta.json`, `assets/ui_presets.json`,
`assets/wave-previews/*.bin`, `LICENSE`).

## 2. Parameter wire format and IDs

- Keys are the documented keys. Global keys are bare, per-layer parameters use
  `l1_`..`l4_`. Bare layer keys (the child-template form) target the focused
  layer from `edit_layer`.
- `set_param` accepts an enum index or name, case-insensitively. `get_param`
  reports the index.
- Floats are printed with three decimals in `get_param` and in `state`;
  ints/enums are printed without a decimal part.
- Factory presets and the state blob store floats with three decimals, so a
  save/load round-trip is byte-identical for a given bank item.
- Internally a preset value is `(param id, float)`:
  - globals: `id = index` (0..25)
  - layers: `id = 0x8000 | ((layer - 1) << 9) | index`
- `state` is a flat JSON object. `apply_key` ignores unknown keys and applies
  values in order; the parser never allocates.

## 3. Global flavour key is `flavour_global`

docs/04 lists both a global `flavour` and a per-layer `flavour`. Inside a child
page the per-layer parameter is addressed by the template key `flavour`, which
collides with the global key in `chain_params` (keys must be unique). The
implementation therefore exposes the global parameter as `flavour_global` and
keeps `flavour` / `lN_flavour` for the per-layer value:

- state blob: `"flavour_global":"0.150"`, `"l1_flavour":"0.250"`
- Layers page knob: `flavour_global`
- Voice page knob and any child page: `flavour`
- Factory preset JSON from docs 09/10 that still writes a global `flavour` is
  translated to `flavour_global` by the preset generator.

`get_param("flavour_global")` and `set_param("flavour_global", ...)` work as
documented for every other parameter.

## 4. Enums

Enum descriptors publish `min: 0`, `max: option_count - 1`. This matters because
preset application and enum parsing clamp through the same descriptor; deriving
the range from the option list keeps all 74 wavetable slots and all 26 mod
sources addressable.

## 5. FX parameter controls stay floats

`fxN_p1`..`fxN_p6` are declared as float 0..1 with per-type names on the
type-gated page. Many of them are really selectors (Delay sync division, Drive
mode, Reverb freeze, ...). Declaring a different enum per FX type would require
26 separate chain parameters for the same six keys and would break scene
automation. The DSP quantises the selector-like controls internally instead;
the page labels (`Sync`, `Mode`, `PingP`, ...) make the intent visible.

Selecting a type from Off with all six knobs still at zero writes the documented
per-type default knob values, so a fresh Delay/Reverb sounds musical immediately.
FX type changes cross-fade over 20 ms and clear the arena incrementally, at most
64 KB per block.

## 6. Contract decisions

- `chain_params` lists globals plus bare layer keys. Sound pages use those bare
  aliases and the focused `edit_layer`, matching Main's write path. Concrete
  `lN_*` keys remain on the simultaneous Layer Mix/Route and Key Split pages.
- Only Layer Tools uses child-key expansion; it retains the host's layer
  Copy/Delete gesture without putting sound edits through child pages.
- `root` is the Main knob grid from docs/05 §4.2; its eight knobs are exactly
  `osc1_wave`, `osc2_wave`, `cutoff`, `res`, `fenv`, `e3_a`, `e3_r`, `flavour`.
  The preset selector triple lives on a separate `presets` level, rather than
  combining a browser and Main grid on root (which showed selector cells in the
  Main row on device).
- Gate parameters are exactly four: `layer_count`, `layer_mode`, `fx1_type`,
  `fx2_type`.
- Page-local labels are unique and at most five characters; the generator and
  `tests/contract/validate_contract.py` enforce it.
- `osc1_wave` / `osc2_wave` declare `live: true` and a `wave_view` /
  `wave_view2` extra key for the hardware wave display. The Remote UI instead
  lazy-loads generated 64-point table previews from `assets/wave-previews/` and
  renders locally as table or wave-position parameters change.
- `preset_list` is a read-only, prebuilt JSON string of all factory names
  (used by the Remote UI). `preset_name`, `preset`, `preset_count` retain their
  documented behaviour.
- `get_param("split_voices")` returns `""` (no per-voice outputs).
- `get_param("rui_live")` and `set_param("audition", …)` serve the Remote UI;
  see §7.2.
- `get_param("rui_poll")` returns `rev:on:tick:bpm[:devms]` (docs/05 9.3):
  - `rev` is a monotonic edit counter, bumped by every `set_param` that changes
    a snapshot-visible value; the `state` blob carries the same value in its
    `"rev"` field, and loading a state restores it so a save/load round-trip
    stays byte-identical.
  - `on` is 1 while the host reports `MOVE_CLOCK_STATUS_RUNNING`.
  - `tick` is the 24-PPQN transport position (`get_beat_position() * 24`, or an
    internal accumulator on older hosts).
  - `bpm` comes from `get_bpm()`.
  - `devms` is only appended while playing; the stopped digest is byte-stable.

## 7.1 On-device UI audit against docs/05

The generated hierarchy now pins the Main/Osc/Envelope/Mixer/Filter/Amp/Play/
Voice/LFO/Mod/FX/Layer page knob order, the four visibility gates, all layer
copy keys, note-type Key Split controls, and the Presets selector on a separate
level. `validate_contract.py` checks those layouts and help coverage; the help
generator adds a knob-order leaf for every grid and selector page.
The `get_param("ui_hierarchy")` wire value is the direct `{"levels": ...}`
object (the outer `ui_hierarchy` key is for module.json declarations only); the
contract test guards this because the wrapped form makes the device fall back to
the flat PARAMS list.

Both oscillator wave-position parameters declare a lazy `card_script` in
`chain_params`; `cards.js` draws the current 64-point wave while the knob is
touched or turned. The card is hidden otherwise and degrades to the normal knob
on hosts without card support. The Remote UI now uses `knobs` as the control
order (with `params` only for page-local metadata), groups Sound pages into
collapsed sections, updates waveform canvases while their tab is open, and adds
category filters plus a searchable Load Layer source list. Its float writes
are normalized to three decimal places as required by docs/05 §9.2.

Remaining host/UX gaps are explicit:

- The dedicated full-width **Wave canvas page** from docs/05 §4.3 is not in the
  on-device hierarchy. The hardware has per-wave position widgets; the full
  two-wave display is currently only in `web_ui.html`.
- `Load Layer` is a separate items-list level reached from Tools, not a fourth
  knob binding. The host treats item selectors as their own page/door.
- Cutoff-to-Hz, LFO-rate-to-Hz, and envelope-knob-to-time displays are nonlinear
  mappings. The current hierarchy publishes the correct editable base ranges,
  but the host contract has no transform callback, so those cells show the raw
  control values rather than the design document's mapped units. Do not attach
  `Hz`/`sec` to the normalized values; that would be misleading.


## 7.2 Remote UI architecture

The Remote UI (`src/web_ui.html` + `src/web/`) is laid out in signal-flow
order — Osc 1, Osc 2, Dirt & Drive, Filter, Amp, Effects — with a modulation
dock (Envelopes, LFOs, Matrix, Play, Global) below, instead of mirroring the
hardware's 8-knob pages. (The earlier single-file page was retired on
2026-10-23 once the preset browser and Load Layer picker replaced it.)

- `tools/gen/gen.py` writes `assets/ui_meta.json` from `tools/gen/params.py`:
  every parameter's range, options, default and scope, `DISPLAY` curves
  (cutoff note → Hz, envelope 20·x³ s, LFO 0.01·5000ˣ Hz, glide 5·x² s,
  key-range note names), per-type FX knob labels/curves (`FX_PARAMS`, mirroring
  `fx.c`, with defaults parsed from `FX_DEFAULTS`), the mod-target → knob map,
  and the preset/layer-source lists. Nothing in the browser hard-codes these.
- `store.js` keeps one value cache. Controls bind to bare keys; layer keys
  resolve to `l<edit_layer>_<key>` and are re-pointed (not rebuilt) when the
  edit layer changes. Device updates repaint only bindings of changed keys,
  once per animation frame. Writes are optimistic, ≤ 30/s per key with the
  final value always sent, and device echoes are ignored for 300 ms after a
  write so older values cannot snap a control back.
- `format.js` converts values to text in real units and parses typed values
  ("2.5 kHz", "350 ms", "C#5"); `controls.js` provides the knob (drag,
  Shift-fine, double-click reset, keys, wheel while focused, click to type),
  enum and toggle widgets; `layout.js` builds the shell once.
- Visual editors (Phase 2), with their maths in `curves.js` so it is
  testable:
  - `wavetable.js`: each oscillator card shows the whole table as a waterfall,
    the selected position as stepped 8-bit samples, and shades the positions
    Wave env and velocity can reach (clamped to the morph slots 0–60; 61–63
    are fixed classic waves). Matrix slots aimed at the wave and Aftertouch
    to Wave add their own spans, shaded and captioned in their source colour
    (`waveReach` in `curves.js` sums them). Drag across it to set the position. With Link
    on, Osc 2 shows Osc 1's table and position, as `params.c`/`voice.c` play
    them. The table button opens a searchable grid of morph thumbnails.
  - `filter.js`: small-signal response of the voice filter (4-pole ladder,
    k = 3.9·res with (1 + k/2) input gain, for LP24/HP24; SVF with
    R = 1 − 0.97·res for the 12 dB types), log axis 20 Hz–20 kHz, with the
    filter-envelope sweep (Env × 72 st) shaded. Drag sideways for cutoff,
    vertically for resonance; arrow keys work when focused.
  - `envelope.js`: ADSR editors in the dock. Stage widths follow the knob
    values; the curves follow `env_step` (attack chases 1.3 and stops at 1.0;
    decay/release use a time constant of 1/4.6 of the stage time). Handles
    drag, double-click to reset, and take arrow keys.
- Modulation (Phase 3), `mod.js` + `lfo.js`:
  - `ModModel` reads the focused layer's eight slots and gives each target
    knob one ring per live slot (up to three drawn), spanning what the slot
    sweeps: `modTargetScale` is the knob units per amount 1.0 that `voice.c`
    adds (cutoff 72 st, wave 63, osc pitch 48 st, LFO rate 0.5, others 1.0;
    Amp Level and FX Send multiply, `"mul"`), and bipolar sources (LFOs, Key,
    Bend, Random Bi, Voice #, Layer Alt, Flavour) swing both ways.
  - Header source chips: drag one onto a knob to route it (reuses an existing
    slot, else the first empty one at +25 %); click one to arm it, then drag
    any target knob to set that source's amount on it. Escape disarms.
  - LFO previews draw two cycles of the shape (`lfo_shape`), the Fade In ramp,
    the rate or division and what the LFO drives. The dot animates at the free
    rate only: the Remote UI does not receive the host tempo.
- Phase 4, all loaded on first use (dynamic `import()`, styles in
  `lazy.css`):
  - `presets.js`: preset browser drawer — search, type chips,
    favourites (localStorage, by name), arrow keys audition, Enter keeps;
    a Load layer tab loads any factory layer into the focused layer.
  - `history.js` (after first paint): undo/redo of every edit made in the
    browser (a drag is one step; Cmd/Ctrl+Z, Shift+Cmd+Z / Ctrl+Y) and of
    bulk actions (preset or layer load, Init, Randomise, layer copy, A/B),
    which store a snapshot and write back only differing values. A/B
    compare keeps two sounds; B starts as a copy; loading a preset resets it.
    Edits made on Move itself are not recorded, and history is lost on reload.
  - `layers.js` (dock tab): mode and count with a description, one strip per
    layer pinned to that layer, Copy to L*, and a 128-key keyboard with
    draggable range bars (dimmed outside Split mode).
  - `fxchain.js`: routing diagram in the Effects card; click a layer to cycle
    Dry/FX1/FX2, click the middle to switch Serial/Parallel.
  - `help.js`: side panel following the pointer/focus: real-unit range,
    default, layer, curve and mod-target notes, the matching `help.json` page
    and all help topics. Opens with the ? button or key.
- Phase 5, DSP additions for the Remote UI:
  - `get_param("rui_live")` (`amino_live` in `amino.c`) returns
    `v=total,l1..l4;l=ph1,cur1,ph2,cur2;e=st,lv,st,lv,st,lv;p=peak;b=bpm;t=ms`:
    active voices, the focused layer's newest voice's LFO phases/held random
    values and Wave/Filter/Amp envelope stage (`AminoEnvStage`) and level, the
    output peak (block peak with ~300 ms decay, kept by `render_block`), the
    tempo, and the instance clock. It is computed on request; the only
    render-side cost is the peak. It is declared as a viz extra key on Osc 1's
    wave widget, so schwung-manager pushes it to the browser about every
    500 ms at rest and 150 ms after an edit (the harness emulates this). The
    UI animates LFO dots between updates from the phase, rate and tempo.
  - `set_param("audition", "note:vel")` plays a note (vel 0 releases;
    `"off"` releases all) for the browser keyboard (`keyboard.js`). It is not
    a sound edit: `rev` and the preset index are unchanged. Up to eight notes
    are tracked; in Chan mode they use the focused layer's MIDI channel; any
    still held after 30 s are released by `render_block`, so a closed tab
    cannot leave a note hanging. The browser also releases on blur and when
    the page is hidden.
  - Everything runs on the audio callback (plugin_api.h), so neither needs
    locking. Native tests cover both, including the timeout.
- Animated modulation cues (`modanim.js`, loaded after first paint): each
  mod ring carries a dot at the value its source gives right now, and the
  knob's track a ghost tick for the combined result
  (`curves.modulatedPositions`). LFOs run on from the last telemetry phase at
  their rate (`lfoRateHz`, free or tempo-synced); envelopes run on from the
  last reported stage and level through `curves.envStep`, the same maths as
  `env_step`, so both move smoothly between the ~2 Hz updates. Sources with
  no telemetry (velocity, wheel, aftertouch, key, random…) get a flowing dash
  instead of an invented position. Header chips pulse with their source.
  Dots dim while no voice plays (LFOs keep running as a preview); reduced
  motion updates only on telemetry. Telemetry follows the focused layer's
  newest voice, so with several notes held the cues show that one.
- Payload: `assets/ui_meta.json` stores shared option lists once
  (`optionLists` + `optionsRef`, restored by `expandMeta`), and the preset and
  Load Layer lists live in `assets/ui_presets.json`, loaded after first paint.
  First load (HTML, CSS, eager modules, metadata) is about 134 KB of the
  150 KB in docs/05 §9.2; the envelope and LFO editors load with their dock
  tab and the Phase 4–5 modules when first used. New features should load
  lazily the same way.
- `assets/wave-previews/NN.bin` are offset-binary (128 = zero): 64 waves ×
  64 samples, each the average of two signed source samples. (Before
  2026-10-23 the generator averaged the signed bytes as unsigned, which drew
  every wave's negative half at the top in the old single-file page.)
- `tests/host/test_web_ui.mjs` covers the metadata, formatting/parsing, the
  curve maths, the preview encoding, and the store's layer re-pointing,
  coalescing and echo handling.

### Development harness

`python3 tools/remote_ui/dev_server.py` serves the UI at
`http://localhost:7700/` in the same sandboxed iframe the manager uses, with a
stand-in `schwung-remote-api.js` backed by one instance of
`build/libamino_host`. `src/web*` edits are served live (run `gen.py` after
changing `params.py`). The harness page has width presets, a Classic/New
switch, an on-screen and computer keyboard (A–K, Z/X octave) and streams the
instance's audio to the browser.

## 8. Memory and realtime behaviour

- Four instance slots are a static pool (`g_pool`); `create_instance` claims one
  with an atomic compare-exchange and `destroy_instance` releases it. No malloc
  anywhere in the module.
- Each instance owns two 820000-byte FX arenas and 16 voices. Four resident
  instances are about 6.6 MB of BSS.
- `render_block`, `set_param`, `get_param`, `on_midi` and the v2 entry point do
  no file I/O, allocation, locking or logging. `state` and `get_param` use
  `snprintf` into the caller's buffer, as docs/06 specifies.
- Wavetable bytes are signed int8 and are normalized by 128 before entering
  the signal path, including phase/amplitude modulation. Per-voice control-rate
  slices write at `base + i` within the block; native Init tests cover both
  normalization and full-block output.
- Continuous per-layer controls use a shared 30 ms one-pole smoother with
  snapshots every 16 frames, avoiding block-sized parameter steps while keeping
  all voices in a layer on the same control trajectory.
- `Scale=Set` reads Move's root and scale through the Schwung `move_info` export
  (C minor remains the fallback when the host does not publish set info).
- Live parameter writes rebuild only the table or modulation cache they affect;
  ordinary cutoff/level/envelope edits do not reconstruct unrelated runtime
  routing state.
- The docs/03 14.1 CPU guard (stop granting voices above 8 after N overloaded
  blocks) is not implemented: the host API exposes no monotonic clock, and the
  only available time sources are syscalls that are not realtime-safe. `voices`
  still defaults to 8 and accepts up to 16, so the guard can be added on device
  once a safe timing source exists; until then 16 voices is a user choice, as
  docs/03 allows.
- The whole wavetable bank and the 324 factory presets are `const` data in
  `dsp.so`; `dsp.so` is about 1.3 MB, of which 606 KB is wavetable samples.

## 9. Build and test

Portable cross-compile (Zig used during development; any aarch64 Linux GCC
works):

```
zig cc -target aarch64-linux-gnu -std=c11 -O2 -fPIC -shared \
  -I src/dsp -o modules/sound_generators/amino/dsp.so \
  src/dsp/params.c src/dsp/voice.c src/dsp/fx.c src/dsp/amino.c \
  src/dsp/params_gen.c src/dsp/factory_presets.c \
  src/dsp/contract_gen.c src/dsp/wavetables.c -lm -ldl
```

Native smoke test:

```
zig cc -std=c11 -O0 -I src/dsp -o amino_test.exe \
  src/dsp/*.c tests/host/test_main.c -lm
./amino_test.exe
```

Contract validation:

```
py -3 tests/contract/validate_contract.py
```

The native test covers contract sizes, all 324 presets, an eleven-type FX sweep in
Parallel and Serial, four-layer rendering, mono/legato, sustain, randomise/init,
state round-trip, partial and malformed state loads, Remote UI digest behaviour,
repeatable random-LFO output across instance slots, Mono retrigger phase
continuity, a MIDI stress loop and the post-note-off silence floor. The Init
render also checks that output spans all 128 frames and is not hard-clipped; a
rapid Mono note-on/off regression protects against a release-tail voice masking
the newly gated voice from note-off.

Golden audio test (docs/08 3.2). `golden_main.c` creates a fresh instance per
preset, uses the preset tempo and advancing transport, clears/fades in FX before
the phrase, then plays the same fixed three-note phrase. It measures RMS, peak,
zero-crossing rate and a 24-bin log-spaced spectral centroid, and prints one JSON
object per preset.
`check_golden.py` compares that against the committed `golden.json`:

```
zig cc -std=c11 -O2 -I src/dsp -o amino_golden.exe \
  src/dsp/*.c tests/golden/golden_main.c -lm
py -3 tests/golden/check_golden.py amino_golden.exe
```

Tolerances are 12-20 % relative (plus a small absolute floor) to absorb libm and
ISA differences between build hosts. Re-generate goldens deliberately with
`--write` after an intentional sound change.

Factory preset loads seed the instance stream from `preset index + 1`; each
voice stream xors the same seed with the voice-id salt used by the Python
emulator. Random-driven renders are therefore repeatable across instances and
processes; `Randomize` still advances the instance stream.

## 10. IP guard-rails

All 74 tables are generated by `tools/wavegen` from MIT/CC0 material or
procedural recipes (docs/07). Module, preset and parameter names avoid the
trademarks listed in docs/07; the emulator's historical module name is not used
anywhere in the shipped data.

## 8. Browser build (WebAssembly)

- **Engine:** `web/wasm_host.c` is a one-instance stand-in for the Schwung host
  (like `tools/emulator/c_host.c`) with fixed scratch buffers, compiled with
  `zig cc -target wasm32-wasi -mexec-model=reactor` together with `src/dsp`.
  The module has no imports. `-DAMINO_WASM` drops the aarch64-only
  `reserved == +120` assert in `plugin_api.h` (wasm32 pointers are 4 bytes);
  the device build is unchanged. The engine already follows the host sample
  rate, so the AudioContext rate is passed to `wa_init`.
- **Audio:** `web/worklet.js` owns the engine and renders one 128-frame quantum
  per `process()`. The page talks to it by messages that are handled in order.
- **Remote UI:** `web/bridge.js` mirrors `tools/remote_ui/dev_server.py` (value
  burst, then diffs, viz extra-key pump) and `web/schwung-remote-api.js` is the
  in-page `schwungRemote`. The module's `web_ui.html` is copied with its script
  path made relative so the site works under a GitHub Pages sub-path.
- **MIDI routing:** `web/router.js` (pure, tested in `tests/web/`). With
  routing on, the engine is put in Channel layer mode with layer N on engine
  channel N; each incoming message is fanned out to the layers whose
  assignment matches (Omni hears all, Off hears none). Note, poly pressure and
  All Notes/Sound Off go per layer; other messages go once on channel 1.
  Preset loads that change Layer Mode are overridden while routing is on.
- **Single file:** `scripts/build_single.py` packs `amino.wasm`, the worklet
  source, the UI modules/styles and the JSON/wave data into one gzip+base64
  payload inside `build/amino.html`, together with `web/embed.js` and the host
  scripts bundled as one classic script. The worklet loads from a `data:` URL;
  the Remote UI becomes an iframe `srcdoc` whose ES modules come from an import
  map of `data:` URLs (relative specifiers are rewritten to `amino-ui/*.js`),
  and whose `fetch()` is answered from the embedded store. Needs
  `DecompressionStream` (current Chrome, Edge, Firefox, Safari).
- **Limits:** no WebMIDI in Safari; no host transport (the page has BPM and a
  Transport switch for synced LFOs/delays); audio starts only after a click.
