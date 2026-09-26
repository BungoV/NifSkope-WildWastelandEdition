"""ROADS1 work item 2: are the pavement's 2x2 tile edges slab joints in the texture, or a bake artifact?

  python tile_edges.py <bake tag>

1. the texture: mean luma of sidewalkconcrete01_d along u, v = 0 and 0.5 (the joints) against the whole texture.
2. the sheet against the independent raster (pave_faith.py INGAME, which samples the texture and nothing else):
   per-texel luma correlation over the pavement sample set, and the sheet's own dark-line texels -- if the
   sheet's lines are the texture's, the raster predicts them.
3. the bake's own grid: sheet minus raster on texels next to a cell edge (every 256 texels) and next to a
   16-texel block edge, against the interior. A bake seam shows as a step there; a texture joint does not."""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import faith_cmp as fc  # noqa: E402
import roadgeo as rg  # noqa: E402

LW = np.array([0.2126, 0.7152, 0.0722])


def main():
    R = rg.Reader()
    t = R.texture('landscape/roads/sidewalkconcrete01_d.dds')
    a = t.mips[0][:, :, :3] @ LW * 255
    n = a.shape[0]
    j = np.zeros(n, bool)
    for c in (0, n // 2):
        j[(np.arange(c - 8, c + 8)) % n] = True
    print('texture %dx%d mean %.1f | joint columns (u=0, 0.5, 16 texels wide) %.1f | joint rows %.1f | slab interior %.1f' % (
        n, n, a.mean(), a[:, j].mean(), a[j, :].mean(), a[~j][:, ~j].mean()))
    ing = np.load(os.path.join(HERE, 'out', 'raster_INGAME.npz'))
    win, meta = ing['win'], ing['meta']
    ok = win >= 0
    sw = np.zeros(win.shape, bool)
    sw[ok] = meta[win[ok], 0] == 1
    sel = sw & (ing['cov'] >= 1.0) & (meta[np.maximum(win, 0), 1] == 0)
    li = ing['rgb'] @ LW
    for tag in sys.argv[1:]:
        ls = fc.sheet(tag) @ LW
        x, y = li[sel], ls[sel]
        r = np.corrcoef(x, y)[0, 1]
        dark = sel & (li < li[sel].mean() - 30)
        print('%s: pavement texels %d, corr(sheet, raster) %.3f; texels the RASTER calls dark (>30 below its mean): %d,'
              ' raster %.1f sheet %.1f; the rest raster %.1f sheet %.1f' % (
                  tag, sel.sum(), r, dark.sum(), li[dark].mean(), ls[dark].mean(), li[sel & ~dark].mean(),
                  ls[sel & ~dark].mean()))
        sdark = sel & (ls < ls[sel].mean() - 30)
        print('   texels the SHEET calls dark: %d, of them the raster also dark (>15 below its mean): %.1f %%' % (
            sdark.sum(), 100.0 * (sdark & (li < li[sel].mean() - 15)).sum() / max(1, sdark.sum())))
        H, W = ls.shape
        yy, xx = np.mgrid[0:H, 0:W]
        d = ls - li
        for per, name in ((256, 'cell edge'), (16, '16-texel block edge'), (4, 'BC 4-texel block edge')):
            edge = ((xx % per) == 0) | ((xx % per) == per - 1) | ((yy % per) == 0) | ((yy % per) == per - 1)
            e, i = sel & edge, sel & ~edge
            print('   sheet - raster at %-22s %+.2f (n %d) vs interior %+.2f (n %d)' % (
                name, d[e].mean(), e.sum(), d[i].mean(), i.sum()))


if __name__ == '__main__':
    main()
