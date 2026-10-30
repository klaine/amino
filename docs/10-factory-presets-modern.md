# 10 — Factory Presets: Modern Bank (64–127)

> Authoring source: the numbers below are doc numbers (`doc_no`), not positions in the shipped factory bank, which is one bank sorted by category ([06](06-presets-and-state.md)).

The bank in [09](09-factory-presets.md) is the **Nostalgia** bank (0–63):
envelope-swept tables, aliasing, dirt, the 1980s digital palette. This **Modern**
bank (64–127) uses the same engine and categories for current production styles.

## 1. What makes a preset "Modern"

| Aspect | Nostalgia bank | Modern bank |
|--------|----------------|-------------|
| Oscillator character | Vintage (aliased) | **Clean** on most tonal presets; Vintage kept where grit is the point |
| Tables | Resonant, Sync, Formant, Bell | Stack, Choir, Glass, PhaseDist, Granite, Upper, Bass Spectra |
| Timing | Free-running LFOs | **Tempo-synced** LFOs, delays, tremolo Pump |
| Harmony | Single notes | **Chord** mode stabs; Scale remains user-controlled |
| Low end | Natural | Controlled: mono basses, Drive/EQ Low Cut, Compressor glue |
| FX | One effect per sound | **Serial chains**: Drive → Comp, Chorus → Reverb, Delay → Reverb; **Shimmer** on ambient sounds |
| Expression | AT → vibrato/cutoff | AT → wave/LFO rate; several presets MPE-ready |
| Flavour | Often high (.2–.6) | Low (≤ .1) for tight sounds; high only on textures |

Archetypes are taken from widely documented genre vocabulary (808 basses,
supersaw leads, future-bass chords, neuro/reese basses, melodic-techno plucks, dub
stabs, lo-fi keys, sidechain-pumped pads, risers/impacts). All presets are
original.

**Release:** ships with **v0.3** (needs Clean mode, Chord, Scale, MPE). FX
FX features it uses (Serial, Pump, Crush, Freeze, Shimmer) are part of the v0.2 FX engine
(11).

## 2. Notation

Same as 09 §2, plus:

| Token | Meaning |
|-------|---------|
| `Clean` | Layer `osc_mode` = Clean (default here is Vintage unless stated) |
| `Scale Set` | Scale follows Move's set root & scale |
| `Chord m7` | Layer chord mode |
| `MPE` | Global MPE on; note in description only — user enables per setup |
| `FX1 Drive .4 (Hard) → FX2 Comp 1.0` | `fx_chain` = Serial, FX1 then FX2 |
| `FX1 … · FX2 …` | Parallel |

Factory presets do not store the `scale` parameter. `Scale Set` in the sound
notes is a suggested performance setting, not a preset override.
| `sync 1/8` | LFO synced to division |

Effect parameter names are those in 11 §4.

## 3. Modern Init (preset 64)

Delta from Init (09 §3):

| Area | Value |
|------|-------|
| Osc | `Clean`, O1 Basic@63, O2 Basic@63 f+7 L.6 |
| Filter | LP24 c110 r.1 |
| Global | flavour .05 |
| FX | FX1 Comp .5 (Thresh −14, Ratio 2:1) → FX2 Reverb .2 (Size .5, Decay 1.8 s) |

## 4. Bank overview

| Range | Category | Count |
|-------|----------|-------|
| 64 | IN Init Modern | 1 |
| 65–73 | BS Bass | 9 |
| 74–82 | LD Lead | 9 |
| 83–92 | PD Pad | 10 |
| 93–99 | KY Keys | 7 |
| 100–107 | PL Pluck | 8 |
| 108–113 | FX Effects | 6 |
| 114–118 | DR Drones | 5 |
| 119–127 | ML Multi-layer | 9 |

## 5. Bass (65–73)

