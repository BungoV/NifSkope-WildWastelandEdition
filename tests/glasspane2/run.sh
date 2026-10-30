#!/bin/bash
# Lane GLASSPANE2 (FIX1030B round 2, 2026-10-30): two gates, green + reds, offline (no NifSkope, no window).
#   tests/glasspane2/run.sh <out dir> [<Data dir> [<resource root> ...]]     (from the tree root; CXX defaults to g++)
# A  BGSMRECV1 / BACKLIGHTFLAG1: tests/glasspane2/bgsm_census.py on the game's BA2s. Green must print
#    "BGSMRECV PASS 7" (our BGSM reader read to every file's last byte and agreeing with the NIFs Bethesda exported from
#    the same materials). Reds: RED=short (one byte short) and RED=swap (cast/receive/hide reordered) must print
#    "BGSMRECV FAIL" and exit 1. The reds walk a 3000-NIF sample (MAXNIF), the green every NIF (~1.5 min).
# B  FXNORMALA1: tests/glasspane2/glasspane2_test.cpp on src/gl/glasspanerules.h. Green "GLASSPANE2 PASS 4"; the red
#    build -DGLASSPANE2_RED (the base: no normal-alpha factor) must print "FAIL fixture:" and exit 1.
set -u
OUT="${1:?usage: run.sh <out dir> [<Data dir> [<resource root> ...]]}"
shift
PY="${PY:-$(command -v python)}"	# the caller's python; taken before the PATH below can shadow it
[ -d /c/msys64/ucrt64/bin ] && export PATH="/c/msys64/ucrt64/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
export TMPDIR="${TMPDIR:-$OUT}"
bad=0

# ---- A: the BGSM reader against the game's own files
MAXNIF=0 RED= timeout 900 "$PY" -I tests/glasspane2/bgsm_census.py "$OUT/census.txt" "$@" < /dev/null > "$OUT/census.out" 2>&1
rc=$?
grep -E '^(R1|R2 bit  (castShadows|hair|skinTint|tessellate)|R2 float (grayscale|backLight)|R2 receive|BL VERDICT|BGSMRECV)' "$OUT/census.out"
if [ $rc -ne 0 ] || ! grep -q '^BGSMRECV PASS 7$' "$OUT/census.out"; then echo "A GREEN FAILED (rc $rc)"; bad=1; fi
for r in short swap; do
	MAXNIF=3000 RED=$r timeout 900 "$PY" -I tests/glasspane2/bgsm_census.py "$OUT/census_$r.txt" "$@" < /dev/null > "$OUT/census_$r.out" 2>&1
	rc=$?
	if [ $rc -eq 1 ] && grep -q '^BGSMRECV FAIL' "$OUT/census_$r.out"; then
		echo "A red $r: fails as it must ($(grep -c '^BGSMRECV FAIL' "$OUT/census_$r.out") check(s)): $(grep -m1 '^BGSMRECV FAIL' "$OUT/census_$r.out")"
	else
		echo "A RED $r DID NOT FAIL (rc $rc)"; tail -5 "$OUT/census_$r.out"; bad=1
	fi
done

# ---- B: the env cube x the normal map's alpha
SRC=tests/glasspane2/glasspane2_test.cpp
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" . > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GLASSPANE2 PASS 4$' "$OUT/green.txt"; then echo "B GREEN FAILED (rc $rc)"; bad=1; fi
"$CXX" -O2 -std=c++17 -I src -DGLASSPANE2_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/red.exe" . > "$OUT/red.txt"; rc=$?
if [ $rc -eq 1 ] && grep -q '^FAIL fixture:' "$OUT/red.txt"; then
	echo "B red (the base, no factor): fails as it must ($(grep -c '^FAIL' "$OUT/red.txt") check(s)): $(grep '^FAIL fixture:' "$OUT/red.txt")"
else
	echo "B RED DID NOT FAIL (rc $rc)"; cat "$OUT/red.txt"; bad=1
fi
[ $bad -eq 0 ] && echo "GLASSPANE2 GATE OK (A green + 2 reds, B green 4 + red)" || echo "GLASSPANE2 GATE FAILED"
exit $bad
