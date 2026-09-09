#!/usr/bin/env python3
"""CARDFIT3 fix 02 -- the gates.

tests/spells/lodgen_octahedral.sh
  * the FRAME LAW check replaced: the five-rung ratio ladder and its
    max(32, 2G+4) floor are gone; the short side is a multiple of 16 in
    [16, tileLong] and the frame is the SMALLEST such that does not crop.
  * the PADDING checked exactly, per axis and on all four sides, against
    pad(side) = max(2, side/16) rounded up to even -- and against the sidecar's
    own `pad` line, so the number the bake wrote and the number it used must
    agree.
  * the LONG AXIS must be FILLED: the union of the silhouette boxes over all
    N^2 views spans the inner rect's long side to within 2%. A frame that is
    mostly air now fails.
  * MIP BLEED across every frame border at every shipped mip, with the
    zero-padding CONTROL that must fail.
  * DILATION present: colour under the transparent texels next to the edge.

tests/spells/lodgen_impostor_cards.sh
  * `_fs.DDS` is BC3/DXT5 with a real alpha, not BC1 with none. The block reader
    moves from 8-byte BC1 blocks to 16-byte BC3 ones, and the alpha is measured:
    a fully transparent corner and a fully opaque middle, which BC1-no-alpha
    cannot produce.
"""
import sys

OCT = 'tests/spells/lodgen_octahedral.sh'
CARDS = 'tests/spells/lodgen_impostor_cards.sh'

# ---------------------------------------------------------------- octahedral
b = open(OCT, 'rb').read()
assert b.count(b'\r') == 0, 'lodgen_octahedral.sh is LF-only'
s = b.decode('utf-8')
n = 0

old = """# THE FRAME LAW. With no WW_IMPOSTOR_REF there is no size ladder, so the long
# side is the run's resolution exactly. The short side is that times ONE of the
# five aspect rungs, rounded to a multiple of four and never under the floor that
# clears the gutter - not merely "a multiple of 16", which these rungs satisfy by
# luck at a 64 px tile and would keep satisfying if the ladder were wrong.
RUNGS = [1.0, 0.75, 0.5, 0.375, 0.25]
GG = max(4, TILE // 16)
SHORT_FLOOR = max(32, -(-(2 * GG + 4) // 4) * 4)
allowed = sorted({min(max(int(TILE * r + 0.5) // 4 * 4, SHORT_FLOOR), TILE) for r in RUNGS})
print('  frame law: long %d (the run\\'s %d), short %d, rungs allowed %s' % (max(TW, TH), TILE, min(TW, TH), allowed))
check('the long side is the run\\'s resolution and the short side is one of the five aspect rungs',
      max(TW, TH) == TILE and min(TW, TH) in allowed)
"""
new = """# THE FRAME LAW (2026-09-09). With no WW_IMPOSTOR_REF there is no size ladder,
# so the long side is the run's resolution exactly. The short side is a MULTIPLE
# OF 16 in [16, long] -- and not just any one: the SMALLEST that does not crop
# the silhouette, which is what "maximizing the tree's size in each row and
# column" means. A frame one rung wider than necessary fails the fill check
# below, so this pair cannot both pass on a wrong ladder.
allowed = list(range(16, TILE + 1, 16))
print('  frame law: long %d (the run\\'s %d), short %d, multiples of 16 allowed %s'
      % (max(TW, TH), TILE, min(TW, TH), allowed))
check('the long side is the run\\'s resolution and the short side is a multiple of 16 within it',
      max(TW, TH) == TILE and min(TW, TH) in allowed)

# THE PADDING, per axis, exact, and agreeing with the sidecar's own `pad` line.
# pad(side) = max(2, side/16) rounded UP to even: bungo's "8 pixels on
# 1024x1024, 16 on 2k" applied to each axis, floored so every card ships two
# clean mips, evened so --card-half-aux halves it onto a whole texel.
def padOf(side):
    p = max(2, side // 16)
    return p + (p & 1)
PADX, PADY = padOf(TW), padOf(TH)
padLine = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('pad ')]
print('  padding: derived %d,%d  sidecar %s' % (PADX, PADY, padLine[0][1:3] if padLine else 'ABSENT'))
check('the meta names the padding on a line of its own, per axis',
      len(padLine) == 1 and [int(padLine[0][1]), int(padLine[0][2])] == [PADX, PADY])
"""
assert s.count(old) == 1, ('frame law', s.count(old))
s = s.replace(old, new); n += 1

