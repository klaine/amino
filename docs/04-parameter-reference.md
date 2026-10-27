# 04 — Parameter Reference

## 1. Conventions

- **Global keys** are bare: `master_vol`.
- **Per-layer keys** follow `l{index}_{key}` with `index` 1–4: `l1_cutoff`,
  `l3_osc2_wave`. On-device sound pages use the bare `{key}` alias and edit the
  focused `edit_layer`; `l{index}_{key}` remains available for explicit layer
  controls and clients. The dedicated Layer Tools page keeps a child selector
  for Copy/Delete.
- **Mod slot keys** nest: `l{index}_m{slot}_{src|dst|amt|via|via_depth}` with slot 1–8.
- Floats travel as decimal strings with ≥ 3 decimals; enums as **index** strings
  (names also accepted on input).
- Factory presets leave `scale` untouched. `Set` follows Move's current root and
  scale when selected manually.
- `short` = `short_name` drawn in the 5-char knob cell. `name` is shown in the
  header when the knob is touched.
- `M` = matrix target available. `S` = smoothed.
- All params are listed in `chain_params` (so scenes, chain LFOs and automation can
  reach them) unless marked *internal*.

## 2. Global parameters

| Key | Name | short | Type | Range | Default | Unit |
|-----|------|-------|------|-------|---------|------|
| `preset` | Preset | — | int | 0..N−1 | 0 | selector (not a knob) |
| `preset_name` | Preset Name | — | string | | | read |
| `preset_count` | Preset Count | — | int | | | read |
| `master_vol` | Master Volume | Vol | float | 0..1 | 0.8 | % |
| `patch_gain` | Patch Gain | Gain | float | −24..+24 | 0 | dB; per-patch output trim before the master soft clip. Factory presets are level-calibrated with it; a state blob without it plays at 0 dB |
| `layer_count` | Layers | Lyrs | int | 1..4 | 1 | |
| `layer_mode` | Layer Mode | Mode | enum | Stack, RoundRb, RandRR, Split, Chan | Stack | |
| `edit_layer` | Edit Layer | Edit | int | 1..4 | 1 | `child_index_param` |
| `voices` | Voices | Voice | int | 1..16 | 8 | |
| `bend_range` | Bend Range | Bend | int | 0..24 | 2 | st |
| `mpe` | MPE | MPE | enum | Off, On | Off | |
| `flavour` | Flavour (global) | Flav | float | 0..1 | 0.15 | % |
| `fx1_type` | FX 1 Type | Type | enum | Off, Chorus, Flanger, Phaser, Tremolo, Drive, Comp, EQ, Delay, Reverb, Shimmer, Trance Gate | Off | |
| `fx_chain` | FX Chain | Chain | enum | Parallel, Serial | Serial | |
| `fx1_mix` | FX 1 Mix | Mix | float | 0..1 | 0.3 | % |
| `fx1_p1`..`fx1_p6` | type-dependent (see §5) | | float | 0..1 | per type | |
| `fx2_type` … `fx2_p6` | as FX 1 | | | | Off, 0.3 | |

## 3. Per-layer parameters (`l{n}_…`)

### 3.1 Layer

| Key | Name | short | Type | Range | Default |
|-----|------|-------|------|-------|---------|
| `on` | Layer On | On | enum | Off, On | L1 On, others Off |
| `vol` | Layer Volume | Vol | float 0..1 | | 0.8 |
| `poly` | Voice Mode | Poly | enum | Poly, Mono, Legato | Poly |
| `octave` | Octave | Oct | int | −3..+3 | 0 |
| `glide` | Glide | Glide | float 0..1 (→ 0–5 s) | | 0 |
| `key_lo` / `key_hi` | Key Low / High | KeyLo / KeyHi | note | 0..127 | 0 / 127 |
| `midi_ch` | MIDI Channel | Chan | int | 1..16 | n |
| `route` | FX Route | Route | enum | Dry, FX1, FX2 | FX1 |
| `send` | FX Send | Send | float 0..1 | | 1.0 (M) |
| `flavour` | Flavour | Flav | float 0..1 | | 0.0 (adds to global) |
| `osc_mode` | Osc Character | Char | enum | Vintage, Clean | Vintage |
| `scale` | Scale | Scale | enum | Off, Set, Major, Minor, Dorian, Phryg, Lydian, Mixo, Penta, MinPen, Blues, Chrom… | Off |
| `chord` | Chord | Chord | enum | Off, Maj, Min, 7, Maj7, m7, sus2, sus4, 5th, Oct, Stack4 | Off |

