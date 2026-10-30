# 14 — Expansion Bank (224–255) and Wavetables 48–57

> Authoring source: the numbers below are doc numbers (`doc_no`), not positions in the shipped factory bank, which is one bank sorted by category ([06](06-presets-and-state.md)).

32 presets that show off the ten expansion wavetables
([07](07-wavetables-and-ip.md) §3.4). Same notation and voicing rules as
[09](09-factory-presets.md) §2 and [10](10-factory-presets-modern.md) §2. Every
preset uses at least one table from 48–57, and every new table appears in at
least three presets.

The bank ships with **v0.4** (needs Clean mode, Cross mod and wavetables 48–57).
In the browser it follows Minimal Techno.

## 1. Why these ten tables

| # | Name | Gap it fills | Typical use |
|---|------|--------------|-------------|
| 48 | Even | Only odd-harmonic (Odd) existed; even harmonics give hollow octave/fifth-less tones | Organs, glow pads, octave basses |
| 49 | Partials | No way to scan single harmonics over a steady fundamental | Whistles, harmonic arps, additive bells, risers |
| 50 | Feedback | Operator self-feedback (sine → saw → edge) without using Cross mod | Growl basses, gritty leads, FM pings |
| 51 | Window | Windowed-resonance waves: filter-like squelch without a filter, stays 8-bit stepped | Squelch leads, resonant plucks |
| 52 | Rave | Sub saw + octave PWM saw pair (hoover family) | Rave leads, reese basses, sirens |
| 53 | Octaves | Saw registrations at 1×/2×/4×/8× (16′–2′) | Octave stabs and basses, organ drones |
| 54 | Logic | Bitwise (XOR) 7-bit waves — lo-fi but different from Digital/Granite | Chip basses, glitch FX |
| 55 | Reed | Clarinet → oboe spectra; missing woodwind family next to Sax/Flute | Solo reeds, wind sections |
| 56 | Pluck | Ideal plucked-string spectra, pluck position centre → bridge | Nylon/guitar/harp keys and plucks |
| 57 | Sweep | Saw through a baked 24 dB low-pass sweep with a small peak | Filterless acid/sweeps, cheap Clean-mode "filter" via Wave Env |

## 2. Preview sequences

The bank uses the **category phrases** of the Nostalgia/Modern banks at 120 BPM
(`render.py` `phrase()`): `bass`, `lead`, `keys`, `pad`, `pluck`, `fx`, `drone`.

---

