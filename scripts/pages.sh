#!/bin/sh
# Put build/amino.html on the `pages` branch as index.html, for GitHub Pages (serve from that branch).
#   scripts/pages.sh             # update the local pages branch (scripts/build_web.sh runs this)
#   scripts/pages.sh --push      # also push it to origin
#   scripts/pages.sh --gallery   # also re-render the preset gallery (tools/emulator/gallery.py --site)
#
# The branch is written with git plumbing, so the working tree and the current branch are left
# alone. It holds one commit with no history (index.html, .nojekyll and gallery/), replaced only
# when something changes, so a multi-MB file doesn't pile up per build; that's why the push is
# forced. gallery/ (324 MP3 previews) is carried over from the pages branch (local, else origin's) unless
# --gallery re-renders it, which needs the lameenc package and takes a while.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

PUSH=0
GALLERY=0
for arg in "$@"; do
  case "$arg" in
    --push) PUSH=1 ;;
    --gallery) GALLERY=1 ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "== pages: not a git checkout, skipped"
  exit 0
fi
[ -f build/amino.html ] || { echo "build/amino.html missing: run scripts/build_web.sh" >&2; exit 1; }

# The tree is assembled in a throwaway index, so the real one is never touched.
IDX=$(mktemp -u)
TMP=""
trap 'rm -rf "$IDX" "$TMP"' EXIT
export GIT_INDEX_FILE="$IDX"
git update-index --add --cacheinfo "100644,$(git hash-object -w /dev/null),.nojekyll"
git update-index --add --cacheinfo "100644,$(git hash-object -w build/amino.html),index.html"
if [ "$GALLERY" = 1 ]; then
  TMP=$(mktemp -d)
  python3 tools/emulator/gallery.py --site "$TMP"
  (cd "$TMP" && find . -type f | sed 's#^\./##' | sort) | while IFS= read -r f; do
    git update-index --add --cacheinfo "100644,$(git hash-object -w "$TMP/$f"),gallery/$f"
  done
elif PREV=$(git rev-parse -q --verify refs/heads/pages || git rev-parse -q --verify refs/remotes/origin/pages) &&
     git rev-parse -q --verify "$PREV:gallery" >/dev/null; then
  git read-tree --prefix=gallery/ "$PREV:gallery"
else
  echo "== pages: no gallery/ on the pages branch (run scripts/pages.sh --gallery)"
fi
TREE=$(git write-tree)
unset GIT_INDEX_FILE

if [ "$(git rev-parse -q --verify 'refs/heads/pages^{tree}' || true)" = "$TREE" ]; then
  echo "== pages: unchanged"
else
  SRC=$(git rev-parse --short HEAD)
  git diff --quiet HEAD -- || SRC="$SRC with local changes"
  git update-ref refs/heads/pages "$(git commit-tree "$TREE" -m "Amino standalone synth, built from $SRC")"
  echo "== pages: index.html from $SRC"
fi

if [ "$PUSH" = 1 ]; then
  if git rev-parse -q --verify refs/remotes/origin/pages >/dev/null; then
    git push --force-with-lease=pages:refs/remotes/origin/pages origin pages
  else
    git push origin pages
  fi
fi
