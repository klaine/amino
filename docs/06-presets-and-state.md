# 06 — Presets & State

## 1. Requirements recap

- Schwung autosave, "My Presets", scenes and automation all rely on one opaque
  `state` blob: `get_param("state")` must be **complete and self-contained**;
  `set_param("state", blob)` must restore it exactly.
- `state` must reflect **base** values (the host puts modulated knobs back to base
  around a state read).
- Always answer the `state` query (return blob, or `-1` if none) — never time out.
- No allocation or file I/O in either direction.

## 2. State blob format

Flat JSON object, keys = parameter keys, values = strings (same wire format as
`set_param`). Flat because the host's JSON tools and the Remote UI param explosion
only handle flat objects.

```json
{
  "fmt": "amino",
  "v": "2",
  "name": "Glass Insect",
  "master_vol": "0.800",
  "layer_count": "2",
  "layer_mode": "1",
  "edit_layer": "1",
  "fx1_type": "9",
  "fx1_mix": "0.250",
  "l1_osc1_table": "12",
  "l1_osc1_wave": "23",
  "...": "..."
}
```

Rules:
- `v` = state schema version. Loader accepts any `v ≤ current`, applies
  migrations, ignores unknown keys, and leaves params whose keys are missing
  untouched (section 7). Blobs carry every key, so a full blob still restores
  the patch exactly.
- Always write **every** key. Omitting default values would make old presets
  change sound whenever a default changes.
- Size estimate: ~450 keys × ~22 bytes ≈ 10 KB. Fits the shadow param mailbox
  (up to 64 KB).
- `edit_layer` is saved (users return to the layer they were editing).

### Serialiser
Iterates a static `param_desc[]` table (key, type, offset into instance struct,
format). `snprintf` into the caller buffer; returns `-1` if the buffer is too
small (never truncates silently).

### Parser
Single-pass, allocation-free tokenizer over the incoming string: for each
`"key":"value"` pair → binary search the sorted `param_desc[]` → apply. Values are
applied as **targets**; smoothers are snapped (no glide) on a full state load.
All voices are released with a 5 ms fade before applying a state load.

## 3. Factory presets

- Authored as JSON files in `presets/factory/*.json` (same format as the state
  blob, plus `category`).
- A build script converts them into a `const` C array (`factory_presets.c`): per
  preset a name, category and compact `uint16`-indexed value list. No file I/O at
  runtime.
- `preset` selector: `set_param("preset", "N")` applies preset N (same path as a
  state load). `preset_name`, `preset_count` answer the browser.
- Categories (name prefix in the browser, e.g. `BS Sub Crunch`): **BS** Bass,
  **LD** Lead, **PD** Pad, **KY** Keys, **PL** Pluck, **FX** Effects,
  **DR** Drones, **ML** Multi-layer, **IN** Init.
- Source preset files may be **deltas from Init** (only changed keys); the build
  script expands them to full key sets, so the compiled bank and every saved
  `state` always contain every key.
- Today `presets/factory/*.json` are **generated** from docs 09/10/13/14/16 by
  `tools/emulator/extract_presets.py` (multi-layer presets from
  `tools/emulator/ml_presets.py`). Once sound design moves to the JSON files, they
  become the source of truth and the docs tables are regenerated from them.
- Banks: **Nostalgia** 0–63 ([09](09-factory-presets.md)), **Modern** 64–127
  ([10](10-factory-presets-modern.md)), and the genre banks **2000s Trance**
  128–151, **Tech House** 152–175, **Deep House** 176–199, **Minimal Techno**
  200–223 ([13](13-genre-banks.md)), **Expansion** 224–255
  ([14](14-expansion-bank.md)), **Trance Gate Showcase** 256–259
  ([16](16-trance-gate-presets.md)), and **Expansion II** 260–323
  ([17](17-expansion-bank-2.md)). Factory presets do not store `scale`, except
  257 and 259, which shipped with `Scale Set`. Each preset file carries `bank`, `category`
  and the bank's design `bpm`; the Remote UI filters by bank and category, the
  hardware browser lists in index order.
- Preset 0 is always **Init** (1 layer, Saw table, filter open, no FX); preset 64
  is **Init Modern**.

### Factory content plan
| Release | Count | Notes |
|---------|-------|-------|
| v0.1 | 20 | Single-layer subset of Nostalgia, tables 0–15 only, FX off |
| v0.2 | 64 | Full Nostalgia bank: round-robin, split, layered, FX enabled |
| v0.3 | 224 | + Modern bank (Clean mode, chords, scales, MPE) + 4 genre banks |
| v0.4 | 256 | + Expansion bank (wavetables 48–57) |
| v0.5 | 324 | + Trance Gate Showcase, Expansion II bank (wavetables 58–73) |

Every factory preset must:
- Set at least the Main page to musically useful ranges (Main is what users touch).
- Use the Play page so pressure does something audible.
- Pass a loudness check (−18 to −12 LUFS short-term on a C3 chord) to keep
  presets level-matched.

## 4. User presets

Handled by Schwung's generic "My Presets" (stores our `state`). No module code
needed beyond a correct `state`. The preset name inside our blob (`name`) is
updated when the host saves a user preset **only if** the host passes it — otherwise
the factory name is kept (**VERIFY** host behaviour).

## 5. Layer operations

| Operation | Mechanism |
|-----------|-----------|
| Copy layer A → B | Host gesture (hold Copy + pick) writes `child_copy_keys` from A to B |
| Init layer | Host gesture (hold Delete + pick) writes each key's declared `default` |
| Undo | Host-provided, one level |
| **Load Layer** | Tools page → items list `layer_src_list` (entries `"<preset>/L<n>"`, only non-empty layers). Select writes `layer_load_src`; module copies that factory layer's values into the focused layer. |
| Init (trigger) | `init_layer` resets focused layer to Init-preset layer 1 |
| Randomise | `randomize` with `rand_amt`: perturbs table, wave, cutoff, res, env times, LFO, dirt within musical bounds; seeded so Undo via host works |

`layer_src_list` is built **at compile time** (factory presets are static), so
`get_param` only `memcpy`s a prepared string. Must stay < 64 KB; if > 128 entries,
paginate by category into sub-levels.

## 6. Versioning & migration

| Change type | Handling |
|-------------|----------|
| New param | Missing in old blobs → default. Default must reproduce old behaviour. |
| Renamed param | Alias table in parser (`old_key → new_key`). |
| Enum options added | Append only (indices are stored). A reorder needs a `v` bump and an index remap for older blobs (v2: Filter Type, see params.c). |
| Knob curve fix | v2: Delay Time is read on the documented exp curve; v1 blobs with a Delay slot have `fxN_p1` converted so the saved time is kept. |
| Range change | Migration function keyed on `v`. |
| Wavetable added | Append only; table index is stored. |

## 7. Tests

- Round-trip: for each factory preset, `apply → get state → apply → get state`
  is byte-identical.
- Fuzz: random/truncated/garbage blobs never crash or hang; partial blobs leave
  untouched params intact.
- Migration: golden blobs from each released version load with expected values.
