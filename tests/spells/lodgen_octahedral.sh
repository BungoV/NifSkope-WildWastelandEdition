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
# `oct N tw th halfW halfH cx cy cz span family base conv` - neither the family
# nor the RUN's chosen resolution is the last token any more: the VIEW
# CONVENTION token is (2026-09-19, the azimuth repair). RE-BASED from
# `legacy 64$`, which was written when `base` ended the line. A set whose line
# lacks `spec1` was baked with the azimuth turned by 180 degrees.
grep -qE "^oct 4 .* legacy 64 spec1$" "$W/bake/${BASE}.txt" && ok "the meta's oct line says legacy, names the run's 64 px and declares the spec1 view convention" || bad "no legacy oct line naming the run's resolution and the spec1 convention in the meta"
NCAND="$(grep -c "^lodm .* none " "$W/bake/${BASE}.txt")"
echo "  .lodm candidates looked for: $NCAND"
[ "$NCAND" -ge 1 ] && ok "the meta names every .lodm candidate it looked for" || bad "no lodm candidate lines in the meta"
grep -q "^model " "$W/bake/${BASE}.txt" && ok "the meta names the model it photographed" || bad "no model line in the meta"
# THE CAMERA THE SHEET WAS PHOTOGRAPHED THROUGH, in the bake's own words and
# read back off the live GLView, not off what the hook asked for. Every extent
# this sidecar records is a world measurement taken off viewport pixels through
# ONE units-per-pixel constant, and only an orthographic camera makes that true;
# until 2026-09-10 nothing headless ever called setProjection and every card was
# drawn through a 60-degree perspective frustum while being measured as if it
# were not (lane HOOKCAM). The floor for this check is bake 4's perspective
# control below, which must say `persp` on the same line.
grep -qx "projection ortho" "$W/bake/${BASE}.txt" && ok "the meta says the sheet was photographed orthographically" || bad "the meta does not say 'projection ortho' (got: $(grep '^projection' "$W/bake/${BASE}.txt" | head -1))"
# 'orthofit <asked> <achieved> <persp 0|1>': the fit read back through the same
# accessor the frames are sized with, and the projection it was read through.
# The first two are true in either projection -- both are Dist/Zoom -- which is
# exactly why the third field exists.
OF="$(grep "^orthofit " "$W/bake/${BASE}.txt" | head -1)"
echo "  bake 1 $OF"
"$PY" - "$OF" <<'PYEOF'
import sys
f = sys.argv[1].split()
ask, got, persp = float(f[1]), float(f[2]), int(f[3])
ok = persp == 0 and abs(got - ask) <= 1e-3 * max(1.0, ask)
print(('  ok   ' if ok else '  FAIL ') + 'the fit was read back through an ORTHOGRAPHIC camera (asked %.4f, achieved %.4f, persp %d)' % (ask, got, persp))
sys.exit(0 if ok else 1)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))
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
# THE SHORT AXIS IS AS NARROW AS IT CAN BE, stated as the ladder's own law: the
# short side is the SMALLEST multiple of 16 in [16, long] whose inner rect is not
# narrower, in proportion, than the silhouette pass one measured -- so ONE RUNG
# NARROWER WOULD HAVE CROPPED IT.
#
# It was an AIR BUDGET in texels ("at most one rung, 16") until 2026-09-09
# evening, calibrated on the union of the frames' own silhouette boxes. That
# proxy stopped meaning what it says when PER-FRAME POSITIONING landed: the union
# used to be wider than any single view, because the views sat at different
# offsets inside the frame, and now it IS the widest single view -- so the same
# bake reads one texel narrower and the budget tips over its own boundary with
# nothing having got worse (measured: 29 texels of silhouette before, 28 after,
# against a bound of 16 texels of air). The law itself is checkable directly,
# because the `framefit` line records what the ladder was fed, so it is now
# checked directly and the air is printed as information.
#
# The old five-rung ladder cannot pass this either: TreeBlasted05 sat in a 32x32
# frame whose inner rect was 24 texels against a silhouette that needed 4, three
# rungs above the smallest that fits.
SHORT = min(TW, TH)
print('  air on the short axis: inner %d, silhouette %d -> %d texels'
      ' (information; the ladder\'s law is the check below)'
      % (INNER_S, unionS, INNER_S - unionS))
_ff = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('framefit ')]
if not _ff:
    check('the meta carries the framefit line the aspect ladder is checked from', False)
else:
    _sx, _sy = float(_ff[0][1]), float(_ff[0][2])
    LONG = max(TW, TH)
    wantRatio = min(_sx, _sy) / max(_sx, _sy)
    il = LONG - 2 * padOf(LONG)
    rungs = [(_s, (_s - 2 * padOf(_s)) / il) for _s in range(16, LONG + 1, 16)]
    fits = [r for r in rungs if r[1] >= wantRatio]
    smallest = fits[0][0] if fits else LONG
    below = [r for r in rungs if r[0] < smallest]
    print('  aspect ladder: silhouette %.1f x %.1f units wants a short/long inner ratio of %.4f;'
          ' rungs %s; smallest that does not crop %d, shipped %d'
          % (_sx, _sy, wantRatio, ['%d:%.3f' % r for r in rungs], smallest, SHORT))
    check('the short side is the SMALLEST multiple of 16 whose inner rect does not crop the measured silhouette',
          SHORT == smallest)
    # the floor on the other side: one rung narrower must actually crop, or the
    # statement above is one a frame of almost any width could satisfy
    check('and one rung narrower would have cropped it (the ladder sits on its floor, not merely on a rung)',
          not below or below[-1][1] < wantRatio)