## Bank: Expansion · 224–255 · 120 BPM · phrase `category`

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 224 | BS Octave Punch | Stacked octave saws, punchy | `Clean` O1 Octaves@20 we+.3 | LP24 c60 r.25 e+.4 | E1 0/.2/0/.15 · E2 0/.2/0/.15 · E3 0/.3/.5/.12 | Mono · `fv .4` | FX1 Comp .8 (Ratio 4:1) |
| 225 | BS Feedback Growl | Feedback-FM growl, sine to saw on each note | O1 Feedback@10 we+.6 · O2 Basic@0 s−12 L.5 | LP24 c70 r.2 · Trans .3 pre | E1 0/.35/.1/.2 · E3 0/.3/.7/.1 | Mono gl .04 | FX1 Drive .2 (Soft) → FX2 Comp .8 |
| 226 | BS Sweep Squelch | Filterless acid line from the baked sweep | O1 Sweep@10 we+.7 | — | E1 0/.18/0/.12 · E3 0/.25/.6/.1 | Leg gl .08 · AT→Wave +.3 | FX1 Delay .15 (1/16d, Fdbk 30 %) |
| 227 | BS Rave Reese | Hoover-family reese bass | O1 Rave@20 · O2 Rave@30 f+12 L1 | LP24 c66 r.2 | E3 0/.3/1/.12 | Mono gl .06 · LFO1 Tri .35 · M: LFO1→O1Wave +.2 | FX1 Chorus .25 → FX2 Comp .8 |
| 228 | BS Logic Bits | Bitwise 8-bit bass over a sub | O1 Logic@10 · O2 Basic@0 s−12 L.6 | LP24 c72 r.3 e+.35 | E2 0/.2/0/.15 · E3 0/.25/.4/.1 | Mono | FX1 Comp .8 |
| 229 | LD Partial Whistle | Harmonic-scanning whistle | `Clean` O1 Partials@15 | — | E3 .02/.3/.8/.3 | Leg gl .08 · AT→Wave +.4 · LFO1 Sine .6 · M: LFO1→Pitch +.004 | FX1 Delay .3 (1/8d, Fdbk 40 %) → FX2 Reverb .3 |
| 230 | LD Hoover | Rave hoover with pitch scoop | O1 Rave@40 · O2 Rave@50 f+15 L1 | LP24 c100 | E1 0/.4/0/.3 · E3 .01/.3/1/.3 | Leg gl .15 · M: Wave Env→Pitch −.04 · M: Wave Env→O1Wave +.3 | FX1 Chorus .4 (Ensemble, Depth .7) → FX2 Reverb .3 |
| 231 | LD Reed Solo | Clarinet that turns oboe under pressure | `Clean` O1 Reed@10 v+.3 | LP12 c96 fv .3 | E3 .04/.4/.8/.25 | Leg gl .06 · AT→Wave +.5 · AT→Vib .4 | FX2 Reverb .3 |
| 232 | LD Window Squelch | Resonant window sweep lead | O1 Window@10 we+.5 | LP24 c104 | E1 .02/.6/.3/.3 · E3 0/.3/.9/.2 | Leg gl .05 · AT→Wave +.3 | FX1 Delay .3 (1/8d, PingP .6) |
| 233 | LD Feedback Edge | Gritty feedback-FM solo | O1 Feedback@30 · O2 Feedback@30 f+8 L.7 | LP24 c100 r.2 · Trans .3 post | E3 .01/.3/.9/.2 | Leg gl .06 · LFO1 Tri .55 · M: LFO1→O1Wave +.15 · AT→Wave +.3 | FX1 Delay .3 (1/4d, Fdbk 35 %) → FX2 Reverb .25 |
| 234 | KY Nylon Keys | Nylon-string keys, bright pick | `Clean` O1 Pluck@20 v+.3 we+.3 | LP24 c90 fv .4 | E1 0/.4/0/.3 · E3 0/.6/.2/.35 | — | FX1 Chorus .2 → FX2 Reverb .3 |
| 235 | KY Even Organ | Hollow even-harmonic organ | O1 Even@30 · O2 Octaves@20 L.5 | LP24 c96 | E3 0/.3/.8/.12 | LFO1 Sine .55 · M: LFO1→Pitch +.002 | FX1 Chorus .3 (Rate .5) → FX2 Reverb .2 |
| 236 | KY Octave Stab | One-finger octave-saw m7 stab | `Clean` O1 Octaves@35 · O2 Octaves@35 f+10 L.8 · `Chord m7` | LP24 c80 r.2 e+.35 | E2 0/.25/.1/.2 · E3 0/.3/.1/.2 | — | FX1 Delay .25 (1/8d) → FX2 Reverb .25 |
| 237 | KY Partial Bells | Additive bells, partial falls during decay | `Clean` O1 Partials@30 we−.4 · O2 Basic@0 s+12 L.3 | — | E1 0/.5/0/.4 · E3 0/.7/.1/.5 | Pan Alt .4 | FX1 Delay .2 (1/8d) → FX2 Reverb .35 |
| 238 | KY Logic Keys | Bitwise keys over a string pluck | O1 Logic@25 · O2 Pluck@30 L.5 | LP24 c84 r.2 e+.3 fv .3 | E2 0/.3/.1/.2 · E3 0/.45/.2/.25 | — | FX1 Chorus .25 → FX2 Reverb .2 |
| 239 | PD Reed Ensemble | Woodwind section pad | `Clean` O1 Reed@20 · O2 Reed@35 f+9 L.8 | LP24 c86 | E3 .4/.5/1/.6 | LFO1 Tri .3 · M: LFO1→O1Wave +.2 · AT→Wave +.3 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .4 |
| 240 | PD Hollow Glow | Even-harmonic glow with a partial halo | `Clean` O1 Even@20 · O2 Partials@25 s+12 L.4 | LP24 c88 | E3 .5/.5/1/.7 | LFO1 Sine .25 · M: LFO1→O2Wave +.3 | FX2 Shimmer .4 (Interval +12, Shimmer 30 %) |
| 241 | PD Sweep Bloom | Filterless saw bloom | `Clean` O1 Sweep@5 we+.7 · O2 link f+12 L1 | — | E1 .6/.6/.7/.7 · E3 .4/.5/1/.7 | AT→Wave +.2 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .4 (Size .8) |
| 242 | PD Rave Strings | Hoover string pad, counter-moving waves | O1 Rave@10 · O2 Rave@20 f+14 L1 | LP24 c84 r.15 | E3 .35/.5/1/.6 | LFO1 Tri .3 · M: LFO1→O1Wave +.25 · M: LFO1→O2Wave −.25 | FX1 Chorus .35 (Ensemble) → FX2 Reverb .4 |
| 243 | PD Window Drift | Slowly scanning resonant windows | `Clean` O1 Window@20 · O2 Window@40 f+7 L.7 | LP24 c90 | E3 .5/.5/1/.7 | LFO1 Sine .2 · M: LFO1→O1Wave +.4 · LFO2 Tri .15 · M: LFO2→O2Wave −.3 | FX1 Phaser .25 → FX2 Reverb .45 (Decay 5 s) |
| 244 | PL Guitar Pick | Picked string, bright attack | `Clean` O1 Pluck@40 we−.6 v+.3 | LP24 c92 fv .3 | E1 0/.25/0/.2 · E3 0/.4/0/.3 | — | FX1 Delay .25 (1/8d, Fdbk 35 %) → FX2 Reverb .2 |
| 245 | PL Feedback Ping | Feedback-FM ping | `Clean` O1 Feedback@0 we+.8 | — | E1 0/.15/0/.1 · E3 0/.25/0/.2 | Pan Alt .4 | FX1 Delay .3 (1/8d, PingP .7) |
| 246 | PL Bit Logic Arp | Bitwise arp with a crushed edge | O1 Logic@5 we+.6 | LP24 c86 r.25 e+.3 | E1 0/.2/0/.15 · E2 0/.2/0/.15 · E3 0/.22/0/.18 | — | FX1 Drive .2 (Crush .3) → FX2 Delay .3 (1/16) |
| 247 | PL Window Pluck | Resonant window snap | O1 Window@50 we−.8 | — | E1 0/.2/0/.15 · E3 0/.25/0/.2 | — | FX1 Delay .25 (1/8d) → FX2 Reverb .25 |
| 248 | PL Reed Staccato | Short reed staccato | `Clean` O1 Reed@40 v+.2 | LP24 c96 e+.2 | E2 0/.15/0/.1 · E3 0/.22/.25/.12 | — | FX2 Reverb .25 (Size .6) |
| 249 | FX Partial Riser | Climbing harmonic scan | `Clean` O1 Partials@0 we+1.0 | — | E1 .75/.5/1/.5 · E3 .2/.5/1/.6 | M: Wave Env→Pitch +.25 | FX1 Delay .3 (1/8d, Fdbk 50 %) → FX2 Reverb .45 |
| 250 | FX Logic Glitch | Stepped bitwise glitch | O1 Logic@30 | — | E3 0/.4/1/.2 | LFO1 S&H .8 · M: LFO1→O1Wave +.6 · M: LFO1→Pitch +.05 | FX1 Delay .3 (1/16, PingP .8) |
| 251 | FX Feedback Storm | Feedback-FM noise storm | O1 Feedback@60 · O2 Feedback@50 f+30 L.8 | BP12 c70 r.4 e+.4 · Static .2 | E2 .6/.5/1/.6 · E3 .3/.5/1/.6 | LFO1 Rand .5 · M: LFO1→Cutoff +.2 | FX1 Flanger .4 (Sync 1 bar) → FX2 Reverb .5 |
| 252 | FX Rave Siren | Hoover siren swoop | O1 Rave@30 · O2 Rave@40 f+20 L1 | LP24 c100 | E3 .05/.5/1/.5 | LFO1 Tri .45 · M: LFO1→Pitch +.1 | FX1 Delay .4 (1/4d, Fdbk 55 %) → FX2 Reverb .4 |
| 253 | DR Octave Drone | Slowly shifting octave registrations | O1 Octaves@0 · O2 Even@20 s−12 L.6 | LP24 c80 | E3 .6/.5/1/.8 | LFO1 Sine .1 · M: LFO1→O1Wave +.6 | FX1 Chorus .25 → FX2 Reverb .5 (Decay 6 s) |
| 254 | DR Sweep Tide | Two baked sweeps drifting apart | `Clean` O1 Sweep@20 · O2 Sweep@20 f+6 L1 | — | E3 .7/.5/1/.8 | LFO1 Sine .08 · M: LFO1→O1Wave +.5 · LFO2 Tri .1 · M: LFO2→O2Wave +.5 | FX2 Reverb .55 (Size .9, Decay 8 s) |
| 255 | DR Reed Monastery | Droning reed organ with wind | O1 Reed@5 · O2 Even@10 s+7 L.5 | LP24 c78 · Static .1 | E3 .7/.5/1/.8 | LFO1 Tri .12 · M: LFO1→O1Wave +.3 · Flav .3 | FX2 Reverb .55 (Decay 8 s) |
