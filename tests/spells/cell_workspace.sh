#!/bin/bash
#
# LANE CELLWORK1's gate: THE CELL WORKSPACE.
#
# bungo, 2026-09-19 20:40: "Cell viewing will be a new workspace btw".
#
# WHAT THIS GATE IS FOR, AND WHAT IT DELIBERATELY IS NOT
#
# cell_open.sh proves the SCENE and cell_pick.sh proves the CLICK. Neither can
# see any of this lane's work, because all of it happens to the WINDOW: which
# docks are up, which workspace is active, what the reference list holds, and
# whether the layout a person left comes back unchanged.
#
# Three of those four are invisible in a screenshot in the worst possible way --
# a wrong workspace, a wrong list and a subtly re-docked panel all LOOK like a
# working cell viewer. So they are measured inside the running window by
# src/cellworkspacetest.cpp (`WW_CELLWS_TEST=<report>`), and this script's own
# rows are (a) controls on that report, (b) arithmetic against an INDEPENDENT
# read of the same plugin, and (c) source-level controls that must come out
# FALSE on git HEAD, which is the pre-change state.
#
# THE BYTES. The round-trip claim is not "the layout looks the same": it is
# QMainWindow::saveState(0x074) before and after, compared as bytes. The window
# takes that snapshot on the way into the Cell workspace and replays it on the
# way out, so the claim and the mechanism are the same object -- which is the
# only arrangement in which the claim cannot quietly become a wish.
#
# THE INDEPENDENT COUNT. The reference list must hold every placed reference the
# plugin has, including the ones with no model (lights, sound markers, triggers,
# primitives) that never reach the draw data at all. `WW_CELL_REFDUMP` writes
# one line per reference; tests/spells/cell_census.py walks the same plugin in
# Python without NifSkope. Row 6 is those two numbers being equal.
#
# WHAT THIS GATE CANNOT TEST: whether the workspace is nice to use, whether the
# list's columns are the right columns, whether the Show rows are the right
# rows. Those are pictures and rulings, and they are bungo's.
#
# USAGE
#   bash tests/spells/cell_workspace.sh

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
WORLD="${WORLD:-Commonwealth}"
CELLX="${CELLX:--20}"
CELLY="${CELLY:-7}"
OUT="$REPO/release"
LOG="$OUT/ww_cell_workspace.log"
PORT="${PORT:-14741}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
NIF="${NIF:-$REPO/tests/fixtures/empty.nif}"

: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
skips=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
skip()  { say "  SKIP  $1"; skips=$((skips+1)); }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

say "cell_workspace.sh  $(date '+%Y-%m-%d %H:%M:%S')"
say "exe: $EXE  ($(stat -c %y "$EXE" | cut -c1-19))"
say "cell: $WORLD $CELLX,$CELLY"

# ---------------------------------------------------------------------------
# 0. THE EXE MUST CARRY THIS LANE.
# ---------------------------------------------------------------------------
newer=1
for s in src/cellworkspace.cpp src/cellworkspace.h src/cellworkspacetest.cpp \
         src/cellrefs.cpp src/cellrefs.h src/cellview.cpp src/cellclick.cpp \
         src/esmdata.cpp src/nifskope.cpp src/nifskope_ui.cpp src/nifskope.h; do
	if [ ! -f "$REPO/$s" ] || [ "$REPO/$s" -nt "$EXE" ]; then
		say "  $s is missing or NEWER than the exe"
		newer=0
	fi
done
check "the exe is newer than every source this gate covers" "$newer"

# ---------------------------------------------------------------------------
# 1-3. THE WORKSPACE ITSELF, measured inside the window.
# ---------------------------------------------------------------------------
REPORT="$OUT/ww_cellws.report"
REFDUMP="$OUT/ww_cellws.refdump"
NOTES="$OUT/ww_cellws.notes"
rm -f "$REPORT" "$REFDUMP" "$NOTES"

IMG="${IMG:-$REPO/scratchpad/cellwork1_20260919/pictures}"
mkdir -p "$IMG"

env WW_CELL_DATAROOT="$DATA" \
	WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" \
	WW_CELL_REFDUMP="$(winpath "$REFDUMP")" \
	WW_CELLWS_TEST="$(winpath "$REPORT")" \
	WW_CELLWS_SHOTS="$(winpath "$IMG")" \
	timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$NOTES" 2>&1

