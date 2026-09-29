"""TERR1 continuation: WHAT stamps the G2 violations -- read the diagnostic bake's per-texel stamp record.

The diagnostic exe (stamp_diag.patch applied; env WW_TERR1_STAMP_DIAG=<file>) writes, for every texel the colour
lerp gave a normal weight, six float32: world x, world y, stamp weight w, the colour lerp's own move in levels
(max channel of |road - ground| * ra * 255, BEFORE the 8-bit round and BC1), raGeom (road plane coverage), nA
(normal plane coverage). This script takes the G2 violating blocks (g2diag.json, from the same ON/OFF/noroads
sheets), finds every texel in them whose normal moved > 1 degree, and looks each one up in the record.

PRE-REGISTERED READING (written before any diagnostic bake ran, 2026-09-27):
  A  >= 90% of the moved violation texels have a record AND colour move < 2 levels
     -> the stamp is a real road/flat fragment whose colour barely differs from the ground; BC1 (565 endpoints,
        4-entry palette) keeps the block's colour bytes while the normal moves. The code does what it says; the
        gate's proxy (BC1 colour bytes) is too coarse. Fix = the gate's mask.
  B  >= 10% of the moved violation texels have NO record
     -> something other than the stamp moved the msn there (a code defect outside lodgenVtStampMsn). Find it.
  C  records exist but >= 10% show colour move >= 2 levels
     -> the colour moved at the texel and was undone or overwritten after the lerp (fill, tint, grade). Find it.
usage: python g2stampdiag.py <diag record file> <on VT.2.lodt> <off VT.2.lodt> <g2diag.json> <out.json>
"""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import ang


def main():
    rec = np.fromfile(sys.argv[1], dtype='<f4').reshape(-1, 6)
    on, off = VM.Sheets(sys.argv[2]), VM.Sheets(sys.argv[3])
    rows = json.load(open(sys.argv[4]))['rows']
    # key the record by texel centre (tiles overlap in their borders: a texel may appear twice; keep the max w)
    key = np.round(rec[:, 0] * 2).astype(np.int64) * 10_000_000 + np.round(rec[:, 1] * 2).astype(np.int64)
    look = {}
    for k, r in zip(key.tolist(), rec):
        if k not in look or r[2] > look[k][2]:
            look[k] = r
    D, B, C = on.D, on.B, on.C
    hits = []
    miss = 0
    for r in rows:
        tx, ty = r['tile']; bj, bi = r['block']
        a = ang(VM.msn_world(on.tile(tx, ty, 2, True)), VM.msn_world(off.tile(tx, ty, 2, True)))
        for dj in range(4):
            for di in range(4):
                j, i = bj * 4 + dj, bi * 4 + di
                if a[j, i] <= 1.0:
                    continue
                gi = tx * C + (i - B); gj = ty * C + (j - B)
                wx = VM.WX0 + (gi + 0.5) * VM.UPT; wy = VM.WYTOP - (gj + 0.5) * VM.UPT
                k = int(round(wx * 2)) * 10_000_000 + int(round(wy * 2))
                if k in look:
                    w, dl, rg, na = (float(v) for v in look[k][2:])
                    hits.append(dict(x=wx, y=wy, angle=float(a[j, i]), w=w, colour_move=dl, raGeom=rg, nA=na))
                else:
                    miss += 1
    n = len(hits) + miss
    dl = np.array([h['colour_move'] for h in hits]) if hits else np.zeros(0)
    out = dict(records=int(len(rec)), moved_violation_texels=n, with_record=len(hits), without_record=miss,
               colour_move_lt2_share=float((dl < 2).mean()) if len(dl) else None,
               colour_move_median=float(np.median(dl)) if len(dl) else None,
               w_median=float(np.median([h['w'] for h in hits])) if hits else None)
    if n and miss / n >= 0.10:
        out['reading'] = 'B'
    elif len(dl) and (dl >= 2).mean() >= 0.10:
        out['reading'] = 'C'
    elif n and len(hits) / n >= 0.90 and (dl < 2).mean() >= 0.90:
        out['reading'] = 'A'
    else:
        out['reading'] = 'none of A/B/C cleanly'
    out['texels'] = hits[:200]
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    print(json.dumps({k: v for k, v in out.items() if k != 'texels'}, indent=1))


if __name__ == '__main__':
    main()
