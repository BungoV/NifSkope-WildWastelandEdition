#!/usr/bin/env python3
"""IDENTGAP -- the driver.  Six groups, two identity tables, three cameras.

Every number printed lands in `run.log` and `rows.json`; `report.md` quotes
nothing that is not in one of those two.
"""
import json
import numpy as np
import os
import sys
import time

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identgap_20260919'
IMG = LANE + '/images'
IP = ROOT + '/scratchpad/identprox_20260919'
IR_ = ROOT + '/scratchpad/identres_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (LANE, IR_, IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4core as H                                            # noqa: E402
import h4map as MP                                            # noqa: E402
import hwycams                                                # noqa: E402
import cams as CAMS                                           # noqa: E402
import join as JN                                             # noqa: E402
from scene import Terrain, Objects                            # noqa: E402
import identgap as IG                                         # noqa: E402

INF = float('inf')
LOG = open(LANE + '/run.log', 'a')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0), ('street', 120.0, 5.0)]
DS = (64.0, 128.0, 256.0, 512.0, 1024.0)

_V = os.environ.get('IDENTGAP_VIEWS')
if _V:
    VIEWS = [v for v in VIEWS if v[0] in _V.split(',')]
JOUT = LANE + ('/rows_partial.json' if _V else '/rows.json')

# the bias grid G0 and G4 are tuned over.  Stated before anything was measured.
NB = (0.0, 0.5, 1.0, 2.0, 3.0, 4.0)          # normal offset, in texels
DB = (0.25, 0.5, 1.0, 2.0)                   # constant depth bias, in texels
SL = (0.0, 1.0, 2.0, 4.0)                    # slope-scaled term


def load_view(name, ter, az, el):
    if name == 'hwydeck':
        cam = hwycams.build(ter, 1600, 900)[name]
        gp = IP + '/gb_%s.npz' % name
        tp = IP + '/truth_%s_az%03d_el%02d.npy' % (name, int(az), int(el))
    else:
        cam = CAMS.build(ter, 1600, 900)[name]
        gp = H4 + '/gb_%s.npz' % name
        tp = H4 + '/truth_%s_az%03d_el%02d.npy' % (name, int(az), int(el))
    for q in (gp, tp):
        if not os.path.exists(q):
            raise SystemExit('missing cache ' + q)
    return cam, H._GB(cam, np.load(gp)), np.load(tp)


def stats(gb, tr, lit, m, selfm):
    st = {}
    for nm, mm in (('all', m), ('terrain', gb.terfirst), ('objects', gb.objfirst)):
        k = int(mm.sum())
        st[nm] = 100.0 * float((tr[mm] != lit[mm]).sum()) / k if k else float('nan')
        st[nm + '_fd'] = 100.0 * float((tr[mm] & ~lit[mm]).sum()) / k if k else float('nan')
        st[nm + '_fl'] = 100.0 * float((~tr[mm] & lit[mm]).sum()) / k if k else float('nan')
    k = int(selfm.sum())
    st['self_lost'] = 100.0 * float(lit[selfm].sum()) / k if k else float('nan')
    return st