# PER-FRAME POSITIONING (bungo, 2026-09-09 evening). Every frame shifts its own
# silhouette to its own centre, so the frame holds the WIDEST SINGLE VIEW rather
# than the union of all of them, and the shift is written to the sidecar as one
# `frameoff i j ox oy` line per frame -- model units, along that view's own right
# and up axes -- so a reader puts the quad back where the model was.
#
# Three statements, and each needs the others:
#   1. every frame's silhouette IS centred in its frame, to within a texel;
#   2. the sidecar's offsets, converted to texels and added back, would put those
#      centres where a fixed-centre bake had them -- and that spread must be MORE
#      than a texel, or the law bought nothing and the first check is vacuous;
#   3. the frame is sized from the widest single view, which the `framefit` line
#      reports beside the union the old law used.
foff = {}
for l in open(f'{d}/{base}.txt').read().splitlines():
    t = l.split()
    if t and t[0] == 'frameoff' and len(t) >= 5:
        foff[(int(t[1]), int(t[2]))] = (float(t[3]), float(t[4]))
clampLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('frameclamped ')]
fitLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('framefit ')]
check('the meta carries one frameoff line per frame, and none outside the grid', len(foff) == N * N)
check('the meta says how many frames had their crop pulled back inside the photograph, and it is none',
      len(clampLine) == 1 and int(clampLine[0][1]) == 0)
# units per texel, from the recorded FULL half extents against the whole frame
uptX = 2.0 * halfW / TW
uptY = 2.0 * halfH / TH
# measured at the bake's OWN coverage floor of 16/255, not at 128: the bake
# centred the floor-16 box, and judging it by a different threshold measures the
# thresholds' disagreement rather than the centring
def covered16(i, j): return [k for k, p in enumerate(tile(alb, i, j).getdata()) if p[3] >= 16]
nowOff, wasOff = [], []
for j in range(N):
    for i in range(N):
        ks = covered16(i, j)
        if not ks:
            continue
        xs = [k % TW for k in ks]; ys = [k // TW for k in ks]
        cx = 0.5 * (min(xs) + max(xs) + 1) - 0.5 * TW
        cy = 0.5 * (min(ys) + max(ys) + 1) - 0.5 * TH
        nowOff.append(max(abs(cx), abs(cy)))
        ox, oy = foff.get((i, j), (0.0, 0.0))
        # the sidecar's y is UP-positive; the image's y runs down
        wasOff.append(max(abs(cx + ox / uptX), abs(cy - oy / uptY)))
print('  per-frame centring: silhouette box centre off its frame centre, worst of %d frames: %.2f texels now;'
      ' %.2f texels with the recorded offsets added back (what a fixed-centre bake had)'
      % (len(nowOff), max(nowOff) if nowOff else -1, max(wasOff) if wasOff else -1))
# THE TOLERANCE, stated before the run. Pass one measures in VIEWPORT pixels and
# pass two downsamples into the frame, so an extremity a fraction of a texel wide
# can drop under the floor on the way in and move the frame-resolution box by
# about a texel a side -- half of that on a centre. 1.5 texels is that, plus
# rounding, and nothing else.
check('every frame\'s silhouette is centred in its own frame, within 1.5 texels', nowOff and max(nowOff) <= 1.5)
# THE CONTROL for that check: undoing the shift must make the centring measurably
# WORSE, or "the frames are centred" is a statement the metric cannot fail on.
check('and the recorded offsets are real: putting them back moves a frame more than a texel further off centre',
      wasOff and max(wasOff) > max(nowOff) + 1.0)
if fitLine:
    sx, sy, ux, uy = (float(v) for v in fitLine[0][1:5])
    print('  framefit: widest single view %.1f x %.1f units, union about the centre %.1f x %.1f'
          ' -> the frame is %.1f%% / %.1f%% narrower on x / y than a fixed-centre bake would need'
          % (sx, sy, ux, uy, 100.0 * (1 - sx / ux), 100.0 * (1 - sy / uy)))
check('the meta reports what the frame was sized from and what a fixed centre would have needed',
      len(fitLine) == 1 and float(fitLine[0][1]) <= float(fitLine[0][3]) + 1e-3
      and float(fitLine[0][2]) <= float(fitLine[0][4]) + 1e-3)

# MIP BLEED, ON THE GAP RULE, ONE LEVEL SHALLOWER (bungo, 2026-09-09 evening:
# "SHIP ONE MIP FEWER: mips = log2(gap) so the deepest shipped level still has a
# full texel of margin per side"). Frames never mix during CONSTRUCTION -- the box
# filter halves an even frame into an even frame -- so the bleed is at SAMPLE
# time: a tap ON a frame's UV border reads half of that frame's last texel and
# half of the neighbour's first. What it picks up of the NEIGHBOUR is decided by
# the margin inside each frame, gap/2, so the chain stops while
#
#   gap / 2^(k+1) >= 1            i.e.   mips = log2( min(gap) )
#
# and not one level later, which is where each margin is half a texel and a
# border tap does reach the neighbour's edge. Two measurements, both on the
# shipped levels: the SEPARATION between the two silhouettes, across the two
# texels a border tap reads,
#
#   gap = (255 - alphaA)/255 + (255 - alphaB)/255      in texels
#
# which must stay at two whole texels now, and the NEIGHBOUR ALPHA a tap actually
# picks up, which must be ZERO. Only INTERIOR borders are measured: the sheet's
# outer border has no neighbouring frame beyond it and is sampled clamped, so
# half a gap is all it needs.
#
# The CONTROL is the same sheet with the margins stripped and the inner rects
# re-tiled edge to edge, and it MUST come out under a texel -- a check that
# cannot fail on its input is not a check.
MIPS = 0
g_ = min(GAPX, GAPY)
while g_ >= 2:
    g_ //= 2; MIPS += 1
MIPS = max(1, MIPS)
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
def border_alpha(px, w, h, fw, fh, mips):
    """what a bilinear tap taken ON an interior frame border picks up of the
    NEIGHBOUR: half the alpha of the texel on the other side. Zero is the whole
    point of shipping one mip fewer, so it is measured as well as the gap."""
    worst, samples = 0, 0
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
                worst = max(worst, px[y * w + x - 1][3], px[y * w + x][3]); samples += 1
        for j in range(1, N):
            y = j * fhk
            if y < 1 or y >= h:
                continue
            for x in range(w):
                worst = max(worst, px[(y - 1) * w + x][3], px[y * w + x][3]); samples += 1
    return worst, samples
apx = list(alb.getdata())
mg, ns = min_gap(apx, alb.width, alb.height, TW, TH, MIPS)
ba, bs_ = border_alpha(apx, alb.width, alb.height, TW, TH, MIPS)
print('  %d shipped mips (gap %d,%d): narrowest gap across an interior frame border %.3f texels, over %d border samples;'
      ' worst neighbour alpha a border tap picks up %d/255' % (MIPS, GAPX, GAPY, mg, ns, ba))
check('every shipped mip keeps a whole texel of gap between the silhouettes that meet on a frame border',
      ns > 0 and mg >= 0.999)
# and the point of the shallower chain: a border tap reads NOTHING of its
# neighbour, at every level shipped. The level that was dropped is the one where
# each margin is half a texel and this number stops being zero.
check('no shipped mip lets a border tap pick up any of the neighbouring frame (zero cross-frame bleed)',
      bs_ > 0 and ba == 0)
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
ca, cas = border_alpha(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, every silhouette cropped to its own box and filling its cell: %d covered texels sit on a cell border; narrowest gap %.3f texels over %d samples (must be under 1); worst border alpha %d/255 (must be above 0)'
      % (edge, cg, cs, ca))
check('the gap check FAILS on a sheet with no spacing at all (the metric can see it)',
      edge > 0 and cs > 0 and cg < 0.999)
check('the border-alpha check FAILS on that same sheet (this metric can see it too)',
      cas > 0 and ca > 0)

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
game = 'Data\\FO4CSLOD\\Cards\\' + ident + '_oct'
check('the .lodm is a lodm 1 %s card' % fam, lm.get('lodm') == 1 and lm.get('family') == fam and lm.get('kind') == 'card')
tex = lm.get('textures', {})
check('the .lodm names the four sheets under the family\'s keys', tex.get(colorKey) == game + colorSfx + '.DDS' and tex.get('normal') == game + '_n.DDS' and tex.get(maskKey) == game + maskSfx + '.DDS' and tex.get('emissive') == game + emiSfx + '.DDS')
oct = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('oct ')][0]
tw, th = int(oct[2]), int(oct[3])
card = lm.get('card', {})
check('the .lodm carries the grid, the frame and the extents of the meta', card.get('oct') == 4 and card.get('frame') == [tw, th] and abs(card.get('half', [0, 0])[1] - float(oct[5])) < 1e-2 and card.get('depthSpan') == float(oct[9]))
# THE CAMERA reaches the .lodm, in the sidecar's own word. It is what says the
# `half`, `center` and `frameOffset` above describe the sheet beside them: they
# are world measurements taken off viewport pixels through one units-per-pixel
# constant, which only an orthographic camera makes true. The floor is bake 4's
# perspective control and the card-array harness's second layer, which carries
# no such key because its sidecar names no camera.
check('the .lodm names the camera the sheet was photographed through, and it is orthographic'
      ' (got %r)' % card.get('projection'), card.get('projection') == 'ortho')
