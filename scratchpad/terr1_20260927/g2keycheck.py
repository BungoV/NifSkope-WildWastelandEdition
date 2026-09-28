"""TERR1 G2: is the diag record's key sound? Share of texels with a record, per class, on the stored sheets.

usage: python g2keycheck.py <stamp_diag.bin> <on VT.2.lodt> <off VT.2.lodt> <noroads VT.2.lodt> <out.json>
classes (stored texels incl. border, every tile):
  colour   decoded colour differs roads vs no-roads by > 2 levels (a stamped texel for sure): record share must be ~1
  moved    msn normal moved > 1 degree ON vs OFF
  moved_nocolour  moved, and the colour's whole 4x4 block is identical roads vs no-roads (the G2 violators' texels)
  quiet    neither: record share should be ~0 outside the roads
"""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import ang


def main():
    rec = np.fromfile(sys.argv[1], dtype='<f4').reshape(-1, 6)
    on, off, nr = VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3]), VM.Sheets(sys.argv[4])
    kx = np.round(rec[:, 0] * 2).astype(np.int64); ky = np.round(rec[:, 1] * 2).astype(np.int64)
    keys = set((kx * 10_000_000 + ky).tolist())
    D, B, C = on.D, on.B, on.C
    tot = {k: [0, 0] for k in ('colour', 'moved', 'moved_nocolour', 'quiet')}
    for ty in range(on.v.tilesY):
        for tx in range(on.v.tilesX):
            c1 = on.tile(tx, ty, 1, True)[..., :3]; c0 = nr.tile(tx, ty, 1, True)[..., :3]
            cd = np.abs(c1 - c0).max(-1) > 2
            cblk = (c1 != c0).any(-1).reshape(D // 4, 4, D // 4, 4).any(axis=(1, 3))
            cblk = np.repeat(np.repeat(cblk, 4, 0), 4, 1)
            a = ang(VM.msn_world(on.tile(tx, ty, 2, True)), VM.msn_world(off.tile(tx, ty, 2, True)))
            mv = a > 1.0
            jj, ii = np.mgrid[0:D, 0:D]
            wx = VM.WX0 + (tx * C + (ii - B) + 0.5) * VM.UPT
            wy = VM.WYTOP - (ty * C + (jj - B) + 0.5) * VM.UPT
            k = np.round(wx * 2).astype(np.int64) * 10_000_000 + np.round(wy * 2).astype(np.int64)
            has = np.fromiter((v in keys for v in k.ravel().tolist()), bool, k.size).reshape(D, D)
            for name, m in (('colour', cd), ('moved', mv), ('moved_nocolour', mv & ~cblk), ('quiet', ~cd & ~mv)):
                tot[name][0] += int(m.sum()); tot[name][1] += int((m & has).sum())
    out = {k: dict(n=v[0], with_record=v[1], share=(v[1] / v[0]) if v[0] else None) for k, v in tot.items()}
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    print(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
