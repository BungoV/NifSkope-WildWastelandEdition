#!/bin/bash
#
# THE CELL VIEW'S SCREEN-SPACE GI, first live smoke (lane SSGI1; src/gl/cellssgi.h). Vault 111 cryo walkway, interior,
# probe GI off (WW_CELL_GI=0: the bounce is then the screen's alone), DOF/MBLUR off, effects hidden, decals pinned.
#   base   the perf1 exe (BASE_EXE), no SSGI in it         off   this exe, row off
#   on     WW_RENDER_SSGI=1 (quality ultra)                 on2   the same again (two identical runs)
#   redoff WW_CELL_SSGI_RED=off: computed, not applied (must equal off)
# Checks (cell_ssgi_check.py): off == base byte for byte; on brighter than off near geometry; on == on2; redoff == off;
# the notes carry "cell ssgi: on" and no shader link error.
# USAGE  bash tests/spells/cell_ssgi.sh      (under withlock.sh nifskope)

set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BASE_EXE="${BASE_EXE:-/e/Projects/NifskopeWWE-perf1/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_ssgi}"
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
OUT="${OUT:-$REPO/scratchpad/ssgi1/smoke}"
PORT="${PORT:-14793}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1920x1080}"
CELL="${CELL:-Vault111Cryo}"; CENTER="${CENTER:-350,-512,40}"; DIST="${DIST:-450}"; VIEW="${VIEW:-4}"
mkdir -p "$OUT"

shoot() {	# <tag> <exe> <env...>
	local tag="$1" exe="$2"; shift 2
	if [ -n "${STOP_AT:-}" ] && [ "$(date +%H%M)" -ge "$STOP_AT" ]; then echo "$tag skipped: past STOP_AT $STOP_AT"; return; fi
	env "$@" WW_RENDER_CENTER="$CENTER" WW_RENDER_DIST="$DIST" WW_RENDER_FOV=70 \
		WW_CELL_OPEN="$ESM|interior|$CELL" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
		WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 600 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$OUT/$tag.notes" 2>&1
	echo "$tag rc=$? $(date +%H:%M:%S)"
}
shoot base "$BASE_EXE"
shoot off "$EXE"
shoot on "$EXE" WW_RENDER_SSGI=1 WW_CELL_SSGI_QUALITY=ultra
shoot on2 "$EXE" WW_RENDER_SSGI=1 WW_CELL_SSGI_QUALITY=ultra
shoot redoff "$EXE" WW_RENDER_SSGI=1 WW_CELL_SSGI_QUALITY=ultra WW_CELL_SSGI_RED=off
python "$(dirname "$0")/cell_ssgi_check.py" "$(winpath "$OUT")"
