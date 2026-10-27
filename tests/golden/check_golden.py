#!/usr/bin/env python3
"""Golden-audio regression check for the Amino C engine (docs/08 3.2).

Usage:
    py -3 tests/golden/check_golden.py <golden_main.exe>
    py -3 tests/golden/check_golden.py <golden_main.exe> --write

`--write` replaces tests/golden/golden.json with the current render.  Without
it the committed file is compared using spectral + RMS tolerances.
"""
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GOLDEN = HERE / "golden.json"
FACTORY = HERE.parents[1] / "presets" / "factory"

# Relative tolerances; libm/ISA differences between build hosts are the reason
# these are not exact.
TOL = {
    "rms": 0.12,
    "peak": 0.20,
    "cent": 0.15,
    "zcr": 0.20,
}
ABS = {
    "rms": 3.0,
    "peak": 300.0,
    "cent": 60.0,
    "zcr": 0.004,
}


def run(exe):
    proc = subprocess.run([str(exe)], capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit("golden renderer failed:\n" + proc.stderr)
    rows = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    expected = len(list(FACTORY.glob("*.json")))
    if len(rows) != expected:
        sys.exit("expected %d preset rows, got %d" % (expected, len(rows)))
    for i, row in enumerate(rows):
        if row["p"] != i:
            sys.exit("preset rows out of order")
        for key in ("rms", "peak", "cent", "zcr"):
            if not math.isfinite(row[key]):
                sys.exit("non-finite %s for preset %d" % (key, row["p"]))
    return rows


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    exe = Path(sys.argv[1])
    if not exe.exists():
        sys.exit("renderer not found: %s" % exe)
    rows = run(exe)

    if "--write" in sys.argv:
        GOLDEN.write_text(json.dumps(rows, indent=1) + "\n", encoding="utf-8")
        print("wrote %s (%d presets)" % (GOLDEN.name, len(rows)))
        return 0

    if not GOLDEN.exists():
        sys.exit("missing %s; run with --write once" % GOLDEN)
    golden = json.loads(GOLDEN.read_text(encoding="utf-8"))
    if len(golden) != len(rows):
        sys.exit("golden has %d rows, render has %d" % (len(golden), len(rows)))

    bad = 0
    worst = {k: (0.0, -1) for k in TOL}
    for want, got in zip(golden, rows):
        if want["p"] != got["p"]:
            sys.exit("preset index mismatch")
        for key, rel in TOL.items():
            delta = abs(got[key] - want[key])
            limit = max(ABS[key], rel * abs(want[key]))
            if limit > 0.0:
                ratio = delta / limit
                if ratio > worst[key][0]:
                    worst[key] = (ratio, want["p"])
            if delta > limit:
                bad += 1
                print("preset %3d %-4s want %.4f got %.4f (delta %.4f > %.4f)"
                      % (want["p"], key, want[key], got[key], delta, limit))

    for key, (ratio, preset) in worst.items():
        where = "preset %d" % preset if preset >= 0 else "exact"
        print("worst %-4s %.0f%% of tolerance (%s)" % (key, ratio * 100.0, where))
    if bad:
        sys.exit("golden check FAILED (%d metric(s))" % bad)
    print("golden OK: %d presets within tolerance" % len(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
