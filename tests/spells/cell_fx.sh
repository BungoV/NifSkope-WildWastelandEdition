#!/bin/bash
#
# THE CELL-LIT EFFECTS (lane EFX2, 2026-10-01; src/gl/celllights.h wwCellFxRed, docs/PRTP_PLAN.md 2q).
#
# Not judged by eye. Per camera, four windows shoot the same frame with Cell lights on and the imagespace off
# (its exposure would move every pixel with the effects): the effects hidden (WW_CELL_FX_RED=hide, the
# reference), the run under test, the viewer's effect shader (legacy) and the Soft fades held at 1 (nosoft).
# tests/spells/cell_fx_check.py judges them:
#   N  nothing but the effects moves
#   S  the Soft fades only take away, and take away somewhere
#   H  the haze drops: the effects' mean change over the frame <= the camera's limit x the viewer shader's
#      (measured 2026-10-01: 0.39 on the walkway, 0.86 at the far end; limits 0.6 and 0.95)
#
#
# THE LIT EFFECTS (lane FXLIT1, 2026-10-02; src/gl/cellfxlit.h, docs/PRTP_PLAN.md): an effect shape whose material
# sets the effect-lighting flag is lit by its placed model's (up to) four lights. Per LSHOTS camera the viewer
# writes, for those shapes alone, the model (probe 70), the world position (71 / 72) and the multiplier (73 / 74);
# tests/spells/cell_fxlit_check.py computes the multiplier itself from the plugin and the models on disk:
#   L  the share of pixels that agree >= 95%, and the viewer's total over the expected total within 5%
#   U  outside the lit effects, every pixel equals the exe before the lane (BEFORE=<exe>; SKIP without one):
#      the shapes without the flag keep their path
#
# RED CONTROLS:  --red legacy   the viewer's effect shader (must FAIL stage H)
#                --red nosoft   the fades held at 1 (must FAIL stage S)
#                --red white    the lit effects left unlit, as before the lane (must FAIL stage L)
#                --red nofade   the lights without the placement's fade offset (must FAIL stage L)
#                --red all      every light of the cell instead of the model's four (must FAIL stage L)
#                --red nopower  the falloff without its 2.2 (must FAIL stage L)
#
# USAGE  bash tests/spells/cell_fx.sh [--red legacy|nosoft|white|nofade|all|nopower]
#        SHOTS="cell:x,y,z:view:dist:hmax ..." to pick cameras; RECHECK=1 judges the shots already on disk.
#        LSHOTS="cell:x,y,z:view:dist ..." the lit-effect cameras (x,y,z = the look-at, the eye is dist behind it);
#        PICK=rule|strong the light pick to run and expect (default: the viewer's own, which must be $LPICK).
#        A stage-L red reuses the green run's probes 70..72 (LBASE=<green OUT>) and shoots 73 / 74 alone.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_fx}"
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
OUT="${OUT:-$REPO/scratchpad/efx2_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_fx.log"
PORT="${PORT:-14746}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# the Vault's cryo walkway (the ground steam, bungo's side-by-side) and its far end
SHOTS="${SHOTS:-Vault111Cryo:300,-512,40:3:500:0.6 Vault111Cryo:-4600,-280,120:5:350:0.95}"
# lane FXLIT1: the cryo walkway from bungo's eye (644,-480,60) to the far door
LSHOTS="${LSHOTS:-Vault111Cryo:384,-480,60:3:260}"
LPICK="strong"		# the viewer's default pick (src/gl/cellfxlit.cpp)
LBASE="${LBASE:-$REPO/scratchpad/efx2_20261001/gate}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_fxlit1.exe}"
case "$RED" in white|nofade|all|nopower) LRED="$RED" ;; *) LRED="" ;; esac

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_fx.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/gl/celllights.h src/gl/renderer.cpp src/gl/glscene.cpp src/gl/glproperty.cpp \
	src/gl/cellfxlit.cpp src/gl/cellfxlit.h src/gl/bsshape.cpp src/cellview.cpp src/lodgen.cpp \
	res/shaders/cell_lights.glsl res/shaders/fo4_effectshader.frag res/shaders/fo4_effectcell.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
copies=1
for s in cell_lights.glsl fo4_effectshader.frag fo4_effectcell.frag; do
	cmp -s "$REPO/res/shaders/$s" "$(dirname "$EXE")/shaders/$s" || { say "  shaders/$s beside the exe is not res/shaders/$s"; copies=0; }
done
check "the shaders beside the exe are the tree's" "$copies"

