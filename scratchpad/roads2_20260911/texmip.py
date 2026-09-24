"""Read ONE mip level of a BC1/BC3 DDS, and the whole-texture average.

Needed because roads2lib.Dds decodes mip 0 only, and the question in front of
the lane is what the road's diffuse looks like at the mip the bake asks for
(~8 of 11) against what it looks like averaged flat (mip 11, 1x1).
"""

import os
import struct

import numpy as np

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
SEP = chr(92)


def _blocks(fourcc, w, h):
    bs = 8 if fourcc == b'DXT1' else 16
    return ((w + 3) // 4) * ((h + 3) // 4) * bs


def _decode_bc(b, off, fourcc, w, h):
    bw, bh = (w + 3) // 4, (h + 3) // 4
    bs = 8 if fourcc == b'DXT1' else 16
    out = np.zeros((h, w, 3), dtype=np.float64)
    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * bs
            if fourcc != b'DXT1':
                o += 8
            c0, c1 = struct.unpack_from('<2H', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]
            p = np.empty((4, 3))
            for k, c in enumerate((c0, c1)):
                p[k] = [((c >> 11) & 31) * 255.0 / 31.0,
                        ((c >> 5) & 63) * 255.0 / 63.0,
                        (c & 31) * 255.0 / 31.0]
            if c0 > c1 or fourcc != b'DXT1':
                p[2] = (2 * p[0] + p[1]) / 3.0
                p[3] = (p[0] + 2 * p[1]) / 3.0
            else:
                p[2] = (p[0] + p[1]) / 2.0
                p[3] = 0.0
            for y in range(4):
                for x in range(4):
                    py, px = by * 4 + y, bx * 4 + x
                    if py < h and px < w:
                        out[py, px] = p[(bits >> (2 * (4 * y + x))) & 3]
    return out


class Tex:
    def __init__(self, rel):
        p = rel.replace(SEP, '/')
        for cand in (os.path.join(DATA, p), os.path.join(DATA, 'textures', p)):
            if os.path.isfile(cand):
                p = cand
                break
        else:
            raise IOError(rel)
        self.path = p
        b = open(p, 'rb').read()
        assert b[:4] == b'DDS '
        self.height, self.width = struct.unpack_from('<2I', b, 12)
        self.mips = max(1, struct.unpack_from('<I', b, 28)[0])
        fourcc = b[84:88]
        off = 128
        if fourcc == b'DX10':
            dxgi = struct.unpack_from('<I', b, 128)[0]
            off = 148
            fourcc = {71: b'DXT1', 72: b'DXT1', 77: b'DXT5',
                      78: b'DXT5'}.get(dxgi, b'DXT5')
        self._b, self._off, self._fourcc = b, off, fourcc
        self._cache = {}

    def level(self, m):
        """The mip as an (h,w,3) float array, 0..255."""
        m = int(max(0, min(m, self.mips - 1)))
        if m in self._cache:
            return self._cache[m]
        off = self._off
        w, h = self.width, self.height
        for k in range(m):
            off += _blocks(self._fourcc, w, h)
            w = max(1, w >> 1)
            h = max(1, h >> 1)
        a = _decode_bc(self._b, off, self._fourcc, w, h)
        self._cache[m] = a
        return a

    def sample(self, u, v, m):
        """Bilinear, wrapping, on mip m -- the shape getPixelT has."""
        a = self.level(m)
        h, w = a.shape[:2]
        x = (u % 1.0) * w - 0.5
        y = (v % 1.0) * h - 0.5
        x0, y0 = int(np.floor(x)), int(np.floor(y))
        fx, fy = x - x0, y - y0
        x0 %= w; y0 %= h
        x1, y1 = (x0 + 1) % w, (y0 + 1) % h
        return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x1] * fx * (1 - fy)
                + a[y1, x0] * (1 - fx) * fy + a[y1, x1] * fx * fy)

    def average(self):
        return self.level(self.mips - 1).reshape(-1, 3).mean(axis=0)
