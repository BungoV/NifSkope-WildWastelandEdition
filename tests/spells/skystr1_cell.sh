#!/bin/bash
#
# THE SKY VISIBILITY STRENGTH (lane SKYSTR1, 2026-10-13; src/probesky.cpp octantVis, Show > Lighting > Sky visibility).
#
# The slider blends every probe octant's stored sky visibility toward an unoccluded sky, as FO4CS's
# [Skylighting] fStrength does (lerp(1, v, s)): 100 % is the bake as it is, 0 % is the sky with no occlusion.
# Not judged by eye: tests/spells/skystr1_check.py reads the pictures and the relight's numbers.
#
#   bake   one window places + bakes Concord's probes (Lookdev pinned, as tests/spells/cell_sky.sh) with its soup
#   shots  per camera (in-bounds, player eye; checked by notes/neongi1/tools/camcheck.py when its files are there),
#          from that bake (WW_CELL_GI_FROM: no new bake): the exe from before the lane (before.png), this exe at
#          its default (s100), pinned 100 (s100pin), pinned 0 (s0), pinned 50 (s50), and the existing refuter
#          WW_CELL_SKY_RED=novis (n0, every octant sees sky); on the first camera also probe 5 (the bounce a
#          surface takes / pi) of each; the relight's dumps at 0, 50 and 100
#   live   one window with the Show-menu driver: shot, set 0, shot, set 100, shot, set 50, shot, dump, quit; then a
#          second window in the same settings scope: the row must read 50 (persisted), and the menu layout
#   check  skystr1_check.py (stages K E Z H) and --live, cell_showmenu.py layout on the live dump
#
# RED CONTROL (WW_CELL_SKYSTR_RED=ignore: the slider read as 100 whatever it says): --red ignore shoots s0 and
# s50 (and their dumps) with the defect on; the check must FAIL K, Z and H.
#
# USAGE  bash tests/spells/skystr1_cell.sh [--red ignore]     PHASE="bake shots live check" (default all four)
#        RECHECK=1 judges the files already there (no launch).

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_skystr1.exe}"
SCOPE="${SCOPE:-skystr1_cell}"
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
OUT="${OUT:-$REPO/scratchpad/skystr1_20261013/gate}"
LOG="$OUT/skystr1${RED:+_red_$RED}.log"
PORT="${PORT:-14833}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
WEATHER="${WEATHER:-CommonwealthClear}"
HOUR="${HOUR:-12}"
OPEN="${OPEN:-Commonwealth|-15,17|1}"
PHASE="${PHASE:-bake shots live check}"
RECHECK="${RECHECK:-0}"
[ "$RECHECK" = 1 ] && PHASE="check"
# in-bounds player-eye cameras in Concord (eye x y z, target x y z, fov), from lane GISHADE1's list, judged by
# neongi1's camcheck (frame back-faces < 2 %): hcorner2 the street corner (GISHADE1's hcorner aimed 18 u higher:
# the original reads 0.020 and is refused), stairA inside the ruin by the stair
CAMS="${CAMS:-hcorner2:-57800,70325,6290,-58200,70250,6280,70 stairA:-58600,70330,6320,-58850,70450,6450,70}"
CAMTOOLS="${CAMTOOLS:-/e/Projects/Claude/notes}"
TRIS="${TRIS:-$CAMTOOLS/gishade1/tris_concord.npy}"
NAV="${NAV:-$CAMTOOLS/gishade1/nav/nav_m15_17.npz}"
CAMCHECK="${CAMCHECK:-$CAMTOOLS/neongi1/tools/camcheck.py}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
has() { case " $PHASE " in *" $1 "*) return 0;; esac; return 1; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "skystr1_cell.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}   phase: $PHASE   port $PORT"
if [ "$RECHECK" != 1 ]; then
	newer=1
	for s in src/probesky.cpp src/probegi.cpp src/gl/celllights.cpp src/cellworkspace.cpp src/cellworkspacetest.cpp \
		res/shaders/cell_lights.glsl; do
		[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
	done
	check "the exe is newer than every source this gate covers" "$newer"
	cmp -s "$REPO/res/shaders/cell_lights.glsl" "$REPO/release/shaders/cell_lights.glsl"
	check "the shader beside the exe is the source's" "$([ $? = 0 ] && echo 1 || echo 0)"
fi

# the weather the view is lit by, pinned (as cell_sky.sh): Lookdev, the plain sun arc, no dome / fog / ground
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER="$WEATHER" WW_LOOKDEV_HOUR="$HOUR" WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" )
# everything that moves frame to frame off: one picture per launch, the same on every run
STILL=( WW_TAA=0 WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_IS=0 WW_CELL_GI_PASSES=1 WW_CELL_PARTICLES=0 WW_CELL_GRASS=0 )
BAKE="$OUT/bake"
FROM=( WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$BAKE")" )

campath() {   # campath <name> <ex,ey,ez,ax,ay,az,fov>: a two-frame path that holds the camera still
	local w; w="$(echo "$2" | tr ',' ' ')"
	printf 'fps 16\nframes 2\nkey 0.000000 %s\nkey 0.062500 %s\n' "$w" "$w" > "$OUT/$1.path"
}

shoot() {   # shoot <exe> <shot.png> <path name or ''> <env...>
	local exe="$1" shot="$2" cam="$3"; shift 3
	local notes="${shot%.png}.notes" base="${shot%.png}" camenv=()
	mkdir -p "$(dirname "$shot")"
	rm -f "$shot" "$notes" "${base}"_[0-9][0-9][0-9].png
	[ -n "$cam" ] && camenv=( WW_RENDER_PATH="$(winpath "$OUT/$cam.path")" ) || camenv=( WW_RENDER_VIEW=1 )
	# a launch that dies after the load with no picture (seen 10-13 on the exe from before the lane too: rc 127,
	# the log ends at the cull line) is tried again, up to three times; every try's rc goes to the log
	local try rc
	for try in 1 2 3; do
		env "${STILL[@]}" "${LD[@]}" "$@" "${camenv[@]}" \
			WW_CELL_OPEN="$ESM|$OPEN" WW_CELL_DATAROOT="$DATA" \
			WW_CELL_PROBES="$(winpath "$OUT/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_LIT=1 \
			WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
		rc=$?
		[ -n "$cam" ] && [ -s "${base}_001.png" ] && mv -f "${base}_001.png" "$shot"
		rm -f "${base}"_[0-9][0-9][0-9].png
		echo "  $(basename "$(dirname "$shot")")/$(basename "$shot") try $try rc $rc free $(( $(awk '/MemFree/{print $2}' /proc/meminfo) / 1024 )) MB $([ -s "$shot" ] && echo picture || echo NO-PICTURE)" >> "$LOG"
		[ -s "$shot" ] && break
		sleep 5
	done
	[ -s "$shot" ] && echo 1 || echo 0
}

# ---- the cameras: in-bounds, player eye
if [ "$RECHECK" != 1 ] && [ -z "$RED" ] && has shots; then
	if [ -f "$CAMCHECK" ] && [ -f "$TRIS" ] && [ -f "$NAV" ]; then
		cargs=()
		for c in $CAMS; do cargs+=( "${c%%:*}" "$(echo "${c#*:}" | tr ',' ' ')" ); done
		python "$CAMCHECK" "$TRIS" "$NAV" "${cargs[@]}" > "$OUT/camcheck.txt" 2>&1
		sed 's/^/  /' "$OUT/camcheck.txt" | tee -a "$LOG"
		n="$(echo $CAMS | wc -w)"
		check "every camera in-bounds ($n)" "$([ "$(grep -c IN-BOUNDS "$OUT/camcheck.txt")" = "$n" ] && echo 1 || echo 0)"
	else
		say "  FAIL  no camcheck files ($CAMCHECK, $TRIS, $NAV)"; fails=$((fails+1))
	fi
fi

if [ -z "$RED" ] && has bake; then
	say "== bake (Concord $OPEN, $WEATHER hour $HOUR)"
	rm -rf "$BAKE" "$OUT/soup.psp"
	ok="$(shoot "$EXE" "$OUT/bake_lit.png" "" WW_CELL_GI=1 WW_CELL_PROBE_SOUP="$(winpath "$OUT/soup.psp")" \
		WW_CELL_PROBE_BAKE="$(winpath "$BAKE")")"
	grep -h "  gi" "$OUT/bake_lit.notes" | head -3 | cut -c1-300 | tee -a "$LOG"
	check "the bake written" "$([ "$ok" = 1 ] && ls "$BAKE"/*.tbk > /dev/null 2>&1 && echo 1 || echo 0)"
fi

first=1
for c in $CAMS; do
	name="${c%%:*}"; d="$OUT/$name"
	campath "$name" "${c#*:}"
	[ "$RECHECK" = 1 ] && continue
	has shots || continue
	say "== camera $name ${RED:+(RED $RED)}"
	ok=1
	dmp() { [ "$first" = 1 ] && echo "WW_CELL_GI_DUMP=$(winpath "$OUT/$1")" || echo "WW_CELL_GI_DUMP_UNUSED=1"; }
	if [ -z "$RED" ]; then
		[ -x "$BEFORE" ] || { say "  no exe from before the lane at $BEFORE"; fails=$((fails+1)); }
		[ "$(shoot "$BEFORE" "$d/before.png" "$name" "${FROM[@]}")" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/s100.png" "$name" "${FROM[@]}" "$(dmp dump_s100)")" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/s100pin.png" "$name" "${FROM[@]}" WW_CELL_SKYSTR=100)" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/s0.png" "$name" "${FROM[@]}" WW_CELL_SKYSTR=0 "$(dmp dump_s0)")" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/s50.png" "$name" "${FROM[@]}" WW_CELL_SKYSTR=50 "$(dmp dump_s50)")" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/n0.png" "$name" "${FROM[@]}" WW_CELL_SKY_RED=novis)" = 1 ] || ok=0
		if [ "$first" = 1 ]; then
			[ "$(shoot "$BEFORE" "$d/probe5_before.png" "$name" "${FROM[@]}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
			for s in s100:100 s0:0 s50:50; do
				[ "$(shoot "$EXE" "$d/probe5_${s%%:*}.png" "$name" "${FROM[@]}" WW_CELL_SKYSTR="${s#*:}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
			done
			[ "$(shoot "$EXE" "$d/probe5_n0.png" "$name" "${FROM[@]}" WW_CELL_SKY_RED=novis WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
		fi
	else
		rp="red_${RED}_"
		redenv=( WW_CELL_SKYSTR_RED="$RED" )
		[ "$(shoot "$EXE" "$d/${rp}s0.png" "$name" "${FROM[@]}" "${redenv[@]}" WW_CELL_SKYSTR=0 "$(dmp ${rp}dump_s0)")" = 1 ] || ok=0
		[ "$(shoot "$EXE" "$d/${rp}s50.png" "$name" "${FROM[@]}" "${redenv[@]}" WW_CELL_SKYSTR=50 "$(dmp ${rp}dump_s50)")" = 1 ] || ok=0
		if [ "$first" = 1 ]; then
			for s in s0:0 s50:50; do
				[ "$(shoot "$EXE" "$d/probe5_${rp}${s%%:*}.png" "$name" "${FROM[@]}" "${redenv[@]}" WW_CELL_SKYSTR="${s#*:}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
			done
		fi
	fi
	grep -h "skyVis\|sky:" "$d/s50.notes" 2>/dev/null | head -2 | cut -c1-200 | tee -a "$LOG"
	check "$name: the pictures written" "$ok"
	first=0
done

# ---- live: the row in a running window, then a relaunch in the same settings scope
if [ -z "$RED" ] && has live && [ "$RECHECK" != 1 ]; then
	say "== live (Show > Lighting > Sky visibility in a running window)"
	L="$OUT/live"; rm -rf "$L"; mkdir -p "$L/a" "$L/b"
	c1="${CAMS%% *}"; cam="$(echo "${c1#*:}" | tr ',' ' ')"
	F="CellWorkspaceSkyVis"
	live_run() {   # live_run <dir> <steps> [keep]
		[ -n "${3:-}" ] || fresh_scope > /dev/null
		env "${STILL[@]}" "${LD[@]}" "${FROM[@]}" WW_CELL_LIT=1 \
			WW_CELL_OPEN="$ESM|$OPEN" WW_CELL_DATAROOT="$DATA" WW_CELL_PROBES="$(winpath "$OUT/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
			WW_SHOWMENU_GATE="$(winpath "$1")" WW_SHOWMENU_GATE_STEPS="$2" \
			WW_SETTINGS_SCOPE="$SCOPE" timeout 1500 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$1/run.notes" 2>&1
		echo $?
	}
	for try in 1 2 3; do   # a window that dies after its load is tried again (see shoot)
		rc="$(live_run "$L/a" "cam:$cam,dump:start.tsv,set:$F=100:10000,shot:l100,set:$F=0,shot:l0,set:$F=100,shot:l100b,set:$F=50,shot:l50,dump:end.tsv,quit")"
		say "  first window try $try rc $rc"
		[ -s "$L/a/end.tsv" ] && break
	done
	reg_val="$(reg query "$REGKEY\\WW" //v CellSkyVis 2>/dev/null | awk '/CellSkyVis/{print $NF}')"
	say "  the settings scope holds WW/CellSkyVis = ${reg_val:-nothing} after the first window"
	for try in 1 2 3; do
		rc="$(live_run "$L/b" "dump:relaunch.tsv,quit" keep)"
		say "  second window try $try rc $rc"
		[ -s "$L/b/relaunch.tsv" ] && break
	done
	grep -h "$F" "$L"/a/*.log "$L"/a/*.txt 2>/dev/null | head -4 | tee -a "$LOG"
	start="$(awk -F'\t' -v f="$F" '$1=="VALUE" && $4==f {print $5}' "$L/a/start.tsv" 2>/dev/null)"
	again="$(awk -F'\t' -v f="$F" '$1=="VALUE" && $4==f {print $5}' "$L/b/relaunch.tsv" 2>/dev/null)"
	say "  the row read ${start:-nothing} at the start (fresh scope), ${again:-nothing} after a relaunch (set to 50)"
	check "the row starts at 100 and keeps 50 across a relaunch (no restart needed for the change itself)" \
		"$([ "${start%%.*}" = 100 ] && [ "${again%%.*}" = 50 ] && echo 1 || echo 0)"
fi

if has check; then
	say "== check"
	python "$(dirname "$0")/skystr1_check.py" "$ESM" "$OUT" --weather "$WEATHER" --hour "$HOUR" ${RED:+--red "$RED"} \
		> "$OUT/check${RED:+_red_$RED}.txt" 2>&1
	sed 's/^/  /' "$OUT/check${RED:+_red_$RED}.txt" | tee -a "$LOG"
	if [ -n "$RED" ]; then
		for st in K Z H; do
			check "the red control FAILS stage $st" "$(grep -q "^$st FAIL" "$OUT/check_red_$RED.txt" && echo 1 || echo 0)"
		done
	else
		check "every stage (K E Z H) passes" "$(grep -q "^skystr PASS" "$OUT/check.txt" && echo 1 || echo 0)"
		if [ -f "$OUT/live/a/l50.png" ]; then
			python "$(dirname "$0")/skystr1_check.py" --live "$OUT/live/a" > "$OUT/live/check.txt" 2>&1
			sed 's/^/  /' "$OUT/live/check.txt" | tee -a "$LOG"
			check "the slider works live" "$(grep -q "^skystr live PASS" "$OUT/live/check.txt" && echo 1 || echo 0)"
			python "$(dirname "$0")/cell_showmenu.py" layout "$OUT/live/a/end.tsv" > "$OUT/live/layout.txt" 2>&1
			sed 's/^/  /' "$OUT/live/layout.txt" | tee -a "$LOG"
			check "the Show menu holds the row in its place" "$(grep -q "^RESULT layout PASS" "$OUT/live/layout.txt" && echo 1 || echo 0)"
		fi
	fi
fi
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
