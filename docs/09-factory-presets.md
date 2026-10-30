# 09 — Factory Presets: Nostalgia Bank (0–63)

> Authoring source: the numbers below are doc numbers (`doc_no`), not positions in the shipped factory bank, which is one bank sorted by category ([06](06-presets-and-state.md)).

The Modern bank (64–127) is in [10](10-factory-presets-modern.md).

## 1. Sources & approach

The bank is **original**. No preset is copied, transcribed or renamed from Waldorf
material. We derive *archetypes* — kinds of sound the Microwave family is known
for — from public descriptions, and voice them with Amino's own wavetables (07).

Public sources used for archetypes:
- Microwave 1 architecture as publicly described (Wikipedia "Waldorf Microwave",
  Waldorf product pages, magazine reviews): 2 oscillators sharing a 64-wave table
  with classic waves at the top, wave envelope sweeps, analog filter after a gritty
  8-bit oscillator.
- Waldorf Protein product page and Synth Anatomy review (Feb 2026): layers,
  round-robin, dirt types, flavour, drive, Iridium-style FX.
- General PPG/Microwave-era sound vocabulary: envelope-swept wavetables, hard-sync
  leads, formant/vocal tables, glassy digital bells, stepped LFO scans, aliasing
  as a feature.

Archetype → technique mapping used throughout the bank:

| Archetype | Technique in Amino |
|-----------|--------------------|
| Wave envelope sweep | E1 + `wenv` on a harmonic-rich table |
| Stepped scan | Slow LFO / Saw LFO on `wave`; stepping is audible by design |
| Shared-table oscillators | Osc 2 `link` On + detune or semitone offset |
| Digital bell / glass | Bell / Glass / Metal tables, short E3, no filter env |
| Sync lead | Sync / SqSync tables swept by E1 or aftertouch |
| Vocal | Formant A/B, Choir with AT → Wave |
| Grit | Dirt (Static/Crackle/Click), drive, high notes left to alias |
| Movement between notes | Flavour, round-robin layers, Random pan |

## 2. Notation

Values are knob values as defined in 04; anything not listed = **Init** (§3).

| Token | Meaning |
|-------|---------|
| `O1 Resonant@12` | Osc 1 table *Resonant*, wave position 12 |
| `s−12`, `f+7` | semitones, cents |
| `L.5` | osc level 0.5 (Osc 2 is 0 in Init, so listing Osc 2 implies a level) |
| `we+.8`, `v+.3` | wave-env amount, velocity → wave |
| `link` | Osc 2 linked to Osc 1's table and wave modulation |
| `XM PM .4` | cross mod Osc 2 → Osc 1: type (PM, AM, Sync) and amount |
| `LP24 c64 r.3 e+.4` | filter type, cutoff (0–127), resonance, env amount |
| `kbd .8`, `fv .4` | filter keytrack, filter velocity |
| `E1 0/.4/0/.3` | envelope A/D/S/R knob values (time curve in 03 §8) |
| `Trans .4 pre` | drive type, amount, position |
| `Click .3` | dirt type and level |
| `LFO1 Sine .35` | LFO shape, free rate (03 §9). `sync 1/4` = synced |
| `M: LFO1→O1Wave +.3` | mod-matrix slot (filled from M1 upward) |
| `AT→Cut +.3` | Play-page expression route |
| `FX1 Reverb .35 (Size .7, Decay .6)` | FX slot, type, mix, named p-params |
| `Mono`, `Leg`, `gl .1` | voice mode, glide |
| `Flav .3` | layer flavour |
| `ARP` | voiced for use with Schwung's `arp` MIDI FX |

Basic table layout (07): positions 0 Sine → 20 Tri → 40 Saw → 60 Square;
61/62/63 = classic Tri/Square/Saw in every table.

## 3. Init (preset 0)

| Area | Value |
|------|-------|
| Layers | 1, mode Stack |
| Osc 1 | Basic@63 (saw), L1 |
| Osc 2 | Basic@63, L0 |
| Filter | LP24 c127 r0 e0, kbd .5, fv .2 |
| E1 / E2 / E3 | 0/.5/0/.3 · 0/.4/.2/.3 · 0/.3/1/.25 |
| LFO | LFO1 Tri .4, LFO2 Tri .4, no matrix amounts |
| Play | all expression routes 0, amp vel .5 |
| FX | FX1 Off, FX2 Off; route FX1 |
| Global | master .8, flavour .15, voices 8 |

Everything below is a delta from this.

