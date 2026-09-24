#!/usr/bin/env bash
# Lane LODIV7 -- the seven pictures, CHANVIEW1 framing, one window at a time.
#
# Every launch here is the gate's own `shot()` shape (tests/spells/lodi_v7.sh):
# the `.lodl` is passed POSITIONALLY because WW_RENDER_SHOT only arms when a file
# is on the command line (src/nifskope_ui.cpp:22056), the sheets and the region
# are set so the picture is made under the conditions the gate specifies, the
# launch is wrapped in `timeout`, and nothing may outlive its own shot.
#
# Run it ONCE, from the repo root, with no NifSkope and no Fallout4 up.
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42953}"
LANE="$ROOT/scratchpad/lodiv7_20260918"
OUT="$LANE/images"
V7I="$LANE/v7/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
V6I="$LANE/g1_new_v6/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi"
V="$ROOT/scratchpad/viewfix_20260917"
LODL="${LODL:-$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl}"
SHEETS="${SHEETS:-$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth}"
REGION="4,-12,7,-9,0"

# CHANVIEW1 -- the framing lodl_channels.sh uses, so these and its pictures are
# the same camera.
CX=24900; CY=-41300; CZ=450; ORTHO=2600; VIEW=8; SIZE=1400x1091

mkdir -p "$OUT"
winpath () { command -v cygpath >/dev/null 2>&1 && cygpath -w "$1" || echo "$1"; }

# SHOT_CX/CY/ORTHO reframe; SHOT_LODI_REGION narrows the OBJECTS to a cell
# rectangle (WW_LODI_REGION, src/lodinative.cpp lodiSpecFromEnv). Empty means
# unset as far as qgetenv is concerned, so the default is the terrain's region.
SHOT_CX=""; SHOT_CY=""; SHOT_ORTHO=""; SHOT_LODI_REGION=""

shot () {  # shot <tag> <channel> <lodi>
	local tag="$1" chan="$2" lodi="$3" rc=0
	echo "-- $tag   channel=$chan"
	WW_LODL_OBJECTS="$(winpath "$lodi")" \
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODI_REGION="$SHOT_LODI_REGION" \
	WW_LODL_CHANNEL="$chan" \
	WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="${SHOT_CX:-$CX},${SHOT_CY:-$CY},$CZ" \
	WW_RENDER_ORTHO="${SHOT_ORTHO:-$ORTHO}" WW_RENDER_VIEW="$VIEW" \
	WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \
	WW_WINDOW_AT=1960,40 \
		timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" > "$OUT/$tag.log" 2>&1
	rc=$?
	if [ -s "$OUT/$tag.png" ]; then
		echo "   ok  $(stat -c%s "$OUT/$tag.png") B"
		grep -o 'WW_LODL_CHANNEL=.*' "$OUT/$tag.log" | head -1 | sed 's/^/   note: /'
	else
		echo "   NO PICTURE for $tag (exit $rc)"
	fi
	if tasklist 2>/dev/null | grep -qi "NifSkope.exe"; then
		echo "   STOP: a NifSkope is still up after '$tag' (port $PORT). Unwedge it"
		echo "         WITHOUT killing it: UTF-16LE UDP 'NifSkope::open <win path>'"
		echo "         to 127.0.0.1:$PORT (src/main.cpp IPCsocket)."
		exit 9
	fi
}

date
shot 1_v7_identity  identity  "$V7I"     # the GROUP -- one colour a house
shot 2_v7_placement placement "$V7I"     # the unique identity, one colour a placement
shot 3_v6_identity  identity  "$V6I"     # no group table: the fallback, and the note says so
shot 4_v7_sky       sky       "$V7I"     # the per-vertex stream
shot 5_v6_sky       sky       "$V6I"     # the flat placement byte, and the note says so
shot 6_v7_ao        ao        "$V7I"     # the control: the channel that did NOT change

# The largest group, framed on its own box. There is no shipped knob that draws
# ONE group on its own, so this narrows the objects to the single CELL it stands
# in and the caption admits what else is in frame.
SHOT_LODI_REGION="5,-11,5,-11"; SHOT_CX=21891; SHOT_CY=-42868; SHOT_ORTHO=1750
shot 7_v7_group_largest identity "$V7I"
SHOT_LODI_REGION=""; SHOT_CX=""; SHOT_CY=""; SHOT_ORTHO=""

date
echo "pictures: $(ls -1 "$OUT"/*.png 2>/dev/null | wc -l) png in $OUT"
