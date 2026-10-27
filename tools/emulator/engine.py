"""Amino reference engine (dry): pure-Python model of docs/03 + docs/04.

Approximations vs. the planned C engine are marked APPROX. FX are not rendered.
"""
import math

from amino_params import CHORDS, GLOBAL_DEFAULTS, LAYER_DEFAULTS
from fx import FxSlot, default_knobs

SR = 44100
BLOCK = 16                       # control rate (docs/03 §10)
BPM = 120.0
TAU = 2 * math.pi
PITCH_RANGE = 48.0               # semitones for amount 1.0 on pitch targets
CUTOFF_RANGE = 72.0              # semitones for amount 1.0 on cutoff
WAVE_RANGE = 63.0                # wave slots for amount 1.0 on wave targets
PM_RANGE = 1.5                   # Osc1 phase offset in cycles at XMod 1.0 (curve: amount²)
SYNC_RANGE = 36.0                # semitones Osc1 (slave) rises at XMod 1.0 in Sync
VOICE_GAIN = 0.9
SCALE_MINOR = (0, 2, 3, 5, 7, 8, 10)   # "Scale Set" assumes the set is C minor in previews


class XorShift32:
    """Deterministic random stream shared with the native DSP implementation."""

    __slots__ = ("state",)

    def __init__(self, seed):
        self.state = int(seed) & 0xFFFFFFFF or 0x01234567

    def next_u32(self):
        x = self.state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF or 0x01234567
        return self.state

    def random(self):
        return (self.next_u32() & 0xFFFFFF) / 16777215.0

    def bipolar(self):
        return (self.next_u32() & 0xFFFFFF) * (2.0 / 16777215.0) - 1.0

    def choice(self, seq):
        return seq[self.next_u32() % len(seq)]


def env_time(x):
    return max(0.0005, 20.0 * max(0.0, x) ** 3)        # docs/03 §8


def lfo_hz(x):
    return 0.01 * 5000.0 ** min(max(x, 0.0), 1.2)      # docs/03 §9


def glide_time(x):
    return 5.0 * x * x


def div_beats(div):
    div = div.strip()
    m = div.split()
    if len(m) == 2 and m[1].startswith("bar"):
        return 4.0 * float(m[0])
    mod = 1.0
    if div.endswith("T"):
        mod, div = 2.0 / 3.0, div[:-1]
    elif div.endswith("d"):
        mod, div = 1.5, div[:-1]
    num, den = div.split("/")
    return 4.0 * float(num) / float(den) * mod


def note_hz(n):
    return 440.0 * 2.0 ** ((n - 69.0) / 12.0)


def wave_slot(base, modulated):
    """Modulation scans the table content (0-60); classic slots 61-63 only via the base knob."""
    if base >= 61:
        return min(63, base)
    return min(60, max(0, int(modulated)))


def quantize_scale(note):
    pc = note % 12
    while pc not in SCALE_MINOR:
        note -= 1
        pc = note % 12
    return note


# ------------------------------------------------------------------ shapers

def shaper(kind, x):
    if kind == "Trans":
        return math.tanh(x + 0.2) - 0.19738
    if kind == "Tube":
        return math.tanh(x) if x > 0 else math.tanh(0.6 * x) / 0.6 * 0.8
    if kind == "Diode":
        return 1.0 if x > 1.0 else (-1.0 if x < -1.0 else x)
    if kind == "Fold":
        return math.sin(x)
    return x


# ------------------------------------------------------------------ envelope

class Env:
    __slots__ = ("stage", "level", "a", "d", "s", "r")

    def __init__(self):
        self.stage, self.level = "off", 0.0

    def gate_on(self, a, d, s, r):
        self.a, self.d, self.s, self.r = a, d, s, r
        self.stage = "a"

    def gate_off(self):
        if self.stage != "off":
            self.stage = "r"

    def step(self, n, a_mod=0.0, d_mod=0.0, r_mod=0.0):
        st = self.stage
        if st == "a":
            t = env_time(self.a + a_mod) / 1.466     # overshoot target 1.3 reaches 1.0 at t
            self.level += (1.3 - self.level) * (1.0 - math.exp(-n / (t * SR)))
            if self.level >= 1.0:
                self.level, self.stage = 1.0, "d"
        elif st == "d":
            t = env_time(self.d + d_mod) / 4.6
            self.level += (self.s - self.level) * (1.0 - math.exp(-n / (t * SR)))
        elif st == "r":
            t = env_time(self.r + r_mod) / 4.6
            self.level -= self.level * (1.0 - math.exp(-n / (t * SR)))
            if self.level < 1e-4:
                self.level, self.stage = 0.0, "off"
        return self.level