## 4. Bank overview

| Range | Category | Count |
|-------|----------|-------|
| 0 | IN Init | 1 |
| 1–9 | BS Bass | 9 |
| 10–18 | LD Lead | 9 |
| 19–28 | PD Pad | 10 |
| 29–35 | KY Keys | 7 |
| 36–43 | PL Pluck | 8 |
| 44–49 | FX Effects | 6 |
| 50–54 | DR Drones | 5 |
| 55–63 | ML Multi-layer | 9 |

**v0.1 subset (20, tables 0–15, single layer, FX off):**
0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 13, 14, 15, 17, 27, 29, 33, 36, 39.
In v0.1 these ship with their FX lines removed; v0.2 restores the FX.

## 5. Bass (1–9)

All bass presets: `Mono`, E3 sustain high unless noted, AT routes kept subtle so
pad pressure adds bite without pitch wobble.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 1 | BS Stepped Sub | Round sub with saw edge | O1 Basic@0 · O2 Basic@63 s−12 L.35 | LP24 c60 r.15 e+.35 | E2 0/.25/0/.2 · E3 0/.3/1/.15 | gl .1 · `fv .3` | — |
| 2 | BS Resonant Crunch | Acid-ish resonant table bite | O1 Resonant@10 we+.5 · O2 link s−12 L.6 | LP24 c55 r.45 e+.5 · Trans .4 pre | E1 0/.3/0/.2 · E2 0/.28/.1/.2 · E3 0/.35/.8/.15 | Leg · AT→Cut +.3 | — |
| 3 | BS Sync Growl | Swept sync with tube grit | O1 Sync@0 we+.7 · O2 Basic@63 s−12 L.5 | LP12 c70 r.3 e+.3 · Tube .5 post | E1 0/.35/.2/.25 | LFO1 Tri .55 · M: LFO1→O1Wave +.15 · AT→Wave +.4 | FX1 Drive .3 (Gain .4, Tone .6) |
| 4 | BS Digital Pluck | Short stepped 8-bit pluck bass | O1 Digital@30 v+.3 · O2 Steps@8 L.4 | LP24 c50 r.2 e+.6 · Click .3 | E2 0/.2/0/.15 · E3 0/.25/0/.15 | — | FX1 Comp 1.0 (Thresh .4, Ratio .5) |
| 5 | BS Formant Wah | Vowel-sweeping bass | O1 FormantA@0 we+.8 | LP12 c64 r.35 | E1 .1/.4/.3/.3 | AT→Wave +.5 | FX1 Chorus .2 (Rate .3, Depth .4) |
| 6 | BS PWM Rubber | Animated pulse width | O1 Pulse@0 · O2 Pulse@20 f+7 L.7 | LP24 c58 r.25 e+.3 | E2 0/.3/.2/.2 · E3 .02/.3/.9/.2 | LFO1 Tri .45 · M: LFO1→O1Wave +.35 | — |
| 7 | BS Fold Thump | Velocity-folded punch | O1 Fold@0 v+.4 we+.5 | LP24 c64 r.1 · Diode .3 pre · Click .4 | E1 0/.22/0/.2 · E3 0/.3/0/.2 | `fv .5` | — |
| 8 | BS Ring Fifth | Metallic ring-mod bass | O1 Basic@0 · O2 Basic@0 s+7 L1 | LP24 c60 r.3 e+.4 · Ring .5 | E2 0/.3/.1/.2 | AT→Cut +.25 | — |
| 9 | BS Gritty Reese | Detuned saw stack, slow movement | O1 Stack@20 · O2 Stack@20 f+12 L1 | LP24 c52 r.2 · Tube .35 pre · Static .08 | E3 .02/.3/1/.2 | Leg gl .15 · LFO2 Rand .3 · M: LFO2→Cutoff +.1 | FX1 Chorus .15 |

## 6. Lead (10–18)