def tune_bias(mp, gb, tr, m, ident, D, taps, sub=3, label=''):
    """Sweep (normal offset, depth bias, slope) and return the best cell.

    Scored on every `sub`-th decided pixel for speed, then the winner is
    re-scored on ALL of them by the caller.  The objective is the OBJECT
    disagreement total -- a bias that kills acne by peter-panning the shadow off
    the wall pays for it in false-LIT, and this criterion makes it pay."""
    pos, nrm, idt = gb.pos[m], gb.nrm[m], ident[m]
    idx = np.arange(0, len(pos), sub)
    pos, nrm, idt = pos[idx], nrm[idx], idt[idx]
    om = gb.objfirst[m][idx]
    tm = gb.terfirst[m][idx]
    trm = tr[m][idx]
    best = None
    tab = []
    for nb in NB:
        for db in DB:
            for sl in SL:
                df, _, _ = IG.query_gate(mp, pos, nrm, idt, D=D, normal_bias=nb,
                                         depth_bias=db, slope=sl, taps=taps)
                lt = df < 0.5
                ko = int(om.sum())
                fd = 100.0 * float((trm[om] & ~lt[om]).sum()) / ko
                fl = 100.0 * float((~trm[om] & lt[om]).sum()) / ko
                kt = int(tm.sum())
                te = 100.0 * float((trm[tm] != lt[tm]).sum()) / kt if kt else 0.0
                tot = fd + fl
                tab.append((tot, fd, fl, te, nb, db, sl))
                if best is None or tot < best[0]:
                    best = (tot, fd, fl, te, nb, db, sl)
    tab.sort()
    p('    TUNE %s (%d cells, every %dth of %s px): best objects total %.2f%% at normal '
      'offset %.1f texel, depth bias %.2f texel, slope %.1f  (obj false-DARK %.2f%% '
      'false-LIT %.2f%%, terrain %.2f%%)'
      % (label, len(tab), sub, '{:,}'.format(int(m.sum())), best[0], best[4], best[5],
         best[6], best[1], best[2], best[3]))
    for r in tab[:3]:
        p('      runner-up  total %5.2f%%  fD %5.2f%%  fL %5.2f%%  terrain %5.2f%%  '
          '| normal %.1f depth %.2f slope %.1f' % (r[0], r[1], r[2], r[3], r[4], r[5], r[6]))
    mind = min(tab, key=lambda r: r[1])
    p('      the cell with the LEAST object false-DARK is normal %.1f depth %.2f slope %.1f: '
      'fD %.2f%% but fL %.2f%% (total %.2f%%) -- rejected, it peter-pans'
      % (mind[4], mind[5], mind[6], mind[1], mind[2], mind[0]))
    return dict(nb=best[4], db=best[5], sl=best[6], sweep_best=best[0],
                least_fd=dict(nb=mind[4], db=mind[5], sl=mind[6], fd=mind[1], fl=mind[2]))


SSS_STEPS = (16, 32, 64)
SSS_REACH = (0.05, 0.10)