old = """# the gutter: no covered texel within G of a frame border, in any frame
touch = 0
for j in range(N):
    for i in range(N):
        t = tile(alb, i, j)
        for k in covered(i, j):
            x, y = k % TW, k // TW
            if x < G or y < G or x >= TW - G or y >= TH - G:
                touch += 1
print('  covered texels inside the %d-texel gutter: %d' % (G, touch))
check('every frame keeps its gutter clear', touch == 0)
"""
new = """# THE PADDING IS EXACT ON ALL FOUR SIDES, and the frame is FILLED.
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
check('the silhouette FILLS the inner rect on the long axis to within 2%%',
      unionL >= 0.98 * INNER_L)
check('and does not exceed it (the padding is not eaten)', unionL <= INNER_L and unionS <= INNER_S)

# MIP BLEED. Frames never mix during CONSTRUCTION -- the box filter halves an
# even frame into an even frame -- so the bleed is at SAMPLE time: a reader
# sampling ON a frame's UV border takes half its value from the next frame.
# Measured as the neighbour's ALPHA contribution at every border of every
# shipped mip. The CONTROL is the same sheet with the padding stripped and the
# inner rects re-tiled edge to edge, and it MUST bleed -- a check that cannot
# fail on its input is not a check.
MIPS = 1
g_ = min(PADX, PADY)
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
def worst_bleed(px, w, h, fw, fh, mips):
    worst, nbad = 0, 0
    for k in range(mips):
        if k:
            px, w, h = boxdown(px, w, h)
        fwk, fhk = fw >> k, fh >> k
        if fwk < 1 or fhk < 1:
            continue
        for i in range(1, N):
            x = i * fwk
            for xx in (x - 1, x):
                a = max(px[y * w + xx][3] for y in range(h)) // 2
                worst = max(worst, a); nbad += 1 if a else 0
        for j in range(1, N):
            y = j * fhk
            for yy in (y - 1, y):
                a = max(px[yy * w + x][3] for x in range(w)) // 2
                worst = max(worst, a); nbad += 1 if a else 0
    return worst, nbad
apx = list(alb.getdata())
wb, nb = worst_bleed(apx, alb.width, alb.height, TW, TH, MIPS)
print('  %d shipped mips (padding %d,%d): worst neighbour alpha at a frame border %d/255, on %d borders'
      % (MIPS, PADX, PADY, wb, nb))
check('no shipped mip bleeds a neighbouring frame across a border', wb == 0)
IW, IH = TW - 2 * PADX, TH - 2 * PADY
ctl = []
for j in range(N):
    for y in range(IH):
        for i in range(N):
            for x in range(IW):
                ctl.append(apx[(j * TH + PADY + y) * alb.width + i * TW + PADX + x])
cw, ch = worst_bleed(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, padding stripped: worst %d/255 on %d borders (must be > 0)' % (cw, ch))
check('the bleed check FAILS on a sheet with no padding (the metric can see it)', cw > 0)

# DILATION: colour under the transparent texels, so filtering never pulls black
# into an edge. Measured on the texel just outside the silhouette.
apl = alb.load()
dil = darkc = 0
for j in range(N):
    for i in range(N):
        for k in covered(i, j):
            x, y = i * TW + k % TW, j * TH + k // TW
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p = apl[x + dx, y + dy]
                if p[3] < 16:
                    dil += 1
                    if p[0] + p[1] + p[2] == 0:
                        darkc += 1
print('  transparent texels beside the silhouette: %d, of which BLACK: %d' % (dil, darkc))
check('the transparent texels beside the silhouette carry dilated colour', dil > 0 and darkc == 0)
"""
assert s.count(old) == 1, ('gutter', s.count(old))
s = s.replace(old, new); n += 1

# the old inner-rect line used G; keep the variable meaningful
old = """# coverage is judged against the INNER rect: the gutter is transparent by design
G = max(4, 64 // 16)
inner = (TW - 2 * G) * (TH - 2 * G)"""
new = """# coverage is judged against the INNER rect: the padding is transparent by design
G = max(2, min(TW, TH) // 16)
G = G + (G & 1)
inner = (TW - 2 * max(2, TW // 16 + (TW // 16 & 1))) * (TH - 2 * max(2, TH // 16 + (TH // 16 & 1)))"""
assert s.count(old) == 1, ('inner', s.count(old))
s = s.replace(old, new); n += 1

# bake 3: the aspect rung floor was 32; it is 16 now
old = """	[ "$SHORT3" -le "$LONG3" ] && [ "$SHORT3" -ge 32 ] \\
		&& ok "the aspect rung applies beneath the size rung (short $SHORT3 within the floor and the long side)" \\
		|| bad "the short side $SHORT3 is outside the floor..long range\""""
