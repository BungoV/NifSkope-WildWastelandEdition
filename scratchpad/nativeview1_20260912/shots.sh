#!/usr/bin/env bash
# Lane NATIVEVIEW1 -- the picture set, from NATIVE files only, with the legacy
# .BTR/.BTO of the same camera beside each one as a control.
#
# One window at a time, always --port, always WW_WINDOW_AT.  The game check is
# NOT in here: it is its own command and its answer is read before this runs.
set -u
ROOT="E:/Projects/NifskopeWildWastelandEdition"
R="$ROOT/scratchpad/nativeview1_20260912"
S="$ROOT/scratchpad/showcase1_20260912/out"
W="$R/work/shots"
LOG="$R/work/shots.log"
NS="$ROOT/release/NifSkope.exe"
PORT=42931
mkdir -p "$W"
: > "$LOG"

LODL="$S/lodl/Terrain/Commonwealth.lodl"
LODI="$S/look/native/Commonwealth.lodi"
SHEETS="$S/look/mod/Terrain"
OBJ="$S/look/obj"
RES="$R/resroot;$OBJ"

# the measured chunk (-20,24) dim 4: world centre and half-width
CEN="-73728,106496,0"
LCEN="8192,8192,0"          # the .BTR's own space: its shape sits at 0,0,0
# An OBLIQUE view needs the look-at at the land's own height or the picture is
# mostly empty sky: this chunk's terrain sits around z 8200 (the .BTR's
# vertices read 2052 through a shape scale of 4, and the .lodi's own placements
# read 8027..10032), so both obliques look at z 8500.
CENO="-73728,106496,8500"
LCENO="8192,8192,8500"
ORT=8192
# the whole .lodi extent, for the far picture
FCEN="-57344,114688,0"
FORT=32768

shot() {  # shot <name> <file> <view> <center> <ortho> [extra env...]
	local name="$1" file="$2" view="$3" ctr="$4" ort="$5"; shift 5
	echo "=== $name" >> "$LOG"
	env "$@" \
		WW_WINDOW_AT=1960,40 \
		WW_RENDER_SHOT="$W/$name.png" WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ort" \
		WW_LODL_SHEET_CACHE="$R/work/sheetcache" \
		timeout 300 "$NS" --port "$PORT" "$file" >> "$LOG" 2>&1
	echo "  $name rc=$? $(stat -c %s "$W/$name.png" 2>/dev/null) B"
}

# (i) terrain AND objects, from the .lodl lit by its sheets with the .lodi
#     appended -- top and oblique
for v in 1 8; do
	if [ "$v" = 1 ]; then n=i_native_both_top; c="$CEN"; else n=i_native_both_obl; c="$CENO"; fi
	shot "$n" "$LODL" "$v" "$c" "$ORT" \
		WW_LODL_REGION="-20,24,-17,27,2" \
		WW_LODL_SHEETS="$SHEETS" \
		WW_LODL_OBJECTS="$LODI" \
		WW_LODGEN_RESOURCES="$RES"
done

# (ii) objects alone, from the .lodi -- top and oblique
for v in 1 8; do
	if [ "$v" = 1 ]; then n=ii_native_obj_top; c="$CEN"; else n=ii_native_obj_obl; c="$CENO"; fi
	shot "$n" "$LODI" "$v" "$c" "$ORT" \
		WW_LODI_REGION="-20,24,-17,27" \
		WW_LODGEN_RESOURCES="$RES"
done

# the LEGACY controls of the same two cameras: the bake keeps terrain and
# objects in two files and in two spaces (the .BTR's shape is at 0,0,0 with
# chunk-local vertices, the .BTO's carries the chunk's world origin)
for v in 1 8; do
	if [ "$v" = 1 ]; then s=top; lc="$LCEN"; c="$CEN"; else s=obl; lc="$LCENO"; c="$CENO"; fi
	shot "legacy_btr_$s" "$OBJ/Commonwealth.4.-20.24.BTR" "$v" "$lc" "$ORT" \
		WW_LODGEN_RESOURCES="$RES"
	shot "legacy_bto_$s" "$OBJ/Commonwealth.4.-20.24.BTO" "$v" "$c" "$ORT" \
		WW_LODGEN_RESOURCES="$RES"
done

# (iii) the whole region at the COARSEST cluster level, and the finest beside it
shot "iii_native_far_coarse" "$LODI" 1 "$FCEN" "$FORT" \
	WW_LODI_LEVEL=7 WW_LODGEN_RESOURCES="$RES"
shot "iii_native_far_fine" "$LODI" 1 "$FCEN" "$FORT" \
	WW_LODI_LEVEL=0 WW_LODGEN_RESOURCES="$RES"

echo "log: $LOG"
