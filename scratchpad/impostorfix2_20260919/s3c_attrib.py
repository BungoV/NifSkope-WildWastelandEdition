"""Section 3: is the rock regression a shape loss or a DISPLACEMENT, and does
the parallax step carry it?  Registration is FROZEN at the calibrated value for
every row, so any residual shift is caused by the sheet content / shader path."""
import sys, os, json, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
from calib import alpha_ref, mask_at, place, best_offset

TAG='rock_n4'
C=json.load(open('calib.json'))[TAG]; S=C['scale']; OFF=C['off']
def resid(m,g,R=48):
    inter,dy,dx=best_offset(m,g)
    mm=np.roll(np.roll(m,dy,0),dx,1)
    return dy,dx,iou(mm,g)
rows=[]
print('%-34s %8s %10s %9s %9s'%('variant','IoU','IoU@shift','dy el15','dy el45'),flush=True)
for name,which,par in (('shipped heights, ray on','cards_before',True),
                       ('shipped heights, ray off','cards_before',False),
                       ('repaired heights, ray on','cards',True),
                       ('repaired heights, ray off','cards',False)):
    cs=Sheets(TAG,which)
    io=[];ish=[];d15=[];d45=[]
    for v in VIEWS:
        dy,dx=OFF['%d_%d'%v]
        a=alpha_ref(cs,dirOf(*v),parallax=par)
        m=place(mask_at(a,cs,S,cs.covFloor),dy,dx)
        g=grabmask(TAG,'after',*v,'mesh')
        io.append(iou(m,g))
        sy,sx,i2=resid(m,g); ish.append(i2)
        (d15 if v[1]==15 else d45).append(sy)
    print('%-34s %8.4f %10.4f %9.2f %9.2f'%(name,np.mean(io),np.mean(ish),np.mean(d15),np.mean(d45)),flush=True)
    rows.append((name,np.mean(io),np.mean(ish),np.mean(d15),np.mean(d45)))
print(flush=True)
print('height repair with the ray ON : IoU %+.4f, shape@shift %+.4f, el15 shift %+.2f px'%(rows[2][1]-rows[0][1],rows[2][2]-rows[0][2],rows[2][3]-rows[0][3]),flush=True)
print('the ray itself (repaired sheet): IoU %+.4f, shape@shift %+.4f, el15 shift %+.2f px'%(rows[2][1]-rows[3][1],rows[2][2]-rows[3][2],rows[2][3]-rows[3][3]),flush=True)
print('px -> world units at this subject: 1 px = %.2f units'%(1.0/S),flush=True)