All leads: `Mono` / `Leg` with short glide, AT → Vibrato on most (pressure =
vibrato on pads).

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 10 | LD Classic Sync | The swept sync lead | O1 Sync@0 we+.6 · O2 Basic@63 f+5 L.5 | LP24 c90 r.2 e+.2 | E1 0/.5/.3/.3 | Leg gl .12 · LFO1 Sine .7 MW On · AT→Vib .4 | FX1 Delay .2 (Time 1/8d via Sync, Fdbk .35) |
| 11 | LD Glass Whistle | Pure glassy top line | O1 Glass@40 · O2 Basic@0 s+12 L.4 | LP12 c110 | E3 .1/.3/.8/.35 | AT→Vib .5 | FX1 Reverb .25 (Size .6, Decay .5) |
| 12 | LD Vowel Talker | Pressure-driven vowels | O1 FormantA@0 · O2 link f+6 L.6 | LP12 c95 r.2 | E3 .02/.3/1/.3 | AT→Wave +.8 | FX1 Delay .2 |
| 13 | LD Square 8-Bit | Chip-style square lead | O1 Basic@62 · O2 Steps@4 s+12 L.5 | LP24 c100 r.1 · Static .05 | — | Leg gl .08 · AT→Cut +.3 | — |
| 14 | LD Resonant Scream | Driven resonant table | O1 Resonant2@30 we+.4 | LP24 c80 r.55 e+.3 · Trans .6 pre | E1 .05/.5/.4/.3 | AT→Cut +.4 | FX1 Delay .25 |
| 15 | LD Harmonic Rise | Partials bloom while held | O1 Harmonic@0 we+1.0 · O2 link f+6 L.7 | LP24 c100 | E1 .45/.6/1/.4 | AT→Vib .3 | FX1 Reverb .2 |
| 16 | LD Bell Lead | Bell attack, sustained body | O1 Basic@0 · O2 Basic@0 s+22 f−31 L0 · XM PM .15 | LP12 c105 | E1 0/.45/0/.35 · E3 0/.45/.4/.35 | M: Wave Env→XMod +.45 · M: Velocity→XMod +.15 · AT→Vib .3 | FX1 Chorus .2 |
| 17 | LD Odd Hollow | Clarinet-ish odd harmonics | O1 Odd@15 | LP24 c85 r.3 | — | Mono gl .1 · LFO1 Tri .5 · M: LFO1→O1Wave +.2 | — |
| 18 | LD Chaos Solo | Unstable, crackling solo | O1 Chaos@10 v+.4 | LP24 c90 r.35 · Tube .3 pre · Crackle .15 | — | AT→Wave +.5 | FX1 Delay .3 (PingP .8) |

## 7. Pad (19–28)

All pads: Poly, E3 attack ≥ .35, Pan Spread or Random, Flavour ≥ .2.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 19 | PD Wave Drift | Two slow independent scans | O1 Resonant@0 · O2 Resonant@30 f+8 L.8 | LP24 c85 r.15 | E3 .4/.5/.9/.55 | LFO1 Sine .35 · LFO2 Tri .3 · M: LFO1→O1Wave +.6 · M: LFO2→O2Wave +.5 · Pan Spread .6 · Flav .3 | FX2 Reverb .4 (Size .8, Decay .7) · route FX2 |
| 20 | PD Formant Choir | Sung vowels, slow open | O1 Choir@10 we+.4 · O2 FormantB@20 f−7 L.7 | LP12 c92 | E1 .5/.6/.6/.5 · E3 .45/.5/1/.55 | LFO1 Sine .6 · M: LFO1→Pitch +.02 · AT→Wave +.3 | FX2 Reverb .35 |
| 21 | PD Glass Spectrum | Glittering high spectrum | O1 Glass@0 we+.9 · O2 Harmonic@40 L.5 | HP12 c40 | E1 .6/.7/.5/.6 · E3 .5/.5/.9/.6 | Pan Random .5 | FX2 Reverb .45 |
| 22 | PD Digital Strings | 8-bit ensemble strings | O1 Strings@20 · O2 Strings@24 f+10 L1 | LP24 c80 r.1 e+.2 | E2 .4/.5/.6/.5 · E3 .35/.5/.9/.5 | LFO1 Tri .55 · M: LFO1→Pitch +.015 | FX1 Chorus .35 (Depth .6, Spread .8) |
| 23 | PD Steps Of Time | Tempo-locked stepped scan | O1 Steps@0 · O2 Basic@0 s−12 L.4 | LP24 c75 r.25 | E3 .35/.5/.9/.5 | LFO1 SawUp sync 1 bar · M: LFO1→O1Wave +1.0 | FX2 Reverb .3 |
| 24 | PD Breathing Comb | Hollow, swaying comb | O1 Comb@10 | BP12 c80 r.3 · Static .1 | E3 .45/.5/.9/.6 | LFO1 Sine .3 · M: LFO1→O1Wave +.5 | FX2 Reverb .5 |
| 25 | PD Fractal Snow | Detailed, crackly shimmer | O1 Fractal@0 we+.7 | LP24 c95 · Crackle .2 | E1 .7/.7/.8/.6 · E3 .5/.5/1/.6 | Flav .4 | FX2 Reverb .5 (Mod .6) |
| 26 | PD Brass Swell | Swelling synth brass | O1 Brass@0 we+.5 · O2 link f+6 L1 | LP24 c70 r.1 e+.4 | E1 .35/.4/.6/.4 · E2 .35/.5/.6/.4 · E3 .35/.4/.9/.4 | AT→Cut +.4 | FX1 Chorus .2 |
| 27 | PD Eighties Sweep | Big envelope-swept digital pad | O1 Resonant@0 we+1.0 · O2 Resonant2@0 we+.8 f+6 L1 | LP24 c80 r.25 | E1 .7/.8/.4/.6 · E3 .45/.5/1/.6 | Pan Spread .5 | FX2 Reverb .4 |
| 28 | PD Organ Air | Breathy organ wash | O1 Organ@20 · O2 Breath@10 L.3 | LP12 c100 | E3 .2/.3/1/.4 | — | FX1 Phaser .3 (Rate .3, Depth .5) |

