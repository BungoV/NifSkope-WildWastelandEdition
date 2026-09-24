"""N ladder, honestly: each N gets its OWN registration fitted against the
harness card grab for blast_n4's mesh (same subject, same mesh grabs)."""
import sys, json, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
import calib
from calib import place, mask_at, alpha_ref, best_offset
calib.REFH=448
from PIL import Image
print('%-10s %6s %8s %8s %9s %8s %10s'%('tag','N','scale','IoU','2deg mean','2deg max','30deg card'),flush=True)
for tag in ('blast_n4','blast_n5','blast_n8','blast_n12'):
    cs=Sheets(tag)
    A={v:alpha_ref(cs,dirOf(*v)) for v in VIEWS}
    G={v:grabmask('blast_n4','after',*v,'card') for v in VIEWS}   # N=4's own card grab as the ruler
    smax=min(512.0/(2*cs.half[0]),768.0/(2*cs.half[1]))
    best=None;step=(smax-0.45*smax)/18.0
    for s in np.arange(0.45*smax,smax*1.0001,step):
        tot=0.;n=0
        for v in VIEWS[0:12:3]+VIEWS[12:24:3]:
            m=mask_at(A[v],cs,s,cs.covFloor);g=G[v]
            if m.shape[0]>g.shape[0] or m.shape[1]>g.shape[1]: continue
            inter,_,_=best_offset(m,g);uni=m.sum()+g.sum()-inter
            tot+=inter/uni if uni else 0;n+=1
        if n and (best is None or tot/n>best[1]): best=(float(s),tot/n)
    s0=best[0]
    for s in np.arange(max(0.02,s0-2*step),min(smax,s0+2*step)+1e-9,step/8.0):
        tot=0.;n=0
        for v in VIEWS[0:12:3]+VIEWS[12:24:3]:
            m=mask_at(A[v],cs,s,cs.covFloor);g=G[v]
            if m.shape[0]>g.shape[0] or m.shape[1]>g.shape[1]: continue
            inter,_,_=best_offset(m,g);uni=m.sum()+g.sum()-inter
            tot+=inter/uni if uni else 0;n+=1
        if n and tot/n>best[1]: best=(float(s),tot/n)
    s=best[0];vals=[];offs={}
    for v in VIEWS:
        m=mask_at(A[v],cs,s,cs.covFloor);inter,dy,dx=best_offset(m,G[v]);offs[v]=(dy,dx)
        vals.append(iou(place(m,dy,dx),grabmask('blast_n4','after',*v,'mesh')))
    fine=[];prev=None
    for az in range(0,92,2):
        mm=alpha_ref(cs,dirOf(az,15))>=cs.covFloor
        if prev is not None: fine.append(1.0-iou(mm,prev))
        prev=mm
    c30=[]
    for k in range(12):
        az0,az1=k*30,((k+1)%12)*30
        d0=place(mask_at(alpha_ref(cs,dirOf(az0,15)),cs,s,cs.covFloor),*offs[(az0,15)])
        d1=place(mask_at(alpha_ref(cs,dirOf(az1,15)),cs,s,cs.covFloor),*offs[(az1,15)])
        c30.append(1.0-iou(d0,d1))
    print('%-10s %6d %8.3f %8.4f %9.4f %8.4f %10.4f'%(tag,cs.N,s,np.mean(vals),np.mean(fine),np.max(fine),np.mean(c30)),flush=True)
