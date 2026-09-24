#!/bin/bash
#
# Lane BUILD7 -- THE FRAMES FOR BUNGO.
#
# bungo, 2026-09-10, verbatim: "you can give me a few frames of imported
# animations on the human rig, at different points in animation's time, then
# when I confirm they're good you'll do a test export / import for animations
# to see if they're 1:1 accurate".
#
# Six renders of fixtures/human_male_vanilla.nif from ONE pinned camera:
# the bind pose (no clip), then the clip at 0, 1/4, 1/2, 3/4 and the last
# frame. Same camera, same size, same overlays off, so any difference in the
# picture is the animation's.
#
# This is NOT gate (e) -- that is scratchpad/hkx2_20260910/shots.sh, run
# unchanged. This script adds the two intermediate frames bungo asked for and
# uses the FRONT view (WW_RENDER_VIEW=5, ViewFront) rather than the startup
# camera.
#
#   PREFIX=jog CLIP=... FD=0.0333333 NFRAMES=23 bash frames.sh
#   VIEW=4 SUFFIX=_side ONLY=half ... bash frames.sh   # one side view
#
set -u
. "$(dirname "$0")/../../tests/spells/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
PREFIX="${PREFIX:-hkx}"
SUFFIX="${SUFFIX:-}"
OUT="${OUT:-$ROOT/scratchpad/build7_20260910/images}"
PORT="${PORT:-42297}"
ONLY="${ONLY:-}"

# The NIF is passed as ARGV. A Git-Bash parent does NOT get MSYS2's automatic
# path conversion, so an /e/... argv reaches the app as a path it cannot open
# and the scene loads with ONE node -- measured 2026-09-10, lane BUILD7, on
# tests/spells/hkxanim_play.sh with a relative SRC (0 of 95 bones matched).
SRC="$(winpath "$SRC")"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP"; exit 2; }
mkdir -p "$OUT"

FD="${FD:-0.0333333}"
NFRAMES="${NFRAMES:-23}"

# ONE camera for every tile of both sheets.
export WW_RENDER_SIZE="${WW_RENDER_SIZE:-1000x1000}"
export WW_RENDER_VIEW="${VIEW:-5}"          # 5 = ViewFront (src/glview.h:298)
export WW_RENDER_CLEAN=1
export WW_RENDER_CENTER="${WW_RENDER_CENTER:-0,0,62}"
# ORTHOGRAPHIC, not the 35-degree perspective gate (e) uses. Measured reason:
# the Mixamo clip's COM track travels 487 units in +Y (its root-motion channel
# is all zero), and in a FRONT view +Y is the view axis -- under perspective the
# figure shrinks to nothing by the last frame (gate (e)'s mixamo_frame_half and
# _last came out as 5 KB of empty background). An orthographic camera is
# depth-independent, so the same pin frames every frame of both clips at the
# same scale and the same screen position, and the only thing that changes
# between two tiles is the POSE.
export WW_RENDER_ORTHO="${WW_RENDER_ORTHO:-80}"

shot () {	# shot <tag> <frameindex|-1 for bind>
	local tag="$1" fi="$2" out t
	out="$OUT/${PREFIX}${SUFFIX}_${tag}.png"
	[ -n "$ONLY" ] && [ "$ONLY" != "$tag" ] && return 0
	rm -f "$ROOT/release/ww_camera_pin.log"
	if [ "$fi" = "-1" ]; then
		t=0.0
		echo "== $out  BIND POSE (no clip)"
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_TIME="$t" \
			"$NS" --port "$PORT" "$SRC" >/dev/null 2>&1
	else
		t=$(python -c "print(round($FD*$fi,6))")
		echo "== $out  frame $fi  t=$t"
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_TIME="$t" \
			WW_HKXANIM_CLIP="$(winpath "$CLIP")" \
			"$NS" --port "$PORT" "$SRC" >/dev/null 2>&1
	fi
	if [ -f "$out" ]; then
		ls -l --time-style=+%H:%M:%S "$out" | awk '{print "   "$5" bytes  "$6}'
	else
		echo "   NO PNG"; return 1
	fi
	[ -f "$ROOT/release/ww_camera_pin.log" ] && \
		grep -m1 "grab" "$ROOT/release/ww_camera_pin.log" | sed 's/^/   pin: /'
	# the frame index and its time, for the caption, written beside the picture
	echo "$fi $t" > "${out%.png}.txt"
	return 0
}

Q1=$(python -c "print(int(($NFRAMES-1)*0.25+0.5))")
Q2=$(python -c "print(int(($NFRAMES-1)*0.5+0.5))")
Q3=$(python -c "print(int(($NFRAMES-1)*0.75+0.5))")
LAST=$((NFRAMES-1))

RC=0
shot bind    -1     || RC=1
shot f0      0      || RC=1
shot fq1     "$Q1"  || RC=1
shot fhalf   "$Q2"  || RC=1
shot fq3     "$Q3"  || RC=1
shot flast   "$LAST"|| RC=1

if [ -z "$ONLY" ]; then
	echo "== md5 (six identical hashes = the pose never reached the rig)"
	md5sum "$OUT/${PREFIX}${SUFFIX}"_{bind,f0,fq1,fhalf,fq3,flast}.png 2>/dev/null
	U=$(md5sum "$OUT/${PREFIX}${SUFFIX}"_{bind,f0,fq1,fhalf,fq3,flast}.png 2>/dev/null \
	     | awk '{print $1}' | sort -u | wc -l)
	echo "distinct images: $U of 6"
	[ "$U" -ge 5 ] || { echo "FAIL: the pictures do not differ"; RC=1; }
fi
exit $RC
