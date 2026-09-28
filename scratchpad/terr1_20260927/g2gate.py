"""TERR1 G2 (fixed gate): every msn block that moved ON vs OFF holds a texel the stamp itself wrote.

usage: python g2gate.py <stamp_diag.bin> <on VT.2.lodt> <off VT.2.lodt> <noroads VT.2.lodt> <out.json>

Why the gate changed (measured 2026-09-28, g2block.py): the first G2 took "the roads mask" to be the blocks whose
COLOUR bytes differ roads vs no-roads. All 324 blocks it flagged hold a real stamp record (a road/flat fragment the
colour lerp gave a normal weight), 262 of them exactly one texel, colour move before rounding median 5.6 levels: the
colour sheet's BC1 (565 endpoints, 4-entry palette) swallows a one-texel colour move that small, the msn keeps the
normal move. So the colour bytes are a proxy for the stamp that under-reports it. The stamp's own mask is the
diagnostic exe's per-texel record (stamp_diag.patch, WW_TERR1_STAMP_DIAG; the diagnostic bake's sheets are checked
equal to the ON bake's before this runs).

Gate: msn blocks ON != OFF (BC1 bytes, mip 0, stored sheet incl. border, every tile) that hold NO record texel = 0.
Refuters (both must fire):
  planted   one byte flipped in an unmoved msn block with no record -> exactly 1 violation
  shifted   the record mask moved one block east (a stamp written one block off) -> violations > 0
The old proxy's count is reported beside it (the 324).
"""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import sheet_blocks


def main():
    rec = np.fromfile(sys.argv[1], dtype='<f4').reshape(-1, 6)
    on, off, nr = VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3]), VM.Sheets(sys.argv[4])
    kx = np.round(rec[:, 0] * 2).astype(np.int64); ky = np.round(rec[:, 1] * 2).astype(np.int64)
    keys = set((kx * 10_000_000 + ky).tolist())
    D, B, C = on.D, on.B, on.C
    nb = D // 4
    moved = viol = viol_proxy = viol_shift = 0
    stamped_unmoved = 0
    plant = None
    for ty in range(on.v.tilesY):
        for tx in range(on.v.tilesX):
            m_on, m_off = sheet_blocks(on, tx, ty, 2), sheet_blocks(off, tx, ty, 2)
            md = (m_on != m_off).any(-1)
            cd = (sheet_blocks(on, tx, ty, 1) != sheet_blocks(nr, tx, ty, 1)).any(-1)
            jj, ii = np.mgrid[0:D, 0:D]
            wx = VM.WX0 + (tx * C + (ii - B) + 0.5) * VM.UPT
            wy = VM.WYTOP - (ty * C + (jj - B) + 0.5) * VM.UPT
            k = np.round(wx * 2).astype(np.int64) * 10_000_000 + np.round(wy * 2).astype(np.int64)
            has = np.fromiter((v in keys for v in k.ravel().tolist()), bool, k.size).reshape(D, D)
            sb = has.reshape(nb, 4, nb, 4).any(axis=(1, 3))
            sh = np.zeros_like(sb); sh[:, 1:] = sb[:, :-1]
            moved += int(md.sum())
            viol += int((md & ~sb).sum())
            viol_proxy += int((md & ~cd).sum())
            viol_shift += int((md & ~sh).sum())
            stamped_unmoved += int((~md & sb).sum())
            if plant is None:
                cand = np.argwhere(~md & ~sb)
                if len(cand):
                    bj, bi = cand[len(cand) // 2]
                    m2 = m_off.copy(); m2[bj, bi, 0] ^= 1
                    plant = dict(tile=[tx, ty], block=[int(bj), int(bi)],
                                 violations=int(((m_on != m2).any(-1) & ~sb).sum()) - int((md & ~sb).sum()))
    out = dict(msn_blocks_moved=moved, violations=viol, old_proxy_violations=viol_proxy,
               refuter_planted=plant, refuter_shifted_violations=viol_shift,
               stamped_blocks_unmoved=stamped_unmoved)
    out['gate'] = 'PASS' if viol == 0 and plant and plant['violations'] == 1 and viol_shift > 0 else 'FAIL'
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    print(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
