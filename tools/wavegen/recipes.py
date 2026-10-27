"""Amino wavetable recipes.

Procedural tables (0-31) are original Amino content (MIT).
Tables 32-47 are built from AKWF-FREE single-cycle waves (CC0-1.0).
Tables 48-57 (expansion) are procedural again (MIT).
Tables 58-67 (v0.5) are procedural (MIT), 68-73 come from AKWF-FREE (CC0-1.0).
Each procedural recipe maps t in 0..1 (slot 0..60) to one float waveform.
"""
import math
import random

from wtlib import H, N, SLOTS, additive, align, analyze, bandlimit, harmonics, lerp

TAU = 2 * math.pi


def saw_amps(k):
    return 1.0 / k


def sq_amps(k):
    return 1.0 / k if k % 2 else 0.0


def tri_amps(k):
    return (1.0 / (k * k)) * (1 if (k // 2) % 2 == 0 else -1) if k % 2 else 0.0


SINE = additive([1.0])
TRI = additive(harmonics(tri_amps))
SAW = additive(harmonics(saw_amps))
SQUARE = additive(harmonics(sq_amps))


def segments(t, shapes):
    """Piecewise-linear morph through a list of waveforms."""
    pos = t * (len(shapes) - 1)
    i = min(int(pos), len(shapes) - 2)
    return lerp(shapes[i], shapes[i + 1], pos - i)


def gauss(x, w):
    return math.exp(-(x / w) ** 2)


# ---------------------------------------------------------------- 0-15 (v0.1)

def basic(t):
    return segments(t, [SINE, TRI, SAW, SQUARE])


def pulse(t):
    d = 0.5 - 0.48 * t
    return additive(harmonics(lambda k: math.sin(math.pi * k * d) / k),
                    [math.pi / 2] * H)


def resonant(t):
    h = 2 + 40 * t
    return additive(harmonics(lambda k: (1 / k) * (1 + 6 * gauss(k - h, 1.5))))


def resonant2(t):
    h = 3 + 47 * t
    return additive(harmonics(lambda k: sq_amps(k) * (1 + 6 * gauss(k - h, 2.0))
                              + 0.4 * gauss(k - h, 1.0)))


def sync(t):
    r = 1 + 7 * t
    return bandlimit(lambda p: 2 * ((p * r) % 1.0) - 1)


VOWELS = {  # F1, F2, F3 in Hz
    "a": (730, 1090, 2440), "e": (530, 1840, 2480), "i": (270, 2290, 3010),
    "o": (570, 840, 2410), "u": (300, 870, 2240),
}


F4 = (3300, 250)  # fixed upper formant ("singer's formant" region)


def resonance(f, F, B):
    """Band-pass resonator magnitude: 1 at F, -3 dB at F +/- B/2, 6 dB/oct skirts."""
    return f * B / math.hypot(F * F - f * f, f * B)


def formant_wave(f0, formants, bw=(80, 90, 120), gain=(1.0, 0.5, 0.25, 0.15), tilt=1.2,
                 floor=0.05):
    """Parallel formant vowel: a glottal-like floor plus band-pass formants on every harmonic."""
    res = list(zip(formants, bw)) + [F4]

    def amp(k):
        f = k * f0
        return (floor + sum(g * resonance(f, F, B) for (F, B), g in zip(res, gain))) / k ** tilt
    return additive(harmonics(amp))


def vowel_path(t, order):
    pos = t * (len(order) - 1)
    i = min(int(pos), len(order) - 2)
    f = pos - i
    a, b = VOWELS[order[i]], VOWELS[order[i + 1]]
    return [x + (y - x) * f for x, y in zip(a, b)]


def formant_a(t):
    return formant_wave(110, vowel_path(t, "aeiou"))


def formant_b(t):
    f0 = 110 + 86 * t
    return formant_wave(f0, vowel_path(t, "uoae"), bw=(110, 150, 200))


def harmonic(t):
    n = 1 + 31 * t
    return additive(harmonics(lambda k: (1 / math.sqrt(k)) * max(0.0, min(1.0, n - k + 1))))


def odd(t):
    n = 1 + 62 * t
    return additive(harmonics(lambda k: (1 / k ** 0.7) * max(0.0, min(1.0, n - k + 1))
                              if k % 2 else 0.0))


def bell(t):
    ratios = [1, 2, 3, 4, 5, 7]
    pos = t * (len(ratios) - 1)
    i = min(int(pos), len(ratios) - 2)
    f = pos - i
    idx = 1 + 4 * f
    a = bandlimit(lambda p: math.sin(TAU * p + idx * math.sin(TAU * ratios[i] * p)))
    b = bandlimit(lambda p: math.sin(TAU * p + idx * math.sin(TAU * ratios[i + 1] * p)))
    return lerp(a, b, f)


DRAWBARS = [  # 16', 8'(2), 5 1/3'(3), 4'(4), 2 2/3'(6), 2'(8), 1 3/5'(10), 1 1/3'(12), 1'(16)
    "800000000", "888000000", "868000000", "888800000", "808400000",
    "888600004", "868868008", "800000888", "888888888",
]
DRAWBAR_HARM = [1, 2, 3, 4, 6, 8, 10, 12, 16]


def organ(t):
    pos = t * (len(DRAWBARS) - 1)
    i = min(int(pos), len(DRAWBARS) - 2)
    f = pos - i
    amps = [0.0] * H
    for reg, w in ((DRAWBARS[i], 1 - f), (DRAWBARS[i + 1], f)):
        for d, h in zip(reg, DRAWBAR_HARM):
            amps[h - 1] += w * int(d) / 8.0
    return additive(amps)


def digital(t):
    steps = max(4, int(round(64 - 60 * t)))
    bits = 8 - 6 * t
    levels = 2 ** bits / 2
    base = lerp(SINE, SAW, min(1.0, 2 * t))
    out = []
    for i in range(len(base)):
        v = base[(i * steps // len(base)) * len(base) // steps]
        out.append(round(v * levels) / levels)
    return out


_NOISE = random.Random(12)
NOISE_PHASES = [_NOISE.uniform(0, TAU) for _ in range(H)]
NOISE_JITTER = [_NOISE.uniform(0.4, 1.0) for _ in range(H)]


def noise(t):
    alpha = 2.5 - 2.5 * t
    return additive(harmonics(lambda k: NOISE_JITTER[k - 1] / k ** alpha), NOISE_PHASES)


def saw_fn(p):
    return 2 * (p % 1.0) - 1


def pwm_saw(t):
    d = 0.5 * t
    return bandlimit(lambda p: saw_fn(p) + saw_fn(p + d))


def fold(t):
    g = 1 + 9 * t
    return bandlimit(lambda p: math.sin(g * math.sin(TAU * p)))


def steps(t):
    n = int(round(2 + 30 * t))
    return [math.sin(TAU * (math.floor(i * n / 128) + 0.5) / n) for i in range(128)]


# ---------------------------------------------------------------- 16-31 (v0.2)

def strings(t):
    return additive(harmonics(lambda k: (1 / k ** (1.0 + 1.2 * t))
                              * (1 + 0.6 * math.sin(0.7 * k))))


def brass(t):
    return additive(harmonics(lambda k: math.exp(-k / (1 + 12 * t)) * (1.15 if k % 2 else 1.0)))


def bass_spectra(t):
    h = 1 + 6 * t
    return additive(harmonics(lambda k: 1 / k ** 2 + 0.8 * gauss(k - h, 1.5)))


def choir(t):
    return formant_wave(220, vowel_path(t, "aoue"), bw=(100, 130, 170))


GLASS_PARTIALS = [1, 2, 5, 9, 14, 20, 27, 35, 44, 54]


def glass(t):
    amps = [0.0] * H
    for idx, k in enumerate(GLASS_PARTIALS):
        amps[k - 1] = (0.5 if idx == 0 else 0.0) + gauss(idx - 9 * t, 2.0) / (1 + 0.05 * k)
    return additive(amps)


def chaos(t):
    r = 3.5 + 0.5 * t
    x = 0.3
    for _ in range(500):
        x = r * x * (1 - x)
    # 32 values per cycle keep periodic orbits below the harmonic limit
    seq = []
    for _ in range(32):
        x = r * x * (1 - x)
        seq.append(2 * x - 1)
    return bandlimit(lambda p: seq[int(p * 32) % 32])


def comb(t):
    period = 2 + 14 * t
    return additive(harmonics(lambda k: (1 / k) * (0.5 + 0.5 * math.cos(TAU * k / period))))


def phase_dist(t):
    d = 0.5 - 0.48 * t

    def fn(p):
        q = 0.5 * p / d if p < d else 0.5 + 0.5 * (p - d) / (1 - d)
        return -math.cos(TAU * q)
    return bandlimit(fn)


def fractal(t):
    a = 0.3 + 0.5 * t
    amps = [0.0] * H
    for n in range(6):
        amps[2 ** n - 1] = a ** n
    return additive(amps, [math.pi / 2] * H)


def upper(t):
    c = 2 + 53 * t
    return additive(harmonics(lambda k: (0.3 if k == 1 else 0.0) + gauss(k - c, 3.0)))


def clav(t):
    d = 0.08 + 0.3 * t
    return additive(harmonics(lambda k: math.sin(math.pi * k * d) / k * (1 + k / 12)),
                    [math.pi / 2] * H)


_BREATH = random.Random(27)
BREATH_PHASES = [_BREATH.uniform(0, TAU) for _ in range(H)]
BREATH_JITTER = [_BREATH.uniform(0.6, 1.0) for _ in range(H)]


def breath(t):
    # whispered vowel: flat source, broad formants, fundamental region suppressed
    res = list(zip(vowel_path(t, "aou"), (300, 350, 450))) + [(3300, 600), (5000, 1500)]
    gain = (0.6, 0.8, 0.7, 0.6, 0.5)

    def amp(k):
        f = k * 110
        hp = (f / 600) ** 2 / (1 + (f / 600) ** 2)
        env = 0.15 + sum(g * resonance(f, F, B) for (F, B), g in zip(res, gain))
        return BREATH_JITTER[k - 1] * hp * env
    return additive(harmonics(amp), BREATH_PHASES)


def sq_sync(t):
    r = 1 + 7 * t
    return bandlimit(lambda p: 1.0 if (p * r) % 1.0 < 0.5 else -1.0)


BAR_MODES = [1.0, 2.76, 5.40, 8.93, 13.34]


def metal(t):
    s = 3 + 5 * t
    amps = [0.0] * H
    for r in BAR_MODES:
        k = int(round(r * s))
        if 1 <= k <= H:
            amps[k - 1] += 1 / r
    return additive(amps)


_STACK = random.Random(30)
STACK_SPREAD = [_STACK.uniform(-1, 1) for _ in range(H)]


def stack(t):
    return additive(harmonics(saw_amps), [TAU * 0.5 * t * STACK_SPREAD[k] * (k + 1) ** 0.5
                                          for k in range(H)])


def granite(t):
    rng = random.Random(31)
    segs = int(round(4 + 60 * t))
    bits = 4 - 3 * t
    levels = max(1.0, 2 ** bits / 2)
    vals = [round(rng.uniform(-1, 1) * levels) / levels for _ in range(segs)]
    return [vals[i * segs // 128] for i in range(128)]


PROCEDURAL = [
    ("Basic", basic), ("Pulse", pulse), ("Resonant", resonant), ("Resonant 2", resonant2),
    ("Sync", sync), ("Formant A", formant_a), ("Formant B", formant_b), ("Harmonic", harmonic),
    ("Odd", odd), ("Bell", bell), ("Organ", organ), ("Digital", digital),
    ("Noise", noise), ("PWM Saw", pwm_saw), ("Fold", fold), ("Steps", steps),
    ("Strings", strings), ("Brass", brass), ("Bass Spectra", bass_spectra), ("Choir", choir),
    ("Glass", glass), ("Chaos", chaos), ("Comb", comb), ("PhaseDist", phase_dist),
    ("Fractal", fractal), ("Upper", upper), ("Clav", clav), ("Breath", breath),
    ("SqSync", sq_sync), ("Metal", metal), ("Stack", stack), ("Granite", granite),
]

# (name, AKWF folder, keyframe count). Keyframes are sorted dark -> bright.
AKWF_TABLES = [
    ("Cello", "AKWF_cello", 8), ("Violin", "AKWF_violin", 8),
    ("Voice", "AKWF_hvoice", 8), ("EPiano", "AKWF_epiano", 8),
    ("Clavinet", "AKWF_clavinet", 8), ("E-Organ", "AKWF_eorgan", 8),
    ("FM", "AKWF_fmsynth", 8), ("Chip", "AKWF_oscchip", 8),
    ("VGame", "AKWF_vgame", 8), ("Sax", "AKWF_altosax", 8),
    ("Flute", "AKWF_flute", 8), ("Overtone", "AKWF_overtone", 8),
    ("Crushed", "AKWF_bitreduced", 8), ("Granular", "AKWF_granular", 8),
    ("Upright", "AKWF_dbass", 8), ("Piano", "AKWF_piano", 8),
]


# ---------------------------------------------------------------- 48-57 (expansion)

def even(t):
    n = 2 + 61 * t

    def amp(k):
        if k == 1:
            return 1.0
        if k % 2:
            return 0.0
        return max(0.0, min(1.0, (n - k) / 2 + 1)) / k ** 0.7
    return additive(harmonics(amp))


def partials(t):
    pos = 1 + 31 * t
    k0 = int(pos)
    f = pos - k0
    amps = [0.0] * H
    amps[0] += 0.6
    amps[k0 - 1] += 1 - f
    if k0 < H:
        amps[k0] += f
    return additive(amps)


def feedback(t):
    # operator self-feedback (two-sample average, as in classic FM chips)
    beta = 1.8 * t
    m = N * 8
    y1 = y2 = 0.0
    for _ in range(3):
        cycle = []
        for n in range(m):
            y = math.sin(TAU * n / m + beta * 0.5 * (y1 + y2))
            y2, y1 = y1, y
            cycle.append(y)
    return additive(*align(*analyze(cycle)))


def window(t):
    r = 1 + 15 * t
    return bandlimit(lambda p: (1 - p) * (1 - math.cos(TAU * r * p)) / 2)


def rave(t):
    d = 0.04 + 0.42 * t
    return bandlimit(lambda p: 0.7 * saw_fn(p) + saw_fn(2 * p) + saw_fn(2 * p + d))


OCTAVE_REGS = [  # weights of saws at 1x, 2x, 4x, 8x
    (1, 0, 0, 0), (1, .7, 0, 0), (1, .8, .5, 0), (.6, 1, .7, 0), (.4, .8, 1, .5), (0, .6, 1, .8),
]


def _octave_shape(reg):
    amps = [0.0] * H
    for w, n in zip(reg, (1, 2, 4, 8)):
        for k in range(n, H + 1, n):
            amps[k - 1] += w * n / k
    return additive(amps)


OCTAVE_SHAPES = [_octave_shape(r) for r in OCTAVE_REGS]


def octaves(t):
    return segments(t, OCTAVE_SHAPES)


def logic(t):
    # bitwise i XOR (i*r mod 128); naive like Digital/Steps/Granite
    pos = t * 14
    i0 = min(int(pos), 13)

    def wave(r):
        return [(i ^ ((i * r) & 127)) / 63.5 - 1 for i in range(128)]
    return lerp(wave(2 + i0), wave(3 + i0), pos - i0)


def reed(t):
    e = 0.05 + 0.95 * t   # even-harmonic share: cylindrical -> conical bore
    hf = 4 + 8 * t
    return additive(harmonics(lambda k: (1.0 if k % 2 else e) / k ** (1.3 - 0.4 * t)
                              * (1 + 2.5 * t * gauss(k - hf, 2.5))))


def pluck(t):
    p = 0.5 - 0.46 * t    # pluck position along the string
    return additive(harmonics(lambda k: math.sin(math.pi * k * p) / k ** 1.6))


def sweep(t):
    fc = 60 ** t          # cutoff in harmonics, exponential like a filter knob

    def amp(k):
        x = k / fc
        return (1 / k) / math.sqrt(1 + x ** 8) * (1 + 1.5 * gauss(k - fc, 0.5 + 0.1 * fc))
    return additive(harmonics(amp))


EXPANSION = [
    ("Even", even), ("Partials", partials), ("Feedback", feedback), ("Window", window),
    ("Rave", rave), ("Octaves", octaves), ("Logic", logic), ("Reed", reed),
    ("Pluck", pluck), ("Sweep", sweep),
]


# ---------------------------------------------------------------- 58-67 (v0.5, procedural)

_CHORD = random.Random(58)
CHORD_PHASES = [_CHORD.uniform(0, TAU) for _ in range(H)]
# Chord tones as harmonic numbers: 4/8/16 = root, 3/6/12/24 = fifth, 5/10 = major
# third, 19 = minor third, 15 = major 7th, 28 = minor 7th, 9/18/36 = ninth.
CHORD_VOICINGS = [
    (2, 3, 4, 6),               # power (root + fifth)
    (4, 5, 6, 8),               # major
    (8, 10, 12, 15),            # maj7
    (8, 9, 12, 16),             # sus2
    (16, 19, 24, 32),           # minor
    (16, 19, 24, 28),           # m7
    (16, 19, 24, 28, 36),       # m9
]


def _chord_shape(tones):
    amps = [0.0] * H
    for k, a in ((1, 1.0), (2, 0.5)):   # root body keeps the played pitch
        amps[k - 1] += a
    for k in tones:                      # each tone is a short saw: k, 2k, 3k
        for m in (1, 2, 3):
            if k * m <= H:
                amps[k * m - 1] += 0.55 / m
    return additive(amps, CHORD_PHASES)


CHORD_SHAPES = [_chord_shape(v) for v in CHORD_VOICINGS]


def chord(t):
    return segments(t, CHORD_SHAPES)


def vosim(t):
    # VOSIM: a burst of decaying sin^2 pulses, then silence; pulse width sets the formant
    h = 2 * 12 ** t                       # formant at harmonic 2 -> 24
    w = 1 / h
    count = max(1, min(8, int(0.85 / w)))

    def fn(p):
        i = int(p / w)
        if i >= count:
            return 0.0
        return 0.8 ** i * math.sin(math.pi * (p - i * w) / w) ** 2
    return bandlimit(fn, 8)


def _scanned_frames():
    # closed ring of 128 masses, uneven springs, struck once; frames are snapshots in time
    rng = random.Random(60)
    springs = [0.18 + 0.22 * rng.random() for _ in range(N)]
    x = [math.exp(-((i - 32) / 5.0) ** 2) - 0.6 * math.exp(-((i - 80) / 9.0) ** 2) for i in range(N)]
    v = [0.0] * N
    frames = []
    for s in range(SLOTS):
        frames.append(list(x))
        for _ in range(5):
            acc = [springs[i] * (x[i - 1] - x[i]) + springs[(i + 1) % N] * (x[(i + 1) % N] - x[i])
                   - 0.002 * x[i] for i in range(N)]
            v = [v[i] + acc[i] - 0.03 * (2 * v[i] - v[i - 1] - v[(i + 1) % N]) for i in range(N)]
            x = [x[i] + v[i] for i in range(N)]
    return [additive(*analyze(f)) for f in frames]


SCANNED = _scanned_frames()


def scanned(t):
    return SCANNED[round(t * (SLOTS - 1))]


PRIMES = [1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61]


def prime(t):
    # fundamental plus prime-numbered harmonics only, added one by one
    n = 2 + 17 * t
    amps = [0.0] * H
    for i, k in enumerate(PRIMES):
        amps[k - 1] = max(0.0, min(1.0, n - i + 1)) / k ** 0.6
    return additive(amps)


def sine_sync(t):
    # sine slave reset by the master; some master sine keeps the pitch at whole ratios
    r = 1 + 7 * t
    return bandlimit(lambda p: 0.65 * math.sin(TAU * r * p) + 0.35 * math.sin(TAU * p), 8)


MALLET_BARS = [  # (harmonic, amplitude): tuned-bar spectra rounded to whole harmonics
    ((1, 1.0), (4, 0.2), (10, 0.04)),                              # soft marimba
    ((1, 1.0), (4, 0.5), (10, 0.22)),                              # marimba
    ((1, 1.0), (2, 0.06), (4, 0.6), (10, 0.12)),                   # vibes
    ((1, 1.0), (6, 0.45), (13, 0.18)),                             # kalimba
    ((1, 1.0), (3, 0.6), (6, 0.3), (9, 0.18)),                     # xylophone
    ((1, 1.0), (2, 0.6), (3, 0.45), (4, 0.3), (6, 0.2)),           # steel pan
    ((1, 1.0), (3, 0.5), (5, 0.45), (9, 0.4), (14, 0.25), (20, 0.12)),  # glockenspiel
]


def _bar_shape(bar):
    amps = [0.0] * H
    for k, a in bar:
        amps[k - 1] = a
    return additive(amps)


MALLET_SHAPES = [_bar_shape(b) for b in MALLET_BARS]


def mallet(t):
    return segments(t, MALLET_SHAPES)


def _pulse_fn(d):
    return lambda p: 1.0 if p % 1.0 < d else -1.0


MIXER_REGS = [  # (saw, pulse, pulse width, triangle) at 2x, then the sub square at 1x
    (1.0, 0.0, 0.5, 0.0, 0.5),
    (1.0, 0.7, 0.25, 0.0, 0.5),
    (0.0, 1.0, 0.25, 0.0, 0.7),
    (0.0, 1.0, 0.5, 0.0, 0.45),
    (0.0, 0.0, 0.5, 1.0, 0.8),
    (1.0, 1.0, 0.5, 0.0, 1.0),
    (1.0, 0.6, 0.12, 0.0, 0.9),
]


def _mixer_shape(reg):
    saw, pul, width, tri, sub = reg
    pf = _pulse_fn(width)

    def fn(p):
        q = (2 * p) % 1.0
        return (saw * saw_fn(q) + pul * pf(q)
                + tri * (1 - 4 * abs(q - 0.5)) + sub * (1.0 if p < 0.5 else -1.0))
    return bandlimit(fn, 8)


MIXER_SHAPES = [_mixer_shape(r) for r in MIXER_REGS]


def mixer(t):
    # polysynth oscillator mixer: main oscillators at 2x so the sub square sits on the played note
    return segments(t, MIXER_SHAPES)


def sub(t):
    # psychoacoustic sub: sine plus low harmonics that small speakers can reproduce
    g = t ** 0.8
    return additive(harmonics(lambda k: 1.0 if k == 1 else (0.75 * g ** (0.5 * (k - 1)) / k ** 0.8
                                                            if k <= 8 else 0.0)),
                    [0.0 if k % 2 else math.pi / 2 for k in range(1, H + 1)])


def notch(t):
    n = 2 * 30 ** t
    w = 0.6 + 0.08 * n
    return additive(harmonics(lambda k: (1 / k) * (1 - 0.97 * gauss(k - n, w))))


def drive(t):
    g = 40 ** t
    b = 0.12 * t                           # slight bias adds even harmonics as it drives

    def fn(p):
        return math.tanh(g * (math.sin(TAU * p) + b)) / math.tanh(g)
    return bandlimit(fn, 8)


V05 = [
    ("Chord", chord), ("Vosim", vosim), ("Scanned", scanned), ("Prime", prime),
    ("SineSync", sine_sync), ("Mallet", mallet), ("Mixer", mixer), ("Sub", sub),
    ("Notch", notch), ("Drive", drive),
]

# ---------------------------------------------------------------- 68-73 (v0.5, AKWF-FREE)

AKWF_V05 = [
    ("E-Bass", "AKWF_ebass", 8), ("E-Guitar", "AKWF_eguitar", 8),
    ("Distorted", "AKWF_distorted", 8), ("Theremin", "AKWF_theremin", 8),
    ("Raw", "AKWF_raw", 8), ("Drawn", "AKWF_hdrawn", 8),
]
