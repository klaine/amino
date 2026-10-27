"""Amino effects (docs/11): parameter tables and pure-Python DSP for the emulator.

Knob values (fxN_p1..p6, 0..1) map to display values through the ranges in docs/11 §4.
APPROX: Drive uses first-order ADAA instead of 2x oversampling, one shared
pitch shifter for Shimmer instead of one per feedback line.
"""
import math

SR = 44100
TAU = 2 * math.pi

DELAY_SYNC = ["Off", "1/32", "1/16T", "1/16", "1/16d", "1/8T", "1/8", "1/8d", "1/4T", "1/4", "1/4d", "1/2", "1 bar"]
TREM_SYNC = ["Off", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar"]
FLANGE_SYNC = ["Off", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars"]


class P:
    """One p-slot: name, aliases, curve ('lin'|'exp'|'enum'), range or options, display default."""

    def __init__(self, name, curve, lo=None, hi=None, default=None, options=None, aliases=(), unit=""):
        self.name, self.curve, self.lo, self.hi = name, curve, lo, hi
        self.options, self.default, self.unit = options, default, unit
        self.aliases = {name.lower(), *[a.lower() for a in aliases]}

    def to_display(self, x):
        x = min(1.0, max(0.0, x))
        if self.curve == "enum":
            return self.options[int(round(x * (len(self.options) - 1)))]
        if self.curve == "exp":
            return self.lo * (self.hi / self.lo) ** x
        return self.lo + (self.hi - self.lo) * x

    def to_knob(self, v):
        if self.curve == "enum":
            return self.options.index(v) / max(1, len(self.options) - 1)
        if self.curve == "exp":
            return math.log(v / self.lo) / math.log(self.hi / self.lo)
        return (v - self.lo) / (self.hi - self.lo)


FX_DEFS = {
    "Chorus": [P("Rate", "exp", .05, 5, .6, unit="Hz"), P("Depth", "lin", 0, 100, 40, unit="%"),
               P("Delay", "lin", 2, 30, 8, unit="ms"), P("Feedback", "lin", -90, 90, 0, aliases=["Fdbk"], unit="%"),
               P("Spread", "lin", 0, 180, 90), P("Mode", "enum", options=["Chorus", "Ensemble"], default="Chorus")],
    "Flanger": [P("Rate", "exp", .02, 10, .25, unit="Hz"), P("Depth", "lin", 0, 100, 60, unit="%"),
                P("Feedback", "lin", -95, 95, 50, aliases=["Fdbk"], unit="%"), P("Delay", "exp", .1, 10, 1.5, unit="ms"),
                P("Spread", "lin", 0, 180, 90), P("Sync", "enum", options=FLANGE_SYNC, default="Off")],
    "Phaser": [P("Rate", "exp", .02, 10, .3, unit="Hz"), P("Depth", "lin", 0, 100, 60, unit="%"),
               P("Feedback", "lin", -95, 95, 40, aliases=["Fdbk"], unit="%"),
               P("Stages", "enum", options=[4, 6, 8, 12], default=6),
               P("Center", "exp", 100, 4000, 700, unit="Hz"), P("Spread", "lin", 0, 180, 90)],
    "Tremolo": [P("Rate", "exp", .1, 20, 4, unit="Hz"), P("Depth", "lin", 0, 100, 50, unit="%"),
                P("Shape", "enum", options=["Sine", "Tri", "Square", "SawUp", "SawDn", "Pump"], default="Sine"),
                P("Sync", "enum", options=TREM_SYNC, default="Off"), P("Stereo", "lin", 0, 100, 0, unit="%"),
                P("Phase", "lin", 0, 360, 0)],
    "Drive": [P("Gain", "lin", 0, 36, 12, unit="dB"), P("Tone", "lin", -6, 6, 0, unit="dB"),
              P("Type", "enum", options=["Soft", "Tube", "Hard", "Fold", "Crush"], default="Soft"),
              P("Output", "lin", -24, 0, -6, aliases=["Out"], unit="dB"),
              P("Low Cut", "exp", 20, 500, 20, aliases=["LowCut"], unit="Hz"), P("Crush", "lin", 0, 1, .3)],
    "Comp": [P("Threshold", "lin", -40, 0, -18, aliases=["Thresh"], unit="dB"), P("Ratio", "exp", 1, 20, 4),
             P("Attack", "exp", .1, 100, 10, aliases=["Atk"], unit="ms"),
             P("Release", "exp", 10, 1000, 150, aliases=["Rel"], unit="ms"),
             P("Makeup", "lin", 0, 24, 6, unit="dB"), P("Knee", "lin", 0, 12, 6, unit="dB")],
    "EQ": [P("Low", "lin", -15, 15, 0, unit="dB"), P("Mid", "lin", -15, 15, 0, unit="dB"),
           P("Mid Freq", "exp", 200, 8000, 1000, aliases=["MidF"], unit="Hz"), P("High", "lin", -15, 15, 0, unit="dB"),
           P("Mid Q", "exp", .3, 4, .9, aliases=["Q"]), P("Low Cut", "exp", 20, 300, 20, aliases=["LowCut"], unit="Hz")],
    "Delay": [P("Time", "exp", 1, 2000, 350, unit="ms"), P("Sync", "enum", options=DELAY_SYNC, default="1/8d"),
              P("Feedback", "lin", 0, 100, 40, aliases=["Fdbk"], unit="%"), P("Tone", "lin", -1, 1, -.2),
              P("Ping-Pong", "lin", 0, 100, 0, aliases=["PingP"], unit="%"), P("Mod", "lin", 0, 100, 10, unit="%")],
    "Reverb": [P("Size", "lin", 0, 100, 60, unit="%"), P("Decay", "exp", .2, 20, 2.5, unit="s"),
               P("Damping", "lin", 0, 100, 50, aliases=["Damp"], unit="%"),
               P("Pre-Delay", "lin", 0, 200, 20, aliases=["PreD"], unit="ms"),
               P("Mod", "lin", 0, 100, 30, unit="%"), P("Width", "lin", 0, 100, 100, unit="%")],
    "Shimmer": [P("Size", "lin", 0, 100, 70, unit="%"), P("Decay", "exp", .2, 20, 6, unit="s"),
                 P("Damping", "lin", 0, 100, 40, aliases=["Damp"], unit="%"),
                 P("Shimmer", "lin", 0, 100, 40, aliases=["Shim"], unit="%"),
                 P("Interval", "enum", options=[-12, 5, 7, 12, 19, 24], default=12, aliases=["Intvl"]),
                 P("Mod", "lin", 0, 100, 40, unit="%")],
    "Trance Gate": [P("Pattern", "enum", options=["Classic 8ths", "Offbeat 8ths", "16th Pulse",
                                                      "32nd Stutter", "Rolling Trance", "Broken Rave",
                                                      "Long Gate"], default="Classic 8ths"),
                    P("Timing", "enum", options=["1/8", "1/16", "1/32"], default="1/16"),
                    P("Attack", "lin", 0, 50, 2, unit="ms"),
                    P("Release", "lin", 0, 50, 8, unit="ms"),
                    P("Gate Length", "lin", 0, 100, 60, aliases=["Length"], unit="%"),
                    P("Floor", "lin", 0, 100, 0, unit="%")],
}
FX_TYPES = ["Off", *FX_DEFS]
TIME_FX = {"Chorus", "Flanger", "Phaser", "Delay", "Reverb", "Shimmer"}


def default_knobs(fx_type):
    return [p.to_knob(p.default) for p in FX_DEFS[fx_type]]


def div_seconds(div, bpm):
    div = str(div).strip()
    parts = div.split()
    if len(parts) == 2 and parts[1].startswith("bar"):
        beats = 4.0 * float(parts[0])
    else:
        mod = 1.0
        if div.endswith("T"):
            mod, div = 2 / 3, div[:-1]
        elif div.endswith("d"):
            mod, div = 1.5, div[:-1]
        n, d = div.split("/")
        beats = 4.0 * float(n) / float(d) * mod
    return beats * 60.0 / bpm


# ------------------------------------------------------------------ building blocks

class DelayLine:
    __slots__ = ("buf", "size", "w")

    def __init__(self, seconds):
        self.size = int(seconds * SR) + 4
        self.buf = [0.0] * self.size
        self.w = 0

    def write(self, x):
        self.buf[self.w] = x
        self.w += 1
        if self.w >= self.size:
            self.w = 0

    def read(self, delay):
        """Fractional read `delay` samples behind the last write (cubic Hermite, docs/11 §4.1)."""
        pos = self.w - 1 - delay
        while pos < 0:
            pos += self.size
        i = int(pos)
        f = pos - i
        buf, n = self.buf, self.size
        x0 = buf[i]
        x1 = buf[i + 1 if i + 1 < n else 0]
        if f == 0.0:
            return x0
        if delay < 2.0:  # x2 would be the not-yet-written slot
            return x0 + (x1 - x0) * f
        xm1 = buf[i - 1]
        x2 = buf[(i + 2) % n]
        c1 = 0.5 * (x1 - xm1)
        c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2
        c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
        return ((c3 * f + c2) * f + c1) * f + x0


def one_pole_coef(fc):
    return 1.0 - math.exp(-TAU * min(fc, SR * 0.45) / SR)


class Biquad:
    __slots__ = ("b0", "b1", "b2", "a1", "a2", "z1", "z2")

    def __init__(self, kind, f, gain_db=0.0, q=0.707):
        A = 10 ** (gain_db / 40)
        w = TAU * f / SR
        cw, sw = math.cos(w), math.sin(w)
        al = sw / (2 * q)
        if kind == "peak":
            b0, b1, b2, a0, a1, a2 = 1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A
        elif kind == "lowshelf":
            s = 2 * math.sqrt(A) * al
            b0 = A * ((A + 1) - (A - 1) * cw + s)
            b1 = 2 * A * ((A - 1) - (A + 1) * cw)
            b2 = A * ((A + 1) - (A - 1) * cw - s)
            a0 = (A + 1) + (A - 1) * cw + s
            a1 = -2 * ((A - 1) + (A + 1) * cw)
            a2 = (A + 1) + (A - 1) * cw - s
        elif kind == "highshelf":
            s = 2 * math.sqrt(A) * al
            b0 = A * ((A + 1) + (A - 1) * cw + s)
            b1 = -2 * A * ((A - 1) + (A + 1) * cw)
            b2 = A * ((A + 1) + (A - 1) * cw - s)
            a0 = (A + 1) - (A - 1) * cw + s
            a1 = 2 * ((A - 1) - (A + 1) * cw)
            a2 = (A + 1) - (A - 1) * cw - s
        else:  # highpass
            b0, b1, b2, a0, a1, a2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al
        self.b0, self.b1, self.b2 = b0 / a0, b1 / a0, b2 / a0
        self.a1, self.a2 = a1 / a0, a2 / a0
        self.z1 = self.z2 = 0.0

    def __call__(self, x):
        y = self.b0 * x + self.z1
        self.z1 = self.b1 * x - self.a1 * y + self.z2
        self.z2 = self.b2 * x - self.a2 * y
        return y


# ------------------------------------------------------------------ effects

class Effect:
    insert = False          # linear dry/wet blend instead of equal-power

    def __init__(self, v, bpm):
        self.v, self.bpm = v, bpm

    def tail(self):
        return 0.1


class Chorus(Effect):
    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.lines = [DelayLine(0.08), DelayLine(0.08)]
        self.voices = 3 if v["Mode"] == "Ensemble" else 2
        self.phase = 0.0

    def process(self, L, R, t0):
        v = self.v
        rate, depth, base = v["Rate"], v["Depth"] / 100, v["Delay"] / 1000 * SR
        fb, spread = v["Feedback"] / 100, math.radians(v["Spread"])
        nv = self.voices
        offs = [k * TAU / nv for k in range(nv)]
        lines = self.lines
        inc = rate / SR
        for i in range(len(L)):
            ph = self.phase * TAU
            out = []
            for c, line in enumerate(lines):
                acc = 0.0
                for o in offs:
                    acc += line.read(base * (1 + 0.8 * depth * math.sin(ph + o + c * spread)))
                out.append(acc / nv)
            lines[0].write(L[i] + fb * out[0])
            lines[1].write(R[i] + fb * out[1])
            L[i], R[i] = out
            self.phase = (self.phase + inc) % 1.0


class Flanger(Effect):
    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.lines = [DelayLine(0.03), DelayLine(0.03)]
        self.phase = 0.0

    def process(self, L, R, t0):
        v = self.v
        rate = v["Rate"] if v["Sync"] == "Off" else 1.0 / div_seconds(v["Sync"], self.bpm)
        depth, fb, base = v["Depth"] / 100, v["Feedback"] / 100, v["Delay"] / 1000 * SR
        spread = math.radians(v["Spread"])
        inc = rate / SR
        for i in range(len(L)):
            ph = self.phase * TAU
            yl = self.lines[0].read(1 + base * (1 + 0.95 * depth * math.sin(ph)))
            yr = self.lines[1].read(1 + base * (1 + 0.95 * depth * math.sin(ph + spread)))
            self.lines[0].write(L[i] + fb * yl)
            self.lines[1].write(R[i] + fb * yr)
            L[i], R[i] = yl, yr
            self.phase = (self.phase + inc) % 1.0


class Phaser(Effect):
    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.n = int(v["Stages"])
        self.z = [[0.0] * self.n, [0.0] * self.n]
        self.last = [0.0, 0.0]
        self.phase = 0.0

    def process(self, L, R, t0):
        v = self.v
        depth, fb, center = v["Depth"] / 100, v["Feedback"] / 100, v["Center"]
        spread = math.radians(v["Spread"])
        inc = v["Rate"] / SR
        bufs = (L, R)
        for i in range(len(L)):
            ph = self.phase * TAU
            for c in (0, 1):
                f = center * 2 ** (2 * depth * math.sin(ph + c * spread))
                t = math.tan(math.pi * min(f, 18000) / SR)
                a = (t - 1) / (t + 1)
                x = bufs[c][i] + fb * self.last[c]
                z = self.z[c]
                for k in range(self.n):
                    y = a * x + z[k]
                    z[k] = x - a * y
                    x = y
                self.last[c] = x
                bufs[c][i] = x
            self.phase = (self.phase + inc) % 1.0


class Tremolo(Effect):
    insert = True

    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.g = [1.0, 1.0, 1.0, 1.0]
        self.slew = one_pole_coef(1 / (TAU * 0.001))   # 2 x ~1 ms poles: no clicks on Square/Saw/Pump

    def process(self, L, R, t0):
        v = self.v
        depth, shape, stereo = v["Depth"] / 100, v["Shape"], v["Stereo"] / 100
        if v["Sync"] == "Off":
            period = 1.0 / v["Rate"]
        else:
            period = div_seconds(v["Sync"], self.bpm)
        if shape == "Pump" and v["Sync"] == "Off":
            period = div_seconds("1/4", self.bpm)
        offset = v["Phase"] / 360
        sg, k = self.g, self.slew
        for i in range(len(L)):
            t = t0 + i / SR
            for c in (0, 1):
                p = (t / period + offset + c * 0.5 * stereo) % 1.0
                if shape == "Pump":
                    g = 1 - depth * math.exp(-p / 0.18)
                else:
                    if shape == "Sine":
                        u = 0.5 - 0.5 * math.cos(TAU * p)
                    elif shape == "Tri":
                        u = 1 - abs(2 * p - 1)
                    elif shape == "Square":
                        u = 1.0 if p < 0.5 else 0.0
                    elif shape == "SawUp":
                        u = p
                    else:
                        u = 1 - p
                    g = 1 - depth * (1 - u)
                sg[c] += (g - sg[c]) * k
                sg[c + 2] += (sg[c] - sg[c + 2]) * k
            L[i] *= sg[2]
            R[i] *= sg[3]


class TranceGate(Effect):
    insert = True

    PATTERNS = (0x3333, 0xCCCC, 0x5555, 0x1111, 0xDB6D, 0x6D35, 0x0FFF)
    STEP_BEATS = (0.5, 0.25, 0.125)

    def process(self, L, R, t0):
        v = self.v
        step_beats = self.STEP_BEATS[("1/8", "1/16", "1/32").index(v["Timing"])]
        cycle_beats = step_beats * 16
        pattern = self.PATTERNS[("Classic 8ths", "Offbeat 8ths", "16th Pulse",
                                 "32nd Stutter", "Rolling Trance", "Broken Rave",
                                 "Long Gate").index(v["Pattern"])]
        step_seconds = step_beats * 60 / self.bpm
        open_seconds = step_seconds * v["Gate Length"] / 100
        attack, release = v["Attack"] / 1000, v["Release"] / 1000
        floor = v["Floor"] / 100
        for i in range(len(L)):
            phase = ((t0 + i / SR) * self.bpm / 60 / cycle_beats) % 1
            position = phase * 16
            step = int(position)
            elapsed = (position - step) * step_seconds
            gain = floor
            if pattern & (1 << step) and elapsed < open_seconds:
                env = 1.0
                if attack > 0 and elapsed < attack:
                    env = elapsed / attack
                if release > 0 and open_seconds - elapsed < release:
                    env = min(env, (open_seconds - elapsed) / release)
                gain += (1 - floor) * min(1.0, max(0.0, env))
            L[i] *= gain
            R[i] *= gain


LN2 = math.log(2.0)


def _logcosh(x):
    ax = abs(x)
    return ax + math.log1p(math.exp(-2.0 * ax)) - LN2


# (shaper, antiderivative) pairs for first-order ADAA
SHAPERS = {
    "Soft": (math.tanh, _logcosh),
    "Tube": (lambda x: math.tanh(x) if x > 0 else math.tanh(0.6 * x) / 0.6 * 0.8,
             lambda x: _logcosh(x) if x > 0 else 0.8 / 0.36 * _logcosh(0.6 * x)),
    "Hard": (lambda x: 1.0 if x > 1 else (-1.0 if x < -1 else x),
             lambda x: abs(x) - 0.5 if abs(x) > 1 else 0.5 * x * x),
    "Fold": (math.sin, lambda x: -math.cos(x)),
}


class Drive(Effect):
    """Waveshaper with first-order antiderivative anti-aliasing (ADAA) instead of oversampling."""
    insert = True

    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.hp = [0.0, 0.0]
        self.lp = [0.0, 0.0]
        self.hold = [0.0, 0.0]
        self.xp = [0.0, 0.0]
        self.Fp = [0.0, 0.0]
        self.cnt = 0.0

    def process(self, L, R, t0):
        v = self.v
        g = 10 ** (v["Gain"] / 20)
        out = 10 ** (v["Output"] / 20)
        typ = v["Type"]
        hpc = one_pole_coef(v["Low Cut"])
        tilt = v["Tone"]
        lo_g, hi_g = 10 ** (-tilt / 40), 10 ** (tilt / 40)
        tc = one_pole_coef(1000)
        crush = v["Crush"]
        rate = (2000 / SR) ** crush
        levels = 2 ** (16 - 12 * crush) / 2
        f_, F_ = SHAPERS.get(typ, (None, None))
        xp, Fp = self.xp, self.Fp
        bufs = (L, R)
        for i in range(len(L)):
            if typ == "Crush":
                self.cnt += rate
                take = self.cnt >= 1.0
                if take:
                    self.cnt -= 1.0
            for c in (0, 1):
                x = bufs[c][i]
                self.hp[c] += (x - self.hp[c]) * hpc
                x = (x - self.hp[c]) * g
                if f_:
                    Fx = F_(x)
                    dx = x - xp[c]
                    y = (Fx - Fp[c]) / dx if abs(dx) > 1e-5 else f_(0.5 * (x + xp[c]))
                    xp[c], Fp[c] = x, Fx
                    x = y
                else:
                    if take:
                        self.hold[c] = round(math.tanh(x) * levels) / levels
                    x = self.hold[c]
                self.lp[c] += (x - self.lp[c]) * tc
                bufs[c][i] = (self.lp[c] * lo_g + (x - self.lp[c]) * hi_g) * out


class Comp(Effect):
    insert = True

    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.env = -120.0

    def process(self, L, R, t0):
        v = self.v
        thr, ratio, knee = v["Threshold"], v["Ratio"], v["Knee"]
        att = math.exp(-1 / (v["Attack"] / 1000 * SR))
        rel = math.exp(-1 / (v["Release"] / 1000 * SR))
        makeup = v["Makeup"]
        slope = 1 - 1 / ratio
        for i in range(len(L)):
            lvl = max(abs(L[i]), abs(R[i]), 1e-6)
            db = 20 * math.log10(lvl)
            c = att if db > self.env else rel
            self.env = db + (self.env - db) * c
            over = self.env - thr
            if over > knee / 2:
                gr = over * slope
            elif over > -knee / 2 and knee > 0:
                gr = slope * (over + knee / 2) ** 2 / (2 * knee)
            else:
                gr = 0.0
            g = 10 ** ((makeup - gr) / 20)
            L[i] *= g
            R[i] *= g


class EQ(Effect):
    insert = True

    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        self.chains = []
        for _ in (0, 1):
            ch = []
            if v["Low Cut"] > 21:
                ch.append(Biquad("hp", v["Low Cut"]))
            if abs(v["Low"]) > 0.05:
                ch.append(Biquad("lowshelf", 120, v["Low"]))
            if abs(v["Mid"]) > 0.05:
                ch.append(Biquad("peak", v["Mid Freq"], v["Mid"], v["Mid Q"]))
            if abs(v["High"]) > 0.05:
                ch.append(Biquad("highshelf", 8000, v["High"]))
            self.chains.append(ch)

    def process(self, L, R, t0):
        for buf, ch in zip((L, R), self.chains):
            for f in ch:
                for i in range(len(buf)):
                    buf[i] = f(buf[i])


class Delay(Effect):
    def __init__(self, v, bpm):
        super().__init__(v, bpm)
        t = v["Time"] / 1000 if v["Sync"] == "Off" else div_seconds(v["Sync"], bpm)
        while t > 2.0:
            t /= 2
        self.t = t * SR
        self.lines = [DelayLine(2.1), DelayLine(2.1)]
        self.f = [0.0, 0.0]
        self.phase = 0.0

    def tail(self):
        fb = self.v["Feedback"] / 100
        if fb <= 0.01:
            return self.t / SR + 0.1
        return min(5.0, self.t / SR * math.log(0.001) / math.log(min(fb, 0.99)))

    def process(self, L, R, t0):
        v = self.v
        fb, pp, mod, tone = v["Feedback"] / 100, v["Ping-Pong"] / 100, v["Mod"] / 100, v["Tone"]
        lp = tone < 0
        c = one_pole_coef(20000 * (1000 / 20000) ** (-tone)) if lp else one_pole_coef(20 * 50 ** tone)
        wow_depth = mod * 0.002 * SR
        for i in range(len(L)):
            d = self.t + wow_depth * math.sin(TAU * self.phase)
            self.phase = (self.phase + 0.5 / SR) % 1.0
            yl, yr = self.lines[0].read(d), self.lines[1].read(d)
            outs = []
            for k, y in enumerate((yl, yr)):
                self.f[k] += (y - self.f[k]) * c
                outs.append(self.f[k] if lp else y - self.f[k])
            fl, fr = outs
            mono = 0.5 * (L[i] + R[i])
            inl = L[i] * (1 - pp) + mono * pp
            inr = R[i] * (1 - pp)
            self.lines[0].write(math.tanh(inl + fb * ((1 - pp) * fl + pp * fr)))
            self.lines[1].write(math.tanh(inr + fb * ((1 - pp) * fr + pp * fl)))
            L[i], R[i] = yl, yr


class Reverb(Effect):
    BASE = [1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116]
    DIFF = [142, 107, 379, 277]
    DIFF_R = [151, 113, 389, 293]
    MOD_RATE = [0.61, 0.83, 0.71, 0.97, 0.53, 0.89, 0.67, 0.77]   # Hz, one LFO per line

    def __init__(self, v, bpm, shimmer=False):
        super().__init__(v, bpm)
        self.shimmer = shimmer
        scale = 0.6 + 1.4 * v["Size"] / 100
        self.len = [int(b * scale) for b in self.BASE]
        self.lines = [DelayLine(L / SR + 0.01) for L in self.len]
        self.diff = [[DelayLine(d / SR + 0.001) for d in ds] for ds in (self.DIFF, self.DIFF_R)]
        self.pre = [DelayLine(0.21), DelayLine(0.21)]
        self.damp = [0.0] * 8
        self.mph = [k / 8 for k in range(8)]
        self.freeze = v["Decay"] >= 19.99
        t60 = max(0.2, v["Decay"])
        self.gain = [1.0 if self.freeze else 10 ** (-3 * L / (t60 * SR)) for L in self.len]
        damp = v["Damping"] / 100
        self.dc = one_pole_coef(500 + 19500 * (1 - damp) ** 2)
        if shimmer:
            self.sbuf = DelayLine(0.1)
            self.sphase = 0.0
            self.sratio = 2 ** (v["Interval"] / 12)
            self.slp = self.shp = 0.0
            self.samt = 0.0 if self.freeze else v["Shimmer"] / 100

    def tail(self):
        return 6.0 if self.freeze else min(6.0, self.v["Decay"] * 0.8 + 0.3)

    def process(self, L, R, t0):
        v = self.v
        pre = v["Pre-Delay"] / 1000 * SR if "Pre-Delay" in v else 0.02 * SR
        width = v["Width"] / 100 if "Width" in v else 1.0
        mod = v["Mod"] / 100 * 12
        lines, lens, gains, dl = self.lines, self.len, self.gain, self.damp
        dc, freeze = self.dc, self.freeze
        win = 0.08 * SR
        mph, minc = self.mph, [r / SR for r in self.MOD_RATE]
        for i in range(len(L)):
            xs = []
            for ch, (src, pl, chain, taps) in enumerate(((L, self.pre[0], self.diff[0], self.DIFF),
                                                          (R, self.pre[1], self.diff[1], self.DIFF_R))):
                pl.write(src[i])
                x = 0.0 if freeze else pl.read(pre)
                for d, n in zip(chain, taps):
                    y = d.read(n - 1)
                    w = x + 0.6 * y
                    d.write(w)
                    x = y - 0.6 * w
                xs.append(x)
            o = []
            for k in range(8):
                o.append(lines[k].read(lens[k] - 1 + mod * math.sin(TAU * mph[k])))
                mph[k] = (mph[k] + minc[k]) % 1.0
            if not freeze:
                for k in range(8):
                    dl[k] += (o[k] - dl[k]) * dc
                    o[k] = dl[k]
            a, b, c, d_, e, f, g, h = o
            a, b, c, d_, e, f, g, h = a + b, a - b, c + d_, c - d_, e + f, e - f, g + h, g - h
            a, b, c, d_, e, f, g, h = a + c, b + d_, a - c, b - d_, e + g, f + h, e - g, f - h
            a, b, c, d_, e, f, g, h = a + e, b + f, c + g, d_ + h, a - e, b - f, c - g, d_ - h
            mix = (a, b, c, d_, e, f, g, h)
            shim = 0.0
            if self.shimmer and self.samt > 0:
                self.sbuf.write(0.25 * (o[0] + o[1] + o[2] + o[3]))
                self.sphase = (self.sphase + (1 - self.sratio) / win) % 1.0
                p1, p2 = self.sphase, (self.sphase + 0.5) % 1.0
                s = (self.sbuf.read(p1 * win) * math.sin(math.pi * p1) ** 2
                     + self.sbuf.read(p2 * win) * math.sin(math.pi * p2) ** 2)
                self.slp += (s - self.slp) * 0.6
                self.shp += (self.slp - self.shp) * 0.02
                shim = math.tanh((self.slp - self.shp) * 1.5) * self.samt
            for k in range(8):
                fbk = mix[k] * 0.35355339 * gains[k]
                if k < 4:
                    fbk = fbk * (1 - 0.5 * self.samt) + shim if self.shimmer else fbk
                lines[k].write(fbk + xs[k & 1])
            l_out = (o[0] + o[2] + o[4] + o[6]) * 0.3
            r_out = (o[1] + o[3] + o[5] + o[7]) * 0.3
            mid, side = 0.5 * (l_out + r_out), 0.5 * (l_out - r_out) * width
            L[i], R[i] = mid + side, mid - side


def make_effect(fx_type, knobs, bpm):
    if fx_type == "Off" or fx_type not in FX_DEFS:
        return None
    v = {p.name: p.to_display(k) for p, k in zip(FX_DEFS[fx_type], knobs)}
    cls = {"Chorus": Chorus, "Flanger": Flanger, "Phaser": Phaser, "Tremolo": Tremolo,
           "Trance Gate": TranceGate,
           "Drive": Drive, "Comp": Comp, "EQ": EQ, "Delay": Delay}.get(fx_type)
    if cls:
        return cls(v, bpm)
    return Reverb(v, bpm, shimmer=(fx_type == "Shimmer"))


class FxSlot:
    def __init__(self, fx_type, knobs, mix, bpm):
        self.fx = make_effect(fx_type, knobs, bpm)
        self.type = fx_type
        self.mix = mix

    def process(self, L, R, t0):
        """Returns the slot output (dry/wet blended); input lists are not modified."""
        if self.fx is None:
            return L, R
        wl, wr = list(L), list(R)
        self.fx.process(wl, wr, t0)
        m = self.mix
        if self.fx.insert:
            dg, wg = 1 - m, m
        else:
            dg, wg = math.cos(m * math.pi / 2), math.sin(m * math.pi / 2)
        return ([d * dg + w * wg for d, w in zip(L, wl)], [d * dg + w * wg for d, w in zip(R, wr)])

    def tail(self):
        return self.fx.tail() if self.fx else 0.0