## 8. Keys (29–35)

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 29 | KY Digital EP | Bell-tine electric piano | O1 Bell@5 v+.4 we+.3 · O2 Basic@0 L.6 · XM PM .15 | LP24 c90 fv .4 | E1 0/.45/0/.35 · E3 0/.55/.3/.35 | Pan Alt .3 · M: Wave Env→XMod +.35 · M: Velocity→XMod +.2 | FX1 Chorus .25 |
| 30 | KY Clav Grit | Funky clav with click | O1 Clav@10 v+.5 | HP12 c30 · Trans .3 pre · Click .3 | E3 0/.3/0/.15 | — | FX1 Phaser .3 |
| 31 | KY Glass Mallet | Glass marimba | O1 Glass@30 · O2 Bell@40 s+12 L.4 | — | E3 0/.45/0/.4 | — | FX2 Reverb .3 |
| 32 | KY Organ Perc | Organ with percussive 2nd | O1 Organ@0 · O2 Basic@0 s+19 L0 | — | E1 0/.25/0/.2 | M: Wave Env→O2Level +1.0 | FX1 Chorus .3 (Rate .6) |
| 33 | KY Harp Steps | Plucked harp, wave closes | O1 Steps@10 v+.4 we−.4 | LP24 c95 e+.3 | E1 0/.4/0/.4 · E2 0/.4/0/.4 · E3 0/.5/0/.45 | ARP | FX2 Reverb .35 |
| 34 | KY Toy Piano | Little digital toy keys | O1 Digital@50 · O2 Bell@10 s+12 L.6 | Click .2 | E3 0/.4/0/.3 | Flav .3 | FX2 Reverb .2 |
| 35 | KY Wurly Bits | Driven reed-ish keys | O1 Odd@10 v+.5 | LP24 c85 fv .5 · Tube .45 pre | E3 0/.45/.4/.3 | — | FX1 Tremolo .4 (Rate .55, Depth .5, Stereo 60 %) → FX2 Reverb .15 |

## 9. Pluck (36–43)

All plucks are `ARP`: short, tonal, tidy tails, work at 1/16 with Schwung `arp`.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 36 | PL Wave Pluck | Table closes like a filter | O1 Resonant@40 we−.7 | LP24 c80 e+.4 | E1 0/.25/0/.2 · E2 0/.22/0/.2 · E3 0/.3/0/.25 | — | FX1 Delay .25 (sync 1/8) |
| 37 | PL Sync Pluck | Sync zap on every note | O1 Sync@40 we−.6 | LP24 c95 | E1 0/.2/0/.2 · E3 0/.28/0/.25 | — | FX1 Delay .2 |
| 38 | PL Kalimba Byte | Thumb-piano bell | O1 Bell@0 v+.3 | HP12 c45 | E3 0/.32/0/.3 | — | FX2 Reverb .25 |
| 39 | PL Formant Pop | Vocal "pop" pluck | O1 FormantA@40 we−.8 | LP12 c90 | E1 0/.18/0/.15 · E3 0/.25/0/.25 | — | FX1 Delay .3 |
| 40 | PL Metal Tick | Inharmonic tick | O1 Metal@20 | HP24 c50 · Geiger .2 | E3 0/.2/0/.2 | — | FX2 Reverb .25 |
| 41 | PL Fold Drop | Fold collapsing to sine | O1 Fold@50 we−1.0 | Diode .2 post | E1 0/.25/0/.2 · E3 0/.3/0/.25 | — | FX1 Delay .3 (PingP 1.0) |
| 42 | PL Chaos Grain | Every note different | O1 Chaos@30 v+.6 | LP24 c100 | E3 0/.2/0/.25 | Flav .6 · M: Random→O1Wave +.3 | FX2 Reverb .3 |
| 43 | PL Steel Ping | Harmonic + metal partial | O1 Harmonic@50 · O2 Metal@10 s+24 L.3 | — | E3 0/.3/0/.3 | — | FX1 Delay .3 |

