#!/usr/bin/env python3
"""IDENTRES -- the driver.  Four rows, two views, one table, nine pictures.

Every number printed here also lands in `run.log` and `rows.json`; the report
quotes nothing that is not in one of those two files.

THE PALETTE.  identprox's right panel wore a teal wash (`shade.NODATA`) on any
pixel for which the shadow map held NO texel at all -- almost all of it the
distant terrain OUTSIDE the map's own (v, s) box, which stops at the chunk plus
a 2,048 u margin.  It marked "the map was never asked about this pixel", not
"this pixel is in shadow".  It is switched off here so the two panels carry one
palette and the eye compares shadows only; the count it used to mark is printed
in each caption instead.
"""
import json
import numpy as np
import os
import sys
import time

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identres_20260919'
IMG = LANE + '/images'
IP = ROOT + '/scratchpad/identprox_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (LANE, IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4core as H                                            # noqa: E402
import h4map as MP                                            # noqa: E402
import hwy_render as HR                                       # noqa: E402
import hwycams                                                # noqa: E402
import cams as CAMS                                           # noqa: E402
import join as JN                                             # noqa: E402
import shade as SH                                            # noqa: E402
from scene import Terrain, Objects                            # noqa: E402
import identres as IR                                         # noqa: E402

LOG = open(LANE + '/run.log', 'a')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


# the four rows, in the order the report prints them
ROWS = [('R1', '64 u texel, 1 nearest tap, flat bias', dict(texel=64.0, taps=1, slope=0.0)),
        ('R2', '16 u texel, 3x3 PCF, slope-scaled bias', dict(texel=16.0, taps=3, slope=1.0)),
        ('R3', '4 u texel, 3x3 PCF, slope-scaled bias', dict(texel=4.0, taps=3, slope=1.0)),
        ('R4', 'NO MAP -- per-pixel sun cast keyed on identity', dict(ray=True))]

VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0)]

# a smoke run picks a subset; the full run sets neither
_R = os.environ.get('IDENTRES_ROWS')
_V = os.environ.get('IDENTRES_VIEWS')
if _R:
    ROWS = [r for r in ROWS if r[0] in _R.split(',')]
if _V:
    VIEWS = [v for v in VIEWS if v[0] in _V.split(',')]
# a partial run never overwrites the full run's table
JOUT = LANE + ('/rows_partial.json' if (_R or _V) else '/rows.json')


def load_view(name, ter, ob, az, el):
    """The cached G-buffer and LEFT panel of the lane that already built them.
    READ ONLY: nothing is written back into another lane's folder."""
    if name == 'hwydeck':
        cam = hwycams.build(ter, 1600, 900)[name]
        gp, tp = IP + '/gb_%s.npz' % name, IP + '/truth_%s_az%03d_el%02d.npy' % (name, int(az), int(el))
    else:
        cam = CAMS.build(ter, 1600, 900)[name]
        gp, tp = H4 + '/gb_%s.npz' % name, H4 + '/truth_%s_az%03d_el%02d.npy' % (name, int(az), int(el))
    for q in (gp, tp):
        if not os.path.exists(q):
            raise SystemExit('missing cache ' + q)
    return cam, H._GB(cam, np.load(gp)), np.load(tp)