All: `Mono`, EQ/Drive Low Cut kept at 20 Hz so subs stay intact, Flavour 0.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 65 | BS 808 Glide | Long sub with punch and slides | `Clean` O1 Basic@0 · O2 Basic@20 L.2 | LP24 c70 | E1 0/.12/0/.1 · E3 0/.55/.3/.35 | Leg gl .2 · M: Wave Env→Pitch +.15 | FX1 Drive .4 (Tube, Gain 10 dB) |
| 66 | BS Neuro Comb | Rhythmic comb growl | O1 Comb@0 · O2 Stack@10 s−12 L.5 | LP24 c90 r.4 · Trans .6 post | E3 0/.3/1/.15 | LFO1 SawDn sync 1/8 · M: LFO1→O1Wave +.6 · AT→Wave +.4 | FX1 Drive .5 (Hard) → FX2 Comp 1.0 |
| 67 | BS Wobble Sync | Classic wobble, pressure speeds it | O1 SqSync@0 | LP24 c50 r.5 | E3 0/.3/1/.15 | LFO1 Sine sync 1/8T · M: LFO1→Cutoff +.6 · M: LFO1→O1Wave +.3 · M: Poly AT→LFO1 Rate +.5 | FX1 Drive .3 → FX2 Comp 1.0 |
| 68 | BS Reese Modern | Wide, slowly phasing reese | `Clean` O1 Stack@30 · O2 Stack@30 f+18 L1 | LP24 c60 r.2 | E3 .02/.3/1/.2 | LFO1 Tri .35 · LFO2 Rand .3 · M: LFO1→O1Wave +.2 · M: LFO2→O2Wave +.2 | FX1 Chorus .2 → FX2 Comp .8 |
| 69 | BS House Pluck | Short filtered house bass | `Clean` O1 Basic@40 · O2 Basic@60 f+5 L.6 | LP24 c45 r.35 e+.6 | E2 0/.22/0/.18 · E3 0/.3/0/.2 | `fv .4` | FX1 Comp .8 |
| 70 | BS Hollow PD | Phase-distortion hollow bass | O1 PhaseDist@20 we+.4 | LP12 c90 | E1 0/.25/.2/.2 | — | FX1 EQ 1.0 (Low +4, Low Cut 30 Hz) |
| 71 | BS Distorted Sub | Sub + crushed upper octave | O1 Basic@0 · O2 Granite@20 s+12 L.3 | LP24 c75 | E3 0/.3/1/.15 | AT→Cut +.3 | FX1 Drive .4 (Crush, Low Cut 150 Hz) → FX2 Comp 1.0 |
| 72 | BS Formant Growl | Talking synced growl | O1 Choir@0 · O2 Basic@0 L0 · XM Sync .25 | LP24 c80 r.3 · Trans .5 pre | E3 0/.3/1/.15 | LFO1 Tri sync 1/4 · M: LFO1→O1Wave +.7 · M: LFO1→XMod +.15 · AT→Wave +.3 | FX1 Drive .3 → FX2 Comp 1.0 |
| 73 | BS Pressure Bass | Press to open and morph | `Clean` O1 BassSpectra@10 | LP24 c55 r.3 | E3 0/.3/1/.2 | AT→Wave +.6 · AT→Cut +.4 · MPE-ready | FX1 Comp .7 |

## 6. Lead (74–82)

All: `Scale Set` unless noted, Leg with short glide.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 74 | LD Supersaw Lead | Bright detuned stack | `Clean` O1 Stack@40 · O2 Stack@40 f+15 L1 | LP24 c110 | E3 .01/.3/1/.25 | gl .05 · AT→Vib .3 | FX1 Chorus .3 → FX2 Reverb .25 |
| 75 | LD Trap Bell Glide | Sliding bell lead | `Clean` O1 Basic@0 · O2 Basic@0 s+22 f−31 L0 · XM PM .2 | — | E1 0/.5/0/.4 · E3 0/.5/.6/.35 | Mono gl .25 · M: Wave Env→XMod +.4 | FX1 Delay .25 (1/8d) → FX2 Reverb .2 |
| 76 | LD Hyper Square | Driven octave squares | `Clean` O1 Basic@62 · O2 Basic@62 s+12 f+8 L.6 | LP24 c115 · Hard .3 post | — | AT→Cut +.3 | FX1 Chorus .25 → FX2 Reverb .2 |
| 77 | LD Vocal Chop Lead | Stepped vowel chops | `Clean` O1 Choir@20 | LP12 c100 | E3 0/.25/.7/.2 | LFO1 S&H sync 1/16 · M: LFO1→O1Wave +.5 | FX1 Delay .3 (1/8, Ping-Pong .8) |
| 78 | LD Portamento Sine | Smooth whistling glide | `Clean` O1 Basic@0 · O2 Basic@20 s+12 L.2 | — | E3 .05/.3/1/.3 | gl .3 · AT→Vib .5 | FX1 Delay .2 → FX2 Reverb .3 |
| 79 | LD Glitch Lead | Crushed stepping lead | O1 Granite@10 | LP24 c100 | — | LFO1 S&H sync 1/32 · M: LFO1→O1Wave +.4 | FX1 Drive .4 (Crush .5) |
| 80 | LD Pressure Sync | Sync swept only by pressure | `Clean` O1 Basic@60 · O2 Basic@0 L0 · XM Sync | LP24 c105 | E3 .01/.3/1/.25 | M: Poly AT→XMod +.6 · MPE-ready | FX1 Delay .2 |
| 81 | LD Detuned Pulse | Phasey pulse duo | `Clean` O1 Pulse@10 · O2 Pulse@10 f−12 L1 | LP24 c100 | — | LFO1 Tri .5 · M: LFO1→O1Wave +.2 | FX1 Phaser .25 → FX2 Reverb .2 |
| 82 | LD Formant Glide | Singing portamento | O1 FormantB@30 we+.4 | LP12 c100 | E1 .1/.5/.5/.3 | gl .2 · AT→Wave +.6 | FX1 Chorus .2 → FX2 Reverb .25 |

