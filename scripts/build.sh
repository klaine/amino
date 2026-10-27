#!/bin/sh
# Build Amino on macOS or Linux: optional codegen, native tests, the host
# engine library used by tools/emulator/render.py, aarch64
# dsp.so and amino-module.tar.gz. Run from anywhere:
#
#   scripts/build.sh            # tests (if present) + device build + package
#   scripts/build.sh --gen      # run tools/gen/gen.py first
#   scripts/build.sh --no-test  # skip native tests
#
# Needs zig (brew install zig) for the cross-compile; native tests use $CC
# (default cc). Set ZIG=/path/to/zig to override the zig lookup.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

GEN=0
TEST=1
for arg in "$@"; do
  case "$arg" in
    --gen) GEN=1 ;;
    --no-test) TEST=0 ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

PY=${PYTHON:-python3}
CC=${CC:-cc}
BUILD="$ROOT/build"
MODULE="$ROOT/modules/sound_generators/amino"
mkdir -p "$BUILD"

find_zig() {
  if [ -n "${ZIG:-}" ]; then echo "$ZIG"; return; fi
  if command -v zig >/dev/null 2>&1; then command -v zig; return; fi
  # pip install ziglang (the Windows setup in AGENTS.md uses this route)
  "$PY" -c 'import ziglang, os; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))' 2>/dev/null || true
}
ZIG=$(find_zig)
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then
  echo "zig not found: brew install zig (or set ZIG=...)" >&2
  exit 1
fi

if [ "$GEN" = 1 ]; then
  echo "== gen"
  "$PY" tools/gen/gen.py
fi

if [ "$TEST" = 1 ]; then
  if [ -f tests/contract/validate_contract.py ]; then
    echo "== contract"
    "$PY" tests/contract/validate_contract.py
  else
    echo "== contract: skipped (tests/contract/validate_contract.py missing)"
  fi
  if [ -f tests/host/test_main.c ]; then
    echo "== native test"
    "$CC" -std=c11 -O0 -I src/dsp -o "$BUILD/amino_test" src/dsp/*.c tests/host/test_main.c -lm
    "$BUILD/amino_test"
  else
    echo "== native test: skipped (tests/host/test_main.c missing)"
  fi
  if [ -f tests/host/test_mod_matrix_ui.mjs ] && command -v node >/dev/null 2>&1; then
    echo "== mod matrix ui test"
    node tests/host/test_mod_matrix_ui.mjs
  else
    echo "== mod matrix ui test: skipped (node or tests/host/test_mod_matrix_ui.mjs missing)"
  fi
  if [ -f tests/host/test_web_ui.mjs ] && command -v node >/dev/null 2>&1; then
    echo "== remote ui test"
    node tests/host/test_web_ui.mjs
  else
    echo "== remote ui test: skipped (node or tests/host/test_web_ui.mjs missing)"
  fi
  if [ -f tests/golden/golden_main.c ] && [ -f tests/golden/golden.json ]; then
    echo "== golden"
    "$CC" -std=c11 -O2 -I src/dsp -o "$BUILD/amino_golden" src/dsp/*.c tests/golden/golden_main.c -lm
    "$PY" tests/golden/check_golden.py "$BUILD/amino_golden"
  else
    echo "== golden: skipped (tests/golden sources or golden.json missing)"
  fi
fi

echo "== host engine library (preview renderer)"
case "$(uname -s)" in
  Darwin) LIBEXT=dylib ;;
  *) LIBEXT=so ;;
esac
"$CC" -std=c11 -O2 -fPIC -shared -I src/dsp -o "$BUILD/libamino_host.$LIBEXT" \
  src/dsp/*.c tools/emulator/c_host.c -lm

echo "== dsp.so (aarch64-linux-gnu, $("$ZIG" version))"
# -ffile-prefix-map: no absolute build paths (and so no user name) in shipped binaries
"$ZIG" cc -target aarch64-linux-gnu.2.28 -std=c11 -O2 -fPIC -shared -ffile-prefix-map="$ROOT"=. -I src/dsp \
  -o "$MODULE/dsp.so" src/dsp/*.c -lm -ldl
# zig leaves a .so.o next to the output on some versions
rm -f "$MODULE/dsp.so.o"

echo "== package"
# COPYFILE_DISABLE keeps macOS tar from adding ._ AppleDouble files
COPYFILE_DISABLE=1 tar -czf "$ROOT/amino-module.tar.gz" \
  --exclude '.DS_Store' -C "$ROOT/modules/sound_generators" amino
echo "built $MODULE/dsp.so and $ROOT/amino-module.tar.gz"