# ------------------------------------------------------------------ LFO

class Lfo:
    __slots__ = ("phase", "prev", "cur", "rng", "value")

    def __init__(self, rng, phase=0.0):
        self.phase, self.rng = phase, rng
        self.prev, self.cur = rng.bipolar(), rng.bipolar()
        self.value = 0.0

    def step(self, hz, shape, n):
        p = self.phase + hz * n / SR
        if p >= 1.0:
            p -= math.floor(p)
            self.prev, self.cur = self.cur, self.rng.bipolar()
        self.phase = p
        if shape == "Sine":
            v = math.sin(TAU * p)
        elif shape == "Tri":
            v = 4 * p if p < 0.25 else (2 - 4 * p if p < 0.75 else 4 * p - 4)
        elif shape == "Pulse":
            v = 1.0 if p < 0.5 else -1.0
        elif shape == "SawDn":
            v = 1.0 - 2.0 * p
        elif shape == "SawUp":
            v = 2.0 * p - 1.0
        elif shape == "Rand":
            v = self.prev + (self.cur - self.prev) * p
        else:
            v = self.cur
        self.value = v
        return v


# ------------------------------------------------------------------ layer & voice

class Layer:
    """Resolved per-layer params (docs/04 keys)."""

    def __init__(self, params, tables, table_index):
        p = dict(LAYER_DEFAULTS)
        p.update(params)
        self.p = p
        self.t1 = tables[table_index[p["osc1_table"]]]
        self.link = p["osc2_link"] == "On"
        self.t2 = self.t1 if self.link else tables[table_index[p["osc2_table"]]]
        self.mods = [(p[f"m{i}_src"], p[f"m{i}_dst"], float(p[f"m{i}_amt"]))
                     for i in range(1, 9) if p[f"m{i}_src"] != "Off" and p[f"m{i}_dst"] != "Off"
                     and float(p[f"m{i}_amt"]) != 0.0]
        self.chord = CHORDS.get(p["chord"], [0])
        self.rr_counter = 0


