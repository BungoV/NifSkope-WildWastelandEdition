#!/bin/bash
#
# THE BYTE GATE FOR THE PANEL'S NEW ROWS (lane PANEL1, 2026-09-12).
#
# bungo, 15:4x: "Erosion is a knob in the menu, corret?" -- it was not -- then
# "They should all be configurable in the gen menu". 57 rows answer that. A row
# that exists, scrolls, keeps its value and names its switch can still be wired
# to nothing, so this spell asks the only question that settles it: does the
# BAKE move when the row moves?
#
# Three phases, each a number, on one chunk -- Sanctuary (-20,24) at dim 4:
#
#   (a) THE RUNG. The same panel-driven run, on `NifSkope.before_panel1.exe`
#       and on the new exe, compared file by file. Every new row is at its
#       default in both, so the trees must be IDENTICAL: no default moved.
#   (b) PER ROW. The new exe bakes once with every new row at its default, then
#       once per row with that row alone moved. A row whose bake is identical
#       to the default one reaches NOTHING, and it is named. Two rows -- the
#       thread counts -- are the control: they must NOT move a byte.
#   (c) THE COMMAND LINE. The same chunk baked by `-no-gui lodgen` with the
#       switches the panel's defaults correspond to, compared with the panel's
#       own default bake, so panel and command line can be held to the same
#       bytes rather than to the same intentions.
#
# It is slow (50-odd bakes) and opt-in for that reason; `lod_generation.sh`
# stays the quick structural spell.
#
# USAGE
#   bash tests/spells/lodgen_byte_gate.sh
#   PHASES=bc bash tests/spells/lodgen_byte_gate.sh    # skip the cross-exe one
#   WW_LODGEN_GATE_MS=600000 bash tests/spells/lodgen_byte_gate.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_panel1.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
LOG="$ROOT/release/ww_lodgen_test.log"
DIG="$ROOT/tests/spells/lodgen_tree_digest.py"
PHASES="${PHASES:-abc}"
TMP="${TEMP:-/c/Users/$USER/AppData/Local/Temp}"
fails=0

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

# THE GAME, and the one-GUI-instance rule. A harness launch while Fallout 4 is
# up is a lane's mistake, not a flake; another lane's --port instance is that
# lane's work and is waited for, never killed.
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "FAIL: Fallout 4 is running -- no GUI harness while the game is up"
	exit 2
fi
waitForOtherGui() {
	local n=0
	while powershell -NoProfile -Command \
		"Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -like '*--port*' } | Measure-Object | ForEach-Object { \$_.Count }" \
		2>/dev/null | tr -d '\r' | grep -qv '^0$'; do
		n=$(( n + 1 ))
		[ "$n" -gt 180 ] && { echo "FAIL: another --port NifSkope has been up for 30 minutes"; exit 2; }
		echo "  another --port NifSkope is up; waiting (${n})"
		sleep 10
	done
}

# ---- (a) THE RUNG: the same panel run on both exes, compared -----------------
if [ "${PHASES#*a}" != "$PHASES" ]; then
	echo "== (a) the rung: the panel's own run, before and after =="
	RUNDIR="$TMP/Lodgen_LODUI1_run"
	for who in rung new; do
		[ "$who" = rung ] && E="$RUNG" || E="$NS"
		[ -x "$E" ] || { echo "FAIL: no exe at $E"; fails=$(( fails + 1 )); continue; }
		waitForOtherGui
		rm -f "$LOG"
		WW_LODGEN_TEST=1 WW_LODGEN_RUN=1 "$E" --port 42361 "$SRC" >/dev/null 2>&1
		if [ ! -d "$RUNDIR" ]; then
			echo "FAIL: $who wrote no output tree at $RUNDIR"
			fails=$(( fails + 1 ))
			continue
		fi
		rm -rf "$TMP/Lodgen_PANEL1_$who"
		cp -r "$RUNDIR" "$TMP/Lodgen_PANEL1_$who"
		cp -f "$LOG" "$ROOT/release/ww_lodgen_run_$who.log" 2>/dev/null
		echo "  $who: $(grep -a ' checks, ' "$LOG" | tail -1)"
	done
	if [ -d "$TMP/Lodgen_PANEL1_rung" ] && [ -d "$TMP/Lodgen_PANEL1_new" ]; then
		python "$DIG" "$TMP/Lodgen_PANEL1_rung" "$TMP/Lodgen_PANEL1_new" || fails=$(( fails + 1 ))
	else
		echo "FAIL: one of the two trees is missing"
		fails=$(( fails + 1 ))
	fi
