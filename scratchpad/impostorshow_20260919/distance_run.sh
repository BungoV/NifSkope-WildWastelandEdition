#!/bin/sh
# ---------------------------------------------------------------------------
# distance_run.sh -- the DISTANCE STRIP's runs.
#
# One run per SCENE: the full mesh, then each authored vanilla LOD model of the
# same object. The CARD is drawn in every one of them (it is armed from the
# `.lodm`, not from the scene), so the card column can be taken from the first
# run and the rest contribute only their scene column -- but each run saves its
# own card grab anyway, which is the cheapest possible check that the card did
# not change when the scene did.
#
# NO DECIMATION ANYWHERE. The "vanilla LOD" rows are Bethesda's own authored
# LOD NIFs out of Data\meshes\LOD\..., opened as they ship. Nothing in this
# lane simplifies a mesh.
#
#   sh distance_run.sh <tag> <lodm> <nif> [px list]
# ---------------------------------------------------------------------------
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"

tag="$1"; lodm="$2"; nif="$3"; px="${4:-256,128,64,32,16}"
port="${PORT:-27871}"
size="${SIZE:-1024x1024}"

mkdir -p "$R/dist/$tag"
rm -f "$R/dist/$tag"/*.png

echo "distance $tag: $px px at $size"
WW_IMPOSTOR_PREVIEW=distance \
WW_IMPOSTOR_LODM="$lodm" \
WW_IMPOSTOR_LOG="$R/dist/$tag.log" \
WW_IMPOSTOR_SHOT="$R/dist/$tag/d" \
WW_IMPOSTOR_DIST_PX="$px" \
WW_IMPOSTOR_AZIM="${AZIM:-45}" \
WW_IMPOSTOR_ELEV="${ELEV:-15}" \
WW_RENDER_SIZE="$size" \
WW_WINDOW_AT=1960,40 \
timeout 900 "$EXE" "$nif" --port "$port" > "$R/dist/$tag.stdout" 2>&1
rc=$?
echo "  exe rc=$rc"
grep -E "^(viewport|requested|distance )" "$R/dist/$tag.log"
exit $rc
