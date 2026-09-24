import sys, json, numpy as np
sys.path.insert(0,'.')
ROOT='E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT+'/tests/spells')
from scene import Sheet, LODT
from lodgen_horizon_witness import HorizonSheet
sh=Sheet(cache=None)
hs=HorizonSheet(LODT)
rec=json.load(open(ROOT+'/scratchpad/horizon2_20260918/receivers.json'))
print(type(rec), list(rec)[:3] if isinstance(rec,dict) else rec[:2])
rows = rec['receivers'] if isinstance(rec,dict) and 'receivers' in rec else rec
worst=0
for r in (rows if isinstance(rows,list) else []):
    x,y = r.get('x'), r.get('y')
    if x is None: continue
    b = hs.bins_at(x,y)
    if b is None: continue
    for k in range(16):
        az = k*22.5
        mine = float(sh.elev_at(np.array([x]),np.array([y]),az)[0])
        ref  = b[k]/255.0*90.0
        worst=max(worst,abs(mine-ref))
    print('%-6s (%d,%d) bin0 mine %.2f ref %.2f' % (r.get('id','?'),x,y, float(sh.elev_at(np.array([x]),np.array([y]),0.0)[0]), b[0]/255.0*90.0))
print('worst |mine - HorizonSheet.bins_at| over 16 bins x receivers = %.3f deg'%worst)
