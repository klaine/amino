#!/bin/sh
# Build the static WebAssembly site (the synth in a browser, with WebMIDI):
#
#   scripts/build_web.sh            # tests + build/site
#   scripts/build_web.sh --no-test  # skip the node tests
#
# Compiles src/dsp to build/site/amino.wasm, copies web/ and the packaged
# Remote UI (modules/sound_generators/amino, produced by scripts/build.sh --gen)
# into build/site/.  Serve it with: python3 -m http.server -d build/site
# Also writes build/amino.html: the whole synth as ONE self-contained file
# (scripts/build_single.py), and a copy as build/site/amino-standalone.html, then updates the
# local pages branch from it (scripts/pages.sh).
# Needs zig (brew install zig, or pip install ziglang) and node for the tests.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

TEST=1
for arg in "$@"; do
  case "$arg" in
    --no-test) TEST=0 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

PY=${PYTHON:-python3}
find_zig() {
  if [ -n "${ZIG:-}" ]; then echo "$ZIG"; return; fi
  if command -v zig >/dev/null 2>&1; then command -v zig; return; fi
  "$PY" -c 'import ziglang, os; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))' 2>/dev/null || true
}
ZIG=$(find_zig)
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then
  echo "zig not found: brew install zig (or set ZIG=...)" >&2
  exit 1
fi

MODULE="$ROOT/modules/sound_generators/amino"
SITE="$ROOT/build/site"
if [ ! -f "$MODULE/assets/ui_meta.json" ]; then
  echo "missing $MODULE/assets: run scripts/build.sh --gen first" >&2
  exit 1
fi
mkdir -p "$ROOT/build"

echo "== amino.wasm ($("$ZIG" version))"
# AMINO_WASM drops the device-only 64-bit ABI assert in plugin_api.h.
# -ffile-prefix-map and --strip-all (as Crest does): no absolute build paths, and so no user name, in
# the published wasm. The strip also drops the debug info of zig's cached wasi-libc.
"$ZIG" cc -target wasm32-wasi -mexec-model=reactor -DAMINO_WASM -std=c11 -O2 -ffile-prefix-map="$ROOT"=. \
  -I src/dsp -o "$ROOT/build/amino.wasm" src/dsp/*.c web/wasm_host.c -lm \
  -Wl,--no-entry -Wl,-z,stack-size=1048576 -Wl,--strip-all \
  -Wl,--initial-memory=33554432 -Wl,--max-memory=134217728 -Wl,--export-dynamic

if [ "$TEST" = 1 ]; then
  if command -v node >/dev/null 2>&1; then
    echo "== web tests"
    node tests/web/test_router.mjs
    node tests/web/test_wasm.mjs
  else
    echo "== web tests: skipped (node missing)"
  fi
fi

echo "== site"
rm -rf "$SITE"
mkdir -p "$SITE/static" "$SITE/remote-ui/amino"
cp web/index.html web/app.js web/engine.js web/bridge.js web/router.js web/worklet.js "$SITE/"
cp web/schwung-remote-api.js "$SITE/static/"
cp "$ROOT/build/amino.wasm" "$SITE/"
cp -R "$MODULE/assets" "$SITE/remote-ui/amino/assets"
cp "$MODULE/ui_pages.json" "$MODULE/help.json" "$SITE/remote-ui/amino/"
# The module page loads the bridge from an absolute /static path; make it
# relative so the site also works under a sub-path (GitHub Pages project sites).
"$PY" - "$MODULE/web_ui.html" "$SITE/remote-ui/amino/web_ui.html" <<'PYEOF'
import sys
src = open(sys.argv[1], encoding="utf-8").read()
old = 'src="/static/schwung-remote-api.js"'
assert old in src, "web_ui.html no longer loads /static/schwung-remote-api.js"
open(sys.argv[2], "w", encoding="utf-8").write(src.replace(old, 'src="../../static/schwung-remote-api.js"'))
PYEOF
touch "$SITE/.nojekyll"

echo "== single file"
"$PY" scripts/build_single.py --out "$ROOT/build/amino.html"
cp "$ROOT/build/amino.html" "$SITE/amino-standalone.html"
echo "built $SITE ($(du -sh "$SITE" | cut -f1))"
sh scripts/pages.sh   # build/amino.html as index.html on the local pages branch (push: scripts/pages.sh --push)
