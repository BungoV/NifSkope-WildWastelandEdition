#!/bin/bash
#
# Overlays > Show Skeleton draws the SAME skeleton the Skeleton Manager does,
# through the mesh, and follows the animation.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-10, verbatim: "Add to the overlays: View skeleton, shows you
# the bones, basically the same view as in the skeleton manager". The second
# half of that sentence is the whole gate. Something that draws bones is easy;
# something that draws THE SAME bones, in the same three classes, with the same
# counts as the dock he is comparing against, is the thing he asked for.
#
# WHAT IS MEASURED (the in-app half, WW_SKELOVERLAY_TEST, one run)
#
#   (a)  the overlay's joint count is the SKELETON MANAGER DOCK'S own row count
#        under its All filter -- read off the dock's tree, not off the analysis
#        the overlay is itself built from, which would be circular
#   (a') FLOOR: with the overlay off, its census is all zeros
#   (b)  the three colour counts are the dock's Bones / Deforming / Unused
#        filter counts, and the muted ones are All minus Bones
#   (c)  the render with the overlay ON differs from the render with it OFF
#        only inside a mask rasterised from the segments the overlay REPORTS
#        having drawn
#   (c') FLOOR: the mask is not the whole frame, and the tolerance still
#        refuses the overlay's own difference
#   (d)  scrubbed to frame N of a loaded clip, every drawn joint is within
#        1e-3 units of the ANIMATED NiNode's world position
#   (d') FLOOR: those same positions against the bind pose must disagree
#   (e)  toggling the overlay off restores the off-render
#
# ADDED BY LANE SKELFIX, 2026-09-10, after the director saw long segments
# fanning out of the character in on_frame46.png -- 300.5 units to the world
# origin, because the clip carries the travel on COM while Root, Camera,
# CamTarget and the AnimObject nodes stay where the file put them. A bone body
# now joins two ARMATURE nodes only; every other node still gets its joint
# marker, so (a) and (b) are untouched.
#
#   (f)  at frame N no drawn segment is longer than 1.5x the longest BIND-pose
#        segment between two nodes the dock calls bones
#   (f') FLOOR: the OLD rule (every parent -> child pair, same readback) fails it
#   (g)  every drawn segment endpoint is inside the bones' own bounding box
#   (g') FLOOR: the old rule puts endpoints outside it
#   (h)  the census field `skipped` is written and moves
#   (i)  the overlay states its rule in words, and the armature has members
#        and non-members
#
# ADDED BY LANE SKEL2, 2026-09-11, after bungo's three rulings: the bone view
# is to MIRROR the Skeleton Manager, both views are to share one renderer and
# both are to be improved, and the bone is to be Blender's octahedron in blue.
#
#   (c)/(e) carry a MEASURED tolerance of 64 px instead of demanding exact
#        framebuffer equality. Five identical runs on the rung measured (c) at
#        16 / 13 / 14 / 9 / 1 and (e) at 12 / 22 / 19 / 15 / 21; BUILD11's four
#        runs of the previous exe gave 10 / 0 / 4 / 0 and 17 / 0 / 37 / 2. The
#        worst value on record is 37 out of ~1.2 million pixels.
#   (k)  under each chip and under a search, the overlay lists exactly the
#        blocks the dock lists, BY NAME
#   (k') FLOOR: a chip the dock is not showing makes the two disagree
#   (l)  selection is two-way, and a double-click on a row frames the bone
#   (l') FLOOR: a click away from every bone selects none
#   (m)  every row at depth 6+ shows its whole name
#   (m') FLOOR: the shipped column law left that row no room
#   (n)  the census field `filtered` is written and moves with the chip
#   (o)  Octahedral, Stick and Wire are three different pictures
#   (p)  X-ray off changes the picture
#
#   S1   ONE RENDERER: neither drawPoseSkeleton() nor drawSkeletonOverlay()
#        draws a bone itself; both hand a list to GLView::drawArmature(), and
#        one palette function answers for the dock and the viewport
#   S2   THE WAY BACK: `Wire` covers the same pixels the shipped overlay
#        covered (skeleton_overlay_coverage.py against the kept rung renders).
#        The brief asked for byte-for-byte and that is impossible here, because
#        his own colour ruling changes every overlay pixel's colour; what the
#        colour ruling did not license to move is WHICH pixels are covered.
#   (j)  RE-REGISTERED under the BONES chip: every pixel the overlay changed is
#        on the character. Under All it cannot hold and is not asked to -- the
#        marker-only camera and anim-object nodes are DELIBERATELY drawn, and
#        two of them project above the back at frame 46. Those two are NAMED
#        instead, by skeleton_overlay_dots.py, from the overlay's own dump.
#   (j') FLOOR: the BUILD9 picture the defect was seen in must FAIL (j)
#
# THE PICTURES (the render-hook half)
#
#   off / on                    one pinned orthographic camera, the model
#                               alone, differing only by the overlay
#   bones_octa/stick/wire       the same pose in each display mode
#   bones_bones_chip            the Bones chip: no grey dots
#   bones_all_chip              All: the dots, named by the dots script
#   bones_xray                  X-ray off
#   on_frame46 / off_frame46    the Mixamo clip at frame 46
#
# NOTE ON PATHS: a Git-Bash parent gets NO MSYS2 argv/environment conversion,
# so every path handed to the exe goes through winpath() first. A repo-relative
# NIF in argv opens a scene with ONE unnamed node and says nothing (lane
# BUILD7, 2026-09-10).
#
# NOTE ON PORTS: the GUI needs a free IPC port and NifSkope EXITS SILENTLY if
# it cannot bind one. Keep the number below ~49152.
#
# USAGE
#   bash tests/spells/skeleton_overlay.sh

