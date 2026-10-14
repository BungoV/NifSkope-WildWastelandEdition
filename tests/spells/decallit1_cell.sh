#!/bin/bash
#
# THE EFFECT DECALS TAKE THE LIGHT OF THE SURFACE UNDER THEM (lane DECALLIT1, 2026-10-14; src/gl/celllights.h
# wwIsCellDecalProgramName, res/shaders/fo4_effectdecal.frag).
#
# The game puts an effect shader with the Decal flag in its decal groups, drawn into the surface under it, so the
# deferred lights, ambient, sky, obscurance and fog light it as they light that surface. Goodneighbor's brick streaks
# (DecoStain*Wall*.nif, BldStreaks01.BGEM) drew self-lit instead: white drips on a shaded wall in the fog.
# Not judged by eye: tests/spells/decallit1_check.py reads three probe pictures at bungo's rev1 camera.
#
#   light  probe 6 (the linear frame, x GAIN), WW_CELL_DECALLIT_PROBE=1: every effect decal draws a white colour,
#          opaque where it shows -- the light it takes, alone
#   wall   probe 6 x GAIN, WW_CELL_DECALLIT_HIDE=2: no effect decal, the wall under them (with its own lighting-shader
#          decals, which are part of the surface) at the same pixels
#   alb    probe 81 (the albedo as drawn, the surface decals composited), WW_CELL_DECALLIT_HIDE=2: no effect decal
#   check  the region is where the decals drew (light differs from wall: geometry, not a hand mask); per pixel the
#          wall's light is wall / albedo^2 (the cell path's linear albedo); the median of light / that must be 1
#          within 0.8..1.25, and under 1 % of the region may clip
#
# RED CONTROL (WW_CELL_DECALLIT_RED=unlit: the effect decals self-lit, as before): --red unlit shoots the light
# picture with the defect on; the check must FAIL (the white decal reads 1, several times the wall's light).
#
# USAGE  bash tests/spells/decallit1_cell.sh [--red unlit]     RECHECK=1 judges the pictures already there.
#        Needs the profile's plugins (PROFILE) and a Goodneighbor GI bake (BAKE); both default to bungo's notes.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/decallit1_20261014/gate}"
LOG="$OUT/decallit1${RED:+_red_$RED}.log"
PORT="${PORT:-14953}"
SIZE="${SIZE:-1920x1040}"
GAIN="${GAIN:-4}"
WEATHER="${WEATHER:-CommonwealthFoggy}"
HOUR="${HOUR:-18}"
RECHECK="${RECHECK:-0}"
NOTES="${NOTES:-/e/Projects/Claude/notes}"
PROFILE="${PROFILE:-$NOTES/allon2/shots/profile.txt}"
BAKE="${BAKE:-$NOTES/neongi1/bake_veil}"
LAUNCH="${LAUNCH:-$NOTES/finalfix/launch.sh}"	# starts the exe through PowerShell (a bash start can exit silently)
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SCOPE="${SCOPE:-decallit1_cell}"
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

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
say "decallit1_cell.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}   $WEATHER $HOUR   gain $GAIN"

