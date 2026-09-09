#!/bin/bash
# THE TRANSITION GATE, the way the product actually does it.
#
# bungo: "the tree must be positioned correctly, so that when a 3d tree
# transitions to an imposter, the tree won't change position".
#
# The same object chunk, same cells, same camera, twice:
#   MESH  -- the refs stand on their own LOD meshes (the default at dim 4)
#   CARD  -- the refs stand on their impostor cards (--impostors-from-level 0)
# and a third time as the CONTROL, against a card library whose `center` has
# been deliberately ZEROED, which is what a reader that ignores the field does.
#
# The camera's absolute scale is unknown and does not matter: all three halves
# share one camera, so a displacement between them is a displacement in the
# WORLD, which is exactly the quantity under test. (The render hook cannot
# serve as a metric camera today -- WW_RENDER_CENTER has no effect and a
# 512-unit cube spans 547 / 107 / 25 px at WW_RENDER_DIST 500 / 1000 / 2000,
# so neither its look-at nor its scale is pinned. Measured 2026-09-09.)
#
#   bash shoot_transition2.sh <cards dir> <out dir>
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
NS=$REPO/release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=${1:?cards dir}
OUT=${2:?out dir}
export WW_WINDOW_AT=1960,40
mkdir -p "$OUT"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

# THE CONTROL LIBRARY: the same sheets, the same everything, with `center`
# zeroed in the sidecar so the converter writes a .lodm that puts the quad at
# the pivot. Nothing else differs.
CTL="$OUT/cards_center0"
rm -rf "$CTL"; mkdir -p "$CTL"
cp "$CARDS"/*.png "$CTL"/ 2>/dev/null
python - "$CARDS" "$CTL" <<'PY'
import sys, os, glob
src, dst = sys.argv[1], sys.argv[2]
for p in glob.glob(os.path.join(src, '*.txt')):
    out = []
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if t and t[0] == 'oct' and len(t) >= 12:
            t[6] = t[7] = t[8] = '0'          # cx cy cz -> 0
            ln = ' '.join(t) + '\n'
        elif t and t[0] in ('front', 'side') and len(t) >= 6:
            t[3] = t[4] = t[5] = '0'
            ln = ' '.join(t) + '\n'
        out.append(ln)
    open(os.path.join(dst, os.path.basename(p)), 'w', encoding='utf-8', newline='\n').writelines(out)
print('control library: %d sidecars with center zeroed' % len(glob.glob(os.path.join(dst, '*.txt'))))
PY

bake() {   # bake <tag> <cards> <extra args...>
	local tag=$1; shift
	local cards=$1; shift
	local d="$OUT/$tag"
	rm -rf "$d"; mkdir -p "$d/textures/terrain/Commonwealth"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
		--no-identity --data-root "$DATA" \
		--out-dir "$d" --tex-dir "$d/textures/terrain/Commonwealth" \
		${cards:+--impostors "$cards"} "$@" >/dev/null 2>&1
	ls -l "$d"/*.BTO 2>/dev/null | awk '{print "  ", $5, $NF}'
}

echo "== MESH  (the refs on their own LOD meshes)"
bake mesh ""
echo "== CARD  (the same refs on their impostor cards)"
bake card "$CARDS" --impostors-from-level 0
echo "== CTL   (the same cards with center zeroed)"
bake ctl "$CTL" --impostors-from-level 0

shoot() {  # shoot <tag> <bto>
	local tag=$1 bto=$2
	WW_RENDER_SHOT="$OUT/${tag}.png" WW_RENDER_SIZE=1400x900 WW_RENDER_VIEW=5 WW_RENDER_CLEAN=1 \
	WW_RENDER_TIME=1 "$NS" "$bto" --port 45921 >/dev/null 2>&1
	ls -l "$OUT/${tag}.png" 2>/dev/null | awk '{print "  ", $5, "bytes", $NF}'
}

for tag in mesh card ctl; do
	B="$(ls "$OUT/$tag"/*.BTO 2>/dev/null | head -1)"
	[ -n "$B" ] || { echo "no BTO for $tag"; continue; }
	echo "== shoot $tag"
	shoot "$tag" "$B"
done
