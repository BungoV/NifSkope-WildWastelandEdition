"""TERR1: the Boston VT.2 sheets as whole-box mosaics, decoded by maps1's offline reader
(tests/spells/lodgen_vt_check.Lodv container parse + vtbake1 vtread numpy BC1/BC3), sharing no code with the viewer.

Box: cells W -8 .. E 3, S -12 .. N -1 (12 x 12 cells), VT level dim 2 -> 6 x 6 tiles, content 512, 16 world units a
texel, rows north-up (tile ty 0 is north). Mosaic texel (j, i) has its centre at
    x = -8*4096 + (i + 0.5) * 16,   y = 0 - (j + 0.5) * 16.
"""
import sys
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtbake1_20260923')
import lodgen_vt_check as V
import vtread as VR

WX0, WYTOP, UPT = -8 * 4096.0, 0.0, 16.0


class Sheets:
    def __init__(self, path):
        self.v = V.Lodv(path)
        v = self.v
        self.role = {v.sheets[i]['role']: i for i in range(v.sheetCount)}
        self.D, self.B, self.C = v.stored, v.border, v.content

    def tile(self, tx, ty, role, stored=False):
        v = self.v
        D, B, C = self.D, self.B, self.C
        idx = ty * v.tilesX + tx
        e = v.table[idx]
        p = v.payload(idx)
        si = self.role[role]
        sd = v.sheets[si]
        cover = bool(e['flags'] & 2)
        o = v.sheetOffset(cover, si, 0)
        if role == 4:
            a = np.frombuffer(p, dtype='<u2', count=D * D, offset=o).reshape(D, D).astype(np.int32)
            return a if stored else a[B:B + C, B:B + C]
        fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
        bb = 16 if fmt in (77, 78) else 8
        nb = D // 4
        blk = np.frombuffer(p, dtype=np.uint8, count=nb * nb * bb, offset=o).reshape(nb, nb, bb)
        rgb = VR.decode_bc1_blocks(blk[..., bb - 8:], bb == 16).astype(np.int32)
        if bb == 16:
            a = VR.decode_bc3_alpha(blk[..., :8]).astype(np.int32)
        else:
            a = np.full(rgb.shape[:2], -1, dtype=np.int32)
        out = np.concatenate([rgb, a[..., None]], axis=2)
        return out if stored else out[B:B + C, B:B + C]

    def mosaic(self, role):
        v = self.v
        rows = []
        for ty in range(v.tilesY):
            rows.append(np.concatenate([self.tile(tx, ty, role) for tx in range(v.tilesX)], axis=1))
        return np.concatenate(rows, axis=0)

    def raw_sheet_bytes(self, tx, ty, role):
        """the stored bytes of one sheet of one tile, mip 0 (for byte-identity on a mask)."""
        v = self.v
        idx = ty * v.tilesX + tx
        e = v.table[idx]
        p = v.payload(idx)
        si = self.role[role]
        cover = bool(e['flags'] & 2)
        return p, v.sheetOffset(cover, si, 0)


def height_units(h16):
    """R16 height sheet -> game units (pixel = height/8 + 32767)."""
    return (h16.astype(np.float64) - 32767.0) * 8.0


def msn_world(m):
    """msn texels (R east, G up, B north) -> float unit-ish vectors east, north, up."""
    e = m[..., 0] / 255.0 * 2 - 1
    up = m[..., 1] / 255.0 * 2 - 1
    n = m[..., 2] / 255.0 * 2 - 1
    return np.stack([e, n, up], axis=-1)


def texel_of(x, y):
    return int((WYTOP - y) // UPT), int((x - WX0) // UPT)
