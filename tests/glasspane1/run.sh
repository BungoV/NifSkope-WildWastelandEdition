#!/bin/bash
# Lane GLASSPANE1 (FIX1030B, 2026-10-30): the effect shader's env mask channel, green and red, on a census of the
# game's own files (tests/glasspane1/glass_census.py: Materials + texture BA2s, no NifSkope, no window).
#   tests/glasspane1/run.sh <out dir> [<Data dir> [<resource root> ...]]     (from the tree root; CXX defaults to g++)
# Green must print "GLASSPANE PASS 6"; the red build (-DGLASSPANE_RED = the base's G default) must print
# "FAIL rule:" and "FAIL census:" and exit 1. The census also prints the store glass twin and the receive-shadows
# verdict (section A); those are reports, not judged here. Runs are capped by timeout (a new exe never runs uncapped).
set -u
OUT="${1:?usage: run.sh <out dir> [<Data dir> [<resource root> ...]]}"
shift
PY="${PY:-$(command -v python)}"	# the caller's python (numpy); taken before the PATH below can shadow it
# lane-hygiene: a g++ test exe runs with the UCRT64 runtime first on PATH (another libstdc++ crashes it, rc 139)
[ -d /c/msys64/ucrt64/bin ] && export PATH="/c/msys64/ucrt64/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
export TMPDIR="${TMPDIR:-$OUT}"
bad=0
timeout 1200 "$PY" -I tests/glasspane1/glass_census.py "$OUT/census.tsv" "$@" < /dev/null > "$OUT/census.txt" 2>&1
rc=$?
cat "$OUT/census.txt"
[ $rc -eq 0 ] || { echo "CENSUS FAILED (rc $rc)"; exit 2; }
SRC=tests/glasspane1/glasspane_test.cpp
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" "$OUT/census.tsv" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GLASSPANE PASS 6$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
"$CXX" -O2 -std=c++17 -I src -DGLASSPANE_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/red.exe" "$OUT/census.tsv" > "$OUT/red.txt"; rc=$?
if [ $rc -eq 1 ] && grep -q '^FAIL rule:' "$OUT/red.txt" && grep -q '^FAIL census:' "$OUT/red.txt"; then
	echo "red (the base's G default): fails as it must ($(grep -c '^FAIL' "$OUT/red.txt") check(s)): $(grep '^census:' "$OUT/red.txt")"
else
	echo "RED DID NOT FAIL (rc $rc)"; cat "$OUT/red.txt"; bad=1
fi
[ $bad -eq 0 ] && echo "GLASSPANE GATE OK (green 6 + red)" || echo "GLASSPANE GATE FAILED"
exit $bad
