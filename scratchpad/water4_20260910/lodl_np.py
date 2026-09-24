# -*- coding: utf-8 -*-
"""lodl_np.py -- one body's region of a `.lodl` version-3 plane as a numpy
array, through lane WATER2's INDEPENDENT decoder (`lodl_v3_authority.py`),
which shares no code with the writer or the marking tool.

    from lodl_np import body_region
    d, b, ids, flow, (px0, py0) = body_region(path, 3)

`ids` and `flow` are uint16 arrays over the body's cell bbox at the BODY
plane's rate; row 0 is SOUTH, as in the file.  Also reads the dye plane this
lane adds (the reserved word at 0xF4, section bit 1 << 8), if present.
"""
import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'water2_20260909'))
from lodl_v3_authority import LodlV3          # noqa: E402

SECT_DYE = 1 << 8


class Lodl(LodlV3):
    def __init__(self, path):
        LodlV3.__init__(self, path)
        self.dyeStore = None
        if self.version >= 3 and (self.sect & SECT_DYE) and self.reserved:
            self.dyeStore = self._read_store(self.reserved, 4)

    def store_region(self, s, px0, py0, w, h):
        """Samples of store `s` over [px0, px0+w) x [py0, py0+h) as an array."""
        e = s['tileEdge']
        bps = s['bps']
        dt = {1: np.uint8, 2: np.uint16, 4: np.uint32}[bps]
        out = np.zeros((h, w), dtype=dt)
        tx0, tx1 = px0 // e, (px0 + w - 1) // e
        ty0, ty1 = py0 // e, (py0 + h - 1) // e
        for ty in range(ty0, ty1 + 1):
            for tx in range(tx0, tx1 + 1):
                if tx < 0 or ty < 0 or tx >= s['tilesX'] or ty >= s['tilesY']:
                    continue
                kind, v = self.tile(s, tx, ty)
                if kind == 'uniform':
                    t = np.full((e, e), v, dtype=dt)
                else:
                    t = np.frombuffer(v, dtype=dt).reshape(e, e)
                # clip the tile into the region
                x0, y0 = tx * e, ty * e
                ax0, ay0 = max(x0, px0), max(y0, py0)
                ax1, ay1 = min(x0 + e, px0 + w), min(y0 + e, py0 + h)
                if ax1 <= ax0 or ay1 <= ay0:
                    continue
                out[ay0 - py0:ay1 - py0, ax0 - px0:ax1 - px0] = \
                    t[ay0 - y0:ay1 - y0, ax0 - x0:ax1 - x0]
        return out

    def body_bbox_texels(self, b):
        px0 = (b['x0'] - self.minX) * self.bodyS
        py0 = (b['y0'] - self.minY) * self.bodyS
        w = (b['x1'] - b['x0'] + 1) * self.bodyS
        h = (b['y1'] - b['y0'] + 1) * self.bodyS
        return px0, py0, w, h

    def height_region(self, px0, py0, w, h):
        """Terrain height at body-plane texels, from the level-0 plane.

        The level-0 height plane is the progressive block pyramid; the writer's
        `LodtFile::height( gx, gy )` walks it.  This decoder does not read the
        pyramid, so the depth used here comes from the C++ side when needed.
        Returns None to say so rather than inventing a flat bed."""
        return None


def body_region(path, wanted):
    d = Lodl(path)
    b = d.bodies[wanted - 1]
    assert b['id'] == wanted
    px0, py0, w, h = d.body_bbox_texels(b)
    ids = d.store_region(d.idStore, px0, py0, w, h)
    assert d.flowS == d.bodyS, 'flow rate %d != body rate %d' % (d.flowS, d.bodyS)
    flow = d.store_region(d.flowStore, px0, py0, w, h)
    return d, b, ids, flow, (px0, py0)


def stroke_points(d):
    """The strokes out of the store: list of dicts with body, kind, flags,
    speed, width, pts (world), extra (bytes past the points)."""
    raw = getattr(d, 'strokeRaw', b'')
    out = []
    if len(raw) < 4:
        return out
    n = struct.unpack_from('<I', raw, 0)[0]
    at = 4
    for i in range(n):
        rb = struct.unpack_from('<I', raw, at)[0]
        body, kind, flags = struct.unpack_from('<HBB', raw, at + 4)
        speed, width = struct.unpack_from('<ff', raw, at + 8)
        npts = struct.unpack_from('<H', raw, at + 16)[0]
        pts = [struct.unpack_from('<ff', raw, at + 20 + k * 8) for k in range(npts)]
        extra = raw[at + 20 + npts * 8:at + rb]
        out.append(dict(body=body, kind=kind, flags=flags, speed=speed, width=width,
                        pts=pts, extra=extra))
        at += rb
    return out