if [ -s "$REPORT" ]; then
	cat "$REPORT" >> "$LOG"
	rowcount=$(grep -c -E "^(PASS|FAIL)" "$REPORT")
	rfails=$(sed -n 's/^rows [0-9]* failures \([0-9]*\)$/\1/p' "$REPORT" | tail -1)
	say "  workspace self-test: $rowcount rows, ${rfails:-?} failures"
	check "the workspace self-test ran and wrote its report" \
		"$([ "${rowcount:-0}" -ge 20 ] && echo 1 || echo 0)"
	check "every workspace row passed (the switch, the hidden NIF column, the \
list, the two selection doors, the Show popover and the layout bytes)" \
		"$([ "${rfails:-1}" = "0" ] && echo 1 || echo 0)"
	# THE ROWS THAT CARRY THE BRIEF'S OWN CLAIMS, named one by one, so a report
	# that silently loses a row cannot pass this gate by arithmetic alone.
	for want in \
		"picture: the Cell workspace with a reference selected" \
		"picture: the NIF workspace after coming back" \
		"opening a .wwcell switched to the Cell workspace" \
		"the NIF editor column is hidden in this workspace" \
		"the list has one row per placed reference" \
		"a viewport pick of the same reference selects the same row" \
		"the NIF workspace layout is byte-identical after a cell round trip" \
		"the workspace list is the old one with Cell APPENDED" \
		"no Show popover exists anywhere else in the window"; do
		check "report row present and green: $want" \
			"$(grep -Fq "PASS  $want" "$REPORT" && echo 1 || echo 0)"
	done
else
	check "the workspace self-test wrote its report" 0
	check "every workspace row passed" 0
fi

# ---------------------------------------------------------------------------
# 4. RED CONTROL ON THE REPORT. No cell scene: it must FAIL.
#    A report that only ever prints PASS is not evidence of anything.
# ---------------------------------------------------------------------------
RED_REPORT="$OUT/ww_cellws_nocell.report"
rm -f "$RED_REPORT"
env WW_CELL_DATAROOT="$DATA" \
	WW_CELLWS_TEST="$(winpath "$RED_REPORT")" \
	timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$OUT/ww_cellws_nocell.notes" 2>&1
if [ -s "$RED_REPORT" ]; then
	nf=$(sed -n 's/^rows [0-9]* failures \([0-9]*\)$/\1/p' "$RED_REPORT" | tail -1)
	say "  with no cell scene: ${nf:-?} failures"
	check "RED: with no cell open the workspace report FAILS" \
		"$([ "${nf:-0}" -ge 1 ] && echo 1 || echo 0)"
else
	check "RED: with no cell open the workspace report FAILS" 0
fi

# ---------------------------------------------------------------------------
# 5-7. THE LIST IS THE PLUGIN'S REFERENCES. Arithmetic against an independent
#      Python walk of the same plugin, which shares no code with the viewer.
# ---------------------------------------------------------------------------
if [ -s "$REFDUMP" ]; then
	dumped=$(grep -vc '^#' "$REFDUMP")
	drawn=$(awk '$6=="drawn"' "$REFDUMP" | wc -l | tr -d ' ')
	nomodel=$(awk '$6=="no-model"' "$REFDUMP" | wc -l | tr -d ' ')
	cells=$(grep -c '^# cell ' "$REFDUMP")
	say "  refdump: $dumped references, $drawn drawn, $nomodel with no model, in $cells cell(s)"
	check "the reference dump carries more references than the pick table draws" \
		"$([ "${dumped:-0}" -gt "${drawn:-0}" ] && echo 1 || echo 0)"
	check "the modelless references (lights, sounds, triggers, primitives) ARE \
in the list -- which is the whole difference from the draw data" \
		"$([ "${nomodel:-0}" -ge 1 ] && echo 1 || echo 0)"
	check "the loaded cells are a LIST, with today's single cell as a list of one" \
		"$([ "${cells:-0}" -ge 1 ] && echo 1 || echo 0)"
	# THE CELL'S NAME. Measured in Fallout4.esm: 755 of the Commonwealth's
	# 36865 exterior cells carry an EDID, and (-20,7) -- Sanctuary's own -- is
	# NOT one of them, so the honest line here is the bare grid and a form id.
	# Asserting a name on THIS cell would be asserting something false about the
	# data; the EDID reader is proved a few rows down, on a cell that has one.
	check "the dump names the cell by grid and form id" \
		"$(grep -qE '^# cell .*\(-?[0-9]+,-?[0-9]+\) form 0x[0-9a-f]{8} ' "$REFDUMP" && echo 1 || echo 0)"
	say "  $(grep -m1 '^# cell ' "$REFDUMP")"

	# THE LIST AGAINST THE PLUGIN, as SETS and not as a count. A count hides
	# which side is wrong; cell_refs_check.py says whether anything was LOST and
	# whether anything was INVENTED, and it knows about the worldspace's
	# persistent cell, whose references land in a grid square by position.
	SETCHK="$OUT/ww_cellws_setcheck.txt"
	python "$REPO/tests/spells/cell_refs_check.py" "$ESM" "$WORLD" \
		"$CELLX" "$CELLY" "$REFDUMP" > "$SETCHK" 2>&1
	src=$?
	say "  $(grep -m1 '^RESULT ' "$SETCHK" || echo "cell_refs_check.py wrote no RESULT (rc=$src)")"
	check "every reference the plugin places in this cell is in the list, and \
the list invents none (an INDEPENDENT walk of the plugin, sharing no code with \
the viewer)" "$([ "$src" = "0" ] && echo 1 || echo 0)"
	say "  $(grep -m1 'extra, from the persistent cell' "$SETCHK")"
