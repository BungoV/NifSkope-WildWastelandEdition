#!/usr/bin/env bash
# cell_probes.sh -- place PRTP probes in one cell block and photograph them
# (lane PRTPPLACE, 2026-09-30).
#
#   CELL=-15,17 N=5 PN=3 TAG=concord bash tests/spells/cell_probes.sh
#   INTERIOR=<edid> TAG=<name> bash tests/spells/cell_probes.sh
#
# Writes <OUT>/<TAG>.probes.tsv, .psp (the soup), .notes and one PNG per SHOT
# entry: SHOTS="name|view|cx,cy,cz|dist|ortho;..." (view 1 = top, 8 = user;
# center in WORLD units; ortho empty = perspective).
# The first run has no SHOTS entry of its own: pass at least one.
# RED=wall turns on the placer's deliberate defect (WW_PROBE_RED).
set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_probes}"
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
WORLD="${WORLD:-Commonwealth}"
CELL="${CELL:--15,17}"
N="${N:-5}"
PN="${PN:-3}"
TAG="${TAG:-probes}"
OUT="${OUT:-$REPO/scratchpad/prtpplace_20260930/cells}"
PORT="${PORT:-14737}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1822x960}"
SHOTS="${SHOTS:-top|1|0,0,0||6400}"
mkdir -p "$OUT"
if [ -n "${INTERIOR:-}" ]; then OPEN="$ESM|interior|$INTERIOR"; else OPEN="$ESM|$WORLD|$CELL|$N"; fi

IFS=';' read -r -a shots <<< "$SHOTS"
first=1
for s in "${shots[@]}"; do
	IFS='|' read -r name view ctr dist ortho <<< "$s"
	png="$OUT/$TAG.$name.png"
	rm -f "$png"
	extra=()
	[ -n "$ctr" ] && extra+=( "WW_RENDER_CENTER=$ctr" )
	[ -n "$dist" ] && extra+=( "WW_RENDER_DIST=$dist" )
	[ -n "$ortho" ] && extra+=( "WW_RENDER_ORTHO=$ortho" )
	if [ "$first" = 1 ]; then
		extra+=( "WW_CELL_PROBES=$(winpath "$OUT/$TAG.probes.tsv")" "WW_CELL_PROBE_SOUP=$(winpath "$OUT/$TAG.psp")" )
	else
		extra+=( "WW_CELL_PROBES=$(winpath "$OUT/$TAG.shot.tsv")" )
	fi
	start=$(date +%s)
	env "${extra[@]}" \
		WW_CELL_OPEN="$OPEN" WW_CELL_DATAROOT="$DATA" WW_CELL_PROBES_N="$PN" \
		${RED:+WW_PROBE_RED=$RED} \
		WW_RENDER_SHOT="$(winpath "$png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1200 "$EXE" --port "$PORT" "$(winpath "$SPEC")" \
		> "$OUT/$TAG.$name.notes" 2>&1
	rc=$?
	echo "$TAG.$name: rc $rc, $(( $(date +%s) - start )) s, png $( [ -s "$png" ] && echo yes || echo NO )"
	[ "$first" = 1 ] && cp "$OUT/$TAG.$name.notes" "$OUT/$TAG.notes"
	first=0
done
grep -E "^ *probe|^ *openings|^ *probes" "$OUT/$TAG.notes"
