"""TERR1 stand-in for `--dump-object-ao` while the NifSkope turn is blocked: the object height field (max / min z
per 128-unit square) built from the level-0 LOD triangles of ao2 reg_x7 (the ray cast's own geometry), written in
the OBJH dump format so skycast.py / canyon.py read it unchanged. NOT the bake's field (that one is built from the
full models); a preliminary law choice only, re-run on the real dump when a bake can run.

usage: python objh_from_lod.py <out.bin>
"""
import sys, struct
import numpy as np
import skycast as SC

CELL = 128.0


def main():
    L = SC.ND.read_lodo(SC.OBJ + '.lodo')
    T = SC.ND.read_lodi(SC.OBJ + '.lodi')
    tris, n = SC.world_triangles(L, T, -8, -12, 3, -1)
    gx0, gy0 = int(-10 * 4096 / CELL), int(-14 * 4096 / CELL)
    gw = gh = int(16 * 4096 / CELL)
    HI = np.full((gh, gw), -3.0e38, dtype=np.float32)
    LO = np.full((gh, gw), 3.0e38, dtype=np.float32)
    # sample every triangle on a barycentric grid no coarser than 32 units
    e = np.maximum(np.linalg.norm(tris[:, 1] - tris[:, 0], axis=1), np.linalg.norm(tris[:, 2] - tris[:, 0], axis=1))
    e = np.maximum(e, np.linalg.norm(tris[:, 2] - tris[:, 1], axis=1))
    k = np.clip(np.ceil(e / 32.0).astype(int), 1, 256)
    for kk in np.unique(k):
        sel = tris[k == kk]
        a, b = np.mgrid[0:kk + 1, 0:kk + 1]
        m = (a + b) <= kk
        u = (a[m] / kk)[None, :, None]
        v = (b[m] / kk)[None, :, None]
        p = sel[:, None, 0] * (1 - u - v) + sel[:, None, 1] * u + sel[:, None, 2] * v
        p = p.reshape(-1, 3)
        gx = np.floor(p[:, 0] / CELL).astype(np.int64) - gx0
        gy = np.floor(p[:, 1] / CELL).astype(np.int64) - gy0
        ok = (gx >= 0) & (gy >= 0) & (gx < gw) & (gy < gh)
        np.maximum.at(HI, (gy[ok], gx[ok]), p[ok, 2].astype(np.float32))
        np.minimum.at(LO, (gy[ok], gx[ok]), p[ok, 2].astype(np.float32))
    empty = HI < -1e37
    HI[empty] = -3.0e38
    LO[empty] = 3.0e38
    with open(sys.argv[1], 'wb') as f:
        f.write(b'OBJH' + struct.pack('<4if', gx0, gy0, gw, gh, CELL))
        f.write(HI.astype('<f4').tobytes())
        f.write(LO.astype('<f4').tobytes())
    print('triangles', len(tris), 'placements', n, 'occupied squares', int((~empty).sum()))


if __name__ == '__main__':
    main()
