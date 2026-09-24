#!/bin/sh
# IMPOSTORFIX3's copy of IMPOSTORFIX1's control runner, writing under THIS
# lane's folder so neither lane overwrites the other's grabs. Unchanged
# otherwise: one orbit run of the in-application harness, the card scored
# against the mesh at the named views.
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"
tag="$1"; lodm="$2"; mesh="$3"; views="$4"; blend="${5:-1}"
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
grep -E "^(viewport|orbit azim|orbit (iou|colour|counted))" "$R/control/$tag.log"
