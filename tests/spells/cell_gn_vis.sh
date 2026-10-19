#!/bin/bash
#
# GOODNEIGHBOR SKY VISIBILITY (lane GISHADE1's gate, 2026-10-13; the ww-probe-sky-shade skill's "0.639 vs 0.611").
# One launch: the Goodneighbor 5x5 at 16h, the glass camera, with the GI dump (WW_CELL_GI_DUMP); then
# tests/spells/cell_gn_vis_check.py samples the dump's sky-share grid at the fixture's surface points
# (tests/fixtures/gishade1_gn_vis.tsv: the tracer's cosine sky share there) and judges the canopy against the open
# ground (b/a within 0.12 of the tracer's) and the door recess (within 0.04). Judged from the dump, not the picture:
# the gate is the bake's sky visibility as the shader samples it. GISHADE1's dump16 gives 0.639 against 0.611.
#
# USAGE  bash tests/spells/cell_gn_vis.sh            needs:
#          PLUGINS  the plugin list for WW_CELL_OPEN (comma separated, the profile's order)
#          RES      the resource archives (WW_LODGEN_RESOURCES, ';' separated)
#          BAKE     the Goodneighbor probe bake folder (WW_CELL_GI_FROM; GISHADE1: notes/skypic1/bricks2/gn_tod/bake)
#          PROBES   its probes.tsv (GISHADE1: notes/bounce1/probes.tsv)
#        optional: EXE, OUT, SIZE (1280x720), PORT, LAUNCH (a launcher script "launch <notes> <exe> args..."), any
#        extra env for the run in the environment (e.g. WW_CELL_GI_VIS=march)

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
OUT="${OUT:-$REPO/scratchpad/gn_vis}"
SIZE="${SIZE:-1280x720}"; PORT="${PORT:-14852}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
: "${PLUGINS:?PLUGINS= the plugin list}" "${RES:?RES= the resource archives}" "${BAKE:?BAKE= the bake folder}" "${PROBES:?PROBES= the probes.tsv}"
mkdir -p "$OUT"
LOG="$OUT/cell_gn_vis.log"; : > "$LOG"

SCOPE=gnvisgate
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

# the glass camera (eye, target, fov), a still: 19 frames, the shot is frame 17
printf 'fps 16\nframes 19\n' > "$OUT/glass.path"
for k in 0 1 17 18; do printf 'key %.6f %s\n' "$(python -c "print($k/16)")" "22150 -9950 720 21750 -9540 780 55" >> "$OUT/glass.path"; done

rm -rf "$OUT/dump" "$OUT/glass.notes" "$OUT/glass.png"; mkdir -p "$OUT/dump"
run=( env WW_CELL_BAKE_REFRACT_RED=keep WW_CELL_NOGRID=1 WW_LODGEN_RESOURCES="$RES"
	WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=16 WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0
	WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS="$ESM"
	WW_ALL_ON=1 WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_FARLOD=0 WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$BAKE")"
	WW_CELL_GI_DUMP="$(winpath "$OUT/dump")"
	WW_CELL_OPEN="$PLUGINS|Goodneighbor|5,-3|1" WW_CELL_PROBES="$(winpath "$PROBES")" WW_CELL_PROBES_HIDE=1
	WW_RENDER_PATH="$(winpath "$OUT/glass.path")" WW_RENDER_SHOT="$(winpath "$OUT/glass.png")" WW_RENDER_SIZE="$SIZE"
	WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$(fresh_scope)" )
if [ -n "${LAUNCH:-}" ]; then
	"${run[@]}" bash "$LAUNCH" "$OUT/glass.notes" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > /dev/null 2>&1
else
	"${run[@]}" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$OUT/glass.notes" 2>&1
fi
rc=$?
[ -s "$OUT/glass_017.png" ] && mv "$OUT/glass_017.png" "$OUT/glass.png"
rm -f "$OUT"/glass_[0-9][0-9][0-9].png
echo "glass rc=$rc $( [ -s "$OUT/glass.png" ] && echo picture || echo NO-PICTURE ) dump: $(ls "$OUT/dump" 2>/dev/null | wc -l) files" | tee -a "$LOG"
if [ ! -s "$OUT/dump/gi_skygrid.bin" ] || [ ! -s "$OUT/dump/gi_rooms.bin" ]; then
	echo "FAIL  cell_gn_vis: no GI dump (gi_skygrid.bin, gi_rooms.bin)" | tee -a "$LOG"
	exit 1
fi
python "$REPO/tests/spells/cell_gn_vis_check.py" "$(winpath "$OUT/dump")" 2>&1 | tee -a "$LOG"
[ "${PIPESTATUS[0]}" = 0 ] && { echo "PASS  cell_gn_vis" | tee -a "$LOG"; exit 0; }
echo "FAIL  cell_gn_vis" | tee -a "$LOG"
exit 1