### 3.2 Oscillators (`osc1_…`, `osc2_…`)

| Key suffix | Name | short | Type | Range | Default | Flags |
|------------|------|-------|------|-------|---------|-------|
| `table` | Wavetable | Table | enum | table names (≤ 128) | Osc1 0, Osc2 0 | |
| `wave` | Wave Position | Wave | int | 0..63 | 0 | M S, `live` |
| `semi` | Semitone | Semi | int | −48..+48 | 0 | M, `display: big` |
| `fine` | Fine Tune | Fine | int | −100..+100 | 0 / +7 | cents, S |
| `level` | Level | Level | float 0..1 | | 1.0 / 0.0 | M S |
| `wenv` | Wave Env Amount | WEnv | float −1..+1 | | 0 | S |
| `vel` | Velocity → Wave | Vel | float −1..+1 | | 0 | |
| `kbd` | Keytrack | Kbd | enum | Off, On | On | |
| `link` | Link to Osc 1 (osc2 only) | Link | enum | Off, On | Off | switch |

Cross modulation, Osc 2 → Osc 1 (03 §4.4.1), per layer:

| Key | Name | short | Type | Range | Default | Flags |
|-----|------|-------|------|-------|---------|-------|
| `xmod_type` | Cross Mod Type | XMode | enum | Off, PM, AM, Sync | Off | short options `Off PM AM Sync` |
| `xmod` | Cross Mod Amount | XMod | float 0..1 | | 0 | M S |

### 3.3 Dirt & drive

| Key | Name | short | Type | Range | Default |
|-----|------|-------|------|-------|---------|
| `dirt_type` | Dirt Type | Dirt | enum | Static, Crackle, Geiger, Click, Burst, Ring | Static |
| `dirt` | Dirt Level | Amt | float 0..1 | | 0 (M S) |
| `drv_type` | Drive Type | Type | enum | Off, Trans, Tube, Diode, Fold | Off |
| `drive` | Drive | Drive | float 0..1 | | 0 (M S) |
| `drv_pos` | Drive Position | Pos | enum | Pre, Post | Pre |

### 3.4 Filter

| Key | Name | short | Type | Range | Default | viz |
|-----|------|-------|------|-------|---------|-----|
| `ftype` | Filter Type | Type | enum | LP24, LP12, Ladder 24dB, HP24, HP12, BP12, Off | LP24 | filter/mode |
| `cutoff` | Cutoff | Cutoff | float 0..127 | | 100 | filter/cutoff, M S |
| `res` | Resonance | Res | float 0..1 | | 0.1 | filter/resonance, M S |
| `fenv` | Filter Env Amount | Env | float −1..+1 | | 0.3 | S |
| `fkbd` | Filter Keytrack | Kbd | float 0..2 | | 0.5 | % |
| `fvel` | Filter Velocity | Vel | float −1..+1 | | 0.2 | |

### 3.5 Envelopes (`e1_` Wave, `e2_` Filter, `e3_` Amp, `e4_` Aux)

| Key suffix | Name | short | Type | Range | Default (E1 / E2 / E3 / E4) | viz |
|------------|------|-------|------|-------|------------------------|-----|
| `a` | Attack | Atk | float 0..1 (→ 0–20 s, curved) | | 0 / 0 / 0 / 0 | envelope/attack |
| `d` | Decay | Dec | float 0..1 | | 0.5 / 0.4 / 0.3 / 0.4 | envelope/decay |
| `s` | Sustain | Sus | float 0..1 | | 0 / 0.2 / 1.0 / 0.5 | envelope/sustain |
| `r` | Release | Rel | float 0..1 | | 0.3 / 0.3 / 0.25 / 0.3 | envelope/release |

