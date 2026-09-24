"""DEFECT 2 -- where the surviving height error IS, on the VALIDATED input."""
import os,sys,glob,json
import numpy as np
from PIL import Image
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from s2c_encoder import dds_mip0_blocks,to_blocks,encode_colour,decode_colour,blocks_to_img
TAGS=('blast_n4','blast_n8','maple_n4','dead_n4','rock_n4')
def blk(a,bs=4):
    H,W=a.shape; return a[:H//bs*bs,:W//bs*bs].reshape(H//bs,bs,W//bs,bs).transpose(0,2,1,3).reshape(H//bs,W//bs,-1)
print('%-10s %9s %9s %9s | %8s | %28s'%('tag','>12 lv','>30 lv','of all','1 lv =','bad blocks: on edge / on ring-8 cliff'))
out=[]
for tag in TAGS:
    cs=Sheets(tag,'cards',root=R3,sub='fixture')
    nrm=np.load('nrm_true_%s.npy'%tag)
    pa=np.asarray(Image.open(glob.glob(cs.dir+'*_oct_albedo.png')[0]).convert('RGBA'))[...,3]
    raw,w,h,bw,bh=dds_mip0_blocks(cs.nrmPath)
    B=to_blocks(nrm[...,:3].astype(float)); c0,c1,bits=encode_colour(B,'lum')
    dec=blocks_to_img(decode_colour(c0,c1,bits),h,w)
    ref=nrm[:bh*4,:bw*4,2].astype(float); err=np.abs(dec[...,2]-ref)
    alpha=pa[:bh*4,:bw*4]
    bmax=blk(err).max(-1)
    edge=blk((alpha>=16).astype(float)).ptp(-1)>0
    rng=blk(ref).ptp(-1)
    bad=bmax>12
    unit=cs.span/255.0
    print('%-10s %8d %9d %8.2f%% | %7.1f u | edge %5.1f%% (base %4.1f%%)  range %5.1f lv (base %4.1f)'%(
        tag,int((err>12).sum()),int((err>30).sum()),100*(err>12).mean(),unit,
        100*edge[bad].mean() if bad.any() else 0,100*edge.mean(),
        rng[bad].mean() if bad.any() else 0,rng.mean()))
    out.append(dict(tag=tag,n12=int((err>12).sum()),n30=int((err>30).sum()),
        frac12=float((err>12).mean()),unit=float(unit),
        edge_bad=float(edge[bad].mean()) if bad.any() else 0,edge_base=float(edge.mean()),
        rng_bad=float(rng[bad].mean()) if bad.any() else 0,rng_base=float(rng.mean()),
        nbad=int(bad.sum()),nblk=int(bad.size)))
json.dump(out,open('s2e.json','w'),indent=1)
print()
print('worst-block height error in WORLD UNITS (the parallax step reads this as a depth)')
for r in out:
    print('  %-10s %d blocks over 12 levels of %d (%.1f%%);  12 levels = %.0f world units'%(
        r['tag'],r['nbad'],r['nblk'],100*r['nbad']/r['nblk'],12*r['unit']))
