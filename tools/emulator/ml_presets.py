"""Multi-layer presets (docs/09 §12, docs/10 §12), encoded in the same notation.

Layer fields: base (preset index whose layer 1 is copied), n (notation), pan (fixed),
vol, key (lo, hi), route. Global fields: mode, voices, phrase, fx (FX notation).
"""

ML_PRESETS = {
    55: dict(name="ML Round Robin Keys", mode="RoundRb", phrase="rr", fx="FX1 Chorus .25", layers=[
        dict(base=29, pan=-0.2),
        dict(base=29, n="O1 Bell@12 · E3 0/.5/.3/.35", pan=0.2),
        dict(n="O1 Glass@30 · O2 Basic@0 L.6 · E3 0/.5/.2/.35", pan=0.0),
    ]),
    56: dict(name="ML Random Pluck Cloud", mode="RandRR", phrase="pluck",
             fx="FX1 Delay .3 (sync 1/8, PingP .7) · FX2 Reverb .4", layers=[
        dict(base=36, route="FX1"), dict(base=37, n="octave +1", route="FX1"),
        dict(base=39, route="FX2"), dict(base=40, route="FX2"),
    ]),
    57: dict(name="ML Strings & Air", mode="Stack", phrase="pad", fx="FX1 Chorus .3 · FX2 Reverb .4", layers=[
        dict(base=22, vol=0.8, route="FX1"),
        dict(n="O1 Breath@20 · octave +1 · HP12 c60 · E3 .5/.5/1/.6", vol=0.3, route="FX2"),
    ]),
    58: dict(name="ML Bass / Lead Split", mode="Split", phrase="split", fx="FX1 Delay .2", layers=[
        dict(base=3, key=(0, 59), route="Dry"), dict(base=10, n="Leg", key=(60, 127), route="FX1"),
    ]),
    59: dict(name="ML Octave Organ", mode="Stack", phrase="keys", fx="FX1 Phaser .35", layers=[
        dict(base=28), dict(n="O1 Odd@10 L.5 · octave +1 · E3 0/.3/1/.3"),
    ]),
    60: dict(name="ML Unison Monster", mode="Stack", phrase="lead", fx="FX1 Delay .2", layers=[
        dict(n=f"O1 Basic@63 f{f:+d} · O2 Stack@10 f{f:+d} L.5 · LP24 c85 r.2 e+.3 · gl .1 · Tube .3 pre · Leg",
             pan=p) for f, p in ((-12, -0.8), (-4, -0.3), (4, 0.3), (12, 0.8))
    ]),
    61: dict(name="ML Fifth Stab", mode="Stack", phrase="keys", fx="FX2 Reverb .3", layers=[
        dict(n="O1 Resonant@25 we-.4 · LP24 c90 e+.4 · E1/E2/E3 0/.25/0/.2"),
        dict(n="O1 Resonant@25 we-.4 s+7 · LP24 c90 e+.4 · E1/E2/E3 0/.25/0/.2"),
    ]),
    62: dict(name="ML Byte Kit", mode="Split", phrase="drums", fx="FX1 Comp .8", layers=[
        dict(n="O1 Basic@0 · M: Wave Env→Pitch +.8 · E1 0/.15/0/.1 · E3 0/.25/0/.2", key=(36, 37)),
        dict(n="O1 Basic@20 L.4 · Static .8 · BP12 c85 · E3 0/.2/0/.2", key=(38, 41)),
        dict(n="O1 Basic@0 L0 · Static 1.0 · HP24 c110 · E3 0/.1/0/.1 · M: Key→E3 Decay +.3", key=(42, 47)),
        dict(n="O1 Metal@20 · Geiger .2 · E3 0/.25/0/.2", key=(48, 60)),
    ]),
    63: dict(name="ML Ping-Pong Pair", mode="RoundRb", phrase="pluck", fx="FX1 Delay .25 (1/16, PingP .5)", layers=[
        dict(base=38, pan=-0.7, route="FX1"), dict(base=31, pan=0.7, route="FX1"),
    ]),
    119: dict(name="ML Supersaw Wall", mode="Stack", phrase="pad", voices=16,
              fx="FX1 Chorus .3 → FX2 Reverb .35", layers=[
        dict(n="Clean · O1 Stack@40 f-8 · LP24 c100 · E3 .2/.5/1/.5", pan=-0.5),
        dict(n="Clean · O1 Stack@44 f+8 · LP24 c100 · E3 .2/.5/1/.5", pan=0.5),
    ]),
    120: dict(name="ML Vocal Chop RR", mode="RandRR", phrase="pluck",
              fx="FX1 Delay .25 (1/8) → FX2 Reverb .25", layers=[
        dict(n=f"{o} · E3 0/.25/0/.2 · Scale Set", route="FX1") for o in
        ("Clean · O1 Choir@10", "Clean · O1 FormantA@30", "Clean · O1 FormantB@45", "O1 Breath@20 L.8")
    ]),
    121: dict(name="ML Keys/Sub Split", mode="Split", phrase="split",
              fx="FX1 Tremolo .3 (Stereo 80 %)", layers=[
        dict(base=65, key=(0, 47), route="Dry"), dict(base=93, key=(48, 127), route="FX1"),
    ]),
    122: dict(name="ML Pluck & Pad", mode="Stack", phrase="pluck",
              fx="FX1 Delay .3 (1/8d) · FX2 Reverb .45", layers=[
        dict(base=100, vol=0.8, route="FX1"), dict(base=83, vol=0.4, route="FX2"),
    ]),
    123: dict(name="ML Modern Kit", mode="Split", phrase="drums", fx="FX1 Comp .8 (Ratio 6:1)", layers=[
        dict(n="Clean · O1 Basic@0 · M: Wave Env→Pitch +.6 · E1 0/.12/0/.1 · E3 0/.45/0/.3", key=(36, 37)),
        dict(n="O1 Basic@0 L0 · Burst .8 · BP12 c90 r.3 · E3 0/.25/0/.25", key=(38, 41)),
        dict(n="O1 Basic@0 L0 · Static 1.0 · HP24 c115 · E3 0/.08/0/.08 · M: Key→E3 Decay +.3", key=(42, 47)),
        dict(n="Clean · O1 PhaseDist@20 we-.5 · E1 0/.15/0/.1 · E3 0/.2/0/.2", key=(48, 60)),
    ]),
    124: dict(name="ML MPE Duo", mode="Stack", phrase="pad", fx="FX2 Reverb .45", layers=[
        dict(n="Clean · O1 Choir@0 · AT→Wave +.5", vol=0.8),
        dict(n="Clean · O1 Glass@20 · octave +1 · AT→Wave +.4", vol=0.4),
    ]),
    125: dict(name="ML Wide Ping-Pong", mode="RoundRb", phrase="pluck",
              fx="FX1 Delay .25 (1/16, Ping-Pong .5) → FX2 Reverb .25", layers=[
        dict(base=102, pan=-0.7, route="FX1"),
        dict(n="Clean · O1 Bell@8 · E3 0/.3/0/.3 · Scale Set", pan=0.7, route="FX1"),
    ]),
    126: dict(name="ML Chord Stab Wide", mode="Stack", phrase="keys", fx="FX1 Comp .7 → FX2 Reverb .3", layers=[
        dict(n="Clean · O1 Organ@40 · Chord m7 · E3 0/.3/0/.25 · Scale Set", vol=0.8),
        dict(n="Clean · O1 Stack@30 · Chord m7 · octave +1 · Pan Spread .8 · Scale Set", vol=0.5),
    ]),
    127: dict(name="ML Reese & Sub", mode="Stack", phrase="bass", fx="FX1 Chorus .2", layers=[
        dict(n="Clean · O1 Basic@0 · octave -1 · LP24 c50 · Mono", route="Dry"),
        dict(base=68, n="HP12 c40 · Mono", route="FX1"),
    ]),
}

INIT_MODERN = "Clean · O1 Basic@63 · O2 Basic@63 f+7 L.6 · LP24 c110 r.1"
INIT_MODERN_FX = "FX1 Comp .5 (Thresh -14, Ratio 2:1) → FX2 Reverb .2"
