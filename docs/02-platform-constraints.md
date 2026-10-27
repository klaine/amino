# 02 — Move & Schwung Platform Constraints

Facts below are taken from Schwung's `docs/MODULES.md` (main branch, Oct 2026).
Anything marked **VERIFY** must be confirmed on a device or in Schwung source
before implementation depends on it.

## 1. Hardware (Ableton Move)

| Resource | Detail | Relevance |
|----------|--------|-----------|
| CPU | Quad Cortex-A72 @ 1.5 GHz, 2 GB RAM | Audio callback pinned to **core 3** |
| Display | 128 × 64 px, 1-bit | A 128-sample wave fits exactly one pixel per sample |
| Encoders | 8 touch-sensitive endless (CC 71–78; touch = notes 0–7) | Owned by the host's knob grid |
| Master knob | CC 79 (+ touch note 8) | Host-owned unless `claims_master_knob` |
| Jog wheel | CC 14 + click CC 3, touch note 9 | Host-owned: page navigation |
| Pads | 32, velocity + **polyphonic aftertouch** (notes 68–99) | Chain slot receives the *track's* notes, not pad IDs |
| Step buttons | 16 (notes 16–31) | Belong to Move's sequencer; do not capture |
| Buttons | Shift 49, Menu 50, Back 51, Capture 52, Up/Down 54/55, Undo 56, Loop 58, Copy 60, L/R 62/63, Play 85, Rec 86, Mute 88, Record 118, Delete 119; tracks 40–43 | Only some are claimable (see §4) |

## 2. Module type & packaging

