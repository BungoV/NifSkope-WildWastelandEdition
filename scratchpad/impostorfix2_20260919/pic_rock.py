import numpy as np, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0,os.path.abspath('.'))
F=os.path.abspath('../impostorfix1_20260919/control')
CLEAR=np.array([43,45,49],np.int16)
def sil(p):
    a=np.array(Image.open(p).convert('RGB'),np.int16)
    return (np.abs(a-CLEAR).max(2)>12)
def rgb(p): return np.array(Image.open(p).convert('RGB'))
VIEWS=[(60,15),(90,15),(180,15),(0,15)]
cols=[]
for az,el in VIEWS:
    mb=f'{F}/rock_n4_before_b1/v_az{az:03d}_el{el:02d}_mesh.png'
    cb=f'{F}/rock_n4_before_b1/v_az{az:03d}_el{el:02d}_card.png'
    ca=f'{F}/rock_n4_after_b1/v_az{az:03d}_el{el:02d}_card.png'
    M,B,A=sil(mb),sil(cb),sil(ca)
    def iou(x,y): return (x&y).sum()/max(1,(x|y).sum())
    def ov(S):
        o=np.zeros(M.shape+(3,),np.uint8); o[...]=(24,25,28)
        o[M&~S]=(200,60,60)      # mesh only  = MISSING
        o[S&~M]=(60,140,220)     # card only  = EXTRA
        o[S&M]=(210,210,210)
        return o
    col=np.concatenate([rgb(mb),ov(B),ov(A)],0)
    cols.append((col,az,el,iou(M,B),iou(M,A),B.sum()/max(1,M.sum()),A.sum()/max(1,M.sum())))
W=cols[0][0].shape[1]; H=cols[0][0].shape[0]
sc=0.42; w=int(W*sc); h=int(H*sc)
sheet=Image.new('RGB',(w*len(cols),h+64),(16,17,19))
d=ImageDraw.Draw(sheet)
for i,(col,az,el,ib,ia,rb,ra) in enumerate(cols):
    sheet.paste(Image.fromarray(col).resize((w,h),Image.LANCZOS),(i*w,58))
    d.text((i*w+6,4),f'rock_n4 az{az:03d} el{el:02d}',fill=(235,235,235))
    d.text((i*w+6,18),f'BEFORE IoU {ib:.4f}  ink/mesh {rb:.3f}',fill=(255,190,120))
    d.text((i*w+6,32),f'AFTER  IoU {ia:.4f}  ink/mesh {ra:.3f}   d={ia-ib:+.4f}',fill=(150,220,255))
    d.text((i*w+6,46),'rows: MESH grab | BEFORE overlay | AFTER overlay   red=mesh-only(missing) blue=card-only(extra)',fill=(150,150,150))
sheet.save('images/03_rock_worst_before_after.png')
print('wrote images/03_rock_worst_before_after.png',sheet.size)
for col,az,el,ib,ia,rb,ra in cols: print(f'az{az:03d} el{el:02d} before {ib:.4f} after {ia:.4f} delta {ia-ib:+.4f} inkratio {rb:.3f}->{ra:.3f}')
