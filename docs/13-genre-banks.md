# 13 — Genre Banks (128–223)

Four additional factory banks, 24 presets each, in the same notation and
voicing rules as [09](09-factory-presets.md) §2 and [10](10-factory-presets-modern.md) §2.
Every bank has six roles × 4 presets:

| Role | Prefix | Preview sequence role |
|------|--------|-----------------------|
| Bass | `BS` | `bass` |
| Lead | `LD` | `lead` |
| Poly keys | `KY` | `keys` |
| Pad | `PD` | `pad` |
| Pluck / arp | `PL` | `pluck` |
| FX | `FX` | `fx` |

All archetypes come from widely documented genre production vocabulary. The
presets are original. Banks ship with **v0.3** because they rely on Clean mode,
Chord and Scale. In the browser they follow the Modern bank: Trance 128–151,
Tech House 152–175, Deep House 176–199, Minimal Techno 200–223.

**Preview sequences** are the phrases the preview renderer plays through the C engine (`tools/emulator/render.py`,
phrase `<genre>_<role>`). They are written in C minor at the bank tempo. Factory
presets leave Scale user-controlled; the `Scale Set` notes are recommendations.
On Move, Scale=Set follows the song's current root and scale. LFO/FX sync follows
the tempo. Pressure ramps (aftertouch) appear where the table says "AT ramp".
Set the song tempo to the bank tempo to hear synced presets as designed.

---

## Bank: 2000s Trance · 128–151 · 138 BPM · phrase `trance`

Supersaws, gated leads, offbeat rolling basses, big breakdown pads, i–VI–III–VII
progressions (Cm–Ab–Eb–Bb).

