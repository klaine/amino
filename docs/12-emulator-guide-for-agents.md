# 12 — Guide: Preview Rendering and the Python Tools (for agents)

Audience: coding agents (and humans) changing Amino's sound, presets or tests.
It explains which implementation is authoritative, how preset previews and the
gallery page are rendered, and what the Python code in `tools/emulator/` is
still for.

Read first: [02 platform constraints](02-platform-constraints.md),
[03 DSP architecture](03-dsp-architecture.md), [04 parameters](04-parameter-reference.md),
[11 effects](11-effects.md), [15 implementation notes](15-implementation-notes.md).

---

## 1. Source of truth

**The C engine in `src/dsp/` is the reference implementation.** Together with
its tests it defines how Amino behaves:

| Check | What it pins down | Where |
|-------|-------------------|-------|
| Native test | Behaviour and regressions (voices, layers, mod matrix, FX, state, MIDI) | `tests/host/test_main.c` |
| Golden audio | Level/brightness of every factory preset | `tests/golden/` (`golden.json`) |
| Contract | `chain_params` / `ui_pages` UI contract | `tests/contract/validate_contract.py` |

**Rule 1 — Behaviour flows one way:** docs → C engine (+ tests). A behaviour
change updates the docs, the C code, a regression test and, when presets change
sound, `golden.json` (`check_golden.py --write`) in the same change.

**Rule 2 — Previews come from the C engine.** The gallery renders through the
compiled engine (section 3), so what you hear is what the Move module plays.

### 1.1 History: the Python emulator

`tools/emulator/engine.py` and `fx.py` are a pure-Python model of the synth that
served as the executable spec while the C module was written ("docs → emulator →
C"). They are **frozen and no longer maintained**: the C engine has since gained
behaviour the emulator lacks (the Via scaler, Release Velocity, voice-steal
fades, FX type defaults, fixed EQ bands, the bounded `am_tanh`, Legato-aware LFO
fade, ...), and the emulator was never bit-exact anyway (RNG, decimator and
interpolation differ). Do not fix C to match the emulator, and do not extend the
emulator; treat it as a readable description of the original design. (The
`FX_DEFS` parameter tables in `fx.py` are the exception: the preset pipeline
still uses them, see section 2.) Its last
renders are kept in `tools/emulator/out/before_emulator/` for A/B listening.

---

## 2. File map

| File | Role | Status |
|------|------|--------|
| `tools/emulator/notation.py`, `extract_presets.py`, `ml_presets.py` | Docs 09/10/13/14/16/17 → `presets/factory/*.json` | **Active** (preset pipeline, feeds `tools/gen/gen.py`) |
| `tools/emulator/polish.py` | Bank-wide expression routes and Patch Gain trims, applied by `extract_presets.py` (docs/18) | **Active** |
| `tools/emulator/calibrate_levels.py` | Measures each preset's loudness/peak through the C engine and writes `presets/level_trims.json` (docs/18) | **Active** |
| `tools/emulator/render.py` | Demo phrases per category/genre; renders previews | **Active** (audio via the C engine) |
| `tools/emulator/c_host.c` | Minimal Schwung-like host: tempo, transport, MIDI, 128-frame blocks | **Active** (built into the library) |
| `tools/emulator/c_engine.py` | `ctypes` wrapper around `build/libamino_host` | **Active** |
| `tools/emulator/gallery.py` | Builds `out/index.html` (listening page, A/B against `out/before_*`) | **Active** |
| `tools/emulator/amino_params.py` | Init defaults and enum lists used by the preset pipeline | **Active** |
| `tools/emulator/fx.py` `FX_DEFS`, `div_seconds` | FX parameter names, ranges, curves and options; `notation.py` uses them to turn FX notation in the docs into p-values | **Active** — keep in step with the knob curves in `fx.c` |
| `tools/emulator/engine.py`, `fx.py` effect classes | Pure-Python synth and FX model | **Frozen** (history, section 1.1) |

---

## 3. Commands

```sh
# once
tools/wavegen/fetch_akwf.sh
python3 tools/wavegen/wavegen.py               # wavetables -> tools/wavegen/out/

# every time docs 09/10/13/14/16/17 or ml_presets.py change
python3 tools/emulator/extract_presets.py      # -> presets/factory/*.json (324)
scripts/build.sh --gen                         # regenerate C tables, test, build
python3 tools/emulator/calibrate_levels.py     # re-level the bank (docs/18), then
python3 tools/emulator/extract_presets.py      # bake the trims and build --gen again

# render previews with the compiled engine
scripts/build.sh                               # builds build/libamino_host.{dylib,so}
python3 tools/emulator/render.py               # all 324 presets, with FX (~5 s + gallery)
python3 tools/emulator/gallery.py --site DIR   # publishable gallery with MP3s (needs `pip install lameenc`); the live one is /gallery/ on the pages branch
python3 tools/emulator/render.py 3 84 112      # selected indices
python3 tools/emulator/render.py --dry         # FX bypassed

# listen
python3 -m http.server -d tools/emulator/out   # open http://localhost:8000
```

`render.py` refuses to run when `build/libamino_host` is missing or older than
`src/dsp/*` or `c_host.c`; run `scripts/build.sh` first. On Windows build the
library by hand (`zig cc -shared -O2 -I src/dsp -o build/libamino_host.dll
src/dsp/*.c tools/emulator/c_host.c`); `c_engine.py` looks for `.dll` there.

Outputs in `tools/emulator/out/`: `NNN_slug.wav` (44.1 kHz stereo 16-bit),
`levels.csv` (peak/RMS), `index.html`. The run prints `WARNING` lines for
near-silent (< −30 dBFS peak) or clipping (> −0.5 dBFS) presets — treat these
as preset or engine bugs to investigate.

### 3.1 How a preview is rendered

For each preset JSON, `render.py`:

1. creates an engine instance at the preset's design tempo (`bpm` in the JSON; 120
   for Nostalgia/Modern/Expansion, genre tempos for docs/13);