# one window: shoot <run dir> <tag> <exe> [ENV=VAL ...] at the camera in $cell $at $view $dist
shoot() {
	local run="$1" tag="$2" exe="$3"; shift 3
	[ "${RECHECK:-0}" = "1" ] && return
	rm -f "$run/$tag.png"
	env "$@" WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
		WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
}

i=0
[ -n "$LRED" ] && SHOTS=""
for shot in $SHOTS; do
	IFS=: read -r cell at view dist hmax <<< "$shot"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist)"
	mkdir -p "$run"
	for tag in hide on legacy nosoft; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		red=""
		case "$tag" in hide|legacy|nosoft) red="$tag" ;; on) red="$RED" ;; esac
		env ${red:+WW_CELL_FX_RED=$red} WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the four shots written" \
		"$([ -s "$run/hide.png" ] && [ -s "$run/on.png" ] && [ -s "$run/legacy.png" ] && [ -s "$run/nosoft.png" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_fx_check.py" "$run" "${hmax:-0.8}" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	case "$RED" in
		legacy) check "$cell $i: the red control FAILS stage H" "$(grep -q "^H FAIL" "$run/check.txt" && echo 1 || echo 0)" ;;
		nosoft) check "$cell $i: the red control FAILS stage S" "$(grep -q "^S FAIL" "$run/check.txt" && echo 1 || echo 0)" ;;
		"") check "$cell $i: the effects pass N, S and H" "$(grep -q "^effects PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
		*) say "  unknown red control $RED"; fails=$((fails+1)) ;;
	esac
done

# ---- stages L and U (lane FXLIT1): the lit effects against the plugin, and the unlit ones against the exe before
i=0
case "$RED" in legacy|nosoft) LSHOTS="" ;; esac
for shot in $LSHOTS; do
	IFS=: read -r cell at view dist <<< "$shot"
	i=$((i+1))
	run="$OUT/L$i-$cell"
	base="$LBASE/L$i-$cell"
	say "== L$i $cell lit effects, look-at $at (view $view, distance $dist)${PICK:+, pick $PICK}"
	mkdir -p "$run"
	probes="70 71 72 73 74"
	if [ -n "$LRED" ] && [ -s "$base/p70.png" ] && [ -s "$base/p72.png" ] && [ -s "$base/dump.txt" ] && [ "${RECHECK:-0}" != "1" ]; then
		cp "$base/p70.png" "$base/p71.png" "$base/p72.png" "$base/p70.notes" "$base/dump.txt" "$run/"
		probes="73 74"
	fi
	for p in $probes; do
		dump=""
		[ "$p" = 70 ] && dump="WW_CELL_FXLIT_DUMP=$(winpath "$run/dump.txt")"
		shoot "$run" "p$p" "$EXE" WW_CELL_LIT_PROBE="$p" ${LRED:+WW_CELL_FXLIT_RED=$LRED} ${PICK:+WW_CELL_FXLIT_PICK=$PICK} ${dump:+"$dump"}
	done
	check "$cell L$i: the five probes written" \
		"$([ -s "$run/p70.png" ] && [ -s "$run/p71.png" ] && [ -s "$run/p72.png" ] && [ -s "$run/p73.png" ] && [ -s "$run/p74.png" ] && [ -s "$run/dump.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_fxlit_check.py" "$ESM" "$DATA" "$cell" "$run" "${PICK:-$LPICK}" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	if [ -n "$LRED" ]; then
		check "$cell L$i: the red control FAILS stage L" "$(grep -q "^L FAIL" "$run/check.txt" && echo 1 || echo 0)"
		continue
	fi
	check "$cell L$i: the lit effects agree with the plugin" "$(grep -q "^L PASS" "$run/check.txt" && echo 1 || echo 0)"
	[ -n "$RED" ] && continue
	if [ ! -x "$BEFORE" ]; then
		say "  U SKIP no exe from before the lane at $BEFORE (BEFORE=<exe>)"
		continue
	fi
	shoot "$run" before "$BEFORE"
	shoot "$run" on "$EXE" ${PICK:+WW_CELL_FXLIT_PICK=$PICK}
	shoot "$run" hide "$EXE" WW_CELL_FX_RED=hide
	python "$(dirname "$0")/cell_fxlit_check.py" --unlit "$run" >"$run/unlit.txt" 2>&1
	sed 's/^/  /' "$run/unlit.txt" | tee -a "$LOG"
	check "$cell L$i: outside the lit effects nothing moved" "$(grep -q "^U PASS" "$run/unlit.txt" && echo 1 || echo 0)"
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