## 7. Pad (83–92)

All: Poly, Pan Spread, E3 attack ≥ .3.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 83 | PD Lush Supersaw | Huge, soft-attack saw pad | `Clean` O1 Stack@50 · O2 Stack@50 f+20 L1 | LP24 c95 e+.2 | E2 .4/.6/.7/.6 · E3 .35/.5/1/.6 | Pan Spread .8 | FX1 Chorus .3 → FX2 Reverb .4 |
| 84 | PD Future Chords | Pumping minor-7 chords | `Clean` O1 Stack@30 · O2 Basic@63 f+9 L.6 · `Chord m7` · `Scale Set` | LP24 c100 | E3 .05/.5/1/.4 | — | FX1 Tremolo .8 (Pump, Sync 1/4, Depth 70 %) → FX2 Reverb .3 |
| 85 | PD Ambient Granite | Grainy, ever-changing bed | O1 Granite@0 | LP24 c80 | E3 .5/.5/1/.7 | LFO1 Rand .25 · M: LFO1→O1Wave +.6 · Flav .6 | FX2 Reverb .6 (Decay 8 s) |
| 86 | PD Glass Shimmer | Bright, chorused glass | `Clean` O1 Glass@20 · O2 Glass@40 s+12 L.5 | HP12 c50 | E3 .5/.5/1/.7 | — | FX1 Chorus .3 (Ensemble) → FX2 Shimmer .45 (Interval +12, Shimmer 50 %) |
| 87 | PD Choir MPE | Expressive choir | `Clean` O1 Choir@0 | LP12 c95 | E3 .4/.5/1/.6 | AT→Wave +.5 · AT→Cut +.3 · MPE-ready | FX2 Reverb .45 |
| 88 | PD Tape Warble | Wobbly lo-fi strings | O1 Strings@10 · O2 Organ@20 L.4 | LP24 c85 | E3 .4/.5/1/.5 | LFO2 Rand .45 · M: LFO2→Pitch +.01 · Flav .2 | FX1 Drive .15 (Crush .2, Low Cut 120 Hz) → FX2 Reverb .35 |
| 89 | PD Spectra Evolve | Two slow spectral scans | `Clean` O1 Upper@0 · O2 Harmonic@20 L.7 | LP24 c100 | E3 .5/.5/1/.6 | LFO1 Tri .25 · LFO2 Sine .3 · M: LFO1→O1Wave +.8 · M: LFO2→O2Wave +.5 · Pan LFO2 | FX2 Reverb .5 |
| 90 | PD Sidechain Pad | Straight 4/4 pumping pad | `Clean` O1 Stack@20 · O2 Stack@24 f+12 L1 | LP24 c90 | E3 .3/.5/1/.5 | — | FX1 Tremolo .9 (Pump, Sync 1/4, Depth 85 %) → FX2 Reverb .25 |
| 91 | PD Dark Cinematic | Low, slowly breathing | O1 BassSpectra@30 · O2 Comb@10 s−12 L.6 | LP24 c60 r.25 | E3 .6/.5/1/.7 | LFO1 Sine .2 · M: LFO1→Cutoff +.2 | FX2 Reverb .6 (Decay 9 s) |
| 92 | PD PhaseDist Air | Airy PD with breath | `Clean` O1 PhaseDist@0 we+.5 · O2 Breath@0 L.25 | LP24 c100 | E1 .6/.7/.5/.6 · E3 .45/.5/1/.6 | — | FX1 Phaser .3 → FX2 Reverb .4 |