| Role | Preview sequence |
|------|------------------|
| bass | 2 bars rolling offbeat 16ths (three 16ths after each beat), roots C then Ab |
| lead | 2-bar legato anthem melody (C5–G5), AT ramp in bar 2 |
| keys | Offbeat 8th stabs, Cm then Ab triads |
| pad | 4 bars of whole-note chords Cm–Ab–Eb–Bb, AT ramp |
| pluck | 2 bars of 16th arpeggio over Cm then Ab |
| fx | One note held 4 bars, AT ramp |

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 128 | BS Rolling Offbeat | Tight offbeat saw bass | `Clean` O1 Basic@40 · O2 Basic@40 s−12 L.6 | LP24 c58 r.2 e+.45 | E2 0/.2/0/.15 · E3 0/.22/0/.12 | Mono · `fv .3` | FX1 Comp .8 (Ratio 4:1) |
| 129 | BS Trance Sub Pluck | Round sub with a click of tone | `Clean` O1 Basic@0 · O2 Basic@20 L.4 | LP24 c62 e+.3 | E2 0/.2/0/.15 · E3 0/.25/0/.15 | Mono | FX1 EQ 1.0 (Low +3, Low Cut 30 Hz) |
| 130 | BS Acid Squelch | Resonant squelch, slides on overlap | O1 Resonant@15 we+.4 · O2 link L.5 | LP24 c48 r.6 e+.55 · Trans .35 pre | E1 0/.2/0/.15 · E2 0/.18/0/.15 · E3 0/.25/.6/.12 | Leg gl .1 · AT→Cut +.3 | FX1 Delay .15 (1/16d, Fdbk 30 %) |
| 131 | BS Saw Gate Bass | Gated detuned saw bass | `Clean` O1 Stack@20 · O2 Basic@63 s−12 L.6 | LP24 c70 r.15 e+.3 | E2 0/.25/.2/.15 · E3 0/.3/.8/.08 | Mono | FX1 Tremolo .7 (Square, Sync 1/16, Depth 90 %) → FX2 Comp .8 |
| 132 | LD Supersaw Anthem | Wide hands-in-the-air supersaw | `Clean` O1 Stack@45 · O2 Stack@50 f+18 L1 | LP24 c108 | E3 .01/.3/1/.3 | Leg gl .04 · `Scale Set` · AT→Vib .35 | FX1 Chorus .35 (Ensemble, Depth .6) → FX2 Reverb .3 (Size .8, Decay 3 s) |
| 133 | LD Gated Saw Lead | 16th trance gate on twin saws | `Clean` O1 Basic@63 · O2 Basic@63 f+12 L1 | LP24 c100 r.15 | E3 0/.3/1/.15 | Leg gl .03 · `Scale Set` | FX1 Tremolo .9 (Square, Sync 1/16, Depth 85 %) → FX2 Delay .3 (1/8d, Fdbk 45 %) |
| 134 | LD Sync Siren | Swelling sync lead with vibrato | O1 Basic@40 · O2 Basic@0 L0 · XM Sync .1 | LP24 c104 r.2 | E1 .1/.5/.6/.3 | Leg gl .12 · LFO1 Sine .7 · M: LFO1→Pitch +.006 · M: Wave Env→XMod +.35 · M: Poly AT→XMod +.3 | FX1 Delay .3 (1/4d, PingP .6) → FX2 Reverb .3 |
| 135 | LD Pluck Lead Echo | Plucky lead into long echoes | `Clean` O1 Harmonic@30 we+.4 · O2 Basic@63 s+12 L.4 | LP24 c95 e+.3 | E1 0/.35/.2/.3 · E2 0/.35/.3/.3 · E3 0/.5/.5/.3 | `Scale Set` | FX1 Delay .35 (1/8d, Fdbk 55 %) → FX2 Reverb .25 |
| 136 | KY Trance Stab | Classic detuned chord stab | `Clean` O1 Stack@30 · O2 Stack@36 f+14 L1 | LP24 c92 r.2 e+.35 | E2 0/.25/.1/.2 · E3 0/.3/.1/.2 | — | FX1 Delay .25 (1/8d, Fdbk 40 %) → FX2 Reverb .3 (Size .8) |
| 137 | KY Hands Up Piano | Bright layered piano | `Clean` O1 Piano@30 v+.3 · O2 EPiano@20 L.5 | LP24 c100 fv .4 | E3 0/.55/.2/.35 | — | FX1 Comp .6 → FX2 Reverb .25 |
| 138 | KY Detune Organ | Detuned organ stabs | O1 Organ@50 · O2 Organ@50 f+10 L.8 | LP24 c95 | E3 0/.3/.4/.15 | — | FX1 Phaser .25 → FX2 Reverb .2 |
| 139 | KY Bell Chords | Glassy bell chords, alternating pan | `Clean` O1 Basic@0 · O2 Basic@0 s+22 f−31 L0 · XM PM .1 | — | E1 0/.4/0/.4 · E3 0/.5/.1/.4 | Pan Alt .4 · M: Wave Env→XMod +.5 · M: Velocity→XMod +.15 | FX1 Delay .2 (1/8d) → FX2 Reverb .3 |
| 140 | PD Euphoric Saws | Huge breakdown supersaw pad | `Clean` O1 Stack@40 · O2 Stack@44 f+20 L1 | LP24 c90 e+.2 | E2 .4/.6/.8/.6 · E3 .35/.5/1/.6 | Pan Spread .8 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .45 (Size .9, Decay 4 s) |
| 141 | PD Epic Strings | Breakdown string section | `Clean` O1 Violin@20 · O2 Cello@20 s−12 L.7 | LP24 c88 | E3 .45/.5/1/.65 | LFO1 Tri .55 · M: LFO1→Pitch +.004 · Pan Spread .6 | FX2 Reverb .5 (Size .9, Decay 5 s) |
| 142 | PD Pumping Saws | Sidechain-pumped saw pad | `Clean` O1 Stack@50 · O2 Stack@54 f+16 L1 | LP24 c95 | E3 .2/.5/1/.5 | Pan Spread .7 | FX1 Tremolo .9 (Pump, Sync 1/4, Depth 80 %) → FX2 Reverb .3 |
| 143 | PD Choir Shimmer | Angelic choir with octave halo | `Clean` O1 Choir@20 · O2 Voice@30 L.6 | LP12 c95 | E3 .5/.5/1/.7 | AT→Wave +.3 | FX2 Shimmer .45 (Interval +12, Shimmer 40 %) |
| 144 | PL Trance Gate Arp | Ping-pong saw arp | `Clean` O1 Basic@40 · O2 Basic@40 f+10 L1 | LP24 c80 r.3 e+.4 | E2 0/.2/0/.15 · E3 0/.22/0/.18 | `Scale Set` · ARP | FX1 Delay .35 (1/8d, Fdbk 50 %, PingP .7) → FX2 Reverb .3 |
| 145 | PL Glass Arp | Crystal 16th arp | `Clean` O1 Glass@25 | — | E3 0/.3/0/.3 | `Scale Set` · ARP | FX1 Delay .3 (1/16d, PingP .8) → FX2 Reverb .35 |
| 146 | PL Resonant Arp | Bar-long filter breathing arp | O1 Resonant@20 we−.5 | LP24 c78 r.45 e+.4 | E1 0/.2/0/.2 · E2 0/.2/0/.15 · E3 0/.25/0/.2 | `Scale Set` · ARP · LFO1 Tri sync 1 bar · M: LFO1→Cutoff +.15 | FX1 Delay .3 (1/8d, Fdbk 45 %) → FX2 Reverb .25 |
| 147 | PL Pizzicato Arp | Plucked string arp | `Clean` O1 Violin@40 v+.2 | HP12 c40 | E3 0/.2/0/.2 | `Scale Set` · ARP | FX2 Reverb .35 (Size .7) |
| 148 | FX Uplifter | Noise + saw rising sweep | `Clean` O1 Noise@40 L.5 · O2 Stack@20 L.5 | BP12 c50 r.4 e+.8 · Static .4 | E2 .7/.5/1/.5 · E3 .2/.5/1/.6 | M: Filter Env→Pitch +.3 | FX1 Phaser .3 → FX2 Reverb .5 |
| 149 | FX Downsweep | Falling filtered noise | `Clean` O1 Noise@50 | LP24 c50 r.3 e+.9 · Static .5 | E2 0/.75/0/.6 · E3 0/.75/0/.6 | M: Filter Env→Pitch +.2 | FX2 Reverb .6 (Decay 6 s) |
| 150 | FX Laser Zap | Sync zap into ping-pong | `Clean` O1 Sync@0 we+1.0 | — | E1 0/.3/0/.2 · E3 0/.35/0/.3 | M: Wave Env→Pitch +.6 | FX1 Delay .4 (1/8d, Fdbk 50 %, PingP .8) |
| 151 | FX Atmo Wash | Breathy shimmering atmosphere | O1 Breath@20 · O2 Choir@10 f+9 L.5 | HP12 c62 r.1 · Static .35 | E3 .7/.5/1/.8 | LFO1 Sine .25 · M: LFO1→O1Wave +.5 · M: LFO1→Cutoff +.12 · Flav .4 | FX2 Shimmer .6 (Decay 10 s, Interval +12, Shimmer 45 %) |

