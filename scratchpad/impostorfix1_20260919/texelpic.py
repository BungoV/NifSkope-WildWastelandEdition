import numpy as np, bcdec
from PIL import Image, ImageDraw
D='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/blast_n4/cards/'
dds=bcdec.load_dds(D+'000531b3_oct_n.DDS')[0]
png=np.asarray(Image.open(D+'000531b3_oct_normal.png').convert('RGBA')).astype(np.float64)/255
alb=np.asarray(Image.open(D+'000531b3_oct_albedo.png').convert('RGBA')).astype(np.float64)/255
# frame (2,2): 48x128 at (96,256). Crop 24x32 on the trunk.
x0,y0,w,h=96+10,256+70,24,32
Z=14; pad=24; lab=40
cells=[('height, PNG (8 bit)',png[y0:y0+h,x0:x0+w,2]),
       ('height, BC3 blue (5 bit, 4x4 blocks)',dds[y0:y0+h,x0:x0+w,2]),
       ('coverage alpha, PNG',alb[y0:y0+h,x0:x0+w,3])]
CW=w*Z; CH=h*Z
im=Image.new('RGB',(pad+len(cells)*(CW+pad), 64+CH+66),(28,28,32)); d=ImageDraw.Draw(im)
d.text((pad,10),'blast_n4  frame (2,2)  24 x 32 texels of the trunk, 14x, each texel a square',fill=(235,235,235))
d.text((pad,26),'depthSpan 3072 world units over 0..1, so ONE 8-bit step = 12 units and ONE BC1-blue step = 99 units;',fill=(210,200,170))
d.text((pad,42),'the card half-width is 135 units, so one BC block step throws a texel 0.73 of a half-width sideways.',fill=(210,200,170))
for ci,(name,a) in enumerate(cells):
    x=pad+ci*(CW+pad); y=64
    v=np.clip((a-0.42)/0.28,0,1) if ci<2 else a
    rgb=(np.stack([v,v,v],-1)*255).astype(np.uint8)
    im.paste(Image.fromarray(rgb).resize((CW,CH),Image.NEAREST),(x,y))
    for k in range(0,w+1,4): d.line([(x+k*Z,y),(x+k*Z,y+CH)],fill=(255,90,60),width=1)
    for k in range(0,h+1,4): d.line([(x,y+k*Z),(x+CW,y+k*Z)],fill=(255,90,60),width=1)
    d.text((x,y+CH+6),name,fill=(225,225,225))
    if ci<2:
        lv=len(np.unique(np.round(a*255)))
        sp=(a.max()-a.min())*3072
        d.text((x,y+CH+22),'%d distinct levels in this crop'%lv,fill=(190,200,220))
        d.text((x,y+CH+38),'spread = %.0f world units of parallax throw'%sp,fill=(190,200,220))
d.text((pad,64+CH+56),'red grid = the 4x4 BC block lattice. The right-hand panel is the same crop\'s coverage: the trunk is solid there, so this is not an edge artefact.',fill=(170,175,185))
im.save('texel_height_bc.png'); print('wrote texel_height_bc.png',im.size)
