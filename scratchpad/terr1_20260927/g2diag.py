"""TERR1 continuation: what are the G2 violations (msn BC1 blocks that differ stamp ON vs OFF where the colour
blocks do not differ roads vs no-roads)?

Per violating block: tile, block, world centre, border or content, the decoded normal angle ON vs OFF (max over the
16 texels), the decoded colour difference on vs noroads there (max level), the distance in blocks to the nearest
block whose colour differs, and whether the colour differs on vs noflat.
usage: python g2diag.py <on VT.2.lodt> <off VT.2.lodt> <noroads VT.2.lodt> <out.json>
"""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import sheet_blocks, ang


def decode_tile(S, tx, ty, role):
    return S.tile(tx, ty, role, True)


def main():
    on, off, nr = VM.Sheets(sys.argv[1]), VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3])
    D, B, C = on.D, on.B, on.C
    rows = []
    per_tile = {}
    for ty in range(on.v.tilesY):
        for tx in range(on.v.tilesX):
            m_on, m_off = sheet_blocks(on, tx, ty, 2), sheet_blocks(off, tx, ty, 2)
            c_on, c_nr = sheet_blocks(on, tx, ty, 1), sheet_blocks(nr, tx, ty, 1)
            md = (m_on != m_off).any(-1)
            cd = (c_on != c_nr).any(-1)
            v = md & ~cd
            if not v.any():
                continue
            per_tile['%d,%d' % (tx, ty)] = int(v.sum())
            n_on = VM.msn_world(decode_tile(on, tx, ty, 2))
            n_off = VM.msn_world(decode_tile(off, tx, ty, 2))
            a = ang(n_on, n_off)
            col_on = decode_tile(on, tx, ty, 1)[..., :3].astype(int)
            col_nr = decode_tile(nr, tx, ty, 1)[..., :3].astype(int)
            cdiff = np.abs(col_on - col_nr).max(-1)
            cdb = np.argwhere(cd)
            for bj, bi in np.argwhere(v):
                blk = (slice(bj * 4, bj * 4 + 4), slice(bi * 4, bi * 4 + 4))
                dist = float(np.sqrt(((cdb - [bj, bi]) ** 2).sum(-1)).min()) if len(cdb) else -1.0
                # stored texel -> content texel -> world
                ci = bi * 4 + 2 - B; cj = bj * 4 + 2 - B
                border = not (0 <= ci < C and 0 <= cj < C)
                gi = tx * C + ci; gj = ty * C + cj
                rows.append(dict(tile=[tx, ty], block=[int(bj), int(bi)], border=border,
                                 x=VM.WX0 + (gi + 0.5) * VM.UPT, y=VM.WYTOP - (gj + 0.5) * VM.UPT,
                                 max_angle=round(float(a[blk].max()), 2), mean_angle=round(float(a[blk].mean()), 2),
                                 colour_max_diff=int(cdiff[blk].max()), dist_blocks_to_colour_diff=round(dist, 2)))
    out = dict(n=len(rows), per_tile=per_tile)
    if rows:
        ma = np.array([r['max_angle'] for r in rows]); dd = np.array([r['dist_blocks_to_colour_diff'] for r in rows])
        bo = np.array([r['border'] for r in rows])
        out['border_share'] = float(bo.mean())
        out['max_angle_hist'] = {k: int(((ma > lo) & (ma <= hi)).sum()) for k, lo, hi in
                                 (('<=0.5', -1, 0.5), ('0.5-2', 0.5, 2), ('2-10', 2, 10), ('>10', 10, 1e9))}
        out['dist_hist'] = {k: int(((dd > lo) & (dd <= hi)).sum()) for k, lo, hi in
                            (('<=1', -1, 1), ('1-2', 1, 2), ('2-5', 2, 5), ('>5', 5, 1e9))}
        out['colour_max_diff_hist'] = {str(k): int(sum(1 for r in rows if r['colour_max_diff'] == k)) for k in range(0, 4)}
    out['rows'] = rows
    json.dump(out, open(sys.argv[4], 'w'), indent=1)
    print(json.dumps({k: v for k, v in out.items() if k != 'rows'}, indent=1))
    for r in sorted(rows, key=lambda r: -r['max_angle'])[:15]:
        print(r)


if __name__ == '__main__':
    main()
