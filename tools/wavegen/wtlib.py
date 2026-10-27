"""Shared helpers for Amino wavetable generation (Python stdlib only)."""
import math
import struct
import wave
import zlib

N = 128          # samples per wave
SLOTS = 61       # waves 0..60 are table content
WAVES = 64       # waves 61..63 are the classic Tri / Square / Saw
H = N // 2 - 1   # highest harmonic kept (63)

_TRIG = {}


def _trig(m):
    if m not in _TRIG:
        _TRIG[m] = ([math.sin(2 * math.pi * i / m) for i in range(m)],
                    [math.cos(2 * math.pi * i / m) for i in range(m)])
    return _TRIG[m]


def additive(amps, phases=None):
    """Sum of sines: amps[k-1] / phases[k-1] belong to harmonic k (sine phase)."""
    S, C = _trig(N)
    out = [0.0] * N
    for k in range(1, min(len(amps), H) + 1):
        a = amps[k - 1]
        if a == 0.0:
            continue
        ph = phases[k - 1] if phases else 0.0
        s, c = a * math.cos(ph), a * math.sin(ph)
        j = 0
        for i in range(N):
            out[i] += s * S[j] + c * C[j]
            j += k
            if j >= N:
                j -= N
    return out


def analyze(x, kmax=H):
    """Fourier amplitudes/phases (sine convention) of one period sampled at len(x) points."""
    m = len(x)
    S, C = _trig(m)
    amps, phases = [], []
    for k in range(1, kmax + 1):
        a = b = 0.0
        j = 0
        for v in x:
            a += v * S[j]
            b += v * C[j]
            j += k
            if j >= m:
                j -= m
        a *= 2.0 / m
        b *= 2.0 / m
        amps.append(math.hypot(a, b))
        phases.append(math.atan2(b, a))
    return amps, phases


def bandlimit(fn, oversample=4):
    """Render fn(phase 0..1) at oversample*N points, keep harmonics 1..H."""
    m = N * oversample
    amps, phases = analyze([fn(n / m) for n in range(m)])
    return additive(amps, phases)


def align(amps, phases):
    """Rotate so the fundamental has phase 0; keeps adjacent waves phase-coherent."""
    if amps and amps[0] > 1e-6:
        p1 = phases[0]
        phases = [p - (k + 1) * p1 for k, p in enumerate(phases)]
    return amps, phases


def norm(w):
    mean = sum(w) / len(w)
    w = [v - mean for v in w]
    peak = max(abs(v) for v in w)
    return [v / peak for v in w] if peak > 1e-9 else [0.0] * len(w)


def lerp(a, b, f):
    return [x + (y - x) * f for x, y in zip(a, b)]


def harmonics(fn):
    """Amplitude list for harmonics 1..H from fn(k)."""
    return [fn(k) for k in range(1, H + 1)]


def classic():
    tri, sq, saw = [], [], []
    for i in range(N):
        p = i / N
        tri.append(4 * p if p < 0.25 else (2 - 4 * p if p < 0.75 else 4 * p - 4))
        sq.append(1.0 if p < 0.5 else -1.0)
        saw.append(2 * ((p + 0.5) % 1.0) - 1)
    return [tri, sq, saw]


def quantize(w):
    return [max(-128, min(127, int(round(v * 127)))) for v in w]


def read_wav_mono(path):
    with wave.open(str(path), "rb") as f:
        ch, sw, n = f.getnchannels(), f.getsampwidth(), f.getnframes()
        if sw != 2:
            raise ValueError(f"{path}: expected 16-bit PCM")
        data = struct.unpack("<%dh" % (n * ch), f.readframes(n))
    if ch == 1:
        return [v / 32768.0 for v in data]
    return [sum(data[i:i + ch]) / (ch * 32768.0) for i in range(0, len(data), ch)]


def write_wav16(path, samples_i8):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(44100)
        f.writeframes(struct.pack("<%dh" % len(samples_i8), *(v * 256 for v in samples_i8)))


def write_png_gray(path, w, h, pixels):
    raw = b"".join(b"\x00" + bytes(pixels[y * w:(y + 1) * w]) for y in range(h))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9))
                + chunk(b"IEND", b""))


def draw_wave(pixels, img_w, x0, y0, tile_w, tile_h, wave_i8, bg=255, fg=0, mid=200):
    for y in range(tile_h):
        row = (y0 + y) * img_w + x0
        for x in range(tile_w):
            pixels[row + x] = bg
    cy = y0 + tile_h // 2
    for x in range(tile_w):
        pixels[cy * img_w + x0 + x] = mid

    def ypos(v):
        return y0 + int(round((tile_h - 1) * (1 - (v + 128) / 255.0)))

    prev = ypos(wave_i8[0])
    for x in range(tile_w):
        y = ypos(wave_i8[x * len(wave_i8) // tile_w])
        lo, hi = min(prev, y), max(prev, y)
        for yy in range(lo, hi + 1):
            pixels[yy * img_w + x0 + x] = fg
        prev = y
