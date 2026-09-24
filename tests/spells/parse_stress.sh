#!/bin/bash
#
# THE MODEL LAYER ON N THREADS, WITH NOTHING ELSE IN THE PICTURE.
#
# Lane NIFPARSE1, 2026-09-11. BAKEPERF1 concluded "the NIF parser is not
# thread-safe" from one stack taken in the middle of a whole chunk bake -- a run
# that is the parser AND the plugin reader AND the texture cache AND the archive
# layer AND the message sink all at once. That stack names where the damage was
# DETECTED; for STATUS_HEAP_CORRUPTION that is not where it was done.
#
# This spell is the experiment that separates them. `parsestress` reads the NIFs
# once on the calling thread and then builds, loads, walks and destroys
# NifModels from those bytes on N threads -- no EsmWorld, no texture cache, no
# archive lookup, no road gatherer, no file I/O inside the threaded region.
#
#   C1 the model layer itself   -> this goes red
#   C2 the shared BA2File       -> this stays green and the BAKE still faults
#   C3 QMessageBox on a worker  -> this stays green and the BAKE still faults
#
# It is not a crash detector only: every worker digests what it read BACK out of
# the document and must reproduce the single-threaded reference digest exactly,
# so silent corruption that happens not to fault still fails.
#
# CHECKS
#   S1  the sabotage floor fires: one flipped byte for one worker MUST go red
#       (if it does not, the digest is hashing a constant and S2 means nothing)
#   S2  N threads x R reps, no fault, no mismatch, exit 0
#   S3  the run really did the work: loads == threads x reps x files
#   S4  every fixture loaded and digested more than 32 items
#
# USAGE
#   bash tests/spells/parse_stress.sh            # 16 threads, 8 reps
#   THREADS=4 REPS=2 bash tests/spells/parse_stress.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
THREADS="${THREADS:-16}"
REPS="${REPS:-8}"
LOG="$ROOT/release/ww_parse_stress.log"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }

# The fixtures: whatever big, real NIFs the tree carries. A fixture list that
# came out empty is a FAILURE below, never a silent skip.
FIX=()
for f in "$ROOT/fixtures/human_male_vanilla.nif" \
         "$ROOT/release/barn.nif" \
         "$ROOT/tests/render/refraction_fixture.nif" \
         "$ROOT/release/ww_render_shot/cube_lod.nif"; do
	[ -f "$f" ] && FIX+=("$f")
done

fails=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

if [ ${#FIX[@]} -lt 2 ]; then
	bad "S0 fewer than two fixtures found - a stress run over nothing passes trivially"
	echo "RESULT FAIL"
	exit 1
fi
ok "S0 ${#FIX[@]} fixtures"

first="${FIX[0]}"
rest=()
for ((i = 1; i < ${#FIX[@]}; i++)); do rest+=(--stress-file "${FIX[$i]}"); done

: > "$LOG"

# ---- S1 the floor, FIRST: a check nobody has seen fail is not a check -------
echo "== S1 sabotage floor ==" | tee -a "$LOG"
"$NS" -no-gui parsestress "$first" "${rest[@]}" \
	--stress-threads 4 --stress-reps 1 --stress-sabotage digest >> "$LOG" 2>&1
rc=$?
if [ $rc -eq 0 ] && grep -q "floor fired as it must" "$LOG"; then
	ok "S1 one flipped byte is seen - the digest reaches the parse"
else
	bad "S1 the sabotage did NOT go red (rc=$rc) - the digest proves nothing"
fi

# ---- S2/S3/S4 the real run --------------------------------------------------
echo "== S2 ${THREADS} threads x ${REPS} reps ==" | tee -a "$LOG"
out="$("$NS" -no-gui parsestress "$first" "${rest[@]}" \
	--stress-threads "$THREADS" --stress-reps "$REPS" 2>&1)"
rc=$?
echo "$out" >> "$LOG"

# an NTSTATUS comes back as a large unsigned value through bash
if [ $rc -eq 0 ]; then
	ok "S2 exit 0 at ${THREADS} threads"
else
	bad "S2 FAULTED or failed, exit $rc (0xC0000374 = heap corruption)"
fi

if echo "$out" | grep -q "mismatches 0,"; then
	ok "S2b no worker's digest differed from the one-thread reference"
else
	bad "S2b a worker read back something different: $(echo "$out" | grep -m1 'first mismatch')"
fi

want=$(( THREADS * REPS * ${#FIX[@]} ))
got=$(echo "$out" | sed -n 's/.*loads \([0-9]*\),.*/\1/p' | head -1)
if [ "${got:-0}" = "$want" ]; then
	ok "S3 loads $got == threads x reps x files"
else
	bad "S3 loads ${got:-none}, expected $want - the run did not do the work"
fi

if echo "$out" | grep -q "FAIL fixture"; then
	bad "S4 a fixture did not load or digested too few items"
else
	ok "S4 every fixture loaded and was walked"
fi

echo "log: $LOG"
echo "checks $((4 + 1)), failures $fails"
if [ $fails -eq 0 ]; then echo "RESULT PASS"; else echo "RESULT FAIL"; fi
[ $fails -eq 0 ]
