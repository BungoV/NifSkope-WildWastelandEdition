#!/bin/bash
# Lane VKLIGHT1 (2026-11-13, plan rows L1-L3): AO, contact lines and the SSGI cast through the shipped lightRun
# (vkpresent.cpp, the present device of a 4x MSAA window) against GL. tests/vklight/light.sh makes the jobs (GL references,
# red builds) and runs the offline gate first; tests/vkframe/lightview.cpp then runs every job through lightRun.
# bash tests/vkframe/lightview.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]. Needs what light.sh needs.
# Two sets by default: light.sh's fresh jobs from the dump (mem_base) plus the kept set of round 1 (LIGHTVIEW_KEPT: job dirs
# from another dump, its .hdr gone; set it empty to skip). LIGHTVIEW_JOBS="<jobs dir> ..." skips light.sh and runs only the
# jobs already in those dirs (red spv from <out dir>/light/spv).
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
	bash "$ROOT/tests/vklight/light.sh" "$OUT/light" "${2:-E:/Projects/Claude/notes/fogglow1/h1/out/mem_base.hdr}" ao contact ssgi > "$OUT/light.txt" 2>&1
	tail -1 "$OUT/light.txt"
	tail -1 "$OUT/light.txt" | grep -q "VKLIGHT GATE OK" || { echo "LIGHTVIEW GATE FAILED (the offline gate)"; exit 1; }
	DIRS="$OUT/light/jobs ${LIGHTVIEW_KEPT-E:/Projects/Claude/notes/vklight1/run3/jobs E:/Projects/Claude/notes/vklight1/run4/jobs}"
fi
NEED_G=0; NEED_R=0
for d in $DIRS; do
	[ -d "$d" ] || { echo "LIGHTVIEW GATE FAILED (no job dir $d)"; exit 1; }
	NEED_G=$((NEED_G + $(ls "$d" | grep -cE '^(ao|ct|sg)_')))
	NEED_R=$((NEED_R + $(ls "$d"/ao_*/red_*.txt "$d"/ct_*/red_*.txt "$d"/sg_*/red_*.txt 2>/dev/null | wc -l)))
done
# the default two sets: 12 jobs and 7 reds each
[ -n "${LIGHTVIEW_JOBS:-}" ] || [ -n "${LIGHTVIEW_KEPT+x}" ] || [ "$NEED_G $NEED_R" = "24 14" ] \
	|| { echo "LIGHTVIEW GATE FAILED (the two sets hold $NEED_G jobs and $NEED_R reds, 24 and 14 expected)"; exit 1; }
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
[ "$G" = "$NEED_G" ] && [ "$R" = "$NEED_R" ] && [ "$G" -gt 0 ] \
	|| { echo "LIGHTVIEW GATE FAILED ($NEED_G jobs and $NEED_R reds expected)" | tee -a "$OUT/lightview.txt"; exit 1; }
[ "$BAD" = 0 ] && echo "LIGHTVIEW GATE OK" | tee -a "$OUT/lightview.txt" || { echo "LIGHTVIEW GATE FAILED" | tee -a "$OUT/lightview.txt"; exit 1; }
