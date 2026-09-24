import numpy as np, os
from PIL import Image
F=os.path.abspath('../impostorfix1_20260919/control')
CLEAR=np.array([43,45,49],np.int16)
def sil(p):
    a=np.array(Image.open(p).convert('RGB'),np.int16); return (np.abs(a-CLEAR).max(2)>12)
def iou(x,y): return (x&y).sum()/max(1,(x|y).sum())
def bestshift(S,M,R=40):
    fa=np.fft.rfft2(S.astype(np.float32)); fb=np.fft.rfft2(M.astype(np.float32))
    cc=np.fft.irfft2(fa*np.conj(fb),S.shape)
    H,W=S.shape
    best=None
    for dy in range(-R,R+1):
        for dx in range(-R,R+1):
            v=cc[dy%H,dx%W]
            if best is None or v>best[0]: best=(v,dy,dx)
    _,dy,dx=best
    return dy,dx,iou(np.roll(np.roll(S,-dy,0),-dx,1),M)
print(f"{'view':>12} {'IoU raw':>8} {'dy':>4} {'dx':>4} {'IoU@shift':>10} {'recovered':>10}")
tot=[]
for which in ('before','after'):
    rows=[]
    for el in (15,45):
        for az in range(0,360,30):
            m=sil(f'{F}/rock_n4_{which}_b1/v_az{az:03d}_el{el:02d}_mesh.png')
            c=sil(f'{F}/rock_n4_{which}_b1/v_az{az:03d}_el{el:02d}_card.png')
            i0=iou(c,m); dy,dx,i1=bestshift(c,m)
            rows.append((az,el,i0,dy,dx,i1))
    print(f'--- {which} ---')
    for az,el,i0,dy,dx,i1 in rows:
        print(f'  az{az:03d} el{el:02d} {i0:8.4f} {dy:4d} {dx:4d} {i1:10.4f} {i1-i0:+10.4f}')
    mi0=np.mean([r[2] for r in rows]); mi1=np.mean([r[5] for r in rows])
    mdy=np.mean([r[3] for r in rows]); mdy15=np.mean([r[3] for r in rows if r[1]==15]); mdy45=np.mean([r[3] for r in rows if r[1]==45])
    print(f'  MEAN raw {mi0:.4f}  shift-corrected {mi1:.4f}  mean dy {mdy:+.2f} (el15 {mdy15:+.2f}, el45 {mdy45:+.2f})')
    tot.append((which,mi0,mi1,mdy15,mdy45))
print()
print(f"{'':8} {'raw':>8} {'@bestshift':>11} {'dy el15':>8} {'dy el45':>8}")
for w,a,b,c,d in tot: print(f'{w:8} {a:8.4f} {b:11.4f} {c:8.2f} {d:8.2f}')
print(f'DELTA    {tot[1][1]-tot[0][1]:+8.4f} {tot[1][2]-tot[0][2]:+11.4f}')
