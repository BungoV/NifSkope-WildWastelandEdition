#!/usr/bin/env python
"""Decode two BC1/BC3 DDS sheets and diff them per texel, with the locality
statistic V9a needs: how far every differing texel sits from the chunk's OUTER
boundary, and how far from the interior dim-2 TILE seams.

Independent of the writer it judges: the BC decode here is re-typed from the
format, not imported from lodgen.

  python ddsdiff.py A.DDS B.DDS [--alpha] [--label NAME]
"""
import struct, sys

def dds_read(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h = struct.unpack_from('<7I', b, 4)          # size, flags, height, width, pitch, depth, mips
    height, width, mips = h[2], h[3], h[6]
    fourcc = b[84:88]
    off = 128
    if fourcc == b'DX10':
        off = 148
    return b, off, width, height, max(1, mips), fourcc

def _c565(c):
    r = ((c >> 11) & 31); g = ((c >> 5) & 63); bl = c & 31
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (bl * 255 + 15) // 31)

def decode_bc(data, off, w, h, bc3):
    """Return (rgb, alpha) as lists of length w*h; rgb entries are (r,g,b)."""
    rgb = [(0, 0, 0)] * (w * h)
    alpha = [255] * (w * h)
    bw, bh = (w + 3) // 4, (h + 3) // 4
    stride = 16 if bc3 else 8
    p = off
    for by in range(bh):
        for bx in range(bw):
            blk = data[p:p + stride]
            p += stride
            if bc3:
                a0, a1 = blk[0], blk[1]
                bits = int.from_bytes(blk[2:8], 'little')
                if a0 > a1:
                    at = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else:
                    at = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                cblk = blk[8:16]
            else:
                at = None
                cblk = blk
            c0, c1 = struct.unpack_from('<HH', cblk, 0)
            idx = struct.unpack_from('<I', cblk, 4)[0]
            p0, p1 = _c565(c0), _c565(c1)
            if c0 > c1 or bc3:
                pal = [p0, p1,
                       tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1,
                       tuple((p0[k] + p1[k]) // 2 for k in range(3)),
                       (0, 0, 0)]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x >= w or y >= h:
                        continue
                    sel = (idx >> (2 * (j * 4 + i))) & 3
                    rgb[y * w + x] = pal[sel]
                    if bc3:
                        asel = (bits >> (3 * (j * 4 + i))) & 7
                        alpha[y * w + x] = at[asel]
                    elif c0 <= c1 and sel == 3:
                        alpha[y * w + x] = 0
    return rgb, alpha

def load_mip0(path):
    b, off, w, h, mips, fourcc = dds_read(path)
    bc3 = (fourcc == b'DXT5')
    rgb, a = decode_bc(b, off, w, h, bc3)
    return rgb, a, w, h, fourcc.decode('latin1'), mips
