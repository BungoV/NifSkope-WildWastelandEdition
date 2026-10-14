#!/bin/bash
#
# A RECESSED OUTDOOR WALL THE PROBE GI MUST NOT BLACK OUT (lane GIBLACK1, 2026-10-14; res/shaders/cell_lights.glsl
# cellGiSample's lend rule). Judged by tests/spells/cell_giblack_check.py, not by eye.
#
# The defect: an outdoor wall at the back of a narrow recess whose air the rooms grid marks solid. Both of the
# surface's own reads are solid, so the seen fill borrowed the read of a sealed room behind the wall (no sky in its
# probes): a valid voxel that says "no sky here", and the wall went black. The fix: on the exterior sky-lit grid a
# surface whose both reads are solid borrows only outdoor answers; with none it keeps the weather's ambient.
#
# Each camera is shot three ways at 22h (DOF and motion blur off, volumetric fog off):
#   on    the probe GI on (the fix)
#   off   WW_CELL_GI=0: the weather's flat ambient, the reference
#   red   WW_CELL_PV_RED=borrow: the old search (borrows any room's answer)
# then cell_giblack_check.py over the camera's wall rect: on must PASS (mean luminance not below (1-bar) of off, no
# near-zero pixel), red must FAIL (run with --expect fail).
#
#   memden   Goodneighbor, the Memory Den street: the recess left of the pillar
#   recess2  Goodneighbor, a second recessed wall (east face, about x 23560 y -12370), chosen by the twin's scan
#
# USAGE  bash tests/spells/cell_giblack.sh            needs:
#          PLUGINS  the plugin list for WW_CELL_OPEN (comma separated, the profile's order)
#          RES      the resource archives (WW_LODGEN_RESOURCES, ';' separated)
#          BAKE     a probe bake folder of the Goodneighbor 5x5 (holds bake/ and probes.tsv)
#        optional: EXE, OUT, SIZE (1920x1040), PORT, LAUNCH (a launcher script "launch <notes> <exe> args...";
#        without it the exe is run straight), CAMS (names to shoot), BAR (0.5),
#        WAITFREE_KB (wait for that much free RAM before each launch)

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
OUT="${OUT:-$REPO/scratchpad/giblack1_20261014/gate}"
SIZE="${SIZE:-1920x1040}"; PORT="${PORT:-14851}"; BAR="${BAR:-0.5}"
CAMS="${CAMS:-memden recess2}"
CHECK="$REPO/tests/spells/cell_giblack_check.py"
SPEC="$REPO/tests/fixtures/empty.wwcell"
: "${PLUGINS:?PLUGINS= the plugin list}" "${RES:?RES= the resource archives}" "${BAKE:?BAKE= the bake folder}"
mkdir -p "$OUT"
LOG="$OUT/cell_giblack.log"; : > "$LOG"
say() { echo "$*" | tee -a "$LOG"; }

# camera: "eye target fov" and the wall rect (pixels of a 1920-wide frame)
cam_of() { case "$1" in
	memden)  echo "22600 -10050 740 23600 -9500 760 55";;
	recess2) echo "24362 -12370 640 23562 -12370 600 55";;
esac; }
rect_of() { case "$1" in
	memden)  echo "150,250,420,760";;
	recess2) echo "${RECT2:-760,330,1160,760}";;
esac; }

SCOPE=giblack1gate
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$REPO/tests/spells/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
trap wipe_scope EXIT

mkpath() {	# a still camera: 19 frames, the shot is frame 17 (TAA settled)
	printf 'fps 16\nframes 19\n' > "$OUT/$1.path"
	local k; for k in 0 1 17 18; do printf 'key %.6f %s\n' "$(python -c "print($k/16)")" "$(cam_of "$1")" >> "$OUT/$1.path"; done
}

shoot() {	# shoot <cam> <tag> [env...]
	local cam="$1" tag="$2"; shift 2
	[ -s "$OUT/$tag.png" ] && { say "$tag kept"; return 0; }
	local t=0	# WAITFREE_KB: wait (up to 30 min) for that much free RAM before a launch
	while [ -n "${WAITFREE_KB:-}" ] && [ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -lt "$WAITFREE_KB" ] && [ $t -lt 1800 ]; do sleep 15; t=$((t+15)); done
	rm -f "$OUT/$tag.notes" "$OUT/${tag}"_[0-9][0-9][0-9].png
	local run=( env WW_CELL_BAKE_REFRACT_RED=keep WW_CELL_NOGRID=1 WW_LODGEN_RESOURCES="$RES"
		WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=22 WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0
		WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS="$ESM"
		WW_ALL_ON=1 WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_FARLOD=0 WW_VOLFOG=0
		WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$BAKE/bake")" "$@"
		WW_CELL_OPEN="$PLUGINS|Goodneighbor|5,-3|1" WW_CELL_PROBES="$(winpath "$BAKE/probes.tsv")" WW_CELL_PROBES_HIDE=1
		WW_RENDER_PATH="$(winpath "$OUT/$cam.path")" WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE="$SIZE"
		WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$(fresh_scope)" )
	if [ -n "${LAUNCH:-}" ]; then
		"${run[@]}" bash "$LAUNCH" "$OUT/$tag.notes" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > /dev/null 2>&1
	else
		"${run[@]}" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$OUT/$tag.notes" 2>&1
	fi
	local rc=$?
	[ -s "$OUT/${tag}_017.png" ] && mv "$OUT/${tag}_017.png" "$OUT/$tag.png"
	rm -f "$OUT/${tag}"_[0-9][0-9][0-9].png
	say "$tag rc=$rc $( [ -s "$OUT/$tag.png" ] && echo picture || echo NO-PICTURE ) sky dome lines: $(grep -c 'lookdev sky: sky:dome' "$OUT/$tag.notes" 2>/dev/null)"
	[ -s "$OUT/$tag.png" ]
}

fails=0
for cam in $CAMS; do
	mkpath "$cam"
	shoot "$cam" "${cam}_on" || fails=$((fails+1))
	shoot "$cam" "${cam}_off" WW_CELL_GI=0 || fails=$((fails+1))
	shoot "$cam" "${cam}_red" WW_CELL_PV_RED=borrow || fails=$((fails+1))
	[ -s "$OUT/${cam}_on.png" ] && [ -s "$OUT/${cam}_off.png" ] && [ -s "$OUT/${cam}_red.png" ] || continue
	python "$CHECK" "$(winpath "$OUT/${cam}_on.png")" "$(winpath "$OUT/${cam}_off.png")" --rect "$(rect_of "$cam")" \
		--bar "$BAR" --label "$cam on" 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails+1))
	python "$CHECK" "$(winpath "$OUT/${cam}_red.png")" "$(winpath "$OUT/${cam}_off.png")" --rect "$(rect_of "$cam")" \
		--bar "$BAR" --expect fail --label "$cam red borrow" 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails+1))
done
if [ $fails = 0 ]; then say "PASS  cell_giblack: $CAMS"; exit 0; fi
say "FAIL  cell_giblack: $fails failed"; exit 1
