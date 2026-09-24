#!/bin/bash
#
# A loaded Havok animation (.hkx) is a ROW of the ANIMATION DOCK's list, and
# (lane UI6, 2026-09-11) that dock is the Animation workspace: the Animation
# Manager this spell was written against is retired. Same questions, same
# loader, same playback -- the widget names moved with the surface.
# the timeline drives it.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-10: "Also, add support of these to the timeline workspace" /
# "or 'animation' workspace", on top of the earlier ruling that a .hkx loads
# "either win exporer pick or drag and drop". Lane HKX2 proved the PLAYBACK:
# the decoded pose reaches the rig and unloading puts it back. Nothing there
# touches a widget, so nothing there can say whether the clip is visible in the
# dock at all, whether the frame ruler ticks at the clip's own rate, or whether
# a dropped file goes anywhere. That is what this asks, from the UI end.
#
# THE FIXTURE IS THE POINT.
#
#   Running_To_Slide_And_Back_To_Running.hkx is a Mixamo clip at 60 fps with 93
#   frames -- NOT 30, which every vanilla Fallout 4 clip is. A dock that ticks
#   at a fixed 30 would call frame 46 "frame 23" and nothing else in the picture
#   would look wrong. The readout check is therefore the rate check.
#
#   skeleton.hkx reads perfectly and carries no animation. It is the refusal
#   that is not an error: its bones ARE kept for clips that need names, and the
#   row still has to appear, marked refused, saying why -- a file that vanishes
#   without a trace is the worst answer a drop can give.
#
# WHAT IS MEASURED
#
#   (a)  loading puts ONE row in the dock's list saying 93 frames and 60 fps
#   (a') FLOOR: a name that was never loaded has no row
#   (b)  at frame 46 every bound node's transform equals the DECODER's output
#        (translation <= 1e-4, rotation <= 0.01 deg, the 4*asin metric)
#   (b') FLOOR: held against frame 0 the same test goes red
#   (c)  the readout says "frame 46 / 92" (true only at 60 fps), the Speed row
#        writes the playback speed, the Loop row flips the transport's action
#   (c') FLOORS: at t=0 the readout says frame 0; the speed reads 1 first
#   (d)  unloading restores every Transform byte for byte
#   (d') FLOOR: while posed, more than zero nodes differ
#   (e)  DROPPING the file on the window yields the same row, through the real
#        application event filter
#   (e') FLOOR: dropping a file that is not an animation adds no row
#   (f)  skeleton.hkx lands as a row MARKED REFUSED with the reason in words
#   (f') FLOOR: the clip that plays is not marked refused
#   (g)  the panel-style counts, each with a floor
#
# Plus one picture: the dock with the clip loaded and the timeline at frame 46.
#
# EVERY PATH GIVEN TO THE EXE IS ABSOLUTE AND WINDOWS-SHAPED. A relative one
# opened a scene of a single unnamed node for lane BUILD7 and cost it a whole
# render round; Git-Bash performs no MSYS2 path conversion and winpath() only
# rewrites the /e/... form.
#
# NOTE ON PORTS: the GUI needs a free IPC port and NifSkope EXITS SILENTLY if it
# cannot bind one. Keep the number below ~49152.
#
# USAGE
#   bash tests/spells/hkxanim_ui.sh

set -u

# Real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/fixtures/Running_To_Slide_And_Back_To_Running.hkx}"
REFUSE="${REFUSE:-$ROOT/scratchpad/hkx1_20260910/clips/skeleton.hkx}"
EXPECT="${EXPECT:-93,60}"
FRAME="${FRAME:-46}"
SHOTDIR="${SHOTDIR:-$ROOT/scratchpad/hkx3_20260910}"
SHOT="${SHOT:-$SHOTDIR/dock_clip_midclip.png}"
LOG="$ROOT/release/ww_hkxanim_ui_test.log"
PORT="${PORT:-42293}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP"; exit 2; }
[ -f "$REFUSE" ] || { echo "no non-animation .hkx at $REFUSE"; exit 2; }
mkdir -p "$SHOTDIR"

rm -f "$LOG" "$SHOT"
WW_HKXANIM_UI_TEST=1 \
WW_HKXANIM_UI_CLIP="$(winpath "$CLIP")" \
WW_HKXANIM_UI_REFUSE="$(winpath "$REFUSE")" \
WW_HKXANIM_UI_EXPECT="$EXPECT" \
WW_HKXANIM_UI_FRAME="$FRAME" \
WW_HKXANIM_UI_SHOT="$(winpath "$SHOT")" \
	"$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
echo "--- skips (a SKIP is never a pass) ---"
grep "SKIP" "$LOG" || echo "(none)"
[ -f "$SHOT" ] && echo "dock grab: $SHOT"
grep -q "^PASS" "$LOG" && exit 0
exit 1
