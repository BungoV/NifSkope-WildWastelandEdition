#!/bin/bash
#
# Octahedral impostors under docs/LODGEN_IMPOSTOR_SPEC.md, in BOTH LOD material
# families: one model photographed from an N x N grid of views over the upper
# hemisphere into four sheets - colour (albedo + coverage), normal (X, Y,
# height, sway), the mask sheet, GSAOS (gloss, specular, AO, subsurface
# mask) for a vanilla-sourced set or RMAOS (roughness, metallic, AO,
# subsurface mask) for a .lodm-sourced one, and the EMISSIVE sheet (_g legacy,
# _e pbr, BC1, no alpha) - converted beside the cards with a
# `<id>_oct.lodm` naming the set, and a `C` manifest line for
# every placement that stands on such a card.
#
# Two REAL bakes through the GUI hook (N=4, 16 views each), on the first
# impostor candidate of the Sanctuary region:
#   1. as the game ships it: the LEGACY family, R gloss = smoothness x the
#      vanilla _s map's G, G specular = the map's R x the strength; and the
#      emissive BLACK, because the near maple is alpha-tested on every shape
#      and an alpha-tested material spends its alpha on the cut-out; and the
#      card .lodm's emissiveScale 0, because no shape of the near maple
#      own-emits with a colour that is not black
#   2. with a source .lodm per material in WW_LODGEN_DATA_ROOT: the PBR
#      family, the third sheet the specular slot RAW (so R and G are bake 1's
#      G and R), the albedo retargeted to the material's normal map (so the
#      colour moves), and the emissive the GLOW slot raw - the fixture names
#      the material's own diffuse there, so the emissive sheet must come out
#      as the colour sheet, which is what makes the black in bake 1 a
#      measurement and not a channel that never writes; and the card .lodm's
#      emissiveScale the 2.5 the fixtures name, which is what makes the 0 in
#      bake 1 a measurement and not a field that is always zero
# then the far chunk (-32,16) at dim 16 from each card set. Every check is a
# measurement of the output, never of the hook's structure.
#
# The FRAME SIZE CLASS is checked here too: the longer side is the tile and the
# shorter one a multiple of 16, so a worldspace's trees fall into a handful of
# sheet sizes that a card array can group; and the recorded extents carry the
# frame's aspect exactly, because the extents are widened to the frame rather
# than the silhouette stretched into it.
#
# USAGE
#   bash tests/spells/lodgen_octahedral.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
. "$ROOT/tests/spells/_harness.sh"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# `formid extent model` since the size ladder: the extent is column two
read -r FORMID EXTENT MODEL < <("$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --list-impostor-candidates 2>/dev/null | tr -d '\r' | grep -E '^[0-9a-fA-F]{8} ' | head -1)
[ -n "${FORMID:-}" ] || { bad "no impostor candidate in the region"; echo "RESULT FAIL"; exit 1; }
ID="$(echo "$FORMID" | tr 'A-F' 'a-f')"
MESH="$DATA/meshes/${MODEL//\\//}"
[ -f "$MESH" ] || { bad "candidate model missing: $MESH"; echo "RESULT FAIL"; exit 1; }
ok "candidate $ID: $MODEL"
BASE="$(basename "${MODEL//\\//}" .nif | tr 'A-Z' 'a-z')"

# ---------------------------------------------------------------- bake 1: legacy
mkdir -p "$W/bake" "$W/cards"
WW_IMPOSTOR_BAKE="$(winpath "$W/bake")" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64 timeout 240 "$NS" "$(winpath "$MESH")" --port 45919 >/dev/null 2>&1
for s in albedo normal gsaos g; do
	[ -s "$W/bake/${BASE}_oct_$s.png" ] || { bad "sheet ${BASE}_oct_$s.png was not written"; ls "$W/bake"; echo "RESULT FAIL"; exit 1; }
done
ok "the four legacy sheets were written (albedo, normal, gsaos, g)"
[ -e "$W/bake/${BASE}_oct_e.png" ] && bad "a pbr-name emissive sheet was written by the legacy bake"
# `oct N tw th halfW halfH cx cy cz span family base` - the family is no longer
# the last token, the RUN's chosen resolution is
grep -qE "^oct 4 .* legacy 64$" "$W/bake/${BASE}.txt" && ok "the meta's oct line says legacy and names the run's 64 px" || bad "no legacy oct line naming the run's resolution in the meta"
NCAND="$(grep -c "^lodm .* none " "$W/bake/${BASE}.txt")"
echo "  .lodm candidates looked for: $NCAND"
[ "$NCAND" -ge 1 ] && ok "the meta names every .lodm candidate it looked for" || bad "no lodm candidate lines in the meta"
grep -q "^model " "$W/bake/${BASE}.txt" && ok "the meta names the model it photographed" || bad "no model line in the meta"
HID="$(grep -c "^hidden " "$W/bake/${BASE}.txt")"
EXP="$("$PY" - "$MESH" <<'PYEOF'
import sys, io, contextlib, re
sys.path.insert(0, "E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype")
import nifparse
with contextlib.redirect_stdout(io.StringIO()):
    data, hdr, strings, blocks = nifparse.parse(sys.argv[1])
print(sum(1 for s in strings if re.search(r'_L[1-9]$', s if isinstance(s, str) else s.decode('latin1', 'replace'))))
PYEOF
)"
echo "  engine detail steps hidden for the bake: $HID (the model names $EXP)"
[ "$HID" = "$EXP" ] && ok "every _L detail step of the model was hidden, and nothing else" || bad "hidden $HID of the model's $EXP _L shapes"
grep -q "^mask " "$W/bake/${BASE}.txt" && echo "  subsurface mask decided by: $(grep "^mask " "$W/bake/${BASE}.txt" | cut -d' ' -f2)" || bad "the meta does not say how the mask was decided"
# THE EMISSIVE MULTIPLE: the sheet carries the colour, the .lodm carries the
# multiple (bungo: "carry the multiplier in lodm"). 0 here, and measured as a
# number: no shape of the near maple own-emits with a colour that is not black.
EMI="$(grep "^emissive " "$W/bake/${BASE}.txt" | head -1)"
echo "  meta emissive line: ${EMI:-<none>}"
[ -n "$EMI" ] && ok "the meta names the set's emissive multiple" || bad "no emissive line in the meta"
[ "$(echo "$EMI" | cut -d' ' -f2)" = "0" ] && ok "the legacy bake's multiple is 0 (nothing own-emits with a lit colour)" || bad "the legacy bake's emissive multiple is not 0: $EMI"
MLOD="$("$PY" - "$MESH" <<'PYEOF'
import sys, io, contextlib
sys.path.insert(0, "E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype")
import nifparse
with contextlib.redirect_stdout(io.StringIO()):
    data, hdr, strings, blocks = nifparse.parse(sys.argv[1])
print(sum(1 for i, t, s, z in blocks if t == 'BSMeshLODTriShape'))
PYEOF
)"
RNG="$(grep -c "^ranges " "$W/bake/${BASE}.txt")"
echo "  in-mesh detail ranges reduced to the first: $RNG (the model has $MLOD BSMeshLODTriShape)"
if [ "$MLOD" -ge 1 ]; then [ "$RNG" -ge 1 ] && ok "the bake draws only the first range of the in-mesh steps" || bad "a BSMeshLODTriShape kept its L1/L2 ranges in the bake"; fi

