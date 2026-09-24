import refcard as R, numpy as np, math, copy
from PIL import Image, ImageDraw
from scipy import ndimage
R.USE_DDS=True
cs=R.CardSet('blast_n4'); c12=R.CardSet('blast_n12')
A=cs.alb[...,3]; Hh=cs.nrm[...,2]; full=A>=250/255.
fixH=Hh.copy()
for j in range(4):
  for i in range(4):
    sl=(slice(j*128,(j+1)*128),slice(i*48,(i+1)*48)); f=full[sl]
    if f.sum()==0: continue
    idx=ndimage.distance_transform_edt(~f,return_distances=False,return_indices=True)
    fixH[sl]=Hh[sl][tuple(idx)]
fx=copy.copy(cs); fx.nrm=cs.nrm.copy(); fx.nrm[...,2]=fixH
def dirOf(az,el):
    a=math.radians(az); e=math.radians(el); return np.array([math.cos(e)*math.cos(a),math.cos(e)*math.sin(a),math.sin(e)])
res=(192,512); W,H=res
ROWS=[('N=12 frame: the mesh, near enough','T',None),
      ('this card, SPEC as shipped  (mean 0.358)','R',(cs,dict())),
      ('height dilated in the BAKE  (mean 0.428)','R',(fx,dict())),
      ('   ... + nearest frame only (mean 0.477)','R',(fx,dict(nframes=1)))]
azs=[0,60,120,180,240,300]; el=15
pad=6; lab=300
im=Image.new('RGB',(lab+len(azs)*(W+pad)+pad, 30+len(ROWS)*(H+22)),(28,28,32)); d=ImageDraw.Draw(im)
d.text((8,8),'blast_n4  elev 15  --  Python reference card, written from the spec, reading the SAME BC3 .DDS the viewer reads',fill=(240,240,240))
for ri,(name,kind,spec) in enumerate(ROWS):
    y=30+ri*(H+22); d.text((8,y+H//2),name,fill=(185,195,215))
    for ci,az in enumerate(azs):
        dd=dirOf(az,el); t=R.truth(c12,dd,res,cs.half)[0]
        m = t if kind=='T' else R.render(spec[0],dd,res=res,**spec[1])[0]
        rgb=np.zeros((H,W,3),np.uint8); rgb[...,:]=(22,22,26)
        rgb[t&~m]=(70,80,110); rgb[m&~t]=(190,70,60); rgb[m&t]=(225,215,180)
        x=lab+ci*(W+pad); im.paste(Image.fromarray(rgb),(x,y))
        d.text((x+2,y+H+4),'az %d   IoU %.3f'%(az,R.iou(m,t)),fill=(205,205,205))
d.text((8,30+len(ROWS)*(H+22)-14),'cream = agrees   red = card ink with no mesh under it   blue = mesh the card missed',fill=(170,175,185))
im.save('ref_strip_dds_el15.png'); print('wrote ref_strip_dds_el15.png',im.size)
