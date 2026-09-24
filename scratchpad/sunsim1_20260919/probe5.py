import sys, json, numpy as np
sys.path.insert(0,'.')
ROOT='E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT+'/tests/spells')
from scene import Sheet, LODT
from lodgen_horizon_witness import HorizonSheet
sh=Sheet(verbose=False); hs=HorizonSheet(LODT)
rows=json.load(open(ROOT+'/scratchpad/horizon2_20260918/receivers.json'))
def nearest(x,y,k):
    i=int(round((x-sh.ox)/sh.upt-0.5)); j=int(round((y-sh.oy)/sh.upt-0.5))
    return sh.plane[j,i,k]
worst=0; n=0
for r in rows:
    b=hs.bins_at(r['x'],r['y'])
    for k in range(16):
        worst=max(worst,abs(int(nearest(r['x'],r['y'],k))-int(b[k]))); n+=1
print('CONTROL nearest-texel: worst |my plane byte - HorizonSheet.bins_at| over %d taps = %d byte(s)'%(n,worst))
# and a dense control over the whole chunk on a 64-texel stride
rng=np.random.default_rng(7); bad=0; tot=0; wb=0
for _ in range(400):
    x=rng.uniform(*(sh.ox+16, sh.ox+16384-16)); y=rng.uniform(sh.oy+16, sh.oy+16384-16)
    b=hs.bins_at(x,y)
    if b is None: continue
    for k in range(16):
        d=abs(int(nearest(x,y,k))-int(b[k])); wb=max(wb,d); tot+=1
        if d: bad+=1
print('CONTROL 400 random points x 16 bins: %d/%d taps differ, worst %d byte(s)'%(bad,tot,wb))