# THE VIEW CONVENTION reaches the .lodm, in the sidecar's own word (2026-09-19,
# the azimuth repair). It is what lets a consumer tell a set baked with frame
# (i,j) holding the view from direction (i,j) -- the spec's law -- from one
# baked before the repair, whose frames sit 180 degrees of azimuth away. The
# floor is every set on disk before this exe: they carry no key at all, and the
# card-array harness's synthetic legacy sidecar is kept that way on purpose.
check('the meta\'s oct line carries the view convention as its thirteenth token'
      ' (got %r)' % (oct[12] if len(oct) > 12 else None),
      len(oct) > 12 and oct[12] == 'spec1')
check('the .lodm carries the view convention the sidecar stated'
      ' (got %r)' % card.get('conv'), card.get('conv') == 'spec1')
# F5 (lane CARDWIDTH, 2026-09-10): THE COVERAGE CONTRACT reaches the .lodm, in the
# sidecar's own three numbers. It is what says which alpha a consumer must test at
# to draw the silhouette `half` and `frameOffset` describe -- without it a
# consumer's own 0.5 draws a smaller tree than the mesh it replaced. The floor is
# the same absence as the camera's: a set from before the line carries no key.
_cov = card.get('coverage')
_covMeta = [l.split()[1:4] for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('coverage ')]
print('  .lodm coverage: %r (the meta said %r)' % (_cov, _covMeta))
check('F5: the .lodm carries the coverage contract the sidecar stated',
      isinstance(_cov, dict) and len(_covMeta) == 1
      and [_cov.get('floor'), _cov.get('test'), _cov.get('base')]
          == [int(v) for v in _covMeta[0]])
check("F5: a consumer testing at the contract's `test` selects the set floored at `floor`",
      _cov.get('test') == 128 and _cov.get('floor') == 16 and _cov.get('base') == 160)
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
# PER-FRAME POSITIONING reaches the .lodm, in the frames' own sheet order: two
# numbers per frame, frame (i,j) at index j*oct + i, and they are the sidecar's
# own numbers rather than a re-derivation.
octN_ = int(oct[1])
sidecar = {}
for l in open(f'{d}/{ident}.txt').read().splitlines():
    t = l.split()
    if t and t[0] == 'frameoff' and len(t) >= 5:
        sidecar[(int(t[1]), int(t[2]))] = (float(t[3]), float(t[4]))
fo = card.get('frameOffset')
same = (isinstance(fo, list) and len(fo) == 2 * octN_ * octN_
        and all(abs(fo[2 * (j * octN_ + i)] - sidecar[(i, j)][0]) < 1e-3
                and abs(fo[2 * (j * octN_ + i) + 1] - sidecar[(i, j)][1]) < 1e-3
                for j in range(octN_) for i in range(octN_) if (i, j) in sidecar))
print('  .lodm frameOffset: %d numbers for %d frames; largest |offset| %.2f units'
      % (len(fo) if isinstance(fo, list) else -1, octN_ * octN_,
         max((abs(v) for v in fo), default=-1.0) if isinstance(fo, list) else -1.0))
check('the .lodm carries the per-frame offsets, one pair per frame in sheet order, as the sidecar wrote them',
      len(sidecar) == octN_ * octN_ and same)
