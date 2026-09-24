#!/bin/bash
# CARDFIX1 step 6: the sway GIF bungo asked for -- the elm's N8 card drawn with the drawer's wind shear
# (WW_IMPOSTOR_SWAY_AMP / WW_IMPOSTOR_SWAY_PHASE, the harness hook fix17 added) at 12 phases, beside the
# mesh from the same camera. The mesh does NOT move: NifSkope has no tree-wind vertex animation, so the
# left half is the reference shape, not a reference motion. Output: $WORK/gif/elm_sway.gif (untracked).
set -u
root=/e/Projects/NifskopeWWE-cardfix1
EXE="$root/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESH="$DATA/meshes/Landscape/Trees/TreeElmForest01.nif"
FID=000531b3
WORK="$( cygpath -m "$root/scratchpad/cardfix1_20260924/wind" )"
G="$WORK/gif"
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
AMP="${AMP:-0.08}"; AZ="${AZ:-30}"; EL="${EL:-5}"; N="${N:-12}"
PORT=27901
if [ -z "${SKIP_BAKE:-}" ]; then
rm -rf "$G"; mkdir -p "$G/bake" "$G/cards" "$G/shots"
WW_IMPOSTOR_BAKE="$G/bake" WW_IMPOSTOR_TILE=256 WW_IMPOSTOR_OCT=8 WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$MESH" --port $PORT > "$G/bake.stdout" 2>&1
for f in "$G/bake/treeelmforest01"_oct_*.png "$G/bake/treeelmforest01"_front.png "$G/bake/treeelmforest01"_side.png; do
	[ -f "$f" ] && cp "$f" "$G/cards/${FID}${f##*/treeelmforest01}"
done
cp "$G/bake/treeelmforest01.txt" "$G/cards/${FID}.txt"
echo "bake: $( grep -m1 '^sway ' "$G/cards/${FID}.txt" ), $( grep -m1 '^oct ' "$G/cards/${FID}.txt" )"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
	--impostors "$G/cards" --data-root "$DATA" -o "$G/chunk.bto" > "$G/lodgen.log" 2>&1
echo "compress rc $?; lodm $( [ -f "$G/cards/${FID}_oct.lodm" ] && echo yes || echo NO )"
fi
# The preview's texture cache has no absolute-path fallback: the sheets must sit under a folder NAMED
# `textures` that is an ancestor-sibling of the .lodm (ImpostorDraw::registerLooseSheets), at the game path
# the .lodm names (Data\FO4CSLOD\Cards\...). Run 1 drew the card blank for want of it ("draw REFUSED").
T="$G/textures/data/fo4cslod/cards"; rm -rf "$G/textures" "$G/shots"; mkdir -p "$T" "$G/shots"
for f in "$G/cards/${FID}"_oct_*.DDS; do cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
echo "texture tree: $( ls "$T" | tr '\n' ' ' )"
for k in $( seq 0 $(( N - 1 )) ); do
	PORT=$(( PORT + 1 ))
	ph=$( "$PY" -c "import math; print('%.5f' % (2 * math.pi * $k / $N))" )
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$G/cards/${FID}_oct.lodm" WW_IMPOSTOR_LOG="$G/shots/p$k.log" \
		WW_IMPOSTOR_SHOT="$G/shots/p$( printf %02d $k )" WW_IMPOSTOR_ORBIT_VIEWS="$AZ:$EL" \
		WW_IMPOSTOR_SWAY_AMP="$AMP" WW_IMPOSTOR_SWAY_PHASE="$ph" WW_RENDER_CLEAN=1 \
		WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 600 "$EXE" --port "$PORT" "$MESH" > /dev/null 2>&1
	echo "phase $k ($ph rad): $( grep -m1 '^sway:' "$G/shots/p$k.log" ) | $( grep -m1 '^orbit azim' "$G/shots/p$k.log" | cut -c1-80 )"
done
"$PY" - "$G" "$AMP" <<'PYEOF'
import sys, glob, os
from PIL import Image, ImageDraw, ImageFont
g, amp = sys.argv[1], sys.argv[2]
cards = sorted(glob.glob(os.path.join(g, 'shots', 'p*_card.png')))
meshes = sorted(glob.glob(os.path.join(g, 'shots', 'p*_mesh.png')))
if not cards or len(cards) != len(meshes):
    raise SystemExit('frames: %d card, %d mesh' % (len(cards), len(meshes)))
m0 = Image.open(meshes[0]).convert('RGB')
def box(im):
    # the union of both halves' ink over every frame, so nothing is cut and the frame does not jump
    bg = im.getpixel((0, 0))
    diff = Image.new('RGB', im.size, bg)
    from PIL import ImageChops
    return ImageChops.difference(im, diff).convert('L').point(lambda v: 255 if v > 12 else 0).getbbox()
ub = None
for p in cards + meshes:
    b = box(Image.open(p).convert('RGB'))
    if b:
        ub = b if ub is None else (min(ub[0], b[0]), min(ub[1], b[1]), max(ub[2], b[2]), max(ub[3], b[3]))
pad = 24
ub = (max(0, ub[0] - pad), max(0, ub[1] - pad), min(m0.width, ub[2] + pad), min(m0.height, ub[3] + pad))
try:
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 26)
except Exception:
    font = ImageFont.load_default()
frames = []
for c, m in zip(cards, meshes):
    a = Image.open(m).convert('RGB').crop(ub); b = Image.open(c).convert('RGB').crop(ub)
    w, h = a.size
    out = Image.new('RGB', (2 * w + 12, h + 44), (30, 30, 30))
    out.paste(a, (0, 44)); out.paste(b, (w + 12, 44))
    d = ImageDraw.Draw(out)
    d.text((10, 8), '3D model', fill=(235, 235, 235), font=font)
    d.text((w + 22, 8), 'Octahedral impostor', fill=(235, 235, 235), font=font)
    frames.append(out.quantize(colors=255, method=Image.MEDIANCUT))
dst = os.path.join(g, 'elm_sway.gif')
frames[0].save(dst, save_all=True, append_images=frames[1:], duration=110, loop=0)
print('gif %s: %d frames %dx%d, amplitude %s' % (dst, len(frames), frames[0].width, frames[0].height, amp))
PYEOF
