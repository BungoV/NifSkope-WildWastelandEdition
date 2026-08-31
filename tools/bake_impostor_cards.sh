#!/bin/bash
#
# LODGEN impostor card baking driver (docs/LODGEN_PLAN.md rung 3).
#
# Asks the CLI which bases in a region lack far MNAM slots, then photographs
# each one's best LOD model through the WW_IMPOSTOR_BAKE hook (orthographic
# front+side, two-pass matte alpha) and files the cards by FORM ID — the
# naming `lodgen --impostors <dir>` consumes. Cards convert to BC1
# punch-through DDS lazily at generation time; ship the directory as
# Data/Textures/Lodgen/Cards.
#
# One GUI launch per model (the hook quits after the grab), serialized under
# the harness lock, windows on the second monitor. ~3 s per card.
#
# USAGE
#   bash tools/bake_impostor_cards.sh <esm> <x0> <y0> <x1> <y1> <outdir>
#   MAX=5 bash tools/bake_impostor_cards.sh ...   # cap for a smoke run

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NS="$ROOT/release/NifSkope.exe"
ESM="${1:?esm}"
X0="${2:?x0}"; Y0="${3:?y0}"; X1="${4:?x1}"; Y1="${5:?y1}"
OUT="${6:?outdir}"
DATAROOT="${DATAROOT:-E:/Tools/Fallout 4/DataUnpacked/Data}"
MAX="${MAX:-100000}"

. "$ROOT/tests/spells/_harness.sh"
mkdir -p "$OUT"
W="$(mktemp -d)"
trap 'rm -rf "$W"; rmdir "$ROOT/.harness.lock" 2>/dev/null' EXIT

mkdir "$ROOT/.harness.lock" 2>/dev/null || { echo "harness lock busy"; exit 1; }

n=0
# tr strips the CLI's CRLF: a \r glued to the model path fails every -f test
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region "$X0" "$Y0" "$X1" "$Y1" \
	--list-impostor-candidates 2>/dev/null | tr -d '\r' | while read -r formid model; do
	[ "$n" -ge "$MAX" ] && break
	n=$((n+1))
	[ -f "$OUT/${formid}.txt" ] && { echo "[$n] $formid cached"; continue; }
	mesh="$DATAROOT/meshes/${model//\\//}"
	if [ ! -f "$mesh" ]; then
		echo "[$n] $formid MISSING $mesh"
		continue
	fi
	rm -rf "$W/bake"; mkdir "$W/bake"
	WW_IMPOSTOR_BAKE="$(winpath "$W/bake")" "$NS" "$(winpath "$mesh")" --port 45917 >/dev/null 2>&1
	base="$(basename "${model//\\//}" .nif | tr 'A-Z' 'a-z')"
	if [ -f "$W/bake/${base}_front.png" ]; then
		mv "$W/bake/${base}_front.png" "$OUT/${formid}_front.png"
		mv "$W/bake/${base}_side.png" "$OUT/${formid}_side.png" 2>/dev/null
		mv "$W/bake/${base}.txt" "$OUT/${formid}.txt"
		echo "[$n] $formid baked ($model)"
	else
		echo "[$n] $formid FAILED ($model)"
	fi
done
echo done
