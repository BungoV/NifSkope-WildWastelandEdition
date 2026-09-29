"""TERR1 continuation: are the G2 violation texels steep-face stamps? Up component of the ON msn normal at the
max-angle texel of each violating block, against the same on every stamped texel (colour on != noroads) that moved."""
import sys, json
import numpy as np
import vtmosaic as VM
from nrmgate import ang
on, off, nr = (VM.Sheets(p) for p in sys.argv[1:4])
rows = json.load(open(sys.argv[4]))['rows']
print('dxgi', [(s['role'], s['dxgi'], s['dxgiCover']) for s in on.v.sheets])
zs = []
for r in rows:
    tx, ty = r['tile']; bj, bi = r['block']
    no = VM.msn_world(on.tile(tx, ty, 2, True)); nf = VM.msn_world(off.tile(tx, ty, 2, True))
    a = ang(no, nf)[bj*4:bj*4+4, bi*4:bi*4+4]
    k = np.unravel_index(np.argmax(a), a.shape)
    n1 = no[bj*4+k[0], bi*4+k[1]]; zs.append(n1[2] / np.linalg.norm(n1))
zs = np.array(zs)
print('violations: n', len(zs), 'up<0.5 share %.3f' % (zs < 0.5).mean(), 'up<0.8 share %.3f' % (zs < 0.8).mean(),
      'median up %.3f' % np.median(zs))
no = VM.msn_world(on.mosaic(2)); nf = VM.msn_world(off.mosaic(2))
st = (np.abs(on.mosaic(1)[..., :3].astype(int) - nr.mosaic(1)[..., :3].astype(int)) > 2).any(-1)
mv = ang(no, nf) > 1
z = no[..., 2] / np.linalg.norm(no, axis=-1)
zo = nf[..., 2] / np.linalg.norm(nf, axis=-1)
for nm, m in (('stamped&moved', st & mv), ('unstamped&moved', ~st & mv), ('all OFF', np.ones_like(st))):
    zz = (z if nm != 'all OFF' else zo)[m]
    print(nm, 'n', int(m.sum()), 'up<0.5 %.4f' % (zz < 0.5).mean(), 'up<0.8 %.4f' % (zz < 0.8).mean(), 'median %.3f' % np.median(zz))