class Voice:
    def __init__(self, vid, seed):
        self.vid = vid
        voice_seed = (0x9E3779B9 ^ (vid * 2654435761) ^ int(seed)) & 0xFFFFFFFF
        self.rng = XorShift32(voice_seed)
        self.active = False
        self.age = 0

    def start(self, layer, note, vel, t_abs, legato_from=None, counter=0, layer_alt=1.0):
        p = layer.p
        self.layer, self.note, self.vel = layer, note, vel
        self.gate, self.active, self.pressure = True, True, 0.0
        self.age = counter
        self.t_on = t_abs
        rng = self.rng
        self.rand = rng.random()
        self.rand_bi = rng.bipolar()
        self.layer_alt = layer_alt
        flav = min(1.0, float(p["flavour"]) + layer.global_flavour)
        self.flav_wave = rng.bipolar() * 3.0 * flav
        self.flav_pitch = rng.bipolar() * 0.12 * flav
        self.flav_cut = rng.bipolar() * flav
        self.flav_src = rng.bipolar() * flav
        target = note + 12 * int(p["octave"])
        if legato_from is None or float(p["glide"]) <= 0.0:
            self.pitch = target
        self.target = target
        if legato_from is None:
            self.envs = [Env(), Env(), Env()]
            self.p1 = self.p2 = 0.0
            self.cur1 = self.cur2 = None
            self.z = [0.0, 0.0, 0.0, 0.0]
            self.lfos = []
            for k in (1, 2):
                ph = 0.0 if p[f"lfo{k}_phase"] == "Voice" else (t_abs * self._lfo_rate(k, 0.0)) % 1.0
                self.lfos.append(Lfo(rng, ph))
            self.burst_env = 0.0
            self.click_left = int(0.004 * SR)
            self.amp_prev = 0.0
        if legato_from is None or p["poly"] != "Legato":
            for i, e in enumerate(self.envs, start=1):
                e.gate_on(float(p[f"e{i}_a"]), float(p[f"e{i}_d"]), float(p[f"e{i}_s"]), float(p[f"e{i}_r"]))
        self._set_pan(p, counter)

    def _set_pan(self, p, counter):
        mode, pan = p["pan_mode"], float(p["pan"])
        if mode == "Random":
            self.pan = self.rng.bipolar() * pan
        elif mode == "Alt":
            self.pan = pan if counter % 2 else -pan
        elif mode == "Spread":
            self.pan = ((self.vid % 8) / 3.5 - 1.0) * pan
        elif mode in ("LFO1", "LFO2"):
            self.pan = 0.0
        else:
            self.pan = pan

    def _lfo_rate(self, k, rate_mod):
        p = self.layer.p
        if p[f"lfo{k}_sync"] == "On":
            return self.layer.bpm / 60.0 / div_beats(p[f"lfo{k}_div"]) * 2.0 ** (rate_mod * 4)
        return lfo_hz(float(p[f"lfo{k}_rate"]) + rate_mod)

    def release(self):
        self.gate = False
        for e in self.envs:
            e.gate_off()

    # ---------------------------------------------------------------- render
    def render(self, outL, outR, n, t_abs, modwheel):
        L = self.layer
        p = L.p
        e1s, e2s, e3s = self.envs

        # pre-pass sources (envelopes from previous block, LFOs advance first)
        src = {
            "Wave Env": e1s.level, "Filter Env": e2s.level, "Amp Env": e3s.level,
            "Velocity": self.vel, "Key": (self.note - 60) / 60.0, "Poly AT": self.pressure,
            "Chan AT": self.pressure, "ModWheel": modwheel, "Random": self.rand,
            "Random Bi": self.rand_bi, "Voice #": (self.vid % 8) / 3.5 - 1.0,
            "Layer Alt": self.layer_alt, "Constant": 1.0, "Flavour": self.flav_src,
        }
        mod = {}
        for s, d, a in L.mods:
            if s in ("LFO1", "LFO2", "LFO1xMW", "LFO2xMW"):
                continue
            mod[d] = mod.get(d, 0.0) + a * src.get(s, 0.0)

        t_note = t_abs - self.t_on
        for k, lfo in ((1, self.lfos[0]), (2, self.lfos[1])):
            hz = self._lfo_rate(k, mod.get(f"LFO{k} Rate", 0.0) * 0.5)
            v = lfo.step(hz, p[f"lfo{k}_shape"], n)
            fade = float(p[f"lfo{k}_fade"])
            if fade > 0:
                v *= min(1.0, t_note / env_time(fade))
            if p[f"lfo{k}_mw"] == "On":
                v *= modwheel
            src[f"LFO{k}"] = v
            src[f"LFO{k}xMW"] = v * modwheel
        for s, d, a in L.mods:
            if s.startswith("LFO"):
                mod[d] = mod.get(d, 0.0) + a * src[s]

        e1 = e1s.step(n, mod.get("E1 Attack", 0.0), mod.get("E1 Decay", 0.0))
        e2 = e2s.step(n, 0.0, mod.get("E2 Decay", 0.0))
        e3_prev = self.amp_prev
        e3 = e3s.step(n, 0.0, mod.get("E3 Decay", 0.0), mod.get("E3 Release", 0.0))
        if e3s.stage == "off":
            self.active = False
            return

        # glide
        gl = float(p["glide"])
        if gl > 0 and self.pitch != self.target:
            c = 1.0 - math.exp(-n / (glide_time(gl) / 4.6 * SR))
            self.pitch += (self.target - self.pitch) * c
            if abs(self.pitch - self.target) < 0.001:
                self.pitch = self.target
        else:
            self.pitch = self.target

        vel, at = self.vel, self.pressure
        vib = float(p["at_vib"]) * at * src["LFO1"] * 1.5
        pitch_common = self.pitch + self.flav_pitch + vib + mod.get("Pitch", 0.0) * PITCH_RANGE

        def osc_freq(o, extra):
            base = pitch_common if p[f"{o}_kbd"] == "On" else 60 + (pitch_common - self.pitch)
            return note_hz(base + int(p[f"{o}_semi"]) + int(p[f"{o}_fine"]) / 100.0 + extra)

        xt = p["xmod_type"]
        xa = min(1.0, max(0.0, float(p["xmod"]) + mod.get("XMod", 0.0)))
        sync_up = xa * SYNC_RANGE if xt == "Sync" else 0.0
        f1 = osc_freq("osc1", mod.get("Osc1 Pitch", 0.0) * PITCH_RANGE + sync_up)
        f2 = osc_freq("osc2", mod.get("Osc2 Pitch", 0.0) * PITCH_RANGE)

        at_w = float(p["at_wave"]) * at * WAVE_RANGE
        w1 = (int(p["osc1_wave"]) + self.flav_wave + at_w
              + float(p["osc1_wenv"]) * e1 * WAVE_RANGE + float(p["osc1_vel"]) * vel * WAVE_RANGE
              + mod.get("Osc1 Wave", 0.0) * WAVE_RANGE)
        if L.link:
            w2 = w1
        else:
            w2 = (int(p["osc2_wave"]) + self.flav_wave + at_w
                  + float(p["osc2_wenv"]) * e1 * WAVE_RANGE + float(p["osc2_vel"]) * vel * WAVE_RANGE
                  + mod.get("Osc2 Wave", 0.0) * WAVE_RANGE)
        nw1 = L.t1[wave_slot(int(p["osc1_wave"]), w1)]
        nw2 = L.t2[wave_slot(int(p["osc1_wave"] if L.link else p["osc2_wave"]), w2)]
        lv1 = max(0.0, float(p["osc1_level"]) + mod.get("Osc1 Level", 0.0))
        lv2 = max(0.0, float(p["osc2_level"]) + mod.get("Osc2 Level", 0.0))
        dirt_t = p["dirt_type"]
        dl = max(0.0, float(p["dirt"]) + mod.get("Dirt Level", 0.0))

        # filter
        ftype = p["ftype"]
        cut = (float(p["cutoff"]) + float(p["fenv"]) * e2 * CUTOFF_RANGE
               + float(p["fkbd"]) * (self.pitch - 60) + float(p["fvel"]) * vel * 48.0
               + float(p["at_cut"]) * at * CUTOFF_RANGE + self.flav_cut
               + mod.get("Cutoff", 0.0) * CUTOFF_RANGE)
        fc = min(18000.0, max(20.0, note_hz(cut)))
        g = math.tan(math.pi * fc / SR)
        res = min(1.0, max(0.0, float(p["res"]) + mod.get("Resonance", 0.0)))
        G = g / (1.0 + g)
        G2, G3 = G * G, G * G * G
        G4 = G2 * G2
        one_g = 1.0 - G
        k4 = res * 3.9
        Rsvf = 1.0 - res * 0.97
        hsvf = 1.0 / (1.0 + 2.0 * Rsvf * g + g * g)

        drv_t = p["drv_type"]
        drv = min(1.0, max(0.0, float(p["drive"]) + mod.get("Drive", 0.0)))
        dg = 1.0 + 15.0 * drv
        dcomp = 1.0 / (1.0 + drv)
        use_drive = drv_t != "Off" and drv > 0.0
        drive_pre = p["drv_pos"] == "Pre"

        # amp & pan
        amp_lvl = (1.0 - float(p["amp_vel"]) * (1.0 - vel)) * float(p["vol"]) * VOICE_GAIN
        amp_lvl *= max(0.0, 1.0 + mod.get("Amp Level", 0.0))
        pan = self.pan
        if p["pan_mode"] in ("LFO1", "LFO2"):
            pan = src[p["pan_mode"]] * float(p["pan"])
        pan = min(1.0, max(-1.0, pan + mod.get("Pan", 0.0)))
        gl_, gr_ = math.cos((pan + 1) * math.pi / 4), math.sin((pan + 1) * math.pi / 4)
        a0, a1 = e3_prev * amp_lvl, e3 * amp_lvl
        self.amp_prev = e3

        vintage = p["osc_mode"] == "Vintage"
        if vintage:
            d1, d2 = f1 * 128.0 / (SR * 4), f2 * 128.0 / (SR * 4)
        else:
            d1, d2 = f1 * 128.0 / SR, f2 * 128.0 / SR
        p1, p2 = self.p1, self.p2
        wv1 = self.cur1 or nw1
        wv2 = self.cur2 or nw2
        z1, z2, z3, z4 = self.z
        rnd = self.rng.random
        xm = xt == "Sync" or (xt != "Off" and xa > 0.0)
        use1, use2 = lv1 > 0.0, lv2 > 0.0 or dirt_t == "Ring" or xm
        da = (a1 - a0) / n
        over = 4 if vintage else 1
        pm = PM_RANGE * xa * xa * 128.0 if xt == "PM" else 0.0
        am = xa if xt == "AM" else 0.0
        hard = xt == "Sync"

        for i in range(n):
            o1 = o2 = 0.0
            if xm:
                acc1 = acc2 = 0.0
                for _ in range(over):
                    if vintage:
                        s2 = wv2[int(p2)]
                    else:
                        j = int(p2); s2 = wv2[j] + (wv2[(j + 1) & 127] - wv2[j]) * (p2 - j)
                    p2 += d2
                    wrapped = p2 >= 128.0
                    if wrapped: p2 -= 128.0; wv2 = nw2
                    q = (p1 + s2 * pm) % 128.0 if pm else p1
                    if vintage:
                        s1 = wv1[int(q)]
                    else:
                        j = int(q); s1 = wv1[j] + (wv1[(j + 1) & 127] - wv1[j]) * (q - j)
                    if am:
                        s1 *= 1.0 - am * (0.5 - 0.5 * s2)
                    p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                    if hard and wrapped:
                        p1 = p2 * d1 / d2; wv1 = nw1
                    acc1 += s1; acc2 += s2
                o1, o2 = acc1 / over, acc2 / over
            elif vintage:
                if use1:
                    s = wv1[int(p1)]; p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                    s += wv1[int(p1)]; p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                    s += wv1[int(p1)]; p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                    s += wv1[int(p1)]; p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                    o1 = s * 0.25
                if use2:
                    s = wv2[int(p2)]; p2 += d2
                    if p2 >= 128.0: p2 -= 128.0; wv2 = nw2
                    s += wv2[int(p2)]; p2 += d2
                    if p2 >= 128.0: p2 -= 128.0; wv2 = nw2
                    s += wv2[int(p2)]; p2 += d2
                    if p2 >= 128.0: p2 -= 128.0; wv2 = nw2
                    s += wv2[int(p2)]; p2 += d2
                    if p2 >= 128.0: p2 -= 128.0; wv2 = nw2
                    o2 = s * 0.25
            else:
                if use1:
                    j = int(p1); fr = p1 - j
                    a = wv1[j]; o1 = a + (wv1[(j + 1) & 127] - a) * fr
                    p1 += d1
                    if p1 >= 128.0: p1 -= 128.0; wv1 = nw1
                if use2:
                    j = int(p2); fr = p2 - j
                    a = wv2[j]; o2 = a + (wv2[(j + 1) & 127] - a) * fr
                    p2 += d2
                    if p2 >= 128.0: p2 -= 128.0; wv2 = nw2

            x = o1 * lv1 + (o2 * lv2 if dirt_t != "Ring" else 0.0)
            if dl > 0.0:
                if dirt_t == "Static":
                    x += (rnd() * 2 - 1) * dl
                elif dirt_t == "Crackle":
                    if rnd() < 0.002 + 0.02 * dl:
                        x += (rnd() * 2 - 1) * 2 * dl
                elif dirt_t == "Geiger":
                    if rnd() < 0.0008 + 0.004 * dl:
                        self.burst_env = 1.0
                    if self.burst_env > 0.001:
                        x += (rnd() * 2 - 1) * self.burst_env * dl * 2
                        self.burst_env *= 0.93
                elif dirt_t == "Click":
                    if self.click_left > 0:
                        x += (rnd() * 2 - 1) * dl * 2 * self.click_left / (0.004 * SR)
                        self.click_left -= 1
                elif dirt_t == "Burst":
                    if self.burst_env <= 0.001 and rnd() < 0.0004:
                        self.burst_env = 1.0
                    if self.burst_env > 0.001:
                        x += (rnd() * 2 - 1) * self.burst_env * dl
                        self.burst_env *= 0.9997
                elif dirt_t == "Ring":
                    x += o1 * o2 * dl * 2
            x *= 0.5

            if use_drive and drive_pre:
                x = shaper(drv_t, x * dg) * dcomp

            if ftype == "LP24" or ftype == "HP24":
                S = G3 * z1 * one_g + G2 * z2 * one_g + G * z3 * one_g + z4 * one_g
                y4 = (G4 * x + S) / (1.0 + k4 * G4)
                u = math.tanh((x - k4 * y4) * (1.0 + 0.5 * k4) * 0.8) * 1.25
                v = (u - z1) * G; y1 = v + z1; z1 = y1 + v
                v = (y1 - z2) * G; y2 = v + z2; z2 = y2 + v
                v = (y2 - z3) * G; y3 = v + z3; z3 = y3 + v
                v = (y3 - z4) * G; y4 = v + z4; z4 = y4 + v
                x = y4 if ftype == "LP24" else (u - 4 * y1 + 6 * y2 - 4 * y3 + y4)
            elif ftype in ("LP12", "HP12", "BP12"):
                hp = (x - (2 * Rsvf + g) * z1 - z2) * hsvf
                bp = g * hp + z1; z1 = g * hp + bp
                lp = g * bp + z2; z2 = g * bp + lp
                x = lp if ftype == "LP12" else (hp if ftype == "HP12" else bp * 2 * Rsvf)

            if use_drive and not drive_pre:
                x = shaper(drv_t, x * dg) * dcomp

            a0 += da
            x *= a0
            outL[i] += x * gl_
            outR[i] += x * gr_

        self.p1, self.p2 = p1, p2
        self.cur1, self.cur2 = wv1, wv2
        self.z = [z1, z2, z3, z4]


