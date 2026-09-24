import sys, json, numpy as np
sys.path.insert(0,'.')
from inst import *
import calib
from calib import place, mask_at, alpha_ref
calib.REFH=448
from PIL import Image, ImageDraw
CAL=json.load(open('calib.json'))
TAG='blast_n4'; cs=Sheets(TAG); c=CAL[TAG]; s=c['scale']
VS=[(30,15),(270,15),(300,15),(0,15)]
def M(az,el,**kw):
    dy,dx=c['off']['%d_%d'%(az,el)]
    return place(mask_at(alpha_ref(cs,dirOf(az,el),**kw),cs,s,cs.covFloor),dy,dx)
cols=[]
for az,el in VS:
    g=grabmask(TAG,'after',az,el,'mesh')
    spec=M(az,el); dom=M(az,el,nearestOnly=True); w4=M(az,el,wpow=4.0); rj=M(az,el,reject=0.25*255/16)
    def tri(S):
        o=np.zeros(g.shape+(3,),np.uint8); o[...]=(24,25,28)
        o[g&~S]=(200,60,60); o[S&~g]=(60,140,220); o[S&g]=(205,205,205); return o
    ghost=np.zeros(g.shape+(3,),np.uint8); ghost[...]=(24,25,28)
    ghost[dom]=(120,120,120); ghost[spec&~dom]=(255,120,40)     # ink ONLY the minor frames paint
    col=np.concatenate([tri(spec),ghost,tri(w4),tri(rj)],0)
    cols.append((col,az,el,iou(spec,g),iou(w4,g),iou(rj,g),(spec&~dom).sum()/max(1,spec.sum())))
W=cols[0][0].shape[1];H=cols[0][0].shape[0];sc=0.30
w=int(W*sc);h=int(H*sc)
sh=Image.new('RGB',(w*len(cols),h+74),(16,17,19)); d=ImageDraw.Draw(sh)
for i,(col,az,el,i0,i4,ir,mi) in enumerate(cols):
    sh.paste(Image.fromarray(col).resize((w,h),Image.LANCZOS),(i*w,70))
    d.text((i*w+4,3),'blast_n4 az%03d el%02d'%(az,el),fill=(235,235,235))
    d.text((i*w+4,16),'spec  IoU %.4f'%i0,fill=(210,210,210))
    d.text((i*w+4,28),'GHOST ink %.1f%%'%(100*mi),fill=(255,150,70))
    d.text((i*w+4,40),'w^4   IoU %.4f'%i4,fill=(150,220,255))
    d.text((i*w+4,52),'reject IoU %.4f'%ir,fill=(150,255,180))
d.text((4,h+62),'rows: spec | GHOST MAP grey=dominant frame, ORANGE=ink only the minor frames paint | weights^4 | height reject 0.25',fill=(140,140,140))
sh.save('images/04_ghost_minor_frames.png'); print('wrote images/04_ghost_minor_frames.png',sh.size)
for col,az,el,i0,i4,ir,mi in cols: print('az%03d el%02d spec %.4f w4 %.4f reject %.4f ghostink %.1f%%'%(az,el,i0,i4,ir,100*mi))
