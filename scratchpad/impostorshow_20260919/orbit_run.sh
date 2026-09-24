#!/bin/sh
# ---------------------------------------------------------------------------
# orbit_run.sh -- one subject's orbit, for the show's strips and the GIF.
#
#   sh orbit_run.sh <tag> <lodm> <mesh.nif> [azimuths] [elevations]
#
# Writes scratchpad/impostorshow_20260919/orbit/<tag>/ (two PNGs per view) and
# orbit/<tag>.log (the numbers beside the pictures: IoU and mean colour error
# per view, and the means). The window size is FORCED and the harness prints
# the size it got -- the standing native_open.sh red is a maximized persisted
# geometry flooring WW_RENDER_SIZE, so a picture quoted off an unproven size is
# a picture of this machine.
#
# ONE NifSkope AT A TIME. The caller runs the process guard; this script gives
# every run its own --port and parks the window on the second monitor.
# ---------------------------------------------------------------------------
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"

tag="$1"; lodm="$2"; mesh="$3"
az="${4:-12}"; el="${5:-15,45}"
port="${PORT:-27841}"
size="${SIZE:-512x768}"

mkdir -p "$R/orbit/$tag"
rm -f "$R/orbit/$tag"/*.png

echo "orbit $tag: $az azimuths x [$el] at $size"
WW_IMPOSTOR_PREVIEW=orbit \
WW_IMPOSTOR_LODM="$lodm" \
WW_IMPOSTOR_LOG="$R/orbit/$tag.log" \
WW_IMPOSTOR_SHOT="$R/orbit/$tag/v" \
WW_IMPOSTOR_ORBIT_AZ="$az" \
WW_IMPOSTOR_ORBIT_ELEV="$el" \
WW_RENDER_SIZE="$size" \
WW_WINDOW_AT=1960,40 \
timeout 900 "$EXE" "$mesh" --port "$port" > "$R/orbit/$tag.stdout" 2>&1
rc=$?
echo "  exe rc=$rc"
grep -E "^(viewport|requested|grid|frame|half|orbit (iou|colour|counted))" "$R/orbit/$tag.log"
echo "  pngs: $( ls "$R/orbit/$tag" | wc -l )"
exit $rc