---

## Bank: Tech House · 152–175 · 125 BPM · phrase `tech`

Rubbery syncopated basses, organ and vocal stabs, percussive plucks, filtered
pads, swung 16ths (12 %).

| Role | Preview sequence |
|------|------------------|
| bass | 2 bars syncopated 16th groove around C2 with octave and minor-third pickups |
| lead | 2-bar call-and-response hook, AT ramp in bar 2 |
| keys | Syncopated Cm7 then Fm7 stabs (steps 3, 7, 10, 14) |
| pad | 4 bars Cm7–Fm7 (2 bars each), AT ramp |
| pluck | 2 bars percussive 16th one-note groove with accents |
| fx | One note held 2 bars, AT ramp |

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 152 | BS Tech Rubber | Bouncy pulse rubber bass | `Clean` O1 Basic@20 · O2 Pulse@20 s−12 L.5 | LP24 c55 r.35 e+.4 | E2 0/.18/0/.12 · E3 0/.25/.2/.1 | Mono gl .04 · `fv .4` | FX1 Comp .9 (Ratio 4:1) |
| 153 | BS Grit Groove | Gritty spectral groove bass | O1 BassSpectra@20 · O2 Crushed@10 L.3 | LP24 c60 r.25 e+.35 · Trans .4 pre | E2 0/.2/0/.15 · E3 0/.25/.5/.1 | Mono | FX1 Drive .3 (Soft, Low Cut 120 Hz) → FX2 Comp .8 |
| 154 | BS Organ Bass | Organ-tone house bass | O1 E-Organ@20 · O2 Basic@0 s−12 L.6 | LP24 c65 | E3 0/.3/.6/.1 | Mono | FX1 Comp .8 |
| 155 | BS Wobble Low | Eighth-note wobble groove | `Clean` O1 Basic@60 · O2 Basic@0 L.8 · XM Sync .25 | LP24 c50 r.3 | E3 0/.3/1/.1 | Mono · LFO1 Sine sync 1/8 · M: LFO1→Cutoff +.25 · M: LFO1→XMod +.2 | FX1 Drive .25 → FX2 Comp .9 |
| 156 | LD Vocal Hook | Vowel-y vocal hook | `Clean` O1 Voice@30 we+.3 | LP12 c100 | E1 0/.4/.3/.2 · E3 0/.3/.6/.15 | Leg gl .05 · `Scale Set` · AT→Wave +.4 | FX1 Delay .3 (1/8, Fdbk 35 %) → FX2 Reverb .2 |
| 157 | LD Chip Hook | Playful chip hook | O1 Chip@20 · O2 Chip@40 s+12 L.4 | LP24 c110 | E3 0/.25/.5/.12 | Mono gl .03 · `Scale Set` | FX1 Delay .25 (1/16d, PingP .7) |
| 158 | LD Sax Stab Lead | Short sax-like stabs | `Clean` O1 Sax@30 v+.3 | LP24 c95 fv .4 | E3 .02/.35/.6/.2 | Leg gl .06 · `Scale Set` · AT→Vib .4 | FX1 Comp .6 → FX2 Reverb .25 |
| 159 | LD Filter Squeal | Resonant squealing lead | O1 Resonant2@30 | LP24 c85 r.6 | E3 0/.3/.8/.2 | Mono gl .1 · LFO1 Tri sync 1/2 · M: LFO1→Cutoff +.2 · AT→Cut +.4 | FX1 Phaser .3 → FX2 Delay .25 (1/8d) |
| 160 | KY Organ Stab | One-finger m7 organ stab | `Clean` O1 E-Organ@40 · `Chord m7` | LP24 c100 | E3 0/.25/0/.15 | `Scale Set` | FX1 Comp .8 → FX2 Reverb .2 |
| 161 | KY Tine Chop | Chopped tremolo EP | `Clean` O1 EPiano@15 v+.3 | LP24 c92 fv .4 | E3 0/.35/0/.25 | — | FX1 Tremolo .4 (Stereo 80 %, Sync 1/8) → FX2 Delay .2 (1/8d) |
| 162 | KY Jackin Stab | Filtered minor stab | `Clean` O1 Basic@40 · O2 Organ@30 L.6 · `Chord Min` | LP24 c80 r.3 e+.4 | E2 0/.2/0/.15 · E3 0/.22/0/.15 | `Scale Set` | FX1 Delay .3 (1/8d, Fdbk 40 %) → FX2 Reverb .2 |
| 163 | KY Clav Funk | Funky phased clav | `Clean` O1 Clavinet@20 v+.4 | HP12 c35 | E3 0/.28/0/.15 | — | FX1 Phaser .3 (Rate .4) → FX2 Comp .7 |
| 164 | PD Filtered Warmth | Slowly opening warm pad | `Clean` O1 Strings@20 · O2 Stack@30 f+10 L.6 | LP24 c70 r.2 | E3 .4/.5/1/.55 | LFO1 Sine sync 2 bars · M: LFO1→Cutoff +.15 · Pan Spread .6 | FX2 Reverb .35 |
| 165 | PD Chord Memory | One-finger m7 pumping pad | `Clean` O1 Stack@30 · `Chord m7` · `Scale Set` | LP24 c85 | E3 .3/.5/1/.5 | Pan Spread .6 | FX1 Tremolo .7 (Pump, Sync 1/4, Depth 60 %) → FX2 Reverb .3 |
| 166 | PD Dark Drone Pad | Dark comb bed | O1 Comb@20 · O2 BassSpectra@30 s−12 L.6 | LP24 c65 r.25 | E3 .5/.5/1/.6 | LFO1 Tri .3 · M: LFO1→O1Wave +.4 | FX2 Reverb .45 (Decay 6 s) |
| 167 | PD Phased Air | Airy phased glass | `Clean` O1 Breath@20 · O2 Glass@10 L.5 | HP12 c40 | E3 .45/.5/1/.6 | Flav .3 | FX1 Phaser .4 (Rate .25) → FX2 Reverb .4 |
| 168 | PL Perc Stab | Metallic percussive stab | `Clean` O1 Basic@50 · O2 Metal@10 s+12 L.3 | LP24 c75 r.35 e+.5 | E2 0/.15/0/.1 · E3 0/.18/0/.12 | ARP | FX1 Delay .25 (1/16d, Fdbk 30 %) → FX2 Comp .7 |
| 169 | PL Rubber Arp | Pulse arp, width moves per bar | `Clean` O1 Pulse@20 | LP24 c78 r.4 e+.45 | E2 0/.22/0/.15 · E3 0/.26/0/.15 | `Scale Set` · ARP · LFO1 Tri sync 1 bar · M: LFO1→O1Wave +.3 | FX1 Delay .3 (1/8d) |
| 170 | PL Marimba Groove | Woody marimba groove | `Clean` O1 Basic@0 · O2 Basic@0 s+24 L.35 | LP24 c84 e+.3 fv .3 · Click .3 | E2 0/.12/0/.1 · E3 0/.35/0/.3 | `Scale Set` · ARP · Pan Alt .4 | FX2 Reverb .25 |
| 171 | PL Vocal Blip | Random-vowel blips | `Clean` O1 Choir@0 · O2 Voice@10 L.4 | LP12 c100 | E3 0/.3/0/.2 | `Scale Set` · ARP · M: Random→O1Wave +1.0 | FX1 Delay .3 (1/8, PingP .8) |
| 172 | FX Noise Riser | Flanged noise riser | O1 Noise@50 L.3 · Static .6 | BP12 c45 r.5 e+.8 | E2 .7/.5/1/.5 · E3 .1/.5/1/.4 | — | FX1 Flanger .5 (Sync 1 bar) → FX2 Reverb .4 |
| 173 | FX Vox Stab Hit | Vocal minor-chord hit | `Clean` O1 Voice@40 · `Chord Min` | LP12 c100 | E1 0/.3/0/.2 · E3 0/.35/0/.4 | M: Wave Env→Pitch −.1 | FX1 Delay .4 (1/8d, Fdbk 55 %) → FX2 Reverb .35 |
| 174 | FX Glitch Tool | Stepped glitch texture | O1 Granite@20 | — | E3 0/.4/1/.2 | LFO1 S&H sync 1/16 · M: LFO1→O1Wave +.8 · M: LFO1→Pitch +.1 | FX1 Drive .3 (Crush .5) → FX2 Delay .25 (1/16) |
| 175 | FX Sub Drop | Two-octave sub drop | `Clean` O1 Basic@0 s−24 | LP24 c66 | E1 0/.6/0/.5 · E3 0/.6/0/.5 | M: Wave Env→Pitch +.5 | FX1 Drive .3 (Soft) |

