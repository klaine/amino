# Amino — an 8-bit wavetable synth for Ableton Move (Schwung)

**Status:** implemented — `dsp.so` (aarch64), factory bank, contract, Remote UI,
native and golden tests.

Amino is a Schwung `sound_generator` module: an 8-voice (16 max), 4-layer
8-bit wavetable synth with a gritty multimode filter, four envelopes, two LFOs,
an eight-slot mod matrix, expression and two multi-effect slots, with an
interface designed from the ground up for Move's 8 touch-sensitive encoders,
jog wheel, pads and 128×64 display.

**Listen first:** [preset gallery](https://klaine.github.io/amino/gallery/) (all 324 factory
presets, audio previews) · [play it in your browser](https://klaine.github.io/amino/) (WebAssembly build, WebMIDI).

## Goals in one paragraph

Sound like a Protein: gritty, aliased, 8-bit stepped wavetables through a warm
CEM-style multimode filter, with layers, round-robin, dirt noise, flavour and
classic Iridium-style effects. Play better than a Protein on Move: no "Select-knob
diving" — every page is eight physical knobs, envelopes are drawn as envelopes,
and the whole synth is reachable in at most two jog steps from anywhere.

## Document map

| # | Document | What it answers |
|---|----------|-----------------|
| 01 | [Product requirements](docs/01-product-requirements.md) | What we build, for whom, scope per release |
| 02 | [Move & Schwung platform constraints](docs/02-platform-constraints.md) | What the host gives us, what it forbids |
| 03 | [DSP architecture](docs/03-dsp-architecture.md) | Voice/layer engine, oscillators, filter, mod, FX, CPU budget |
| 04 | [Parameter reference](docs/04-parameter-reference.md) | Every parameter key, range, unit, default, short label |
| 05 | [UX & Move control design](docs/05-ux-control-design.md) | Page plan, knob maps, layer workflow, display, gestures |
| 06 | [Presets & state](docs/06-presets-and-state.md) | State blob, factory bank, layer copy/load |
| 07 | [Wavetables & IP](docs/07-wavetables-and-ip.md) | Wavetable format, content strategy, legal guard-rails |
| 08 | [Implementation roadmap & test plan](docs/08-roadmap-and-testing.md) | Milestones, repo layout, verification, risks, decisions |
| 09 | [Factory presets — Nostalgia bank](docs/09-factory-presets.md) | Presets 0–63, Microwave-era archetypes |
| 10 | [Factory presets — Modern bank](docs/10-factory-presets-modern.md) | Presets 64–127, current production styles |
| 11 | [Effects](docs/11-effects.md) | The 9 effect types, routing, parameters, CPU/memory |
| 12 | [Preview rendering & Python tools](docs/12-emulator-guide-for-agents.md) | The C engine as source of truth, rendering the preset gallery with it, and what the (frozen) Python emulator is still for |
| 13 | [Genre banks](docs/13-genre-banks.md) | 2000s Trance, Tech House, Deep House, Minimal Techno — 24 presets each + preview sequences |
| 14 | [Expansion bank](docs/14-expansion-bank.md) | Wavetables 48–57 rationale + presets 224–255 that use them |
| 16 | [Trance Gate showcase](docs/16-trance-gate-presets.md) | Four new presets demonstrating gate patterns with compression, delay and reverb |

## Decisions (2026-10-21)

| Topic | Decision |
|-------|----------|
| Name / id | Amino / `amino` |
| Polyphony | 8 by default, up to 16 when CPU allows |
| Arpeggiator | Not built in; users pair Amino with Schwung's `arp` MIDI FX or Move's sequencer |
| Licence | MIT |
| Factory presets | 324 original: Nostalgia (0–63), Modern (64–127), 2000s Trance (128–151), Tech House (152–175), Deep House (176–199), Minimal Techno (200–223), Expansion (224–255), Trance Gate Showcase (256–259), Expansion II (260–323) |
| Effects | Protein's 9 types + Shimmer + Trance Gate in 2 slots, serial chain, Crush, Pump, Freeze ([11](docs/11-effects.md)) |
| Remote UI | Required for v0.3 |
| Wavetables | 74 generated: 52 procedural (MIT, incl. expansion tables 48–57 and 58–67) + 22 from AKWF-FREE (CC0, incl. 68–73); generator in `tools/wavegen/` |

## Reading order

1. Product owners / sound designers: 01 → 05 → 04.
2. DSP developers: 02 → 03 → 04 → 07.
3. UI developers: 02 → 05 → 04 → 06.

## Build, test, install

The shipped module is the folder `modules/sound_generators/amino/`
(`dsp.so`, `module.json`, `ui_chain.js`, `mod_matrix_ui.mjs`, `ui_pages.json`,
`help.json`, `web_ui.html`, `assets/`, `LICENSE`, `THIRD_PARTY_NOTICES.md`) or
`amino-module.tar.gz`. Download the package from the
[Releases](../../releases) page, or build it: `dsp.so` and the package are
build outputs and are not committed.

```
py -3 tools/gen/gen.py
py -3 tests/contract/validate_contract.py
zig cc -std=c11 -O0 -I src/dsp -o amino_test.exe src/dsp/*.c tests/host/test_main.c -lm
./amino_test.exe
zig cc -target aarch64-linux-gnu.2.28 -std=c11 -O2 -fPIC -shared -I src/dsp \
  -o modules/sound_generators/amino/dsp.so src/dsp/*.c -lm -ldl
```

On macOS/Linux, `scripts/build.sh` runs the same steps (see AGENTS.md); use
`python3` for `py -3` and `brew install zig` for the cross-compiler.

The C engine in `src/dsp/` is the reference implementation; its native, golden
and contract tests define the behaviour. Preset previews and the gallery page
are rendered with the same compiled engine (`build/libamino_host`, built by
`scripts/build.sh`):

```
python3 tools/emulator/render.py
python3 -m http.server -d tools/emulator/out
```

To work on the Remote UI without a Move, run the harness (UI in the
manager's sandboxed iframe, backed by the compiled engine, with keyboard and
audio): `python3 tools/remote_ui/dev_server.py`, then open
<http://localhost:7700/>. See docs/15 §7.2.

The pure-Python emulator in `tools/emulator/engine.py`/`fx.py` is frozen
history; see [docs/12](docs/12-emulator-guide-for-agents.md).

To install on a Move, add an SSH key at `http://move.local/development/ssh`, then
run `scripts/deploy.sh [host]`: it stages `amino-module.tar.gz` on the device and
renames it into `/data/UserData/schwung/modules/sound_generators/amino` (or install
the package with Schwung Manager). Rescan modules afterwards; `scripts/logs.sh`
follows Schwung's debug log. Build and test
details, including the golden-audio check, are in
[docs/15-implementation-notes.md](docs/15-implementation-notes.md).

## Browser build (WebAssembly + WebMIDI)

The same `src/dsp` engine compiles to WebAssembly and runs in an AudioWorklet
behind the Remote UI, as one static site (no server code):

```
scripts/build.sh --gen     # once: generates the module assets the site reuses
scripts/build_web.sh       # build/amino.wasm, node tests, build/site/
python3 -m http.server -d build/site 7710
```

`build/amino.html` (about 1.7 MB) is the same page as **one self-contained
file**: engine, UI, wavetable previews and presets are all inside it, so it can be
mailed, dropped in a chat or opened from disk (a copy is also published as
`amino-standalone.html` on Pages).

Open <http://localhost:7710/>, press **Start Amino**, and play from a WebMIDI
keyboard (Chrome, Edge, Opera; Safari has no WebMIDI), the computer keys A-K
(Z/X shift the octave; they work while the synth panel has focus too) or the
on-screen keys (letters show which computer key plays each; a phone gets one
octave of larger keys). **MIDI channels -> layers** assigns each layer Omni, Off
or a channel 1-16 (mixing is fine); it switches the engine to its Channel layer
mode while on, and the summary line shows whether it is on. The patch is remembered in localStorage.
GitHub Pages serves the `pages` branch, which holds `build/amino.html` as
`index.html` and the preset gallery. Sources are in `web/`; see [docs/15](docs/15-implementation-notes.md) §8.

## Key references

- Schwung framework: <https://github.com/charlesvestal/schwung>
  (`docs/MODULES.md`, `docs/API.md`, `docs/REALTIME_SAFETY.md`, `src/host/plugin_api_v1.h`)
- Waldorf Protein product page and manual: <https://waldorfmusic.com/en/protein>
- Synth Anatomy Protein review (feature inventory used for parity checks):
  <https://synthanatomy.com/2026/02/waldorf-protein-review-a-modern-desktop-wavetable-synth-with-an-8-bit-soul.html>