## 10. Effects (44–49)

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 44 | FX Geiger Field | Radioactive texture | O1 Noise@20 L.2 | BP12 c80 r.5 · Geiger .8 | E3 .3/.5/1/.7 | LFO1 Rand .4 · M: LFO1→Cutoff +.3 | FX2 Reverb .6 |
| 45 | FX Burst Radio | Shortwave bursts | O1 Digital@10 | BP12 c85 r.4 · Burst .7 | — | LFO1 S&H .7 · M: LFO1→O1Wave +.8 | FX1 Delay .4 |
| 46 | FX Laser Drop | Pitch-dropping zap | O1 Sync@0 we+1.0 | LP24 c110 | E1 0/.4/0/.3 | M: Wave Env→Pitch +1.0 | FX1 Delay .35 |
| 47 | FX Byte Riser | Slow stepped rise | O1 Steps@0 we+1.0 | LP24 c60 e+.6 | E1 .7/.5/1/.5 · E2 .7/.5/1/.5 | M: Wave Env→Pitch +.5 | FX2 Reverb .5 |
| 48 | FX Robot Voice | Random vowel talk | O1 FormantB@0 | Ring .4 | — | LFO1 S&H .65 · M: LFO1→O1Wave +.7 | FX1 Delay .2 |
| 49 | FX Ring Bells | Clangorous ring-mod | O1 Bell@30 · O2 Metal@10 s+5 L1 | Ring .7 | E3 0/.55/0/.5 | — | FX2 Reverb .45 |

## 11. Drones (50–54)

All drones: E3 attack ≥ .6, long release, keep sounding while held; good on a
single held pad.

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 50 | DR Slow Tide | Two chaotic scans | O1 Chaos@0 · O2 Fractal@0 L.8 | LP24 c70 r.3 · Static .05 | E3 .6/.5/1/.7 | LFO1 Sine .2 · LFO2 Tri .25 · M: LFO1→O1Wave +.8 · M: LFO2→O2Wave +.7 · M: LFO1→Cutoff +.2 | FX2 Reverb .6 |
| 51 | DR Comb Cathedral | Organ through comb | O1 Comb@20 · O2 Organ@10 s−12 L.7 | LP12 c75 | E3 .65/.5/1/.75 | — | FX2 Reverb .7 (Size 1.0, Decay .9) |
| 52 | DR Steps Ritual | Synced stepped pulse | O1 Steps@0 | LP24 c70 r.4 | E3 .6/.5/1/.6 | LFO1 S&H sync 1/8 · M: LFO1→O1Wave +.6 | FX1 Delay .4 (1/8d, Ping-Pong .6) → FX2 Reverb .3 |
| 53 | DR Fractal Engine | Folded machine hum | O1 Fractal@20 · O2 Fractal@40 s−12 L.6 | LP24 c80 · Fold .4 pre | E3 .6/.5/1/.6 | LFO1 Rand .3 · M: LFO1→Cutoff +.3 | FX2 Reverb .5 |
| 54 | DR Breath Ghost | Airy, wandering pan | O1 Breath@30 | HP12 c45 · Crackle .1 | E3 .7/.5/1/.8 | Pan LFO1 · LFO1 Sine .3 | FX2 Reverb .7 |

## 12. Multi-layer (55–63)

Layer lines use the same notation; only layer-specific settings are listed.
Global FX are shared by all layers.

### 55 ML Round Robin Keys — Round Robin, 3 layers, `ARP`
Each repeated note is a slightly different EP, like a real instrument.
| L | Sound | Pan |
|---|-------|-----|
| 1 | Preset 29 voicing | −.2 |
| 2 | Same, O1 Bell@12, E3 d .5 | +.2 |
| 3 | O1 Glass@30, O2 Basic@0 L.6, E3 0/.5/.2/.35 | 0 |
FX1 Chorus .25 (all routed FX1).

