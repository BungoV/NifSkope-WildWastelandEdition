#!/bin/bash
#
# THE BAKE RELIT BY THE CELL'S LIGHTS (lane PRTPGI, 2026-10-01; src/probegi.h).
#
# Not judged by eye. Per cell:
# lane BAKEBLOCK1 (block rule): Concord is asked n=1; its bake loads the 5x5 + far LOD by itself (the exe
#   promotes every exterior bake and every relight of one), so every Concord run here is the ruled block.
#
#   bake     one window places + bakes the probes, relights the bake (WW_CELL_GI_DUMP writes every
#            stage's numbers) and shoots the picture with Cell lights + GI on (lit.png)
#   nogi     the same view, GI off (the picture without the bounce)
#   probe N  the bake relit again (WW_CELL_GI_FROM, no new bake): probes 2-4 the position and normal,
#            5 the bounce's E / pi at every cell-lit fragment (interiors)
# Then tests/spells/cell_gi_check.py rebuilds each stage from the .tbk files, the soup and ITS OWN walk
# of Fallout4.esm: A the surfels' light (shadowed), B the probes' gather, C the voxel grid (visibility),
# D the picture against its own trilinear sample of the grid, E the same in PBR mode (pbrm_cell.prog).
#   pbr/probe N  probes 2-5 again with WW_PBRM_MODE=pbr (the PBR program's own copy of the cell code)
#
# lane BOUNCE2 (2026-10-03): the relight repeats until it settles (more than one bounce). Also per cell:
#   one      the bake relit with WW_CELL_GI_PASSES=1 (dump_one/): byte-identical to the exe from before the
#            lane (BEFORE, before.png + dump_before/), and checked again (B C F at one pass). Lane ROOMCLAMP1:
#            pinned with WW_CELL_ROOMCLAMP_PIN=off (the rooms clamp off; a gate key, never a toggle)
#   pairs    pairs/<name>/: the Pass view's GI (pass_one|pass_set) and the Combined picture (comb_one|comb_set),
#            one pass against settled, at the cell's camera and at PAIRS_<cell> (name=x,y,z/view/dist)
#   stage F  the passes repeated by the checker's own twin; stage P the pairs (settled never darker)
# Places: Vault111Cryo (its door room below: 74% of its surfels unlit, 85% of their light the bounce),
# DmndSolomonsHouse01, and concord (Commonwealth -15,17 in cell_sky's Lookdev, SKY1's street camera).
#
# RED CONTROLS (each must FAIL its stage):  --red noshadow  A  the lights reach every surfel
#                                           --red flip      B  links gathered from the opposite side
#                                           --red novis     C  the grid blends probes behind walls
#                                           --red off       D+E  GI off: no bounce in either picture
#                                           --red rooms     F  the feedback ignores rooms and walls
#                                           --red grow      F  the feedback's albedo 1.5 (no settling)
#                                           --red onepass   F  the checker's twin stops after one pass
# (noshadow, flip, novis and off are the interiors'; concord skips them.)
#
# USAGE  bash tests/spells/cell_gi.sh [--red noshadow|flip|novis|off|rooms|grow|onepass]
#        CELLS="..." to pick cells; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_bounce2.exe}"
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
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 concord}"
: "${CAM_Vault111Cryo:=-4600,-280,0}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}"
# lane BOUNCE2: the exterior at SKY1's street camera (eye height; the street is near z 6200)
: "${CAM_concord:=-60200,73500,6300}" "${VIEW_concord:=4}" "${DIST_concord:=600}"
# the room lit through its door (room box -596443128, x -3660..-2994, floor z -9), eye 120 above the floor at
# -3440,-417 looking +x; picked by a soup ray test (floor under the eye, 450 clear ahead)
: "${PAIRS_Vault111Cryo:=door=-2990,-417,111/4/450}"
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" )
# lane SKYFULL1: the preview rows ship ON; all four stay pinned OFF here so the probe numbers stay byte-stable
is_ext() { [ "$1" = concord ]; }

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"; LOG="$OUT/cell_gi.log"   # lane BOUNCE2: the exe and the checker need it absolute
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