set -u

# Real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# ---- settings scope (lane FIX1 fix 4, 2026-09-26) ----------------------------
# Every NifSkope WINDOW this spell opens runs in its OWN QSettings scope -- never
# bungo's profile, and never a scope the caller's environment names. A gate that
# inherits the user's settings measures the profile, not the code (lane GATEFIX2,
# native_lighting.sh: his "Vertex Color" unticked in the Lighting shading mode
# turned the .BTR water white). WW_SETTINGS_SCOPE=<scope> moves the whole tree to
# HKCU\Software\NifTools\NifSkope 2.0 <scope> (src/harnesswindow.cpp).
# fresh_scope wipes it before EACH window (a window saves its layout on close, so
# the next would open at another size) and seeds Settings/Version=1: an EMPTY
# scope is a first install, whose settings dialog saves every pane's widget value
# (Background 46,46,46, src/ui/settingspane.cpp). SEED_REG=<file.reg> (keys
# already under the scope) is imported after the seed -- a red control's way to
# render under a chosen profile. The scope is wiped at exit. The -no-gui CLI
# calls are not windows and are left as they were.
SCOPE="${SCOPE:-skeleton_overlay}"
case "$SCOPE" in
	''|*[!A-Za-z0-9_-]*) echo "REFUSED: SCOPE='$SCOPE' is not a usable settings scope name"; exit 2 ;;
