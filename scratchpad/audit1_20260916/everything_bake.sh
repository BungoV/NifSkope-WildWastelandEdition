#!/bin/bash
# AUDIT1 step 3: the bakes that leave the file types a DEFAULT bake does not, so
# that every invariant of step 3 has a real file to be measured on.
#
# A default FO4CS bake writes .BTR, .DDS, .lodb, .lodi, .lodj, .lodo and the
# manifests, and nothing else (measured, section 2). Three types are therefore
# missing from the step-2 trees and each needs its own run:
#
#   (A) --lodl <dir>   the landscape file. Its own run: it takes the command
#                      over and writes no chunks.
#   (B) --vt <dir>     the terrain tile pyramid, the .lodt files. `--lodt` is
#                      RETIRED (src/nifcli.cpp:7759 says so by name), so this is
#                      the only spelling that writes one.
#   (C) already done   `bake/aggreal` is the --impostors + --aggregate run, made
#                      against lane SHOWCASE1's card library; it is what gives
#                      this audit a REAL version-5 aggregate .lodi. It is not
#                      re-baked here.
#
# Every switch these runs add beyond the defaults is named above. Nothing here
# is a second measurement of the default bake.
#
# usage: bash everything_bake.sh
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OUT="$ROOT/scratchpad/audit1_20260916/bake/everything"
REGION="-20 24 -9 35"

rm -rf "$OUT"
mkdir -p "$OUT/lodl" "$OUT/vt/tex"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
WL="$(W "$OUT/lodl")"
WV="$(W "$OUT/vt")"

echo "=== (A) the landscape file, its own run  $(date '+%F %T')"
t0=$(date +%s)
# shellcheck disable=SC2086
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$WL" --lodl "$WL" \
	> "$OUT/lodl.log" 2>&1
rc=$?
t1=$(date +%s)
echo "(A) rc=$rc  $(( t1 - t0 )) s  $(find "$OUT/lodl" -type f | wc -l) files"
grep -aiE "^(error|refused|lodl|landscape|water)" "$OUT/lodl.log" | head -6 | sed 's/^/    /'

echo "=== (B) the terrain tile pyramid  $(date '+%F %T')"
t0=$(date +%s)
# shellcheck disable=SC2086
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$WV" --tex-dir "$WV/tex" --vt "$WV" \
	> "$OUT/vt.log" 2>&1
rc=$?
t1=$(date +%s)
echo "(B) rc=$rc  $(( t1 - t0 )) s  $(find "$OUT/vt" -type f | wc -l) files"
grep -aiE "^(error|refused|vt|tile|pyramid)" "$OUT/vt.log" | head -6 | sed 's/^/    /'

echo "--- what landed, by extension:"
find "$OUT" -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn | sed 's/^/    /'
echo "EVERYTHING-COMPLETE $(date '+%F %T')"
