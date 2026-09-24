import sys, json, numpy as np
sys.path.insert(0,'.')
from inst import *
import calib
from calib import place, mask_at, alpha_ref
calib.REFH=448
import s3_rock as S3
from PIL import Image, ImageDraw
CAL=json.load(open('calib.json'))
JOBS=[('rock_n4',(60,15)),('rock_n4',(90,15)),('blast_n4',(30,15)),('dead_n4',(60,15))]
cols=[]
cache={}
for tag,(az,el) in JOBS:
    for r in ('R3_shipped','R2d8'):
        if (tag,r) not in cache: cache[(tag,r)]=S3.variant(tag,r)
    g=grabmask(tag,'after',az,el,'mesh'); c=CAL[tag]; s=c['scale']; dy,dx=c['off']['%d_%d'%(az,el)]
    def M(cs): return place(mask_at(alpha_ref(cs,dirOf(az,el)),cs,s,cs.covFloor),dy,dx)
    a=M(cache[(tag,'R3_shipped')]); b=M(cache[(tag,'R2d8')])
    def tri(S):
        o=np.zeros(g.shape+(3,),np.uint8); o[...]=(24,25,28)
        o[g&~S]=(200,60,60); o[S&~g]=(60,140,220); o[S&g]=(205,205,205); return o
    cols.append((np.concatenate([tri(a),tri(b)],0),tag,az,el,iou(a,g),iou(b,g)))
W=cols[0][0].shape[1];H=cols[0][0].shape[0];sc=0.40
w=int(W*sc);h=int(H*sc)
sh=Image.new('RGB',(w*len(cols),h+56),(16,17,19)); d=ImageDraw.Draw(sh)
for i,(col,tag,az,el,i0,i1) in enumerate(cols):
    sh.paste(Image.fromarray(col).resize((w,h),Image.LANCZOS),(i*w,52))
    d.text((i*w+4,3),'%s az%03d el%02d'%(tag,az,el),fill=(235,235,235))
    d.text((i*w+4,17),'R3 shipped  IoU %.4f'%i0,fill=(255,190,120))
    d.text((i*w+4,30),'R2d8 dilate IoU %.4f  %+0.4f'%(i1,i1-i0),fill=(150,255,180))
d.text((4,h+44),'top row = the height fill that shipped today   bottom row = dilate 8 texels outside coverage   red=mesh only  blue=card only',fill=(140,140,140))
sh.save('images/02_height_fill_R3_vs_R2d8.png'); print('wrote images/02_height_fill_R3_vs_R2d8.png',sh.size)
for col,tag,az,el,i0,i1 in cols: print('%-9s az%03d el%02d  R3 %.4f  R2d8 %.4f  %+0.4f'%(tag,az,el,i0,i1,i1-i0))
