"""Decode the MASK sheet of one tile, picking the block codec the tile actually
uses.

`lodgen_vt_check.sheetMipBytes` already knows a cover tile's mask sheet is BC3
(16 bytes a block, the alpha half first); `decode_bc1` does not.  Reading a BC3
tile with the BC1 decoder walks 8 bytes a block through a 16-byte stream, which
is garbage from the second block on -- so this module exists and nothing in this
lane calls `decode_bc1` on the mask again.
"""
import struct
import sys
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_vt_check as V


def _rgb(b, o, four_colour_only):
    c0, c1, bits = struct.unpack_from('<HHI', b, o)
    e0, e1 = V.rgb565(c0), V.rgb565(c1)
    if four_colour_only or c0 > c1:
        pal = [e0, e1,
               tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
               tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
    else:
        pal = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
    return pal, bits


def decode_rgb(b, off, w, h, stride, colour_at):
    """`stride` is the block stride (8 for BC1, 16 for BC3) and `colour_at` the
    offset of the colour half inside the block (0 for BC1, 8 for BC3)."""
    out = [[(0, 0, 0)] * w for _ in range(h)]
    bw, bh = w // 4, h // 4
    four = stride == 16
    for by in range(bh):
        for bx in range(bw):
            pal, bits = _rgb(b, off + (by * bw + bx) * stride + colour_at, four)
            for i in range(16):
                out[by * 4 + (i >> 2)][bx * 4 + (i & 3)] = pal[(bits >> (2 * i)) & 3]
    return out


def mask_sheet_index(v):
    return [s for s in range(v.sheetCount) if v.sheets[s]['role'] == 5][0]


def mask_rows(v, index, mip=0):
    """The mask sheet's decoded RGB rows -- B is the AO byte."""
    e = v.table[index]
    p = v.payload(index)
    if p is None:
        return None, None
    ms = mask_sheet_index(v)
    cover = bool(e['flags'] & 2)
    sd = v.sheets[ms]
    fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
    stride = 16 if fmt in (77, 78) else 8
    o = v.sheetOffset(cover, ms, mip)
    side = v.stored >> mip
    return decode_rgb(p, o, side, side, stride, 8 if stride == 16 else 0), fmt
