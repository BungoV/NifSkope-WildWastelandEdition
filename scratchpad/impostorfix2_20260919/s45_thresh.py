"""SECTION 4 -- what paints the trunk base, and SECTION 5 -- the alpha-threshold
sweep on the REPAIRED sheets."""
import sys, json, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
from calib import place, mask_at, alpha_ref
CAL=json.load(open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919/calib.json'))
TAGS=('blast_n4','blast_n8','maple_n4','dead_n4','rock_n4')

def weights_of(cs,d):
    f=sorted(pickFrames(d,cs.N),key=lambda t:-t[2])[:3]
    return [w for _,_,w in f]

def sec4(tag):
    cs=Sheets(tag); c=CAL[tag]; s=c['scale']
    tot=[];minor=[];baseminor=[];iodom=[];ioall=[];w2=[]
    for v in VIEWS:
        dy,dx=c['off']['%d_%d'%v]
        aAll=alpha_ref(cs,dirOf(*v))
        aDom=alpha_ref(cs,dirOf(*v),nearestOnly=True)
        w=weights_of(cs,dirOf(*v)); w2.append(max(w[1:]) if len(w)>1 else 0.0)
        mAll=place(mask_at(aAll,cs,s,cs.covFloor),dy,dx)
        # the dominant frame's OWN contribution, at its own weight
        mDomC=place(mask_at(aDom*w[0],cs,s,cs.covFloor),dy,dx)
        g=grabmask(tag,'after',*v,'mesh')
        add=mAll&~mDomC
        tot.append(mAll.sum()); minor.append(add.sum())
        ys=np.where(g.any(1))[0]
        if len(ys):
            cut=int(ys.max()-0.25*(ys.max()-ys.min()))
            baseminor.append(add[cut:].sum()/max(mAll[cut:].sum(),1))
        iodom.append(iou(mDomC,g)); ioall.append(iou(mAll,g))
    return (float(np.mean(minor))/max(float(np.mean(tot)),1), float(np.mean(baseminor)),
            float(np.mean(iodom)), float(np.mean(ioall)), float(np.mean(w2)), float(np.max(w2)))

THRS=[0.063,0.12,0.20,0.30,0.45]
def sec5(tag):
    cs=Sheets(tag); c=CAL[tag]; s=c['scale']
    A={v:alpha_ref(cs,dirOf(*v)) for v in VIEWS}
    out=[]
    for t in THRS:
        vals=[];gone=0;ink=[]
        for v in VIEWS:
            dy,dx=c['off']['%d_%d'%v]
            m=place(mask_at(A[v],cs,s,t),dy,dx); g=grabmask(tag,'after',*v,'mesh')
            vals.append(iou(m,g)); ink.append(m.sum()/max(g.sum(),1))
            if m.sum() < 0.05*g.sum(): gone+=1
        out.append((t,float(np.mean(vals)),gone,float(np.mean(ink))))
    return out

if __name__=='__main__':
    job=sys.argv[1]
    if job=='4':
        print('%-10s %10s %12s %10s %10s %9s %9s'%('subject','minorInk','minorInkBase','IoU dom','IoU 3frm','w2 mean','w2 max'),flush=True)
        for t in TAGS:
            r=sec4(t)
            print('%-10s %9.1f%% %11.1f%% %10.4f %10.4f %9.3f %9.3f'%(t,100*r[0],100*r[1],r[2],r[3],r[4],r[5]),flush=True)
    else:
        print('%-10s %6s %8s %22s %9s'%('subject','thresh','IoU','views card vanishes /24','card/mesh ink'),flush=True)
        for t in TAGS:
            for th,v,g,ik in sec5(t):
                print('%-10s %6.3f %8.4f %22d %9.2f'%(t,th,v,g,ik),flush=True)
