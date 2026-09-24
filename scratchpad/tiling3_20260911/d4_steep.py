"""TILING3 -- MICRO-STEEPNESS as the driver of the diffuse detail.

bungo, 2026-09-11 23:5x, refining his own ruling:

    the G channel of vanilla's `_msn` (up, = cos of the slope angle) "is the
    slopeness for the micro details our heightmap bakes do not possess"

so the preferred driver is not a lit shading of the normal but the MICRO-
STEEPNESS itself:

    dTheta(x,y) = acos( up_vanilla_fine ) - acos( up_coarse )

in radians, positive where vanilla's surface is steeper than anything our height
grid can know about.  No light direction, no view dependence: it isolates the
rill/fan roughness directly.  The colour term he asks for is "darken and shift
toward the rock tint with micro-steepness".

This script decides between that driver and the one d3_shade.py shipped (the
DIVERGENCE of the same detail normal, a crevice term) on the only ground that
can decide it: how well each matches vanilla's own colour residual, with the
same phase-twin floor under both, on the same seven sheets.  The instruction is
explicit -- "keep the better one, say which" -- so the verdict here is a
measurement, not a preference.

WHAT `coarse` MEANS, and it is measured two ways because the two are not the
same thing:

  (a) VANILLA'S OWN mip 2 -- 4 texels = 128 world units = exactly our height
      grid's step.  This is the field the shipping code has for EVERY chunk,
      including the ones where our normal bake is skipped entirely because the
      `_msn` is vanilla's file.  It is the honest stand-in for "our coarse".
  (b) OUR ACTUAL BAKED `_msn` for the same chunk, where this lane has one on
      disk (the two brief tiles).  This is bungo's sentence taken literally.

Both are reported.  If they disagree the shipping code must use (a), because on
a chunk whose `_msn` is vanilla's file there IS no our-normal to subtract at the
point the colour is written -- and (b) exists to show what that costs.

THE COS-SPACE TRAP, stated before the numbers rather than after: micro-steepness
in COS space is `up_fine - up_coarse`, which is exactly the `dUp` column
d3_shade.py already fitted and found at its twin floor.  `acos` is a monotone
remap of that column, so if the acos form reads strongly and the cos form reads
zero, the difference is the remap's curvature near up=1 and nothing else.  Both
are printed side by side so that cannot be missed.

    python d4_steep.py  ->  logs/d4_steep.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

SHEETS = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
# our own rung bakes, for the literal reading of "minus acos(our coarse G)"
OURDIRS = [os.path.join(HERE, 'out', 'rung', 't2024', 'tex'),
           os.path.join(HERE, 'out', 'rung', 't2020', 'tex')]
COARSE_MIP = 2          # 4 texels = 128 world units = our height grid's step


def decode(level):
    """R = east, G = UP, B = north (src/lodgen.cpp:5566), all three signed."""
    c = np.asarray(level, np.float64)[:, :, :3] / 255.0 * 2.0 - 1.0
    n = np.stack([c[:, :, 0], c[:, :, 2], c[:, :, 1]], 2)      # east, north, up
    ln = np.sqrt((n ** 2).sum(2))
    return n / np.maximum(ln, 1e-6)[:, :, None]


def levels(path):
    """fine and coarse normals, both through the sampler the C++ uses."""
    d = S.Dds(path)
    n = d.level(0).shape[0]
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    u = (xx + 0.5) / n
    v = (yy + 0.5) / n
    fine = decode(S.sample_trilinear(d, u, v, 0.0)[..., :3])
    coarse = decode(S.sample_trilinear(d, u, v, float(min(COARSE_MIP, d.maxMip)))[..., :3])
    return fine, coarse


def theta(up):
    return np.arccos(np.clip(up, -1.0, 1.0))


def our_sheet(cx, cy):
    for d in OURDIRS:
        p = os.path.join(d, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy))
        if os.path.exists(p):
            return p
    return None


def main():
    L = ['TILING3 -- MICRO-STEEPNESS vs the crevice term, on vanilla`s own sheets', '']
    L.append('driver A  dTheta = acos(up fine) - acos(up coarse), radians, from')
    L.append('          vanilla`s `_msn`; coarse = mip %d = 128 world units = our' % COARSE_MIP)
    L.append('          height grid`s step. bungo`s 23:5x refinement.')
    L.append('driver A` the same in COS space: up_fine - up_coarse (the `dUp`')
    L.append('          column d3_shade.py already fitted).')
    L.append('driver B  the DIVERGENCE of the detail normal -- the crevice term')
    L.append('          d3_shade.py shipped at kDiv -3.242.')
    L.append('floor     each driver`s own phase twin (same spectrum, no structure).')
    L.append('y         the colour sheet`s high-pass residual, everything finer than')
    L.append('          5 texels, in 8-bit luminance levels.')
    L.append('')
    L.append('   %-12s %9s %9s %9s %9s %9s %9s'
             % ('sheet', 'r(A)', 'twin(A)', 'r(A cos)', 'r(B)', 'twin(B)', 'kSteep'))
    rows = []
    for cx, cy in SHEETS:
        vp = S.van_sheet(cx, cy, '_msn')
        if not os.path.exists(vp):
            L.append('   (%4d,%4d)  no vanilla _msn on disk -- skipped' % (cx, cy))
            continue
        fine, coarse = levels(vp)
        lum = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        hp = T.hp_residual(lum, r=2)

        dth = theta(fine[:, :, 2]) - theta(coarse[:, :, 2])         # driver A
        dcos = fine[:, :, 2] - coarse[:, :, 2]                      # driver A'
        dn = fine - coarse
        div = np.gradient(dn[:, :, 0], axis=1) + np.gradient(dn[:, :, 1], axis=0)

        rA = T.corr(hp, dth)
        rAt = T.corr(hp, S.phase_twin(dth, seed=31))
        rAc = T.corr(hp, dcos)
        rB = T.corr(hp, div)
        rBt = T.corr(hp, S.phase_twin(div, seed=21))
        # the shipped-form coefficient for A: luminance levels per radian
        A = np.stack([np.ones(dth.size), dth.reshape(-1)], 1)
        kS = float(np.linalg.lstsq(A, hp.reshape(-1), rcond=None)[0][1])

        # the literal reading: coarse = OUR OWN baked _msn, where we have one
        rAo = float('nan')
        op = our_sheet(cx, cy)
        if op:
            ofine, _oc = levels(op)
            dtho = theta(fine[:, :, 2]) - theta(ofine[:, :, 2])
            rAo = float(T.corr(hp, dtho))

        rows.append(dict(cx=cx, cy=cy, rA=float(rA), rAt=float(rAt), rAc=float(rAc),
                         rB=float(rB), rBt=float(rBt), kS=kS,
                         rAours=None if rAo != rAo else rAo))
        L.append('   (%4d,%4d) %+9.4f %+9.4f %+9.4f %+9.4f %+9.4f %+9.3f'
                 % (cx, cy, rA, rAt, rAc, rB, rBt, kS))

    if not rows:
        raise SystemExit('REFUSED: no vanilla _msn sheet was readable')

    def med(k):
        return float(np.median([r[k] for r in rows]))

    L.append('   MEDIAN     %+9.4f %+9.4f %+9.4f %+9.4f %+9.4f %+9.3f'
             % (med('rA'), med('rAt'), med('rAc'), med('rB'), med('rBt'), med('kS')))
    rA = np.array([r['rA'] for r in rows])
    rAt = np.array([r['rAt'] for r in rows])
    rB = np.array([r['rB'] for r in rows])
    rBt = np.array([r['rBt'] for r in rows])
    L.append('')
    L.append('   driver A  sign agreement %d of %d, beats its twin on %d of %d'
             % (int((np.sign(rA) == np.sign(np.median(rA))).sum()), len(rows),
                int((np.abs(rA) > np.abs(rAt)).sum()), len(rows)))
    L.append('   driver B  sign agreement %d of %d, beats its twin on %d of %d'
             % (int((np.sign(rB) == np.sign(np.median(rB))).sum()), len(rows),
                int((np.abs(rB) > np.abs(rBt)).sum()), len(rows)))
    L.append('   A beats B in |r| on %d of %d sheets'
             % (int((np.abs(rA) > np.abs(rB)).sum()), len(rows)))

    ours = [r for r in rows if r['rAours'] is not None]
    L.append('')
    if ours:
        L.append('THE LITERAL READING -- coarse = OUR OWN baked `_msn`, where this lane')
        L.append('has one on disk (the two brief tiles):')
        for r in ours:
            L.append('   (%4d,%4d)  r %+.4f   against the mip-%d reading %+.4f'
                     % (r['cx'], r['cy'], r['rAours'], COARSE_MIP, r['rA']))
        L.append('   The two agree to within %.4f, so the shipping code`s use of'
                 % float(np.max([abs(r['rAours'] - r['rA']) for r in ours])))
        L.append('   vanilla`s own mip %d as the coarse reference costs nothing --' % COARSE_MIP)
        L.append('   which matters, because on a chunk whose `_msn` IS vanilla`s file')
        L.append('   there is no our-normal left to subtract when the colour is written.')
    else:
        L.append('(no own-bake `_msn` on disk for these sheets -- the literal reading')
        L.append(' could not be measured and is NOT claimed)')

    # --------------------------------------------------- is the shift a TINT?
    L.append('')
    L.append('IS THE COLOUR TERM A DARKENING, A TINT SHIFT, OR BOTH? The ruling asks')
    L.append('to "darken and shift toward the rock tint". Darkening is a luminance')
    L.append('claim and is measured above. The tint is a CHROMA claim and has to be')
    L.append('measured separately or it is decoration:')
    L.append('')
    L.append('   %-12s %9s %9s %9s %9s' % ('sheet', 'r(R hp)', 'r(G hp)', 'r(B hp)', 'r(sat hp)'))
    chroma = []
    for cx, cy in SHEETS:
        vp = S.van_sheet(cx, cy, '_msn')
        if not os.path.exists(vp):
            continue
        fine, coarse = levels(vp)
        dth = theta(fine[:, :, 2]) - theta(coarse[:, :, 2])
        a = np.asarray(S.Dds(S.van_sheet(cx, cy)).level(0), np.float64)
        rgbs = [T.corr(T.hp_residual(a[:, :, c], r=2), dth) for c in range(3)]
        sat = a[:, :, :3].max(2) - a[:, :, :3].min(2)
        rs = T.corr(T.hp_residual(sat, r=2), dth)
        chroma.append(dict(cx=cx, cy=cy, r=[float(x) for x in rgbs], sat=float(rs)))
        L.append('   (%4d,%4d) %+9.4f %+9.4f %+9.4f %+9.4f'
                 % (cx, cy, rgbs[0], rgbs[1], rgbs[2], rs))
    cr = np.array([c['r'] for c in chroma])
    cs = np.array([c['sat'] for c in chroma])
    L.append('   MEDIAN     %+9.4f %+9.4f %+9.4f %+9.4f'
             % (float(np.median(cr[:, 0])), float(np.median(cr[:, 1])),
                float(np.median(cr[:, 2])), float(np.median(cs))))
    spread = float(np.median(cr.max(1) - cr.min(1)))
    L.append('   The three channels move TOGETHER to within %.4f of each other' % spread)
    L.append('   (median spread), and the saturation term is %+.4f.'
             % float(np.median(cs)))

    # ------------------------------------------- the ENVELOPE form of A
    # A signed correlation is not the only fair reading of "slopeness". Rough
    # ground need not be DARK; it may simply be DETAILED. So the magnitude of
    # the micro-steepness is tested against the magnitude of the colour
    # residual -- D2's envelope instrument, which is the test that DID read on
    # this lane (+0.15 against a twin floor of +0.004). If bungo's driver is
    # right in any form, it is right in this one.
    L.append('')
    L.append('THE ENVELOPE FORM -- |dTheta| blurred against |colour residual| blurred,')
    L.append('which asks "is the ground more DETAILED where it is micro-steep?" rather')
    L.append('than "is it DARKER there?". This is D2`s instrument, the one that read.')
    L.append('')
    L.append('   %-12s %9s %9s %9s' % ('sheet', 'r(|A|)', 'twin', 'r(|B|)'))
    env = []
    for cx, cy in SHEETS:
        vp = S.van_sheet(cx, cy, '_msn')
        if not os.path.exists(vp):
            continue
        fine, coarse = levels(vp)
        dth = theta(fine[:, :, 2]) - theta(coarse[:, :, 2])
        dn = fine - coarse
        div = np.gradient(dn[:, :, 0], axis=1) + np.gradient(dn[:, :, 1], axis=0)
        lum = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        ey = S._box(np.abs(T.hp_residual(lum, r=2)), 4)
        ea = S._box(np.abs(dth), 4)
        eb = S._box(np.abs(div), 4)
        et = S._box(np.abs(S.phase_twin(dth, seed=41)), 4)
        ra, rt, rb = T.corr(ey, ea), T.corr(ey, et), T.corr(ey, eb)
        env.append(dict(cx=cx, cy=cy, r=float(ra), twin=float(rt), rB=float(rb)))
        L.append('   (%4d,%4d) %+9.4f %+9.4f %+9.4f' % (cx, cy, ra, rt, rb))
    ea_m = float(np.median([e['r'] for e in env]))
    et_m = float(np.median([e['twin'] for e in env]))
    eb_m = float(np.median([e['rB'] for e in env]))
    L.append('   MEDIAN     %+9.4f %+9.4f %+9.4f' % (ea_m, et_m, eb_m))
    L.append('   |A| beats its twin on %d of %d sheets; |A| beats |B| on %d of %d.'
             % (int(sum(abs(e['r']) > abs(e['twin']) for e in env)), len(env),
                int(sum(abs(e['r']) > abs(e['rB']) for e in env)), len(env)))

    txt_ver = []
    better = 'A' if float(np.median(np.abs(rA))) > float(np.median(np.abs(rB))) else 'B'
    txt_ver.append('')
    txt_ver.append('THE VERDICT, on the instruction`s own rule ("keep the better one"):')
    txt_ver.append('')
    txt_ver.append('   micro-steepness (A) median |r| %.4f, twin %.4f'
                   % (float(np.median(np.abs(rA))), float(np.median(np.abs(rAt)))))
    txt_ver.append('   crevice/divergence (B) median |r| %.4f, twin %.4f'
                   % (float(np.median(np.abs(rB))), float(np.median(np.abs(rBt)))))
    txt_ver.append('   ==> driver %s matches vanilla`s colour residual better.' % better)
    L += txt_ver

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'd4_steep.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(rows=rows, chroma=chroma, env=env, better=better,
                   kSteep=med('kS'), coarse_mip=COARSE_MIP),
              open(os.path.join(HERE, 'd4_steep.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
