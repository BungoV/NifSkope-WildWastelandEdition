#!/usr/bin/env bash
# STREAMTEX1: build tests/streamtex/streamtex_test (no Qt, no GL), run it green, then the three reds (each must FAIL).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
exe="${TMPDIR:-/tmp}/streamtex_test"
"${CXX:-g++}" -std=c++17 -O2 -I"$root/src/gl" "$root/tests/streamtex/streamtex_test.cpp" -o "$exe" || { echo "BUILD FAILED"; exit 2; }
"$exe"; green=$?
"$exe" --red; red=$?            # WW_CELL_STREAMTEX_RED=off: the keep timer frees loaded cells' textures
"$exe" --red-keep0; redk=$?     # the red with WW_CELL_STREAM_TEXKEEP=0: keep-all, memory grows
"$exe" --red-norefs; redn=$?    # no count by cell: the pool frees what a loaded cell names
echo "green exit $green (want 0), red exit $red, red-keep0 exit $redk, red-norefs exit $redn (want 1 each)"
[ "$green" -eq 0 ] && [ "$red" -eq 1 ] && [ "$redk" -eq 1 ] && [ "$redn" -eq 1 ] && echo "STREAM TEX UNIT PASS" && exit 0
echo "STREAM TEX UNIT FAIL"; exit 1
