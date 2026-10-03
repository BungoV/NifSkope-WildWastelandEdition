#!/bin/bash
#
# AN EXTERIOR BAKE SEES THE 5x5 BLOCK + THE FAR LOD (lane BAKEBLOCK1, 2026-10-03; src/cellview.cpp, src/probefar.cpp).
#
# bungo on CAPTURE1's Concord sheet: "the sky is visible where the geometry should be". RULED 2026-09-30 (PRTP_PLAN
# item 5): each exterior cell is baked with the 5x5 block around it at full detail and LOD beyond. A bake asked as
# `Commonwealth|-15,17|1` now loads the 5x5 and appends the far soup (the worldspace's .lodl ground, .lodi boxes,
# TREE1 trees) out to the bake's ray reach; probes and surfels are still written for the asked cell only.
#
# Runs (headless lean bakes, cell_speed_run.py measures wall s and peak MB into times.tsv):
#   before     the exe before the lane (BEFORE=), Concord -15,17 asked n=1: the one-cell bake
#   green      this exe, Concord asked n=1 (promoted to 5x5 + far)
#   north      this exe, the neighbor -15,18 asked n=1 (the EDGE comparator)
#   red_n1     WW_CELL_BAKEBLOCK_RED=n1 on both: the cell alone, no far (BLOCK FAR OCTANTS HORIZON EDGE fail)
#   red_nolod  WW_CELL_BAKEBLOCK_RED=nolod: the 5x5 with no far soup (FAR HORIZON fail)
# Judge: tests/spells/cell_bakeblock_check.py (independent: its own soup reader and tracer, plus
# tests/prtp_reference.cpp); its header holds the bars.
#
# USAGE  bash tests/spells/cell_bakeblock.sh            (green + both reds, about 9 min, peak ~6.5 GB)
#        RECHECK=1 (judge the runs already baked)
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_bakeblock1.exe}"
REF="${REF:-$REPO/release/prtp_reference.exe}"
SCOPE="${SCOPE:-cell_bakeblock}"
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
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
OUT="${OUT:-$REPO/scratchpad/bakeblock1_20261003/gate}"
PORT="${PORT:-14882}"
RECHECK="${RECHECK:-0}"

bake() {   # bake <exe> <tag> <x,y> [env...]   -- asked n=1 on purpose: the exe must promote it to the 5x5
	local exe="$1" tag="$2" xy="$3"; shift 3
	local run="$OUT/$tag"
	rm -rf "$run"; mkdir -p "$run"
	env "$@" WW_CELL_OPEN="$ESM|Commonwealth|$xy|1" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_RENDER_SHOT="$(winpath "$run/shot.png")" WW_RENDER_SIZE=960x600 WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" \
		python "$REPO/tests/spells/cell_speed_run.py" 2400 "$(winpath "$OUT/times.tsv")" "$tag" "$(winpath "$run/shot.notes")" -- \
			"$(winpath "$exe")" --port "$PORT" "$(winpath "$REPO/tests/fixtures/empty.wwcell")" > /dev/null
	echo "  $tag $(tail -1 "$OUT/times.tsv" | awk -F'	' '{print "rc " $2 ", " $3 " s, peak " $6 " MB"}')  $(grep -o 'bake block[^;]*;[^,]*' "$run/shot.notes" | head -1 | cut -c1-120)"
}

mkdir -p "$OUT"
echo "cell_bakeblock.sh $(date '+%F %T')  out: $OUT"
if [ "$RECHECK" != 1 ]; then
	for f in "$EXE" "$BEFORE" "$REF"; do [ -x "$f" ] || { echo "missing $f"; exit 2; }; done
	rm -f "$OUT/times.tsv"
	bake "$BEFORE" before -15,17
	bake "$EXE" green -15,17
	bake "$EXE" north -15,18
	bake "$EXE" red_n1 -15,17 WW_CELL_BAKEBLOCK_RED=n1
	bake "$EXE" red_n1_north -15,18 WW_CELL_BAKEBLOCK_RED=n1
	bake "$EXE" red_nolod -15,17 WW_CELL_BAKEBLOCK_RED=nolod
fi
CHK="$REPO/tests/spells/cell_bakeblock_check.py"
echo "-- green"
python "$CHK" "$OUT/chk_green" -15 17 "$REF" "$OUT/green" "$OUT/before" "$OUT/north" -15 18 | tee "$OUT/green.verdict"
echo "-- red n1 (must FAIL)"
python "$CHK" "$OUT/chk_n1" -15 17 "$REF" "$OUT/red_n1" "$OUT/before" "$OUT/red_n1_north" -15 18 | tee "$OUT/red_n1.verdict"
echo "-- red nolod (must FAIL on HORIZON)"
python "$CHK" "$OUT/chk_nolod" -15 17 "$REF" "$OUT/red_nolod" "$OUT/before" | tee "$OUT/red_nolod.verdict"
g=$(grep -c "^VERDICT PASS" "$OUT/green.verdict")
r1=$(grep -c "^VERDICT FAIL" "$OUT/red_n1.verdict")
r2=$(grep -c "^FAIL HORIZON" "$OUT/red_nolod.verdict")
if [ "$g" = 1 ] && [ "$r1" = 1 ] && [ "$r2" = 1 ]; then
	echo "cell_bakeblock: PASS (green passes, red n1 fails, red nolod fails the horizon band)"; exit 0
fi
echo "cell_bakeblock: FAIL (green pass $g, red n1 fail $r1, red nolod horizon fail $r2)"; exit 1