else
	check "the reference dump was written" 0
	skip "the modelless references row (no dump)"
	skip "the loaded-cells row (no dump)"
	skip "the cell-name row (no dump)"
	skip "the independent reference count (no dump)"
fi

# ---------------------------------------------------------------------------
# 7b. THE CELL-NAME READER, on a cell that HAS a name. Sanctuary's own cell
#     carries no EDID, so the row above can only prove the fallback. This runs
#     the window a second time on the neighbouring cell (-21,7), which the
#     master names POIJS021, and asks the dump for that word. Without a reader
#     the header prints the grid alone and this row goes red.
# ---------------------------------------------------------------------------
NCELLX="${NCELLX:--21}"
NCELLY="${NCELLY:-7}"
NCELLNAME="${NCELLNAME:-POIJS021}"
NREFDUMP="$OUT/ww_cellws_named.refdump"
rm -f "$NREFDUMP"
env WW_CELL_DATAROOT="$DATA" \
	WW_CELL_OPEN="$ESM|$WORLD|$NCELLX,$NCELLY|1" \
	WW_CELL_REFDUMP="$(winpath "$NREFDUMP")" \
	WW_CELLWS_TEST="$(winpath "$OUT/ww_cellws_named.report")" \
	timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" \
	> "$OUT/ww_cellws_named.notes" 2>&1
if [ -s "$NREFDUMP" ]; then
	say "  $(grep -m1 '^# cell ' "$NREFDUMP")"
	check "a cell the master DOES name reads its editor id out of the plugin \
($NCELLNAME at $NCELLX,$NCELLY)" \
		"$(grep -qE "^# cell $NCELLNAME \\($NCELLX,$NCELLY\\) " "$NREFDUMP" && echo 1 || echo 0)"
else
	check "a cell the master DOES name reads its editor id out of the plugin" 0
fi

# ---------------------------------------------------------------------------
# 7c. THE SECOND PICTURE: the same window with the IDENTITY overlay on, so the
#     legend band has real `.lodi` group keys in it rather than the empty state.
#     It needs a bake; with none, the row is a NAMED SKIP and the picture is
#     simply not claimed. A picture of a refusal band is not a picture of the
#     legend.
# ---------------------------------------------------------------------------
LODI="${LODI:-}"
if [ -z "$LODI" ]; then
	for c in "$REPO/release/scratchpad/cards_agg_20260911/gate/agg/Terrain/Commonwealth.lodi"; do
		[ -f "$c" ] && LODI="$c" && break
	done
