#!/usr/bin/env bash
# STREAMTEX2: build tests/streamtex2/streamtex2_test (no Qt, no GL), run it green, then its four reds (each must FAIL),
# then STREAMTEX1's own unit (tests/spells/stream_tex_unit.sh) on the changed pool header (its results must not move).
# OUT = a scratch folder for the exes (default ${TMPDIR:-/tmp}/streamtex2).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${OUT:-${TMPDIR:-/tmp}/streamtex2}"
mkdir -p "$out"
exe="$out/streamtex2_test"
"${CXX:-g++}" -std=c++17 -O2 -Wall -I"$root/src/gl" "$root/tests/streamtex2/streamtex2_test.cpp" -o "$exe" \
	|| { echo "BUILD FAILED"; exit 2; }
"$exe"; green=$?
"$exe" --red-unfenced; rf=$?    # WW_CELL_STREAMTEX_RED=unfenced (= nofence): a queued frame's handle made non-resident
"$exe" --red-handle; rh=$?      # the delete without the handle drop: a recycled name returns a dead handle
"$exe" --red-busytrim; rb=$?    # WW_CELL_STREAMTEX_RED=busytrim: the incoming row's textures freed, then reloaded
"$exe" --red-eager; re=$?       # round 2, WW_CELL_STREAMTEX_RED=eager: the pool frees inside its budget, row (e)
TMPDIR="$out" bash "$root/tests/spells/stream_tex_unit.sh"; st1=$?
echo "green exit $green (want 0), red-unfenced $rf, red-handle $rh, red-busytrim $rb, red-eager $re (want 1 each)," \
	"STREAMTEX1 unit $st1 (want 0)"
[ "$green" -eq 0 ] && [ "$rf" -eq 1 ] && [ "$rh" -eq 1 ] && [ "$rb" -eq 1 ] && [ "$re" -eq 1 ] && [ "$st1" -eq 0 ] \
	&& echo "STREAMTEX2 UNIT PASS" && exit 0
echo "STREAMTEX2 UNIT FAIL"; exit 1