def sss_pass(cam, gb, tr, m, selfm, az, el, keep, rows, vname, BD, JOUT):
    """The director's addendum: a screen-space shadow march on top of the far row.

    Measured on the pixels the far row leaves FALSE-LIT (truth dark, row lit):
    what share does the march recover, and what NEW false-dark does it invent?
    It can only ever take light away, so it cannot fix false-DARK."""
    import sss as SS
    p('  ---- ADDENDUM: screen-space shadows on top of the far row '
      '(on-screen occluders only)')
    sd = IG.sun_vec(az, el)
    # ---- the march's bias and thickness, tuned the same way the far bias was.
    #      A fixed world-unit bias is nonsense in a frame 20,000 units deep, so
    #      both are fractions of the receiver's camera distance and both are
    #      SWEPT, on the same objective: the least object disagreement total.
    b0 = keep['G1/A/map16'] >= 0.5
    fl_ = gb.objfirst & ~tr & b0
    ok_ = gb.objfirst & tr & b0
    bestp, tabp = None, []
    for br in (0.002, 0.005, 0.01, 0.02, 0.04, 0.08, 0.16):
        for th in (0.01, 0.02, 0.05, 0.10, 0.25, 0.50):
            if th <= br:
                continue
            lt, _ = SS.march(cam, gb, sd, b0, m & b0, steps=32, reach_frac=0.10,
                             bias_rel=br, thick_rel=th)
            rec = 100.0 * float((fl_ & ~lt).sum()) / max(1, int(fl_.sum()))
            new = 100.0 * float((ok_ & ~lt).sum()) / max(1, int(gb.objfirst.sum()))
            ko = int(gb.objfirst.sum())
            tot = (100.0 * float((tr[gb.objfirst] & ~lt[gb.objfirst]).sum()) / ko
                   + 100.0 * float((~tr[gb.objfirst] & lt[gb.objfirst]).sum()) / ko)
            tabp.append((tot, br, th, rec, new))
            if bestp is None or tot < bestp[0]:
                bestp = (tot, br, th, rec, new)
    tabp.sort()
    p('    TUNE the march (%d cells, 32 steps, reach 10%%, base G1/A): best object total '
      '%.2f%% at bias %.3f x depth, thickness %.2f x depth (recovers %.1f%% of the base '
      'false-LIT, adds %.2f%% new false-dark)'
      % (len(tabp), bestp[0], bestp[1], bestp[2], bestp[3], bestp[4]))
    for r in tabp[:3]:
        p('      runner-up total %5.2f%% | bias %.3f thick %.2f | recovered %5.1f%% new '
          'false-dark %5.2f%%' % (r[0], r[1], r[2], r[3], r[4]))
    BR, TH = bestp[1], bestp[2]
    bases = [('G1/A/map16', 'pure identity, table A'),
             ('G2/A/D%d/map16' % int(BD), 'gate D = %d u, table A' % int(BD)),
             ('G1/B/map16', 'pure identity, table B'),
             ('G2/B/D%d/map16' % int(BD), 'gate D = %d u, table B' % int(BD))]
    for rf in SSS_REACH:
        p('    reach %.0f%% of frame height = %d px; the sun ray covers a median %.1f px '
          'of screen at that world length on this camera'
          % (rf * 100, int(rf * cam.h), SS.sun_screen_len(cam, gb, sd, m, rf)))
    for bid, bdesc in bases:
        base = keep[bid] >= 0.5
        fl0 = gb.objfirst & ~tr & base          # the false-LIT this pass must fix
        ok0 = gb.objfirst & tr & base           # correctly lit: new dark here is a NEW error
        p('    base %-18s object false-LIT to recover: %s px;  correctly-lit object px at '
          'risk: %s' % (bid, '{:,}'.format(int(fl0.sum())), '{:,}'.format(int(ok0.sum()))))
        for rf in SSS_REACH:
            for ns in SSS_STEPS:
                t0 = time.time()
                lit, nd = SS.march(cam, gb, sd, base, m & base, steps=ns, reach_frac=rf,
                                   bias_rel=BR, thick_rel=TH)
                rec = 100.0 * float((fl0 & ~lit).sum()) / max(1, int(fl0.sum()))
                new = 100.0 * float((ok0 & ~lit).sum()) / max(1, int(gb.objfirst.sum()))
                st = stats(gb, tr, lit, m, selfm)
                st.update(dict(row='%s+SSS/n%d/r%02d' % (bid, ns, int(rf * 100)),
                               group='SSS', table=bid.split('/')[1], D=None,
                               look='map16+sss', view=vname, az=az, el=el, secs=time.time() - t0,
                               sss_steps=ns, sss_reach=rf, sss_recovered=rec,
                               sss_new_falsedark=new, sss_darkened=nd, base=bid,
                               sss_bias_rel=BR, sss_thick_rel=TH,
                               desc='%s + screen-space shadows, %d steps, reach %.0f%%'
                                    % (bdesc, ns, rf * 100), nodata_px=0, bias=None))
                rows.append(st)
                keep[st['row']] = lit.astype(float)
                json.dump(rows, open(JOUT, 'w'), indent=1)
                p('    %-28s | ALL %6.2f%% | terrain %6.2f%% | obj false-DARK %6.2f%%  '
                  'obj false-LIT %6.2f%% | RECOVERED %5.1f%% of the base false-LIT, NEW '
                  'false-dark %5.2f%% of object px | %s px darkened | %.1f s'
                  % ('%d steps, reach %.0f%%' % (ns, rf * 100), st['all'], st['terrain'],
                     st['objects_fd'], st['objects_fl'], rec, new, '{:,}'.format(nd),
                     time.time() - t0))