## 8. Keys (93–99)

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 93 | KY Neo Soul EP | Warm tine EP, stereo trem | `Clean` O1 Bell@8 v+.4 · O2 Basic@0 L.7 · XM PM .1 | LP24 c88 fv .5 | E1 0/.5/0/.4 · E3 0/.55/.35/.35 | M: Wave Env→XMod +.3 · M: Velocity→XMod +.25 | FX1 Tremolo .3 (Stereo 80 %) → FX2 Reverb .2 |
| 94 | KY Lofi Keys | Detuned, crushed, dusty | O1 Organ@10 · O2 Bell@5 L.5 | LP24 c80 · Crackle .05 | E3 0/.5/.3/.35 | LFO2 Rand .4 · M: LFO2→Pitch +.008 · Flav .3 | FX1 Drive .25 (Crush .35, Low Cut 120 Hz) → FX2 Reverb .25 |
| 95 | KY House Organ | Minor-7 organ stabs | `Clean` O1 Organ@40 · `Chord m7` · `Scale Set` | LP24 c105 | E3 0/.3/0/.2 | — | FX1 Comp .8 → FX2 Reverb .2 |
| 96 | KY Bright Tines | Glassy digital EP | `Clean` O1 PhaseDist@10 v+.5 we−.3 | — | E1 0/.4/0/.3 · E3 0/.5/.3/.35 | — | FX1 EQ 1.0 (High +3) → FX2 Chorus .2 |
| 97 | KY Pressure Clav | Clav that opens with pressure | `Clean` O1 Clav@20 | LP24 c70 r.2 | E3 0/.3/.2/.2 | AT→Cut +.5 | FX1 Phaser .25 |
| 98 | KY Mallet Chords | One-finger major chords | `Clean` O1 Glass@10 · `Chord Maj` · `Scale Set` | — | E3 0/.4/0/.35 | — | FX1 Delay .2 (1/8d) → FX2 Reverb .25 |
| 99 | KY Felt Keys | Soft, felted piano-ish | `Clean` O1 Basic@0 · O2 Harmonic@10 L.4 v+.3 | LP24 c76 fv .6 kbd .8 | E3 0/.5/.2/.4 | — | FX1 Comp .5 → FX2 Reverb .3 |

## 9. Pluck (100–107)

All `ARP` and `Scale Set` unless noted.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 100 | PL Melodic Techno | Resonant, delayed pluck | `Clean` O1 Resonant@30 we−.5 | LP24 c70 r.4 e+.5 | E1 0/.22/0/.2 · E2 0/.2/0/.2 · E3 0/.3/0/.25 | — | FX1 Delay .3 (1/8d, Fdbk 50 %) → FX2 Reverb .3 |
| 101 | PL Future Pluck | Plucked m7 chords | `Clean` O1 Stack@30 · `Chord m7` | LP24 c90 e+.4 | E2 0/.25/0/.2 · E3 0/.3/0/.3 | — | FX1 Delay .25 (1/4) → FX2 Reverb .3 |
| 102 | PL Glass Marimba | Clean mallet | `Clean` O1 Glass@20 | — | E3 0/.35/0/.3 | — | FX2 Reverb .3 |
| 103 | PL Vocal Chop | A different vowel each note | `Clean` O1 Choir@20 | LP12 c100 | E3 0/.25/0/.2 | M: Random→O1Wave +.6 | FX1 Delay .25 (1/16) |
| 104 | PL Bit Pluck | Crushed chip pluck | O1 Granite@5 | — | E3 0/.2/0/.15 | — | FX1 Drive .3 (Crush .4) → FX2 Delay .25 (1/16) |
| 105 | PL Kalimba Wide | Alternating-pan kalimba | `Clean` O1 Bell@0 | HP12 c45 | E3 0/.32/0/.3 | Pan Alt .6 | FX2 Reverb .3 |
| 106 | PL Dub Stab | Minor chord into deep echo | O1 Basic@40 · `Chord Min` | LP24 c65 r.3 e+.4 | E2 0/.25/0/.2 · E3 0/.3/0/.3 | — | FX1 Delay .45 (1/4d, Fdbk 65 %, Tone dark) → FX2 Reverb .35 |
| 107 | PL Pressure Pluck | Re-awaken the tail with pressure | `Clean` O1 SqSync@20 | LP24 c95 | E3 0/.4/.2/.3 | AT→Wave +.6 | FX1 Delay .2 (1/8) |

