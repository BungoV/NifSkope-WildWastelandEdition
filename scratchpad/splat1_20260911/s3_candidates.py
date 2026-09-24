"""SPLAT1 section 3 -- the candidate split.

Three candidates for the excess local variance, separated so each is measured
where the other two cannot act:

  MIP / SAMPLING   -- offline re-bakes with one term at a time removed.
  GRASS TINT       -- the cover byte says where the tint COULD NOT have acted
                      (cover == 0 -> weight 0 -> the albedo is untouched), and
                      one of the two tiles was baked with no cover plane at all.
  VCLR             -- cells that carry no VCLR record cannot have been
                      multiplied by one.
"""
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import splatlib as S                                          # noqa: E402
import offline_bake as B                                      # noqa: E402


def cover_plane(path):
    """The cover byte = alpha of `_data.DDS`, and ONLY when the sheet is DXT5
    and its dwReserved1 carries 'WWCV' (src/lodgen.cpp:6933-6959). A DXT1
    sheet, or an unstamped one, means NO COVER PLANE WAS WRITTEN."""
    b = open(path, 'rb').read(160)
    h = struct.unpack_from('<31I', b, 4)
    fourcc = b[84:88]
    stamp = h[7]
    if fourcc != b'DXT5' or stamp != 0x56435757:
        return None, '%s stamp 0x%08X -> no cover plane' % (fourcc.decode(), stamp)
    d = S.Dds(path)
    return d.level(0)[:, :, 3], 'DXT5 WWCV, coverFull=%d' % (h[8] & 0xFFFFFF)


for (cx0, cy0) in ((-20, 24), (-20, 20)):
    print('=== chunk (%d,%d) ===' % (cx0, cy0))
    vanL = S.lum(S.Dds(S.van_sheet(cx0, cy0)).level(0))
    ourL = S.lum(S.Dds(S.OURS[(cx0, cy0)]).level(0))
    lvV = S.local_var(vanL).mean()
    lvO = S.local_var(ourL).mean()
    excess = lvO - lvV
    print('  vanilla lv %.2f   ours lv %.2f   EXCESS %.2f' % (lvV, lvO, excess))

    # ---- the grass tint, from the bake's own cover plane -------------------
    cov, why = cover_plane(S.COVER[(cx0, cy0)])
    print('  cover plane: %s' % why)
    if cov is None:
        print('    -> THE GRASS TINT CANNOT HAVE ACTED ANYWHERE ON THIS TILE.')
        print('       ours lv %.2f is a tint-free number.' % lvO)
    else:
        m0 = cov <= 0.5
        m1 = cov > 0.5
        lv = S.local_var(ourL)
        lvv = S.local_var(vanL)
        print('    cover == 0 on %6d texels (%.1f%%): ours lv %7.2f   vanilla lv %7.2f   excess %7.2f'
              % (m0.sum(), 100.0 * m0.mean(), lv[m0].mean(), lvv[m0].mean(),
                 lv[m0].mean() - lvv[m0].mean()))
        print('    cover  > 0 on %6d texels (%.1f%%): ours lv %7.2f   vanilla lv %7.2f   excess %7.2f'
              % (m1.sum(), 100.0 * m1.mean(), lv[m1].mean(), lvv[m1].mean(),
                 lv[m1].mean() - lvv[m1].mean()))

    # ---- VCLR -------------------------------------------------------------
    rows = B.vclr_map(cx0, cy0, 4)
    have = [r for r in rows if r[2]]
    print('  VCLR: %d of 16 cells carry one%s'
          % (len(have),
             ('' if not have else '; range %d..%d over those cells'
              % (min(r[3] for r in have), max(r[4] for r in have)))))
    if have and len(have) < 16:
        res = ourL.shape[0]
        span = 4 * 4096.0
        py, px = np.mgrid[0:res, 0:res]
        wy = cy0 * 4096.0 + (1.0 - (py + 0.5) / res) * span
        wx = cx0 * 4096.0 + ((px + 0.5) / res) * span
        mv = np.zeros(ourL.shape, bool)
        for (cx, cy, hasv, _, _) in rows:
            if hasv:
                mv |= ((wx >= cx * 4096.0) & (wx < (cx + 1) * 4096.0)
                       & (wy >= cy * 4096.0) & (wy < (cy + 1) * 4096.0))
        lv = S.local_var(ourL)
        lvv = S.local_var(vanL)
        print('    VCLR cells    %6d texels: ours lv %7.2f  vanilla lv %7.2f  excess %7.2f'
              % (mv.sum(), lv[mv].mean(), lvv[mv].mean(), lv[mv].mean() - lvv[mv].mean()))
        print('    no-VCLR cells %6d texels: ours lv %7.2f  vanilla lv %7.2f  excess %7.2f'
              % ((~mv).sum(), lv[~mv].mean(), lvv[~mv].mean(),
                 lv[~mv].mean() - lvv[~mv].mean()))

    # ---- the sampling, term by term, offline ------------------------------
    print('  offline re-bakes (whole tile, luminance local variance):')
    base = None
    for tag, kw in (('the law as it stands', dict(mip='code')),
                    ('exact footprint box mean', dict(mip='box')),
                    ('base layer only (no 17-grid)', dict(mip='code', layers=False)),
                    ('no VCLR multiply', dict(mip='code', vclr=False)),
                    ('texture global mean (no texture detail)', dict(mip='mean')),
                    ('code mip + 1', dict(mip=1.0)),
                    ('code mip + 2', dict(mip=2.0)),
                    ('code mip + 3', dict(mip=3.0))):
        sheet = B.bake(cx0, cy0, 4, **kw)
        L = S.lum(np.dstack([sheet, np.full(sheet.shape[:2], 255.0)]))
        lv = S.local_var(L).mean()
        if base is None:
            base = lv
        print('    %-40s lv %7.2f   (%+6.2f vs the law; %+6.2f vs vanilla)'
              % (tag, lv, lv - base, lv - lvV))
    print('')