# ------------------------------------------------------------------ instrument

class Instrument:
    def __init__(self, params, tables, table_index, seed=1, fx=True, bpm=BPM):
        g = dict(GLOBAL_DEFAULTS)
        g.update({k: v for k, v in params.items() if not k.startswith("l") or k in GLOBAL_DEFAULTS})
        self.g = g
        self.bpm = bpm
        self.layers = []
        for n in range(1, int(g["layer_count"]) + 1):
            lp = {k[len(f"l{n}_"):]: v for k, v in params.items() if k.startswith(f"l{n}_")}
            if n == 1:
                lp.setdefault("on", "On")
            layer = Layer(lp, tables, table_index)
            layer.global_flavour = float(g["flavour"])
            layer.bpm = bpm
            self.layers.append(layer)
        self.slots = []
        for k in (1, 2):
            t = g[f"fx{k}_type"] if fx else "Off"
            knobs = []
            if t != "Off":
                knobs = default_knobs(t)
                for j in range(6):
                    if f"fx{k}_p{j + 1}" in g:
                        knobs[j] = float(g[f"fx{k}_p{j + 1}"])
            self.slots.append(FxSlot(t, knobs, float(g[f"fx{k}_mix"]), bpm))
        self.serial = g["fx_chain"] == "Serial"
        self.voices = [Voice(i, seed) for i in range(int(g["voices"]))]
        self.rng = XorShift32(seed)
        self.counter = 0
        self.rr = 0
        self.last_rr = -1
        self.held = {}          # (layer idx, note) -> voices
        self.mono_stack = {}    # layer idx -> list of notes
        self.modwheel = 0.0

    def _targets(self, note):
        on = [i for i, L in enumerate(self.layers) if L.p["on"] == "On"]
        mode = self.g["layer_mode"]
        if not on:
            return []
        if mode == "RoundRb":
            i = on[self.rr % len(on)]
            self.rr += 1
            return [i]
        if mode == "RandRR":
            choices = [i for i in on if i != self.last_rr] or on
            i = self.rng.choice(choices)
            self.last_rr = i
            return [i]
        if mode == "Split":
            return [i for i in on if int(self.layers[i].p["key_lo"]) <= note <= int(self.layers[i].p["key_hi"])]
        return on

    def _alloc(self, layer_idx):
        free = [v for v in self.voices if not v.active]
        if free:
            return free[0]
        rel = [v for v in self.voices if not v.gate]
        pool = rel or self.voices
        return min(pool, key=lambda v: v.age)

    def note_on(self, note, vel, t_abs):
        self.counter += 1
        for li in self._targets(note):
            L = self.layers[li]
            p = L.p
            n0 = quantize_scale(note) if p["scale"] != "Off" else note
            if p["poly"] in ("Mono", "Legato"):
                stack = self.mono_stack.setdefault(li, [])
                stack.append(n0)
                cur = next((v for v in self.voices if v.active and v.layer is L), None)
                if cur is not None and cur.gate:
                    cur.start(L, n0, vel, t_abs, legato_from=True, counter=self.counter)
                else:
                    v = cur or self._alloc(li)
                    v.start(L, n0, vel, t_abs, counter=self.counter)
                continue
            for iv in L.chord:
                v = self._alloc(li)
                v.start(L, n0 + iv, vel, t_abs, counter=self.counter,
                        layer_alt=1.0 if self.counter % 2 else -1.0)
                self.held.setdefault((li, note), []).append(v)

    def note_off(self, note):
        for li, L in enumerate(self.layers):
            if L.p["poly"] in ("Mono", "Legato"):
                stack = self.mono_stack.get(li, [])
                n0 = quantize_scale(note) if L.p["scale"] != "Off" else note
                if n0 not in stack:
                    continue
                stack.remove(n0)
                cur = next((v for v in self.voices if v.active and v.layer is L and v.gate), None)
                if cur is None:
                    continue
                if stack:
                    cur.note = stack[-1]
                    cur.target = stack[-1] + 12 * int(L.p["octave"])
                else:
                    cur.release()
                continue
            for v in self.held.pop((li, note), []):
                if v.layer is L and v.gate:
                    v.release()

    def set_pressure(self, value):
        for v in self.voices:
            if v.active and v.gate:
                v.pressure = value

    def render(self, n, t_abs):
        bufs = {id(L): ([0.0] * n, [0.0] * n) for L in self.layers}
        for v in self.voices:
            if v.active:
                bl, br = bufs[id(v.layer)]
                v.render(bl, br, n, t_abs, self.modwheel)
        buses = {r: ([0.0] * n, [0.0] * n) for r in ("Dry", "FX1", "FX2")}
        for L in self.layers:
            route = L.p["route"]
            if route != "Dry" and self.slots[int(route[-1]) - 1].fx is None:
                route = "Dry"
            send = float(L.p["send"]) if route != "Dry" else 1.0
            bl, br = bufs[id(L)]
            ol, orr = buses[route]
            for i in range(n):
                ol[i] += bl[i] * send
                orr[i] += br[i] * send
        outL, outR = buses["Dry"]
        f1l, f1r = self.slots[0].process(*buses["FX1"], t_abs)
        f2in = buses["FX2"]
        if self.serial:
            f2in = ([a + b for a, b in zip(f2in[0], f1l)], [a + b for a, b in zip(f2in[1], f1r)])
            f1l = f1r = None
        f2l, f2r = self.slots[1].process(*f2in, t_abs)
        for i in range(n):
            outL[i] += f2l[i] + (f1l[i] if f1l else 0.0)
            outR[i] += f2r[i] + (f1r[i] if f1r else 0.0)
        return outL, outR

    def fx_tail(self):
        return max(s.tail() for s in self.slots)
