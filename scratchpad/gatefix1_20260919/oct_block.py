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
