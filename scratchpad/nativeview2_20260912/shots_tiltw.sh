#!/usr/bin/env bash
# Lane NATIVEVIEW2 -- gate (d)'s direction control, added after the first pass.
# Same cameras and same file as shots.sh's terrain arms; only the sheet cache
# changes (tiltwcache: the +X tilt mirrored to -X).
#
#   bash shots_tiltw.sh <exe> <tag>
#
# The game check is NOT in here: it is its own command, run and read before this.
set -u
EXE="$1"; TAG="$2"
ROOT="E:/Projects/NifskopeWildWastelandEdition"
R="$ROOT/scratchpad/nativeview2_20260912"
R1="$ROOT/scratchpad/nativeview1_20260912"
S="$ROOT/scratchpad/showcase1_20260912/out"
W="$R/work/$TAG"
LOG="$R/work/$TAG.tiltw.log"
PORT="${PORT:-42934}"
mkdir -p "$W"
: > "$LOG"

LODL="$S/lodl/Terrain/Commonwealth.lodl"
SHEETS="$S/look/mod/Terrain"
OBJ="$S/look/obj"
RES="$R1/resroot;$OBJ"
TILTW="$R/work/tiltwcache"

CEN="-73728,106496,0"
CENO="-73728,106496,8500"
ORT=8192

for v in 1 8; do
	if [ "$v" = 1 ]; then n=t_tiltw_top; c="$CEN"; else n=t_tiltw_obl; c="$CENO"; fi
	echo "=== $n" >> "$LOG"
	env WW_WINDOW_AT=1960,40 \
		WW_RENDER_SHOT="$W/$n.png" WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_VIEW="$v" WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$c" WW_RENDER_ORTHO="$ORT" \
		WW_PROGRAM_CENSUS="$W/$n.census.txt" \
		WW_LODL_REGION="-20,24,-17,27,2" \
		WW_LODL_SHEETS="$SHEETS" \
		WW_LODL_SHEET_CACHE="$TILTW" \
		WW_LODGEN_RESOURCES="$RES" \
		timeout 300 "$EXE" --port "$PORT" "$LODL" >> "$LOG" 2>&1
	rc=$?
	echo "  $n rc=$rc $(stat -c %s "$W/$n.png" 2>/dev/null || echo NO) B"
done
echo "log: $LOG"
