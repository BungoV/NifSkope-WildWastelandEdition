"""Calibrate the instrument: fit (scale, per-view offset) against the harness's
own CARD grab, then check the numpy card against the MESH grab reproduces the
number the harness printed for that subject."""
import sys, os, json, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
from scipy.signal import fftconvolve
from PIL import Image

TAGS=tuple((__import__('os').environ.get('TAGS') or 'blast_n4,blast_n8,maple_n4,dead_n4,rock_n4').split(','))
HARNESS={'blast_n4':0.5038,'blast_n8':0.6754,'maple_n4':0.3545,'dead_n4':0.5721,'rock_n4':0.7724}
HARNESS_BEFORE={'blast_n4':0.3651,'blast_n8':0.3853,'maple_n4':0.3550,'dead_n4':0.4176,'rock_n4':0.7891}
CAL=os.path.join(os.path.dirname(__file__),'calib.json')
REFH=1024                              # quad raster height used for the scale search

def best_offset(card, target):
    a=target.astype(np.float32); b=card.astype(np.float32)[::-1,::-1]
    c=fftconvolve(a,b,mode='full')
    idx=np.unravel_index(np.argmax(c),c.shape); bh,bw=card.shape
    return float(c[idx]), idx[0]-(bh-1), idx[1]-(bw-1)

def alpha_ref(cs,d,**kw):
    H=REFH; W=max(4,int(round(H*cs.half[0]/cs.half[1])))
    _,a=render(cs,d,(W,H),**kw)
    return a

def mask_at(a, cs, s, thr):
    W=max(4,int(round(2*cs.half[0]*s))); H=max(4,int(round(2*cs.half[1]*s)))
    z=np.asarray(Image.fromarray(a.astype(np.float32)).resize((W,H),Image.BILINEAR))
    return z>=thr

def place(m,dy,dx,shape=(768,512)):
    out=np.zeros(shape,bool); h,w=m.shape
    y0,x0=max(0,dy),max(0,dx); y1,x1=min(shape[0],dy+h),min(shape[1],dx+w)
    if y1<=y0 or x1<=x0: return out
    out[y0:y1,x0:x1]=m[y0-dy:y1-dy,x0-dx:x1-dx]; return out

def calibrate(tag, which='after'):
    cs=Sheets(tag,'cards' if which=='after' else 'cards_before')
    A={v:alpha_ref(cs,dirOf(*v)) for v in VIEWS}
    G={v:grabmask(tag,which,*v,'card') for v in VIEWS}
    sub=VIEWS[0:12:3]+VIEWS[12:24:3]
    smax=min(512.0/(2*cs.half[0]), 768.0/(2*cs.half[1]))
    best=None
    step=None
    for step,lo,hi in (((smax-0.45*smax)/18.0, 0.45*smax, smax*1.0001),):
        for s in np.arange(lo,hi,step):
            tot=0.;n=0
            for v in sub:
                m=mask_at(A[v],cs,s,cs.covFloor); g=G[v]
                if m.shape[0]>g.shape[0] or m.shape[1]>g.shape[1]: continue
                inter,_,_=best_offset(m,g); uni=m.sum()+g.sum()-inter
                tot+=inter/uni if uni else 0; n+=1
            if n and (best is None or tot/n>best[1]): best=(float(s),tot/n)
    s0=best[0]
    for s in np.arange(max(0.02,s0-2*step),min(smax,s0+2*step)+1e-9,step/8.0):
        tot=0.;n=0
        for v in sub:
            m=mask_at(A[v],cs,s,cs.covFloor); g=G[v]
            if m.shape[0]>g.shape[0] or m.shape[1]>g.shape[1]: continue
            inter,_,_=best_offset(m,g); uni=m.sum()+g.sum()-inter
            tot+=inter/uni if uni else 0; n+=1
        if n and tot/n>best[1]: best=(float(s),tot/n)
    s=best[0]; offs={}; cc=[]
    for v in VIEWS:
        m=mask_at(A[v],cs,s,cs.covFloor); g=G[v]
        inter,dy,dx=best_offset(m,g); uni=m.sum()+g.sum()-inter
        offs['%d_%d'%v]=[int(dy),int(dx)]; cc.append(inter/uni)
    return cs,s,offs,float(np.mean(cc))

if __name__=='__main__':
    out={}
    print('%-10s %6s %10s %12s %10s %8s'%('tag','scale','card-card','npy-vs-mesh','harness','delta'),flush=True)
    for tag in TAGS:
        cs,s,offs,cc=calibrate(tag)
        vs=[]
        for v in VIEWS:
            dy,dx=offs['%d_%d'%v]
            m=place(mask_at(alpha_ref(cs,dirOf(*v)),cs,s,cs.covFloor),dy,dx)
            vs.append(iou(m,grabmask(tag,'after',*v,'mesh')))
        mm=float(np.mean(vs))
        out[tag]={'scale':s,'off':offs,'cardcard':cc,'npy_mesh':mm,'harness':HARNESS[tag],
                  'per_view':{('%d_%d'%v):float(x) for v,x in zip(VIEWS,vs)}}
        print('%-10s %6.3f %10.4f %12.4f %10.4f %8.4f'%(tag,s,cc,mm,HARNESS[tag],mm-HARNESS[tag]),flush=True)
    old=json.load(open(CAL)) if os.path.exists(CAL) else {}
    old.update(out)
    json.dump(old,open(CAL,'w'),indent=1)
    out=old
    print('wrote',CAL,flush=True)