# ... and they are not all zero, which is what a set from before the law means
check('the per-frame offsets are not all zero (the shift actually happened)',
      isinstance(fo, list) and max((abs(v) for v in fo), default=0.0) > 0.0)
# THE OFFSETS AGREE WITH THE PICTURE THEY SHIFTED, exactly.
#
# For view v the silhouette spans [off - halfV, off + halfV] about the camera
# centre, so max(|x0|, |x1|) is |off| + halfV -- and the UNION half-extent the
# `framefit` line records is the largest of those over all N^2 views. So for
# EVERY frame, |off| plus that frame's own half box (read off the sheet) must not
# exceed the union, and for at least ONE frame it must reach it. An offset with
# the wrong sign, the wrong scale, or from a stale bake breaks the first; a set of
# zeros breaks the second.
#
# This ties the written field to the picture. The transition rule against an
# INDEPENDENT reader -- the model's own declared bound spheres, extended to the
# most displaced frame, with the zeroed control that must fail -- is
# `scratchpad/cardfinal_20260909/transition_bounds.py`, which needs the model and
# is run by the lane rather than here.
#
# A quarter of the card's half extent stood here for one run of this harness and
# was wrong: it was a fraction chosen without measurement, and the maple's TOP
# view legitimately sits 239 units off centre (28.8%), because a canopy's plan
# view is not centred on the trunk.
ffL = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('framefit ')]
albS = Image.open(f'{d}/{ident}_oct_albedo.png').convert('RGBA')
uX, uY = (float(ffL[0][3]), float(ffL[0][4])) if ffL else (0.0, 0.0)
th_ = int(oct[3])
uptX_, uptY_ = 2.0 * card['half'][0] / tw, 2.0 * card['half'][1] / th_
reach, over = 0.0, 0
for j in range(octN_):
    for i in range(octN_):
        cell = albS.crop((i * tw, j * th_, (i + 1) * tw, (j + 1) * th_))
        ks = [k for k, p in enumerate(cell.getdata()) if p[3] >= 16]
        if not ks:
            continue
        xs = [k % tw for k in ks]
        ys = [k // tw for k in ks]
        hX = 0.5 * (max(xs) - min(xs)) * uptX_
        hY = 0.5 * (max(ys) - min(ys)) * uptY_
        ox, oy = fo[2 * (j * octN_ + i)], fo[2 * (j * octN_ + i) + 1]
        rx, ry = (abs(ox) + hX) / max(uX, 1e-6), (abs(oy) + hY) / max(uY, 1e-6)
        reach = max(reach, rx, ry)
        if rx > 1.02 or ry > 1.02:
            over += 1
print('  offsets against the picture: union half-extent %.1f x %.1f units;'
      ' worst frame reaches %.3f of it; frames past it: %d' % (uX, uY, reach, over))
check('every frame\'s offset plus its own silhouette stays inside the extent a fixed-centre card spanned',
      uX > 0 and uY > 0 and over == 0)
check('and at least one frame REACHES that extent (a sheet of zero offsets could not)', reach >= 0.90)
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
# THE MIP CAP IS log2(min(gapX, gapY)) -- 3 levels on a 128-texel frame at gap 8,
# 2 on a 64 at gap 4. ONE FEWER than the cap of the morning of 2026-09-09, which
# shipped the level where each margin is half a texel and a border tap therefore
# reaches the neighbour (bungo, that evening: "SHIP ONE MIP FEWER ... so the
# deepest shipped level still has a full texel of margin per side"). Floored at
# one level, for a 16-texel frame whose gap is already on its floor of 2.
expect, _g = 0, min(_gapOf(tw), _gapOf(th))
while _g >= 2:
    expect += 1; _g //= 2
expect = max(1, expect)
okm = True
for s in (colorSfx, '_n', maskSfx):
    b = open(f'{d}/{ident}_oct{s}.DDS', 'rb').read()
    h, w = struct.unpack_from('<II', b, 12); mips = struct.unpack_from('<I', b, 28)[0]; four = b[84:88]
    # IMPOSTORDEPTH2 (2026-09-23, bungo): the `_n` is DX10 BC7_UNORM (dxgi 98, arraySize 1);
    # the colour and mask sheets stay DXT5. Was: all three DXT5.
    dx = struct.unpack_from('<5I', b, 128) if four == b'DX10' else (0, 0, 0, 0, 0)
    print('  _oct%s.DDS: %dx%d %s%s mips %d (expected %d)' % (s, w, h, four.decode('latin1'), (' dxgi %d arraySize %d' % (dx[0], dx[3])) if dx[0] else '', mips, expect))
    if s == '_n':
        okm = okm and four == b'DX10' and dx[0] == 98 and dx[1] == 3 and dx[3] == 1 and mips == expect
    else:
        okm = okm and four == b'DXT5' and mips == expect
check('the colour and mask sheets are BC3, the _n is BC7 (DX10 dxgi 98), and they mip only while a whole texel of gap survives (mips %d)' % card.get('mips', -1), okm and card.get('mips') == expect)
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
# RE-BASED with the legacy line above: the conv token now ends the oct line.
grep -qE "^oct 4 .* pbr 64 spec1$" "$W/bake2/${BASE}.txt" && ok "the meta's oct line says pbr, names the run's 64 px and declares the spec1 view convention" || bad "no pbr oct line naming the run's resolution and the spec1 convention in the second meta"
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
game = 'Data\\FO4CSLOD\\Cards\\' + ident + '_oct'
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


# ------------------------------------ bake 4: THE ORTHOGRAPHIC CAMERA, ON A CUBE
#
# The bake sizes every frame with orthographicHalfHeight() -- Dist / Zoom --
# and converts a silhouette from viewport pixels to world units with ONE
# units-per-pixel constant. Both are statements about an ORTHOGRAPHIC camera,
# and until 2026-09-10 the headless renderer never had one: restoreUi()
# hard-codes a 60-degree perspective and nothing in the bake called
# setProjection(). The name `orthographicHalfHeight` is not a measurement, which
# is why the read-back beside the fit never caught it.
#
# A CUBE is the fixture because its orthographic silhouette is arithmetic. For a
# box of half-extents h about its own centre, seen along a view whose screen
# RIGHT and UP axes are r and u,
#
#     halfR = hx|rx| + hy|ry| + hz|rz|        halfU = hx|ux| + hy|uy| + hz|uz|
#
# and the bake's own camera law -- setRotation( -90 + elev, 0, 90 - azim ) --
# makes those axes, through Matrix::fromEuler with y = 0,
#
#     r = ( sin azim, -cos azim, 0 )
#     u = ( sin elev cos azim, sin elev sin azim, cos elev )
#
# The frame spans 2 * fullHalfW by 2 * fullHalfH world units over tw by th
# texels (the meta's oct line), so the predicted span is 2*halfR*tw /
# (2*fullHalfW) texels, in EVERY frame and at every depth. That last clause is
# the whole point: a perspective frame magnifies by eye / (eye - d) for a
# point d in front of the card plane, so it cannot hold.
#
# Nothing here is typed. The cube's own half-extent is measured through the
# PINNED ORTHOGRAPHIC camera of the render hook (WW_RENDER_ORTHO, whose gate is
# tests/spells/render_shot.sh section 7, 27 of 27 on 2026-09-10) -- a
# different code path from the bake, so this is not our own output judging our
# own output. The floor is the PERSPECTIVE CONTROL: the same cube baked with
# WW_IMPOSTOR_PERSP=1, which must fail every check this one passes.
CUBE="$W/cube512.nif"
"$NS" -no-gui new -o "$(winpath "$CUBE")" --cube --size 512 >/dev/null 2>&1
if [ ! -s "$CUBE" ]; then
	bad "the 512-unit cube fixture was not written by -no-gui new --cube --size 512"
else
	ok "the 512-unit cube fixture was written ($(ls -l "$CUBE" | awk '{print $5}') bytes)"
	# (a) the fixture's own size, through the hook's pinned orthographic camera.
	# The same recipe as render_shot.sh section 7, whose own gate proves this
	# camera: the fixture's centre is (0,0,256) -- the cube spans z 0..512, not
	# -256..256 -- and half-width 512 over a 640 px viewport puts 512 units of
	# cube across 320 px, so one pixel of edge is 1.6 units.
	WW_RENDER_SHOT="$(winpath "$W/cube_front.png")" WW_RENDER_VIEW=5 WW_RENDER_ORTHO=512 \
		WW_RENDER_DIST=1000 WW_RENDER_CENTER=0,0,256 WW_RENDER_SIZE=640x480 \
		WW_RENDER_CLEAN=1 WW_CAMERA_CENSUS="$(winpath "$W/cubecam.log")" \
		timeout 240 "$NS" "$(winpath "$CUBE")" --port 45924 >/dev/null 2>&1
	CUBEHALF="$("$PY" - "$W/cube_front.png" "$W/cubecam.log" <<'PYEOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
W, H = im.size
px = im.load()
bg = px[2, 2]
# the silhouette is anything that is not the viewport clear colour; the cube is
# lit grey on a dark ground, so a plain difference threshold separates them and
# the FLOOR is that the box must not be the whole frame or a single pixel
x0, x1 = W, -1
for y in range(H):
    for x in range(W):
        c = px[x, y]
        if abs(c[0]-bg[0]) + abs(c[1]-bg[1]) + abs(c[2]-bg[2]) > 24:
            if x < x0: x0 = x
            if x > x1: x1 = x
upp = None
for line in open(sys.argv[2]):
    for t in line.split():
        if t.startswith('upp='):
            upp = float(t[4:])
if upp is None or x1 < x0 or (x1 - x0 + 1) >= W - 2 or (x1 - x0 + 1) < 8:
    sys.stderr.write('cube span unreadable: %d..%d of %d, upp=%r\n' % (x0, x1, W, upp))
    print('0')
else:
    print('%.4f' % (0.5 * (x1 - x0 + 1) * upp))
PYEOF
)"
	echo "  the cube fixture measured through WW_RENDER_ORTHO=512: half-extent $CUBEHALF units (512 asked of the CLI)"
	# 3 units of tolerance: the threshold instrument counts the antialiased edge
	# pixel on each side, and each is worth 1.6 units at this scale
	AWKOK="$(awk -v h="$CUBEHALF" 'BEGIN{ d = h - 256; if (d < 0) d = -d; print (h > 0 && d <= 3.0) ? 1 : 0 }')"
	[ "$AWKOK" = "1" ] && ok "the pinned orthographic camera measures the fixture at 256 units, the size the CLI was asked for" \
		|| bad "the fixture measures $CUBEHALF units, not 256 -- the fixture or the pinned camera is wrong, and the frames below cannot be predicted"
	# (b) the bake, and (c) the perspective control
	mkdir -p "$W/bake4" "$W/bake4p"
	WW_IMPOSTOR_BAKE="$(winpath "$W/bake4")" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE=64 \
		timeout 240 "$NS" "$(winpath "$CUBE")" --port 45925 >/dev/null 2>&1
	WW_IMPOSTOR_PERSP=1 WW_IMPOSTOR_BAKE="$(winpath "$W/bake4p")" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE=64 \
		timeout 240 "$NS" "$(winpath "$CUBE")" --port 45926 >/dev/null 2>&1
	grep -qx "projection ortho" "$W/bake4/cube512.txt" && ok "the cube bake says orthographic" || bad "the cube bake does not say 'projection ortho'"
	grep -qx "projection persp" "$W/bake4p/cube512.txt" && ok "the perspective CONTROL says perspective, so the switch and the line both move" \
		|| bad "the WW_IMPOSTOR_PERSP control does not say 'projection persp' -- the control is not a control"
	"$PY" - "$W/bake4" "$W/bake4p" "$CUBEHALF" <<'PYEOF'
