"""TERR1 sky law identity gate: where NO object is in the law's reach, mask B must be the terrain-only byte.

usage: python opengate.py <on VT.2.lodt> <off VT.2.lodt> <objh dump .bin> <out.json>

The law (lodgenSkyDirBlocked) reads the object lattice out to 1458 units; where every square within that reach
(plus one square of slack for the lattice's own size) is empty, its increment is exactly 0.0f, so the byte must equal
the terrain-only bake's. Mask B lives in a BC-compressed channel (role 5, channel 2): a darkened texel moves its
block's endpoints and can move its block-mates. So the gate is taken on WHOLE 4x4 blocks whose 16 texels all have an
empty reach ("free blocks"): decoded mask B identical at every texel. Texels with an empty reach that share a block
with a darkened texel are reported separately (BC coupling, not the law).
Refuter: one planted texel change in a free block must count exactly 1.
"""
import sys, json
import numpy as np
from scipy.ndimage import binary_dilation
import vtmosaic as VM
from canyon import load_objh


def main():
    on, off = VM.Sheets(sys.argv[1]), VM.Sheets(sys.argv[2])
    gx0, gy0, gw, gh, cell, HI, LO = load_objh(sys.argv[3])
    assert on.B % 4 == 0, 'border not block-aligned'
    a = on.mosaic(5)[..., 2].astype(np.int32)
    b = off.mosaic(5)[..., 2].astype(np.int32)
    N = a.shape[0]
    occ = HI > -1e29
    R = int(np.ceil(1458.0 / cell)) + 1
    yy, xx = np.mgrid[-R:R + 1, -R:R + 1]
    near = binary_dilation(occ, structure=(xx * xx + yy * yy) <= R * R)
    jj, ii = np.mgrid[0:N, 0:N]
    x = VM.WX0 + (ii + 0.5) * VM.UPT
    y = VM.WYTOP - (jj + 0.5) * VM.UPT
    gx = np.floor(x / cell).astype(np.int64) - gx0
    gy = np.floor(y / cell).astype(np.int64) - gy0
    inside = (gx >= R) & (gy >= R) & (gx < gw - R) & (gy < gh - R)
    tnear = np.where(inside, near[np.clip(gy, 0, gh - 1), np.clip(gx, 0, gw - 1)], True)
    free_t = ~tnear
    nb = N // 4
    free_b = free_t.reshape(nb, 4, nb, 4).all(axis=(1, 3))
    free_bt = np.repeat(np.repeat(free_b, 4, 0), 4, 1)
    d = a != b

    def count(dd):
        return int((dd & free_bt).sum())
    viol = count(d)
    # refuter: plant one change in the first free block
    bj, bi = np.argwhere(free_b)[len(np.argwhere(free_b)) // 2]
    a2 = a.copy(); a2[bj * 4 + 1, bi * 4 + 2] ^= 1
    planted = count(a2 != b) - viol
    coupled = free_t & ~free_bt
    out = dict(texels=int(N * N), free_texels=int(free_t.sum()), free_blocks=int(free_b.sum()),
               free_block_texels=int(free_bt.sum()), violations=viol, refuter_planted=planted,
               coupled_free_texels=int(coupled.sum()), coupled_moved=int((d & coupled).sum()),
               coupled_max_absdiff=int(np.abs(a - b)[coupled].max()) if coupled.any() else 0,
               near_texels=int(tnear.sum()), near_moved=int((d & tnear).sum()),
               near_darker=int(((a < b) & tnear).sum()), near_brighter=int(((a > b) & tnear).sum()))
    out['gate'] = 'PASS' if viol == 0 and planted == 1 else 'FAIL'
    json.dump(out, open(sys.argv[4], 'w'), indent=1)
    for k, v in out.items():
        print(k, v)


if __name__ == '__main__':
    main()
