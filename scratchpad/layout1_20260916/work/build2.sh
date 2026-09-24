#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 1
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
D=/e/Projects/NifskopeWildWastelandEdition/scratchpad/layout1_20260916/work/tmp
mkdir -p "$D"
W="$(cd "$D" && pwd -W)"
export TMPDIR="$W" TMP="$W" TEMP="$W"
echo "TMP=$TMP"
make -j2 > scratchpad/layout1_20260916/work/build2.log 2>&1
echo "make rc=$?"