"$PY" - "$W/bake" "$BASE" gsaos g 64 <<'PYEOF'
import sys, hashlib
from PIL import Image
d, base, fam, emiSfx, TILE = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5])
octLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('oct ')][0]
clsLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('class ')]
N, TW, TH = int(octLine[1]), int(octLine[2]), int(octLine[3])
print('  frame: %d x %d px, %s x %s units, family %s' % (TW, TH, octLine[4], octLine[5], octLine[10]))
alb = Image.open(f'{d}/{base}_oct_albedo.png').convert('RGBA')
nrm = Image.open(f'{d}/{base}_oct_normal.png').convert('RGBA')
rm = Image.open(f'{d}/{base}_oct_{fam}.png').convert('RGBA')
emi = Image.open(f'{d}/{base}_oct_{emiSfx}.png').convert('RGBA')
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
check('sheets are N frames wide and tall', alb.size == (N*TW, N*TH) and nrm.size == alb.size and rm.size == alb.size and emi.size == alb.size)
# THE FRAME LAW (2026-09-09). With no WW_IMPOSTOR_REF there is no size ladder,
# so the long side is the run's resolution exactly. The short side is a MULTIPLE
# OF 16 in [16, long] -- and not just any one: the SMALLEST that does not crop
# the silhouette, which is what "maximizing the tree's size in each row and
# column" means. A frame one rung wider than necessary fails the fill check
# below, so this pair cannot both pass on a wrong ladder.
allowed = list(range(16, TILE + 1, 16))
print('  frame law: long %d (the run\'s %d), short %d, multiples of 16 allowed %s'
      % (max(TW, TH), TILE, min(TW, TH), allowed))
check('the long side is the run\'s resolution and the short side is a multiple of 16 within it',
      max(TW, TH) == TILE and min(TW, TH) in allowed)

# THE GAP, per axis, exact, and agreeing with the sidecar's own `gap` line.
# bungo's number is the DISTANCE BETWEEN TWO RENDERED OBJECTS (2026-09-09), so
# gap(side) = max(2, side/16) rounded UP to even is 8 texels between two
# silhouettes on a 1024 sheet of 128-texel frames, and the margin on EACH side
# of a frame is half of it. The inner rect is therefore `side - gap`: 15/16 of
# the frame on every side that is a multiple of 32.
def gapOf(side):
    g = max(2, side // 16)
    return g + (g & 1)
def padOf(side):
    return gapOf(side) // 2
GAPX, GAPY = gapOf(TW), gapOf(TH)
PADX, PADY = padOf(TW), padOf(TH)
gapLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('gap ')]
print('  gap: derived %d,%d  sidecar %s; margin per side %d,%d; inner rect %dx%d of %dx%d (%.4f x %.4f of the frame)'
      % (GAPX, GAPY, gapLine[0][1:3] if gapLine else 'ABSENT', PADX, PADY,
         TW - GAPX, TH - GAPY, TW, TH, (TW - GAPX) / TW, (TH - GAPY) / TH))
check('the meta names the gap on a line of its own, per axis',
      len(gapLine) == 1 and [int(gapLine[0][1]), int(gapLine[0][2])] == [GAPX, GAPY])
# and no `pad` line survives beside it: two spellings of one quantity is how a
# reader ends up applying the wrong law to the right number
check('the meta does not also carry the superseded per-side `pad` line',
      not [l for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('pad ')])
