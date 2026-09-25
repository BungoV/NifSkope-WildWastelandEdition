#!/bin/bash
# CARDFIX1 step 7 job 5: "3D model" | "Octahedral impostor" per channel for the .pbrm maple fixture.
# The mesh half takes the bake's .pbrm retarget (WW_IMPOSTOR_MESH_PBRM=1, the fixture read from
# WW_LODGEN_DATA_ROOT) and shows LOD channel 10 (slot 7 raw: R roughness, G metallic) or channel 12 (the
# colour the bake photographs: the TINTED base); the card half shows its own sheet channel (6 = _rmaos R,
# 7 = _rmaos G, 1 = _bc colour). Uses the gate's compressed pbrm set (tests/spells/impostor_pbrm.sh).
# Output: pbrm/pics/*.png (untracked).
set -u
root=/e/Projects/NifskopeWWE-cardfix1
EXE="$root/release/NifSkope.exe"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESH="$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif"
FID=000531b3
GATE="$( cygpath -m "$root/scratchpad/cardfix1_20260924/pbrm/gate" )"
G="$( cygpath -m "$root/scratchpad/cardfix1_20260924/pbrm/pics" )"
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
VIEW="${VIEW:-30:5}"
PORT=27931
rm -rf "$G"; mkdir -p "$G/shots"
[ -f "$GATE/pbrm/cards/${FID}_oct.lodm" ] || { echo "no compressed pbrm set: run tests/spells/impostor_pbrm.sh first"; exit 2; }
T="$G/textures/data/fo4cslod/cards"; mkdir -p "$T"
for f in "$GATE/pbrm/cards/${FID}"_oct*.DDS; do cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
mkdir -p "$G/cards"; cp "$GATE/pbrm/cards/${FID}_oct.lodm" "$G/cards/"
for pair in "rough 10 6" "metal 10 7" "colour 12 1"; do
	set -- $pair
	PORT=$(( PORT + 1 ))
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$G/cards/${FID}_oct.lodm" WW_IMPOSTOR_LOG="$G/shots/$1.log" \
		WW_IMPOSTOR_SHOT="$G/shots/$1" WW_IMPOSTOR_ORBIT_VIEWS="$VIEW" \
		WW_IMPOSTOR_MESH_PBRM=1 WW_LODGEN_DATA_ROOT="$GATE/fix" WW_IMPOSTOR_MESH_CHANNEL="$2" WW_IMPOSTOR_CHANNEL="$3" \
		WW_RENDER_CLEAN=1 WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 600 "$EXE" --port "$PORT" "$MESH" > /dev/null 2>&1
	echo "$1: $( grep -c 'retargeted' "$G/shots/$1.log" ) shapes retargeted; $( ls "$G/shots" | grep -c "^$1.*png" ) shots; $( grep -m1 -i 'refus' "$G/shots/$1.log" )"
done
"$PY" - "$G" <<'PYEOF'
import sys, glob, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageChops
g = sys.argv[1]
try:
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 26)
except Exception:
    font = ImageFont.load_default()
rows = []
for name, ch, label in (("rough", 0, "roughness"), ("metal", 1, "metallic"), ("colour", None, "colour (tinted)")):
    m = glob.glob(os.path.join(g, 'shots', name + '*_mesh.png')); c = glob.glob(os.path.join(g, 'shots', name + '*_card.png'))
    if not m or not c:
        print('%s: shots missing (%d mesh, %d card)' % (name, len(m), len(c))); continue
    a = Image.open(sorted(m)[0]).convert('RGB'); b = Image.open(sorted(c)[0]).convert('RGB')
    if ch is not None:   # the mesh's channel 10 is slot 7 raw: pick the one channel, grey, as the card shows it
        x = np.asarray(a)[..., ch]
        a = Image.fromarray(np.dstack([x, x, x]).astype(np.uint8))
    ub = None   # one crop for every row: the COLOUR shots' ink (a dark channel value hides in the background)
    cm = sorted(glob.glob(os.path.join(g, 'shots', 'colour*_mesh.png'))) + sorted(glob.glob(os.path.join(g, 'shots', 'colour*_card.png')))
    for im in [Image.open(q).convert('RGB') for q in cm] or (a, b):
        bb = ImageChops.difference(im, Image.new('RGB', im.size, im.getpixel((0, 0)))).convert('L').point(
            lambda v: 255 if v > 12 else 0).getbbox()
        if bb:
            ub = bb if ub is None else (min(ub[0], bb[0]), min(ub[1], bb[1]), max(ub[2], bb[2]), max(ub[3], bb[3]))
    pad = 24
    ub = (max(0, ub[0] - pad), max(0, ub[1] - pad), min(a.width, ub[2] + pad), min(a.height, ub[3] + pad))
    a, b = a.crop(ub), b.crop(ub)
    w, h = a.size
    out = Image.new('RGB', (2 * w + 12, h + 44), (30, 30, 30))
    out.paste(a, (0, 44)); out.paste(b, (w + 12, 44))
    d = ImageDraw.Draw(out)
    d.text((10, 8), '3D model -- %s' % label, fill=(235, 235, 235), font=font)
    d.text((w + 22, 8), 'Octahedral impostor -- %s' % label, fill=(235, 235, 235), font=font)
    p = os.path.join(g, 'pbrm_%s.png' % name)
    out.save(p); rows.append(out)
    print('%s: %s %dx%d' % (name, p, out.width, out.height))
if rows:
    W = max(r.width for r in rows); H = sum(r.height for r in rows) + 8 * (len(rows) - 1)
    s = Image.new('RGB', (W, H), (30, 30, 30)); y = 0
    for r in rows:
        s.paste(r, (0, y)); y += r.height + 8
    s.save(os.path.join(g, 'pbrm_all.png')); print('sheet: %s' % os.path.join(g, 'pbrm_all.png'))
PYEOF
