#!/bin/bash
#
# THE CHARGEN PREVIEW CLIPS -- Rigging Manager > Chargen preview: Gender, Body
# Shape Cycle, Facebones Cycle (lane MORPHCYC1, 2026-09-29).
#
# bungo, 2026-09-29, verbatim: "add a button in the rigging manager to load
# these animations and preview them, for both males and females, the
# animations get loaded as the loaded ones, so you can play them as regular
# animations" -- "So body shape, facebones cycle."
#
# ONE window, four NIFs in turn (src/morphcyctest.cpp, WW_MORPHCYC_TEST):
#   male head, female head, male body, female body. The harness presses the
#   dock's own buttons and reads the result through Shape::skinVertex.
#
# ROWS (every one printed with its numbers by the harness log)
#   (x)       male facebones: five channels at +1 against Blender's max vertex
#             displacement, 1e-3 each
#   (x floor) the same five under two WRONG readings must each miss one
#   (b)       body cycle at FAT == saved x diag(RACE fat scale), 1e-5;
#             floor: against the MUSCULAR scale it must be off by > 1e-3
#   (r)(l)(e)(u)(v) rest frame, animations list, export refusal, byte-exact
#             unload, thin -> fat movement
#
# ONE NIFSKOPE AT A TIME, --port below 49152, the window on the second monitor
# (WW_WINDOW_AT, _harness.sh), and its settings in a scope of its own that is
# seeded here and wiped at exit -- never bungo's profile.
#
# USAGE
#   bash tests/spells/morphcyc.sh
set -u
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
CA="${CA:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets}"
MHEAD="$CA/BaseMaleHead_faceBones.nif"
FHEAD="$CA/BaseFemaleHead_faceBones.nif"
MBODY="$CA/MaleBody.nif"
FBODY="$CA/FemaleBody.nif"
OUT="${OUT:-$ROOT/scratchpad/morphcyc1/run}"
LOG="$ROOT/release/ww_morphcyc_test.log"
PORT="${PORT:-42371}"

SCOPE="${SCOPE:-morphcyc}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
trap wipe_scope EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 9; }
for f in "$MHEAD" "$FHEAD" "$MBODY" "$FBODY"; do
	[ -f "$f" ] || { echo "no fixture at $f -- this gate needs the unpacked corpus"; exit 9; }
done
mkdir -p "$OUT"
rm -f "$LOG"

WW_MORPHCYC_TEST=1 \
WW_MORPHCYC_SHOT="$(winpath "$OUT/dock.png")" \
WW_MORPHCYC_NIFS="$(winpath "$FHEAD");$(winpath "$MBODY");$(winpath "$FBODY")" \
WW_SETTINGS_SCOPE="$(fresh_scope)" \
	"$NS" --port "$PORT" "$(winpath "$MHEAD")" > "$OUT/stdout.txt" 2>&1 &
pid=$!
for i in $(seq 1 300); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
# the harness quits the app itself; this only waits for it
for i in $(seq 1 30); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done

if [ ! -f "$LOG" ]; then
	echo "FAIL  the harness wrote no log (did the app exit before it ran, or is port $PORT bound?)"
	exit 1
fi
cp "$LOG" "$OUT/ww_morphcyc_test.log"
cat "$LOG"
grep -q '^PASS$' "$LOG" && exit 0
exit 1