## 10. Effects (108–113)

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 108 | FX Sync Riser | 4-second filter + pitch riser | `Clean` O1 Basic@40 · O2 Basic@0 L0 · XM Sync | LP24 c30 r.3 e+.9 | E2 .6/.5/1/.5 · E3 .1/.5/1/.6 | M: Filter Env→Pitch +.25 · M: Filter Env→XMod +.7 | FX2 Reverb .5 |
| 109 | FX Downlifter | Falling crushed sweep | O1 Granite@30 | LP24 c100 | E1 0/.7/0/.6 · E3 0/.7/0/.6 | M: Wave Env→Pitch −1.0 | FX2 Reverb .6 |
| 110 | FX Glitch Stutter | Gated random stutter | O1 Digital@20 | — | — | LFO1 S&H sync 1/32 · M: LFO1→O1Wave +1.0 · M: LFO1→Pan +.6 | FX1 Tremolo 1.0 (Square, Sync 1/16) → FX2 Delay .3 (1/32) |
| 111 | FX Impact | Pitch-drop boom + noise | `Clean` O1 Basic@0 | Static .6 | E1 0/.4/0/.3 · E3 0/.5/0/.6 | M: Wave Env→Pitch +1.0 | FX1 Comp 1.0 → FX2 Reverb .5 |
| 112 | FX Frozen Space | Hold-and-freeze glass | `Clean` O1 Glass@30 | HP12 c50 | E3 .3/.5/1/.5 | — | FX2 Shimmer .7 (Decay 15 s, Interval +12) — turn Decay fully up to Freeze while holding a chord |
| 113 | FX Alien Radio | Ring-mod vowel chatter | O1 FormantB@0 | Ring .5 | — | LFO1 S&H .65 · M: LFO1→O1Wave +.7 | FX1 Phaser .4 → FX2 Delay .3 |

## 11. Drones (114–118)

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 114 | DR Infinite Choir | Slow, endless choir | `Clean` O1 Choir@20 | LP12 c90 | E3 .6/.5/1/.8 | LFO1 Sine .2 · M: LFO1→O1Wave +.5 | FX2 Shimmer .5 (Decay 12 s, Interval +12, Shimmer 35 %) |
| 115 | DR Granite Field | Grainy noise landscape | O1 Granite@0 | LP24 c85 · Crackle .2 | E3 .6/.5/1/.8 | LFO1 Rand .3 · M: LFO1→Cutoff +.3 · Flav .5 | FX2 Reverb .7 |
| 116 | DR Sub Pulse | Tempo-pulsing sub drone | `Clean` O1 Basic@0 · O2 Basic@20 s+12 L.3 | LP24 c70 | E3 .6/.5/1/.6 | — | FX1 Tremolo .9 (Pump, Sync 1/4) → FX2 Reverb .3 |
| 117 | DR Spectral Organ | Organ with rising partials | O1 Upper@0 · O2 Organ@20 s−12 L.7 | LP24 c95 | E3 .6/.5/1/.7 | LFO1 Tri .15 · M: LFO1→O1Wave +.9 | FX1 Phaser .3 → FX2 Shimmer .45 (Interval +7, Shimmer 30 %) |
| 118 | DR Choir Wall | Thick detuned choir wall | `Clean` O1 Stack@20 · O2 Choir@10 f+15 L.6 | LP24 c90 | E3 .65/.5/1/.8 | Pan Spread 1.0 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .6 |

## 12. Multi-layer (119–127)

