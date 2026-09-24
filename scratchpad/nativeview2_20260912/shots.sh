#!/usr/bin/env bash
# Lane NATIVEVIEW2 -- the picture/measurement set, one exe per run.
#
#   bash shots.sh <exe> <tag>
#
# <exe> is an ABSOLUTE path (release/NifSkope.exe or the rung copy), <tag> names
# the output subdirectory under work/.  Every path in argv and every WW_* path is
# absolute E:/... (the render-shot skill: a relative WW_RENDER_SHOT writes
# nothing and says nothing).
#
# The game check is NOT in here: it is its own command, run and read before this.
set -u
EXE="$1"; TAG="$2"
ROOT="E:/Projects/NifskopeWildWastelandEdition"
R="$ROOT/scratchpad/nativeview2_20260912"
R1="$ROOT/scratchpad/nativeview1_20260912"
S="$ROOT/scratchpad/showcase1_20260912/out"
W="$R/work/$TAG"
LOG="$R/work/$TAG.log"
PORT="${PORT:-42933}"
mkdir -p "$W"
: > "$LOG"

LODL="$S/lodl/Terrain/Commonwealth.lodl"
LODI="$S/look/native/Commonwealth.lodi"
SHEETS="$S/look/mod/Terrain"
OBJ="$S/look/obj"
RES="$R1/resroot;$OBJ"

OWN="$R1/work/sheetcache"            # the bake's own normal tiles
FLAT="$R/work/flatcache"             # every .n.DDS replaced by (128,255,128)
TILT="$R/work/tiltcache"             # ... by the +X tilt of gate (d)

# chunk (-20,24) at dim 4: the same cameras NATIVEVIEW1 used
CEN="-73728,106496,0"
CENO="-73728,106496,8500"
LCEN="8192,8192,0"
LCENO="8192,8192,8500"
ORT=8192

shot() {  # shot <name> <file> <view> <center> <ortho> [extra env...]
	local name="$1" file="$2" view="$3" ctr="$4" ort="$5"; shift 5
	echo "=== $name" >> "$LOG"
	env "$@" \
		WW_WINDOW_AT=1960,40 \
		WW_RENDER_SHOT="$W/$name.png" WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ort" \
		WW_PROGRAM_CENSUS="$W/$name.census.txt" \
		timeout 300 "$EXE" --port "$PORT" "$file" >> "$LOG" 2>&1
	local rc=$?
	echo "  $name rc=$rc $(stat -c %s "$W/$name.png" 2>/dev/null || echo NO) B"
}

# --- terrain ALONE, own normal tiles vs flat tiles vs the +X tilt, top+oblique
for arm in own flat tilt; do
	case "$arm" in
		own)  CACHE="$OWN" ;;
		flat) CACHE="$FLAT" ;;
		tilt) CACHE="$TILT" ;;
	esac
	[ -d "$CACHE" ] || { echo "  (no $arm cache, skipped)"; continue; }
	for v in 1 8; do
		if [ "$v" = 1 ]; then n="t_${arm}_top"; c="$CEN"; else n="t_${arm}_obl"; c="$CENO"; fi
		shot "$n" "$LODL" "$v" "$c" "$ORT" \
			WW_LODL_REGION="-20,24,-17,27,2" \
			WW_LODL_SHEETS="$SHEETS" \
			WW_LODL_SHEET_CACHE="$CACHE" \
			WW_LODGEN_RESOURCES="$RES"
	done
done

# --- NATIVEVIEW1's picture (i): terrain AND objects, top + oblique
for v in 1 8; do
	if [ "$v" = 1 ]; then n=i_native_both_top; c="$CEN"; else n=i_native_both_obl; c="$CENO"; fi
	shot "$n" "$LODL" "$v" "$c" "$ORT" \
		WW_LODL_REGION="-20,24,-17,27,2" \
		WW_LODL_SHEETS="$SHEETS" \
		WW_LODL_SHEET_CACHE="$OWN" \
		WW_LODL_OBJECTS="$LODI" \
		WW_LODGEN_RESOURCES="$RES"
done

# --- the LEGACY control column, same two cameras
for v in 1 8; do
	if [ "$v" = 1 ]; then s=top; lc="$LCEN"; c="$CEN"; else s=obl; lc="$LCENO"; c="$CENO"; fi
	shot "legacy_btr_$s" "$OBJ/Commonwealth.4.-20.24.BTR" "$v" "$lc" "$ORT" \
		WW_LODGEN_RESOURCES="$RES"
	shot "legacy_bto_$s" "$OBJ/Commonwealth.4.-20.24.BTO" "$v" "$c" "$ORT" \
		WW_LODGEN_RESOURCES="$RES"
done

echo "log: $LOG"