shoot() {   # [XE=<exe>] [XCAM=x,y,z/view/dist] shoot <run dir> <cell> <tag> <env...>
	local run="$1" cell="$2" tag="$3"; shift 3
	local shot="$run/$tag.png" notes="$run/$tag.notes" exe="${XE:-$EXE}" c="${XCAM:-}"
	mkdir -p "$(dirname "$shot")"
	rm -f "$shot" "$notes"
	local cv="CAM_$cell" vv="VIEW_$cell" dv="DIST_$cell" cam=() open=()
	[ -z "$c" ] && [ -n "${!cv:-}" ] && c="${!cv}/${!vv:-${VIEW:-1}}/${!dv:-${DIST:-1400}}"
	if [ -n "$c" ]; then
		local rest="${c#*/}"
		cam=( WW_RENDER_CENTER="${c%%/*}" WW_RENDER_VIEW="${rest%%/*}" WW_RENDER_DIST="${rest#*/}" WW_RENDER_FOV=70 )
	else
		cam=( WW_RENDER_VIEW="${VIEW:-1}" )
	fi
	if is_ext "$cell"; then
		open=( WW_CELL_IS=0 "${LD[@]}" WW_CELL_OPEN="$ESM|Commonwealth|-15,17|1" )
	else
		open=( WW_CELL_OPEN="$ESM|interior|$cell" )
	fi
	# lane CAPTURE1 drops refraction-only shapes from the soup (cell_albedo_check.py refract gates that); kept
	# here so the one-pass run stays byte for byte the pre-BOUNCE2 exe (which ignores the key)
	env WW_CELL_LIT=1 WW_CELL_BAKE_REFRACT_RED=keep "${open[@]}" "$@" "${cam[@]}" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for cell in $CELLS; do
	say "== $cell"
	case "$RED" in noshadow|flip|novis|off) is_ext "$cell" && { say "  skip  $cell: the $RED red is an interior's"; continue; } ;; esac
	run="$OUT/$cell"
	rm -rf "$run/bake" "$run/dump" "$run/pbr" "$run/dump_one" "$run/dump_before" "$run/pairs"
	mkdir -p "$run"
	gi=1; [ "$RED" = "off" ] && gi=0
	redenv=()
	case "$RED" in ""|off|onepass) ;; *) redenv=( WW_CELL_GI_RED="$RED" ) ;; esac
	ok=1
	[ "$(shoot "$run" "$cell" lit WW_CELL_GI=$gi "${redenv[@]}" \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_CELL_GI_DUMP="$(winpath "$run/dump")")" = 1 ] || ok=0
	grep -h "  gi" "$run/lit.notes" | head -3 | cut -c1-400 | tee -a "$LOG"
	from=( WW_CELL_GI_FROM="$(winpath "$run/bake")" )
	if [ -z "$RED" ] || [ "$RED" = "off" ]; then
		[ "$(shoot "$run" "$cell" nogi WW_CELL_GI=0 "${from[@]}")" = 1 ] || ok=0
		# lane SKYFULL1: an exterior's sheet = the lit picture under the whole sky (not measured; the dome checked)
		if is_ext "$cell" && [ -z "$RED" ]; then
			[ "$(shoot "$run" "$cell" sheet_lit WW_CELL_GI=$gi "${from[@]}" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_MOON=1 WW_LODGEN_RESOURCES="$DATA")" = 1 ] 				&& grep -q "lookdev sky: sky:dome(" "$run/sheet_lit.notes" || { say "  the sheet did not draw the dome"; ok=0; }
		fi
		if ! is_ext "$cell"; then
			for p in 2 3 4 5; do
				[ "$(shoot "$run" "$cell" probe$p WW_CELL_GI=$gi WW_CELL_LIT_PROBE=$p "${from[@]}")" = 1 ] || ok=0
			done
			mkdir -p "$run/pbr"
			for p in 2 3 4 5; do
				[ "$(shoot "$run" "$cell" pbr/probe$p WW_CELL_GI=$gi WW_CELL_LIT_PROBE=$p "${from[@]}" \
					WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_PROGRAM_CENSUS="$(winpath "$run/pbr/probe$p.prog.txt")")" = 1 ] || ok=0
			done
		fi
	fi
	if [ -z "$RED" ]; then
		# lane BOUNCE2: the one-pass pin against the exe from before the lane, and the 1-pass / settled pairs
		# lane WATER1: Vault111Cryo holds placed water (IntPondDarkWaterCalm) that the water shader now draws;
		# the rung is about the bounce, so the one shot pins the water draw off (WW_CELL_WATER=0)
		[ "$(shoot "$run" "$cell" one WW_CELL_GI=1 WW_CELL_GI_PASSES=1 "${from[@]}" WW_CELL_WATER=0 WW_CELL_ROOMCLAMP_PIN=off WW_CELL_GI_FILL=0 \
			WW_CELL_GI_DUMP="$(winpath "$run/dump_one")")" = 1 ] || ok=0
		if is_ext "$cell"; then
			# lane BAKEBLOCK1: the relight now loads the 5x5 + far LOD the bake traced; the exe before BOUNCE2
			# relights one cell, so this rung holds on the interiors only (the one-pass rebuild above still runs)
			say "  skip  $cell: the one-pass rung against $BEFORE (it cannot load the 5x5 + far LOD)"
		elif [ -x "$BEFORE" ]; then
			[ "$(XE="$BEFORE" shoot "$run" "$cell" before WW_CELL_GI=1 "${from[@]}" \
				WW_CELL_GI_DUMP="$(winpath "$run/dump_before")")" = 1 ] || ok=0
			same=1; n=0
			for f in "$run"/dump_before/*.bin; do   # gi_meta.txt carries timings
				n=$((n+1))
				cmp -s "$f" "$run/dump_one/$(basename "$f")" || { say "  dump_one/$(basename "$f") differs"; same=0; }
			done
			check "$cell: one pass is the exe from before the lane, byte for byte ($n dump files)" \
				"$([ "$n" -gt 0 ] && echo "$same" || echo 0)"
			# the picture: the GPU's own run-to-run noise is 1 level on a few pixels (the same exe twice
			# differs so, scratch bounce2); allow at most 1 level on 100 pixels and print the count
			pd="$(python -c "import sys,numpy as np;from PIL import Image
a,b=(np.asarray(Image.open(f).convert('RGB'),int) for f in sys.argv[1:3]);d=np.abs(a-b).max(2)
print(int((d>0).sum()),int(d.max()))" "$(winpath "$run/one.png")" "$(winpath "$run/before.png")" 2>/dev/null)"
			say "  one.png against before.png: ${pd:-unreadable} (pixels that differ, largest level)"
			check "$cell: the one-pass picture is the exe from before the lane (noise only)" \
				"$([ -n "$pd" ] && [ "${pd% *}" -le 100 ] && [ "${pd#* }" -le 1 ] && echo 1 || echo 0)"
		else
			check "$cell: the exe from before the lane at $BEFORE" 0
		fi
		for pr in "main=" $(v="PAIRS_$cell"; echo "${!v:-}"); do
			name="${pr%%=*}"; pc="${pr#*=}"; d="pairs/$name"
			[ "$(XCAM="$pc" shoot "$run" "$cell" $d/pass_one WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=1 WW_CELL_GI_PASSES=1 "${from[@]}")" = 1 ] || ok=0
			[ "$(XCAM="$pc" shoot "$run" "$cell" $d/pass_set WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=1 "${from[@]}")" = 1 ] || ok=0
			[ "$(XCAM="$pc" shoot "$run" "$cell" $d/comb_one WW_CELL_GI=1 WW_CELL_PASS=0 WW_CELL_GI_PASSES=1 "${from[@]}")" = 1 ] || ok=0
			[ "$(XCAM="$pc" shoot "$run" "$cell" $d/comb_set WW_CELL_GI=1 WW_CELL_PASS=0 "${from[@]}")" = 1 ] || ok=0
		done
	fi
	check "$cell: the pictures written" "$ok"
	stg=ABCDEFP; is_ext "$cell" && stg=BCFP   # an exterior's light is cell_sky's to check
	ckenv=(); [ "$RED" = onepass ] && ckenv=( GI_CHECK_RED=onepass )
	env "${ckenv[@]}" python "$(dirname "$0")/cell_gi_check.py" "$ESM" "$cell" "$run" "$stg" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	want=""
	case "$RED" in noshadow) want=A ;; flip) want=B ;; novis) want=C ;; off) want=D ;; rooms|grow|onepass) want=F ;; esac
	if [ "$want" = D ] && grep -q "^D .*(dim cell)" "$run/check.txt"; then
		say "  skip  $cell: no bounce in frame, the off red has nothing to remove"
	elif [ -n "$want" ]; then
		check "$cell: the red control FAILS stage $want" "$(grep -q "^$want FAIL" "$run/check.txt" && echo 1 || echo 0)"
		[ "$want" = D ] && check "$cell: the red control FAILS stage E (PBR) too" \
			"$(grep -q "^E FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: every stage matches the independent rebuild" "$(grep -q "^gi PASS" "$run/check.txt" && echo 1 || echo 0)"
		# lane BOUNCE2: the one-pass pin's own dump, every stage that reads it, the twin pinned to one pass
		WW_CELL_GI_PASSES=1 GI_CHECK_DUMP=dump_one python "$(dirname "$0")/cell_gi_check.py" "$ESM" "$cell" "$run" BCF \
			> "$run/check_one.txt" 2>&1
		sed 's/^/  one: /' "$run/check_one.txt" | tee -a "$LOG"
		check "$cell: one pass matches the independent rebuild (pinned)" "$(grep -q "^gi PASS" "$run/check_one.txt" && echo 1 || echo 0)"
		grep -q "^F PASS" "$run/check.txt" && grep -q "^P PASS" "$run/check.txt"
		check "$cell: the passes settle (F) and the settled pairs never darken (P)" "$([ $? = 0 ] && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
