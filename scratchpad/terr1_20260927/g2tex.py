"""TERR1 continuation: texel view of the worst G2 violations (normal ON/OFF world vector, colour on/noroads)."""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import ang
on, off, nr = (VM.Sheets(p) for p in sys.argv[1:4])
rows = json.load(open(sys.argv[4]))['rows']
rows.sort(key=lambda r: -r['max_angle'])
allz = []
for r in rows:
    tx, ty = r['tile']; bj, bi = r['block']
    no = VM.msn_world(on.tile(tx, ty, 2, True)); nf = VM.msn_world(off.tile(tx, ty, 2, True))
    co = on.tile(tx, ty, 1, True)[..., :3].astype(int); cn = nr.tile(tx, ty, 1, True)[..., :3].astype(int)
    a = ang(no, nf)
    blk = a[bj*4:bj*4+4, bi*4:bi*4+4]
    k = np.unravel_index(np.argmax(blk), blk.shape); j, i = bj*4+k[0], bi*4+k[1]
    n1 = no[j, i] / np.linalg.norm(no[j, i]); n0 = nf[j, i] / np.linalg.norm(nf[j, i])
    allz.append(n1[2] if n1.shape[0] == 3 else 0)
    ring = np.abs(co[j-2:j+3, i-2:i+3] - cn[j-2:j+3, i-2:i+3]).max(-1)
    if len(allz) <= 8:
        print(r['tile'], r['block'], 'texel', (j, i), 'angle %.1f' % a[j, i], 'ON', np.round(n1, 2), 'OFF', np.round(n0, 2))
        print('   colour |on-noroads| 5x5 around:\n', ring)
        print('   angle 5x5:\n', np.round(a[j-2:j+3, i-2:i+3], 0))
