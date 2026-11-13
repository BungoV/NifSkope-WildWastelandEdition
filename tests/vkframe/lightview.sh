#!/bin/bash
# Lane VKLIGHT1 (2026-11-13, plan rows L1-L3): AO, contact lines and the SSGI cast through the shipped lightRun
# (vkpresent.cpp, the present device of a 4x MSAA window) against GL. tests/vklight/light.sh makes the jobs (GL references,
# red builds) and runs the offline gate first; tests/vkframe/lightview.cpp then runs every job through lightRun.
# bash tests/vkframe/lightview.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]. Needs what light.sh needs.
# LIGHTVIEW_JOBS="<jobs dir> ..." skips light.sh and runs the jobs already in those dirs (red spv from <out dir>/light/spv).
# Not while NifSkope or the game runs (a window of ours). Last line: `LIGHTVIEW GATE OK` or `LIGHTVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if powershell -NoProfile -Command "if (Get-Process -Name NifSkope,Fallout4 -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"; then
	echo "LIGHTVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1
fi
mkdir -p "$OUT"
DIRS="${LIGHTVIEW_JOBS:-}"
if [ -z "$DIRS" ]; then
	bash "$ROOT/tests/vklight/light.sh" "$OUT/light" "${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}" ao contact ssgi > "$OUT/light.txt" 2>&1
	tail -1 "$OUT/light.txt"
	tail -1 "$OUT/light.txt" | grep -q "VKLIGHT GATE OK" || { echo "LIGHTVIEW GATE FAILED (the offline gate)"; exit 1; }
	DIRS="$OUT/light/jobs"
fi
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/lightview.cpp" -o "$OUT/lightview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "LIGHTVIEW GATE FAILED (g++)"; exit 1; }
: > "$OUT/lightview.txt"
G=0; R=0; BAD=0
for d in $DIRS; do
	JOBS=$(ls "$d" | grep -E '^(ao|ct|sg)_' | tr '\n' ' ')
	"$OUT/lightview.exe" "$d" "$OUT/light/spv" $JOBS > "$OUT/lv_one.txt" 2>&1
	cat "$OUT/lv_one.txt" | tee -a "$OUT/lightview.txt"
	tail -1 "$OUT/lv_one.txt" | grep -q "LIGHTVIEW GATE OK" || BAD=1
	set -- $(grep -E '^greens [0-9]+, reds [0-9]+$' "$OUT/lv_one.txt" | tr -d ',' | awk '{print $2, $4}')
	G=$((G + ${1:-0})); R=$((R + ${2:-0}))
done
echo "all dirs: greens $G, reds $R" | tee -a "$OUT/lightview.txt"
[ "$G" = 12 ] && [ "$R" = 7 ] || { echo "LIGHTVIEW GATE FAILED (12 jobs and 7 reds expected)" | tee -a "$OUT/lightview.txt"; exit 1; }
[ "$BAD" = 0 ] && echo "LIGHTVIEW GATE OK" | tee -a "$OUT/lightview.txt" || { echo "LIGHTVIEW GATE FAILED" | tee -a "$OUT/lightview.txt"; exit 1; }