import sys, math
from PIL import Image

fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond:
        fails += 1

def meta(d):
    m = {}
    for line in open(d + '/cube512.txt'):
        f = line.split()
        if f:
            m.setdefault(f[0], []).append(f[1:])
    return m

def axes(i, j, n):
    u = i / (n - 1) * 2.0 - 1.0
    v = j / (n - 1) * 2.0 - 1.0
    dx, dy = (u + v) * 0.5, (u - v) * 0.5
    dz = 1.0 - abs(dx) - abs(dy)
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    dx, dy, dz = dx / L, dy / L, dz / L
    elev = math.asin(max(-1.0, min(1.0, dz)))
    azim = math.atan2(dy, dx)
    r = (math.sin(azim), -math.cos(azim), 0.0)
    up = (math.sin(elev) * math.cos(azim), math.sin(elev) * math.sin(azim), math.cos(elev))
    return r, up

def mask(sheet, i, j, tw, th, thr=16, dec=None, dilate=0):
    # `dec` = (floor, base) off the bake's own `coverage` line. The sheet stores a
    # MEASURED COVERAGE FRACTION re-encoded as 0 or as a byte in [base,255]; with
    # `dec` given the stored byte is decoded back to that fraction on 0..255
    # BEFORE `thr` is applied, so a caller can ask about HALF COVERAGE instead of
    # asking about the stored byte. Lane GATEFIX1, 2026-09-19.
    px = sheet.load()
    out = []
    for y in range(th):
        row = []
        for x in range(tw):
            a = px[i * tw + x, j * th + y][3]
            if dec is not None:
                f, b = dec
                a = 0 if a < b else f + int(round((a - b) * (255.0 - f) / (255.0 - b)))
            row.append(1 if a >= thr else 0)
        out.append(row)
    # `dilate` grows the mask by that many 8-neighbour rings. Used ONLY by the red
    # control F2b: a silhouette one texel too wide on every side is the defect F1
    # exists to catch, and it must not clear F1's bar.
    for _ in range(dilate):
        prev = [r[:] for r in out]
        for y in range(th):
            for x in range(tw):
                if prev[y][x]:
                    continue
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        yy, xx = y + dy, x + dx
                        if 0 <= yy < th and 0 <= xx < tw and prev[yy][xx]:
                            out[y][x] = 1
    return out