2. loads the **compiled** factory preset with `set_param("preset", index)`
   (identical to the JSON: `gen.py` builds `factory_presets.c` from it);
   `--dry` then sets both FX slots Off;
3. renders 32 blocks of pre-roll so FX arena clearing and fade-in finish;
4. starts the transport at beat 0 and plays the phrase as MIDI note on/off
   (velocity × 127) plus channel aftertouch for the phrase's pressure curve,
   applying events at 128-frame block boundaries like the device;
5. keeps rendering after the phrase until the output stays below ≈ −72 dBFS for
   0.3 s, at most 6 s.

Phrases, by doc number (`doc_no`): category phrases for 0–127, 224–255 and 260–323,
`trance` role phrases for 256–259, and genre sequences `<genre>_<role>` for 128–223 (`phrase()`,
`genre_phrase()`). Chord presets play one key per step; the engine adds the
chord tones.

---

## 4. Using the engine programmatically

Run from `tools/emulator/` after `scripts/build.sh`:

```python
import c_engine

lib = c_engine.load()
with c_engine.Engine(lib, bpm=120) as eng:
    eng.set("preset", 3)                       # or any set_param key/value
    eng.set("l1_cutoff", 40)
    eng.transport(True)
    eng.note_on(48, 0.9)
    pcm = bytearray()
    for blk in range(int(1.0 * 44100 / 128)):
        if blk == int(0.5 * 44100 / 128):
            eng.note_off(48)
        pcm += eng.render()                    # interleaved stereo int16 bytes
    print(eng.get("state")[:80])
```

