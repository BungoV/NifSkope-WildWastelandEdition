"""SECTION 3 -- the rock regression, and the height-fill candidates re-scored on
all five subjects with the corrected decoder."""
import sys, glob, json, copy, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from PIL import Image
from scipy.ndimage import distance_transform_edt
from inst import *
from bcenc import bc3_roundtrip
from calib import place, mask_at, alpha_ref
CAL=json.load(open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919/calib.json'))
TAGS=('blast_n4','blast_n8','maple_n4','dead_n4','rock_n4')

def png_normal(tag):
    d='%s/fixture/%s/cards/'%(R1,tag)
    return np.asarray(Image.open(glob.glob(d+'*_oct_normal.png')[0]).convert('RGBA')).astype(float)/255.0

def fill(h,c,rule):
    full=c>=250
    if not full.any(): return np.full_like(h,0.5)
    dist,idx=distance_transform_edt(~full,return_indices=True)
    dil=h[idx[0],idx[1]]
    if rule=='R3_shipped':   return np.where(c>0,dil,0.5)
    if rule=='R2_dilate_all':return dil
    if rule=='R2d8':         return np.where((c>0)|(dist<=8),dil,0.5)
    if rule=='R2d16':        return np.where((c>0)|(dist<=16),dil,0.5)
    if rule=='R1_plane_all': return np.where(full,h,0.5)
    raise KeyError(rule)

RULES=['as baked (flood = frame mean)','R1_plane_all','R3_shipped','R2d8','R2d16','R2_dilate_all']

def variant(tag,rule):
    cs=Sheets(tag); ref=png_normal(tag); cov=np.round(cs.alb[...,3]*255)
    if rule.startswith('as baked'):
        k=copy.copy(cs); k.nrm=Sheets(tag,'cards_before').nrm.copy(); return k
    out=ref.copy(); N,fw,fh=cs.N,cs.fw,cs.fh
    for j in range(N):
        for i in range(N):
            ys,xs=slice(j*fh,(j+1)*fh),slice(i*fw,(i+1)*fw)
            out[ys,xs,2]=fill(out[ys,xs,2],cov[ys,xs],rule)
    k=copy.copy(cs); k.nrm=bc3_roundtrip(out); return k

def score(cs,tag):
    c=CAL[tag]; s=c['scale']; vals={}
    for v in VIEWS:
        dy,dx=c['off']['%d_%d'%v]
        m=place(mask_at(alpha_ref(cs,dirOf(*v)),cs,s,cs.covFloor),dy,dx)
        vals[v]=iou(m,grabmask(tag,'after',*v,'mesh'))
    return vals

if __name__=='__main__':
    print('%-30s'%'rule'+''.join('%11s'%t for t in TAGS),flush=True)
    keep={}
    for r in RULES:
        row=[]
        for t in TAGS:
            v=score(variant(t,r),t); keep[(r,t)]=v; row.append(np.mean(list(v.values())))
        print('%-30s'%r+''.join('%11.4f'%x for x in row),flush=True)
    print(flush=True)
    print('rock_n4, per view, shipped repair minus as-baked (numpy instrument):',flush=True)
    a=keep[('R3_shipped','rock_n4')]; b=keep[('as baked (flood = frame mean)','rock_n4')]
    for v in sorted(VIEWS,key=lambda v:a[v]-b[v])[:6]:
        print('   az%4d el%3d   as-baked %.4f  shipped %.4f  %+0.4f'%(v[0],v[1],b[v],a[v],a[v]-b[v]),flush=True)
    json.dump({'%s|%d_%d'%(r,v[0],v[1]):keep[(r,t)][v] for r in RULES for t in ['rock_n4'] for v in VIEWS},
              open('s3_rock_views.json','w'),indent=1)