# bungo's own fraction, stated as arithmetic on the sides this bake produced.
# Exact wherever a side is a multiple of 32; a side of 48, 80 or 112 rounds the
# gap up to even and gives back at most one texel.
for _side, _gp in ((TW, GAPX), (TH, GAPY)):
    if _side % 32 == 0:
        check('the inner rect is exactly 15/16 of the %d-texel frame side' % _side,
              _side - _gp == _side * 15 // 16)
# and the meta says which run this was, so a downscaled frame is not mistaken
# for a differently-configured bake
check('the oct line names the run\'s resolution as its last token', int(octLine[11]) == TILE)
check('the meta names the class on a line of its own', len(clsLine) == 1 and [int(clsLine[0][1]), int(clsLine[0][2])] == [TW, TH])
def tile(im, i, j): return im.crop((i*TW, j*TH, (i+1)*TW, (j+1)*TH))
def covered(i, j): return [k for k, p in enumerate(tile(alb, i, j).getdata()) if p[3] >= 128]
hashes = {hashlib.md5(tile(alb, i, j).tobytes()).hexdigest() for j in range(N) for i in range(N)}
counts = [len(covered(i, j)) for j in range(N) for i in range(N)]
# coverage is judged against the INNER rect, `frame - gap`: the margins are
# transparent by design, and there is one gap's worth of them per axis
inner = (TW - GAPX) * (TH - GAPY)
print('  distinct albedo frames: %d of %d; covered pixels per frame: min %d max %d of %d inside the margins (best %.1f%%)' % (len(hashes), N*N, min(counts), max(counts), inner, 100.0*max(counts)/inner))
check('the views differ and none is empty', len(hashes) >= 8 and min(counts) >= 10)
check('the matte leaves the background transparent (no frame is fully covered)', max(counts) < inner)
# the fit: the frame follows the recorded extents' aspect (a sphere fit would be square), and a
# bare tree still covers something; the LOD maple's pre-baked crown gave 14%, the near maple's twigs 4-5%
halfW, halfH = float(octLine[4]), float(octLine[5])
# EXACT now: the extents are widened to the frame's aspect rather than the
# silhouette being stretched into it, so the recorded quad IS the frame
aspectOk = abs(TW / TH - halfW / halfH) <= 0.01 * (TW / TH) if halfH > 0 else False
print('  frame aspect %.4f, extents aspect %.4f' % (TW / TH, halfW / halfH))
check('the recorded extents carry the frame\'s aspect (the silhouette is given air, not stretched), best view >= 2% of the inner rect', aspectOk and max(counts) >= 0.02 * inner)
# THE PADDING IS EXACT ON ALL FOUR SIDES, and the frame is FILLED.
#
# Two halves, and both are needed: "no covered texel inside the padding" alone
# passes on a frame that is all padding, and "the silhouette is big" alone
# passes on a frame with no padding at all.
touch = 0
ux0, ux1, uy0, uy1 = TW, 0, TH, 0        # the union of the silhouette boxes
for j in range(N):
    for i in range(N):
        for k in covered(i, j):
            x, y = k % TW, k // TW
            if x < PADX or y < PADY or x >= TW - PADX or y >= TH - PADY:
                touch += 1
            ux0 = min(ux0, x); ux1 = max(ux1, x + 1)
            uy0 = min(uy0, y); uy1 = max(uy1, y + 1)
print('  covered texels inside the %d,%d-texel padding: %d' % (PADX, PADY, touch))
check('every frame keeps its padding clear on all four sides', touch == 0)
INNER_L, INNER_S = max(TW, TH) - 2 * (PADY if TH > TW else PADX), min(TW, TH) - 2 * (PADX if TH > TW else PADY)
unionL = (uy1 - uy0) if TH > TW else (ux1 - ux0)
unionS = (ux1 - ux0) if TH > TW else (uy1 - uy0)
print('  union of the %d silhouette boxes: %d x %d texels; inner rect %d x %d (long axis fill %.1f%%)'
      % (N * N, ux1 - ux0, uy1 - uy0, INNER_S, INNER_L, 100.0 * unionL / INNER_L))
# THE LONG AXIS IS FILLED. Not to 100%: pass one measures the silhouette in
# VIEWPORT pixels and pass two downsamples that into the frame, so an extremity
# a fraction of a texel wide falls under the coverage floor on the way in.
# Measured 89.3% at a 64 px frame and 94.6% at 128 px, so the floor is 85%.
check('the silhouette fills at least 85% of the inner rect on the long axis',
      unionL >= 0.85 * INNER_L)
check('and does not exceed it (the padding is not eaten)', unionL <= INNER_L and unionS <= INNER_S)
# THE SHORT AXIS IS AS NARROW AS IT CAN BE. A long-axis floor alone cannot see
# the defect this law was written for -- the old five-rung ladder left
# TreeBlasted05's silhouette in 4 texels of a 32-texel frame while its LONG axis
# was fine. The discriminating statement is that the frame is the SMALLEST
# multiple of 16 that does not crop, so ONE RUNG NARROWER WOULD HAVE CUT it.
# Stated as an AIR BUDGET rather than as tightness against the frame-resolution
# silhouette, because the two are not the same thing and the difference is not a
# defect: the frame is sized from pass one's VIEWPORT-resolution measurement, so
# that a faint extremity a fraction of a texel wide is never cropped, and the
# frame-resolution silhouette is therefore always a little smaller. What the law
# does promise is that the quantisation costs AT MOST ONE RUNG: 16 texels.
#
# The old five-rung ladder cannot pass this. TreeBlasted05 sat in a 32x32 frame
# (inner 24) with a 4-texel silhouette -- 20 texels of air, five rungs' worth.
SHORT = min(TW, TH)
print('  air on the short axis: inner %d, silhouette %d -> %d texels (one rung is 16)'
      % (INNER_S, unionS, INNER_S - unionS))
check('the short side carries at most ONE rung of air, which is the bound the quantisation itself sets',
      INNER_S - unionS < 16)

# MIP BLEED, ON THE GAP RULE. Frames never mix during CONSTRUCTION -- the box
# filter halves an even frame into an even frame -- so the bleed is at SAMPLE
# time: a tap ON a frame's UV border reads half of that frame's last texel and
# half of the neighbour's first. What has to survive at every shipped level is
# therefore the SEPARATION between the two silhouettes that meet on the border,
# measured across exactly those two texels as transparent coverage:
#
#   gap = (255 - alphaA)/255 + (255 - alphaB)/255      in texels
#
# and the promise is that it is still a whole texel at the deepest level shipped,
# which is what `mips = 1 + log2(min(gap))` buys. Only INTERIOR borders are
# measured: the sheet's outer border has no neighbouring frame beyond it and is
# sampled clamped, so half a gap is all it needs.
#
# The CONTROL is the same sheet with the margins stripped and the inner rects
# re-tiled edge to edge, and it MUST come out under a texel -- a check that
# cannot fail on its input is not a check.
MIPS = 1
g_ = min(GAPX, GAPY)
while g_ >= 2:
    g_ //= 2; MIPS += 1
def boxdown(px, w, h):
    w2, h2 = w // 2, h // 2
    out = []
    for y in range(h2):
        for x in range(w2):
            acc = [0, 0, 0, 0]
            for dy in (0, 1):
                for dx in (0, 1):
                    p = px[(y * 2 + dy) * w + (x * 2 + dx)]
                    for c in range(4):
                        acc[c] += p[c]
            out.append(tuple((v + 2) >> 2 for v in acc))
    return out, w2, h2
def min_gap(px, w, h, fw, fh, mips):
    worst, samples = 2.0, 0
    for k in range(mips):
        if k:
            px, w, h = boxdown(px, w, h)
        fwk, fhk = fw >> k, fh >> k
        if fwk < 2 or fhk < 2:
            continue
        for i in range(1, N):
            x = i * fwk
            if x < 1 or x >= w:
                continue
            for y in range(h):
                g = (255 - px[y * w + x - 1][3]) / 255.0 + (255 - px[y * w + x][3]) / 255.0
                worst = min(worst, g); samples += 1
        for j in range(1, N):
            y = j * fhk
            if y < 1 or y >= h:
                continue
            for x in range(w):
                g = (255 - px[(y - 1) * w + x][3]) / 255.0 + (255 - px[y * w + x][3]) / 255.0
                worst = min(worst, g); samples += 1
    return worst, samples
apx = list(alb.getdata())
mg, ns = min_gap(apx, alb.width, alb.height, TW, TH, MIPS)
print('  %d shipped mips (gap %d,%d): narrowest gap across an interior frame border %.3f texels, over %d border samples'
      % (MIPS, GAPX, GAPY, mg, ns))
check('every shipped mip keeps a whole texel of gap between the silhouettes that meet on a frame border',
      ns > 0 and mg >= 0.999)
IW, IH = TW - 2 * PADX, TH - 2 * PADY
NEAR = Image.Resampling.NEAREST if hasattr(Image, 'Resampling') else Image.NEAREST
cells = []
for j in range(N):
    row = []
    for i in range(N):
        ks = covered(i, j)
        xs = [k % TW for k in ks]; ys = [k // TW for k in ks]
        box = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
        row.append(list(tile(alb, i, j).crop(box).resize((IW, IH), NEAR).getdata()))
    cells.append(row)
ctl = []
for j in range(N):
    for y in range(IH):
        for i in range(N):
            ctl.extend(cells[j][i][y * IW:(y + 1) * IW])
edge = sum(1 for j in range(N) for i in range(N) for y in range(IH) for x in range(IW)
           if (x in (0, IW - 1) or y in (0, IH - 1)) and cells[j][i][y * IW + x][3] >= 128)
cg, cs = min_gap(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, every silhouette cropped to its own box and filling its cell: %d covered texels sit on a cell border; narrowest gap %.3f texels over %d samples (must be under 1)'
      % (edge, cg, cs))
check('the gap check FAILS on a sheet with no spacing at all (the metric can see it)',
      edge > 0 and cs > 0 and cg < 0.999)

# DILATION is NOT checked here. This is the BAKE's PNG and it is un-dilated by
# design -- the dilation is lodgenCard's, on the way into the DDS -- and a check
# here reads 355 black texels of 473 and is right to. It is measured further
# down, on the converted sheet's own block endpoints ("edge blocks in the colour
# sheet ... with a near-black endpoint: 0"), which is where the property exists.
def chan(im, i, j, c):
    px = list(tile(im, i, j).getdata()); return [px[k][c] for k in covered(i, j)]
def mean(v): return sum(v) / max(1, len(v))
# normal: X, Y in view space (opposite views mirror in red); measured, not assumed
diffs = [abs(mean(chan(nrm, i, j, 0)) - mean(chan(nrm, N-1-i, N-1-j, 0))) for j in range(N) for i in range(N)]
print('  normal X: opposite-view red difference max %.1f (view space if large)' % max(diffs))
check('the normal sheet differs between opposite views (not one frame copied)', max(diffs) > 5)
px11 = list(tile(nrm, 1, 1).getdata())
check('normals vary within a covered frame', len({px11[k][:2] for k in covered(1, 1)}) >= 32)
# normal blue = height, normal alpha = sway
hv = chan(nrm, 1, 1, 2)
print('  height distinct values %d in one frame' % len(set(hv)))
check('height varies over the covered pixels (normal blue)', len(set(hv)) >= 8)
a = tile(alb, 1, N-1); s = tile(nrm, 1, N-1)
rows = [y for y in range(TH) if any(a.getpixel((x, y))[3] >= 128 for x in range(TW))]
q = max(1, len(rows) // 4)
def meanG(ys): return mean([s.getpixel((x, y))[3] for y in ys for x in range(TW) if a.getpixel((x, y))[3] >= 128])
print('  sway (normal alpha): top quarter %.1f, bottom quarter %.1f' % (meanG(rows[:q]), meanG(rows[-q:])))
check('sway rises with height (normal alpha)', meanG(rows[:q]) > meanG(rows[-q:]) + 40)
# the mask sheet: R (gloss) and G (specular) both carry the vanilla _s map's texture, AO varies, the mask separates leaf cards from the trunk
rv = chan(rm, 1, N-1, 0); gv = chan(rm, 1, N-1, 1); av = chan(rm, 1, 1, 2)
print('  gloss (R) distinct values %d mean %.1f, specular (G) distinct values %d mean %.1f, AO distinct values %d' % (len(set(rv)), mean(rv), len(set(gv)), mean(gv), len(set(av))))
check('gloss carries the map (more than four values)', len(set(rv)) >= 4)
check('specular carries the map (more than four values)', len(set(gv)) >= 4)
check('AO varies over the covered pixels (mask sheet blue)', len(set(av)) >= 8)
r = tile(rm, 1, N-1)
def maskMean(ys): return mean([r.getpixel((x, y))[3] for y in ys for x in range(TW) if a.getpixel((x, y))[3] >= 128])
print('  subsurface mask: crown rows %.1f, trunk rows %.1f' % (maskMean(rows[:q]), maskMean(rows[-q:])))
check('the subsurface mask marks the leaf cards and not the trunk', maskMean(rows[:q]) > 128 and maskMean(rows[-q:]) < 128)
# THE EMISSIVE SHEET. The near maple is alpha-tested on every shape (measured in
# WW_CHANGES 2026-09-06h: branches at threshold 150, bark at 24), and an
# alpha-tested material spends its alpha on the cut-out, so under the vanilla
# glow rule it emits NOTHING. Measured as the maximum over the covered texels of
# every frame, not a mean: one lit texel would be a leak.
ep = emi.load(); ap = alb.load()
W0, H0 = emi.size
maxE, nCov = 0, 0
for y in range(H0):
    for x in range(W0):
        if ap[x, y][3] >= 16:
            nCov += 1
            maxE = max(maxE, ep[x, y][0], ep[x, y][1], ep[x, y][2])
print('  emissive over %d covered texels: max channel %d (the alpha-tested rule says 0)' % (nCov, maxE))
check('an alpha-tested model emits nothing (the emissive sheet is black wherever it is covered)', nCov >= 200 and maxE == 0)
sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

# the cards, filed by form ID as the driver files them, then the far chunk
for f in "$W/bake/${BASE}"*; do
	n="$(basename "$f")"; cp "$f" "$W/cards/${ID}${n#$BASE}"
done
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao --impostors "$W/cards" \
	--data-root "$DATA" -o "$W/chunk.bto" >/dev/null 2>&1
[ -s "$W/chunk.bto" ] || { bad "the far chunk was not written"; echo "RESULT FAIL"; exit 1; }
CL="$(grep -c "^C " "$W/chunk.bto.manifest.txt")"
echo "  C lines in the manifest: $CL"
[ "$CL" -ge 1 ] && grep -q "^C [0-9]* .* 4 .*${ID}_oct\.lodm$" "$W/chunk.bto.manifest.txt" && ok "the manifest's C line names the grid and the set's .lodm" || bad "no C line with N=4 ending in ${ID}_oct.lodm"
for s in "_oct_d.DDS" "_oct_n.DDS" "_oct_gsaos.DDS" "_oct_g.DDS" "_oct.lodm"; do
	[ -s "$W/cards/${ID}$s" ] && ok "${ID}$s written" || bad "${ID}$s missing"
done
for s in "_oct.DDS" "_oct_ds.DDS" "_oct_bc.DDS" "_oct_rmaos.DDS" "_oct_e.DDS"; do
	[ -e "$W/cards/${ID}$s" ] && bad "a wrong-family or old-name sheet was written: ${ID}$s"
done
"$PY" - "$W/cards" "$ID" legacy <<'PYEOF'
import struct, sys, json
d, ident, fam = sys.argv[1], sys.argv[2], sys.argv[3]
from PIL import Image
pbr = fam == 'pbr'
colorSfx, maskSfx = ('_bc', '_rmaos') if pbr else ('_d', '_gsaos')
colorKey, maskKey = ('baseColor', 'rmaos') if pbr else ('diffuse', 'gsaos')
emiSfx = '_e' if pbr else '_g'		# the emissive's key is `emissive` in both families
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
# the set's .lodm: envelope, family, the three sheets, the grid
b = open(f'{d}/{ident}_oct.lodm', 'rb').read()
ver, n = struct.unpack_from('<II', b, 4)
check('the .lodm envelope is LODM v1 with an exact payload size', b[:4] == b'LODM' and ver == 1 and n == len(b) - 12)
lm = json.loads(b[12:])
print('  lodm: %s' % json.dumps(lm, separators=(',', ':'))[:200])
game = 'Data\\Textures\\Lodgen\\Cards\\' + ident + '_oct'
check('the .lodm is a lodm 1 %s card' % fam, lm.get('lodm') == 1 and lm.get('family') == fam and lm.get('kind') == 'card')
tex = lm.get('textures', {})
check('the .lodm names the four sheets under the family\'s keys', tex.get(colorKey) == game + colorSfx + '.DDS' and tex.get('normal') == game + '_n.DDS' and tex.get(maskKey) == game + maskSfx + '.DDS' and tex.get('emissive') == game + emiSfx + '.DDS')
oct = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('oct ')][0]
tw, th = int(oct[2]), int(oct[3])
card = lm.get('card', {})
check('the .lodm carries the grid, the frame and the extents of the meta', card.get('oct') == 4 and card.get('frame') == [tw, th] and abs(card.get('half', [0, 0])[1] - float(oct[5])) < 1e-2 and card.get('depthSpan') == float(oct[9]))
# the two spacings, and which is which: `pad` is the margin on EACH side and
# `gap` is the distance between two neighbouring silhouettes, exactly twice it
def _gapOf(side):
    g = max(2, side // 16)
    return g + (g & 1)
def _padOf(side): return _gapOf(side) // 2
print('  .lodm spacing: pad %s (derived %s), gap %s (derived %s)'
      % (card.get('pad'), [_padOf(tw), _padOf(th)], card.get('gap'), [_gapOf(tw), _gapOf(th)]))
check('the .lodm records the per-side padding and the gap, and the gap is twice the padding',
      card.get('pad') == [_padOf(tw), _padOf(th)] and card.get('gap') == [_gapOf(tw), _gapOf(th)]
      and card.get('gap') == [2 * p for p in card.get('pad', [0, 0])])
# the multiple, at the .lodm's top level for a card set; the meta's own number
emi = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('emissive ')]
print('  emissiveScale in the card .lodm: %r (the meta said %r)'
      % (lm.get('emissiveScale'), emi[0][1] if emi else None))
check('the card .lodm carries the meta\'s emissive multiple at the top level',
      len(emi) == 1 and lm.get('emissiveScale') is not None
      and abs(float(lm['emissiveScale']) - float(emi[0][1])) < 1e-6)
check('a model whose shapes do not own-emit with a lit colour carries emissiveScale 0',
      float(lm.get('emissiveScale', -1)) == 0.0)
# dilation: the ring of texels just outside the silhouette carries the leaf's colour, not black - measured on the shipped BC3 sheet's colour endpoints
alb = Image.open(f'{d}/{ident}_oct_albedo.png').convert('RGBA')
W0, H0 = alb.size
px = alb.load()
ring = []
for y in range(1, H0 - 1):
    for x in range(1, W0 - 1):
        if px[x, y][3] >= 128:
            continue
        if any(px[x + dx, y + dy][3] >= 128 for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
            ring.append((x, y))
print('  ring texels outside the silhouette: %d' % len(ring))
b = open(f'{d}/{ident}_oct{colorSfx}.DDS', 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
bw = (w + 3) // 4
dark = 0; tested = 0
seen = set()
for (x, y) in ring:
    bx, by = x // 4, y // 4
    if (bx, by) in seen:
        continue
    seen.add((bx, by))
    off = 128 + (by * bw + bx) * 16 + 8		# past the alpha block: colour endpoints c0, c1
    c0, c1 = struct.unpack_from('<HH', b, off)
    def lum(c): return ((c >> 11) & 31) * 8 + ((c >> 5) & 63) * 4 + (c & 31) * 8
    tested += 1
    if min(lum(c0), lum(c1)) < 24:
        dark += 1
print('  edge blocks in the colour sheet: %d, with a near-black endpoint: %d' % (tested, dark))
check('the colour under the cut-out is dilated (no black endpoint at the edges)', tested > 0 and dark <= tested // 20)
# THE MIP CAP IS THE GAP'S: 1 + log2(min(gapX, gapY)) -- 4 levels on a 128-texel
# frame, 3 on a 64. What stood here counted levels until a frame spanned eight
# texels, a rule that had nothing to do with the spacing and agreed with the
# shipped count only by coincidence on this bake's frame shape.
expect, _g = 1, min(_gapOf(tw), _gapOf(th))
while _g >= 2:
    expect += 1; _g //= 2
okm = True
for s in (colorSfx, '_n', maskSfx):
    b = open(f'{d}/{ident}_oct{s}.DDS', 'rb').read()
    h, w = struct.unpack_from('<II', b, 12); mips = struct.unpack_from('<I', b, 28)[0]; four = b[84:88]
    print('  _oct%s.DDS: %dx%d %s mips %d (expected %d)' % (s, w, h, four.decode('latin1'), mips, expect))
    okm = okm and four == b'DXT5' and mips == expect
check('the sheets are BC3 and mip only while a whole texel of gap survives (mips %d)' % card.get('mips', -1), okm and card.get('mips') == expect)
# the emissive is BC1: no alpha to carry, and half the bytes of the other three
be = open(f'{d}/{ident}_oct{emiSfx}.DDS', 'rb').read()
he, we = struct.unpack_from('<II', be, 12); mipse = struct.unpack_from('<I', be, 28)[0]
def blockBytes(w, h, sz):
    total, mw, mh, n = 0, w, h, 0
    while n < mipse:
        total += ((mw + 3) // 4) * ((mh + 3) // 4) * sz; n += 1
        mw = max(4, mw // 2); mh = max(4, mh // 2)
    return total
print('  _oct%s.DDS: %dx%d %s mips %d, %d bytes (128 + %d expected)'
      % (emiSfx, we, he, be[84:88].decode('latin1'), mipse, len(be), blockBytes(we, he, 8)))
check('the emissive sheet is BC1 (DXT1) at the same size and mip cap, exactly header + its blocks',
      be[84:88] == b'DXT1' and we == w and he == h and mipse == expect and len(be) == 128 + blockBytes(we, he, 8))
sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

# ---------------------------------------------------------------- bake 2: pbr, from a source .lodm per material
# One .lodm per candidate the first bake looked for, in a loose Data root of its own:
# family pbr, its third texture AND its emissive the material's OWN DIFFUSE, so the
# alpha cut-out is the legacy bake's and both the raw mask sheet and the emissive
# sheet must equal the albedo sheet where all are covered (the retargets reached the
# photograph) while the mask differs from the legacy composition (the slot was read
# raw). The emissive here is the other side of bake 1's black: the same channel,
# writing something.
mkdir -p "$W/root" "$W/bake2" "$W/cards2"
"$PY" - "$W/bake/${BASE}.txt" "$W/root" "$DATA" <<'PYEOF'
import sys, os, json, struct
meta, root, data = sys.argv[1:4]
n = 0
for line in open(meta).read().splitlines():
    t = line.split()
    if len(t) < 4 or t[0] != 'lodm' or t[2] != 'none':
        continue
    cand, diffuse = t[1], t[3]
    # emissiveScale 2.5: a value no default could produce, so the card .lodm
    # carrying it proves the source's multiple travelled, not a fallback
    obj = {'lodm': 1, 'family': 'pbr', 'kind': 'source', 'emissiveScale': 2.5,
           'textures': {'rmaos': diffuse, 'emissive': diffuse}}
    payload = json.dumps(obj, separators=(',', ':')).encode()
    p = os.path.join(root, cand.replace('\\', '/'))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'wb').write(b'LODM' + struct.pack('<II', 1, len(payload)) + payload)
    n += 1
    print('  fixture %s -> %s' % (cand, obj['textures']))
sys.exit(0 if n else 1)
PYEOF
[ $? -eq 0 ] && ok "a pbr source .lodm written per candidate" || bad "no fixture .lodm could be written"
WW_LODGEN_DATA_ROOT="$(winpath "$W/root")" WW_IMPOSTOR_BAKE="$(winpath "$W/bake2")" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64 timeout 240 "$NS" "$(winpath "$MESH")" --port 45921 >/dev/null 2>&1
for s in albedo normal rmaos e; do
	[ -s "$W/bake2/${BASE}_oct_$s.png" ] || { bad "sheet ${BASE}_oct_$s.png was not written by the pbr bake"; ls "$W/bake2"; echo "RESULT FAIL"; exit 1; }
done
ok "the four pbr sheets were written (albedo, normal, rmaos, e)"
[ -e "$W/bake2/${BASE}_oct_g.png" ] && bad "a legacy-name emissive sheet was written by the pbr bake"
grep -qE "^oct 4 .* pbr 64$" "$W/bake2/${BASE}.txt" && ok "the meta's oct line says pbr and names the run's 64 px" || bad "no pbr oct line naming the run's resolution in the second meta"
EMI2="$(grep "^emissive " "$W/bake2/${BASE}.txt" | head -1)"
echo "  pbr meta emissive line: ${EMI2:-<none>}"
[ "$(echo "$EMI2" | cut -d' ' -f2)" = "2.5" ] && ok "the pbr bake takes its multiple from the source .lodm (2.5)" || bad "the pbr bake's emissive multiple is not the fixture's 2.5: $EMI2"
NF="$(grep -c "^lodm .* pbr " "$W/bake2/${BASE}.txt")"
[ "$NF" -eq "$NCAND" ] && ok "every candidate resolved its .lodm ($NF of $NCAND)" || bad "only $NF of $NCAND candidates resolved a .lodm"

"$PY" - "$W/bake" "$W/bake2" "$BASE" <<'PYEOF'
import sys
from PIL import Image
d1, d2, base = sys.argv[1:4]
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
a1 = Image.open(f'{d1}/{base}_oct_albedo.png').convert('RGBA'); a2 = Image.open(f'{d2}/{base}_oct_albedo.png').convert('RGBA')
m1 = Image.open(f'{d1}/{base}_oct_gsaos.png').convert('RGBA'); m2 = Image.open(f'{d2}/{base}_oct_rmaos.png').convert('RGBA')
e1 = Image.open(f'{d1}/{base}_oct_g.png').convert('RGBA'); e2 = Image.open(f'{d2}/{base}_oct_e.png').convert('RGBA')
check('the pbr bake has the legacy bake\'s sheet size', a1.size == a2.size and m1.size == m2.size == a1.size and e1.size == e2.size == a1.size)
p1, p2, q1, q2 = a1.load(), a2.load(), m1.load(), m2.load()
g1, g2 = e1.load(), e2.load()
W, H = a1.size
# a bare tree has few FULLY covered texels after the downsample: compare over texels at least a quarter covered
cov = [(x, y) for y in range(H) for x in range(W) if p1[x, y][3] >= 64 and p2[x, y][3] >= 64]
same = sum(1 for x, y in cov if p1[x, y][3] == p2[x, y][3])
print('  texels at least a quarter covered in both bakes: %d, with identical coverage: %d' % (len(cov), same))
check('the cut-out is the same in both bakes', len(cov) >= 200 and same >= 0.95 * len(cov))
# the retarget: the pbr mask sheet is the material's diffuse read raw - the colour sheet itself, both
# un-premultiplied by the texel's coverage (measured before the fix: raw was colour x coverage)
dRaw = sum(abs(q2[x, y][c] - p2[x, y][c]) for x, y in cov for c in range(2)) / max(1, 2 * len(cov))
# and not the legacy composition of the _s map
dLeg = sum(abs(q2[x, y][c] - q1[x, y][c]) for x, y in cov for c in range(2)) / max(1, 2 * len(cov))
print('  pbr mask vs colour sheet (R, G, premultiplied): %.2f; pbr mask vs legacy gsaos (R, G): %.2f (mean over the texels)' % (dRaw, dLeg))
check('a source .lodm retargets the slot and the bake reads it RAW (the mask sheet is the colour sheet)', dRaw <= 3.0)
check('the raw slot differs from the legacy composition', dLeg >= 4.0)
# THE EMISSIVE, both sides of the same channel. Bake 1: the near maple is
# alpha-tested on every shape, so the vanilla glow rule emits nothing and the
# legacy _g sheet is black. Bake 2: the fixture .lodm names the material's own
# diffuse as its emissive, so the glow slot is read RAW and the _e sheet is the
# colour sheet. Without the second half the first is a channel that never writes.
eMax1 = max(max(g1[x, y][:3]) for x, y in cov)
eRaw = sum(abs(g2[x, y][c] - p2[x, y][c]) for x, y in cov for c in range(3)) / max(1, 3 * len(cov))
eMean2 = sum(sum(g2[x, y][:3]) for x, y in cov) / max(1, 3 * len(cov))
print('  emissive: legacy bake max over the compared texels %d; pbr bake vs its colour sheet %.2f, mean level %.1f' % (eMax1, eRaw, eMean2))
check('the alpha-tested legacy bake emits nothing', eMax1 == 0)
check('a source .lodm\'s emissive is read RAW off the glow slot (the emissive sheet is the colour sheet)', eRaw <= 3.0 and eMean2 >= 8.0)
sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

for f in "$W/bake2/${BASE}"*; do
	n="$(basename "$f")"; cp "$f" "$W/cards2/${ID}${n#$BASE}"
done
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao --impostors "$W/cards2" \
	--data-root "$DATA" -o "$W/chunk2.bto" >/dev/null 2>&1
[ -s "$W/chunk2.bto" ] && grep -q "^C [0-9]* .* 4 .*${ID}_oct\.lodm$" "$W/chunk2.bto.manifest.txt" && ok "the pbr card set's chunk names the set's .lodm" || bad "no C line for the pbr card set"
for s in "_oct_bc.DDS" "_oct_n.DDS" "_oct_rmaos.DDS" "_oct_e.DDS" "_oct.lodm"; do
	[ -s "$W/cards2/${ID}$s" ] && ok "${ID}$s written" || bad "${ID}$s missing"
done
if [ -e "$W/cards2/${ID}_oct_d.DDS" ] || [ -e "$W/cards2/${ID}_oct_gsaos.DDS" ] || [ -e "$W/cards2/${ID}_oct_g.DDS" ]; then bad "a legacy-name sheet was written for the pbr set"; fi
"$PY" - "$W/cards2" "$ID" <<'PYEOF'
import struct, sys, json
d, ident = sys.argv[1], sys.argv[2]
b = open(f'{d}/{ident}_oct.lodm', 'rb').read()
lm = json.loads(b[12:])
game = 'Data\\Textures\\Lodgen\\Cards\\' + ident + '_oct'
tex = lm.get('textures', {})
ok = lm.get('family') == 'pbr' and tex.get('baseColor') == game + '_bc.DDS' and tex.get('rmaos') == game + '_rmaos.DDS' and tex.get('emissive') == game + '_e.DDS'
print(('  ok   ' if ok else '  FAIL ') + 'the pbr set\'s .lodm says pbr and names _bc/_rmaos/_e')
# the source .lodm's own multiple, carried to the set: 2.5, against bake 1's 0
okS = lm.get('emissiveScale') is not None and abs(float(lm['emissiveScale']) - 2.5) < 1e-6
print(('  ok   ' if okS else '  FAIL ') + 'the pbr set\'s .lodm carries the source\'s emissiveScale 2.5 (got %r)' % lm.get('emissiveScale'))
be = open(f'{d}/{ident}_oct_e.DDS', 'rb').read()
okE = be[84:88] == b'DXT1'
print(('  ok   ' if okE else '  FAIL ') + 'the pbr emissive sheet is BC1 (DXT1), fourCC %s' % be[84:88].decode('latin1'))
sys.exit(0 if (ok and okE and okS) else 1)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

# ------------------------------------------- bake 3: the SIZE ladder
# The ladder cannot be seen in one bake: it compares a base against the largest
# in the RUN. Photograph the same model claiming the run's largest is four times
# its size, which must take it down exactly two rungs, 64 -> 16... except the
# ladder floors at 32, so two rungs from 64 lands ON the floor. Use 256 so the
# rungs are 256/128/64 and the floor is nowhere near.
mkdir -p "$W/bake3"
REFX="$(awk -v e="${EXTENT:-0}" 'BEGIN{printf "%.1f", (e+0 > 0 ? e*4 : 100000)}')"
echo "  bake 3: the candidate's extent is ${EXTENT:-?} units, the run's largest is claimed at $REFX (four times)"
WW_IMPOSTOR_BAKE="$(winpath "$W/bake3")" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=256 WW_IMPOSTOR_REF="$REFX" \
	timeout 240 "$NS" "$(winpath "$MESH")" --port 45923 >/dev/null 2>&1
if [ -s "$W/bake3/${BASE}.txt" ]; then
	L3="$(grep "^oct " "$W/bake3/${BASE}.txt" | head -1)"
	echo "  bake 3 oct line: $L3"
	LONG3="$(echo "$L3" | awk '{ print ($3 > $4) ? $3 : $4 }')"
	BASE3="$(echo "$L3" | awk '{ print $12 }')"
	# a quarter the size is two halvings: 256 -> 64
	[ "$LONG3" = "64" ] && ok "a base a quarter of the run's largest drops two rungs (256 -> $LONG3)" \
		|| bad "the size ladder did not drop two rungs: long side $LONG3, expected 64"
	[ "$BASE3" = "256" ] && ok "the downscaled set still records the run's 256 px, so it is not read as another run" \
		|| bad "the oct line's base token is $BASE3, expected 256"
	# and the aspect rung still applies UNDER the size rung
	SHORT3="$(echo "$L3" | awk '{ print ($3 < $4) ? $3 : $4 }')"
	echo "  bake 3 frame: ${SHORT3} x ${LONG3}"
	[ "$SHORT3" -le "$LONG3" ] && [ "$SHORT3" -ge 16 ] && [ $(( SHORT3 % 16 )) -eq 0 ] \
		&& ok "the aspect quantisation applies beneath the size rung (short $SHORT3, a multiple of 16 within the long side)" \
		|| bad "the short side $SHORT3 is not a multiple of 16 in 16..$LONG3"
else
	bad "bake 3 wrote no meta"
fi

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