The wrapper goes through the real plugin entry points (`create_instance`,
`set_param`, `get_param`, `on_midi`, `render_block`), so anything the host can do
is scriptable: state blobs, layer tools, FX type changes, and any MIDI message
with `eng.midi(status, d1, d2)` (CCs, pitch bend, poly aftertouch).
At most four instances exist at once (the module's static pool); close them.

---

## 5. Canonical constants

These were first decided in the emulator and now live in C; change them there
(and in docs/04 or 11, the tests and goldens) — never in the frozen emulator.

| Constant | Value | Where in C |
|----------|-------|------------|
| Control rate | 16 samples | `AMINO_CTRL` (`amino.h`) |
| Env time | `t = 20 s · x³`, min 0.5 ms; attack target 1.3, τ = t/1.466; decay/release τ = t/4.6 | `am_env_time` (`util.h`), `env_step` (`voice.c`) |
| LFO rate | `0.01 · 5000^x` Hz; sync via `div_beats` | `am_lfo_hz`, `div_beats` |
| Glide | `5 s · x²`, τ = t/4.6 | `am_glide_time` |
| Cutoff | MIDI-note units → `am_note_hz`, clamp 20 Hz–18 kHz | `render_voice` |
| Wave position | modulated position clamped 0–60; base ≥ 61 = fixed classic wave; new wave applied at phase wrap | `render_voice` |
| Mod scaling | Pitch ±48 st, Cutoff ±72 st, Wave ±63 slots, Level/Dirt/Res/Drive/Pan ±1, LFO rate ±0.5 knob, env-stage mods in knob units | `AMINO_*_RANGE` (`amino.h`) |
| Via | multiplier `1 − depth + depth · via`; LFO-sourced or LFO-scaled routes run after the LFO update | `am_via_multiplier`, `render_voice` |
| Expression | AT→Vib ±1.5 st × LFO1; Vel→Cutoff `fvel · vel · 48` st; Kbd `fkbd · (pitch − 60)` st | `render_voice` |
| Amp velocity | `1 − amp_vel · (1 − vel)` | `render_voice` |
| Flavour | wave ±3 slots, pitch ±12 ct, cutoff ±1 st, × (layer + global flavour) | `voice_start_now` |
| Ladder | `k = res · 3.9`, input gain `(1 + 0.5k) · 0.8`, `am_tanh` on feedback node | `render_voice` |
| SVF | `R = 1 − 0.97 · res`, BP output × 2R | `render_voice` |
| Drive (layer) | gain `1 + 15·d`, output `1/(1 + d)` | `render_voice` |
| Mixer | `(o1·L1 + o2·L2 + dirt) · 0.5` | `render_voice` |
| Voice gain | 0.9; master `am_tanh(x · master_vol)`, `am_tanh` clamped at ±3 | `AMINO_VOICE_GAIN`, `render_block` (`amino.c`) |
| Pan | equal power `cos/sin((pan+1)·π/4)` | `am_pan_gains` |
| Steal fade | 2 ms linear, then silence until the queued note starts | `voice_trigger`, `render_voice` |
| FX curves & defaults | per-type tables | `fx.c` (`FX_DEFAULTS`, `fx_*`) |
| FX mix | equal-power for time FX, linear for Tremolo/Drive/Comp/EQ/Trance Gate | `amino_fx_process` |
| FX modulation | Chorus taps, Phaser sweep and Reverb line modulation evaluated every 16 samples, linearly interpolated | `FX_MOD_CHUNK` (`fx.c`) |

---

## 6. Checking a sound change

1. Make the change in `src/dsp/` and add or adjust a regression test in
   `tests/host/test_main.c`.
2. `scripts/build.sh` — the golden check reports how far each preset moved.
3. To compare by ear, move the current previews aside first, then re-render:
   ```sh
   mkdir tools/emulator/out/before_<label>
   mv tools/emulator/out/*.wav tools/emulator/out/before_<label>/
   python3 tools/emulator/render.py
   ```
   `gallery.py` adds an A/B switch per card for every `out/before_*` folder.
4. When the change is intended, `tests/golden/check_golden.py <golden binary>
   --write` and commit `golden.json` with the change.

To isolate a problem: render one preset (`render.py N`), bypass FX (`--dry`),
then reduce the preset with `c_engine` (section 4) by setting single keys back to
Init values until the difference disappears.

---

## 7. Guard-rails for agents

- Previews and goldens must come from the C engine; never from `engine.py`.
- Never change a constant silently to make a test pass. A constant change is a
  behaviour change: update section 5, docs/04 or 11, the tests and goldens, and
  say so in the change description.
- Keep the Python tools stdlib-only (no numpy) so they run anywhere.
- Regenerate, do not hand-edit: `presets/factory/*.json` (from docs 09/10/13/14/16
  + `ml_presets.py`), `tools/wavegen/out/*` (from `wavegen.py`) and the generated
  `src/dsp/*_gen.c`, `factory_presets.c`, `wavetables.c` (from `tools/gen/gen.py`).
- Do not commit `tools/emulator/out/` (git-ignored); commit fixtures and goldens
  you deliberately curate under `tests/`.
- Respect licences: wavetables 32–47 derive from AKWF-FREE (CC0); 0–31 and 48–57
  are procedural (MIT); no Waldorf data.
- New wavetables are **appended** (table index is stored in presets/state); add
  the recipe to `recipes.EXPANSION`-style lists after the AKWF block, never in
  between.
