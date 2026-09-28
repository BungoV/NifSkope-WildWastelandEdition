"""TERR1 G2 per BLOCK: does every G2 violating msn block hold a texel the stamp really wrote (a diag record)?

usage: python g2block.py <stamp_diag.bin> <on VT.2.lodt> <off VT.2.lodt> <noroads VT.2.lodt> <out.json>

BC1 couples the 16 texels of a block: stamping ONE texel moves the block's endpoints and so its block-mates, which have
no record of their own. g2stampdiag.py read per texel and so counted those block-mates as "no record" (reading B).
The per-block question: a violating block (msn bytes ON != OFF, colour bytes roads == no-roads) with NO record texel
at all would be a stamp outside the stamp -- a code defect. A block with a record texel is a real stamp whose colour
change did not survive the colour sheet's BC1; for those the record's colour move (levels, before rounding) and weight
are summarised.
Control: the same per-block test on blocks whose msn did NOT change must find (almost) no record texels with w > 0
whose stamp moved the normal -- reported as `unchanged_blocks_with_record`.
"""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import sheet_blocks


def main():
    rec = np.fromfile(sys.argv[1], dtype='<f4').reshape(-1, 6)
    on, off, nr = VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3]), VM.Sheets(sys.argv[4])
    kx = np.round(rec[:, 0] * 2).astype(np.int64); ky = np.round(rec[:, 1] * 2).astype(np.int64)
    look = {}
    for k, r in zip((kx * 10_000_000 + ky).tolist(), rec):
        if k not in look or r[2] > look[k][2]:
            look[k] = r
    D, B, C = on.D, on.B, on.C
    nb = D // 4
    viol = 0; viol_rec = 0; rows = []
    unchanged = 0; unchanged_rec = 0
    for ty in range(on.v.tilesY):
        for tx in range(on.v.tilesX):
            md = (sheet_blocks(on, tx, ty, 2) != sheet_blocks(off, tx, ty, 2)).any(-1)
            cd = (sheet_blocks(on, tx, ty, 1) != sheet_blocks(nr, tx, ty, 1)).any(-1)
            jj, ii = np.mgrid[0:D, 0:D]
            wx = VM.WX0 + (tx * C + (ii - B) + 0.5) * VM.UPT
            wy = VM.WYTOP - (ty * C + (jj - B) + 0.5) * VM.UPT
            k = np.round(wx * 2).astype(np.int64) * 10_000_000 + np.round(wy * 2).astype(np.int64)
            has = np.fromiter((v in look for v in k.ravel().tolist()), bool, k.size).reshape(D, D)
            hb = has.reshape(nb, 4, nb, 4).any(axis=(1, 3))
            unchanged += int((~md).sum()); unchanged_rec += int((~md & hb).sum())
            for bj, bi in np.argwhere(md & ~cd):
                viol += 1
                recs = [look[int(k[bj * 4 + dj, bi * 4 + di])] for dj in range(4) for di in range(4)
                        if has[bj * 4 + dj, bi * 4 + di]]
                if recs:
                    viol_rec += 1
                    r = max(recs, key=lambda q: q[2])
                    rows.append(dict(tile=[tx, ty], block=[int(bj), int(bi)], n_rec=len(recs), w=float(r[2]),
                                     colour_move=float(r[3]), raGeom=float(r[4]), nA=float(r[5]),
                                     colour_move_max=float(max(q[3] for q in recs))))
                else:
                    rows.append(dict(tile=[tx, ty], block=[int(bj), int(bi)], n_rec=0))
    cm = np.array([r['colour_move_max'] for r in rows if r['n_rec']])
    out = dict(violating_blocks=viol, with_record=viol_rec, without_record=viol - viol_rec,
               n_rec_hist={str(n): sum(1 for r in rows if r['n_rec'] == n) for n in range(17)},
               colour_move_max_median=float(np.median(cm)) if len(cm) else None,
               colour_move_max_lt2=int((cm < 2).sum()), colour_move_max_2to8=int(((cm >= 2) & (cm < 8)).sum()),
               colour_move_max_ge8=int((cm >= 8).sum()),
               unchanged_blocks=unchanged, unchanged_blocks_with_record=unchanged_rec, rows=rows)
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    print(json.dumps({k: v for k, v in out.items() if k != 'rows'}, indent=1))


if __name__ == '__main__':
    main()
