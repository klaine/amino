# 17 — Expansion Bank II (260–323) and Wavetables 58–73

64 presets that show off the sixteen v0.5 wavetables
([07](07-wavetables-and-ip.md) §3.5). Same notation and voicing rules as
[09](09-factory-presets.md) §2 and [10](10-factory-presets-modern.md) §2. Every
preset uses one of tables 58–73 on Osc 1, and every new table leads exactly four
presets, each in a different role where the table allows it.

The bank ships with **v0.5** (needs wavetables 58–73). In the browser it follows
Trance Gate Showcase.

## 1. Why these sixteen tables

| # | Name | Gap it fills | Typical use |
|---|------|--------------|-------------|
| 58 | Chord | No single-oscillator chords; chord mode needs voices | One-finger dub/house stabs, chord pads |
| 59 | Vosim | Vowels from pulse trains, not additive formants (Formant A/B, Choir) | Talking basses, sung leads, chatter FX |
| 60 | Scanned | Snapshots of a struck spring ring: organic motion no formula table has | Evolving pads, struck keys, drones |
| 61 | Prime | Odd/Even exist; prime-only harmonics give a glassy, slightly hollow tone | Bells, whistles, spectral drones |
| 62 | SineSync | Sync and SqSync are harsh; a sine slave sweeps softly | Soft sync leads, sync plucks, sirens |
| 63 | Mallet | Mallet presets borrowed Glass/Bell; these are tuned-bar spectra | Marimba, vibes, kalimba, steel pan, glock |
| 64 | Mixer | Classic polysynth mixer registrations with a sub square | Poly basses, brass stabs, string pads |
| 65 | Sub | Pure sine vanishes on small speakers; added low harmonics carry it | Subs and 808s on the Move speaker, sub drones |
| 66 | Notch | Comb has many notches; one moving notch reads as a frozen phaser | Phasing pads, jet FX |
| 67 | Drive | Saturation at the source without FX Drive | Warm basses, overdriven leads, organ grit |
| 68 | E-Bass | Only Upright / Bass Spectra for bass instruments | Finger, pick and muted bass |
| 69 | E-Guitar | No guitar waves | Guitar solos, funk chops, power chords, e-bow pads |
| 70 | Distorted | Sampled clipped waves, rougher than Drive | Grind basses, fuzz leads, impacts |
| 71 | Theremin | Near-sine but alive; softer than Flute | Theremin leads, ghostly pads, sci-fi FX |
| 72 | Raw | Captured analog oscillator cycles, drift and imperfections included; the procedural tables are all ideal | Analog-style basses, mono leads, string machines |
| 73 | Drawn | Hand-drawn irregular shapes, lo-fi without bitcrush | Chip leads, blips, glitches |

Table layouts that matter for presets:

- **Chord** keyframes: 0 power (root + fifth) · 10 major · 20 maj7 · 30 sus2 · 40 minor · 50 m7 · 60 m9.
  The chord sits 2–4 octaves above a root body, so presets play it `s−24`. In-between
  positions blend two chords.
- **Mallet** keyframes: 0 soft marimba · 10 marimba · 20 vibes · 30 kalimba · 40 xylophone · 50 steel pan · 60 glockenspiel.
- **Mixer** keyframes: 0 saw · 10 saw + pulse 25 % · 20 pulse 25 % · 30 square · 40 triangle · 50 saw + square · 60 saw + pulse 12 %,
  each with a sub square. The main oscillators run at 2× so the sub sits on the played note.
- **Vosim** formant moves from harmonic 2 (0) to 24 (60); **SineSync** ratio 1 (0) → 8 (60);
  **Notch** notch at harmonic 2 (0) → 60 (60).
- **Sub**, **Drive**, **Prime** get brighter towards 60; **Scanned** goes from the hammer strike (0) to the settled ring (60).
- AKWF tables (68–73) are sorted dark (0) → bright (60).

## 2. Preview sequences