---

## Bank: Deep House · 176–199 · 122 BPM · phrase `deep`

Warm round basses, minor-9 chords, tine keys, soft leads, dubby delays, swung 16ths
(15 %).

| Role | Preview sequence |
|------|------------------|
| bass | 2 bars warm syncopated bass, Cm then Fm |
| lead | 2-bar slow soulful melody, AT ramp in bar 2 |
| keys | Classic deep-house stabs (steps 3, 6, 10) with Cm9 then Fm9 voicings |
| pad | 4 bars Cm9–Abmaj7 (2 bars each), AT ramp |
| pluck | 2 bars 8th-note arpeggio over Cm9 then Fm9 |
| fx | One low note held 2 bars, AT ramp |

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 176 | BS Deep Round | Round sine-tri bass | `Clean` O1 Basic@0 · O2 Basic@20 L.3 | LP24 c58 e+.2 | E2 0/.3/0/.2 · E3 .01/.4/.7/.15 | Mono gl .06 | FX1 Comp .7 (Ratio 3:1) |
| 177 | BS Warm Ladder | Warm detuned ladder bass | O1 Basic@40 · O2 Basic@40 f−8 L1 | LP24 c50 r.3 e+.35 · Tube .2 pre | E2 0/.3/.2/.2 · E3 0/.4/.7/.15 | Leg gl .08 · `fv .4` | FX1 Comp .7 |
| 178 | BS Upright Deep | Plucked upright bass | `Clean` O1 Upright@20 v+.3 | LP24 c70 fv .3 | E3 0/.45/.3/.25 | Mono | FX1 EQ 1.0 (Low +2, Low Cut 35 Hz) |
| 179 | BS Sine Pluck | Sine pluck with pitch punch | `Clean` O1 Basic@0 · O2 Basic@0 s+12 L.25 | LP24 c80 | E1 0/.1/0/.1 · E3 0/.35/0/.25 | Mono · M: Wave Env→Pitch +.03 | FX1 Comp .6 |
| 180 | LD Flute Whisper | Breathy flute line | `Clean` O1 Flute@20 · O2 Breath@10 L.2 | LP12 c100 | E3 .1/.4/.8/.35 | Leg gl .08 · `Scale Set` · AT→Vib .5 | FX1 Delay .25 (1/4d, Fdbk 40 %) → FX2 Reverb .35 |
| 181 | LD Soft Sine Lead | Gliding soft sine | `Clean` O1 Basic@0 · O2 Basic@20 s+12 L.2 | — | E3 .05/.4/.8/.35 | Leg gl .15 · `Scale Set` · AT→Vib .5 | FX1 Delay .3 (1/8d) → FX2 Reverb .35 |
| 182 | LD Sax Breath | Smoky sax-like lead | `Clean` O1 Sax@20 v+.3 · O2 Breath@0 L.15 | LP24 c95 fv .4 | E3 .03/.4/.7/.3 | Leg gl .06 · `Scale Set` · AT→Wave +.3 | FX2 Reverb .35 |
| 183 | LD Glass Melody | Bell-glass melody | `Clean` O1 Glass@30 · O2 Bell@10 s+12 L.3 | — | E3 0/.5/.5/.4 | Leg gl .05 · `Scale Set` | FX1 Delay .3 (1/4d, PingP .6) → FX2 Reverb .3 |
| 184 | KY Deep Chord Stab | Dubby one-finger m7 stab | `Clean` O1 Stack@20 · O2 Organ@20 L.6 · `Chord m7` | LP24 c78 r.2 e+.25 | E2 0/.3/.1/.25 · E3 0/.35/.1/.3 | `Scale Set` | FX1 Delay .3 (1/4d, Fdbk 50 %, Tone dark) → FX2 Reverb .3 |
| 185 | KY Velvet Tines | Soft tremolo EP | `Clean` O1 EPiano@10 v+.4 · O2 Basic@0 L.4 · XM PM .08 | LP24 c85 fv .5 | E1 0/.4/0/.35 · E3 0/.55/.3/.35 | M: Wave Env→XMod +.25 · M: Velocity→XMod +.2 | FX1 Tremolo .3 (Stereo 90 %, Rate .45) → FX2 Reverb .3 |
| 186 | KY Deep Organ | Chorused deep organ | `Clean` O1 E-Organ@20 | LP24 c80 | E3 0/.3/.6/.2 | — | FX1 Chorus .3 (Rate .5) → FX2 Reverb .2 |
| 187 | KY Felt Piano Deep | Muted felt piano | `Clean` O1 Piano@15 v+.3 | LP24 c78 fv .5 | E3 0/.55/.2/.4 | Flav .2 | FX1 Comp .5 → FX2 Reverb .35 |
| 188 | PD Warm Strings | Warm ensemble strings | `Clean` O1 Strings@30 · O2 Cello@20 L.5 | LP24 c78 | E3 .45/.5/1/.6 | Pan Spread .6 | FX1 Chorus .3 (Ensemble) → FX2 Reverb .4 |
| 189 | PD Choir Mist | Misty breathing choir | `Clean` O1 Choir@30 · O2 Breath@20 L.3 | LP12 c90 | E3 .5/.5/1/.7 | LFO1 Sine .3 · M: LFO1→O1Wave +.3 · AT→Wave +.3 | FX2 Reverb .5 (Decay 6 s) |
| 190 | PD Analog Drift | Drifting detuned saws | O1 Basic@40 · O2 Basic@40 f+9 L1 | LP24 c72 r.15 | E3 .4/.5/1/.6 | LFO2 Rand .3 · M: LFO2→Pitch +.004 · Flav .3 · Pan Spread .5 | FX1 Chorus .3 → FX2 Reverb .4 |
| 191 | PD Sunset Pad | Evolving warm spectrum | `Clean` O1 Upper@10 · O2 Harmonic@20 L.6 | LP24 c85 | E3 .5/.5/1/.6 | LFO1 Tri .25 · M: LFO1→O1Wave +.6 | FX2 Shimmer .4 (Interval +7, Shimmer 30 %) |
| 192 | PL Deep Pluck | Dub-echo saw pluck | `Clean` O1 Basic@30 · O2 Basic@30 f+8 L.6 | LP24 c66 r.25 e+.4 | E2 0/.25/0/.2 · E3 0/.3/0/.3 | `Scale Set` · ARP | FX1 Delay .35 (1/4d, Fdbk 50 %, Tone dark) → FX2 Reverb .3 |
| 193 | PL Kalimba Deep | Kalimba with alternating pan | `Clean` O1 Bell@5 v+.2 | HP12 c40 | E3 0/.35/0/.3 | `Scale Set` · ARP · Pan Alt .5 | FX1 Delay .25 (1/8d) → FX2 Reverb .3 |
| 194 | PL Harp Ripple | Rippling harp | `Clean` O1 Overtone@20 v+.3 | LP24 c95 | E3 0/.45/0/.4 | `Scale Set` · ARP | FX2 Reverb .4 (Size .8) |
| 195 | PL Muted Pluck | Muted guitar-like pluck | `Clean` O1 Clavinet@10 | LP24 c75 e+.3 | E2 0/.15/0/.1 · E3 0/.2/0/.15 | `Scale Set` · ARP | FX1 Chorus .25 → FX2 Delay .2 (1/8d) |
| 196 | FX Vinyl Air | Airy vinyl hiss | O1 Breath@10 L.3 | HP12 c80 r.1 · Static .3 | E3 .5/.5/1/.6 | Flav .4 | FX2 Reverb .4 |
| 197 | FX Dub Siren | Quarter-note dub siren | `Clean` O1 Basic@0 | LP24 c100 | E3 0/.5/1/.4 | LFO1 Tri sync 1/4 · M: LFO1→Pitch +.1 | FX1 Delay .5 (1/4d, Fdbk 65 %, Tone dark) → FX2 Reverb .35 |
| 198 | FX Deep Swell | Swelling choir-strings | `Clean` O1 Choir@10 · O2 Strings@20 L.5 | LP24 c40 e+.7 | E2 .7/.5/1/.6 · E3 .6/.5/1/.7 | — | FX2 Shimmer .5 (Decay 8 s) |
| 199 | FX Drop Hit | Pitch-drop impact | `Clean` O1 Basic@0 s−12 | LP24 c90 · Static .5 | E1 0/.4/0/.3 · E3 0/.5/0/.5 | M: Wave Env→Pitch +.4 | FX1 Comp .8 → FX2 Reverb .45 |

