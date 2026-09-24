#!/usr/bin/env python3
"""lodl_bulk.py -- whole-worldspace decode of a .lodl, for lane WATER1.

The per-texel census needs EVERY level-0 height sample, and the authority
decoder (tests/spells/lodl_open_authority.py) answers one sample per call by
design.  This module imports that decoder -- it does not restate the format --
and adds one bulk path: walk every block once, inflate it once, and scatter its
samples into a numpy grid.

Nothing here re-derives an offset or a pyramid rule.  The class `Lodt` and its
`plane_word` stay the authority; `bulk_height()` is checked AGAINST it on a
random sample set by lodl_bulk_selfcheck (see census_water.py --selfcheck), so
a scatter mistake is loud rather than plausible.

Grids returned are row-0-SOUTH, the file's own order (docs/LODGEN_BTD_FORMAT.md).
"""

import os
import struct
import sys

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_SPELLS = os.path.join(_HERE, '..', '..', 'tests', 'spells')
sys.path.insert(0, os.path.abspath(_SPELLS))

from lodl_open_authority import Lodt  # noqa: E402


def bulk_height_words(d):
    """The whole level-0 height plane as uint16 words, shape (H, W), row 0 south.

    W = cellsX * spc, H = cellsY * spc.  A sample lives exactly once in the
    pyramid, at the coarsest level whose stride divides both coordinates, so
    every entry is written exactly once -- asserted by the coverage count.
    """
    W = d.cellsX * d.spc
    H = d.cellsY * d.spc
    out = np.zeros((H, W), dtype=np.uint16)
    seen = np.zeros((H, W), dtype=bool)
    coarsest = d.levels - 1
    be = d.blockEdge
    half = be // 2

    # index bases per level, matching Lodt.plane_word's `first`
    base = {}
    acc = 0
    for j in range(coarsest, -1, -1):
        base[j] = acc
        acc += d.blocks_x(j) * d.blocks_y(j)
    assert acc == d.nBlocks, 'block count %d, header directory holds %d' % (acc, d.nBlocks)

    # local (wx, wy) tables per level class
    wx_c, wy_c = np.meshgrid(np.arange(be), np.arange(be))     # coarsest: full grid
    wx_c = wx_c.ravel()
    wy_c = wy_c.ravel()
    px, py = np.meshgrid(np.arange(half), np.arange(half))
    px = px.ravel()
    py = py.ravel()
    # k = (py*half + px)*3 + sub, sub 0 right, 1 below, 2 below-right
    wx_f = np.empty(half * half * 3, dtype=np.int64)
    wy_f = np.empty(half * half * 3, dtype=np.int64)
    for sub, (dx, dy) in enumerate([(1, 0), (0, 1), (1, 1)]):
        wx_f[sub::3] = px * 2 + dx
        wy_f[sub::3] = py * 2 + dy

    d.f.seek(d.oDir)
    dirbuf = d.f.read(d.nBlocks * 16)

    for level in range(coarsest, -1, -1):
        bx, by = d.blocks_x(level), d.blocks_y(level)
        if level == coarsest:
            n = be * be
            wx, wy = wx_c, wy_c
        else:
            n = half * half * 3
            wx, wy = wx_f, wy_f
        for bj in range(by):
            for bi in range(bx):
                idx = base[level] + bj * bx + bi
                off, csz, usz = struct.unpack_from('<QII', dirbuf, idx * 16)
                d.f.seek(off)
                import zlib
                raw = zlib.decompress(d.f.read(csz))
                assert len(raw) == usz
                words = np.frombuffer(raw, dtype='<u2', count=n, offset=0)  # plane 0 = height
                gx = (bi * be + wx) << level
                gy = (bj * be + wy) << level
                m = (gx < W) & (gy < H)
                out[gy[m], gx[m]] = words[m]
                seen[gy[m], gx[m]] = True
    assert seen.all(), 'pyramid left %d of %d level-0 samples unwritten' % (
        int((~seen).sum()), W * H)
    return out


def heights(d, words=None):
    """World-unit heights, float32, from the stored words."""
    if words is None:
        words = bulk_height_words(d)
    return (words.astype(np.float32) - 32767.0) * d.quantum


def cell_table(d):
    """(minH, maxH, waterH, waterType, flags) as arrays shaped (cellsY, cellsX)."""
    d.f.seek(d.oCell)
    raw = d.f.read(d.cellsX * d.cellsY * 16)
    rec = np.frombuffer(raw, dtype=np.dtype([
        ('lo', '<f4'), ('hi', '<f4'), ('wh', '<f4'),
        ('wt', '<u2'), ('fl', '<u2')]))
    rec = rec.reshape(d.cellsY, d.cellsX)
    return (rec['lo'].copy(), rec['hi'].copy(), rec['wh'].copy(),
            rec['wt'].copy(), rec['fl'].copy())


def watr_table(d):
    d.f.seek(d.oWatr)
    return np.frombuffer(d.f.read(d.nWatr * 4), dtype='<u4').copy()


def default_water(d):
    """(height, WATR form id) from the v2 header, or None at version 1."""
    if d.version < 2:
        return None
    d.f.seek(0x98)
    h, t = struct.unpack('<fI', d.f.read(8))
    return h, t


def open_lodl(path):
    return Lodt(path)
