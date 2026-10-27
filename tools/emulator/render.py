#!/usr/bin/env python3
"""Render audio previews of Amino factory presets with the compiled C engine.

The audio comes from build/libamino_host (src/dsp/*.c plus c_host.c, built by
scripts/build.sh), so previews are what the Move module plays: each preset is
loaded by index, the demo phrase is sent as MIDI, and the transport runs from
the phrase start, in 128-frame blocks like the device.

Usage:
  scripts/build.sh                                  # builds the engine library
  python3 tools/emulator/render.py                  # all presets
  python3 tools/emulator/render.py 3 10 84          # selected indices
  python3 tools/emulator/render.py --jobs 4
  python3 tools/emulator/render.py --dry            # bypass FX
Outputs tools/emulator/out/NNN_name.wav (44.1 kHz stereo 16-bit), levels.csv
and the gallery page index.html.
"""
import argparse
import json
import math
import multiprocessing as mp
import sys
import time
import wave
from pathlib import Path

import c_engine

ROOT = Path(__file__).resolve().parents[2]
PRESETS = ROOT / "presets" / "factory"
OUT = Path(__file__).resolve().parent / "out"

SR = 44100
BPM = 120.0
BLOCK = c_engine.BLOCK
PREROLL_BLOCKS = 32         # FX arena clear + type fade-in before the phrase
TAIL_MAX = 6.0              # seconds after the phrase
TAIL_SILENCE = 0.3          # stop once this long stays below SILENCE_PEAK
SILENCE_PEAK = 8            # int16 units, about -72 dBFS

BEAT = 60.0 / BPM
S16 = BEAT / 4


def notes(seq, step, gate=0.9, vel=0.85, start=0.0):
    """seq of note numbers (or lists for chords, None for rests) on a grid."""
    ev = []
    for i, n in enumerate(seq):
        if n is None:
            continue
        for nn in (n if isinstance(n, list) else [n]):
            t = start + i * step
            ev.append((t, "on", nn, vel if i % 2 == 0 else vel * 0.85))
            ev.append((t + step * gate, "off", nn, 0))
    return ev


def ramp(t0, t1, v0=0.0, v1=1.0):
    def f(t):
        if t <= t0:
            return v0
        if t >= t1:
            return v1
        return v0 + (v1 - v0) * (t - t0) / (t1 - t0)
    return f


def phrase(name, bpm=BPM):
    """(events, length in s, pressure(t)) — demo phrases per category or genre role."""
    if name.split("_")[0] in GENRES:
        return genre_phrase(name, bpm)
    zero = ramp(0, 1, 0, 0)
    if name == "init":
        return [(0, "on", 48, .8), (1.4, "off", 48, 0), (1.5, "on", 60, .8), (2.9, "off", 60, 0)], 3.0, zero
    if name == "bass":
        bar1 = notes([36, 36, 48, 36, 39, 36, 43, 41], S16 * 2, gate=0.6)
        bar2 = notes([36, 36, 48, 36, 39, 36, 43, 41], S16 * 2, gate=1.3, start=2.0)
        return bar1 + bar2, 4.0, ramp(2.0, 4.0, 0, 0.8)
    if name == "lead":
        ev, t = [], 0.0
        for n, d in ((60, .5), (63, .5), (67, 1.0), (70, .25), (72, 1.75)):
            ev += [(t, "on", n, .85), (t + d + 0.06, "off", n, 0)]
            t += d
        return ev, t, ramp(2.5, 4.0, 0, 1.0)
    if name == "pad":
        ch = [48, 55, 58, 63]
        return [(0, "on", n, .7) for n in ch] + [(3.5, "off", n, 0) for n in ch], 3.5, ramp(1.5, 3.4, 0, 1.0)
    if name == "keys":
        ev = []
        for t, ch, d in ((0.0, [48, 55, 60, 63], .9), (1.0, [46, 53, 58, 62], .9), (2.0, [44, 51, 56, 60], 1.5)):
            ev += [(t, "on", n, .75) for n in ch] + [(t + d, "off", n, 0) for n in ch]
        return ev, 3.5, zero
    if name == "pluck":
        seq = [48, 55, 60, 63, 67, 63, 60, 55] * 4
        return notes(seq, S16, gate=0.8, vel=0.9), len(seq) * S16, ramp(2.0, 4.0, 0, 0.7)
    if name == "rr":
        return notes([60, 60, 60, 60, 63, 63, 67, 67, 60, 60, 60, 60], S16 * 2, gate=0.7), 3.0, zero
    if name == "fx":
        return [(0, "on", 60, .9), (4.5, "off", 60, 0)], 4.5, ramp(0.5, 4.5, 0, 1.0)
    if name == "drone":
        return [(0, "on", 36, .8), (0, "on", 43, .8), (5.0, "off", 36, 0), (5.0, "off", 43, 0)], 5.0, ramp(2, 5, 0, .6)
    if name == "split":
        b, _, _ = phrase("bass")
        l, _, _ = phrase("lead")
        return sorted(b + l, key=lambda e: e[0]), 4.0, ramp(2.5, 4.0, 0, 1.0)
    if name == "drums":
        k, s, h, o, p = 36, 38, 42, 46, 50
        grid = [[k, h], [h], [h], [h], [s, h], [h], [p, h], [h], [k, h], [h], [k, h], [p], [s, h], [h], [o], [h]] * 2
        return notes(grid, S16, gate=0.5, vel=0.9), len(grid) * S16, zero
    raise ValueError(name)