`viz.group` = `e1`, `e2`, `e3`, `e4`. E4 (Aux) has no fixed destination: it
reaches the sound through its two dedicated routes (§3.10) or as mod source
*Aux Env* in the matrix. It has its own Move page under Filter & Amp.

### 3.6 Amp & pan

| Key | Name | short | Type | Range | Default |
|-----|------|-------|------|-------|---------|
| `amp_vel` | Amp Velocity | Vel | float 0..1 | | 0.5 |
| `pan_mode` | Pan Mode | PanM | enum | Fixed, Random, Alt, Spread, LFO1, LFO2 | Fixed |
| `pan` | Pan / Width | Pan | float −1..+1 | | 0 (M S) |

### 3.7 LFOs (`lfo1_`, `lfo2_`)

| Key suffix | Name | short | Type | Range | Default |
|------------|------|-------|------|-------|---------|
| `shape` | Shape | Shape | enum | Sine, Tri, Pulse, SawDn, SawUp, Rand, S&H | Tri |
| `rate` | Rate | Rate | float 0..1 (→ 0.01–50 Hz) | | 0.4 (M) |
| `sync` | Sync | Sync | enum | Off, On | Off |
| `div` | Sync Rate | Div | `rate` type (include_triplets) | 1/32T..16 bars | 1/4 |
| `phase` | Phase Mode | Phase | enum | Free, Voice, Global | Voice |
| `fade` | Fade In | Fade | float 0..1 | | 0 |
| `mw` | ModWheel Link | MW | enum | Off, On | Off |
| `pitch` | LFO to Pitch | Pitch | float −1..+1 (`%`) | ±12 semitones at ±100% | 0 |

`pitch` is a dedicated LFO → pitch route (vibrato) outside the matrix. It
follows the LFO's own value, so `mw` On turns it into mod-wheel vibrato, and
the *LFO1/LFO2 Depth* matrix targets scale it like the LFO's matrix slots.

`rate` and `div` are both always shown; `sync` selects which one is used. Hiding
one with `visible_if` would spend the 4-gate budget (see 05 §4.11).

### 3.8 Mod matrix (`m1_` … `m8_`)

| Key suffix | Name | short | Type | Range | Default |
|------------|------|-------|------|-------|---------|
| `src` | Source | Src | enum | 27 sources (03 §10) | Off |
| `dst` | Target | Dst | enum | 25 targets | Off |
| `amt` | Amount | Amt | float −1..+1 | | 0 |
| `via` | Via | Via | enum | Off + sources | Off |
| `via_depth` | Via Depth | VDep | float | 0..1 | 1 |

Shift+knob fine adjustment for Amount uses a 0.001 step (0.1 percentage point).
Via Depth blends between an unscaled route (0) and the selected Via source at
full strength (1).

Init patch slots: M1 = LFO1 → Osc1 Wave (0), M2 = LFO2 → Cutoff (0),
M3 = ModWheel → LFO1 Depth (0). Amounts at 0 so they are "pre-wired": the user
only turns Amount. Aftertouch and velocity have dedicated routes (§3.9), so they
do not consume matrix slots.

### 3.9 Expression routes (Move-specific, outside the matrix)

Dedicated, always-present routes for pad playing. Shown on the **Play** page.

| Key | Name | short | Type | Range | Default |
|-----|------|-------|------|-------|---------|
| `osc1_vel` | Velocity → Osc 1 Wave | Vel W1 | (same key as §3.2) | | 0 |
| `osc2_vel` | Velocity → Osc 2 Wave | Vel W2 | (same key as §3.2) | | 0 |
| `fvel` | Velocity → Cutoff | VelF | (same key as §3.4) | | 0.2 |
| `amp_vel` | Velocity → Amp | VelA | (same key as §3.6) | | 0.5 |
| `fkbd` | Filter Keytrack | Kbd | (same key as §3.4) | | 0.5 |
| `at_wave` | Aftertouch → Wave (both osc) | ATWav | float −1..+1 | | 0 |
| `at_cut` | Aftertouch → Cutoff | ATCut | float −1..+1 | | 0 |
| `at_vib` | Aftertouch → Vibrato depth (LFO1 → pitch) | ATVib | float 0..1 | | 0 |

