#!/bin/bash
#
# VOLUMETRIC PARTICLES IN THE CELL VIEW (lane VOLPART1, 2026-10-14; src/gl/cellvolair.h, res/shaders/particles.frag).
#
# Not judged by eye. Goodneighbor's street steam (the PCloud systems), his profile stack (WW_ALL_ON=1, the neon veil
# GI bake, the air on, DOF / motion blur / far LOD / glare off), 1920 x 1040, a still 19-frame path, frame 17 kept.
# Cameras (both IN-BOUNDS, notes/neongi1/tools/camcheck.py):
#   rev1    23300 -9700 745 -> 22300 -10150 800 fov 55   CommonwealthFoggy 18:00 (his report's frame)
#   memden  22600 -10050 740 -> 23600 -9500 760 fov 55   CommonwealthClear 22:00
# Per camera:
#   ana      WW_CELL_VOLPART=1, the air's terms 3 (the directional light + the placed lights), no shadow of any kind
#            (WW_CELL_SHADOW=0, WW_LOOKDEV_SHADOWS=0): every probe rebuilt in numpy  + the particle and air dumps
#   full     WW_CELL_VOLPART=1, every term, the shadows on (the shipped look on): the "after" picture + dumps
#   nosoftp  full + WW_CELL_VOLPART_RED=nosoft (the plate the hard-edge stage compares)
#   hide     WW_CELL_VOLPART_HIDE=1: no volumetric particle or effect card drawn (the plate the steam is measured against)
#   off      the row off (WW_CELL_VOLPART=0)        before / rung2   the exe from before the lane, twice
# Then tests/spells/volpart1_check.py (its header says what each stage is).
#
# RED CONTROLS (rev1 only, where every stage's population is met; each must FAIL its stage):
#   --red nolights  the placed lights put nothing on a puff        A L
#   --red white     every light grey                                 T
#   --red flat      every pixel faces the eye                        N
#   --red nothick   alpha ignored (all single scatter, no puff)      K P
#   --red nofog     no air between the puff and the eye              F
#   --red nosoft    no depth fade                                    E
#   --red noshadow  the sun's cascades and the shadow cubes ignored  G
#   --red noedge    no edge-on fade                                  V
#   --red nophase   the phase flat (isotropic)                       H A
#   --red nonear    no near-camera fade                              M
#
# USAGE  bash tests/spells/volpart1_cell.sh [--red NAME]   RECHECK=1 judges again; CAMS="rev1" limits the cameras;
#        a shot already there is kept (a run the memory guard stopped resumes), FRESH=1 shoots everything again
# Run under the nifskope lock (withlock.sh nifskope ...). Needs NOTES (the notes folder holding allon2/shots/profile.txt,
# neongi1/bake_veil and finalfix/launch.sh) and BEFORE (the exe from before the lane, with its own shaders).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
NOTES="${NOTES:-/e/Projects/Claude/notes}"
BEFORE="${BEFORE:-$NOTES/volpart1/before_run/NifSkope.exe}"
SCOPE="${SCOPE:-volpart1_cell}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$REPO/tests/spells/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
case "$RED" in ""|nolights|white|flat|nothick|nofog|nosoft|noshadow|noedge|nophase|nonear) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$NOTES/volpart1/gate${RED:+_red_$RED}}"
GREEN="${GREEN:-$NOTES/volpart1/gate}"
PORT="${PORT:-14941}"
SIZE="${SIZE:-1920x1040}"
CAMS="${CAMS:-rev1 memden}"
[ -n "$RED" ] && CAMS=rev1
RECHECK="${RECHECK:-0}"
declare -A CAM=( [rev1]="23300 -9700 745 22300 -10150 800 55" [memden]="22600 -10050 740 23600 -9500 760 55" )
declare -A WEA=( [rev1]=CommonwealthFoggy [memden]=CommonwealthClear )
declare -A HOUR=( [rev1]=18 [memden]=22 )
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
PROF="$NOTES/allon2/shots/profile.txt"
PLUGINS="$(grep -E '^plugin [0-9]+: ' "$PROF" | sed 's/^plugin [0-9]*: //' | tr -d '\r' | paste -sd, -)"
RES="$(grep -E '^resource [0-9]+: ' "$PROF" | sed 's/^resource [0-9]*: //' | tr -d '\r' | paste -sd';' -)"
BD="$NOTES/neongi1/bake_veil"
LAUNCH="$NOTES/finalfix/launch.sh"

mkdir -p "$OUT"
LOG="$OUT/volpart1_cell.log"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
say "volpart1_cell.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED (must fail)}  out: $OUT"

building() {	# a live build (its lock and its Windows pid alive; a stopped build can leave the lock behind)
	local BL=/e/Projects/NifskopeWildWastelandEdition/.lanes_build.lock w
	[ -d "$BL" ] || return 1
	w="$(cat "$BL/winpid" 2>/dev/null)"
	[ -n "$w" ] && tasklist //FI "PID eq $w" 2>/dev/null | grep -q " $w "
}
room() {	# Goodneighbor takes 6-7 GB; the run guard stops it under 3 GB free: wait for 10 GB (two freezes on 10-03); never beside the game
	local t=0	# a build beside it spikes past the guard
	while { [ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -lt 10485760 ] || building; } && [ $t -lt 3600 ]; do sleep 15; t=$((t+15)); done
	if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -qi 'Fallout4.exe'; then say "  Fallout4 is running: no launch"; return 1; fi
	[ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -ge 10485760 ] || { say "  under 10 GB free: no launch"; return 1; }
}

