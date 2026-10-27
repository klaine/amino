# 07 — Wavetables & IP Guard-rails

## 1. Legal guard-rails (read first)

| Do | Don't |
|----|-------|
| Generate our own wavetables with documented scripts | Ship, transcode or "re-derive" Microwave/Protein ROM wavetables |
| Use third-party waves only under CC0 / public domain (AKWF-FREE) | Use GPL (e.g. Surge) or "free for use" waves with unclear licences |
| Describe the module as "8-bit wavetable layer synth" | Use "Waldorf", "Protein", "Microwave" in the module name, id or presets |
| Write the DSP from public descriptions and listening | Disassemble Protein firmware or copy Waldorf code |
| Use generic waveform names ("Resonant", "Formant A") | Use the original table names |
| Mention inspiration only in prose docs, if at all | Imply endorsement by Waldorf |

Optional user import (v0.4) lets users load their **own** files; we ship no
converter for proprietary dumps.

> Not legal advice. Have the naming and description reviewed before publishing to
> the Schwung catalog.

## 2. Wavetable format (runtime)

| Property | Value |
|----------|-------|
| Waves per table | 64 (index 0–63) |
| Samples per wave | 128 |
| Sample format | signed 8-bit (`int8_t`) |
| Slots 61 / 62 / 63 | Triangle / Square / Saw — identical in every table |
| In-between waves | Precomputed offline from keyframes (linear interpolation → 8-bit re-quantise) |
| Storage | `const int8_t tables[N][64][128]` in `wavetables.c` (8 KB per table) |
| Clean-mode mip-maps | Generated offline: 8 octave bands × float or int16, only for tables used in Clean mode (budget ≤ 4 MB) |

Why 8-bit and stepped: the character the Protein is loved for comes from coarse
quantisation, aliasing and wave stepping. Interpolating between waves or using
high bit depth would make it sound like any modern wavetable synth.

## 3. Content plan

### 3.1 v0.1 (16 tables)
| # | Name | Construction |
|---|------|--------------|
| 0 | Basic | Sine → Tri → Saw → Square morph |
| 1 | Pulse | Pulse width 50 % → 2 % |
| 2 | Resonant | Saw through swept resonant peak (additive) |
| 3 | Resonant 2 | Square with swept peak |
| 4 | Sync | Hard-sync sweep (ratio 1 → 8) |
| 5 | Formant A | Vowel formants a→e→i→o→u |
| 6 | Formant B | Male/female choir formants |
| 7 | Harmonic | Additive, adds partials 1→32 |
| 8 | Odd | Odd harmonics only, growing |
| 9 | Bell | Phase-modulation sweep over integer ratios 1, 2, 3, 4, 5, 7 with rising index |
| 10 | Organ | Drawbar registrations |
| 11 | Digital | Bit-crushed / sample-and-held shapes |
| 12 | Noise | Random 8-bit waves, increasingly noisy (seeded) |
| 13 | PWM Saw | Saw plus a phase-shifted saw, shift 0 → ½ (ends as octave saw) |
| 14 | Fold | Wavefolded sine, fold amount ↑ |
| 15 | Steps | Staircase quantised waves, 2 → 32 steps |

### 3.2 v0.2 (tables 16–31)
| # | Name | Construction |
|---|------|--------------|
| 16 | Strings | Additive spectra of a bowed-string model, bright → dark |
| 17 | Brass | Additive brass-like spectra, growing upper partials |
| 18 | Bass Spectra | Low-partial-heavy shapes for basses |
| 19 | Choir | Formant pairs of sung vowels, smoother than Formant A/B |
| 20 | Glass | Sparse high partials (2, 5, 9, 14…), crystalline |
| 21 | Chaos | Logistic-map waveforms, r = 3.5 → 4.0 |
| 22 | Comb | Spectral comb, notch spacing sweep |
| 23 | PhaseDist | Phase-distortion (CZ-style) sine → resonant shapes |
| 24 | Fractal | Self-similar (Weierstrass-style) waves, detail ↑ |
| 25 | Upper | Band-pass partial cluster sweeping upward |
| 26 | Clav | Pulse + comb, clavinet-like |
| 27 | Breath | Seeded noise shaped by formants, airy |
| 28 | SqSync | Hard-sync sweep on a square master |
| 29 | Metal | Inharmonic partial sets, folded to 128 samples |
| 30 | Stack | Pre-mixed detuned-saw snapshots (supersaw-like) |
| 31 | Granite | Random 8-bit fragments with 1-bit stepping, gritty |

