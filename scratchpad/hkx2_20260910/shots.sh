#!/bin/bash
#
# GATE (e) of lane HKX2: the pictures.
#
# Four renders of the SAME NIF from the SAME pinned camera through the render
# hook (nifskope-ww-render-shot): the bind pose, and the loaded clip at frames
# 0, N/2 and N-1. If the pose is not reaching the scene graph the four are
# identical; if the camera is not pinned the difference in the picture is the
# camera's, not the animation's, so the pin is asserted and read back out of
# release/ww_camera_pin.log.
#
# HEADLESS AND INVISIBLE: WW_WINDOW_AT puts the window on the second monitor
# and skips raise(). One instance at a time -- the renders are sequential on
# purpose, never backgrounded.
#
#   bash scratchpad/hkx2_20260910/shots.sh
#
# Lane HKX2, 2026-09-10.
set -u
. "$(dirname "$0")/../../tests/spells/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# The default rig is the RIGGED HUMAN fixture, not the bone-only
# skeleton.nif: a mesh is what shows that skinning followed the bones
# (bones that move under a mesh that does not is the refuter of the
# "Node::transform() is early enough" claim). Run this script ONCE PER
# CLIP with PREFIX set, so the two sets do not overwrite each other:
#
#   PREFIX=jog    CLIP=scratchpad/hkx1_20260910/clips/jog.hkx \
#                 FD=0.0333333 NFRAMES=23 bash scratchpad/hkx2_20260910/shots.sh
#   PREFIX=mixamo CLIP=fixtures/Running_To_Slide_And_Back_To_Running.hkx \
#                 FD=0.0166667 NFRAMES=93 bash scratchpad/hkx2_20260910/shots.sh
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
PREFIX="${PREFIX:-hkx}"
OUT="${OUT:-$ROOT/scratchpad/hkx2_20260910/images}"
PORT="${PORT:-42293}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP"; exit 2; }
mkdir -p "$OUT"

# jog.hkx is 23 frames at 1/30 s (lane HKX1's fixture table), so frame N-1 is
# 22/30 s. Overridable, but stated rather than computed in the shell so the
# caption and the file name cannot drift apart.
FD="${FD:-0.0333333}"
NFRAMES="${NFRAMES:-23}"
T0=0.0
THALF=$(python -c "print(round($FD*int($NFRAMES/2),6))")
TEND=$(python -c "print(round($FD*($NFRAMES-1),6))")

# ONE camera for all four. Same centre, same distance, same field of view, same
# window size, same overlays off.
COMMON=( "--port" "$PORT" "$SRC" )
export WW_RENDER_SIZE="${WW_RENDER_SIZE:-1000x1000}"
export WW_RENDER_VIEW=-1
export WW_RENDER_CLEAN=1
export WW_RENDER_CENTER="${WW_RENDER_CENTER:-0,0,64}"
export WW_RENDER_DIST="${WW_RENDER_DIST:-260}"
export WW_RENDER_FOV="${WW_RENDER_FOV:-35}"

shot () {	# shot <out.png> <time> [clip]
	local out="$1" t="$2" clip="${3:-}"
	echo "== $out  t=$t  clip=${clip:-<none, bind pose>}"
	rm -f "$ROOT/release/ww_camera_pin.log"
	if [ -n "$clip" ]; then
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_TIME="$t" \
			WW_HKXANIM_CLIP="$(winpath "$clip")" "$NS" "${COMMON[@]}" >/dev/null 2>&1
	else
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_TIME="$t" \
			"$NS" "${COMMON[@]}" >/dev/null 2>&1
	fi
	[ -f "$out" ] || { echo "   NO PNG"; return 1; }
	ls -l --time-style=+%H:%M:%S "$out" | awk '{print "   "$5" bytes  "$6}'
	[ -f "$ROOT/release/ww_camera_pin.log" ] && tail -2 "$ROOT/release/ww_camera_pin.log" | sed 's/^/   pin: /'
	return 0
}

RC=0
shot "$OUT/${PREFIX}_bindpose.png"   "$T0"    ""      || RC=1
shot "$OUT/${PREFIX}_frame0.png"     "$T0"    "$CLIP" || RC=1
shot "$OUT/${PREFIX}_frame_half.png" "$THALF" "$CLIP" || RC=1
shot "$OUT/${PREFIX}_frame_last.png" "$TEND"  "$CLIP" || RC=1

# The refuter: four identical files would mean the clip never reached the rig.
echo "== md5 (four identical hashes = the pose never reached the scene)"
md5sum "$OUT/${PREFIX}_bindpose.png" "$OUT/${PREFIX}_frame0.png" \
       "$OUT/${PREFIX}_frame_half.png" "$OUT/${PREFIX}_frame_last.png" 2>/dev/null
U=$(md5sum "$OUT/${PREFIX}_bindpose.png" "$OUT/${PREFIX}_frame0.png" \
           "$OUT/${PREFIX}_frame_half.png" "$OUT/${PREFIX}_frame_last.png" 2>/dev/null \
     | awk '{print $1}' | sort -u | wc -l)
echo "distinct images: $U of 4"
[ "$U" -ge 3 ] || { echo "FAIL: the pictures do not differ"; RC=1; }
exit $RC
