"""SECTION 2 -- N=4 GHOSTING. Sharper weights, a height-consistency rejection,
and the N ladder, each with a TEMPORAL number beside the IoU.
REFH is dropped to 448: the control at the bottom shows that costs <=0.0013 IoU
and 5x the speed."""
import sys, json, os, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
import calib
from calib import place, mask_at, alpha_ref
calib.REFH=int(os.environ.get('REFH','448'))
CAL=json.load(open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919/calib.json'))

VARIANTS=[
 ('spec: 3 frames, barycentric',      dict()),
 ('weights^2',                        dict(wpow=2.0)),
 ('weights^4',                        dict(wpow=4.0)),
 ('nearest frame only',               dict(nearestOnly=True)),
 ('height reject 0.25 span-step',     dict(reject=0.25*255/16)),
 ('height reject 1 span-step',        dict(reject=1.0*255/16)),
 ('height reject 4 span-step',        dict(reject=4.0*255/16)),
]

def run(tag, calibtag=None, variants=None, meshtag=None):
    cs=Sheets(tag); c=CAL[calibtag or tag]; s=c['scale']; mt=meshtag or tag
    out=[]
    for name,kw in (variants or VARIANTS):
        vals=[]
        for v in VIEWS:
            dy,dx=c['off']['%d_%d'%v]
            m=place(mask_at(alpha_ref(cs,dirOf(*v),**kw),cs,s,cs.covFloor),dy,dx)
            vals.append(iou(m,grabmask(mt,'after',*v,'mesh')))
        fine=[];prev=None
        for az in range(0,92,2):
            mm=alpha_ref(cs,dirOf(az,15),**kw)>=cs.covFloor
            if prev is not None: fine.append(1.0-iou(mm,prev))
            prev=mm
        c30=[];m30=[]
        for k in range(12):
            az0,az1=k*30,((k+1)%12)*30
            d0=place(mask_at(alpha_ref(cs,dirOf(az0,15),**kw),cs,s,cs.covFloor),*c['off']['%d_%d'%(az0,15)])
            d1=place(mask_at(alpha_ref(cs,dirOf(az1,15),**kw),cs,s,cs.covFloor),*c['off']['%d_%d'%(az1,15)])
            c30.append(1.0-iou(d0,d1)); m30.append(1.0-iou(grabmask(mt,'after',az0,15,'mesh'),grabmask(mt,'after',az1,15,'mesh')))
        out.append((name,float(np.mean(vals)),float(np.mean(fine)),float(np.max(fine)),float(np.mean(c30)),float(np.mean(m30))))
        print('%-30s %-10s %7.4f %9.4f %8.4f %10.4f %10.4f'%(name,tag,out[-1][1],out[-1][2],out[-1][3],out[-1][4],out[-1][5]),flush=True)
    return out

if __name__=='__main__':
    print('%-30s %-10s %7s %9s %8s %10s %10s'%('variant','subject','IoU','2deg mean','2deg max','30deg card','30deg MESH'),flush=True)
    for t in ('blast_n4','maple_n4','dead_n4','rock_n4','blast_n8'): run(t)
    print(flush=True); print('=== N LADDER, blasted tree, same mesh grabs, same registration ===',flush=True)
    b4=Sheets('blast_n4')
    for t in ('blast_n4','blast_n5','blast_n8','blast_n12'):
        cs=Sheets(t)
        ok=np.allclose(np.asarray(cs.half),np.asarray(b4.half)) and cs.span==b4.span
        print('   %-10s half match %s  span %s'%(t,ok,cs.span),flush=True)
        run(t,calibtag='blast_n4',meshtag='blast_n4',variants=[('spec: 3 frames, barycentric',dict())])