### 3.3 v0.3 (tables 32–47) — from AKWF-FREE (CC0)

Built from [AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE)
(Adventure Kid Waveforms, Kristoffer Ekstrand, **CC0-1.0**), pinned to commit
`8de90bf9`. Each table takes 8 single-cycle waves from one AKWF family, sorted
dark → bright by spectral centroid, resampled to 128 samples (harmonics 1–63),
phase-aligned on the fundamental, and spread across slots 0–60 with linear
in-betweens.

| # | Name | AKWF family |
|---|------|-------------|
| 32 | Cello | `AKWF_cello` |
| 33 | Violin | `AKWF_violin` |
| 34 | Voice | `AKWF_hvoice` |
| 35 | EPiano | `AKWF_epiano` |
| 36 | Clavinet | `AKWF_clavinet` |
| 37 | E-Organ | `AKWF_eorgan` |
| 38 | FM | `AKWF_fmsynth` |
| 39 | Chip | `AKWF_oscchip` |
| 40 | VGame | `AKWF_vgame` |
| 41 | Sax | `AKWF_altosax` |
| 42 | Flute | `AKWF_flute` |
| 43 | Overtone | `AKWF_overtone` |
| 44 | Crushed | `AKWF_bitreduced` |
| 45 | Granular | `AKWF_granular` |
| 46 | Upright | `AKWF_dbass` |
| 47 | Piano | `AKWF_piano` |

CC0 needs no attribution, but we credit AKWF in `THIRD_PARTY.md` of the release
and in `manifest.json` (folder, commit, chosen keyframe files per table).

### 3.4 v0.4 (tables 48–57) — expansion (procedural, MIT)

Fills gaps in 0–47 (even spectra, single-partial scans, operator feedback,
woodwinds, plucked strings, baked filter sweep). Rationale and the presets that
use them: [14](14-expansion-bank.md).

| # | Name | Construction |
|---|------|--------------|
| 48 | Even | Fundamental + even harmonics only, adding partials 2 → 62 (1/k^0.7) |
| 49 | Partials | Fundamental (0.6) + one sine partial scanning 1 → 32, crossfaded between neighbours |
| 50 | Feedback | Sine operator with self-feedback (average of last two outputs), β 0 → 1.8 |
| 51 | Window | Windowed resonance: `(1−p)·(1−cos 2πrp)/2`, ratio r 1 → 16 |
| 52 | Rave | 0.7 × saw + two octave saws with phase offset 0.04 → 0.46 (hoover family) |
| 53 | Octaves | Saw registrations at 1×/2×/4×/8×, 16′ → 8′+4′+2′ (last waves lose the fundamental) |
| 54 | Logic | `i XOR (i·r mod 128)`, r 2 → 16 with crossfades; naive (not band-limited) |
| 55 | Reed | Odd-dominant clarinet → full-spectrum oboe with a formant peak at harmonic 4 → 12 |
| 56 | Pluck | Ideal plucked string `sin(πkp)/k^1.6`, pluck position p 0.5 → 0.04 |
| 57 | Sweep | Saw through a baked 24 dB/oct low-pass, cutoff harmonic 1 → 60 (exponential) with a small peak |

### 3.5 v0.5 (tables 58–73) — second expansion

Fills further gaps (single-oscillator chords, pulse-train vowels, physical
snapshots, tuned bars, polysynth mixer registrations, speaker-friendly subs) and
adds six unused AKWF-FREE families (CC0, same commit and method as §3.3). Table
layouts and the presets that use them: [17](17-expansion-bank-2.md).

