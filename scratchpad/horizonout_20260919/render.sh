#!/usr/bin/env bash
# HORIZONOUT step 6 -- the pictures the ruling is entitled to.
#
# The baked horizon is gone and the far shadow keys on the `.lodi` GROUP, so the
# pictures that matter are (1) what the group looks like, (2) what the one bit
# that outlived the route looks like, and (3) what the ruled proximity join did
# to the group, LEGACY beside PROXIMITY at the same camera.
#
# Adapted from scratchpad/chanview1_20260918/render.sh and
# scratchpad/lodiv7_20260918/pictures.sh: the render HOOK, never a desktop
# capture, one NifSkope at a time, the window on the second monitor.
#
# USAGE  bash scratchpad/horizonout_20260919/render.sh [tag ...]

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42977}"
LANE="$ROOT/scratchpad/horizonout_20260919"
OUT="${OUT:-$LANE/images}"

PROX="$LANE/join/prox/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
LEGACY="$LANE/join/legacy/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
SCRAP="$LANE/scrap/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
LODL="$ROOT/scratchpad/viewfix_20260917/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl"
SHEETS="$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth"

# the region both halves share: chunk 4.4.-12 is cells [4,-12]..[7,-9]
REGION="4,-12,7,-9,0"
SIZE=1400x1091

winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

[ -x "$EXE" ] || { echo "SKIP: no NifSkope.exe at $EXE"; exit 2; }
for f in "$PROX" "$LEGACY" "$SCRAP" "$LODL"; do
	[ -f "$f" ] || { echo "SKIP: fixture missing -- $f"; exit 2; }
done
if tasklist 2>/dev/null | grep -qi -E "Fallout4|NifSkope"; then
	echo "SKIP: Fallout4 or a NifSkope is up -- one instance at a time"; exit 2
fi
mkdir -p "$OUT"

# $1 tag  $2 lodi  $3 channel  $4 cx  $5 cy  $6 cz  $7 ortho  $8 view
shot() {
	local tag=$1 lodi=$2 chan=$3 cx=$4 cy=$5 cz=$6 ortho=$7 view=$8
	WW_LODL_OBJECTS="$(winpath "$lodi")" \
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODL_CHANNEL="$chan" \
	WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$cx,$cy,$cz" \
	WW_RENDER_ORTHO="$ortho" WW_RENDER_VIEW="$view" \
	WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 WW_WINDOW_AT=1960,40 \
		timeout 900 "$EXE" --port "$PORT" "$(winpath "$LODL")" \
		> "$OUT/$tag.log" 2>&1
	local rc=$?
	printf '%-26s %7s B  rc %d\n' "$tag" \
		"$(stat -c '%s' "$OUT/$tag.png" 2>/dev/null || echo 0)" "$rc"
	grep -ho 'WW_LODL_CHANNEL=.*' "$OUT/$tag.log" | head -2 | cut -c1-150 | sed 's/^/   note: /'
	grep -ho '^[0-9]* placements read.*' "$OUT/$tag.log" | head -1 | cut -c1-150 | sed 's/^/   read: /'
}

# The OBJECTS ALONE, no terrain: the `.lodi` is opened as the document, which is
# the only way to frame a cell the `.lodl` fixture does not cover.
# $1 tag  $2 lodi  $3 channel  $4 cellx  $5 celly  $6 cx  $7 cy  $8 cz  $9 ortho  $10 view
shotlodi() {
	local tag=$1 lodi=$2 chan=$3 ex=$4 ey=$5 cx=$6 cy=$7 cz=$8 ortho=$9 view=${10}
	WW_LODI_REGION="$ex,$ey,$ex,$ey" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODL_CHANNEL="$chan" \
	WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$cx,$cy,$cz" \
	WW_RENDER_ORTHO="$ortho" WW_RENDER_VIEW="$view" \
	WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 WW_WINDOW_AT=1960,40 \
		timeout 900 "$EXE" --port "$PORT" "$(winpath "$lodi")" \
		> "$OUT/$tag.log" 2>&1
	local rc=$?
	printf '%-26s %7s B  rc %d\n' "$tag" \
		"$(stat -c '%s' "$OUT/$tag.png" 2>/dev/null || echo 0)" "$rc"
	grep -ho 'WW_LODL_CHANNEL=.*' "$OUT/$tag.log" | head -2 | cut -c1-200 | sed 's/^/   note: /'
	grep -ho '^[0-9]* placements read.*' "$OUT/$tag.log" | head -1 | cut -c1-150 | sed 's/^/   read: /'
}

run () {   # run <tag> ... -- with no arguments every picture is taken
	local tag=$1; shift
	if [ ${#WANT[@]} -eq 0 ]; then shot "$tag" "$@"; return; fi
	for w in "${WANT[@]}"; do [ "$w" = "$tag" ] && shot "$tag" "$@"; done
	return 0
}
runl () {  # the same, on the objects-alone route
	local tag=$1; shift
	if [ ${#WANT[@]} -eq 0 ]; then shotlodi "$tag" "$@"; return; fi
	for w in "${WANT[@]}"; do [ "$w" = "$tag" ] && shotlodi "$tag" "$@"; done
	return 0
}
WANT=("$@")

echo "exe $EXE"
echo "out $OUT"

# 1-2  THE GROUP, which is what the far shadow is keyed on now.
#      close = the oblique street view CHANVIEW1 and LODIV7 both used, so this
#      lane's picture can be laid beside theirs; full = the whole chunk from above.
run 1_identity_close      "$PROX"   identity   24900 -41300  450 2600 8
run 2_identity_full       "$PROX"   identity   24576 -40960    0 8192 1

# 3-4  THE ONE BIT THAT OUTLIVED THE ROUTE. Magenta = the player can scrap it.
run 3_scrappable_close    "$SCRAP"  scrappable 24900 -41300  450 2600 8
run 4_scrappable_full     "$SCRAP"  scrappable 24576 -40960    0 8192 1

# 5    The east edge of the chunk, where DN135_GwinnettExt stands.
run 5_identity_east       "$PROX"   identity   31000 -42449  900 4500 8

# 6-7  THE JOIN, the same camera twice: 205 groups under the legacy rule,
#      6 under the ruled proximity one.
run 6_gwinnett_legacy     "$LEGACY" identity   31780 -42449  933 3400 8
run 7_gwinnett_proximity  "$PROX"   identity   31780 -42449  933 3400 8

# 8    A cell where the bit is actually SET. Chunk 4.4.-12 carries none of the
#      fourteen -- `scrappable` there prints `constant 0` and draws all grey,
#      which is the honest picture of that chunk and not a picture of the bit.
#      The four TreeCluster03 the workshop at 001B31E6 can scrap stand in cell
#      10,-1, which the `.lodl` fixture does not cover, so this one is the
#      OBJECTS ALONE.
runl 8_scrappable_yes     "$SCRAP"  scrappable 10 -1  44300 -3800 400 900 8

echo "done"
