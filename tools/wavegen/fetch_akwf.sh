#!/bin/sh
# Fetch the CC0 AKWF-FREE single-cycle waveforms, pinned to a known commit.
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)/.cache"
COMMIT=8de90bf94376670947369e69de0af6b9fbd19286
mkdir -p "$DIR"
cd "$DIR"
if [ ! -d AKWF-FREE ]; then
  git clone --filter=blob:none --sparse https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE.git
fi
cd AKWF-FREE
git sparse-checkout set AKWF
git fetch --depth 1 origin "$COMMIT"
git checkout --quiet "$COMMIT"
echo "AKWF-FREE @ $COMMIT ready in $DIR/AKWF-FREE/AKWF"