shoot1() {	# shoot1 <exe> <cam> <tag> [env...]
	local exe="$1" cam="$2" tag="$3"; shift 3
	local run="$OUT/$cam"
	mkdir -p "$run"
	rm -f "$run/$tag.png" "$run/$tag.notes" "$run/${tag}"_[0-9][0-9][0-9].png
	printf 'fps 16\nframes 19\n' > "$run/$cam.path"
	for k in 0 1 17 18; do printf 'key %s %s\n' "$(python -c "print($k/16)")" "${CAM[$cam]}" >> "$run/$cam.path"; done
	env WW_CELL_BAKE_REFRACT_RED=keep WW_CELL_NOGRID=1 WW_LODGEN_RESOURCES="$RES" \
		WW_LOOKDEV=1 WW_LOOKDEV_WEATHER="${WEA[$cam]}" WW_LOOKDEV_HOUR="${HOUR[$cam]}" WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0 \
		WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS="$ESM" \
		WW_ALL_ON=1 WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_FARLOD=0 WW_CELL_GLARE=0 WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$BD/bake")" \
		WW_CELL_AIR=1 WW_LOOKDEV_FOG=1 WW_LOOKDEV_SHADOWS=1 "$@" \
		WW_CELL_OPEN="$PLUGINS|Goodneighbor|5,-3|1" WW_CELL_PROBES="$(winpath "$BD/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_RENDER_PATH="$(winpath "$run/$cam.path")" WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$(fresh_scope)" \
		bash "$LAUNCH" "$run/$tag.notes" "$exe" --port "$PORT" "$(winpath "$REPO/tests/fixtures/empty.wwcell")" > /dev/null 2>&1
	[ -s "$run/${tag}_017.png" ] && mv "$run/${tag}_017.png" "$run/$tag.png"
	rm -f "$run/${tag}"_[0-9][0-9][0-9].png
	say "  $cam/$tag $([ -s "$run/$tag.png" ] && echo shot || echo NO-PICTURE) sky:$(grep -c 'lookdev sky: sky:dome' "$run/$tag.notes" 2>/dev/null)" \
		"$(grep -o 'cell particles volumetric: .*' "$run/$tag.notes" | tail -1)" \
		"$(grep -o 'cell effects volumetric: .*' "$run/$tag.notes" | tail -1)" \
		"$(grep -ci 'errors during shader compilation' "$run/$tag.notes" 2>/dev/null) shader-errors  $(date +%T)"
}
shoot() {	# a floored window is refused by the exe (no picture): up to 3 tries
	local k
	# a stopped run resumes (a red keeps only what it shot itself, never the green copy)
	[ -s "$OUT/$2/$3.png" ] && [ "${FRESH:-0}" != 1 ] && { [ -z "$RED" ] || [ -e "$OUT/$2/$3.red" ]; } && { say "  $2/$3 kept"; return 0; }
	rm -f "$OUT/$2/$3.red"
	for k in 1 2 3; do
		room || return 1
		shoot1 "$@"
		[ -s "$OUT/$2/$3.png" ] && { [ -n "$RED" ] && touch "$OUT/$2/$3.red"; return 0; }
	done
	return 1
}
dumps() { echo WW_CELL_VOLPART_DUMP="$(winpath "$OUT/$1/$2.vp")" WW_CELL_AIR_DUMP="$(winpath "$OUT/$1/$2.air")"; }
NOSH=( WW_CELL_SHADOW=0 WW_LOOKDEV_SHADOWS=0 )

if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	[ -n "$RED" ] && [ -d "$GREEN" ] && cp -rn "$GREEN"/. "$OUT"/ 2>/dev/null
	R=(); [ -n "$RED" ] && R=( WW_CELL_VOLPART_RED="$RED" )
	for c in $CAMS; do
		say "== $c"
		case "$RED" in ""|nolights|white|flat|nothick|nofog|noedge|nophase|nonear)
			shoot "$EXE" "$c" ana WW_CELL_VOLPART=1 WW_CELL_AIR_TERMS=3 "${NOSH[@]}" "${R[@]}" $(dumps "$c" ana) ;;
		esac
		case "$RED" in ""|nolights|noshadow|nosoft)
			shoot "$EXE" "$c" full WW_CELL_VOLPART=1 "${R[@]}" $(dumps "$c" full) ;;
		esac
		if [ -z "$RED" ]; then
			shoot "$EXE" "$c" nosoftp WW_CELL_VOLPART=1 WW_CELL_VOLPART_RED=nosoft
			shoot "$EXE" "$c" hide WW_CELL_VOLPART=1 WW_CELL_VOLPART_HIDE=1
			shoot "$EXE" "$c" off WW_CELL_VOLPART=0
			shoot "$BEFORE" "$c" before
			shoot "$BEFORE" "$c" rung2
		fi
	done
fi
VOLPART1_RED="$RED" python "$REPO/tests/spells/volpart1_check.py" "$OUT" $CAMS 2>&1 | tee -a "$LOG"
exit "${PIPESTATUS[0]}"
