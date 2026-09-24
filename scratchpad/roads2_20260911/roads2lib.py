"""Lane ROADS2's own measurement helpers.

Two things ROADS1's readers do not keep and this lane needs:

  * VERTEX COLOURS, and specifically vertex ALPHA.  tests/spells/gltf_nifread.py
    walks past them (`if va & VA_COLORS: p += 4`), so the vertex buffer is
    re-walked here from the shape's own header to pull the four colour bytes.
    The header prologue is re-typed from that reader's `_read_shape`, field for
    field, so the offset is derived and not guessed.
  * bAlphaBlend and the blend functions out of the BGSM/BGEM, which
    scratchpad/roads1_20260911/matinfo.py reads and discards.

Nothing here is shared with src/lodgen.cpp.
"""

import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
R1 = os.path.join(HERE, '..', 'roads1_20260911')
sys.path.insert(0, R1)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))

from gltf_nifread import Nif, VA_COLORS, VA_VERTEX, VA_UV, VA_UV2, \
    VA_NORMALS, VA_TANGENTS, VA_FULLPREC, VA_SKINNED, VA_EYEDATA  # noqa: E402
import matinfo                                                    # noqa: E402

SEP = chr(92)


def shape_vertex_data_offset(nif, shape):
    """Byte offset of the shape's vertex buffer, and its alpha-property ref.

    Re-typed from gltf_nifread.Nif._read_shape: after the AVObject come the
    bounding sphere (16), Skin (4), Shader Property (4), ALPHA PROPERTY (4),
    the vertex-desc qword (8), Num Triangles (4), Num Vertices (2) and
    Data Size (4).
    """
    i = shape['block']
    start = nif.start[i]
    name, t, r, s, o = nif._avobject(start)
    o += 16
    o += 4                              # Skin
    o += 4                              # Shader Property
    alphaProp = nif._i32(o); o += 4     # Alpha Property
    o += 8                              # Vertex Desc
    o += 4                              # Num Triangles
    o += 2                              # Num Vertices
    o += 4                              # Data Size
    return o, alphaProp


def vertex_colors(nif, shape):
    """(N,4) float RGBA in 0..1, or None when the shape carries no colours."""
    va = shape['va']
    if not (va & VA_COLORS):
        return None
    base, _ = shape_vertex_data_offset(nif, shape)
    stride = shape['stride']
    nv = shape['numVerts']
    # the byte offset of the colour field inside one vertex, same field order
    off = 0
    if va & VA_VERTEX:
        off += 16 if (va & VA_FULLPREC) else 8
    if va & VA_UV:
        off += 4
    if va & VA_UV2:
        off += 4
    if va & VA_NORMALS:
        off += 4
    if va & VA_TANGENTS:
        off += 4
    out = np.empty((nv, 4), dtype=np.float64)
    for k in range(nv):
        b = struct.unpack_from('<4B', nif.data, base + k * stride + off)
        out[k] = [x / 255.0 for x in b]
    return out


def alpha_property(nif, ref):
    """(flags, threshold) of a NiAlphaProperty block, or None."""
    if ref is None or ref < 0 or ref >= nif.numBlocks:
        return None
    if nif.type[ref] != 'NiAlphaProperty':
        return None
    o = nif.start[ref]
    o += 4                              # Name
    ne = nif._u32(o); o += 4 + 4 * ne    # Extra Data List
    o += 4                              # Controller
    flags = struct.unpack_from('<H', nif.data, o)[0]; o += 2
    thr = nif.data[o]
    return flags, thr


def read_material_full(path):
    """matinfo.read_material plus bAlphaBlend and the two blend functions."""
    b = open(path, 'rb').read()
    if b[:4] not in (b'BGSM', b'BGEM'):
        return None
    r = matinfo._R(b[4:])
    v = r.u32()
    r.u32()
    r.f32(); r.f32(); r.f32(); r.f32()
    alpha = r.f32()
    alphaBlend = r.u8()
    srcBlend = r.u32()
    dstBlend = r.u32()
    alphaTestRef = r.u8()
    alphaTest = r.u8()
    r.u8(); r.u8()
    r.u8(); r.u8()
    decal = r.u8()
    twoSided = r.u8()
    decalNoFade = r.u8()
    nonOccluder = r.u8()
    return dict(kind=b[:4].decode('latin-1'), version=v, alpha=alpha,
                alphaBlend=bool(alphaBlend), srcBlend=srcBlend,
                dstBlend=dstBlend, alphaTest=bool(alphaTest),
                alphaTestRef=alphaTestRef, decal=bool(decal),
                twoSided=bool(twoSided), decalNoFade=bool(decalNoFade),
                nonOccluder=bool(nonOccluder))


class Dds(object):
    """BC1/BC3 mip-0 decoder, independent of the generator: the same shape as
    tests/spells/lodgen_terrain_model.py's reader, re-typed so this lane does
    not depend on a file another lane may be editing."""

    def __init__(self, path):
        b = open(path, 'rb').read()
        assert b[:4] == b'DDS ', path
        h = struct.unpack_from('<7I', b, 4)
        self.height, self.width = h[2], h[3]
        self.mips = max(1, struct.unpack_from('<I', b, 4 + 24)[0])
        fourcc = b[84:88]
        off = 128
        if fourcc == b'DX10':
            dxgi = struct.unpack_from('<I', b, 128)[0]
            off = 148
            fourcc = {71: b'DXT1', 72: b'DXT1', 77: b'DXT5', 78: b'DXT5'}.get(dxgi, b'DXT5')
        self.fmt = fourcc.decode('latin-1')
        self.rgb = self._decode(b, off, fourcc)

    def _decode(self, b, off, fourcc):
        w, h = self.width, self.height
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
                p = np.empty((4, 3), dtype=np.float64)
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
                        if py >= h or px >= w:
                            continue
                        out[py, px] = p[(bits >> (2 * (4 * y + x))) & 3]
        return out

    def lum(self):
        return (0.2126 * self.rgb[:, :, 0] + 0.7152 * self.rgb[:, :, 1]
                + 0.0722 * self.rgb[:, :, 2])
