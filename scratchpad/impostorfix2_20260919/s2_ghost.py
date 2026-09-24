"""SECTION 2 -- N=4 GHOSTING. Sharper blend weights, a height-consistency
rejection between frames, and N=8/12, scored on the same 24 orbit views, with a
TEMPORAL number beside every IoU."""
import sys, json, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
from calib import place, mask_at, alpha_ref
CAL=json.load(open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919/calib.json'))

VARIANTS=[
 ('spec: 3 frames, barycentric',      dict()),
 ('weights^2',                        dict(wpow=2.0)),
 ('weights^4',                        dict(wpow=4.0)),
 ('nearest frame only',               dict(nearestOnly=True)),
 ('height reject k=0.25 span-step',   dict(reject=0.25*255/16)),
 ('height reject k=1 span-step',      dict(reject=1.0*255/16)),
 ('height reject k=4 span-step',      dict(reject=4.0*255/16)),
]

def run(tag, cs=None, calibtag=None, extra=None, label=None):
    cs=cs or Sheets(tag); c=CAL[calibtag or tag]; s=c['scale']
    rows=[]
    for name,kw in (extra or VARIANTS):
        kk=dict(kw)
        vals=[]
        for v in VIEWS:
            dy,dx=c['off']['%d_%d'%v]
            m=place(mask_at(alpha_ref(cs,dirOf(*v),**kk),cs,s,cs.covFloor),dy,dx)
            vals.append(iou(m,grabmask(tag,'after',*v,'mesh')))
        # temporal: 2-degree orbit at elev 15, azim 0..90
        fine=[]
        prev=None
        for az in range(0,92,2):
            a=alpha_ref(cs,dirOf(az,15),**kk)
            mm=a>=cs.covFloor
            if prev is not None:
                fine.append(1.0-iou(mm,prev))
            prev=mm
        # 30-degree step, card vs the MESH's own 30-degree step
        c30=[];m30=[]
        for k in range(12):
            az0,az1=k*30,(k+1)%12*30
            d0=place(mask_at(alpha_ref(cs,dirOf(az0,15),**kk),cs,s,cs.covFloor),*c['off']['%d_%d'%(az0,15)])
            d1=place(mask_at(alpha_ref(cs,dirOf(az1,15),**kk),cs,s,cs.covFloor),*c['off']['%d_%d'%(az1,15)])
            c30.append(1.0-iou(d0,d1))
            m30.append(1.0-iou(grabmask(tag,'after',az0,15,'mesh'),grabmask(tag,'after',az1,15,'mesh')))
        rows.append((name,float(np.mean(vals)),float(np.mean(fine)),float(np.max(fine)),
                     float(np.mean(c30)),float(np.mean(m30))))
    return rows

if __name__=='__main__':
    import sys as S
    tags=(S.argv[1] if len(S.argv)>1 else 'blast_n4,maple_n4,dead_n4,rock_n4,blast_n8').split(',')
    print('%-32s %-10s %7s %8s %8s %8s %8s'%('variant','subject','IoU','2deg mean','2deg max','30deg card','30deg MESH'),flush=True)
    for t in tags:
        for name,v,fm,fx,c30,m30 in run(t):
            print('%-32s %-10s %7.4f %8.4f %8.4f %8.4f %8.4f'%(name,t,v,fm,fx,c30,m30),flush=True)
