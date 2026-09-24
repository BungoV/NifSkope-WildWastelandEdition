#!/bin/bash
#
# Lane SKEL2 -- the RUNG's pictures, taken before the build.
#
# They are gate S2's reference. bungo's colour ruling ("Just keep the color of
# the bones blue") makes an exact framebuffer match with the old overlay
# impossible by construction, so the way-back display (`Wire`) is proved a
# different way: the SET OF PIXELS THE OVERLAY COVERS must be the same as the
# rung's. That needs the rung's own on/off pair at a pinned camera, and the
# rung is about to be overwritten by the link.
#
# WW_RENDER_SIZE is max(width, a build-dependent floor) by height-59, so the
# size is READ BACK, never assumed (nifskope-ww-render-shot).
set -u
. "$(dirname "$0")/../../tests/spells/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="$ROOT/fixtures/human_male_vanilla.nif"
CLIP="$ROOT/fixtures/Running_To_Slide_And_Back_To_Running.hkx"
OUT="${OUT:-$ROOT/scratchpad/skel2_20260910/rung}"
PORT="${PORT:-42351}"
mkdir -p "$OUT"

export WW_RENDER_SIZE="${WW_RENDER_SIZE:-1000x1000}"
export WW_RENDER_VIEW=5
export WW_RENDER_CLEAN=1
export WW_RENDER_CENTER=0,0,62
export WW_RENDER_ORTHO=80

shot () {   # shot <name> <overlay 0|1> <clip|""> <time>
	local name="$1" ov="$2" clip="$3" t="$4" out
	out="$OUT/$name.png"
	rm -f "$out"
	env WW_SKELETON_OVERLAY="$ov" \
		${clip:+WW_HKXANIM_CLIP="$(winpath "$clip")"} \
		WW_RENDER_TIME="$t" \
		WW_RENDER_SHOT="$(winpath "$out")" \
		"$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1
	if [ -f "$out" ]; then
		python -c "from PIL import Image; im=Image.open(r'$(winpath "$out")'); print('   $name  %dx%d  %d bytes' % (im.size[0], im.size[1], __import__('os').path.getsize(r'$(winpath "$out")')))"
	else
		echo "   $name  NO PNG"
		return 1
	fi
}

T=$(python -c "print(round(46/60,6))")
shot off        0 ""      0.0
shot on         1 ""      0.0
shot off_frame46 0 "$CLIP" "$T"
shot on_frame46  1 "$CLIP" "$T"
md5sum "$OUT"/*.png
echo "RUNG SHOTS DONE"