fi

# ---- (b) PER ROW: one bake per row, on the new exe ---------------------------
if [ "${PHASES#*b}" != "$PHASES" ]; then
	echo "== (b) per row: does the bake move when the row moves? =="
	waitForOtherGui
	rm -f "$LOG"
	WW_LODGEN_TEST=1 WW_LODGEN_GATE=1 "$NS" --port 42362 "$SRC" >/dev/null 2>&1
	if [ ! -f "$LOG" ]; then
		echo "FAIL: no log -- did the app exit before the harness ran?"
		exit 1
	fi
	grep -a "^gate \|^  row \|^  same bytes\|^  refused" "$LOG"
	cp -f "$LOG" "$ROOT/release/ww_lodgen_gate.log"
	COUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
	FAILED="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $3}')"
	echo "checks run: ${COUNT:-none} (floor 125), failures: ${FAILED:-?}"
	case "$COUNT" in
		''|*[!0-9]*) echo "FAIL: the log carries no check count"; exit 1 ;;
	esac
	[ "$COUNT" -ge 125 ] || { echo "FAIL: only $COUNT checks ran, floor is 125"; fails=$(( fails + 1 )); }
	grep -qa "^gate rows bumped: " "$LOG" || { echo "FAIL: the gate leg never ran"; fails=$(( fails + 1 )); }
	grep -q "^PASS" "$LOG" || fails=$(( fails + 1 ))
fi