| # | Name | Construction |
|---|------|--------------|
| 58 | Chord | Root body (harmonics 1, 2) + chord tones as short saws (k, 2k, 3k): power, major, maj7, sus2, minor, m7, m9 keyframes, seeded phases |
| 59 | Vosim | VOSIM: up to 8 sin² pulses decaying ×0.8, then silence; pulse width puts the formant at harmonic 2 → 24 (exponential) |
| 60 | Scanned | Closed ring of 128 masses with seeded uneven springs, struck once; 61 snapshots, 5 steps apart |
| 61 | Prime | Fundamental + prime harmonics only (2, 3, 5 … 61) added one by one, 1/k^0.6 |
| 62 | SineSync | 0.65 × sine slave hard-synced at ratio 1 → 8 + 0.35 × master sine (keeps the pitch at whole ratios) |
| 63 | Mallet | Tuned-bar spectra rounded to harmonics: soft marimba, marimba, vibes, kalimba, xylophone, steel pan, glockenspiel |
| 64 | Mixer | Polysynth mixer registrations (saw, pulse 25/12 %, square, triangle) at 2× over a sub square at 1× |
| 65 | Sub | Sine + harmonics 2–8 fading in (psychoacoustic sub for small speakers) |
| 66 | Notch | Saw with one notch sweeping harmonic 2 → 60 (exponential), widening as it rises |
| 67 | Drive | Sine through tanh, gain 1 → 40 with a slight bias (some even harmonics) |
| 68 | E-Bass | `AKWF_ebass` |
| 69 | E-Guitar | `AKWF_eguitar` |
| 70 | Distorted | `AKWF_distorted` |
| 71 | Theremin | `AKWF_theremin` |
| 72 | Raw | `AKWF_raw` (captured analog oscillator cycles) |
| 73 | Drawn | `AKWF_hdrawn` (hand-drawn cycles) |

Tables 74 and up stay reserved for sound-designer keyframe sets (the table
enum allows up to 128).

### 3.6 Generation pipeline (implemented)

```
tools/wavegen/
  wtlib.py            # DSP helpers, WAV/PNG I/O (Python stdlib only, no numpy)
  recipes.py          # procedural 0–31, AKWF 32–47, expansion 48–57, v0.5 58–67 + AKWF 68–73
  wavegen.py          # builds every table and all outputs
  fetch_akwf.sh       # sparse-clones AKWF-FREE at the pinned commit into .cache/
  out/
    amino_wavetables.bin   # 74 × 64 × 128 int8 = 592 KB, table-major
    manifest.json          # per table: name, source, recipe/keyframes, licence, sha256
    wav/NN_name.wav        # 64 frames × 128 samples, 16-bit, for listening
    preview/NN_name.png    # 8 × 8 grid of all 64 waves
    index.png              # all tables, waves 0/10/20/30/40/50/60
```

```sh
tools/wavegen/fetch_akwf.sh          # once
python3 tools/wavegen/wavegen.py     # ~10 s; --no-akwf for procedural only (expansion indices then shift)
```

Next step (with the DSP work): emit `wavetables.c` and `wavetables.mjs` from
`amino_wavetables.bin` + `manifest.json`.

Pipeline rules:
- Every wave DC-removed and peak-normalised to the full 8-bit range.
- Waves are band-limited to 63 harmonics (the most a 128-sample cycle holds),
  except the deliberately stepped/crushed tables (Digital, Steps, Granite, Logic) and
  the classic waves 61–63, which are naive like the original hardware.
- Deterministic (seeded RNG); output committed so builds need no Python on CI.
- `index.png` and per-table previews are reviewed by a sound designer, and every
  table is auditioned (WAV) before merge.

## 4. User wavetables (v0.4, optional)

- Source: WAV files in `/data/UserData/UserLibrary/Wavetables/Amino/` — single-cycle
  frames of 2048 samples (Serum convention) or 128 samples.
- Conversion runs on a **demoted worker thread** (SCHED_OTHER, cores 0–2): resample
  each frame to 128 samples, pick/interpolate to 61 slots, quantise to 8 bit,
  append classic waves 61–63, publish by atomic pointer swap.
- Max 16 user tables per instance; stored in preallocated slots.
- `state` stores the user table **file path**, and the audio thread falls back to
  table 0 until loading finishes.
