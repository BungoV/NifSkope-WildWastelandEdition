#!/bin/bash
#
# Loaded NIFs merge targeting: the skull is optional, but decisive when selected.
#
# This uses two real Power Armor pieces and the game's real skeleton because a
# bare cube fixture cannot distinguish a flat skin-reference node list from a genuine
# parent/child bone hierarchy. It covers all three contracts:
#   - clothes merge normally with no skeleton;
#   - a marked skeleton outside the selection does not interfere;
#   - a selected flat marker is refused, while a selected real skeleton becomes
#     the target even when the clothing row was requested as target.

set -u

. "$(dirname "$0")/../spells/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45927}"
LOG="$ROOT/release/ww_workspace_test.log"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
LEFT="${LEFT:-$X/X01_ArmLeft.nif}"
RIGHT="${RIGHT:-$X/X01_ArmRight.nif}"
SKELETON="${SKELETON:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$LEFT" ] || { echo "no left-arm fixture at $LEFT"; exit 2; }
[ -f "$RIGHT" ] || { echo "no right-arm fixture at $RIGHT"; exit 2; }
[ -f "$SKELETON" ] || { echo "no skeleton fixture at $SKELETON"; exit 2; }

"$EXE" -no-gui new --cube -o "$(winpath "$TMP/primary.nif")" >/dev/null 2>&1
[ -s "$TMP/primary.nif" ] || { echo "FAIL: could not build the primary fixture"; exit 1; }

rm -f "$LOG"
WW_WORKSPACE_TEST="$(winpath "$LEFT");$(winpath "$RIGHT");$(winpath "$SKELETON")" \
	WW_WORKSPACE_MERGE=1 WW_WORKSPACE_SKELETON_CONTRACT=1 \
	"$EXE" --port "$PORT" "$(winpath "$TMP/primary.nif")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