def main():
    t00 = time.time()
    os.makedirs(IMG, exist_ok=True)
    p('=' * 118)
    p('IDENTGAP  %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)

    # ---- the two identity tables -------------------------------------------
    uA, invA = np.unique(P.shipped, return_inverse=True)
    gidA = (invA + 1).astype(np.int64)
    pairs, gaps = JN.candidates(P, verbose=False)
    rawB = JN.identity(P, pairs, gaps, who='all', measure='mesh', gap=64.0)
    uB, invB = np.unique(rawB, return_inverse=True)
    gidB = (invB + 1).astype(np.int64)
    p('identity A = the shipped .lodi v7 groups: %d groups over %d placements' % (len(uA), P.n))
    p('identity B = the ruled proximity join (non-tree, mesh gap <= 64 u): %d groups over %d '
      'placements   [identprox RECOMMENDATION.txt says 588 -> 167]' % (len(uB), P.n))
    assert len(uA) == 588, len(uA)
    assert len(uB) == 167, len(uB)
    p('  CONTROL both gates reproduce identprox: A 588 groups, B 167 groups.')

    # the biggest joined identity, for the table-B picture
    cntB = np.bincount(gidB)
    p('  identity B largest group: %d placements; A largest: %d'
      % (int(cntB.max()), int(np.bincount(gidA).max())))

    GT = {'A': gidA[ob.inst][ob.tri[:, 0]], 'B': gidB[ob.inst][ob.tri[:, 0]]}

    # ---- G3's geometry census, once (it does not depend on the camera) ------
    closed, ntri, closed_idx = IG.open_shell_census(ob)
    ncl = sum(1 for v in closed.values() if v)
    nci = sum(1 for v in closed_idx.values() if v)
    tri_open = sum(ntri[i] for i in ntri if not closed[i])
    tri_all = len(ob.tri)
    cards = sum(1 for i in ntri if not closed[i] and ntri[i] <= 2)
    tri_card = sum(ntri[i] for i in ntri if not closed[i] and ntri[i] <= 2)
    p('G3 geometry census (positions welded at 0.25 u): %d placements carry level-0 triangles; '
      '%d are CLOSED shells (every welded edge used exactly twice), %d are OPEN.  %s of %s '
      'triangles (%.1f%%) belong to an open shell.  %d of the open ones are two-triangle CARDS '
      '(%s triangles).  Keyed on raw vertex INDICES instead of welded positions only %d would '
      'count as closed, which is why the weld is there.'
      % (len(ntri), ncl, len(ntri) - ncl, '{:,}'.format(tri_open), '{:,}'.format(tri_all),
         100.0 * tri_open / tri_all, cards, '{:,}'.format(tri_card), nci))
    p('  meaning: under back-face casting an OPEN shell has nothing behind its sun-facing side, '
      'so once that side is culled it writes no depth at all and stops casting -- that is '
      'exactly where G3 leaks.')

    rows = []
    for vname, az, el in VIEWS:
        p('')
        p('=' * 118)
        p('VIEW %s   sun az %.0f el %.0f' % (vname, az, el))
        cam, gb, tr = load_view(vname, ter, az, el)
        m = gb.kind != 0
        v0 = ob.tri[gb.tri, 0]
        ipix = ob.inst[v0]
        IDENT = {}
        for tb, gid in (('A', gidA), ('B', gidB)):
            a = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
            a[gb.objfirst] = gid[ipix[gb.objfirst]]
            IDENT[tb] = a

        # ---- the exact casts -------------------------------------------
        SH = {}
        for tb in ('A', 'B'):
            t0 = time.time()
            SH[tb] = IG.GateShear(ter, ob, GT[tb], az, el, verbose=False)
            p('  cast %s built in %.1f s' % (tb, time.time() - t0))
        bfm, flip, degen = IG.backface_mask(ob, az, el)
        p('  G3 back-face selection at this sun: %s of %s triangles write the map (%.1f%%); '
          '%s triangles had a winding disagreeing with their authored normals and were '
          'oriented by the normals; %d degenerate.'
          % ('{:,}'.format(int(bfm.sum())), '{:,}'.format(tri_all),
             100.0 * int(bfm.sum()) / tri_all, '{:,}'.format(int(flip.sum())), int(degen.sum())))
        sub = IG.TriSubset(ob, bfm)
        t0 = time.time()
        SHBF = IG.GateShear(ter, sub, GT['A'][bfm], az, el, verbose=False)
        p('  cast BACKFACE built in %.1f s' % (time.time() - t0))

        # ---- CONTROLS ---------------------------------------------------
        l0 = np.ones(len(gb.kind), dtype=bool)
        l0[m] = SH['A'].lit_gate(gb.pos[m], IDENT['A'][m], D=0.0)[0]
        p('  CONTROL  the gated cast at D = 0 (no identity) agrees with the LEFT panel on '
          '%.2f%% of the %s decided pixels'
          % (100.0 * float((l0[m] == tr[m]).sum()) / int(m.sum()), '{:,}'.format(int(m.sum()))))
        gi, own = SH['A'].lit_gate(gb.pos[m], IDENT['A'][m], D=INF)
        g1o = SH['A'].lit(gb.pos[m], IDENT['A'][m])[0]
        p('  CONTROL  the gated cast at D = infinity is array-identical to identres '
          'IdentShear.lit (pure identity): %s' % bool(np.array_equal(gi, g1o)))
        mono = True
        prev = None
        for D in (0.0,) + DS + (INF,):
            b = ~SH['A'].lit_gate(gb.pos[m], IDENT['A'][m], D=D)[0]
            if prev is not None and not bool((b <= prev).all()):
                mono = False
            prev = b
        p('  CONTROL  the gated cast is monotone in D -- every shadow at a larger D is a '
          'subset of the shadow at a smaller one: %s' % mono)

        selfm = np.zeros(len(gb.kind), dtype=bool)
        selfm[m] = own
        selfm &= gb.objfirst & ~tr
        p('  self-shadowed object pixels (truth dark, dominant blocker = own group A): %s of %s'
          % ('{:,}'.format(int(selfm.sum())), '{:,}'.format(int(gb.objfirst.sum()))))

        # ---- the maps ---------------------------------------------------
        MAPS = {}
        for tx in (16.0, 64.0):
            for tb in ('A', 'B'):
                t0 = time.time()
                MAPS[(tx, tb)] = MP.ShadowMap(ter, ob, GT[tb], az, el, tx, verbose=False)
                p('  map %s texel %.0f: %d x %d texels, %.1f s'
                  % (tb, tx, MAPS[(tx, tb)].nv, MAPS[(tx, tb)].ns, time.time() - t0))
            MAPS[(tx, 'BF')] = MP.ShadowMap(ter, sub, GT['A'][bfm], az, el, tx, verbose=False)

        # ---- G0's bias, tuned per texel ---------------------------------
        TUNE0 = {}
        for tx, taps, lab in ((16.0, 3, '16 u 3x3 PCF'), (64.0, 1, '64 u nearest')):
            TUNE0[tx] = tune_bias(MAPS[(tx, 'A')], gb, tr, m, IDENT['A'], 0.0, taps,
                                  label='G0 no identity, ' + lab)

        keep = {}

        def run(gid_, group, tb, D, look, bias=None, mp_key=None, desc=''):
            t0 = time.time()
            b = bias or dict(nb=1.0, db=0.5, sl=1.0)
            if look == 'cast':
                sh = SHBF if tb == 'BF' else SH[tb if tb in ('A', 'B') else 'A']
                lit = np.ones(len(gb.kind), dtype=bool)
                lit[m] = sh.lit_gate(gb.pos[m], IDENT['A' if tb in ('-', 'BF') else tb][m],
                                     D=D)[0]
                litf = lit.astype(float)
                nod = 0
            else:
                tx, taps = (16.0, 3) if look == 'map16' else (64.0, 1)
                mp = MAPS[(tx, mp_key or (tb if tb in ('A', 'B') else 'A'))]
                df, _, nd = IG.query_gate(mp, gb.pos[m], gb.nrm[m],
                                          IDENT['A' if tb in ('-', 'BF') else tb][m], D=D,
                                          normal_bias=b['nb'], depth_bias=b['db'],
                                          slope=b['sl'], taps=taps)
                litf = np.ones(len(gb.kind))
                litf[m] = 1.0 - df
                ndf = np.zeros(len(gb.kind), dtype=bool)
                ndf[m] = nd
                nod = int(ndf.sum())
                lit = litf >= 0.5
            st = stats(gb, tr, lit, m, selfm)
            st.update(dict(row=gid_, group=group, table=tb, D=(None if D == INF else D),
                           look=look, view=vname, az=az, el=el, desc=desc, nodata_px=nod,
                           bias=b if look != 'cast' else None, secs=time.time() - t0))
            rows.append(st)
            keep[gid_] = litf
            json.dump(rows, open(JOUT, 'w'), indent=1)
            p('  %-22s | ALL %6.2f%% | terrain %6.2f%% | obj false-DARK %6.2f%%  '
              'obj false-LIT %6.2f%%  (obj total %6.2f%%) | self-shadow lost %6.2f%% | %s'
              % (gid_, st['all'], st['terrain'], st['objects_fd'], st['objects_fl'],
                 st['objects'], st['self_lost'], desc))
            return st

        LOOKS = (('map16', '16 u texel, 3x3 PCF -- the realistic one'),
                 ('cast', 'no map at all -- per-pixel cast, the limit'))
        for look, ldesc in LOOKS:
            p('  ---- lookup: %s' % ldesc)
            bias0 = TUNE0[16.0] if look == 'map16' else None
            run('G0/%s' % look, 'G0', '-', 0.0, look, bias=bias0,
                desc='no identity, tuned bias' if look == 'map16' else 'no identity')
            for tb in ('A', 'B'):
                run('G1/%s/%s' % (tb, look), 'G1', tb, INF, look,
                    desc='pure identity, table %s' % tb)
            for tb in ('A', 'B'):
                for D in DS:
                    run('G2/%s/D%d/%s' % (tb, int(D), look), 'G2', tb, D, look,
                        desc='gate D = %d u, table %s' % (int(D), tb))
            run('G3/%s' % look, 'G3', 'BF', 0.0, look, bias=bias0, mp_key='BF',
                desc='back-face casting, no identity')

        # ---- which D is best, by the two things that matter --------------
        best = {}
        for tb in ('A', 'B'):
            g1 = [r for r in rows if r['view'] == vname and r['group'] == 'G1'
                  and r['table'] == tb and r['look'] == 'map16'][0]
            cand = [r for r in rows if r['view'] == vname and r['group'] == 'G2'
                    and r['table'] == tb and r['look'] == 'map16']
            # the rule: object false-DARK must stay within 0.25 points of G1's
            # (the artefact identity exists to kill), then take the least false-LIT.
            ok = [r for r in cand if r['objects_fd'] <= g1['objects_fd'] + 0.25]
            pick = min(ok, key=lambda r: r['objects_fl']) if ok else None
            best[tb] = pick
            p('  BEST D, table %s: G1 false-DARK %.2f%% false-LIT %.2f%%; of the five gates '
              '%d keep false-DARK within 0.25 points of that, the least false-LIT among them '
              'is D = %s (fD %.2f%% fL %.2f%%)'
              % (tb, g1['objects_fd'], g1['objects_fl'], len(ok),
                 ('%d u' % pick['D']) if pick else 'none',
                 pick['objects_fd'] if pick else float('nan'),
                 pick['objects_fl'] if pick else float('nan')))
        BD = best['A']['D'] if best['A'] else DS[0]

        # ---- G4: the best gate with the normal offset re-tuned -----------
        for tb in ('A', 'B'):
            t4 = tune_bias(MAPS[(16.0, tb)], gb, tr, m, IDENT[tb], BD, 3,
                           label='G4 gate D = %d u, table %s, 16 u 3x3 PCF' % (int(BD), tb))
            run('G4/%s/map16' % tb, 'G4', tb, BD, 'map16', bias=t4,
                desc='gate D = %d u + re-tuned bias, table %s' % (int(BD), tb))

        # ---- the coarse 64 u nearest variants ----------------------------
        p('  ---- lookup: 64 u texel, ONE nearest tap -- the coarse case, where the '
          'mid-wall artefact is visible')
        run('G0/map64', 'G0', '-', 0.0, 'map64', bias=TUNE0[64.0],
            desc='no identity, tuned bias, 64 u nearest')
        for tb in ('A', 'B'):
            run('G1/%s/map64' % tb, 'G1', tb, INF, 'map64',
                desc='pure identity, table %s, 64 u nearest' % tb)
        for tb in ('A', 'B'):
            run('G2/%s/D%d/map64' % (tb, int(BD)), 'G2', tb, BD, 'map64',
                desc='gate D = %d u, table %s, 64 u nearest' % (int(BD), tb))
        # G0 tuned the OTHER way -- the bias cell that fights the artefact hardest,
        # so the report can show what a no-identity build pays to suppress it.
        for tx, look in ((16.0, 'map16'), (64.0, 'map64')):
            lf = TUNE0[tx]['least_fd']
            run('G0strict/%s' % look, 'G0strict', '-', 0.0, look,
                bias=dict(nb=lf['nb'], db=lf['db'], sl=lf['sl']),
                desc='no identity, bias tuned to the LEAST object false-DARK '
                     '(normal %.1f depth %.2f slope %.1f)' % (lf['nb'], lf['db'], lf['sl']))

        # ---- the director's addendum: screen-space shadows on top ---------
        sss_pass(cam, gb, tr, m, selfm, az, el, keep, rows, vname, BD, JOUT)

        np.savez_compressed(LANE + '/lit_%s.npz' % vname,
                            **{k.replace('/', '_'): v.astype(np.float32)
                               for k, v in keep.items()},
                            truth=tr, selfm=selfm, best_d=np.array([BD]))
        p('  saved lit_%s.npz (%d panels) for the picture pass' % (vname, len(keep)))

    p('')
    p('TOTAL %.1f min' % ((time.time() - t00) / 60.0))
    json.dump(rows, open(JOUT, 'w'), indent=1)


if __name__ == '__main__':
    main()
