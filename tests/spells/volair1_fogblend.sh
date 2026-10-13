#!/bin/bash
#
# VOLUMETRIC AIR, OPTION B (lane VOLAIR1, 2026-10-13; WW_CELL_VOLAIR_FOGBLEND, test only, no menu row).
# Option A (the default) draws the real light only (colour x T + L). B=1 also draws the game's fog colour over the
# lit air with the game's own alpha, which is 0 nearer than its start distance (fogK0 z / x). Not judged by eye.
# Vault111Cryo (its own fog starts at 500 units; the gate camera, view 4, dist 500, sees both sides of that).
#   a    WW_CELL_AIR=1 terms 2, FOGBLEND unset        b0   the same, FOGBLEND=0      b1   FOGBLEND=1 (+ a dump)
#   geo  the gate's fog geometry probe (d / 16384 per fragment), shot here unless GEO points at one
# PASS:  b0 == a byte for byte;  b1 differs from a only where d >= start - 64 (none nearer);  b1 differs somewhere.
# RED CONTROL: --red blendnear (the fog colour weighted by the air's own 1 - T: no start distance) must FAIL.
# USAGE  bash tests/spells/volair1_fogblend.sh [--red blendnear]   under the nifskope lock
set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-volair1_fogblend}"
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
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
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
case "$RED" in ""|blendnear) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/volair1_20261013/fogblend${RED:+_red_$RED}}"
PORT="${PORT:-14934}"
mkdir -p "$OUT"
LOG="$OUT/volair1_fogblend.log"; : > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
say "volair1_fogblend.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED (must fail)}  out: $OUT"
shoot() {	# shoot <tag> [env...]
	local tag="$1"; shift
	rm -f "$OUT/$tag.png" "$OUT/$tag.notes"
	local scope; scope="$(fresh_scope)"
	env WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none \
		WW_CELL_OPEN="$ESM|interior|Vault111Cryo" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" "$@" \
		WW_RENDER_VIEW=4 WW_RENDER_CENTER=-4564,-294,150 WW_RENDER_DIST=500 WW_RENDER_FOV=70 \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE=960x600 WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$scope" \
		timeout 1500 "$EXE" --port "$PORT" "$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "$OUT/$tag.notes" 2>&1
	say "  $tag $([ -s "$OUT/$tag.png" ] && echo shot || echo NO-PICTURE)  $(grep -o 'cell air: volair=.*' "$OUT/$tag.notes" | tail -1 | cut -c1-120)"
}
[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
AIR=( WW_CELL_AIR=1 WW_CELL_AIR_TERMS=2 )
if [ -z "$RED" ]; then
	shoot a "${AIR[@]}"
	shoot b0 "${AIR[@]}" WW_CELL_VOLAIR_FOGBLEND=0
	shoot b1 "${AIR[@]}" WW_CELL_VOLAIR_FOGBLEND=1 WW_CELL_AIR_DUMP="$(winpath "$OUT/b1.bin")"
	[ -n "${GEO:-}" ] && cp "$GEO" "$OUT/geo.png" || shoot geo WW_CELL_AIR=0 WW_CELL_FOG_PROBE=8
else
	G="$REPO/scratchpad/volair1_20261013/fogblend"
	for f in a.png b0.png geo.png; do cp "$G/$f" "$OUT/$f"; done
	shoot b1 "${AIR[@]}" WW_CELL_VOLAIR_FOGBLEND=1 WW_CELL_AIR_RED="$RED" WW_CELL_AIR_DUMP="$(winpath "$OUT/b1.bin")"
fi
VOLAIR1_RED="$RED" python "$REPO/tests/spells/volair1_fogblend_check.py" "$OUT" 2>&1 | tee -a "$LOG"
exit "${PIPESTATUS[0]}"
