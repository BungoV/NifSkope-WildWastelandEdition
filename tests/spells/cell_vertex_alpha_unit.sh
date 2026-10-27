#!/usr/bin/env bash
# TERRAINBLOCK1: build tests/cellvertexalpha/cellvertexalphatest (no Qt, no GL), run it green, then with --red (must FAIL).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
exe="${TMPDIR:-/tmp}/cellvertexalphatest"
"${CXX:-g++}" -std=c++20 -O2 -I"$root/src" "$root/tests/cellvertexalpha/cellvertexalphatest.cpp" -o "$exe" \
	|| { echo "BUILD FAILED"; exit 2; }
"$exe"; green=$?
"$exe" --red; red=$?
echo "green exit $green (want 0), red exit $red (want 1)"
[ "$green" -eq 0 ] && [ "$red" -ne 0 ] && echo "CELL VERTEX ALPHA UNIT PASS" && exit 0
echo "CELL VERTEX ALPHA UNIT FAIL"; exit 1
