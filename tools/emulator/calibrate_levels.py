#!/usr/bin/env python3
"""Level-calibrate the factory bank: find each preset's Patch Gain (docs/18).

Plays every preset's demo phrase through the compiled engine (as render.py
does), measures BS.1770 integrated loudness and sample peak of the output and
picks the gain that reaches TARGET_LUFS without the peak passing CEILING_DB.
Short, spiky sounds (plucks, ticks) are therefore held by the peak ceiling and
sit a little under the loudness target, which is how they read in a mix.

The result goes to presets/level_trims.json; extract_presets.py stores it as
`patch_gain` in each preset.  Run after any change that alters a preset's
level, then regenerate:

  scripts/build.sh                     # engine library with the current bank
  python3 tools/emulator/calibrate_levels.py
  python3 tools/emulator/extract_presets.py && scripts/build.sh --gen
"""
import argparse
import json
import math
import multiprocessing as mp
import sys
from pathlib import Path

import c_engine
import render
from polish import TRIMS, load_trims

TARGET_LUFS = -18.0
CEILING_DB = -3.0          # sample peak after the master soft clip
GAIN_MIN, GAIN_MAX = -24.0, 24.0
TOLERANCE_DB = 0.3
PASSES = 4
SR = render.SR


def biquad(b, a, x):
    b0, b1, b2 = b
    a1, a2 = a
    z1 = z2 = 0.0
    out = [0.0] * len(x)
    for i, v in enumerate(x):
        y = b0 * v + z1
        z1 = b1 * v - a1 * y + z2
        z2 = b2 * v - a2 * y
        out[i] = y
    return out


def k_weighting(sr):
    """BS.1770 pre-filter (high shelf) and RLB high-pass, designed for sr."""
    f0, gain_db, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / sr)
    vh = 10 ** (gain_db / 20)
    vb = vh ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    shelf = ((vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0), \
            (2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0)
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / sr)
    a0 = 1 + k / q + k * k
    hp = (1.0, -2.0, 1.0), (2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0)
    return shelf, hp


SHELF, HP = k_weighting(SR)


def measure(frames):
    """(integrated LUFS, sample peak dBFS) of interleaved int16 stereo bytes."""
    pcm = memoryview(frames).cast("h")
    if not len(pcm):
        return -99.0, -99.0
    peak = max(map(abs, pcm)) / 32768.0
    energy = None
    for ch in (0, 1):
        x = [v / 32768.0 for v in pcm[ch::2]]
        y = biquad(*HP, biquad(*SHELF, x))
        sq = [v * v for v in y]
        energy = sq if energy is None else [a + b for a, b in zip(energy, sq)]
    hop = int(0.1 * SR)
    hops = [sum(energy[i:i + hop]) for i in range(0, len(energy) - hop + 1, hop)]
    blocks = [sum(hops[i:i + 4]) / (4 * hop) for i in range(len(hops) - 3)] or \
             [sum(energy) / len(energy)]
    lufs = lambda e: -0.691 + 10 * math.log10(e) if e > 0 else -99.0  # noqa: E731
    gated = [b for b in blocks if lufs(b) > -70.0]
    if not gated:
        return -99.0, 20 * math.log10(max(peak, 1e-9))
    rel = lufs(sum(gated) / len(gated)) - 10.0
    gated = [b for b in gated if lufs(b) > rel]
    return lufs(sum(gated) / len(gated)), 20 * math.log10(max(peak, 1e-9))


_LIB = None


def _init():
    global _LIB
    _LIB = c_engine.load(check=False)


def calibrate(job):
    """Iterate one preset's gain; the loop absorbs the soft clipper and FX."""
    path, gain = job
    doc = json.loads(Path(path).read_text())
    lufs = peak = -99.0
    for _ in range(PASSES):
        lufs, peak = measure(render.play(_LIB, doc, overrides={"patch_gain": f"{gain:.2f}"}))
        if lufs <= -98.0:
            break
        step = min(TARGET_LUFS - lufs, CEILING_DB - peak)
        if abs(step) < TOLERANCE_DB:
            break
        gain = max(GAIN_MIN, min(GAIN_MAX, gain + step))
    return doc["index"], doc["name"], round(gain, 1), lufs, peak


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("indices", nargs="*", type=int, help="only these presets (others keep their trim)")
    ap.add_argument("--jobs", type=int, default=max(1, mp.cpu_count() - 1))
    args = ap.parse_args()
    try:
        c_engine.load()
    except (RuntimeError, OSError) as err:
        sys.exit(str(err))
    trims = load_trims()
    files = sorted(render.PRESETS.glob("*.json"))
    if args.indices:
        files = [f for f in files if int(f.name[:3]) in set(args.indices)]
    jobs = [(str(f), trims.get(int(f.name[:3]), 0.0)) for f in files]
    rows = []
    with mp.Pool(args.jobs, initializer=_init) as pool:
        for r in pool.imap_unordered(calibrate, jobs):
            rows.append(r)
            print(f"[{r[0]:3d}] {r[1]:<24} gain {r[2]:+5.1f} dB  ->  {r[3]:6.1f} LUFS  peak {r[4]:5.1f} dBFS",
                  flush=True)
    for index, _, gain, _, _ in rows:
        trims[index] = gain
    TRIMS.write_text(json.dumps({str(k): trims[k] for k in sorted(trims)}, indent=1) + "\n")
    held = [r for r in rows if r[3] < TARGET_LUFS - 1.0]
    print(f"{len(rows)} presets -> {TRIMS}; {len(held)} held under target by the peak ceiling")


if __name__ == "__main__":
    main()