- `component_type: "sound_generator"`, `chainable: true`, `api_version: 2`.
- Shared library **must** be named `dsp.so` in `modules/sound_generators/amino/`
  (the chain host builds this path itself and ignores `module.json`'s `dsp` field).
- Release artefact: `amino-module.tar.gz` extracting to `amino/`.
- `release.json` on `main` + catalog entry for Schwung Manager.

### Planned `module.json` (sketch)

```json
{
  "id": "amino",
  "name": "Amino",
  "abbrev": "AMINO",
  "version": "0.1.0",
  "api_version": 2,
  "description": "8-bit wavetable layer synth",
  "author": "TBD",
  "license": "MIT",
  "dsp": "dsp.so",
  "capabilities": {
    "chainable": true,
    "component_type": "sound_generator",
    "audio_out": true,
    "midi_in": true,
    "aftertouch": true,
    "claims_edit_ccs": true
  }
}
```

## 3. Plugin API (v2)

```c
void *create_instance(const char *module_dir, const char *json_defaults);
void  destroy_instance(void *inst);
void  on_midi(void *inst, const uint8_t *msg, int len, int source);
void  set_param(void *inst, const char *key, const char *val);
int   get_param(void *inst, const char *key, char *buf, int buf_len);
void  render_block(void *inst, int16_t *out_lr, int frames);   /* interleaved stereo int16 */
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
```

Host facts: 44 100 Hz, **128 frames per block** (≈ 2.9 ms), `host->get_beat_position()`
(may be NULL — guard), `get_bpm()`, `get_clock_status()`.

Optional export: `move_plugin_render_split(inst, voice_out[], n_voices, main_out, frames)`
for per-layer bus routing (accumulate, never memset, state-compatible with
`render_block`).

## 4. Realtime rules (non-negotiable)

Every entry point — including `set_param` and `get_param` — runs on the SPI audio
callback (SCHED_FIFO 70, core 3). Device-wide slack is ≈ 2 370 µs per block,
**shared with Move and every other module**.

Forbidden in any entry point: file I/O, `malloc`/`free`, locks held by non-RT
threads, `fork`/`exec`/`dlopen`, **any logging** (including `fprintf(stderr)`),
unbounded work (FFT, whole-buffer memsets, directory scans).

Consequences for Amino:
- Wavetables and factory presets are **compiled into `dsp.so`** as `const` data.
- All instance memory is allocated once in `create_instance` (runs on a loader
  thread, but must still be fast and thread-safe: shared static tables must be
  built at compile time or with a once-guard that is safe under concurrent loads).
- `get_param("chain_params")` / `("ui_hierarchy")` return **pre-built static strings**
  (no per-call JSON construction beyond `memcpy`).
- `get_param("state")` serialises into a caller buffer with no allocation.
- If a worker thread is ever needed (user wavetable import, v0.4), it must demote
  itself to SCHED_OTHER and pin to cores 0–2 as its first action.

## 5. Host UI model we build on

The Schwung knob grid ("param pages") renders our parameters; we do not draw the
main UI ourselves. What it gives us for free:

| Feature | How we use it |
|---------|---------------|
| `ui_hierarchy` levels → pages of 8 knobs (2 rows × 4) | One page per synth section |
| Jog = page through; Shift+click = section picker | Fast navigation |
| Touch a knob = header shows full name & value | Discoverability |
| Shift = fine adjust; built-in acceleration | Precise edits |
| `viz` groups (envelope, filter, lfo, waveform, fader, switch) | Draw ADSR, filter curve, shapes |
| Child selectors (`child_count`, `child_key_template`, `child_index_param`) | Layer Tools Copy/Delete picker |
| Hold Copy/Delete + pick instance (needs `claims_edit_ccs`) | Layer copy / init |
| `visible_if` (≤ 4 distinct gate params) | Hide layer & FX-type-specific controls |
| Preset browser (`list_param`/`count_param`/`name_param`) | Factory bank |
| "My Presets" + "Module" pages auto-appended | User presets |
| `canvas` + `as_page` + `drawPage` | Wave display page |
| `card_script` | Mod-slot card while turning amount |
| `live: true` + `<key>:effective` | Animate modulated wave position |
| Scenes / chain LFOs / automation | Any `chain_params` key is morphable |
| `help.json` | "Module Help" row on the Module page |

Limits to respect:
- Label cell ≈ 5 characters → every param gets a `short_name`.
- Enum options ≤ 128 (C-side knob mapping truncates beyond).
- `chain_params` + `ui_hierarchy` ≤ 64 KB.
- A viz group must sit contiguously **within one row of 4**.
- A sound generator's `ui_hierarchy` is only read from `get_param`, never from
  `module.json`. Amino returns its page contract as `ui_pages` instead so its
  `ui_chain.js` can own the matrix pads while reusing Schwung's shared page
  controller.
- Selector keys (preset) must not also be listed in `knobs`.
- `max_param` is unsupported; publish real `max`.
- Enum wire: accept both index and name in `set_param`; report one consistently
  from `get_param` (we report **index**).

## 6. Buttons we may claim

Not claimable (host-owned): Shift, Menu, Back, jog + click, 8 knobs, master knob,
track buttons, Mute.

We claim only `claims_edit_ccs` (Undo 56, Copy 60, Delete 119) for layer
copy/clear. We do **not** claim transport, arrows or step buttons — Move keeps them.

Amino's custom UI owns pad input while it is active. It forwards pad notes back
to Move on ordinary pages and consumes them only on Pad Matrix. Track buttons
remain host-owned and continue to select tracks.

## 7. Open platform questions (VERIFY)

1. Does the grid re-plan when `ui_hierarchy` changes after a plain knob write (e.g.
   `layer_count` 1 → 2)? MODULES.md says gate keys trigger re-plan for
   `visible_if`; a changed `child_count` may not. Fallback: always `child_count: 4`
   and gate layer pages with `visible_if`.
2. Does `child_index_param` work on many sibling levels sharing one focus key?
3. Exact host API symbol for BPM in `host_api_v1_t` on the current release.
4. Is per-pad polyphonic aftertouch delivered to chain synths as `0xA0` messages
   with the `aftertouch` capability?
5. CPU headroom actually available on core 3 with a typical Move set running.