esac
[ ${#SCOPE} -le 40 ] || { echo "REFUSED: SCOPE='$SCOPE' is longer than 40"; exit 2; }
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {  # wipe + seed, then print the name: WW_SETTINGS_SCOPE="$(fresh_scope)"
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	# not a Game Manager first install either: version 0 shows an opaque progress dialog on the
	# PRIMARY monitor (src/gamemanager.cpp prog_dialog) before any WW window placement exists
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	# ...and the game manager state an empty scope never gets (its Game Folders come out empty):
	# Fallout 4's path and folders read from THIS MACHINE, never from the user's profile
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	if [ -n "${SEED_REG:-}" ]; then reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1 || true; fi
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/fixtures/Running_To_Slide_And_Back_To_Running.hkx}"
OUT="${OUT:-$ROOT/scratchpad/skeloverlay_20260910}"
RUNG="${RUNG:-$ROOT/scratchpad/skel2_20260910/rung}"
FRAME="${FRAME:-46}"
FPS="${FPS:-60}"
LOG="$ROOT/release/ww_skeloverlay_test.log"
PORT="${PORT:-42303}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP"; exit 2; }
mkdir -p "$OUT/gates"

RC=0

# ------------------------------------------------------- S1 the ONE renderer
# A source gate, and it is the one thing here a picture cannot check: two
# renderers that happen to agree today are still two renderers.
echo "== S1 one renderer"
S1=0
for fn in drawPoseSkeleton drawSkeletonOverlay; do
	n=$(awk -v f="void GLView::$fn" '
		index($0, f) == 1 {on=1}
		on && /^\}/ {on=0}
		on && (/scene->drawLine\(/ || /drawOctahedralBone\(/ || /scene->drawPoints\(/) {c++}
		END {print c+0}' "$ROOT/src/glview.cpp")
	if [ "$n" -eq 0 ]; then
		echo "  ok   $fn draws no bone itself"
	else
		echo "  FAIL $fn still makes $n bone-drawing call(s) of its own"
		S1=1
	fi
	m=$(awk -v f="void GLView::$fn" '
		index($0, f) == 1 {on=1}
		on && /^\}/ {on=0}
		on && /drawArmature\(/ {c++}
		END {print c+0}' "$ROOT/src/glview.cpp")
	if [ "$m" -ge 1 ]; then
		echo "  ok   $fn hands its list to drawArmature()"
	else
		echo "  FAIL $fn never calls drawArmature()"
		S1=1
	fi
done
K=$(grep -c "skeletonKindColor(" "$ROOT/src/glview.cpp")
D=$(grep -c "skeletonKindColor(" "$ROOT/src/skeletontools.cpp")
echo "  palette function: glview.cpp x$K, skeletontools.cpp x$D"
{ [ "$K" -ge 1 ] && [ "$D" -ge 2 ]; } || { echo "  FAIL one palette function is not called by both"; S1=1; }
L=$(grep -rl "skeletonKindColor(" "$ROOT/src" | wc -l)
echo "  files that mention it: $L (the viewport, the dock, its header)"
[ "$L" -le 3 ] || { echo "  FAIL a fourth caller appeared; the colour law has leaked"; S1=1; }
[ "$S1" -eq 0 ] || RC=1

# ---------------------------------------------------------------- the gates
echo "== gates (WW_SKELOVERLAY_TEST) on $(basename "$SRC")"
rm -f "$LOG"
WW_SKELOVERLAY_TEST=1 \
WW_SKELOVERLAY_CLIP="$(winpath "$CLIP")" \
WW_SKELOVERLAY_FRAME="$FRAME" \
WW_SKELOVERLAY_FPS="$FPS" \
WW_SKELOVERLAY_SHOTDIR="$(winpath "$OUT/gates")" \
WW_SKELOVERLAY_DOCKSHOT="$(winpath "$OUT/manager_after.png")" \
	WW_SETTINGS_SCOPE="$(fresh_scope)" "$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1

if [ -f "$LOG" ]; then
	cat "$LOG"
	grep -q "^PASS" "$LOG" || RC=1
else
	echo "FAIL: no log -- did the app exit before the harness ran?"
	RC=1
fi

# The SAME grab with the shipped column law: gate (m)'s floor as a picture
# rather than as arithmetic. One variable, one build.
echo "== the dock under the shipped column law (gate (m)'s floor)"
rm -f "$LOG"
WW_SKELOVERLAY_TEST=1 \
WW_SKELETON_LEGACY_COLUMNS=1 \
WW_SKELOVERLAY_CLIP="$(winpath "$CLIP")" \
WW_SKELOVERLAY_FRAME="$FRAME" \
WW_SKELOVERLAY_FPS="$FPS" \
WW_SKELOVERLAY_SHOTDIR="$(winpath "$OUT/gates_legacy")" \
WW_SKELOVERLAY_DOCKSHOT="$(winpath "$OUT/manager_before.png")" \
	WW_SETTINGS_SCOPE="$(fresh_scope)" "$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1
if [ -f "$LOG" ]; then
	grep -E "^\(m\)|^\(m'\)|rows at depth|name column" "$LOG" | sed 's/^/   legacy: /'
	cp "$LOG" "$OUT/gates_legacy_log.txt" 2>/dev/null
fi
[ -f "$OUT/manager_before.png" ] || { echo "   NO before grab"; RC=1; }

# --------------------------------------------------------------- the pictures
# ONE camera for all of them, and the same pin lane BUILD7 used for the frame
# sheets bungo has already seen, so the pictures are comparable to those.
# ORTHOGRAPHIC on purpose: the Mixamo clip's COM travels 487 units along the
# view axis of a front view, and under perspective the figure shrinks away.
export WW_RENDER_SIZE="${WW_RENDER_SIZE:-1000x1000}"
export WW_RENDER_VIEW="${VIEW:-5}"            # 5 = ViewFront (src/glview.h)
export WW_RENDER_CLEAN=1                      # model only: no grid, axes, nodes
export WW_RENDER_CENTER="${WW_RENDER_CENTER:-0,0,62}"
export WW_RENDER_ORTHO="${WW_RENDER_ORTHO:-80}"

shot () {	# shot <name> <overlay 0|1> <clip|""> <time> [extra env...]
	local name="$1" ov="$2" clip="$3" t="$4" out
	shift 4
	out="$OUT/$name.png"
	rm -f "$out" "$ROOT/release/ww_camera_pin.log"
	# A conditional environment variable goes through `env`, never a bare
	# ${x:+NAME=1} prefix: bash decides which words are assignments BEFORE
	# expansion, so the expanded word lands in command position and the run
	# dies rc 127, silently (lane LODTOPEN3).
	env WW_SKELETON_OVERLAY="$ov" \
		${clip:+WW_HKXANIM_CLIP="$(winpath "$clip")"} \
		WW_RENDER_TIME="$t" \
		WW_RENDER_SHOT="$(winpath "$out")" \
		"$@" \
		WW_SETTINGS_SCOPE="$(fresh_scope)" "$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1
	if [ -f "$out" ]; then
		WWNAME="$name" WWPNG="$(winpath "$out")" python -c "
import os
from PIL import Image
p = os.environ['WWPNG']
im = Image.open(p)
print('   %-24s %dx%d  %d bytes' % (os.environ['WWNAME'], im.size[0], im.size[1], os.path.getsize(p)))
"
	else
		echo "   $name  NO PNG"
		return 1
	fi
	return 0
}

echo "== pictures"
T=$(python -c "print(round($FRAME/$FPS,6))")
shot off              0 ""      0.0                                  || RC=1
shot on               1 ""      0.0 WW_SKELETON_DISPLAY=0            || RC=1
shot bones_octa       1 ""      0.0 WW_SKELETON_DISPLAY=0            || RC=1
shot bones_stick      1 ""      0.0 WW_SKELETON_DISPLAY=1            || RC=1
shot bones_wire       1 ""      0.0 WW_SKELETON_DISPLAY=2            || RC=1
shot bones_xray       1 ""      0.0 WW_SKELETON_DISPLAY=0 WW_SKELETON_XRAY=0 || RC=1
shot bones_bones_chip 1 ""      0.0 WW_SKELETON_DISPLAY=0 WW_SKELETON_CHIP=1 || RC=1
shot bones_all_chip   1 ""      0.0 WW_SKELETON_DISPLAY=0 WW_SKELETON_CHIP=0 || RC=1
shot "on_frame$FRAME"  1 "$CLIP" "$T" WW_SKELETON_DISPLAY=0 \
	WW_SKELOVERLAY_DUMP="$(winpath "$OUT/dump_frame$FRAME.tsv")"       || RC=1
shot "off_frame$FRAME" 0 "$CLIP" "$T"                                 || RC=1
shot "on_frame${FRAME}_bones" 1 "$CLIP" "$T" WW_SKELETON_DISPLAY=0 WW_SKELETON_CHIP=1 \
	WW_SKELOVERLAY_DUMP="$(winpath "$OUT/dump_frame${FRAME}_bones.tsv")" || RC=1

# Two identical hashes here would mean a switch never reached the picture.
echo "== md5"
md5sum "$OUT/off.png" "$OUT/bones_octa.png" "$OUT/bones_stick.png" \
	"$OUT/bones_wire.png" "$OUT/bones_xray.png" "$OUT/bones_bones_chip.png" 2>/dev/null
U=$(md5sum "$OUT/off.png" "$OUT/bones_octa.png" "$OUT/bones_stick.png" \
	"$OUT/bones_wire.png" "$OUT/bones_xray.png" "$OUT/bones_bones_chip.png" 2>/dev/null \
     | awk '{print $1}' | sort -u | wc -l)
echo "distinct images: $U of 6"
[ "$U" -eq 6 ] || { echo "FAIL: six switches did not make six pictures"; RC=1; }

# ------------------------------------------- (j) re-registered: the BONES chip
echo "== (j) under the Bones chip: every overlay pixel is on the character"
python "$ROOT/tests/spells/skeleton_overlay_mask.py" \
	"$OUT/off_frame$FRAME.png" "$OUT/on_frame${FRAME}_bones.png" \
	--out "$OUT/gates/gate_frame${FRAME}_bones_mask.png" || RC=1

echo "== (j, All chip) NOT A GATE any more -- the marker-only nodes are drawn on purpose"
python "$ROOT/tests/spells/skeleton_overlay_mask.py" \
	"$OUT/off_frame$FRAME.png" "$OUT/on_frame$FRAME.png" \
	--out "$OUT/gates/gate_frame${FRAME}_mask.png" || true

# (j') FLOOR. The picture the defect was SEEN in must fail the same test on the
# same reference, or (j) is only measuring that a picture exists.
BEFORE="$ROOT/scratchpad/skelfix_20260910/before_on_frame46.png"
if [ -f "$BEFORE" ] && [ "$FRAME" = "46" ]; then
	echo "== (j') FLOOR: the BUILD9 picture must FAIL (j)"
	if python "$ROOT/tests/spells/skeleton_overlay_mask.py" \
			"$OUT/off_frame$FRAME.png" "$BEFORE" >/dev/null 2>&1; then
		echo "FAIL: the old picture PASSED gate (j) -- the gate cannot fail"
		RC=1
	else
		echo "  ok   the old picture fails gate (j), so the gate can fail"
	fi
else
	echo "== (j') FLOOR NOT RUN: no $BEFORE"
	RC=1
fi

# ------------------------------------------------ S6 name the marker-only dots
echo "== S6 the marker-only nodes that project off the character, NAMED"
if [ -f "$OUT/dump_frame$FRAME.tsv" ]; then
	python "$ROOT/tests/spells/skeleton_overlay_dots.py" \
		"$OUT/off_frame$FRAME.png" "$OUT/on_frame$FRAME.png" \
		"$OUT/dump_frame$FRAME.tsv" --out "$OUT/gates/dots_frame$FRAME.png" || RC=1
else
	echo "FAIL: no dump at $OUT/dump_frame$FRAME.tsv"
	RC=1
fi

# ------------------------------------------------------- S2 the exact way back
echo "== S2 Wire covers the same pixels the shipped overlay covered"
if [ -f "$RUNG/off.png" ] && [ -f "$RUNG/on.png" ]; then
	python "$ROOT/tests/spells/skeleton_overlay_coverage.py" \
		"$RUNG/off.png" "$RUNG/on.png" "$OUT/off.png" "$OUT/bones_wire.png" \
		--out "$OUT/gates/coverage_wire.png" || RC=1
	echo "-- and the same comparison for Octahedral, which MUST differ"
	if python "$ROOT/tests/spells/skeleton_overlay_coverage.py" \
			"$RUNG/off.png" "$RUNG/on.png" "$OUT/off.png" "$OUT/bones_octa.png" \
			--out "$OUT/gates/coverage_octa.png" >/dev/null 2>&1; then
		echo "  FAIL Octahedral covers the same pixels as the old wire overlay"
		RC=1
	else
		echo "  ok   Octahedral is a different shape, as it must be"
	fi
else
	echo "SKIP S2: no rung renders under $RUNG (they are taken before the build)"
	RC=1
fi

exit $RC
