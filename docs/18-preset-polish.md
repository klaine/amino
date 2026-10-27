# 18 — Factory Bank Polish: Expression and Level

The preset tables in docs 09/10/13/14/16/17 describe each sound. Two bank-wide
passes run on top of them in the preset pipeline (`tools/emulator/polish.py`,
called by `extract_presets.py`), so every preset plays and levels the same way
without repeating the rules in 324 table rows.

## 1. Expression: pressure and mod wheel

Every preset except the two Inits answers pad pressure (aftertouch) and the mod
wheel. A layer that already has a route from a controller keeps it untouched.
Every added route is scaled by its controller, so **with no pressure and the
wheel down a preset sounds as authored**: checked bit-identical for 316 of the
320 changed presets. The other four (13, 18, 77, 79) use random sources
(S&H/Rand LFOs, noise); a faster LFO2 draws from the voice's random stream more
often, so they play a different random sequence of the same patch (level within
0.2 dB).

Targets are picked so the move is audible: a low-pass (LP24, LP12, Ladder 24dB)
counts as "openable" below cutoff 112; Osc 1 is "scannable" below wave 61 (61–63
are fixed classic waves).

| Category | Pressure (first that applies) | Mod wheel (first that applies) |
|----------|-------------------------------|--------------------------------|
| BS Bass | AT→Cut .15 · AT→Wave .12 | →Cutoff +.2 · →Drive +.3 (drive on) · →Osc1 Wave +.2 |
| LD Lead | AT→Cut .2 · AT→Wave .15 | vibrato on LFO2 · →Cutoff +.2 |
| KY Keys | AT→Cut .12 · AT→Wave .1 | vibrato on LFO2 (±0.24 st) · →Cutoff +.2 |
| PD Pad, DR Drone | AT→Cut .2 · AT→Wave .2 | →Osc1 Wave +.25 · →Cutoff +.2 · vibrato on LFO2 |
| PL Pluck | AT→Cut .12 · AT→Wave .1 | →E3 Decay +.25 (amp sustain < .5) · →Cutoff +.2 |
| FX | AT→Cut .25 · AT→Wave .25 | →LFO1 Rate +.3 (LFO1 in use) · →Osc1 Wave +.3 · →Cutoff +.25 |
| ML Multi | per layer: AT→Cut .15 · AT→Wave .12 | per layer: →Cutoff +.15 · →Osc1 Wave +.2 |

Mod-wheel vibrato uses LFO2 only when nothing else reads it: Sine, rate .74
(≈ 5.5 Hz), fade .25, `lfo2_mw` On, LFO 2 to Pitch .025 (±0.3 st at full wheel).
Wheel routes use the first free matrix slot.

## 2. Level calibration

The bank is calibrated to one loudness with the global **Patch Gain** trim
(`patch_gain`, ±24 dB, docs/04), applied after the FX and before the master
soft clip. `tools/emulator/calibrate_levels.py` plays every preset's demo
phrase through the compiled engine, measures BS.1770 integrated loudness and
sample peak, and picks the gain that reaches **−18 LUFS** without the sample
peak passing **−3 dBFS**. Short, spiky sounds are held by the peak ceiling and
sit up to ~6 dB under the loudness target, which is how they read in a mix. The
gains live in `presets/level_trims.json`; the Inits stay at 0 dB.

Layer volumes are not touched: they keep the balance between layers as the
sound was designed.

## 3. Engine changes that came with the pass

- Voices are AC-coupled (5 Hz one-pole DC blocker after the drive), so
  asymmetric tables and shapers no longer put DC on the bus.
- Delay Time is read on the documented exponential curve (docs/11); v1 states
  are converted on load (docs/06 §6).

## 4. Workflow after a sound change

```sh
python3 tools/emulator/extract_presets.py      # docs -> JSON (+ expression, trims)
scripts/build.sh --gen                         # engine + library with the new bank
python3 tools/emulator/calibrate_levels.py     # re-level (all, or given indices)
python3 tools/emulator/extract_presets.py && python3 tools/gen/gen.py
tests/golden/check_golden.py build/amino_golden --write   # when the change is intended
```

Measured on the demo phrases (2026-10-26): loudness spread went from
−40…−6 LUFS (σ 5.8 dB) to −23.8…−15.5 LUFS (σ 0.7 dB), 312 of 324 presets within
±1.5 dB of target, highest peak −2.9 dBFS, largest DC offset 0.29 → 0.006 of
full scale.
