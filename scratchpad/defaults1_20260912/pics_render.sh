#!/bin/bash
# DEFAULTS1 -- the pictures.  One NifSkope instance at a time, each on its own
# unused --port, always the second monitor (WW_WINDOW_AT), absolute E:/ paths.
#
#   bash scratchpad/defaults1_20260912/pics_render.sh
#
# All panels of one comparison share ONE pinned camera, so they are comparable
# pixel for pixel.  A chunk of DIM cells at (CX,CY) is world
# x = CX*4096 .. (CX+DIM)*4096, so the look-at is ((2*CX+DIM)/2)*4096 and the
# ortho half-width DIM*2048 makes the chunk fill the frame.
#   (-20,24) dim 4 -> -73728, 106496 ; (-20,20) dim 4 -> -73728, 90112
set -u
R=/e/Projects/NifskopeWildWastelandEdition
G=$R/scratchpad/defaults1_20260912/gate
P=$R/scratchpad/defaults1_20260912/pics
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
NS=$R/release/NifSkope.exe

if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

mkdir -p "$P"

# THE VANILLA DATA IS DELIBERATELY NOT IN THE RESOURCE LIST. With it
# appended the game's own Commonwealth.4.<x>.<y>.DDS wins the lookup and
# every panel renders the SHIPPED sheet -- measured: the paint-1 and
# paint-0 frames came back pixel-identical, difference bounding box None,
# and so did a frame rendered against the wrong root. With the shim alone
# the bake's own sheet is what is drawn. Water and sky textures are then
# missing, which is why the water reads flat.
# a shim resource root shaped like the archive: <root>/Textures/Terrain/<WS>/
shim() {	# $1 = bake dir, $2 = root name -> prints the root
	local root="$P/root_$2"
	rm -rf "$root"
	mkdir -p "$root/Textures/Terrain/Commonwealth"
	cp -f "$1"/tex/*.DDS "$root/Textures/Terrain/Commonwealth/" 2>/dev/null
	if [ -d "$1/tex/Objects" ]; then
		mkdir -p "$root/Textures/Terrain/Commonwealth/Objects"
		cp -f "$1"/tex/Objects/* "$root/Textures/Terrain/Commonwealth/Objects/" 2>/dev/null
	fi
	echo "$root"
}

# shot <file> <root> <out.png> <port> <center> <orthoHalfWidth>
shot() {
	local f="$1" root="$2" out="$3" port="$4" center="$5" ortho="$6"
	rm -f "$out"
	WW_RENDER_SHOT="$(cygpath -w "$out")" WW_RENDER_SIZE=1400x1400 WW_RENDER_VIEW=1 WW_RENDER_TIME=4 WW_RENDER_CLEAN=1 WW_RENDER_CENTER="$center" WW_RENDER_ORTHO="$ortho" WW_LODGEN_RESOURCES="$(cygpath -m "$root")" WW_LODGEN_DATA_ROOT="$(cygpath -m "$root")" WW_WINDOW_AT=1960,40 timeout 180 "$NS" --port "$port" "$(cygpath -m "$f")" > "$P/$(basename "$out").log" 2>&1
	local rc=$?
	printf '  %-28s rc=%-3s %s bytes\n' "$(basename "$out")" "$rc" "$( [ -f "$out" ] && stat -c%s "$out" || echo MISSING )"
}

PORT=42500
step() { PORT=$(( PORT + 1 )); }

# A .BTR's Land shape lives in the file's OWN space -- the chunk node scales a
# 4096-unit box by 4 -- so the land pair is framed at 8192,8192 with an
# 8192-unit half-width, which is the whole chunk edge to edge. A .BTO is in
# WORLD units, so its pair is framed on the chunk's world centre. The two
# cameras are different because the two spaces are; within a pair the camera is
# the same number, which is what makes the pair comparable.
CLAND="8192,8192,0"; OLAND=8192
COBJ="-73728,106496,0"; OOBJ=8192

echo "== (i) the chunk (-20,24), rung default vs new default =="
RR="$(shim "$G/b_rung" rung)"
RN="$(shim "$G/a_new" new)"
step; shot "$G/b_rung/Commonwealth.4.-20.24.BTR" "$RR" "$P/i_rung_btr.png" $PORT "$CLAND" $OLAND
step; shot "$G/a_new/Commonwealth.4.-20.24.BTR"  "$RN" "$P/i_new_btr.png"  $PORT "$CLAND" $OLAND
step; shot "$G/b_rung/Commonwealth.4.-20.24.BTO" "$RR" "$P/i_rung_bto.png" $PORT "$COBJ" $OOBJ
step; shot "$G/a_new/Commonwealth.4.-20.24.BTO"  "$RN" "$P/i_new_bto.png"  $PORT "$COBJ" $OOBJ

echo "== (ii) the kerb (-20,20): road ground paint 1 vs 0 =="
# a2_old = the rung with every new default spelled EXCEPT the verge paint, so
# it is paint 1; a2_new is the new default, paint 0. Identity is off on both,
# so nothing but the verge can move a texel.
RO="$(shim "$G/a2_old" paint1)"
RP="$(shim "$G/a2_new" paint0)"
step; shot "$G/a2_old/Commonwealth.4.-20.20.BTR" "$RO" "$P/ii_paint1_full.png" $PORT "$CLAND" $OLAND
step; shot "$G/a2_new/Commonwealth.4.-20.20.BTR" "$RP" "$P/ii_paint0_full.png" $PORT "$CLAND" $OLAND

# The CROP camera is not chosen by eye: crop_pick.py reads the two full frames,
# finds where they differ most and writes the centre and half-width here.
if [ -f "$P/crop.env" ]; then
	. "$P/crop.env"
	step; shot "$G/a2_old/Commonwealth.4.-20.20.BTR" "$RO" "$P/ii_paint1_crop.png" $PORT "$CROP_CENTER" $CROP_ORTHO
	step; shot "$G/a2_new/Commonwealth.4.-20.20.BTR" "$RP" "$P/ii_paint0_crop.png" $PORT "$CROP_CENTER" $CROP_ORTHO
fi

echo "renders done $(date +%H:%M:%S)"