def bbox(m):
    ys = [y for y, row in enumerate(m) if any(row)]
    if not ys:
        return None
    xs = [x for row in m for x, v in enumerate(row) if v]
    return min(xs), max(xs), min(ys), max(ys)

def measure(d, half, thr=16, dec=None, dilate=0):
    m = meta(d)
    o = m['oct'][0]
    n, tw, th = int(o[0]), int(o[1]), int(o[2])
    fw, fh = float(o[3]), float(o[4])
    sheet = Image.open(d + '/cube512_oct_albedo.png').convert('RGBA')
    rows = []
    for j in range(n):
        for i in range(n):
            r, up = axes(i, j, n)
            hr = half * (abs(r[0]) + abs(r[1]) + abs(r[2]))
            hu = half * (abs(up[0]) + abs(up[1]) + abs(up[2]))
            px = 2.0 * hr * tw / (2.0 * fw)
            py = 2.0 * hu * th / (2.0 * fh)
            mk = mask(sheet, i, j, tw, th, thr, dec, dilate)
            bb = bbox(mk)
            if bb is None:
                rows.append((i, j, px, py, 0.0, 0.0, 1.0, 1.0))
                continue
            x0, x1, y0, y1 = bb
            mx = x1 - x0 + 1
            my = y1 - y0 + 1
            # CENTRAL SYMMETRY: an orthographic projection of a centrally
            # symmetric solid is centrally symmetric. A perspective one is not --
            # the near half is magnified. Disagreement of the mask with its own
            # 180-degree rotation about the silhouette box, as a fraction of the
            # covered texels.
            cov = sum(sum(row) for row in mk)
            dis = 0
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    xr, yr = x0 + x1 - x, y0 + y1 - y
                    if mk[y][x] != mk[yr][xr]:
                        dis += 1
            asym = dis / max(1, cov)
            # NEAR EDGE vs FAR EDGE: the widest row in the top fifth of the
            # silhouette against the widest row in the bottom fifth. Equal under
            # an orthographic camera by the symmetry above; under perspective the
            # edge nearer the eye is wider.
            band = max(1, my // 5)
            wt = max(sum(mk[y]) for y in range(y0, y0 + band))
            wb = max(sum(mk[y]) for y in range(y1 - band + 1, y1 + 1))
            rows.append((i, j, px, py, mx, my, asym, abs(wt - wb) / max(1.0, 0.5 * (wt + wb))))
    return n, tw, th, fw, fh, rows

half = float(sys.argv[3])
n, tw, th, fw, fh, ortho = measure(sys.argv[1], half)
_, _, _, pfw, pfh, persp = measure(sys.argv[2], half)
# THE READER'S OWN THRESHOLD (lane CARDWIDTH, 2026-09-10). Everything above reads
# the sheet at the bake's coverage floor, 16/255. What a consumer DRAWS is the set
# it alpha-tests, and both specs tell it to test at 0.5 -- so the fixture that
# matters to bungo's rule ("the tree won't change position", and the same size) is
# the cube's span at 128/255. Before the coverage re-encoding a bare crown lost up
# to 5.41 texels of half-width between those two sets; a cube is solid, so this is
# the arithmetic half of that fixture and the bar is ONE texel, pre-registered.
_, _, _, _, _, orthoR = measure(sys.argv[1], half, 128)
_, _, _, _, _, perspR = measure(sys.argv[2], half, 128)
print('  frame %dx%d texels, %.2f x %.2f units half-extent; %d frames' % (tw, th, fw, fh, len(ortho)))
print('  i j | predicted x,y | ortho measured x,y | persp measured x,y')
for k in range(len(ortho)):
    i, j, px, py, mx, my, asym, ne = ortho[k]
    _, _, ppx, ppy, pmx, pmy, pasym, pne = persp[k]
    print('  %d %d | %6.2f %6.2f | %4d %4d | %4d %4d' % (i, j, px, py, mx, my, pmx, pmy))
dO = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in ortho)
dP = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in persp)
aO = max(r[6] for r in ortho)
aP = max(r[6] for r in persp)
nO = max(r[7] for r in ortho)
nP = max(r[7] for r in persp)
print('  worst |measured - predicted| over the %d frames: ORTHO %.2f texels, PERSPECTIVE CONTROL %.2f texels' % (len(ortho), dO, dP))
print('  worst central asymmetry: ORTHO %.3f, PERSPECTIVE CONTROL %.3f (fraction of covered texels)' % (aO, aP))
print('  worst near-edge vs far-edge width difference: ORTHO %.3f, PERSPECTIVE CONTROL %.3f (fraction)' % (nO, nP))
# Pre-registered: 2 texels, which is the crop's integer rounding plus one
# antialiased texel of the smooth downsample on each side of the silhouette.
check('every frame of the orthographic bake spans its predicted texels within 2 (worst %.2f)' % dO, dO <= 2.0)
check('the PERSPECTIVE CONTROL does not (worst %.2f, and it must exceed 2)' % dP, dP > 2.0)
check('the orthographic silhouettes are centrally symmetric within 5%% (worst %.3f)' % aO, aO <= 0.05)
check('the PERSPECTIVE CONTROL is not (worst %.3f, and it must exceed 5%%)' % aP, aP > 0.05)
check('near and far edge of a frame carry the same width within 5%% (worst %.3f)' % nO, nO <= 0.05)
check('the PERSPECTIVE CONTROL foreshortens (worst %.3f, and it must exceed 5%%)' % nP, nP > 0.05)
# and the frame the two bakes chose is itself different, because the perspective
# camera measured a different silhouette in pass one
check('the two bakes recorded different half-extents, so the camera reaches the FORMAT and not only the picture (%.2f vs %.2f)' % (fw, pfw), abs(fw - pfw) > 0.5)

