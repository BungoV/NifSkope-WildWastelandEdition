"""Gate F5 on the sheets the bake actually WROTE, plus the re-encode refuter.

Three questions, each with the floor it is judged against:

  1. is the BC1 4x4 block grid gone from the written `_msn`?  Measured with the
     same `grid_line` statistic used on the cache PNGs (mean |neighbour
     difference| on the lines where index % period == 0, over the mean
     elsewhere; 1.0 = no grid).  The BEFORE is the rung's own `_msn` -- which on
     this chunk is Bethesda's BC1-era sheet byte for byte -- at period 4.
  2. is the normal unit length after the write?  Reported in levels of 255,
     beside vanilla's own sheet, which is NOT unit length after a block decode.
  3. THE REFUTER for "uncompressed, because a DXT re-encode brings the blocks
     back": the cleaned 2K sheet is run through the tree's OWN block-endpoint
     rule (min/max luminance endpoints, 4-colour palette, per 4x4 block --
     lodgenEncodeBC1Block, and a BC3 colour block is the same block) and the
     grid line is measured again.  If the re-encoded sheet reads no grid, the
     reason for writing uncompressed is wrong and this file says so.
"""
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sheet_probe import probe                                     # noqa: E402


def grid_line(img, period):
    """Block-grid residue at `period` -- the SAME statistic f5_cache.py used on
    the PNGs, repeated here rather than imported because importing that module
    would re-run its whole 16-sheet measurement."""
    a = img.astype(np.float64)
    if a.ndim == 2:
        a = a[:, :, None]
    dx = np.abs(a[:, 1:] - a[:, :-1]).mean(axis=(0, 2))
    xs = np.arange(1, a.shape[1])
    dy = np.abs(a[1:, :] - a[:-1, :]).mean(axis=(1, 2))
    ys = np.arange(1, a.shape[0])
    return float(0.5 * (dx[xs % period == 0].mean() / dx[xs % period != 0].mean()
                        + dy[ys % period == 0].mean() / dy[ys % period != 0].mean()))

B = os.path.join(HERE, 'bake')
VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CH = 'Commonwealth.4.-20.24'


def bc_roundtrip(rgb):
    """The tree's own colour-block rule, applied and decoded again.

    lodgenEncodeBC1Block picks the block's min- and max-LUMINANCE texels as the
    two endpoints, quantises them to RGB565 and indexes every texel to the
    nearest of the four palette entries.  A BC3 colour block is the same block,
    so this is the re-encode a `--msn-cache` sheet would get if it were written
    as DXT5 instead of uncompressed.
    """
    h, w, _ = rgb.shape
    a = rgb.astype(np.float64)
    bh, bw = h // 4, w // 4
    blk = a[:bh * 4, :bw * 4].reshape(bh, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4)
    blk = blk.reshape(bh, bw, 16, 3)
    lum = blk[:, :, :, 0] * 0.299 + blk[:, :, :, 1] * 0.587 + blk[:, :, :, 2] * 0.114
    lo = np.argmin(lum, axis=2)
    hi = np.argmax(lum, axis=2)
    p0 = np.take_along_axis(blk, hi[:, :, None, None], 2)[:, :, 0, :]
    p1 = np.take_along_axis(blk, lo[:, :, None, None], 2)[:, :, 0, :]

    def q565(c):
        r = np.floor(c[..., 0] / 255.0 * 31 + 0.5)
        g = np.floor(c[..., 1] / 255.0 * 63 + 0.5)
        b = np.floor(c[..., 2] / 255.0 * 31 + 0.5)
        return np.stack([(r * 255 + 15) // 31, (g * 255 + 31) // 63,
                         (b * 255 + 15) // 31], -1)

    p0, p1 = q565(p0), q565(p1)
    pal = np.stack([p0, p1, (2 * p0 + p1 + 1) // 3, (p0 + 2 * p1 + 1) // 3], 2)
    d = ((blk[:, :, :, None, :] - pal[:, :, None, :, :]) ** 2).sum(-1)
    idx = np.argmin(d, axis=3)
    out = np.take_along_axis(pal, idx[:, :, :, None], 2)
    out = out.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4)
    return out.reshape(bh * 4, bw * 4, 3).astype(np.uint8)


res = {}
paths = {
    'rung_msn': os.path.join(B, 'rung', 'tex', CH + '_msn.DDS'),
    'cache_msn': os.path.join(B, 'cache', 'tex', CH + '_msn.DDS'),
    'van_msn': os.path.join(VAN, CH + '_msn.DDS'),
    'rung_col': os.path.join(B, 'rung', 'tex', CH + '.DDS'),
    'vanfmt_col': os.path.join(B, 'vanfmt', 'tex', CH + '.DDS'),
    'van_col': os.path.join(VAN, CH + '.DDS'),
}
rgbs = {}
for k, p in paths.items():
    o, rgb = probe(p)
    rgbs[k] = rgb
    res[k] = {kk: o[kk] for kk in ('bytes', 'w', 'h', 'declared_mips',
                                   'present_mips', 'fourcc', 'alpha_min',
                                   'alpha_max', 'alpha_distinct',
                                   'unit_len_mean', 'unit_len_maxdev_levels',
                                   'chan_mean')}
    res[k]['dxgi_name'] = o.get('dxgi_name', '')

# 1 + 3: the block grid, on the east channel, each at its own block period
res['grid'] = {
    'rung_msn_p4': grid_line(rgbs['rung_msn'][:, :, 0], 4),
    'van_msn_p4': grid_line(rgbs['van_msn'][:, :, 0], 4),
    'cache_msn_p16': grid_line(rgbs['cache_msn'][:, :, 0], 16),
    'cache_msn_p4': grid_line(rgbs['cache_msn'][:, :, 0], 4),
    'cache_reencoded_p4': grid_line(bc_roundtrip(rgbs['cache_msn'])[:, :, 0], 4),
}
print(json.dumps(res, indent=1))
json.dump(res, open(os.path.join(HERE, 'f5_written.json'), 'w'), indent=1)
print('wrote f5_written.json')