# ---- (c) THE COMMAND LINE: the same chunk, file by file ----------------------
#
# The panel bakes every module it is ticked for in ONE run. The command line
# cannot: --lodl takes the run over and writes the landscape file alone, and the
# chunk sweep needs its own --tex-dir. So this is two CLI runs, compared with
# the panel's base tree FILE BY NAME -- the panel nests its output the way the
# game does and the command line writes where it is pointed, so the paths
# differ while the bytes must not.
if [ "${PHASES#*c}" != "$PHASES" ]; then
	echo "== (c) the command line's own bake of the same chunk =="
	if [ ! -f "$ESM" ]; then
		echo "SKIP: no Fallout4.esm at $ESM"
	elif [ ! -d "$TMP/Lodgen_PANEL1_base" ]; then
		echo "SKIP: phase (b) left no base tree to compare with"
	else
		BASE="$TMP/Lodgen_PANEL1_base"
		CLIL="$TMP/Lodgen_PANEL1_cli_lodl"
		CLIS="$TMP/Lodgen_PANEL1_cli_sweep"
		# THE FO4CS-TARGET FILES MOVED under one root inside the mod folder
		# (lane LAYOUT1, 2026-09-16, bungo 19:3x: "The folder should be called
		# FO4CSLOD maybe, so it'd be Data/FO4CSLOD"). Both trees below are FO4CS
		# bakes, so the same sub-path names the same file on both sides.
		FO4="FO4CSLOD/Commonwealth"
		rm -rf "$CLIL" "$CLIS"
		mkdir -p "$CLIL" "$CLIS/tex"
		WL="$(cygpath -m "$CLIL" 2>/dev/null || echo "$CLIL")"
		WS="$(cygpath -m "$CLIS" 2>/dev/null || echo "$CLIS")"
		"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
			--terrain-region -20 24 -20 24 --dim 4 \
			--out-dir "$WL" --lodl "$WL" \
			> "$ROOT/release/ww_lodgen_gate_cli_lodl.log" 2>&1
		echo "  CLI landscape file rc=$?"
		"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
			--terrain-region -20 24 -20 24 --dim 4 \
			--out-dir "$WS" --tex-dir "$WS/tex" --native "$WS" --cover --arrays \
			> "$ROOT/release/ww_lodgen_gate_cli_sweep.log" 2>&1
		echo "  CLI chunk sweep rc=$?"
		same=0; diff=0; missing=0
		cmpone() {   # $1 = the panel's file, $2 = the command line's
			if [ ! -f "$1" ] || [ ! -f "$2" ]; then
				echo "  MISSING: $(basename "$1")"
				missing=$(( missing + 1 ))
			elif cmp -s "$1" "$2"; then
				echo "  identical: $(basename "$1") ($(stat -c%s "$1") bytes)"
				same=$(( same + 1 ))
			else
				echo "  DIFFERS: $(basename "$1") ($(stat -c%s "$1") vs $(stat -c%s "$2") bytes)"
				diff=$(( diff + 1 ))
			fi
		}
		cmpone "$BASE/$FO4/Commonwealth.lodl" "$CLIL/$FO4/Commonwealth.lodl"
		# THE .BTR IS A LEGACY CHUNK and did not move (lane LAYOUT1, 2026-09-16):
		# the panel writes it under meshes/terrain/<ws>/ and the command line at
		# its --out-dir root, exactly as before. The MANIFEST SIDECAR did move:
		# it describes the FO4CS bake, so it lands beside the files it describes
		# under FO4CSLOD/<ws>/ on BOTH front ends.
		cmpone "$BASE/meshes/terrain/Commonwealth/Commonwealth.4.-20.24.BTR" \
			"$CLIS/Commonwealth.4.-20.24.BTR"
		cmpone "$BASE/$FO4/Commonwealth.4.-20.24.BTO.manifest.txt" \
			"$CLIS/$FO4/Commonwealth.4.-20.24.BTO.manifest.txt"
		# THE .BTO IS NOT AN OUTPUT OF EITHER SIDE ANY MORE (lane BTOFREE1,
		# 2026-09-16). Both trees above are FO4CS bakes, so both build their chunks
		# in a scratch folder and remove them. Comparing two files that should not
		# exist would read as MISSING on both sides and count as a failure, so the
		# row asks the question that is actually left: did BOTH front ends drop the
		# chunk, and did BOTH tidy up after themselves? The manifest above is still
		# compared byte for byte, which is what proves the sidecar survived the move.
		pb="$BASE/meshes/terrain/Commonwealth/Commonwealth.4.-20.24.BTO"
		cb="$CLIS/Commonwealth.4.-20.24.BTO"
		if [ ! -e "$pb" ] && [ ! -e "$cb" ]; then
			echo "  dropped by both: Commonwealth.4.-20.24.BTO"
			same=$(( same + 1 ))
		else
			echo "  STILL THERE: Commonwealth.4.-20.24.BTO (panel $([ -e "$pb" ] && echo yes || echo no), cli $([ -e "$cb" ] && echo yes || echo no))"
			diff=$(( diff + 1 ))
		fi
		for d in "$BASE/lodgen_bto_scratch" "$CLIS/lodgen_bto_scratch"; do
			if [ -d "$d" ]; then
				echo "  STILL THERE: the scratch folder $d"
				diff=$(( diff + 1 ))
			else
				same=$(( same + 1 ))
			fi
		done
		for f in Commonwealth.4.-20.24.DDS Commonwealth.4.-20.24_data.DDS \
			Commonwealth.4.-20.24_msn.DDS; do
			cmpone "$BASE/textures/terrain/Commonwealth/$f" "$CLIS/tex/$f"
		done
		for f in Commonwealth.LodgenArrays.256x256.lodm Commonwealth.LodgenArrays.txt \
			Commonwealth.LodgenArrays.256x256_d.DDS Commonwealth.LodgenArrays.256x256_n.DDS \
			Commonwealth.LodgenArrays.256x256_g.DDS Commonwealth.LodgenArrays.256x256_gsaos.DDS; do
			cmpone "$BASE/$FO4/Objects/$f" "$CLIS/$FO4/Objects/$f"
		done
		for f in Commonwealth.lodo Commonwealth.lodi; do
			cmpone "$BASE/$FO4/$f" "$CLIS/$FO4/$f"
		done
		echo "  panel vs command line: $same identical, $diff differ, $missing missing"
		[ "$diff" -eq 0 ] && [ "$missing" -eq 0 ] || fails=$(( fails + 1 ))
	fi
fi

echo "byte gate failures: $fails"
[ "$fails" -eq 0 ] && exit 0
exit 1