# ---- lane CARDWIDTH, 2026-09-10: the coverage contract, and the cube at the
# READER's threshold. Pre-registered in scratchpad/lane_cardwidth_report.md
# section 8 before this block was written.
dOR = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in orthoR)
dPR = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in perspR)
print('  at the READER threshold 128/255: worst |measured - predicted| ORTHO %.2f texels, PERSPECTIVE CONTROL %.2f' % (dOR, dPR))
# ---- F1, and the instrument it is measured through. Lane GATEFIX1, 2026-09-19,
# on the director's ruling: "do NOT raise the bar. Make the row DECODE coverage
# per the spec and test at 0.5; keep bar 1.0."
#
# THE ROW WAS RED FROM THE DAY IT WAS WRITTEN (2026-09-10, lane CARDWIDTH: 1.78;
# 1.68-1.69 on every exe since), and the fault was in the READING, not the bake.
# CARDWIDTH's 1.0 was pre-registered for a HALF-COVERAGE test -- its own comment
# above says "both specs tell it to test at 0.5". The coverage contract the SAME
# lane shipped in the SAME build then re-encoded alpha as 0-or-[160,255], which
# turned `alpha >= 128` into a test for coverage >= 16/255 = 6.27 per cent. The
# F3 row below proves that on this very sheet. A 6.27 per cent test admits a
# barely-touched texel on each side of the true edge, so it reads about one texel
# wide -- and it does so for a PERFECT bake as much as for ours.
#
# Measured, not argued (OCTF1 2026-09-19, reproduced by F2c below): an
# analytically exact cube -- the convex hull of the eight corners, rasterised at
# 12x12 samples per texel -- scores 1.78 through the 6.27 per cent reading and
# 0.71 through a true half-coverage one. 1.0 was unreachable by ANY bake at the
# old threshold. So the threshold moves onto the decoded coverage and the bar
# stays where CARDWIDTH pre-registered it.
_cv = meta(sys.argv[1]).get('coverage')
if not _cv:
    check('F1: the bake states a coverage contract, without which the row cannot decode', False)
