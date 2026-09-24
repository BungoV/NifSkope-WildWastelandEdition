#!/bin/bash
# Lane HORIZON2 step 5 -- the AFTER pictures, from the SHIPPED bytes of this
# lane's bake, in HORIZON1's framings so the two sets lie side by side.
#
# This is HORIZON1's shots.sh with three lines changed (LANE, PORT, and the
# object/sheet sources), because a before-and-after made from two different
# cameras is not a before-and-after.
#
# usage: bash shots.sh [main|calib|bins|all]
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
LANE=$ROOT/scratchpad/horizon2_20260918
V=$ROOT/scratchpad/viewfix_20260917
EXE=$ROOT/release/NifSkope.exe
LODL=$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl
V8I=$LANE/v8/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi
SHEETS=$LANE/v8/vt/FO4CSLOD/Commonwealth
IMG=$LANE/images
PORT=${PORT:-12091}
REGION=4,-12,7,-9,0
SIZE=1400x1091
what=${1:-all}
mkdir -p "$IMG"

# THE GAME RULE, as its own test before every single run and not once at the top.
guard () {
	if tasklist 2>/dev/null | grep -qi -E "Fallout4|NifSkope"; then
		echo "GAME OR NIFSKOPE UP -- stopping before $1"; exit 90
	fi
}

# shot <name> <sun-az,el> <cx> <cy> <cz> <ortho> <channel> [bin-rot]
shot () {
	guard "$1"
	WW_LODL_OBJECTS="$V8I" WW_LODL_SHEETS="$SHEETS" WW_LODL_REGION="$REGION" \
	WW_LODI_LEVEL=0 WW_LODI_SLOT=0 WW_LODL_CHANNEL="$7" \
	WW_SUN="$2" WW_HORIZON_BIN_ROT="${8:-0}" \
	WW_RENDER_SHOT="$IMG/$1.png" WW_RENDER_SIZE="$SIZE" \
	WW_RENDER_CENTER="$3,$4,$5" WW_RENDER_ORTHO="$6" WW_RENDER_VIEW=8 \
	WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 WW_WINDOW_AT=1960,40 \
		timeout 600 "$EXE" --port "$PORT" "$LODL" > "$IMG/$1.log" 2>&1
	local rc=$?
	printf '%-34s exit=%s %8s B  %s\n' "$1" "$rc" \
		"$(stat -c %s "$IMG/$1.png" 2>/dev/null || echo 0)" \
		"$(grep -ao 'horizon 0\.00\.\.[0-9.]* deg mean [0-9.]*, [0-9.]*%* lit' "$IMG/$1.log" | head -1)"
	if tasklist 2>/dev/null | grep -qi "NifSkope.exe"; then
		echo "  STOP: a NifSkope is still up after '$1' (this script's port $PORT)"; exit 9
	fi
}

if [ "$what" = main ] || [ "$what" = all ]; then
	for a in 120 240; do
		for e in 05 15 30; do
			shot "chunk_horizon_close_e${e}_a${a}" "$a,${e#0}" 24900 -41300 450 2600 horizon
			shot "chunk_horizon_full_e${e}_a${a}"  "$a,${e#0}" 24576 -40960 0 8192 horizon
		done
	done
	shot "chunk_horizon_close_e15_a120_CONTROL" 120,15 24900 -41300 450 2600 horizon 4
fi

if [ "$what" = calib ] || [ "$what" = all ]; then
	shot calib_base 120,15 24900 -41300 450 2600 horizon
	shot calib_dx   120,15 25400 -41300 450 2600 horizon
	shot calib_dy   120,15 24900 -40800 450 2600 horizon
	shot calib_dz   120,15 24900 -41300 950 2600 horizon
	shot calib_chk  120,15 25250 -40950 800 2600 horizon
	shot calibf_base 120,15 24576 -40960 0 8192 horizon
	shot calibf_dx   120,15 26576 -40960 0 8192 horizon
	shot calibf_dy   120,15 24576 -38960 0 8192 horizon
	shot calibf_dz   120,15 24576 -40960 2000 8192 horizon
	shot calibf_chk  120,15 25976 -39560 1400 8192 horizon
fi

if [ "$what" = bins ] || [ "$what" = all ]; then
	for b in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
		shot "bin_$(printf '%02d' "$b")" 120,15 24900 -41300 450 2600 "horizonbin=$b"
	done
fi
echo "done: $(ls -1 "$IMG"/*.png 2>/dev/null | wc -l) png in $IMG"