# ------------------------------------------------------------------ genre sequences (docs/13)

Cm, Ab, Eb, Bb = [60, 63, 67], [56, 60, 63], [55, 58, 63], [58, 62, 65]
Cm7, Fm7 = [60, 63, 67, 70], [53, 56, 60, 63]
Cm9, Fm9, Abmaj7 = [51, 55, 58, 62], [56, 60, 63, 67], [56, 60, 63, 67]
GENRES = {"trance": 0.0, "tech": 0.12, "deep": 0.15, "minimal": 0.0}   # swing per genre


def steps(bpm, items, swing=0.0, gate=0.9, legato=False):
    """items: (step, note or [notes], length in 16ths, velocity). Odd 16ths are swung."""
    s16 = 60.0 / bpm / 4
    ev = []
    for step, notes, length, vel in items:
        t = step * s16 + (swing * s16 if step % 2 else 0.0)
        dur = length * s16 * (1.0 if legato else gate) + (0.03 if legato else 0.0)
        for n in notes if isinstance(notes, list) else [notes]:
            ev += [(t, "on", n, vel), (t + dur, "off", n, 0)]
    return ev


def bars(bpm, n):
    return n * 240.0 / bpm


def arp(chord, bar, rate=1, vel=0.85):
    """16th (rate 1) or 8th (rate 2) arpeggio up and back over one bar."""
    tones = chord + [chord[0] + 12, chord[1] + 12] + chord[::-1][1:]
    return [(bar * 16 + i, tones[(i // rate) % len(tones)], rate, vel if i % 4 == 0 else vel * 0.8)
            for i in range(0, 16, rate)]


def genre_phrase(name, bpm):
    genre, role = name.split("_")
    sw = GENRES[genre]
    zero = ramp(0, 1, 0, 0)
    two, four = bars(bpm, 2), bars(bpm, 4)

    if genre == "trance":
        if role == "bass":
            items = [(b * 16 + beat * 4 + k, root, 1, 0.9 if k == 2 else 0.8)
                     for b, root in enumerate((36, 32)) for beat in range(4) for k in (1, 2, 3)]
            return steps(bpm, items, gate=0.7), two, zero
        if role == "lead":
            mel = [(0, 72, 3), (3, 75, 1), (4, 79, 4), (8, 77, 2), (10, 75, 2), (12, 74, 2), (14, 72, 2),
                   (16, 70, 3), (19, 72, 1), (20, 75, 4), (24, 74, 2), (26, 72, 2), (28, 67, 4)]
            return steps(bpm, [(s, n, l, .85) for s, n, l in mel], legato=True), two, ramp(two / 2, two, 0, 1)
        if role == "keys":
            items = [(b * 16 + s, ch, 1, .8) for b, ch in enumerate((Cm, Ab)) for s in (2, 6, 10, 14)]
            return steps(bpm, items, gate=0.8), two, zero
        if role == "pad":
            items = [(b * 16, [ch[0] - 12] + ch, 16, .7) for b, ch in enumerate((Cm, Ab, Eb, Bb))]
            return steps(bpm, items, gate=0.98), four, ramp(two / 2, four, 0, 1)
        if role == "pluck":
            return steps(bpm, arp(Cm, 0) + arp(Ab, 1), gate=0.8), two, zero
        return steps(bpm, [(0, 60, 64, .9)], gate=1.0), four, ramp(0.5, four, 0, 1)

    if genre == "tech":
        if role == "bass":
            bar = [(2, 36, 1), (3, 36, 1), (6, 48, 1), (7, 36, 1), (10, 36, 2), (13, 39, 1), (14, 41, 1)]
            bar2 = [(18, 36, 1), (19, 36, 1), (22, 46, 1), (23, 36, 1), (26, 36, 2), (29, 43, 1), (30, 41, 1)]
            return steps(bpm, [(s, n, l, .85) for s, n, l in bar + bar2], sw, gate=0.75), two, zero
        if role == "lead":
            hook = [(2, 72, 1), (4, 75, 2), (7, 72, 1), (10, 70, 2), (12, 72, 3),
                    (18, 72, 1), (20, 75, 2), (23, 77, 1), (26, 75, 2), (28, 67, 4)]
            return steps(bpm, [(s, n, l, .85) for s, n, l in hook], sw, gate=0.8), two, ramp(two / 2, two, 0, 1)
        if role == "keys":
            items = [(b * 16 + s, ch, 1, .8) for b, ch in enumerate((Cm7, Fm7)) for s in (3, 7, 10, 14)]
            return steps(bpm, items, sw, gate=0.7), two, zero
        if role == "pad":
            items = [(0, Cm7, 32, .7), (32, Fm7, 32, .7)]
            return steps(bpm, items, gate=0.98), four, ramp(two / 2, four, 0, 1)
        if role == "pluck":
            pat = [(0, 60, 1.0), (2, 60, .7), (3, 63, .8), (6, 60, .9), (8, 60, .7), (10, 67, .9),
                   (11, 60, .7), (14, 63, .8)]
            items = [(b * 16 + s, n, 1, v) for b in (0, 1) for s, n, v in pat]
            return steps(bpm, items, sw, gate=0.6), two, zero
        return steps(bpm, [(0, 60, 32, .9)], gate=1.0), two, ramp(0.3, two, 0, 1)

    if genre == "deep":
        if role == "bass":
            line = [(0, 36, 3), (6, 36, 1), (7, 48, 1), (10, 39, 3), (14, 41, 2),
                    (16, 41, 3), (22, 41, 1), (23, 53, 1), (26, 44, 3), (30, 43, 2)]
            return steps(bpm, [(s, n, l, .85) for s, n, l in line], sw, gate=0.9), two, zero
        if role == "lead":
            mel = [(0, 75, 6), (8, 74, 2), (10, 72, 6), (16, 70, 4), (20, 72, 4), (24, 67, 8)]
            return steps(bpm, [(s, n, l, .8) for s, n, l in mel], sw, legato=True), two, ramp(two / 2, two, 0, 1)
        if role == "keys":
            items = [(b * 16 + s, ch, 2, .75) for b, ch in enumerate((Cm9, Fm9)) for s in (3, 6, 10)]
            return steps(bpm, items, sw, gate=0.8), two, zero
        if role == "pad":
            items = [(0, [48] + Cm9, 32, .7), (32, [44] + Abmaj7, 32, .7)]
            return steps(bpm, items, gate=0.98), four, ramp(two / 2, four, 0, 1)
        if role == "pluck":
            return steps(bpm, arp(Cm9, 0, rate=2) + arp(Fm9, 1, rate=2), sw, gate=0.8), two, zero
        return steps(bpm, [(0, 48, 32, .9)], gate=1.0), two, ramp(0.3, two, 0, 1)

    # minimal
    if role == "bass":
        pat = [(2, 1.0), (3, .7), (5, .8), (7, .7), (10, 1.0), (11, .7), (13, .8), (15, .7)]
        items = [(b * 16 + s, 39 if (b == 1 and s == 13) else 36, 1, v) for b in (0, 1) for s, v in pat]
        return steps(bpm, items, gate=0.6), two, zero
    if role == "lead":
        items = [(4, 72, 1, .8), (11, 75, 1, .7), (22, 79, 1, .8), (27, 72, 1, .7)]
        return steps(bpm, items, gate=0.8), two, zero
    if role == "keys":
        items = [(3, Cm7, 1, .8), (11, Cm7, 1, .7), (19, Cm7, 1, .8)]
        return steps(bpm, items, gate=0.8), two, zero
    if role == "pad":
        return steps(bpm, [(0, [48, 55, 60], 64, .7)], gate=0.98), four, ramp(bars(bpm, 1), four, 0, 1)
    if role == "pluck":
        fig = [60, 63, 67]
        items = [(i, fig[i % 3], 1, .9 if i % 4 == 0 else .7) for i in range(32)]
        return steps(bpm, items, gate=0.6), two, zero
    return steps(bpm, [(0, 60, 32, .9)], gate=1.0), two, ramp(0.3, two, 0, 1)


_LIB = None
_FX = True


def _init_worker(fx):
    # main() already checked the library; a failing initializer would make
    # the pool respawn workers forever instead of reporting the error.
    global _LIB, _FX
    _LIB = c_engine.load(check=False)
    _FX = fx


def uses_chords(params):
    layers = int(params.get("layer_count", 1))
    return any(params.get(f"l{n}_chord", "Off") != "Off" for n in range(1, layers + 1))


def play(lib, doc, fx=True, overrides=None):
    """Render doc's demo phrase through the engine; interleaved int16 bytes.
    `overrides` are set_param pairs applied after the preset loads."""
    params = doc["params"]
    bpm = float(doc.get("bpm", BPM))
    events, length, pressure = phrase(doc["phrase"], bpm)
    if uses_chords(params):
        # Chord presets play one key per step; the engine adds the chord tones.
        firsts = {}
        for t, kind, n, v in events:
            if kind == "on":
                firsts[t] = min(firsts.get(t, 999), n)
        keep = {(t, n) for t, n in firsts.items()}
        ons = [(t, k, n, v) for t, k, n, v in events if k == "on" and (t, n) in keep]
        offs = [(t, k, n, v) for t, k, n, v in events if k == "off" and n in {e[2] for e in ons}]
        events = ons + offs
    events = sorted(events, key=lambda e: (e[0], e[1] != "off"))

    frames = bytearray()
    with c_engine.Engine(lib, bpm) as eng:
        eng.set("preset", doc["index"])
        if not fx:
            eng.set("fx1_type", "Off")
            eng.set("fx2_type", "Off")
        for key, value in (overrides or {}).items():
            eng.set(key, value)
        for _ in range(PREROLL_BLOCKS):
            eng.render()
        eng.transport(True)
        ei, pos, quiet = 0, 0, 0
        phrase_end = int(length * SR)
        limit = int((length + TAIL_MAX) * SR)
        while pos < limit:
            t = pos / SR
            while ei < len(events) and events[ei][0] <= t:
                _, kind, n, v = events[ei]
                if kind == "on":
                    eng.note_on(n, v)
                else:
                    eng.note_off(n)
                ei += 1
            eng.pressure(pressure(t))
            n = min(BLOCK, limit - pos)
            block = eng.render(n)
            frames += block
            pos += n
            if pos > phrase_end and ei >= len(events):
                samples = memoryview(block).cast("h")
                quiet = quiet + n if max(map(abs, samples)) <= SILENCE_PEAK else 0
                if quiet >= TAIL_SILENCE * SR:
                    break
    return bytes(frames)


def render_preset(path):
    t_start = time.time()
    doc = json.loads(Path(path).read_text())
    frames = play(_LIB, doc, _FX)
    pcm = memoryview(frames).cast("h")
    peak = max(map(abs, pcm), default=0) / 32767.0
    acc = sum(v * v for v in pcm) / (32767.0 * 32767.0)
    name = Path(path).stem
    with wave.open(str(OUT / f"{name}.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))
    seconds = len(pcm) / 2 / SR
    rms = math.sqrt(acc / max(1, len(pcm)))
    db = lambda x: 20 * math.log10(x) if x > 1e-9 else -120.0  # noqa: E731
    return doc["index"], doc["name"], db(peak), db(rms), seconds, time.time() - t_start


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("indices", nargs="*", type=int)
    ap.add_argument("--jobs", type=int, default=max(1, mp.cpu_count() - 1))
    ap.add_argument("--dry", action="store_true", help="bypass FX1/FX2")
    args = ap.parse_args()
    files = sorted(PRESETS.glob("*.json"))
    if not files:
        sys.exit("No presets: run tools/emulator/extract_presets.py first.")
    if args.indices:
        files = [f for f in files if int(f.name[:3]) in set(args.indices)]
    try:
        c_engine.load()
    except (RuntimeError, OSError) as err:
        sys.exit(str(err))
    OUT.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    with mp.Pool(args.jobs, initializer=_init_worker, initargs=(not args.dry,)) as pool:
        rows = []
        for r in pool.imap_unordered(render_preset, [str(f) for f in files]):
            rows.append(r)
            print(f"[{r[0]:3d}] {r[1]:<24} peak {r[2]:6.1f} dBFS  rms {r[3]:6.1f} dBFS  {r[4]:4.1f}s  ({r[5]:.1f}s)", flush=True)
    rows.sort()
    quiet = [r for r in rows if r[2] < -30.0]
    hot = [r for r in rows if r[2] > -0.5]
    for r in quiet:
        print(f"WARNING [{r[0]}] {r[1]}: peak {r[2]:.1f} dBFS (near-silent)")
    for r in hot:
        print(f"WARNING [{r[0]}] {r[1]}: peak {r[2]:.1f} dBFS (hitting the soft clipper)")
    with open(OUT / "levels.csv", "w") as f:
        f.write("index,name,peak_dbfs,rms_dbfs,seconds\n")
        for r in rows:
            f.write(f"{r[0]},{r[1]},{r[2]:.1f},{r[3]:.1f},{r[4]:.2f}\n")
    print(f"{len(rows)} previews in {time.time() - t0:.0f}s -> {OUT}")
    import gallery
    gallery.main(fx=not args.dry)


if __name__ == "__main__":
    main()