### 56 ML Random Pluck Cloud — Random RR, 4 layers, `ARP`
| L | Sound | Route |
|---|-------|-------|
| 1 | Preset 36 | FX1 |
| 2 | Preset 37, `octave +1` | FX1 |
| 3 | Preset 39 | FX2 |
| 4 | Preset 40 | FX2 |
FX1 Delay .3 (sync 1/8, PingP .7) · FX2 Reverb .4.

### 57 ML Strings & Air — Stack, 2 layers
| L | Sound | Vol |
|---|-------|-----|
| 1 | Preset 22 | .8 |
| 2 | O1 Breath@20 `octave +1`, HP12 c60, E3 .5/.5/1/.6 | .3 |
FX1 Chorus .3 for L1, FX2 Reverb .4 for L2.

### 58 ML Bass / Lead Split — Split, 2 layers
| L | Range | Sound |
|---|-------|-------|
| 1 | 0–59 | Preset 3 (Mono) |
| 2 | 60–127 | Preset 10 (Mono, Leg) |
FX1 Delay .2 routed for L2 only; L1 route Dry.

### 59 ML Octave Organ — Stack, 2 layers
| L | Sound |
|---|-------|
| 1 | Preset 28 without FX |
| 2 | O1 Odd@10 `octave +1` L.5, E3 0/.3/1/.3 |
FX1 Phaser .35 (both).

### 60 ML Unison Monster — Stack, 4 layers, all `Mono` + `Leg`, voices 8
Fat mono lead: one note uses 4 voices.
| L | Detune (f) | Pan |
|---|------------|-----|
| 1 | −12 | −.8 |
| 2 | −4 | −.3 |
| 3 | +4 | +.3 |
| 4 | +12 | +.8 |
All layers: O1 Basic@63, O2 Stack@10 L.5, LP24 c85 r.2 e+.3, gl .1, Tube .3 pre.
FX1 Delay .2.

### 61 ML Fifth Stab — Stack, 2 layers
| L | Sound |
|---|-------|
| 1 | O1 Resonant@25 we−.4, LP24 c90 e+.4, E1/E2/E3 0/.25/0/.2 |
| 2 | Same with O1 s+7 |
FX2 Reverb .3. (v0.3: may switch to the Chord feature.)

### 62 ML Byte Kit — Split, 4 layers (for Move drum-mode notes 36+)
| L | Range | Sound |
|---|-------|-------|
| 1 | 36–37 | Kick: O1 Basic@0, M: Wave Env→Pitch +.8, E1 0/.15/0/.1, E3 0/.25/0/.2 |
| 2 | 38–41 | Snare: O1 Basic@20 L.4, Static .8, BP12 c85, E3 0/.2/0/.2 |
| 3 | 42–47 | Hat: Static 1.0, O1 L0, HP24 c110, E3 0/.1/0/.1 (46 longer via Key→E3 Decay +.3) |
| 4 | 48–60 | Perc: O1 Metal@20, Geiger .2, E3 0/.25/0/.2 |
FX1 Comp .8 (all). On Move: put the slot's track in drum mode.

### 63 ML Ping-Pong Pair — Round Robin, 2 layers, `ARP`
| L | Sound | Pan |
|---|-------|-----|
| 1 | Preset 38 | −.7 |
| 2 | Preset 31 | +.7 |
FX1 Delay .25 (sync 1/16, PingP .5).

## 13. Voicing rules (all presets)

1. **Main page is meaningful:** Wave 1/2, Cutoff/Res, F.Env, Attack/Release and
   Flavour must each produce an audible, useful change from the stored value.
2. **Pressure does something** (except pure drones/FX where noted): at least one
   Play-page route ≠ 0, or AT already used via the matrix.
3. **Levels:** −18 to −12 LUFS short-term on a C3 triad (pads/keys) or C2 single
   note (bass); checked by the golden-render script.
4. **No clipping** at velocity 127 with 8 voices.
5. **Pad range test:** each preset sounds useful from C1 to C6 (Move pad range
   with octave shift); aliasing allowed, harshness above C6 acceptable.
6. **Naming:** `CC Name` with a 2-letter category, ≤ 18 characters, ASCII only,
   no Waldorf/Protein/Microwave/PPG terms.

## 14. Deliverables per preset

- `presets/factory/NN_<slug>.json` — delta from Init (06 §3).
- Golden render WAV (08 §3.2) and a 1-line description for the Remote UI preset
  list and `help.json` "Presets" leaf.
