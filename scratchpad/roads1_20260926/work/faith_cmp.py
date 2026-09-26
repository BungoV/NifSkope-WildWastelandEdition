"""ROADS1 faithfulness gate: a bake's VT.2 colour sheet vs the independent re-rasterisation (pave_faith.py).

  python faith_cmp.py <bake tag> [<bake tag> ...]      (RULE=new: against the ROADS1 placement-rule rasters)

Sample set: texels whose max-z winner (INGAME raster) is a pavement shape at full coverage, and whose 3x3
neighbourhood has that same winner (no triangle / piece / BC1-block edge mixing), the same set for every bake.
Reports mean luma (Rec.709, 0..255) of: the in-game expectation, the old-code expectation, each bake's sheet;
and the per-texel mean |sheet - expectation|. Also the swapped subset on its own."""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread  # noqa: E402

BAKES = r'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/roads1b/bake/'
CX0, CY0, CX1, CY1 = -8, -12, 3, -1
LW = np.array([0.2126, 0.7152, 0.0722])


def sheet(tag, level=2):
    v = vtread.Vt(BAKES + tag + '/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.%d.lodt' % level)
    m, wW, nN = v.mosaic(CX0, CY0, CX1, CY1, 1)
    per = v.content // v.levelDim
    c0 = (CX0 - wW) * per
    r0 = (nN - (CY1 + 1)) * per
    return m[r0:r0 + (CY1 - CY0 + 1) * per, c0:c0 + (CX1 - CX0 + 1) * per, :3].astype(float)


def main():
    sfx = '_NEWRULE' if os.environ.get('RULE') == 'new' else ''
    print('rasters: 6382a09a placement rule' if not sfx else "rasters: ROADS1's placement rule (has-LOD ground pieces in)")
    ing = np.load(os.path.join(HERE, 'out', 'raster_INGAME%s.npz' % sfx))
    old = np.load(os.path.join(HERE, 'out', 'raster_OLD%s.npz' % sfx))
    win = ing['win']
    meta = ing['meta']
    sw = np.zeros(win.shape, bool)
    swapped = np.zeros(win.shape, bool)
    ok = win >= 0
    sw[ok] = meta[win[ok], 0] == 1
    swapped[ok] = meta[win[ok], 1] == 1
    same = np.ones(win.shape, bool)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            same &= np.roll(np.roll(win, dy, 0), dx, 1) == win
    same[0, :] = same[-1, :] = same[:, 0] = same[:, -1] = False
    sel = sw & (ing['cov'] >= 1.0) & same & (old['win'] >= 0)
    sel_sw = sel & swapped
    print('pavement texels (winner = pavement, full coverage, 3x3 interior): %d; of them swapped in game: %d' % (sel.sum(), sel_sw.sum()))
    li = ing['rgb'] @ LW
    lo = old['rgb'] @ LW
    print('  expected IN GAME   lum %.1f' % li[sel].mean(), '| swapped subset %.1f' % (li[sel_sw].mean() if sel_sw.any() else -1))
    print('  expected OLD CODE  lum %.1f' % lo[sel].mean(), '| swapped subset %.1f' % (lo[sel_sw].mean() if sel_sw.any() else -1))
    for tag in sys.argv[1:]:
        s = sheet(tag)
        ls = s @ LW
        d_in = np.abs(ls - li)[sel]
        d_old = np.abs(ls - lo)[sel]
        print('  %-14s lum %.1f | mean|sheet-ingame| %.2f  mean|sheet-oldcode| %.2f | swapped subset: sheet %.1f, |sheet-ingame| %.2f, |sheet-old| %.2f' % (
            tag, ls[sel].mean(), d_in.mean(), d_old.mean(),
            ls[sel_sw].mean() if sel_sw.any() else -1,
            np.abs(ls - li)[sel_sw].mean() if sel_sw.any() else -1,
            np.abs(ls - lo)[sel_sw].mean() if sel_sw.any() else -1))
        rgbd = np.abs(s - ing['rgb'])[sel].mean(0)
        print('      per-channel mean |sheet - ingame| R %.2f G %.2f B %.2f' % tuple(rgbd))


if __name__ == '__main__':
    main()