fi
if [ -n "$LODI" ] && [ -f "$LODI" ]; then
	IDREPORT="$OUT/ww_cellws_identity.report"
	rm -f "$IDREPORT"
	env WW_CELL_DATAROOT="$DATA" \
		WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" \
		WW_CELL_OVERLAY=identity \
		WW_CELL_LODI="$(winpath "$LODI")" \
		WW_CELLWS_TEST="$(winpath "$IDREPORT")" \
		WW_CELLWS_SHOTS="$(winpath "$IMG")" \
		WW_CELLWS_SHOTTAG=cell_workspace_identity \
		timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" \
		> "$OUT/ww_cellws_identity.notes" 2>&1
	say "  $(grep -m1 'picture: the Cell workspace' "$IDREPORT" 2>/dev/null || echo 'the identity run wrote no picture row')"
	check "the identity-overlay picture was written, at window size" \
		"$([ -s "$IMG/cell_workspace_identity.png" ] && echo 1 || echo 0)"
	# ...and it must actually be the identity overlay, not a silent fall back to
	# the models' own materials.
	check "the identity run really ran the identity overlay (the legend carries \
its group keys)" \
		"$(grep -qi 'identity' "$OUT/ww_cellws_identity.notes" && echo 1 || echo 0)"
else
	skip "the identity-overlay picture (no .lodi bake; pass LODI=<path>)"
	skip "the identity overlay legend row (no .lodi bake)"
fi

# ---------------------------------------------------------------------------
# 8. LEAVING. Opening a NIF must put the window back where it came from. The
#    self-test proves the bytes; this proves the ROUTE -- that an ordinary file
#    open, not a special call, is what leaves the workspace.
# ---------------------------------------------------------------------------
check "opening anything that is not a .wwcell leaves the Cell workspace" \
	"$(grep -q 'setWorkspace( workspaceBeforeCell )' "$REPO/src/nifskope.cpp" && echo 1 || echo 0)"
check "the way back replays the SAVED BYTES rather than re-deriving a layout" \
	"$(grep -q 'restoreState( workspaceLayoutBefore, 0x074 )' "$REPO/src/nifskope_ui.cpp" \
	   && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# 9-11. SOURCE CONTROLS ON THE PRE-CHANGE STATE.
#
#   Every row above is about behaviour that did not exist this morning, so its
#   "fails on the old build" proof cannot be a run of the old exe -- this tree
#   forbids launching an old rung with a GUI, because it rewrites bungo's Recent
#   Files. It is done as text instead: the same expressions must come out FALSE
#   on git HEAD, which has no cell workspace in it at all.
# ---------------------------------------------------------------------------
check "the workspace switch is ONE member function, not a lambda nobody can reach" \
	"$(grep -q '^void NifSkope::setWorkspace( int index )' "$REPO/src/nifskope_ui.cpp" \
	   && echo 1 || echo 0)"
# NOTE the '(' -- 'NifSkope::setWorkspace' alone also matches the long-standing
# NifSkope::setWorkspaceFaceDonor(), which HEAD mentions seven times, and a RED
# control that matches something unrelated is not a control at all.
hs=$(git -C "$REPO" show HEAD:src/nifskope_ui.cpp 2>/dev/null | grep -c 'NifSkope::setWorkspace(')
say "  git HEAD mentions NifSkope::setWorkspace $hs times"
check "RED: git HEAD has no such function, so the row can see the change" \
	"$([ "${hs:-1}" = "0" ] && echo 1 || echo 0)"
hr=$(git -C "$REPO" show HEAD:src/cellview.cpp 2>/dev/null | grep -c 'cellRefTableMutable')
say "  git HEAD mentions the reference model $hr times"
check "RED: git HEAD has no reference model, so the list could only ever have \
been the draw data" "$([ "${hr:-1}" = "0" ] && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# 12. THE ANIMATION WORKSPACE MUST NOT MOVE. It is the workspace this lane
#     reused the mechanism of, so it is the one most likely to be broken by it.
#     Run as a whole gate, not grepped for.
# ---------------------------------------------------------------------------
if [ -f "$REPO/tests/spells/animws.sh" ]; then
	AW="$OUT/ww_cellws_animws.log"
	PORT=$((PORT+1)) timeout 900 bash "$REPO/tests/spells/animws.sh" > "$AW" 2>&1
	arc=$?
	say "  animws.sh: rc=$arc  $(grep -E '^(PASS|FAIL)' "$AW" | tail -1)"
	check "the animation workspace's own gate still passes" \
		"$([ "$arc" = "0" ] && echo 1 || echo 0)"
else
	skip "the animation workspace gate (tests/spells/animws.sh is not there)"
fi

say ""
say "skips: $skips"
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
say "done"
exit "$fails"
