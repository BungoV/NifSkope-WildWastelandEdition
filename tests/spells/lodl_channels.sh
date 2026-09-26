#!/bin/bash
#
# WW_LODL_CHANNEL -- every baked channel of the native LOD path is viewable, and
# every picture of one is the channel the SHIPPED file carries.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-18 06:0x: "Beyond the AO, you will also now show me on the same
# chunk: Leaf sway bake, identity bake, sky visibility bake. Anything else I'm
# missing?" -- and, 06:1x: "show me also ground contact blend, ground cover, and
# roughness / metallic or specular / gloss in this case, since these are baked
# from legacy textures and materials, not .pbrm".
#
# A channel view is the easiest thing in the tree to fake. Paint the mesh grey,
# caption it "sky visibility", and nobody can tell from the picture that the
# uniform was never bound: the render is beautiful and completely empty. Root
# MISTAKES 05:1x is exactly that defect, caught once already. So this gate never
# looks at a picture. It asserts:
#
#   (a) every channel NAME renders, and prints a note line whose numbers were
#       READ BACK from what was uploaded -- N placements / vertices / texels > 0,
#       or the word "constant", or the word "ABSENT", each one BY NAME;
#   (b) every channel's render DIFFERS from the default render of the same
#       framing, pixel count > 0. A channel whose two renders are byte-identical
#       is NOT WIRED, and no caption rescues it. The two channels this bake does
#       not carry (`mask-a`, `emissive`) have the INVERTED floor: they MUST be
#       identical AND must say ABSENT by name;
#   (c) the note line's mean equals the independent Python reader's mean within
#       1, for every per-placement, per-vertex and per-texel channel. The reader
#       (`lodl_channels_table.py` over `lodgen_native_decode.py` and
#       `lodgen_vt_check.py`) shares no code with the viewer;
#   (d) `WW_LODL_AO=1` with WW_LODL_CHANNEL unset is BYTE-IDENTICAL to
#       `WW_LODL_CHANNEL=ao`. The AO view bungo already reads must not move
#       because a channel switch was added beside it;
#   (e) an unknown name is REFUSED BY THE NAME GIVEN, lists the known names, and
#       renders byte-identically to the default.
#
# and two floors that must be able to fire: the flat default and the textured
# default are different pictures (so "0 px differ" means identical, not "the
# comparison is broken"), and the 1-unit tolerance in (c) still REFUSES a
# deliberately wrong pairing.
#
# FIXTURES. This renders a real v6 `.lodi`/`.lodo` pair and a real `.lodt` sheet
# container left by the VIEWFIX1 and SLAB1 lanes. They are large and untracked;
# when one is absent the gate SKIPS with the missing path NAMED and does not
# pass. Re-point with LODI=, SHEETS=, LODL=.
#
# ONE NifSkope at a time, second monitor, never a desktop capture: every render
# goes through WW_RENDER_SHOT with WW_WINDOW_AT from _harness.sh.
#
# USAGE
#   bash tests/spells/lodl_channels.sh
#   EXE=<exe> OUT=<dir> PORT=<n> bash tests/spells/lodl_channels.sh

set -u

. "$(dirname "$0")/_harness.sh" 2>/dev/null || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# ---- settings scope (lane FIX1 fix 4, 2026-09-26) ----------------------------
# Every NifSkope WINDOW this spell opens runs in its OWN QSettings scope -- never
# bungo's profile, and never a scope the caller's environment names. A gate that
# inherits the user's settings measures the profile, not the code (lane GATEFIX2,
# native_lighting.sh: his "Vertex Color" unticked in the Lighting shading mode
# turned the .BTR water white). WW_SETTINGS_SCOPE=<scope> moves the whole tree to
# HKCU\Software\NifTools\NifSkope 2.0 <scope> (src/harnesswindow.cpp).
# fresh_scope wipes it before EACH window (a window saves its layout on close, so
# the next would open at another size) and seeds Settings/Version=1: an EMPTY
# scope is a first install, whose settings dialog saves every pane's widget value
# (Background 46,46,46, src/ui/settingspane.cpp). SEED_REG=<file.reg> (keys
# already under the scope) is imported after the seed -- a red control's way to
# render under a chosen profile. The scope is wiped at exit. The -no-gui CLI
# calls are not windows and are left as they were.
SCOPE="${SCOPE:-lodl_channels}"
case "$SCOPE" in
	''|*[!A-Za-z0-9_-]*) echo "REFUSED: SCOPE='$SCOPE' is not a usable settings scope name"; exit 2 ;;
