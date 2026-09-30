#!/bin/bash
#
# Lane PRTP1 (2026-09-30): INTERIOR cells open in the cell view, and every
# placed light is read (LIGH DATA, REFR XRDS / XLIG, initially disabled).
#
# The expected rows come from an INDEPENDENT walk of Fallout4.esm
# (scratchpad/prtp1_20260930/light_census.py, its own GRUP walk in Python,
# sharing no code with src/esmdata.cpp), written before this code first ran.
# light_gate.py diffs NifSkope's WW_CELL_LIGHTS dump against it row by row:
# ref, base, base EDID, position, radius, XRDS, XLIG present, initially disabled.
#
# Ten interiors: the three with the most lights and seven drawn at random
# (seed 930) from the 1,530 cells with lights -- not ten easy ones.
#
# RED CONTROL: `--red` drops one row of each dump; every row must then FAIL.
#
# USAGE
#   bash tests/spells/cell_lights.sh          the gate
#   bash tests/spells/cell_lights.sh --red    the refuter

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_lights}"
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
wipe_scope
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
S="$REPO/scratchpad/prtp1_20260930"
CENSUS="${CENSUS:-$S/fo4_lights.tsv}"
OUT="$S/lights"
LOG="$OUT/cell_lights.log"
PORT="${PORT:-14733}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1822x960}"
RED=""
[ "${1:-}" = "--red" ] && RED="--drop-one"
CELLS="${CELLS:-InstituteConcourse Vault111Cryo Vault75 PackInCZSpotlightMainStorageCell WarehouseGunTraps COPY0019 MedfordMemorial01 DmndSolomonsHouse01 USAFSatellite01 PackInLightBulbHanging01StorageCell}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() {   # check "<what>" <0|1>
	if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi
}
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$CENSUS" ] || { echo "no census at $CENSUS (run light_census.py)"; exit 2; }
say "cell_lights.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL RUN}"
say "exe: $EXE  ($(stat -c %y "$EXE" | cut -c1-19))"

newer=1
for s in src/cellview.cpp src/cellview.h src/esmdata.cpp src/esmdata.h; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

for cell in $CELLS; do
	dump="$OUT/$cell.lights.tsv"; notes="$OUT/$cell.notes"; shot="$OUT/$cell.png"
	rm -f "$dump" "$notes" "$shot"
	WW_CELL_OPEN="$ESM|interior|$cell" \
	WW_CELL_DATAROOT="$DATA" \
	WW_CELL_LIGHTS="$(winpath "$dump")" \
	WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
	WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
	WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	grep -E "cell view interior|lights:" "$notes" >> "$LOG" 2>&1
	if [ -s "$dump" ]; then
		line="$(python "$S/light_gate.py" "$CENSUS" "$cell" "$dump" $RED)"; rc=$?
	else
		line="$cell: no dump written"; rc=1
	fi
	say "  $line"
	if [ -n "$RED" ]; then
		check "$cell: a dropped row FAILS (red control)" "$([ $rc -ne 0 ] && echo 1 || echo 0)"
	else
		check "$cell: every light matches the independent walk" "$([ $rc -eq 0 ] && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
