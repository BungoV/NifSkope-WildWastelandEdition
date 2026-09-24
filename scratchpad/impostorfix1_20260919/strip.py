import refcard as R, numpy as np, math
from PIL import Image, ImageDraw
cs=R.CardSet('blast_n4'); c12=R.CardSet('blast_n12')
def dirOf(az,el):
    a=math.radians(az); e=math.radians(el)
    return np.array([math.cos(e)*math.cos(a), math.cos(e)*math.sin(a), math.sin(e)])
res=(192,512)
ROWS=[('N=12 frame (near-photograph truth)',None),
      ('reference: SPEC as shipped',dict()),
      ('reference: no parallax',dict(parallax=False)),
      ('reference: nearest frame only',dict(nframes=1))]
azs=[0,60,120,180,240,300]; el=15
W,H=res; pad=6; lab=170
im=Image.new('RGB',(lab+len(azs)*(W+pad)+pad, 26+len(ROWS)*(H+22)),(30,30,34))
d=ImageDraw.Draw(im)
d.text((8,6),'blast_n4  elev 15  -- Python reference card from the SHEETS alone (IMPOSTORFIX1)',fill=(235,235,235))
for ri,(name,kw) in enumerate(ROWS):
    y=26+ri*(H+22); d.text((8,y+H//2),name,fill=(180,190,210))
    for ci,az in enumerate(azs):
        dd=dirOf(az,el)
        m = R.truth(c12,dd,res,cs.half)[0] if kw is None else R.render(cs,dd,res=res,**kw)[0]
        t = R.truth(c12,dd,res,cs.half)[0]
        rgb=np.zeros((H,W,3),np.uint8); rgb[...,:]= (24,24,28)
        rgb[m]=(225,215,180)
        x=lab+ci*(W+pad)
        im.paste(Image.fromarray(rgb),(x,y))
        d.text((x+2,y+H+4),'az %d   IoU vs truth %.3f'%(az,R.iou(m,t)),fill=(200,200,200))
im.save('ref_strip_el15.png'); print('wrote ref_strip_el15.png',im.size)