esac
[ ${#SCOPE} -le 40 ] || { echo "REFUSED: SCOPE='$SCOPE' is longer than 40"; exit 2; }
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {  # wipe + seed, then print the name: WW_SETTINGS_SCOPE="$(fresh_scope)"
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	# not a Game Manager first install either: version 0 shows an opaque progress dialog on the
	# PRIMARY monitor (src/gamemanager.cpp prog_dialog) before any WW window placement exists
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	# ...and the game manager state an empty scope never gets (its Game Folders come out empty):
	# Fallout 4's path and folders read from THIS MACHINE, never from the user's profile
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	if [ -n "${SEED_REG:-}" ]; then reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1 || true; fi
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
PORT="${PORT:-42941}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"

V="$ROOT/scratchpad/viewfix_20260917"
LODI="${LODI:-$V/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi}"
SHEETS="${SHEETS:-$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth}"
LODL="${LODL:-$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl}"
LODT="$SHEETS/Commonwealth.VT.1.lodt"
OUT="${OUT:-$ROOT/scratchpad/lodl_channels_gate}"
TABLE="$OUT/table.json"
CHECK="$ROOT/tests/spells/lodl_channels_check.py"
READER="$ROOT/tests/spells/lodl_channels_table.py"

# the chunk, the framing and the region -- one place, so a different fixture
# changes numbers and never arithmetic
REGION="4,-12,7,-9,0"; RX0=4; RY0=-12; RX1=7; RY1=-9
CX=24900; CY=-41300; CZ=450; ORTHO=2600; VIEW=8; SIZE=1400x1091

[ -x "$EXE" ] || { echo "SKIP: no NifSkope.exe at $EXE"; exit 2; }
for f in "$LODI" "$LODT" "$LODL" "$CHECK" "$READER"; do
	[ -f "$f" ] || { echo "SKIP: fixture missing -- $f"; exit 2; }
done

if ! type winpath >/dev/null 2>&1; then
	winpath() {
		case "$1" in
			/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
			*) printf '%s' "$1" ;;
		esac
	}
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.png "$OUT"/*.log "$TABLE"
echo "exe   $EXE  ($(stat -c '%y %s B' "$EXE" 2>/dev/null))"
echo "lodi  $LODI"
echo "sheet $LODT"
echo "out   $OUT"

# ---- the authority: the reader, sharing no code with the viewer --------------
"$PY" "$READER" "$(winpath "$LODI")" "$(winpath "$LODT")" \
	"$RX0" "$RY0" "$RX1" "$RY1" 0 0 2>/dev/null | sed -n '/^{/,$p' > "$TABLE"
[ -s "$TABLE" ] || { echo "SKIP: the reader produced no table"; exit 2; }

# ---- the renders: one NifSkope at a time -------------------------------------
# $1 tag  $2 channel (- for none)  $3 flat  $4 WW_LOD_CHANNEL  $5 WW_LODL_AO
shot() {
	local tag=$1 chan=$2 flat=$3 lodch=$4 ao=$5
	[ "$chan" = "-" ] && chan=""
	WW_LODL_OBJECTS="$(winpath "$LODI")" \
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODL_CHANNEL="$chan" WW_LODL_AO="$ao" WW_LOD_CHANNEL="$lodch" \
	WW_RENDER_FLAT="$flat" WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW="$VIEW" \
	WW_RENDER_CENTER="$CX,$CY,$CZ" WW_RENDER_ORTHO="$ORTHO" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" \
		> "$OUT/$tag.log" 2>&1
	[ -s "$OUT/$tag.png" ] || echo "  (no picture for $tag, exit $?)"
}

shot default    -   1 0 0
for c in identity placement identityraw sky ground seed sway selfao ao \
	mask-r mask-g mask-b mask-a; do
	shot "$c" "$c" 1 0 0
done
# the two textured names: the sheet IS the picture, so texturing on and unlit
# (WW_LOD_CHANNEL=12 is the stock raw-base-colour view)
shot default_tex -        0 12 0
shot normal      normal   0 12 0
shot emissive    emissive 0 12 0
# the refusal, and the way back
shot unknown     nosuchchannel 1 0 0
shot ao_way_back -             1 0 1

# ---- the verdict -------------------------------------------------------------
echo "--- checks ---"
"$PY" "$CHECK" "$(winpath "$OUT")" "$(winpath "$TABLE")"
rc=$?
exit $rc