Poly AT is used per voice when present; channel AT is used otherwise.

### 3.10 Aux Env routes (outside the matrix)

Two dedicated routes from E4, shown on the **Aux Env** page's bottom row.
They add `amt × E4` to the chosen target exactly as a matrix slot would
(same target list and scaling, applied before the LFOs update, so LFO rate
and depth targets work), without using one of the eight slots.

| Key | Name | short | Type | Range | Default |
|-----|------|-------|------|-------|---------|
| `e4_dst1` | Aux Env Target 1 | Dst1 | enum | 25 targets (as `mN_dst`) | Off |
| `e4_amt1` | Aux Env Amount 1 | Amt1 | float −1..+1 (`%`) | | 0 |
| `e4_dst2` | Aux Env Target 2 | Dst2 | enum | 25 targets | Off |
| `e4_amt2` | Aux Env Amount 2 | Amt2 | float −1..+1 (`%`) | | 0 |

## 4. Derived / internal keys (not in `chain_params`)

| Key | Purpose |
|-----|---------|
| `state` | Full serialised state (06) |
| `chain_params` | Static JSON contract |
| `ui_hierarchy` | Static JSON hierarchy |
| `l{n}_osc{k}_wave:effective` | Current modulated wave position for live drawing |
| `wave_view` / `wave_view2` | 128-char hex (64 points × 8-bit, decimated) of the focused layer's current Osc 1 / Osc 2 wave, for hardware canvas consumers |
| `split_voices` | `[{"id":"l1","label":"Layer 1"},…]` (v0.3) |
| `layer_load_src` / `layer_load_go` | Load Layer source preset + trigger (`access: write`) |
| `init_layer` | Trigger: reset focused layer (`access: write`) |
| `randomize` | Trigger: musical randomise of focused layer (`access: write`) |

## 5. FX parameter meanings

`fxN_p1`..`p6` are generic float 0..1 keys so FX type changes never alter the key
set (keeps scenes/presets stable). Both UIs show type-specific controls: the
device has one page per effect type with these labels (05 §4.13) and a card that
prints the value in real units; the Remote UI labels and formats them the same
way. Ranges, curves and defaults: 11 §4.

| Type | p1 | p2 | p3 | p4 | p5 | p6 |
|------|----|----|----|----|----|----|
| Chorus | Rate | Depth | Delay | Fdbk | Spread | Mode |
| Flanger | Rate | Depth | Fdbk | Delay | Spread | Sync |
| Phaser | Rate | Depth | Fdbk | Stages | Center | Spread |
| Tremolo | Rate | Depth | Shape | Sync | Stereo | Phase |
| Drive | Gain | Tone | Type (device: Curve) | Out | LowCut | Crush |
| Comp | Thresh | Ratio | Atk | Rel | Makeup | Knee |
| EQ | Low | Mid | MidF | High | Q | LowCut |
| Delay | Time | Sync | Fdbk | Tone | PingP | Mod |
| Reverb | Size | Decay | Damp | PreD | Mod | Width |
| Shimmer | Size | Decay | Damp | Shim | Intvl | Mod |
| Trance Gate | Pattern (device: Patt) | Timing (Grid) | Attack | Release | Length (Len) | Floor |

## 6. Parameter count

- Global: ~25
- Per layer: 13 + 18 + 5 + 6 + 12 + 3 + 14 + 32 + 3 = **106** (plan; the
  generated table in `tools/gen/params.py` is authoritative — 126 layer keys
  as of the dedicated LFO-pitch and Aux Env routes)
- Total concrete keys: ≈ 25 + 4 × 106 ≈ **450**

`chain_params` with full metadata ≈ 450 × ~110 bytes ≈ 50 KB — under the 64 KB cap
but close. Mitigation: the per-layer entries are declared once via the child
template (host multiplies them), so `chain_params` lists **template keys** where the
host supports it. **VERIFY** whether `chain_params` itself honours child templates;
if not, trim metadata (omit `name` where `short_name` + level context suffices).
