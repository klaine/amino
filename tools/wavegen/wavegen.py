#!/usr/bin/env python3
"""Generate Amino's 8-bit wavetables (64 waves x 128 samples each).

Usage:
  python3 tools/wavegen/wavegen.py                 # procedural + AKWF tables
  python3 tools/wavegen/wavegen.py --no-akwf       # procedural tables only (indices shift)

AKWF-FREE (CC0) must be fetched first with tools/wavegen/fetch_akwf.sh.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import recipes
from wtlib import (H, N, SLOTS, WAVES, additive, align, analyze, classic, draw_wave,
                   lerp, norm, quantize, read_wav_mono, write_png_gray, write_wav16)

HERE = Path(__file__).resolve().parent
AKWF_DEFAULT = HERE / ".cache" / "AKWF-FREE" / "AKWF"
AKWF_COMMIT = "8de90bf94376670947369e69de0af6b9fbd19286"
AKWF_LICENSE = "CC0-1.0 (AKWF-FREE by Kristoffer Ekstrand)"


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def build_procedural(fn):
    return [norm(fn(s / (SLOTS - 1))) for s in range(SLOTS)]


def build_akwf(folder, count):
    files = sorted(folder.glob("*.wav"))
    if not files:
        raise FileNotFoundError(f"no WAV files in {folder}")
    spectra = []
    for f in files:
        amps, phases = analyze(read_wav_mono(f))
        total = sum(amps) or 1.0
        centroid = sum((k + 1) * a for k, a in enumerate(amps)) / total
        spectra.append((centroid, f.name, amps, phases))
    spectra.sort(key=lambda s: s[0])
    count = min(count, len(spectra))
    picks = [spectra[round(i * (len(spectra) - 1) / (count - 1))] for i in range(count)]
    keys = [norm(additive(*align(a, p))) for _, _, a, p in picks]
    slots_at = [round(i * (SLOTS - 1) / (count - 1)) for i in range(count)]
    table = []
    for s in range(SLOTS):
        j = max(i for i, at in enumerate(slots_at) if at <= s)
        if j == count - 1:
            table.append(keys[j])
        else:
            f = (s - slots_at[j]) / (slots_at[j + 1] - slots_at[j])
            table.append(norm(lerp(keys[j], keys[j + 1], f)))
    return table, [name for _, name, _, _ in picks]


def render_grid(path, waves):
    cols, tw, th, gap = 8, 128, 48, 2
    rows = (len(waves) + cols - 1) // cols
    w, h = cols * (tw + gap) + gap, rows * (th + gap) + gap
    px = [140] * (w * h)
    for i, wv in enumerate(waves):
        draw_wave(px, w, gap + (i % cols) * (tw + gap), gap + (i // cols) * (th + gap), tw, th, wv)
    write_png_gray(path, w, h, px)


def render_index(path, tables):
    picks = [0, 10, 20, 30, 40, 50, 60]
    tw, th, gap = 96, 36, 2
    w, h = len(picks) * (tw + gap) + gap, len(tables) * (th + gap) + gap
    px = [140] * (w * h)
    for r, waves in enumerate(tables):
        for c, s in enumerate(picks):
            draw_wave(px, w, gap + c * (tw + gap), gap + r * (th + gap), tw, th, waves[s])
    write_png_gray(path, w, h, px)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--akwf", type=Path, default=AKWF_DEFAULT, help="path to AKWF-FREE/AKWF")
    ap.add_argument("--no-akwf", action="store_true", help="skip AKWF-derived tables")
    ap.add_argument("--out", type=Path, default=HERE / "out")
    args = ap.parse_args()

    if not args.no_akwf and not args.akwf.is_dir():
        sys.exit(f"AKWF folder not found: {args.akwf}\nRun tools/wavegen/fetch_akwf.sh or pass --no-akwf.")

    jobs = [(name, "procedural", fn) for name, fn in recipes.PROCEDURAL]
    if not args.no_akwf:
        jobs += [(name, "akwf", (folder, n)) for name, folder, n in recipes.AKWF_TABLES]
    jobs += [(name, "procedural", fn) for name, fn in recipes.EXPANSION]
    jobs += [(name, "procedural", fn) for name, fn in recipes.V05]
    if not args.no_akwf:
        jobs += [(name, "akwf", (folder, n)) for name, folder, n in recipes.AKWF_V05]

    (args.out / "wav").mkdir(parents=True, exist_ok=True)
    (args.out / "preview").mkdir(parents=True, exist_ok=True)

    classics = [quantize(norm(w)) for w in classic()]
    blob = bytearray()
    manifest = {"format": {"waves": WAVES, "samples": N, "sample_type": "int8",
                           "max_harmonic": H, "classic_slots": {"61": "Triangle", "62": "Square", "63": "Saw"}},
                "tables": []}
    all_tables = []
    for index, (name, kind, spec) in enumerate(jobs):
        print(f"[{index:2d}] {name}", flush=True)
        if kind == "procedural":
            content, sources = build_procedural(spec), []
            entry = {"source": "procedural", "recipe": spec.__name__, "license": "MIT (Amino)"}
        else:
            folder, n = spec
            content, sources = build_akwf(args.akwf / folder, n)
            entry = {"source": "akwf", "akwf_folder": folder, "akwf_commit": AKWF_COMMIT,
                     "keyframes": sources, "license": AKWF_LICENSE}
        waves = [quantize(w) for w in content] + classics
        data = bytes(v & 0xFF for wv in waves for v in wv)
        blob += data
        base = f"{index:02d}_{slug(name)}"
        write_wav16(args.out / "wav" / f"{base}.wav", [v for wv in waves for v in wv])
        render_grid(args.out / "preview" / f"{base}.png", waves)
        manifest["tables"].append({"index": index, "name": name, **entry,
                                   "sha256": hashlib.sha256(data).hexdigest()})
        all_tables.append(waves)

    (args.out / "amino_wavetables.bin").write_bytes(bytes(blob))
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    render_index(args.out / "index.png", all_tables)
    print(f"{len(all_tables)} tables, {len(blob)} bytes -> {args.out}")


if __name__ == "__main__":
    main()
