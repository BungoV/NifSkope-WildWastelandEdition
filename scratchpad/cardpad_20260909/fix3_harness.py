#!/usr/bin/env python3
"""CARDPAD -- tests/spells/lodgen_octahedral.sh: the padding, fill and bleed
checks move to the GAP rule."""

P = 'tests/spells/lodgen_octahedral.sh'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

def sub(old, new, what):
    global s
    n = s.count(old)
    assert n == 1, '%s: %d occurrences' % (what, n)
    s = s.replace(old, new)

# ------------------------------------------------------------ 1. the law itself
sub("""# THE PADDING, per axis, exact, and agreeing with the sidecar's own `pad` line.
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
""", """# THE GAP, per axis, exact, and agreeing with the sidecar's own `gap` line.
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
""", 'the gap law')

# ------------------------------------------------------------ 2. the inner rect
sub("""# coverage is judged against the INNER rect: the padding is transparent by design
G = max(2, min(TW, TH) // 16)
G = G + (G & 1)
inner = (TW - 2 * max(2, TW // 16 + (TW // 16 & 1))) * (TH - 2 * max(2, TH // 16 + (TH // 16 & 1)))
print('  distinct albedo frames: %d of %d; covered pixels per frame: min %d max %d of %d inside the gutter (best %.1f%%)' % (len(hashes), N*N, min(counts), max(counts), inner, 100.0*max(counts)/inner))""",
"""# coverage is judged against the INNER rect, `frame - gap`: the margins are
# transparent by design, and there is one gap's worth of them per axis
inner = (TW - GAPX) * (TH - GAPY)
print('  distinct albedo frames: %d of %d; covered pixels per frame: min %d max %d of %d inside the margins (best %.1f%%)' % (len(hashes), N*N, min(counts), max(counts), inner, 100.0*max(counts)/inner))""",
    'inner rect')

# ------------------------------------------------------------ 3. the bleed check
sub("""# MIP BLEED. Frames never mix during CONSTRUCTION -- the box filter halves an
# even frame into an even frame -- so the bleed is at SAMPLE time: a reader
# sampling ON a frame's UV border takes half its value from the next frame.
# Measured as the neighbour's ALPHA contribution at every border of every
# shipped mip. The CONTROL is the same sheet with the padding stripped and the
# inner rects re-tiled edge to edge, and it MUST bleed -- a check that cannot
# fail on its input is not a check.
MIPS = 1
g_ = min(PADX, PADY)
while g_ >= 2:
    g_ //= 2; MIPS += 1""",
"""# MIP BLEED, ON THE GAP RULE. Frames never mix during CONSTRUCTION -- the box
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
    g_ //= 2; MIPS += 1""",
    'mip count')

sub("""def worst_bleed(px, w, h, fw, fh, mips):
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
check('no shipped mip bleeds a neighbouring frame across a border', wb == 0)""",
"""def min_gap(px, w, h, fw, fh, mips):
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
      ns > 0 and mg >= 0.999)""",
    'bleed metric')

sub("""cw, ch = worst_bleed(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, padding stripped: worst %d/255 on %d borders (must be > 0)' % (cw, ch))
check('the bleed check FAILS on a sheet with no padding (the metric can see it)', cw > 0)""",
"""cg, cs = min_gap(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, margins stripped: narrowest gap %.3f texels over %d samples (must be under 1)' % (cg, cs))
check('the gap check FAILS on a sheet with no margins at all (the metric can see it)', cs > 0 and cg < 0.999)""",
    'bleed control')

# ------------------------------------------------------------ 4. the DDS mip count
sub("""expect, side = 1, min(tw, th)
while side >= 16:
    expect += 1; side //= 2""",
"""# THE MIP CAP IS THE GAP'S: 1 + log2(min(gapX, gapY)) -- 4 levels on a 128-texel
# frame, 3 on a 64. What stood here counted levels until a frame spanned eight
# texels, a rule that had nothing to do with the spacing and agreed with the
# shipped count only by coincidence on this bake's frame shape.
expect, _g = 1, min(_gapOf(tw), _gapOf(th))
while _g >= 2:
    expect += 1; _g //= 2""",
    'DDS mip expectation')

sub("""check('the sheets are BC3 and mip only while a frame spans eight texels (mips %d)' % card.get('mips', -1), okm and card.get('mips') == expect)""",
"""check('the sheets are BC3 and mip only while a whole texel of gap survives (mips %d)' % card.get('mips', -1), okm and card.get('mips') == expect)""",
    'DDS mip check text')

# ------------------------------------------------------------ 5. the .lodm keys
sub("""check('the .lodm carries the grid, the frame and the extents of the meta', card.get('oct') == 4 and card.get('frame') == [tw, th] and abs(card.get('half', [0, 0])[1] - float(oct[5])) < 1e-2 and card.get('depthSpan') == float(oct[9]))""",
"""check('the .lodm carries the grid, the frame and the extents of the meta', card.get('oct') == 4 and card.get('frame') == [tw, th] and abs(card.get('half', [0, 0])[1] - float(oct[5])) < 1e-2 and card.get('depthSpan') == float(oct[9]))
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
      and card.get('gap') == [2 * p for p in card.get('pad', [0, 0])])""",
    'lodm pad/gap keys')

nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0, 'CR moved'
open(P, 'wb').write(nb)
print('fix3 ok: %d -> %d bytes' % (len(b), len(nb)))