if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { say "no NifSkope.exe at $EXE"; exit 2; }
	[ -s "$PROFILE" ] || { say "no profile at $PROFILE"; exit 2; }
	newer=1
	for s in src/gl/celllights.cpp src/gl/renderer.cpp src/gl/bsshape.cpp src/gl/glshape.cpp; do
		[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
	done
	check "the exe is newer than every source this gate covers" "$newer"
	same=1
	for s in fo4_effectshader.frag fo4_effectdecal.frag fo4_effectdecalcsm.frag fo4_default.frag cell_lights.glsl; do
		cmp -s "$REPO/res/shaders/$s" "$(dirname "$EXE")/shaders/$s" || { say "  shaders/$s differs from the source"; same=0; }
	done
	check "the shaders beside the exe are the source's" "$same"

	PLUGINS="$(grep -E '^plugin [0-9]+: ' "$PROFILE" | sed 's/^plugin [0-9]*: //' | tr -d '\r' | paste -sd, -)"
	RES="$(grep -E '^resource [0-9]+: ' "$PROFILE" | sed 's/^resource [0-9]*: //' | tr -d '\r' | paste -sd';' -)"
	# bungo's rev1 camera in Goodneighbor (5,-3), held still over a 19-frame path; frame 17 is kept
	printf 'fps 16\nframes 19\n' > "$OUT/rev1.path"
	for k in 0 1 17 18; do
		printf 'key %s %s\n' "$(python -c "print('%.6f' % ($k / 16))")" "23300 -9700 745 22300 -10150 800 55" >> "$OUT/rev1.path"
	done

	room() {	# the lanes' memory guard kills a run under 3 GB free: wait (up to an hour) for 8.5 GB and no compiler
		local t=0
		while { [ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -lt 8912896 ] \
			|| tasklist 2>/dev/null | grep -qiE '^(cc1plus|cl)\.exe'; } && [ $t -lt 3600 ]; do sleep 15; t=$((t+15)); done
		[ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -ge 7340032 ] || { say "  under 7 GB free after an hour: no launch"; return 1; }
	}
	shoot() {   # shoot <tag> <env...>
		local tag="$1"; shift
		local try rc
		for try in 1 2 3; do
			room || break
			rm -f "$OUT/$tag.png" "$OUT/$tag.notes" "$OUT/${tag}"_[0-9][0-9][0-9].png
			env WW_CELL_NOGRID=1 WW_LODGEN_RESOURCES="$RES" \
				WW_LOOKDEV=1 WW_LOOKDEV_WEATHER="$WEATHER" WW_LOOKDEV_HOUR="$HOUR" WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0 \
				WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_GROUND=0 \
				WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_SHADOWS=1 WW_LOOKDEV_FOG=0 \
				WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_FARLOD=0 WW_CELL_GLARE=0 WW_CELL_IS=0 WW_TAA=0 WW_CELL_SSR=0 \
				WW_CELL_AIR=0 WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$BAKE/bake")" "$@" WW_ALL_ON=1 \
				WW_CELL_OPEN="$PLUGINS|Goodneighbor|5,-3|1" WW_CELL_PROBES="$(winpath "$BAKE/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
				WW_RENDER_PATH="$(winpath "$OUT/rev1.path")" WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
				WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$(fresh_scope)" \
				bash "$LAUNCH" "$OUT/$tag.notes" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > /dev/null 2>&1
			rc=$?
			[ -s "$OUT/${tag}_017.png" ] && mv -f "$OUT/${tag}_017.png" "$OUT/$tag.png"
			rm -f "$OUT/${tag}"_[0-9][0-9][0-9].png
			say "  shot $tag try $try rc $rc $( [ -s "$OUT/$tag.png" ] && echo picture || echo NO-PICTURE )"
			[ -s "$OUT/$tag.png" ] && break
		done
	}
	if [ -n "$RED" ]; then
		shoot light_red_$RED WW_CELL_LIT_PROBE=6 WW_CELL_LIT_PROBE_GAIN="$GAIN" WW_CELL_DECALLIT_PROBE=1 \
			WW_CELL_DECALLIT_RED="$RED"
	else
		shoot light WW_CELL_LIT_PROBE=6 WW_CELL_LIT_PROBE_GAIN="$GAIN" WW_CELL_DECALLIT_PROBE=1
	fi
	[ -s "$OUT/wall.png" ] || shoot wall WW_CELL_LIT_PROBE=6 WW_CELL_LIT_PROBE_GAIN="$GAIN" WW_CELL_DECALLIT_HIDE=2
	[ -s "$OUT/alb.png" ] || shoot alb WW_CELL_LIT_PROBE=81 WW_CELL_DECALLIT_HIDE=2
	tag=light${RED:+_red_$RED}
	for p in "$tag" wall alb; do [ -s "$OUT/$p.png" ] || { say "  FAIL  no $p picture"; fails=$((fails+1)); }; done
	grep -q "fo4_effectdecal" "$OUT/$tag.notes" 2>/dev/null && say "  (the decal program is named in the notes)"
	errs="$(grep -ciE 'shader.*(error|fail)|compile.*error' "$OUT/$tag.notes" 2>/dev/null)"
	check "no shader compile error in the light shot's notes (${errs:-0})" "$([ "${errs:-0}" = 0 ] && echo 1 || echo 0)"
fi

python "$(dirname "$0")/decallit1_check.py" "$OUT" "light${RED:+_red_$RED}" "$GAIN" 2>&1 | tee -a "$LOG"
[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails+1))
say "decallit1_cell.sh: $fails failure(s)${RED:+ (RED CONTROL $RED: a failure is the expected result)}"
[ "$fails" = 0 ]