def main():
    t00 = time.time()
    os.makedirs(IMG, exist_ok=True)
    p('=' * 110)
    p('IDENTRES  %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    low = np.array([m.lower() for m in P.model])
    deck = np.array([('hwdouble' in s) for s in low])
    col = np.array([('hwonrampcol' in s) for s in low])

    # today's identity, exactly `hwy_render.main`'s construction
    u, inv = np.unique(P.shipped, return_inverse=True)
    gid = (inv + 1).astype(np.int64)
    gtri = gid[ob.inst][ob.tri[:, 0]]
    p('identity: today = the shipped .lodi v7 group table, %d groups over %d placements'
      % (len(u), len(gid)))

    rows = []
    for vname, az, el in VIEWS:
        p('')
        p('-' * 110)
        p('VIEW %s   sun az %.0f el %.0f' % (vname, az, el))
        cam, gb, tr = load_view(vname, ter, ob, az, el)
        m = gb.kind != 0
        v0 = ob.tri[gb.tri, 0]
        ipix = ob.inst[v0]
        ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
        ident[gb.objfirst] = gid[ipix[gb.objfirst]]

        # ---- R4's structure first: it also supplies the SELF-SHADOW denominator
        t0 = time.time()
        sh = IR.IdentShear(ter, ob, gtri, az, el)
        p('  identshear built in %.1f s' % (time.time() - t0))
        lit4 = np.ones(len(gb.kind), dtype=bool)
        l4, own = sh.lit(gb.pos[m], ident[m])
        lit4[m] = l4
        # CONTROL: the same cast with the identity rule OFF must be the LEFT panel
        loff = np.ones(len(gb.kind), dtype=bool)
        loff[m] = sh.lit(gb.pos[m], ident[m], use_identity=False)[0]
        agree = 100.0 * float((loff[m] == tr[m]).sum()) / int(m.sum())
        p('  CONTROL  the identity-carrying cast with the rule switched OFF agrees with the '
          'LEFT panel on %.2f%% of the %s decided pixels' % (agree, '{:,}'.format(int(m.sum()))))

        # SELF = object pixels the truth puts in shadow whose dominant blocker in
        # that same cast carries the receiver's OWN group.
        selfm = np.zeros(len(gb.kind), dtype=bool)
        selfm[m] = own
        selfm &= gb.objfirst & ~tr
        p('  self-shadowed object pixels (truth dark, dominant blocker = own group): %s of %s object px'
          % ('{:,}'.format(int(selfm.sum())), '{:,}'.format(int(gb.objfirst.sum()))))

        keep = {}
        common = None
        for rid, rdesc, cfg in ROWS:
            t0 = time.time()
            if cfg.get('ray'):
                litf = lit4.astype(float)
                nodpx = 0
            else:
                mp = MP.ShadowMap(ter, ob, gtri, az, el, cfg['texel'], verbose=True)
                df, _, nod = IR.query_pcf(mp, gb.pos[m], gb.nrm[m], ident[m],
                                          slope=cfg['slope'], taps=cfg['taps'])
                if rid == 'R1':
                    d0, _, _, has0 = HR.query(mp, gb.pos[m], gb.nrm[m], ident[m], slope=0.0)
                    same = bool(np.array_equal(d0, df > 0.5)) and bool(np.array_equal(~has0, nod))
                    p('  CONTROL  R1 one tap identical to identprox hwy_render.query: %s' % same)
                litf = np.ones(len(gb.kind))
                litf[m] = 1.0 - df
                nodf = np.zeros(len(gb.kind), dtype=bool)
                nodf[m] = nod
                nodpx = int(nodf.sum())
                # CONTROL: the SAME map read with R1's own lookup (one nearest
                # tap, flat bias), so the only thing that changed from R1 is the
                # texel.  R2/R3 move texel, filter and bias together because the
                # brief asks for the realistic combination; this isolates one.
                d1, _, _ = IR.query_pcf(mp, gb.pos[m], gb.nrm[m], ident[m], slope=0.0, taps=1)
                l1 = np.ones(len(gb.kind), dtype=bool)
                l1[m] = d1 < 0.5
                st1 = {}
                for nm, mm in (('all', m), ('terrain', gb.terfirst), ('objects', gb.objfirst)):
                    k = int(mm.sum())
                    st1[nm] = 100.0 * float((tr[mm] != l1[mm]).sum()) / k
                    st1[nm + '_fd'] = 100.0 * float((tr[mm] & ~l1[mm]).sum()) / k
                    st1[nm + '_fl'] = 100.0 * float((~tr[mm] & l1[mm]).sum()) / k
                del mp
            lit = litf >= 0.5
            st = {}
            for nm, mm in (('all', m), ('terrain', gb.terfirst), ('objects', gb.objfirst)):
                k = int(mm.sum())
                st[nm] = 100.0 * float((tr[mm] != lit[mm]).sum()) / k if k else float('nan')
                # which SIDE the disagreement falls on: false DARK = truth lit,
                # row dark (acne / a fat caster); false LIT = truth dark, row lit
                # (a shadow the lookup lost).
                st[nm + '_fd'] = 100.0 * float((tr[mm] & ~lit[mm]).sum()) / k if k else float('nan')
                st[nm + '_fl'] = 100.0 * float((~tr[mm] & lit[mm]).sum()) / k if k else float('nan')
            # identprox's OWN denominator: it dropped the pixels the map held no
            # texel for.  Kept only so R1 can be shown to reproduce the picture
            # bungo looked at; the table above uses one denominator for all rows.
            if not cfg.get('ray'):
                dec = m & ~nodf
                for nm, mm in (('all', dec), ('terrain', dec & gb.terfirst),
                               ('objects', dec & gb.objfirst)):
                    kk = int(mm.sum())
                    st[nm + '_ip'] = 100.0 * float((tr[mm] != lit[mm]).sum()) / kk if kk else float('nan')
                if rid == 'R1':
                    ref = ('   -- identprox/images/hwy_today_hwydeck_az180_el10_t64.png says '
                           'ALL 11.29% terrain 13.13% objects 10.21%'
                           if vname == 'hwydeck' else
                           '   -- (no published identprox picture for this camera/sun)')
                    p('  CONTROL  R1 on identprox\'s own denominator (no-data pixels dropped): '
                      'ALL %.2f%% terrain %.2f%% objects %.2f%%%s'
                      % (st['all_ip'], st['terrain_ip'], st['objects_ip'], ref))
            k = int(selfm.sum())
            st['self_lost'] = 100.0 * float(lit[selfm].sum()) / k if k else float('nan')
            st['self_px'] = k
            st['nodata_px'] = nodpx
            st['secs'] = time.time() - t0
            st.update(dict(row=rid, desc=rdesc, view=vname, az=az, el=el))
            rows.append(st)
            p('  %-3s %-46s | ALL %6.2f%%  terrain %6.2f%%  objects %6.2f%% | self-shadow lost %6.2f%% '
              'of %8s px | no-map-data %9s px | on the pixels the map HAD data for: ALL %6.2f%% '
              'terrain %6.2f%% objects %6.2f%% | %.1f s'
              % (rid, rdesc, st['all'], st['terrain'], st['objects'], st['self_lost'],
                 '{:,}'.format(st['self_px']), '{:,}'.format(nodpx),
                 st.get('all_ip', st['all']), st.get('terrain_ip', st['terrain']),
                 st.get('objects_ip', st['objects']), st['secs']))
            p('      side of the disagreement: terrain false-DARK %5.2f%% false-LIT %5.2f%% | '
              'objects false-DARK %5.2f%% false-LIT %5.2f%%'
              % (st['terrain_fd'], st['terrain_fl'], st['objects_fd'], st['objects_fl']))
            if not cfg.get('ray'):
                for kk, vv in st1.items():
                    st['t1_' + kk] = vv
                p('      CONTROL the same map read R1\'s way (1 tap, flat bias -- only the TEXEL '
                  'differs from R1): ALL %5.2f%% terrain %5.2f%% (fD %5.2f%% fL %5.2f%%) '
                  'objects %5.2f%% (fD %5.2f%% fL %5.2f%%)'
                  % (st1['all'], st1['terrain'], st1['terrain_fd'], st1['terrain_fl'],
                     st1['objects'], st1['objects_fd'], st1['objects_fl']))
            json.dump(rows, open(JOUT, 'w'), indent=1)
            picture(gb, cam, tr, litf, az, el, rid, rdesc, st)
            keep[rid] = litf
            if not cfg.get('ray'):
                common = common & ~nodf if common is not None else ~nodf
        # CONTROL for the terrain column: every map row judged on the SAME
        # pixels -- the ones all three maps held a texel for.  The map's extent
        # (the chunk plus a 2,048 u margin) shrinks that set as the texel gets
        # finer, so a rising terrain figure on each row's own denominator can be
        # the denominator moving rather than the texel.
        if common is not None:
            cm_ = m & common
            p('  CONTROL  every row on ONE denominator -- the %s pixels all three maps held a texel '
              'for (%.1f%% of the frame):' % ('{:,}'.format(int(cm_.sum())),
                                              100.0 * int(cm_.sum()) / int(m.sum())))
            for rid in [r[0] for r in ROWS]:
                lt = keep[rid] >= 0.5
                out = []
                for nm, mm in (('ALL', cm_), ('terrain', cm_ & gb.terfirst),
                               ('objects', cm_ & gb.objfirst)):
                    k = int(mm.sum())
                    out.append('%s %5.2f%% (false-DARK %5.2f%% false-LIT %5.2f%%)'
                               % (nm, 100.0 * float((tr[mm] != lt[mm]).sum()) / k,
                                  100.0 * float((tr[mm] & ~lt[mm]).sum()) / k,
                                  100.0 * float((~tr[mm] & lt[mm]).sum()) / k))
                p('      %-3s %s' % (rid, '  '.join(out)))
        if 'R1' in keep and 'R4' in keep and vname == 'hwydeck':
            zoom(gb, cam, tr, keep, ipix, deck, col, vname, az, el)
    p('')
    p('TOTAL %.1f min' % ((time.time() - t00) / 60.0))
    open(LANE + '/DONE', 'w').write('identres done %s\n' % time.strftime('%Y-%m-%d %H:%M:%S'))


def picture(gb, cam, tr, litf, az, el, rid, rdesc, st):
    L = SH.shade(gb, tr.astype(float), el, az)
    R = SH.shade(gb, litf, el, az)              # nodata=None: ONE palette, both panels
    cap = ('sun azimuth %.0f deg elevation %.0f deg   |   camera "%s" %dx%d   |   identity = '
           "today's shipped .lodi v7 groups, unchanged\n"
           'RIGHT panel lookup:  %s  --  %s\n'
           'disagree lit/shadow   ALL %.2f%%   terrain %.2f%%   objects %.2f%%\n'
           'self-shadow lost %.2f%% of the %s object pixels the sun puts in shadow behind their OWN group   |   '
           'pixels the lookup had no data for: %s (drawn lit, NOT tinted)'
           % (az, el, cam.name, cam.w, cam.h, rid, rdesc,
              st['all'], st['terrain'], st['objects'], st['self_lost'],
              '{:,}'.format(st['self_px']), '{:,}'.format(st['nodata_px'])))
    SH.pair(SH.to8(L, cam.w, cam.h), SH.to8(R, cam.w, cam.h), cam.w, cam.h,
            'LEFT  ray-cast sun (truth)', 'RIGHT  %s  %s' % (rid, rdesc), cap,
            '%s/identres_%s_%s_az%03d_el%02d.png' % (IMG, cam.name, rid, int(az), int(el)),
            sub_l='heightmap + 29,587 .lodo triangles; shadow ray with a 16 u footprint',
            sub_r='an object is dark only when the nearer caster carries a DIFFERENT identity')


def zoom(gb, cam, tr, keep, ipix, deck, col, vname, az, el):
    """R1 vs R4 over the deck-and-columns crop, 2x nearest so texels are legible."""
    w, h = cam.w, cam.h
    dk = (gb.objfirst & deck[ipix]).reshape(h, w)
    cm = (gb.objfirst & col[ipix]).reshape(h, w)
    src = cm if int(cm.sum()) else dk
    yy, xx = np.nonzero(src)
    if yy.size == 0:
        p('  zoom: no deck/column pixels in view %s, skipped' % vname)
        return
    CW, CH = 560, 340
    cx = int(np.clip(np.median(xx), CW // 2, w - CW // 2))
    cy = int(np.clip(np.median(yy), CH // 2, h - CH // 2))
    x0, y0 = cx - CW // 2, cy - CH // 2
    p('  zoom crop x %d..%d y %d..%d, centred on the %s support-column pixels; the crop holds %s '
      'deck pixels and %s column pixels'
      % (x0, x0 + CW, y0, y0 + CH, '{:,}'.format(int(cm.sum())),
         '{:,}'.format(int(dk[y0:y0 + CH, x0:x0 + CW].sum())),
         '{:,}'.format(int(cm[y0:y0 + CH, x0:x0 + CW].sum()))))
    out = {}
    for rid in ('R1', 'R4'):
        im = SH.to8(SH.shade(gb, keep[rid], el, az), w, h)[y0:y0 + CH, x0:x0 + CW]
        out[rid] = np.repeat(np.repeat(im, 2, axis=0), 2, axis=1)
    cap = ('the SAME %d x %d pixel crop, centred on the elevated deck and the support columns under '
           'its far end, magnified 2x with no smoothing, camera "%s", sun az %.0f el %.0f.\n'
           'LEFT: the 64 u texel nearest tap of identprox.  RIGHT: no map at all -- the per-pixel sun '
           'cast keyed on the SAME identity data.\n'
           'The identity table is identical in both.  Only the lookup resolution differs, and the '
           'blocks are gone.' % (CW, CH, cam.name, az, el))
    SH.pair(out['R1'], out['R4'], CW * 2, CH * 2,
            'R1  64 u texel, nearest', 'R4  no map, per-pixel cast', cap,
            '%s/identres_zoom_%s_R1_vs_R4.png' % (IMG, cam.name),
            sub_l='one tap, flat bias -- the picture bungo asked about',
            sub_r='same identity rule, no texel anywhere in the answer')


if __name__ == '__main__':
    main()
