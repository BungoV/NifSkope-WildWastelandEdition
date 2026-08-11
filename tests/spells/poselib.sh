#!/bin/bash
#
# The folder pose library, driven through the real dock: save -> list -> select
# -> Apply -> Delete.
#
# WHY THIS EXISTS
#
# WW_POSELIB_TEST has been in the binary since the pose work landed and there
# was never a script to run it, so its failure sat in the handoff as an open
# item nobody could re-measure without hand-assembling a command line. That is
# the same "a feature nothing tests is a feature nobody can tell is there"
# problem the rename harness was written for, one level up: a harness nothing
# runs is a harness nobody can tell is broken.
#
# WHAT IS MEASURED (all of it inside the app; this script only sets the stage)
#
#   - the library-folder QSettings override persists
#   - a posed bone exports into <library>/Poses the way "Save current" does
#   - the dock's refresh lists the saved pose
#   - importing that file DIRECTLY reproduces the rotation      <- isolates the
#     core from the dock, so a failure says which half broke
#   - selecting the row and clicking the dock's real Apply reproduces it too
#   - clicking the real Delete (confirm auto-answered) removes the file
#
# FIXTURE: the power-armour FRAME, not the skeleton, and that is the one thing
# here which is easy to get wrong. `refreshPoseBones()` builds its bone set out
# of the bones every SKINNED shape references, and only falls back to the node
# hierarchy in skeleton VIEW -- which this harness does not turn on. skeleton.nif
# has no skinned shape at all, so the set comes back empty and the run dies at
# "no bone to pose" having measured the library not at all. Frame.nif is skinned
# AND carries its own bone nodes, so it has both halves. Override with SRC=... .
#
# THE LIBRARY ROOT IS THE USER'S OWN SETTING, so it is saved before the run and
# put back on exit -- the harness points it at release/ww_poselib and does not
# restore it itself.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/poselib.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45893}"
LOG="$ROOT/release/ww_poselib_test.log"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/Frame.nif}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

# pure bash: /e/x -> E:/x, no sed, no cygpath — two runtimes that silently stop
# agreeing is how a winpath quietly becomes a no-op
winpath() {
	local p="$1"
	case "$p" in
		/?/*) printf '%s:%s' "$(printf '%s' "${p:1:1}" | tr '[:lower:]' '[:upper:]')" "${p:2}" ;;
		*) printf '%s' "$p" ;;
	esac
}

KEY="HKCU:\\Software\\NifTools\\NifSkope 2.0"
ps() { powershell.exe -NoProfile -NonInteractive -Command "$1" 2>/dev/null | tr -d '\r'; }
SAVED="$(ps "(Get-ItemProperty -Path '$KEY' -Name 'Settings\\Library\\Library Folder' -EA SilentlyContinue).'Settings\\Library\\Library Folder'")"
restore() {
	if [ -n "$SAVED" ]; then
		ps "Set-ItemProperty -Path '$KEY' -Name 'Settings\\Library\\Library Folder' -Value '$SAVED' -Type String" >/dev/null
	else
		ps "Remove-ItemProperty -Path '$KEY' -Name 'Settings\\Library\\Library Folder' -EA SilentlyContinue" >/dev/null
	fi
	rm -rf "$ROOT/release/ww_poselib"
}
trap restore EXIT

rm -f "$LOG"
rm -rf "$ROOT/release/ww_poselib"
WW_POSELIB_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^done$' "$LOG" || { echo "FAIL: the run did not finish"; exit 1; }
grep -q '^PASS' "$LOG" || exit 1
exit 0