The bank uses the **category phrases** of the Nostalgia/Modern banks at 120 BPM
(`render.py` `phrase()`): `bass`, `lead`, `keys`, `pad`, `pluck`, `fx`, `drone`.

---

## Bank: Expansion II · 260–323 · 120 BPM · phrase `category`

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 260 | BS Vosim Talk | Talking VOSIM bass, the vowel opens on each note | O1 Vosim@10 we+.5 · O2 Basic@0 s−12 L.5 | LP24 c84 r.2 | E1 0/.3/.2/.2 · E3 0/.3/.8/.12 | AT→Wave +.4 | FX1 Drive .2 (Soft) → FX2 Comp .8 |
| 261 | BS Sine Sync Bass | Soft sync bass, the ratio jumps on the attack | `Clean` O1 SineSync@0 we+.45 | LP24 c90 e+.2 | E1 0/.22/.1/.15 · E2 0/.25/0/.2 · E3 0/.3/.7/.12 | gl .03 · AT→Wave +.3 | FX1 Comp .8 (Ratio 4:1) |
| 262 | BS Sub Mixer | Polysynth saw over a sub square, punchy | `Clean` O1 Mixer@0 | LP24 c62 r.25 e+.45 | E2 0/.25/.1/.2 · E3 0/.3/.6/.12 | `fv .3` | FX1 Comp .8 (Ratio 4:1) |
| 263 | BS Speaker Sub | Sub that stays audible on small speakers | `Clean` O1 Sub@40 · O2 Sub@0 L.6 | LP24 c70 | E3 0/.4/.8/.15 | gl .04 | FX1 Comp .8 (Ratio 4:1) |
| 264 | BS Sub Punch 808 | Long 808 sub with a saturated punch | `Clean` O1 Sub@10 we+.6 | LP24 c80 | E1 0/.5/0/.3 · E3 0/.6/.3/.4 | Leg gl .15 · M: Wave Env→Pitch +.12 | FX1 Drive .3 (Tube, Gain 8 dB) → FX2 Comp .8 |
| 265 | BS Notch Bass | Saw bass with a slowly moving notch | `Clean` O1 Notch@20 · O2 Basic@0 s−12 L.5 | LP24 c76 r.2 e+.3 | E2 0/.3/.2/.2 · E3 0/.3/.8/.12 | LFO1 Tri sync 1/2 · M: LFO1→O1Wave +.3 | FX1 Comp .8 |
| 266 | BS Driven Round | Warm driven sine bass, harder with velocity | O1 Drive@15 v+.35 · O2 Basic@0 s−12 L.4 | LP24 c70 r.1 fv .3 | E3 0/.35/.8/.12 | AT→Wave +.3 | FX1 Comp .8 (Ratio 4:1) |
| 267 | BS Finger Bass | Round finger-style electric bass | `Clean` O1 E-Bass@15 v+.2 | LP24 c72 e+.2 fv .4 | E2 0/.3/.1/.2 · E3 0/.5/.4/.12 | — | FX1 Comp .7 (Ratio 3:1) |
| 268 | BS Pick Slap | Bright picked bass that pops with velocity | `Clean` O1 E-Bass@40 v+.4 we−.3 | LP24 c88 r.15 e+.3 fv .5 | E1 0/.25/0/.2 · E2 0/.2/0/.15 · E3 0/.4/.3/.12 | — | FX1 Comp .8 (Ratio 4:1) → FX2 Chorus .15 |
| 269 | BS Distorted Grind | Sampled-fuzz bass with grinding mids | O1 Distorted@20 · O2 Basic@0 s−12 L.5 | LP24 c68 r.3 e+.3 | E2 0/.3/.2/.2 · E3 0/.3/1/.12 | LFO1 Tri .3 · M: LFO1→O1Wave +.15 | FX1 Drive .25 (Hard) → FX2 Comp .9 |
| 270 | BS Raw Saw Bass | Imperfect analog saw bass | O1 Raw@30 · O2 Raw@30 f+8 L.7 | LP24 c58 r.3 e+.45 | E2 0/.25/.1/.2 · E3 0/.3/.7/.12 | gl .04 · `fv .3` | FX1 Comp .8 |
| 271 | BS Drawn Bits | Hand-drawn lo-fi chip bass | O1 Drawn@20 · O2 Basic@0 s−12 L.4 | LP24 c80 r.2 e+.3 | E2 0/.2/0/.15 · E3 0/.25/.6/.1 | — | FX1 Drive .15 (Crush .25) → FX2 Comp .8 |
| 272 | LD Vosim Voice | Singing VOSIM lead, pressure opens the vowel | `Clean` O1 Vosim@25 | LP24 c104 | E3 .03/.3/.9/.25 | AT→Wave +.5 · AT→Vib .3 | FX1 Delay .25 (1/8d, Fdbk 35 %) → FX2 Reverb .3 |
| 273 | LD Prime Whistle | Glassy prime-harmonic lead | `Clean` O1 Prime@20 · O2 Prime@35 f+6 L.6 | LP24 c100 | E3 .01/.3/.9/.25 | LFO1 Tri .4 · M: LFO1→O1Wave +.15 · AT→Wave +.3 | FX1 Delay .3 (1/4d, Fdbk 35 %) → FX2 Reverb .25 |
| 274 | LD Soft Sync | Rounded sine-sync sweep lead | O1 SineSync@10 we+.6 | LP24 c108 | E1 .02/.6/.3/.3 · E3 0/.3/.9/.25 | AT→Wave +.3 | FX1 Delay .3 (1/8d, PingP .6) → FX2 Reverb .2 |
| 275 | LD Poly Mixer Lead | Saw + pulse polysynth lead | `Clean` O1 Mixer@10 · O2 Mixer@10 f+10 L.8 | LP24 c92 r.2 e+.2 | E2 .02/.4/.5/.3 · E3 .01/.3/.9/.25 | LFO1 Sine .5 · M: LFO1→Pitch +.003 | FX1 Chorus .3 → FX2 Delay .25 (1/8d) |
| 276 | LD Notch Phase | Phasing saw lead from the notch sweep | `Clean` O1 Notch@30 · O2 Basic@40 f+7 L.5 | LP24 c104 | E3 .01/.3/.9/.25 | LFO1 Tri .35 · M: LFO1→O1Wave +.35 | FX1 Delay .3 (1/4d, Fdbk 35 %) → FX2 Reverb .25 |
| 277 | LD Overdrive Sine | Sine that clips into a square under pressure | O1 Drive@20 v+.3 | LP24 c110 | E3 .01/.3/.9/.25 | AT→Wave +.6 · AT→Vib .3 | FX1 Delay .25 (1/8d) → FX2 Reverb .2 |
| 278 | LD Piccolo Bass | High-register bass-guitar lead | `Clean` O1 E-Bass@55 s+12 v+.3 | LP24 c96 fv .3 | E3 .01/.5/.6/.25 | gl .08 · AT→Wave +.2 · AT→Vib .3 | FX1 Chorus .2 → FX2 Delay .25 (1/8d, Fdbk 30 %) |
| 279 | LD Guitar Solo | Overdriven guitar-wave solo | O1 E-Guitar@50 · O2 E-Guitar@40 f+9 L.6 | LP24 c96 r.15 · Tube .3 post | E3 .01/.4/.85/.3 | gl .06 · LFO1 Sine .55 · M: LFO1→Pitch +.004 · AT→Wave +.3 | FX1 Drive .2 (Tube) → FX2 Delay .3 (1/4d, Fdbk 35 %) |
| 280 | LD Fuzz Lead | Sampled-fuzz screamer | O1 Distorted@45 · O2 Distorted@45 f−12 L.8 | LP24 c100 r.2 | E3 .01/.3/.9/.25 | LFO1 Tri .5 · M: LFO1→O1Wave +.12 · AT→Cut +.3 | FX1 Delay .3 (1/8d, Fdbk 40 %) → FX2 Reverb .25 |
| 281 | LD Theremin | Wavering theremin, vibrato on pressure | `Clean` O1 Theremin@20 | LP12 c100 | E3 .12/.3/1/.4 | gl .3 · LFO1 Sine .6 · M: LFO1→Pitch +.006 · AT→Vib .5 · AT→Wave +.3 | FX1 Delay .25 (1/4, Fdbk 30 %) → FX2 Reverb .4 (Size .8) |
| 282 | LD Raw Mono | Classic analog mono lead | O1 Raw@40 · O2 Raw@50 f+12 L.8 | LP24 c86 r.3 e+.35 | E2 .01/.45/.4/.3 · E3 .01/.3/.9/.25 | gl .07 · LFO1 Tri .5 · M: LFO1→Pitch +.003 · AT→Cut +.3 | FX1 Delay .25 (1/8d, Fdbk 35 %) → FX2 Reverb .2 |
| 283 | LD Drawn Chip | Hand-drawn chiptune lead with a fast trill | O1 Drawn@40 | LP24 c110 | E3 0/.2/.9/.15 | gl .03 · LFO1 Tri sync 1/16 · M: LFO1→Pitch +.01 | FX1 Delay .3 (1/8d, PingP .6) |
| 284 | KY Dub Chord Byte | One-finger m7 dub stab from the Chord table | O1 Chord@50 s−24 | LP24 c78 r.25 e+.4 | E2 0/.25/.05/.2 · E3 0/.3/.1/.25 | — | FX1 Delay .35 (1/4d, Fdbk 50 %) → FX2 Reverb .3 |
| 285 | KY Strike Scan | Struck-membrane keys that settle as they ring | `Clean` O1 Scanned@50 we−.7 | LP24 c96 fv .3 | E1 0/.7/.2/.5 · E3 0/.6/.3/.4 | — | FX1 Chorus .2 → FX2 Reverb .3 |
| 286 | KY Prime Bells | Prime-partial electric bells | `Clean` O1 Prime@15 we+.5 · O2 Basic@0 s+12 L.25 | — | E1 0/.5/0/.4 · E3 0/.7/.1/.5 | Pan Alt .3 | FX1 Delay .2 (1/8d) → FX2 Reverb .35 |
| 287 | KY Vibes | Vibraphone with motor tremolo | `Clean` O1 Mallet@20 | — | E3 0/.8/.15/.6 | — | FX1 Tremolo .5 (Sine, 1/8) → FX2 Reverb .3 |
| 288 | KY Poly Brass Stab | Saw + square polysynth brass stab | `Clean` O1 Mixer@50 · O2 Mixer@0 f+8 L.7 | LP24 c60 r.15 e+.5 | E2 .03/.35/.3/.25 · E3 .01/.3/.7/.2 | — | FX1 Chorus .3 (Ensemble) → FX2 Reverb .2 |
| 289 | KY Clip Organ | Overdriven organ-style keys | O1 Drive@35 · O2 Drive@20 s+12 L.5 | LP24 c96 | E3 0/.3/.85/.1 | LFO1 Sine .55 · M: LFO1→Pitch +.002 | FX1 Chorus .3 (Rate .5) → FX2 Reverb .2 |
| 290 | KY Power Chord | One-finger overdriven power chords | O1 E-Guitar@45 · `Chord 5th` | LP24 c90 r.1 · Tube .4 post | E3 0/.5/.6/.2 | — | FX1 Drive .3 (Tube) → FX2 Reverb .2 |
| 291 | KY Raw Poly | Warm analog poly keys | O1 Raw@25 · O2 Raw@25 f+10 L.8 | LP24 c70 r.15 e+.35 | E2 0/.45/.2/.3 · E3 0/.5/.5/.3 | LFO1 Tri .35 · M: LFO1→O2Wave +.15 | FX1 Chorus .35 (Ensemble) → FX2 Reverb .25 |
| 292 | PD Chord Haze | Chord-in-a-wave pad drifting between chords | `Clean` O1 Chord@40 s−24 · O2 Chord@40 s−24 f+8 L.8 | LP24 c84 | E3 .5/.5/1/.7 | LFO1 Sine .15 · M: LFO1→O1Wave +.15 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .45 (Size .8) |
| 293 | PD Vosim Choir | Vowel-shifting VOSIM choir | `Clean` O1 Vosim@20 · O2 Vosim@30 f+7 L.8 | LP24 c92 | E3 .45/.5/1/.7 | LFO1 Tri .2 · M: LFO1→O1Wave +.25 · LFO2 Sine .15 · M: LFO2→O2Wave −.25 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .45 |
| 294 | PD Scanned Motion | Organic scanning-string pad | `Clean` O1 Scanned@30 · O2 Scanned@45 f+6 L.8 | LP24 c88 | E3 .6/.5/1/.8 | LFO1 Sine .1 · M: LFO1→O1Wave +.4 · LFO2 Tri .13 · M: LFO2→O2Wave −.35 | FX1 Chorus .25 → FX2 Reverb .5 (Decay 5 s) |
| 295 | PD Mallet Haze | Bowed-vibes pad with a glock halo | `Clean` O1 Mallet@20 · O2 Mallet@60 s+12 L.4 | LP24 c90 | E3 .4/.5/1/.7 | LFO1 Sine .2 · M: LFO1→O2Wave −.2 | FX2 Shimmer .4 (Interval +12, Shimmer 30 %) |
| 296 | PD Poly Strings | Polysynth string pad, saw over sub | `Clean` O1 Mixer@0 · O2 Mixer@0 f+12 L1 | LP24 c78 r.1 | E3 .45/.5/1/.6 | LFO1 Tri .3 · M: LFO1→O1Wave +.1 | FX1 Chorus .4 (Ensemble, Depth .7) → FX2 Reverb .4 |
| 297 | PD Notch Phaser | Frozen-phaser saw pad, notches drifting apart | `Clean` O1 Notch@25 · O2 Notch@35 f+9 L.9 | LP24 c92 | E3 .45/.5/1/.7 | LFO1 Sine .12 · M: LFO1→O1Wave +.4 · M: LFO1→O2Wave −.4 | FX1 Chorus .25 → FX2 Reverb .45 |
| 298 | PD E-Bow Swell | Bowed-guitar swell | `Clean` O1 E-Guitar@25 · O2 E-Guitar@30 f+7 L.7 | LP24 c84 r.1 | E3 .7/.5/1/.6 | AT→Wave +.3 · LFO1 Sine .5 · M: LFO1→Pitch +.003 | FX1 Delay .3 (1/4d, Fdbk 45 %) → FX2 Reverb .45 (Size .8) |
| 299 | PD Theremin Choir | Eerie theremin ensemble | `Clean` O1 Theremin@35 · O2 Theremin@15 s+12 L.5 | LP12 c96 | E3 .5/.5/1/.8 | LFO1 Sine .45 · M: LFO1→Pitch +.004 · LFO2 Tri .15 · M: LFO2→O1Wave +.3 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .5 (Size .9) |
| 300 | PD Raw Strings | Analog string machine | O1 Raw@20 · O2 Raw@20 f+14 L1 | LP24 c80 | E3 .4/.5/1/.6 | LFO1 Tri .35 · M: LFO1→O2Wave +.2 | FX1 Chorus .45 (Ensemble, Depth .7) → FX2 Reverb .35 |
| 301 | PL Chord Pluck | One-finger major-7 pluck | `Clean` O1 Chord@20 s−24 | LP24 c70 r.2 e+.5 | E2 0/.22/0/.18 · E3 0/.3/0/.25 | — | FX1 Delay .3 (1/8d, Fdbk 40 %) → FX2 Reverb .25 |
| 302 | PL Scanned Strike | Hammered ring that settles | O1 Scanned@40 we−.7 | LP24 c94 e+.2 | E1 0/.35/0/.3 · E2 0/.25/0/.2 · E3 0/.35/0/.3 | Pan Alt .3 | FX1 Delay .25 (1/8d, PingP .6) → FX2 Reverb .2 |
| 303 | PL Prime Pluck | Glassy prime-partial pluck | `Clean` O1 Prime@10 we+.6 | LP24 c100 | E1 0/.2/0/.15 · E3 0/.3/0/.25 | — | FX1 Delay .3 (1/8d, Fdbk 40 %) → FX2 Reverb .2 |
| 304 | PL Sync Drop | Sine-sync pluck, the ratio falls on each note | O1 SineSync@0 we+.8 | LP24 c104 | E1 0/.18/0/.12 · E3 0/.25/0/.2 | — | FX1 Delay .3 (1/16, PingP .7) → FX2 Reverb .2 |
| 305 | PL Marimba | Wooden marimba that softens as it decays | `Clean` O1 Mallet@0 we+.25 v+.15 | — | E1 0/.25/0/.2 · E3 0/.4/0/.35 | — | FX2 Reverb .25 (Size .6) |
| 306 | PL Kalimba Tines | Thumb-piano tines | `Clean` O1 Mallet@30 we+.2 | LP24 c100 | E1 0/.15/0/.1 · E3 0/.45/0/.4 | Pan Alt .3 | FX1 Delay .25 (1/8d, Fdbk 30 %) → FX2 Reverb .3 |
| 307 | PL Sub Pluck | Deep sub pluck with a harmonic snap | `Clean` O1 Sub@10 we+.8 | LP24 c90 | E1 0/.15/0/.1 · E3 0/.3/0/.25 | — | FX1 Comp .7 → FX2 Delay .2 (1/8d) |
| 308 | PL Muted Bass Pluck | Palm-muted electric bass pluck | `Clean` O1 E-Bass@30 v+.3 | LP24 c64 r.15 e+.45 fv .4 | E2 0/.18/0/.12 · E3 0/.22/0/.15 | — | FX1 Comp .8 → FX2 Delay .2 (1/8d) |
| 309 | PL Funk Chop | Clean-guitar funk chop | `Clean` O1 E-Guitar@15 v+.3 | BP12 c84 r.3 e+.2 | E2 0/.15/0/.1 · E3 0/.18/0/.1 | — | FX1 Phaser .3 → FX2 Delay .2 (1/16) |
| 310 | PL Drawn Blip | Lo-fi hand-drawn blip | O1 Drawn@50 we−.4 | — | E1 0/.15/0/.1 · E3 0/.2/0/.15 | — | FX1 Drive .15 (Crush .3) → FX2 Delay .3 (1/8d, PingP .6) |
| 311 | FX Vosim Chatter | Random vowel chatter | O1 Vosim@30 | LP24 c100 | E3 0/.4/1/.3 | LFO1 S&H .7 · M: LFO1→O1Wave +.5 | FX1 Delay .35 (1/16, PingP .8) → FX2 Reverb .3 |
| 312 | FX Sync Siren | Sine-sync siren sweep | O1 SineSync@30 | LP24 c110 | E3 .05/.5/1/.5 | LFO1 Tri .4 · M: LFO1→O1Wave +.5 · M: LFO1→Pitch +.08 | FX1 Delay .4 (1/4d, Fdbk 55 %) → FX2 Reverb .4 |
| 313 | FX Notch Jet | Jet-plane notch sweep over noise | `Clean` O1 Notch@0 we+1.0 · O2 Noise@40 L.3 | — | E1 .8/.5/1/.6 · E3 .3/.5/1/.6 | M: Wave Env→Pitch +.1 | FX1 Flanger .4 (Sync 1 bar) → FX2 Reverb .45 |
| 314 | FX Overdrive Riser | Sine that drives into a square as it rises | O1 Drive@0 we+1.0 · O2 Drive@0 f+15 L.7 | — | E1 .75/.5/1/.5 · E3 .2/.5/1/.6 | M: Wave Env→Pitch +.25 | FX1 Delay .3 (1/8d, Fdbk 50 %) → FX2 Reverb .45 |
| 315 | FX Fuzz Impact | Distorted downward impact | O1 Distorted@60 we−.8 · O2 Basic@0 s−12 L.6 | LP24 c100 e−.4 | E1 0/.6/0/.5 · E2 0/.6/0/.5 · E3 0/.7/0/.6 | M: Wave Env→Pitch +.3 | FX1 Drive .3 (Hard) → FX2 Reverb .4 (Size .8) |
| 316 | FX Saucer | Sci-fi theremin flyby | `Clean` O1 Theremin@40 | LP12 c110 | E3 .3/.5/1/.6 | LFO1 Sine .7 · M: LFO1→Pitch +.06 · LFO2 Tri .2 · M: LFO2→Pitch +.1 | FX1 Delay .4 (1/4d, Fdbk 55 %, PingP .7) → FX2 Reverb .5 |
| 317 | FX Drawn Glitch | Stepped hand-drawn glitches | O1 Drawn@30 | — | E3 0/.4/1/.2 | LFO1 S&H .8 · M: LFO1→O1Wave +.6 · M: LFO1→Pitch +.05 | FX1 Delay .3 (1/16, PingP .8) |
| 318 | DR Chord Drift | Drifting chord cloud | `Clean` O1 Chord@30 s−24 · O2 Chord@40 s−24 f+5 L.7 | LP24 c78 | E3 .7/.5/1/.8 | LFO1 Sine .06 · M: LFO1→O1Wave +.3 · LFO2 Tri .08 · M: LFO2→O2Wave −.3 | FX2 Reverb .55 (Size .9, Decay 8 s) |
| 319 | DR Scanned Ring | Slowly scanning ring resonance | `Clean` O1 Scanned@20 · O2 Scanned@50 s+7 L.5 | LP24 c84 | E3 .7/.5/1/.8 | LFO1 Sine .08 · M: LFO1→O1Wave +.5 · LFO2 Sine .05 · M: LFO2→O2Wave −.4 | FX1 Phaser .2 → FX2 Reverb .55 (Decay 6 s) |
| 320 | DR Prime Spectrum | Prime partials fading in and out | `Clean` O1 Prime@30 · O2 Basic@0 s−12 L.5 | LP24 c90 | E3 .7/.5/1/.8 | LFO1 Tri .07 · M: LFO1→O1Wave +.5 | FX2 Shimmer .45 (Interval +12, Shimmer 30 %) |
| 321 | DR Sub Pressure | Sub drone that saturates under pressure | `Clean` O1 Sub@10 L.6 · O2 Sub@30 s+7 L.25 | LP24 c70 | E3 .6/.5/1/.8 | AT→Wave +.6 · LFO1 Sine .1 · M: LFO1→O2Wave +.3 | FX1 Chorus .2 → FX2 Reverb .5 (Decay 6 s) |
| 322 | DR Fuzz Monolith | Slow distorted drone wall | O1 Distorted@10 L.6 · O2 Distorted@20 s−12 L.4 | LP24 c72 r.2 | E3 .7/.5/1/.8 | LFO1 Sine .07 · M: LFO1→O1Wave +.4 · M: LFO1→Cutoff +.15 | FX1 Flanger .3 (Sync 4 bars) → FX2 Reverb .55 (Decay 8 s) |
| 323 | DR Theremin Ghost | Ghostly wandering theremin | `Clean` O1 Theremin@10 | LP12 c90 · Static .1 | E3 .8/.5/1/.9 | LFO1 Sine .05 · M: LFO1→O1Wave +.4 · LFO2 Rand .2 · M: LFO2→Pitch +.01 | FX2 Reverb .6 (Size .9, Decay 8 s) |
