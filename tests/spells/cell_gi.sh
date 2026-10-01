#!/bin/bash
#
# THE BAKE RELIT BY THE CELL'S LIGHTS (lane PRTPGI, 2026-10-01; src/probegi.h).
#
# Not judged by eye. Per interior:
#   bake     one window places + bakes the probes, relights the bake (WW_CELL_GI_DUMP writes every
#            stage's numbers) and shoots the picture with Cell lights + GI on (lit.png)
#   nogi     the same view, GI off (the picture without the bounce)
#   probe N  the bake relit again (WW_CELL_GI_FROM, no new bake): probes 2-4 the position and normal,
#            5 the bounce's E / pi at every cell-lit fragment
# Then tests/spells/cell_gi_check.py rebuilds each stage from the .tbk files, the soup and ITS OWN walk
# of Fallout4.esm: A the surfels' light (shadowed), B the probes' gather, C the voxel grid (visibility),
# D the picture against its own trilinear sample of the grid, E the same in PBR mode (pbrm_cell.prog).
#   pbr/probe N  probes 2-5 again with WW_PBRM_MODE=pbr (the PBR program's own copy of the cell code)
#
# RED CONTROLS (each must FAIL its stage):  --red noshadow  A  the lights reach every surfel
#                                           --red flip      B  links gathered from the opposite side
#                                           --red novis     C  the grid blends probes behind walls
#                                           --red off       D+E  GI off: no bounce in either picture
#
# USAGE  bash tests/spells/cell_gi.sh [--red noshadow|flip|novis|off]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_gi}"
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
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/prtpgi_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_gi.log"
PORT="${PORT:-14743}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01}"
: "${CAM_Vault111Cryo:=-4600,-280,0}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_gi.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/probegi.cpp src/gl/celllights.cpp src/cellview.cpp src/gl/renderer.cpp res/shaders/cell_lights.glsl \
	res/shaders/pbrm_default.frag res/shaders/pbrm_cell.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

shoot() {   # shoot <run dir> <cell> <tag> <env...>
	local run="$1" cell="$2" tag="$3"; shift 3
	local shot="$run/$tag.png" notes="$run/$tag.notes"
	rm -f "$shot" "$notes"
	local camvar="CAM_$cell" cam=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${DIST:-1400}" WW_RENDER_FOV=70 )
	env "$@" "${cam[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-1}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for cell in $CELLS; do
	say "== $cell"
	run="$OUT/$cell"
	rm -rf "$run/bake" "$run/dump" "$run/pbr"
	mkdir -p "$run"
	gi=1; [ "$RED" = "off" ] && gi=0
	redenv=(); [ -n "$RED" ] && [ "$RED" != "off" ] && redenv=( WW_CELL_GI_RED="$RED" )
	ok=1
	[ "$(shoot "$run" "$cell" lit WW_CELL_GI=$gi "${redenv[@]}" \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_CELL_GI_DUMP="$(winpath "$run/dump")")" = 1 ] || ok=0
	grep -h "  gi" "$run/lit.notes" | head -1 | tee -a "$LOG"
	from=( WW_CELL_GI_FROM="$(winpath "$run/bake")" )
	if [ -z "$RED" ] || [ "$RED" = "off" ]; then
		[ "$(shoot "$run" "$cell" nogi WW_CELL_GI=0 "${from[@]}")" = 1 ] || ok=0
		for p in 2 3 4 5; do
			[ "$(shoot "$run" "$cell" probe$p WW_CELL_GI=$gi WW_CELL_LIT_PROBE=$p "${from[@]}")" = 1 ] || ok=0
		done
		mkdir -p "$run/pbr"
		for p in 2 3 4 5; do
			[ "$(shoot "$run" "$cell" pbr/probe$p WW_CELL_GI=$gi WW_CELL_LIT_PROBE=$p "${from[@]}" \
				WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_PROGRAM_CENSUS="$(winpath "$run/pbr/probe$p.prog.txt")")" = 1 ] || ok=0
		done
	fi
	check "$cell: the pictures written" "$ok"
	python "$(dirname "$0")/cell_gi_check.py" "$ESM" "$cell" "$run" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	want=""
	case "$RED" in noshadow) want=A ;; flip) want=B ;; novis) want=C ;; off) want=D ;; esac
	if [ "$want" = D ] && grep -q "^D .*(dim cell)" "$run/check.txt"; then
		say "  skip  $cell: no bounce in frame, the off red has nothing to remove"
	elif [ -n "$want" ]; then
		check "$cell: the red control FAILS stage $want" "$(grep -q "^$want FAIL" "$run/check.txt" && echo 1 || echo 0)"
		[ "$want" = D ] && check "$cell: the red control FAILS stage E (PBR) too" \
			"$(grep -q "^E FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: every stage matches the independent rebuild" "$(grep -q "^gi PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
