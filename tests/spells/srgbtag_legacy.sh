#!/bin/bash
# lane SRGBTAG1 (2026-10-03): a Fallout 4 diffuse tagged _SRGB (DXGI 99) must draw like the same blocks
# untagged (DXGI 98) on a LEGACY (BGSM) material, as it does in game (the engine asks for the diffuse slot
# as sRGB either way). Run it through withlock:
#   bash .../withlock.sh nifskope bash tests/spells/srgbtag_legacy.sh [--exe <dir>] [--out <dir>] [--red]
# --red sets WW_SRGBTAG1_RED=1 (the old GL decode). Shots, one NifSkope at a time:
#   g1_u98 / g1_s99  the duct with the fixture diffuse (98 / 99), g1_van  the duct with its vanilla diffuse,
#   g2_suit          the BoS undersuit from the mod folder.
# Measures (python): mean |d| over mesh pixels u98 vs s99 (gate 1), fixture-used check, undersuit mean.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"
ARM="$ROOT/release"; OUT="$ROOT/scratchpad/srgbtag_legacy_$(date +%Y%m%d_%H%M%S)"; RED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--exe) ARM="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		--red) RED=1; shift ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
EXE="${EXE_NAME:-NifSkope.exe}"
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
MOD="/e/Projects/Fallout 4 Mods/mods/BoSInfantryArmor"
FIX="$ROOT/scratchpad/srgbtag1_20261003/fix"
PORT="${SRGBTAG_PORT:-43241}"
SCOPE=srgbtaglegacy
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
[ "$(head -c 2 "$ARM/$EXE")" = "MZ" ] || { echo "REFUSED: $ARM/$EXE is not a finished image"; exit 2; }
[ -s "$FIX/unorm98/Textures/SetDressing/AC ducts01Rubble_d.dds" ] || { echo "REFUSED: run scratchpad/srgbtag1_20261003/fixtures.py first"; exit 2; }
{ echo "exe=$ARM/$EXE red=${RED:-0}"; sha1sum "$ARM/$EXE"; } > "$OUT/arms.txt"

wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {  # never his key: a scope of our own, seeded so the clear colour is a mid grey
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
	local gm; gm="$(mktemp)"
	python "$HERE/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 && reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	reg add "$REGKEY\\Settings\\Theme" //v "Palette Version" //t REG_DWORD //d 3 //f > /dev/null 2>&1
	reg add "$REGKEY\\Settings\\Render\\Colors" //v Background //t REG_SZ //d "#8a8a8a" //f > /dev/null 2>&1
}
trap wipe_scope EXIT
harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}
shot() {  # shot <tag> <nif> <roots, Mod Organizer order: the LAST overrides> [KEY=VALUE ...]
	local tag="$1" nif="$2" roots="$3"; shift 3
	local png="$OUT/$tag.png"; rm -f "$png"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	fresh_scope
	env WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1280x859 WW_RENDER_VIEW=5 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_SS=0 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_LODGEN_RESOURCES="$roots" WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		${RED:+WW_SRGBTAG1_RED=1} "$@" \
		timeout 180 "$ARM/$EXE" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}
DUCT="$DATA/Meshes/SetDressing/ACDucts/ACDuctConnector01.nif"
SUIT="$MOD/meshes/armor/bosinfantryarmor/BoSInfantryUndersuit_M.nif"
shot g1_u98 "$DUCT" "$(winpath "$DATA");$(winpath "$FIX/unorm98")"
shot g1_s99 "$DUCT" "$(winpath "$DATA");$(winpath "$FIX/srgb99")"
shot g1_van "$DUCT" "$(winpath "$DATA")"
shot g2_suit "$SUIT" "$(winpath "$DATA");$(winpath "$MOD")"
# the same undersuit with its _D retagged 98 (blocks untouched): what the UNORM reading draws
[ -s "$FIX/suit98/Textures/armor/bosinfantryarmor/BoSInfantryUndersuit_D.dds" ] &&
	shot g2_suit98 "$SUIT" "$(winpath "$DATA");$(winpath "$MOD");$(winpath "$FIX/suit98")"
python "$HERE/srgbtag_legacy.py" "$OUT" | tee "$OUT/verdict.txt"
