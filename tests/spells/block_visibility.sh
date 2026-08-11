#!/bin/bash
#
# The Block List's visibility: H / Alt+H, and the eye + see-through column that
# replaced Summary.
#
# WHY THIS EXISTS
#
# H and Alt+H worked in the 3D viewport and did nothing in the Block List, which
# is the panel you are looking at when you decide something is in the way. The
# fix is not a second implementation: the key, the row's eye glyph, both context
# menus and the viewport all write ONE set (Scene::hiddenNodes), and the point of
# this harness is that they cannot drift apart -- the glyph path is asserted
# against the set the KEY produced, not against a hard-coded expectation.
#
# WHAT IS MEASURED, per mode
#
#    1. the app started in the mode under test              <- not vacuous
#    2. animations are off                                  <- forced, not inherited
#    3. the Block List shows the visibility column
#    4. Block Details hides it
#    5. the header reads Vis, and
#    6. no column anywhere still says Summary
#    7. the proxy's third column maps to WwVisCol           (hierarchy only)
#    8. the header's total matches its sections             <- the landmine
#    9. every visible row resolves through indexAt          <- what it costs
#   10. an NiAVObject row offers the toggles
#   11. a non-NiAVObject row offers none
#   12. a field row offers none                             (flat list only)
#   13. H over the Block List hides the selected object
#   14. ...and the viewport stops drawing it                <- pixels, not a set
#   15. Alt+H reveals everything
#   16. ...and the frame comes back to what it was          <- exactly, 0 delta
#   17. H hides EVERY selected object                       <- multi-selection
#   18. hiding a NiNode takes its subtree with it
#   19. a child inherits its parent's hiddenness
#   20. clicking the eye reaches the same state the key does <- ONE state
#   21. ...and it did not select the row
#   22. ...and the eye now reads shut
#   23. clicking it again reveals
#   24. sliding off a glyph before releasing cancels it
#   25. ...and it did not select the row on the way
#   26. a click anywhere else in the row still selects it
#   27. a non-drawable row exposes no toggle to hit-test
#   28. ...and clicking where the glyph would be toggles nothing
#   29. clicking the disc marks the block see-through
#   30. see-through is not the same as solid
#   31. see-through is not the same as hidden -- it still draws
#   32. the two ends it sits between really do differ
#   33. H while renaming types an h and hides nothing       <- F2 still works
#   34. the per-type summary is on the block row's tooltip now
#   35. the block list renders
#
# ON 30/31 TOGETHER. A one-sided pixel check would pass on a see-through toggle
# that simply hid the shape: that also "changes the frame". Translucent is the
# thing that is NEITHER, so it is measured against both ends, and 32 proves the
# two ends are themselves distinguishable in this fixture -- without it, 30 and
# 31 could both be satisfied by noise.
#
# WHAT IT STEPS OVER, stated because that is where the last block-list feature
# shipped dead: no physical pointer drives this. The mouse events are synthetic
# and sent to the view's viewport, which is the path Qt routes real ones through
# (drag events are the exception, and are not part of this feature). The rest of
# the gesture -- press claims, move swallows, release over the same glyph -- is
# exercised end to end through those events rather than by calling the toggle.
#
# FIXTURE: the CLI cube (`new --cube`): NiNode 'Scene Root', BSTriShape 'Cube',
# a BSLightingShaderProperty and a BSShaderTextureSet. No game corpus.
#
# USAGE
#   bash tests/spells/block_visibility.sh
#   MODES=list bash tests/spells/block_visibility.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45907}"
LOG="$ROOT/release/ww_blockvis_test.log"
TMP="$(mktemp -d)"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# /c/x -> C:/x, in bash alone: no sed, no cygpath.
winpath() {
	local p="$1"
	case "$p" in
		/?/*) printf '%s:%s' "${p:1:1}" "${p:2}" ;;
		*)    printf '%s' "$p" ;;
	esac
}

# The list mode is read while the window is being built, so it has to be in the
# registry BEFORE launch -- and it is the user's own setting, so it goes back.
KEY="HKCU:\\Software\\NifTools\\NifSkope 2.0\\UI"
ps() { powershell.exe -NoProfile -NonInteractive -Command "$1" 2>/dev/null | tr -d '\r'; }
seed() { ps "Set-ItemProperty -Path '$KEY' -Name 'List Mode' -Value '$1' -Type String"; }
SAVED="$(ps "(Get-ItemProperty -Path '$KEY' -Name 'List Mode' -EA SilentlyContinue).'List Mode'")"
restore() {
	if [ -n "$SAVED" ]; then
		seed "$SAVED"
	else
		ps "Remove-ItemProperty -Path '$KEY' -Name 'List Mode' -EA SilentlyContinue"
	fi
}
trap 'restore; rm -rf "$TMP"' EXIT

"$EXE" -no-gui new --cube -o "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1
[ -s "$TMP/fixture.nif" ] || { echo "FAIL: could not write the cube fixture"; exit 1; }
echo "fixture: cube fixture ($(stat -c%s "$TMP/fixture.nif") bytes)"

fails=0
total_checks=0
total_fails=0
for mode in ${MODES:-hierarchy list}; do
	echo "--- $mode mode"
	seed "$mode"
	[ "$(ps "(Get-ItemProperty -Path '$KEY' -Name 'List Mode').'List Mode'")" = "$mode" ] \
		|| { echo "FAIL: could not seed the list mode"; exit 2; }
	rm -f "$LOG"
	WW_BLOCKVIS_TEST="$mode" \
		"$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
	pid=$!
	for _ in $(seq 1 90); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		kill -0 "$pid" 2>/dev/null || break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null

	if [ ! -f "$LOG" ]; then
		echo "FAIL: the harness wrote no log"; fails=$((fails + 1)); continue
	fi
	cat "$LOG"
	# a process that died half way leaves a log with no verdict in it
	grep -q '^PASS$' "$LOG" || fails=$((fails + 1))
	# grep -c PRINTS 0 and EXITS 1 when it matches nothing, so `|| echo 0`
	# appends a second line and the arithmetic below dies -- fatally, in a
	# non-interactive shell, which silently cost this script its second mode.
	c="$(grep -c '^  ok   \|^  FAIL ' "$LOG" 2>/dev/null || true)"
	f="$(grep -c '^  FAIL ' "$LOG" 2>/dev/null || true)"
	total_checks=$((total_checks + c))
	total_fails=$((total_fails + f))
done

echo "=== $((total_checks - total_fails))/$total_checks checks across both modes"
[ "$fails" = "0" ] || exit 1
exit 0