### 119 ML Supersaw Wall — Stack, 2 layers (best with `voices 16`)
| L | Sound | Pan |
|---|-------|-----|
| 1 | `Clean` Stack@40 f−8 · LP24 c100 · E3 .2/.5/1/.5 | −.5 |
| 2 | `Clean` Stack@44 f+8 · same | +.5 |
FX1 Chorus .3 → FX2 Reverb .35.

### 120 ML Vocal Chop RR — Random RR, 4 layers, `Scale Set`
| L | Sound |
|---|-------|
| 1 | `Clean` Choir@10 |
| 2 | `Clean` FormantA@30 |
| 3 | `Clean` FormantB@45 |
| 4 | Breath@20 L.8 |
All: E3 0/.25/0/.2. FX1 Delay .25 (1/8) → FX2 Reverb .25.

### 121 ML Keys/Sub Split — Split
| L | Range | Sound | Route |
|---|-------|-------|-------|
| 1 | 0–47 | Preset 65 (808 Glide) | Dry |
| 2 | 48–127 | Preset 93 (Neo Soul EP) | FX1 |
FX1 Tremolo .3 (Stereo 80 %) · FX2 Off.

### 122 ML Pluck & Pad — Stack, 2 layers, Parallel FX
| L | Sound | Vol | Route |
|---|-------|-----|-------|
| 1 | Preset 100 (Melodic Techno) | .8 | FX1 Delay .3 (1/8d) |
| 2 | Preset 83 (Lush Supersaw) | .4 | FX2 Reverb .45 |

### 123 ML Modern Kit — Split, 4 layers (Move drum mode, notes 36+)
| L | Range | Sound |
|---|-------|-------|
| 1 | 36–37 | 808 kick: `Clean` Basic@0, M: Wave Env→Pitch +.6, E1 0/.12/0/.1, E3 0/.45/0/.3 |
| 2 | 38–41 | Clap: Burst .8, O1 L0, BP12 c90 r.3, E3 0/.25/0/.25 |
| 3 | 42–47 | Hats: Static 1.0, O1 L0, HP24 c115, E3 0/.08/0/.08; 46 longer via Key→E3 Decay +.3 |
| 4 | 48–60 | Perc: `Clean` PhaseDist@20 we−.5, E1 0/.15/0/.1, E3 0/.2/0/.2 |
FX1 Comp .8 (Ratio 6:1) for all.

### 124 ML MPE Duo — Stack, 2 layers, MPE-ready
| L | Sound | Vol |
|---|-------|-----|
| 1 | `Clean` Choir@0, AT→Wave +.5 | .8 |
| 2 | `Clean` Glass@20 `octave +1`, AT→Wave +.4 | .4 |
FX2 Reverb .45.

### 125 ML Wide Ping-Pong — Round Robin, 2 layers, `ARP`, `Scale Set`
| L | Sound | Pan |
|---|-------|-----|
| 1 | Preset 102 (Glass Marimba) | −.7 |
| 2 | `Clean` Bell@8, E3 0/.3/0/.3 | +.7 |
FX1 Delay .25 (1/16, Ping-Pong .5) → FX2 Reverb .25.

### 126 ML Chord Stab Wide — Stack, 2 layers, `Scale Set`
| L | Sound | Vol |
|---|-------|-----|
| 1 | `Clean` Organ@40 `Chord m7`, E3 0/.3/0/.25 | .8 |
| 2 | `Clean` Stack@30 `Chord m7` `octave +1`, Pan Spread .8 | .5 |
FX1 Comp .7 → FX2 Reverb .3.

### 127 ML Reese & Sub — Stack, 2 layers, both `Mono`
| L | Sound | Route |
|---|-------|-------|
| 1 | `Clean` Basic@0 `octave −1`, LP24 c50 | Dry |
| 2 | Preset 68 (Reese Modern) with LP24 → HP12 c40 | FX1 Chorus .2 |
Keeps the sub mono and centred while the reese stays wide.

## 13. Voicing rules (Modern bank additions to 09 §13)

1. Same loudness target as the Nostalgia bank, so switching banks is level-matched.
2. Bass presets: mono, sub below 120 Hz within ±3 dB of centre (no stereo low end).
3. Synced elements must sound correct at 90–140 BPM.
4. Chord presets must voice inside C2–C5 without masking a bass layer.
5. Every preset is usable with Move's sequencer at 120 BPM without further edits.