---

## Bank: Minimal Techno · 200–223 · 128 BPM · phrase `minimal`

Short, dry, hypnotic: one-note basses, blips, dub chords, polyrhythmic
sequences, clicks and textures. No swing.

| Role | Preview sequence |
|------|------------------|
| bass | 2 bars hypnotic one-note 16th pattern with accents, Eb pickup in bar 2 |
| lead | 2 bars of sparse blips |
| keys | Cm7 dub-chord hits on steps 3, 11 and 19 |
| pad | Cm drone held 4 bars, AT ramp |
| pluck | 2 bars of a 3-note figure over straight 16ths (3-against-4 polyrhythm) |
| fx | One note held 2 bars, AT ramp |

| # | Name | Character | Oscillators | Filter / Drive / Dirt | Envelopes | Motion & Play | FX |
|---|------|-----------|-------------|-----------------------|-----------|---------------|----|
| 200 | BS Minimal Pulse | Short filtered pulse bass | `Clean` O1 Basic@20 | LP24 c62 r.3 e+.45 | E2 0/.15/0/.1 · E3 0/.2/0/.1 | Mono · `fv .5` | FX1 Comp .8 |
| 201 | BS Click Sub | Sub with transient click | `Clean` O1 Basic@0 | LP24 c70 · Click .3 | E3 0/.2/0/.1 | Mono | FX1 EQ 1.0 (Low +2, Low Cut 30 Hz) |
| 202 | BS Acid Minimal | Slow-breathing acid line | O1 Resonant@10 · O2 link L.4 | LP24 c45 r.65 e+.5 · Trans .3 pre | E2 0/.15/0/.1 · E3 0/.2/.4/.1 | Leg gl .08 · `fv .5` · LFO1 Tri sync 2 bars · M: LFO1→Cutoff +.12 | FX1 Delay .2 (1/16d, Fdbk 35 %) |
| 203 | BS FM Thump | Short FM thump | `Clean` O1 Basic@0 · O2 Basic@0 L0 · XM PM .2 | LP24 c72 | E1 0/.12/0/.1 · E3 0/.2/0/.12 | Mono · M: Wave Env→XMod +.5 | FX1 Comp .9 |
| 204 | LD Blip | Sine blip into ping-pong | `Clean` O1 Basic@0 · O2 Glass@10 s+12 L.3 | — | E3 0/.15/0/.2 | Mono · `Scale Set` | FX1 Delay .4 (1/8d, Fdbk 55 %, PingP .8) → FX2 Reverb .2 |
| 205 | LD Metal Ping | Metallic ping | O1 Metal@20 | HP12 c55 | E3 0/.2/0/.2 | Mono · `Scale Set` | FX1 Delay .35 (1/16d, Fdbk 50 %) |
| 206 | LD Sine Sweep Lead | Sine with bar-long sync swell | `Clean` O1 Basic@0 · O2 Basic@0 L0 · XM Sync .2 | LP24 c90 | E3 .02/.3/.7/.25 | Leg gl .1 · `Scale Set` · LFO1 Sine sync 1 bar · M: LFO1→XMod +.2 | FX1 Delay .3 (1/4d) |
| 207 | LD Digital Chirp | Random digital chirps | O1 Digital@30 we−.4 | LP24 c100 | E1 0/.15/0/.1 · E3 0/.2/0/.15 | Mono · `Scale Set` · M: Random→O1Wave +.4 | FX1 Delay .3 (1/16, PingP .7) |
| 208 | KY Dub Chord | Classic dub-techno chord | `Clean` O1 Basic@40 · O2 Basic@40 f+7 L.8 · `Chord m7` | LP24 c62 r.25 e+.35 | E2 0/.25/0/.2 · E3 0/.25/0/.3 | `Scale Set` | FX1 Delay .5 (1/4d, Fdbk 65 %, Tone dark) → FX2 Reverb .35 |
| 209 | KY Hypno Stab | Band-passed comb stab | `Clean` O1 Comb@20 · `Chord Min` | BP12 c80 r.3 | E3 0/.2/0/.15 | `Scale Set` | FX1 Delay .35 (1/8d, Fdbk 50 %) → FX2 Reverb .2 |
| 210 | KY Glass Chord | Open sus2 glass chord | `Clean` O1 Glass@20 · `Chord sus2` | — | E3 0/.4/0/.35 | `Scale Set` | FX1 Delay .3 (1/8d) → FX2 Reverb .3 |
| 211 | KY Organ Pulse | 16th-gated organ | O1 Organ@30 | LP24 c80 | E3 0/.2/.3/.1 | — | FX1 Tremolo .6 (Square, Sync 1/16, Depth 70 %) → FX2 Reverb .2 |
| 212 | PD Minimal Drone | 4-bar breathing drone | `Clean` O1 Basic@40 · O2 Basic@40 f+5 L1 | LP24 c55 r.35 | E3 .5/.5/1/.6 | LFO1 Sine sync 4 bars · M: LFO1→Cutoff +.2 | FX2 Reverb .5 (Decay 6 s) |
| 213 | PD Grain Fog | Grainy fog | O1 Granular@10 · O2 Granular@30 f+9 L.6 | LP24 c88 · Crackle .15 | E3 .5/.5/1/.7 | LFO1 S&H .88 · M: LFO1→O1Wave +.35 · LFO2 S&H .84 · M: LFO2→Amp Level +.35 · M: LFO2→O2Wave +.3 · Flav .5 | FX1 Phaser .3 → FX2 Reverb .5 |
| 214 | PD Cold Organ | Cold scanning organ | `Clean` O1 E-Organ@50 | HP12 c40 | E3 .4/.5/1/.6 | LFO1 Tri sync 2 bars · M: LFO1→O1Wave +.3 | FX2 Reverb .45 |
| 215 | PD Metallic Bed | Flanged metal bed | O1 Metal@30 · O2 Comb@10 L.5 | BP12 c75 r.2 | E3 .5/.5/1/.7 | LFO1 Sine .2 · M: LFO1→O1Wave +.4 | FX1 Flanger .3 (Sync 2 bars) → FX2 Reverb .45 |
| 216 | PL Polyrhythm Seq | Hypnotic filtered sequence | `Clean` O1 Basic@50 | LP24 c70 r.4 e+.45 | E2 0/.18/0/.1 · E3 0/.22/0/.12 | `Scale Set` · ARP · LFO1 Tri sync 2 bars · M: LFO1→Cutoff +.2 | FX1 Delay .3 (1/16d, Fdbk 45 %) |
| 217 | PL Wood Tick | Dry woody tick | `Clean` O1 Overtone@40 | HP12 c60 | E3 0/.12/0/.1 | `Scale Set` · ARP | FX1 Delay .25 (1/8d) → FX2 Reverb .2 |
| 218 | PL Bit Seq | Crushed bit sequence | O1 Crushed@20 | LP24 c90 r.3 e+.3 | E2 0/.15/0/.1 · E3 0/.2/0/.12 | `Scale Set` · ARP | FX1 Drive .2 (Crush .4) → FX2 Delay .3 (1/16) |
| 219 | PL Rim Pluck | Rim-like PD pluck | `Clean` O1 PhaseDist@30 we−.5 | — | E1 0/.12/0/.1 · E3 0/.15/0/.12 | `Scale Set` · ARP · Pan Alt .5 | FX1 Delay .3 (1/8d, PingP .7) |
| 220 | FX Static Sweep | Synced static sweep | O1 Noise@30 L.2 | BP12 c60 r.5 · Static .6 | E3 .3/.5/1/.4 | LFO1 Sine sync 2 bars · M: LFO1→Cutoff +.4 | FX1 Delay .3 (1/16d) → FX2 Reverb .3 |
| 221 | FX Geiger Groove | Ticking geiger texture | O1 Basic@0 L0 | HP12 c60 · Geiger .8 | E3 0/.5/1/.3 | — | FX1 Delay .4 (1/16, Fdbk 50 %, PingP .8) |
| 222 | FX Robot Tick | Ring-mod robot ticks | O1 Digital@10 | Ring .5 | E3 0/.4/1/.2 | LFO1 S&H sync 1/16 · M: LFO1→O1Wave +.7 · M: LFO1→Pan +.5 | FX1 Tremolo .8 (Square, Sync 1/16) → FX2 Delay .2 (1/16) |
| 223 | FX Tunnel | Slow flanged tunnel | `Clean` O1 Comb@40 · O2 Breath@20 L.5 | LP24 c45 r.4 e+.5 | E2 .7/.5/1/.6 · E3 .5/.5/1/.6 | LFO1 Tri sync 4 bars · M: LFO1→O1Wave +.5 | FX1 Flanger .4 (Sync 4 bars) → FX2 Reverb .5 (Decay 8 s) |