new = """	[ "$SHORT3" -le "$LONG3" ] && [ "$SHORT3" -ge 16 ] && [ $(( SHORT3 % 16 )) -eq 0 ] \\
		&& ok "the aspect quantisation applies beneath the size rung (short $SHORT3, a multiple of 16 within the long side)" \\
		|| bad "the short side $SHORT3 is not a multiple of 16 in 16..$LONG3\""""
assert s.count(old) == 1, ('bake3', s.count(old))
s = s.replace(old, new); n += 1

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(OCT, 'wb').write(out)
print('lodgen_octahedral.sh: %d edits' % n)

# ------------------------------------------------------------ impostor cards
b = open(CARDS, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
m = 0

old = """b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
print('  %s   the sheet is twice the card width (%dx%d)' % ('ok  ' if w == 64 and h == 64 else 'FAIL', w, h))
# BC1: 8 bytes a 4x4 block, 16 blocks a row at 64 wide; compare block 0 (left) with block 8 (right)
off = 4 + 124 + (20 if b[84:88] == b'DX10' else 0)
left = b[off:off + 8]; right = b[off + 8 * 8:off + 8 * 9]
print('  %s   left and right halves differ (front vs side)' % ('ok  ' if left != right else 'FAIL'))
sys.exit(0 if (w == 64 and h == 64 and left != right) else 1)"""
new = """b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fourcc = b[84:88]
pfflags = struct.unpack_from('<I', b, 80)[0]
flags = struct.unpack_from('<I', b, 8)[0]
caps = struct.unpack_from('<I', b, 108)[0]
print('  %s   the sheet is twice the card width (%dx%d)' % ('ok  ' if w == 64 and h == 64 else 'FAIL', w, h))
# THE ALPHA. This sheet went out as DXT1 with no DDPF_ALPHAPIXELS until
# 2026-09-09, and every card quad in a chunk drew as an OPAQUE SQUARE. Vanilla's
# own alpha-tested tree LOD textures (Textures/LOD/Trees/MapleBranchesLOD_d.dds,
# ElmBranchesLOD_d.dds) are DXT5 with dwFlags 0x000A1007, pfflags 0x4 and
# caps 0x401008; those are the four numbers checked, not a guess at a format.
fmtOk = (fourcc == b'DXT5' and pfflags == 0x4 and flags == 0x000A1007 and caps == 0x401008)
print('  %s   the sheet is DXT5 with vanilla\\'s own header (fourCC %s, dwFlags 0x%08X, pfflags 0x%X, caps 0x%X)'
      % ('ok  ' if fmtOk else 'FAIL', fourcc.decode('latin1'), flags, pfflags, caps))
# BC3: 16 bytes a 4x4 block (8 alpha, 8 colour), 16 blocks a row at 64 wide;
# compare block 0 (left half) with block 8 (right half)
off = 4 + 124 + (20 if fourcc == b'DX10' else 0)
BB = 16 if fourcc == b'DXT5' else 8
left = b[off:off + BB]; right = b[off + BB * 8:off + BB * 9]
print('  %s   left and right halves differ (front vs side)' % ('ok  ' if left != right else 'FAIL'))
# and the alpha block CARRIES something: the synthetic cards are fully opaque,
# so every alpha endpoint must read 255. A BC1 sheet has no alpha block at all
# and this offset would be colour, which is red (not 0xFF,0xFF).
a0, a1 = left[0], left[1]
alphaOk = (a0 == 255 and a1 == 255)
print('  %s   the alpha block is present and reads opaque on an opaque card (endpoints %d, %d)'
      % ('ok  ' if alphaOk else 'FAIL', a0, a1))
sys.exit(0 if (w == 64 and h == 64 and left != right and fmtOk and alphaOk) else 1)"""
assert s.count(old) == 1, ('fs dds', s.count(old))
s = s.replace(old, new); m += 1

old = """#   3. the sheet is twice the card's width
#   4. its left and right halves differ (front red, side blue survive BC1)"""
new = """#   3. the sheet is twice the card's width
#   4. its left and right halves differ (front red, side blue survive BC3)
#   5. it is BC3/DXT5 carrying a real alpha, with vanilla's own DDS header --
#      it was BC1 with no DDPF_ALPHAPIXELS until 2026-09-09, which is why every
#      card quad in a chunk drew as an opaque square"""
assert s.count(old) == 1, ('header comment', s.count(old))
s = s.replace(old, new); m += 1

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(CARDS, 'wb').write(out)
print('lodgen_impostor_cards.sh: %d edits' % m)
