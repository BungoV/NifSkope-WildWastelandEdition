#!/usr/bin/env bash
# LIGHTSYS2: build tests/cellcluster/cellclustertest (no Qt, no GL), run it green, then with --red (must FAIL).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
exe="${TMPDIR:-/tmp}/cellclustertest"
"${CXX:-g++}" -std=c++20 -O2 -pthread -I"$root/src/gl" "$root/tests/cellcluster/cellclustertest.cpp" \
	"$root/src/gl/cellcluster.cpp" -o "$exe" || { echo "BUILD FAILED"; exit 2; }
"$exe"; green=$?
"$exe" --red >/dev/null; red=$?
echo "green exit $green (want 0), red exit $red (want 1)"
[ "$green" -eq 0 ] && [ "$red" -ne 0 ] && echo "CELL CLUSTER UNIT PASS" && exit 0
echo "CELL CLUSTER UNIT FAIL"; exit 1
