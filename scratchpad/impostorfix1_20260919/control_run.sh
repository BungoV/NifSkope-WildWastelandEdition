#!/bin/sh
# KNOWN-ANSWER CONTROL (lane IMPOSTORFIX1). Photograph the mesh from each BAKE
# DIRECTION and score the card there. With WW_IMPOSTOR_BLEND=0 the card is ONE
# un-blended frame and that frame is the bake's own photograph of the mesh from
# exactly this camera, so the two silhouettes must very nearly coincide.
# Anything else is a defect upstream of every blend question.
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"
tag="$1"; lodm="$2"; mesh="$3"; views="$4"; blend="${5:-0}"
port="${PORT:-27861}"; size="${SIZE:-512x768}"
mkdir -p "$R/control/$tag"; rm -f "$R/control/$tag"/*.png
WW_IMPOSTOR_PREVIEW=orbit \
WW_IMPOSTOR_LODM="$lodm" \
WW_IMPOSTOR_LOG="$R/control/$tag.log" \
WW_IMPOSTOR_SHOT="$R/control/$tag/v" \
WW_IMPOSTOR_ORBIT_VIEWS="$views" \
WW_IMPOSTOR_BLEND="$blend" \
WW_RENDER_SIZE="$size" \
WW_WINDOW_AT=1960,40 \
timeout 900 "$EXE" "$mesh" --port "$port" > "$R/control/$tag.stdout" 2>&1
echo "  exe rc=$?"
grep -E "^(viewport|orbit [0-9]|orbit azim|orbit (iou|colour|counted))" "$R/control/$tag.log"
