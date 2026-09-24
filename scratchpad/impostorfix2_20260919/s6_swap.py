"""SECTION 6 -- the owed `_n` height <-> sway channel swap, through a REAL BC3
encode (mirroring src/lodgen.cpp's own encoder) and decode."""
import sys, glob, json, copy, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from PIL import Image
from scipy.ndimage import distance_transform_edt
from inst import *
from bcenc import bc3_roundtrip
from calib import place, mask_at, alpha_ref
CAL=json.load(open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919/calib.json'))
TAGS=('blast_n4','blast_n8','maple_n4','dead_n4','rock_n4')

def repaired_reference(tag):
    """The sheet the bake produces BEFORE compression, with the shipped repair
       applied: the PNG twin, R3'd per frame against the shipped coverage."""
    d='%s/fixture/%s/cards/'%(R1,tag)
    nrm=np.asarray(Image.open(glob.glob(d+'*_oct_normal.png')[0]).convert('RGBA')).astype(float)/255.0
    cs=Sheets(tag); cov=np.round(cs.alb[...,3]*255)
    N,fw,fh=cs.N,cs.fw,cs.fh
    out=nrm.copy()
    for j in range(N):
        for i in range(N):
            ys,xs=slice(j*fh,(j+1)*fh),slice(i*fw,(i+1)*fw)
            c=cov[ys,xs]; h=out[ys,xs,2]; full=c>=250
            if full.any():
                _,idx=distance_transform_edt(~full,return_indices=True); h=h[idx[0],idx[1]]
            else: h=np.full_like(h,0.5)
            out[ys,xs,2]=np.where(c>0,h,0.5)
    return cs,out

def err(a,b,mask,span):
    d=np.abs(a-b)[mask]*255
    return d.mean(), np.percentile(d,95), d.max(), d.mean()/255*span

if __name__=='__main__':
    print('%-10s %-9s %-24s %7s %7s %7s %9s'%('subject','layout','channel','mean lv','p95 lv','max lv','mean u'),flush=True)
    res={}
    for tag in TAGS:
        cs,ref=repaired_reference(tag)
        cov=np.round(cs.alb[...,3]*255); m=cov>0
        now=bc3_roundtrip(ref)                                   # height in BLUE, sway in ALPHA
        sw=ref.copy(); sw[...,2],sw[...,3]=ref[...,3],ref[...,2] # height in ALPHA, sway in BLUE
        swd=bc3_roundtrip(sw)
        pairs=[('as shipped',now[...,2],ref[...,2],'height (blue, BC1 palette)'),
               ('as shipped',now[...,3],ref[...,3],'sway   (alpha, BC4 ramp)'),
               ('as shipped',now[...,0],ref[...,0],'normal X'),
               ('swapped',   swd[...,3],ref[...,2],'height (alpha, BC4 ramp)'),
               ('swapped',   swd[...,2],ref[...,3],'sway   (blue, BC1 palette)'),
               ('swapped',   swd[...,0],ref[...,0],'normal X')]
        for lay,a,b,nm in pairs:
            e=err(a,b,m,cs.span)
            print('%-10s %-9s %-24s %7.2f %7.2f %7.0f %9.0f'%(tag,lay,nm,e[0],e[1],e[2],e[3]),flush=True)
        # IoU with each layout
        c=CAL[tag]; s=c['scale']; row=[]
        for lay,sheet,hch in (('as shipped',now,2),('swapped',swd,3)):
            k=copy.copy(cs); k.nrm=sheet.copy()
            if hch==3: k.nrm[...,2]=sheet[...,3]    # the drawer reads height from the swapped channel
            vals=[]
            for v in VIEWS:
                dy,dx=c['off']['%d_%d'%v]
                mm=place(mask_at(alpha_ref(k,dirOf(*v)),k,s,k.covFloor),dy,dx)
                vals.append(iou(mm,grabmask(tag,'after',*v,'mesh')))
            row.append(float(np.mean(vals)))
        print('%-10s IoU  as shipped %.4f   swapped %.4f   delta %+0.4f'%(tag,row[0],row[1],row[1]-row[0]),flush=True)
        res[tag]=row