else:
    _cf, _ct, _cb = (int(v) for v in _cv[0][:3])
    _, _, _, _, _, orthoH = measure(sys.argv[1], half, 128, (_cf, _cb))
    _, _, _, _, _, perspH = measure(sys.argv[2], half, 128, (_cf, _cb))
    dOH = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in orthoH)
    dPH = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in perspH)
    print('  DECODED to coverage and tested at HALF (128/255 of the decoded fraction, contract floor %d base %d):'
          % (_cf, _cb))
    print('    worst |measured - predicted| ORTHO %.2f texels, PERSPECTIVE CONTROL %.2f' % (dOH, dPH))
    check('F1: every frame of the cube spans its predicted texels within 1 AT HALF COVERAGE (worst %.2f)' % dOH,
          dOH <= 1.0)
    check('F2 (floor): the PERSPECTIVE CONTROL does not (worst %.2f, and it must exceed 1)' % dPH, dPH > 1.0)
    # F2b, THE RED CONTROL. F1 exists to catch "the silhouette changed size".
    # One 8-neighbour ring of dilation IS that defect, one texel on every side,
    # and it is applied to the SAME sheet through the SAME instrument -- no new
    # bake. If this ever clears 1.0, F1 has stopped responding.
    _, _, _, _, _, orthoD = measure(sys.argv[1], half, 128, (_cf, _cb), 1)
    dOD = max(max(abs(r[4] - r[2]), abs(r[5] - r[3])) for r in orthoD)
    check('F2b (floor): the same cube with its mask dilated one ring -- a silhouette one texel too '
          'wide on every side -- does NOT clear the bar (worst %.2f, and it must exceed 1)' % dOD,
          dOD > 1.0)
    # F2c/F2d, THE KNOWN ANSWER. An analytically exact cube pushed through this
    # very instrument: the convex hull of the eight corners projected on each
    # frame's (r, up), rasterised 12x12 samples per texel over the same extents,
    # thresholded at the same two readings. It says what a DEFECT-FREE bake
    # scores, so the bar is checked against arithmetic and not against us.
    try:
        import numpy as _np
    except ImportError:
        _np = None
    if _np is None:
        check('F2c: numpy is present, so the analytic known answer can be computed', False)
    else:
        def _hull(pts):
            pts = sorted(set(pts))
            def _h(ps):
                st = []
                for p in ps:
                    while len(st) >= 2 and (st[-1][0] - st[-2][0]) * (p[1] - st[-2][1]) \
                            - (st[-1][1] - st[-2][1]) * (p[0] - st[-2][0]) <= 0:
                        st.pop()
                    st.append(p)
                return st
            return _h(pts)[:-1] + _h(pts[::-1])[:-1]
        def _analytic(covThr, SS=12):
            tx, ty = 2.0 * fw / tw, 2.0 * fh / th
            sub = (_np.arange(SS) + 0.5) / SS
            X = (-fw + (_np.arange(tw)[:, None] + sub[None, :]) * tx).ravel()[None, :]
            Y = (-fh + (_np.arange(th)[:, None] + sub[None, :]) * ty).ravel()[:, None]
            worst = 0.0
            for j in range(n):
                for i in range(n):
                    r, up = axes(i, j, n)
                    px = 2.0 * half * (abs(r[0]) + abs(r[1]) + abs(r[2])) * tw / (2.0 * fw)
                    py = 2.0 * half * (abs(up[0]) + abs(up[1]) + abs(up[2])) * th / (2.0 * fh)
                    pts = [(sx * r[0] + sy * r[1] + sz * r[2], sx * up[0] + sy * up[1] + sz * up[2])
                           for sx in (-half, half) for sy in (-half, half) for sz in (-half, half)]
                    P = _hull(pts)
                    ins = _np.ones((th * SS, tw * SS), bool)
                    for k in range(len(P)):
                        ax, ay = P[k]
                        bx, by = P[(k + 1) % len(P)]
                        ins &= ((bx - ax) * (Y - ay) - (by - ay) * (X - ax)) >= 0
                    mk2 = ins.reshape(th, SS, tw, SS).mean(axis=(1, 3)) >= covThr
                    cols = _np.flatnonzero(mk2.any(axis=0))
                    rowsY = _np.flatnonzero(mk2.any(axis=1))
                    mx2 = int(cols[-1] - cols[0] + 1) if cols.size else 0
                    my2 = int(rowsY[-1] - rowsY[0] + 1) if rowsY.size else 0
                    worst = max(worst, abs(mx2 - px), abs(my2 - py))
            return worst
        aHalf = _analytic(0.5)
        aOld = _analytic(_cf / 255.0)
        print('  KNOWN ANSWER, an analytically exact cube through this same instrument:')
        print('    at HALF coverage %.2f texels; read the OLD way (coverage >= %d/255 = %.2f%%) %.2f texels'
              % (aHalf, _cf, 100.0 * _cf / 255.0, aOld))
        print('    the shipped bake, same two readings: %.2f and %.2f' % (dOH, dOR))
        check('F2c (known answer): a defect-free cube clears the 1-texel bar through this instrument '
              '(%.2f, and OCTF1 computed 0.71 on 2026-09-19)' % aHalf, aHalf <= 1.0)
        check('F2d (floor): the SAME defect-free cube read the OLD way does NOT clear it (%.2f, and it '
              'must exceed 1) -- which is why the bar was never the thing that was wrong' % aOld,
              aOld > 1.0)
# F1b is the invariant the coverage contract actually guarantees, and it is the
# one to read if F1 goes red: after the re-encoding the set a consumer TESTS is
# the set the bake FLOORED, so the two readings of the same sheet must agree
# exactly. On a sheet from before the contract they do not -- 5.41 texels apart
# on TreeHero01 -- so this is a check that can fail.
print('  the same cube at the bake floor: %.2f texels; at the reader threshold: %.2f' % (dO, dOR))
check('F1b: the reader threshold and the bake floor measure the SAME silhouette (%.2f vs %.2f)'
      % (dO, dOR), abs(dO - dOR) <= 0.01)

# F3/F4: the coverage contract. The sheet's alpha is written 0 or at/above `base`,
# so a consumer testing at `test` selects exactly the texels whose measured
# coverage reached `floor`. The floor under F3 is the same count on the DECODED
# alpha -- the fraction the bake measured -- which a cube's partly covered edge
# texels put well above zero, so F3 is a check that can fail on its own input.
mcov = meta(sys.argv[1]).get('coverage')
check('F5: the bake states its coverage contract on a line of its own', bool(mcov))
if mcov:
    cfloor, ctest, cbase = (int(v) for v in mcov[0][:3])
    print('  coverage contract: floor %d, test %d, base %d' % (cfloor, ctest, cbase))
    check('F5: the contract is the one this build writes (floor 16, test 128, base 160)',
          (cfloor, ctest, cbase) == (16, 128, 160))
    check('F5: the base leaves BC3 no room to round a covered texel under the test '
          '(base - 255/14 = %.1f > %d)' % (cbase - 255.0 / 14.0, ctest), cbase - 255.0 / 14.0 > ctest)
    sh = Image.open(sys.argv[1] + '/cube512_oct_albedo.png').convert('RGBA')
    ap = list(sh.split()[3].getdata())
    between = sum(1 for v in ap if 0 < v < cbase)
    dec = [0 if v < cbase else cfloor + int(round((v - cbase) * (255.0 - cfloor) / (255.0 - cbase))) for v in ap]
    decBetween = sum(1 for v in dec if 0 < v < cbase)
    print('  base sheet: %d texels with alpha in 1..%d; DECODED, %d' % (between, cbase - 1, decBetween))
    check('F3: no texel of the base sheet carries alpha between 1 and %d (%d found)' % (cbase - 1, between),
          between == 0)
    check('F4 (floor): the DECODED fraction does carry them, so F3 can fail (%d found, must exceed 0)' % decBetween,
          decBetween > 0)
    covered = sum(1 for v in ap if v >= ctest)
    atfloor = sum(1 for v in dec if v >= cfloor)
    check('F3: the set the consumer tests at %d is exactly the set the bake floored at %d (%d vs %d)'
          % (ctest, cfloor, covered, atfloor), covered == atfloor and covered > 0)
sys.exit(1 if fails else 0)
PYEOF
	[ $? -eq 0 ] || fails=$((fails + 1))
fi

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
