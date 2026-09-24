#!/bin/bash
# Lane UINOTES1b -- the UI harness chain, sequential, one NifSkope at a time.
#
#   bash scratchpad/uinotes1_20260912/ui_chain.sh before   # the RUNG exe (04:10:38)
#   bash scratchpad/uinotes1_20260912/ui_chain.sh after    # release/NifSkope.exe (04:38:12)
#
# Rules this encodes (nifskope-ww-resume-pending sections 5 and 11):
#  * its own lock directory, so a second copy is impossible rather than unlikely;
#  * env on the CHILD (env SHOT=... bash spell), never a prefixed function call,
#    because a variable in front of a shell FUNCTION leaks into the next call;
#  * a process guard before EVERY launch: Fallout4 up, or a NifSkope that is not
#    ours, stops the chain;
#  * its own unused --port per harness, all below 49152;
#  * every log kept separately, the summary line echoed as it lands.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
cd "$R" || exit 2
WHICH="${1:?before|after}"
OUT=$R/scratchpad/uinotes1_20260912
LOGS=$OUT/logs/$WHICH
IMG=$OUT/images/$WHICH
mkdir -p "$LOGS" "$IMG"

mkdir "$OUT/.lock_$WHICH" 2>/dev/null || { echo "REFUSED: a $WHICH chain is already running"; exit 8; }
trap 'rmdir "$OUT/.lock_$WHICH" 2>/dev/null' EXIT

case "$WHICH" in
	before) EXEPATH=$R/release/NifSkope.before_uinotes1.exe; P0=43200 ;;
	after)  EXEPATH=$R/release/NifSkope.exe;                 P0=43100 ;;
	*) echo "REFUSED: argument must be before or after"; exit 2 ;;
esac
[ -x "$EXEPATH" ] || { echo "REFUSED: no exe at $EXEPATH"; exit 2; }
echo "chain=$WHICH exe=$(ls -la --time-style=+%Y-%m-%d_%H:%M:%S "$EXEPATH" | awk '{print $6, $5}')"
echo "start $(date +%H:%M:%S)"

guard () {
	local bad
	bad=$(tasklist | grep -icE "Fallout4\.exe")
	[ "$bad" = "0" ] || { echo "STOP: Fallout4.exe is up"; return 1; }
	bad=$(tasklist | grep -icE "NifSkope\.exe")
	[ "$bad" = "0" ] || { echo "STOP: a NifSkope is already running"; return 1; }
	return 0
}

run () {
	local label="$1"; shift
	local secs="$1"; shift
	guard || { echo "### $label SKIPPED (process guard)"; return 9; }
	echo "### $label start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$LOGS/$label.log" 2>&1
	local rc=$?
	echo "### $label rc=$rc  $(date +%H:%M:%S)"
	grep -aE "checks?, [0-9]+ failure|^PASS$|^FAIL|RESULT|^[0-9]+ checks" "$LOGS/$label.log" | tail -4
	echo
}

# 1. animws.sh -- the nine rulings' own gates (a)-(q). OUTDIR carries its pictures.
run animws 600 env EXE="$EXEPATH" PORT=$((P0+1)) OUTDIR="$IMG/animws" \
	bash tests/spells/animws.sh

# 2. hkxanim_ui.sh -- the dock's older contract, incl. (g) the pinned header
run hkxanim_ui 600 env EXE="$EXEPATH" PORT=$((P0+2)) SHOT="$IMG/hkxanim_dock.png" \
	bash tests/spells/hkxanim_ui.sh

# 3. ui_align.sh -- (s1)(s2), the retired status bar; its SHOT is the window seam
run ui_align 600 env EXE="$EXEPATH" PORT=$((P0+3)) SHOT="$IMG/seam.png" \
	bash tests/spells/ui_align.sh

# 4. water_ui.sh -- the main window's bars and the left dock strip; item 1 moved
#    the main window's chrome, so it is run rather than skipped
run water_ui 600 env EXE="$EXEPATH" PORT=$((P0+4)) SHOT="$IMG/topbar.png" \
	TABSHOT="$IMG/watertab.png" bash tests/spells/water_ui.sh

# 5. files_tab.sh -- the left dock's Files tab; same reason as water_ui
run files_tab 600 env EXE="$EXEPATH" PORT=$((P0+5)) SHOT="$IMG/filestab.png" \
	bash tests/spells/files_tab.sh

# 6. top_bar.sh -- the main window's chrome moved in item 1
run top_bar 600 env EXE="$EXEPATH" PORT=$((P0+6)) bash tests/spells/top_bar.sh

# 7. skeleton_overlay.sh -- the dock drives the skeleton overlay
run skeleton_overlay 900 env EXE="$EXEPATH" PORT=$((P0+7)) OUT="$IMG/skeloverlay" \
	bash tests/spells/skeleton_overlay.sh

echo "end $(date +%H:%M:%S)"
echo "left running: $(tasklist | grep -icE 'NifSkope\.exe') NifSkope process(es)"
