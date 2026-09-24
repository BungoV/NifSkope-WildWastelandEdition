import sys, glob, os, numpy as np
from PIL import Image, ImageFilter
base='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostoraa1_20260922/diag/stage'
vars_=sys.argv[1].split(','); s=sys.argv[2]
def load(p):
    im=Image.open(p).convert('RGB'); a=np.asarray(im).astype(float); bg=a[2,2]
    L=a@[0.2126,0.7152,0.0722]; Lb=np.asarray(im.convert('L').filter(ImageFilter.GaussianBlur(4))).astype(float)
    return np.abs(a-bg).sum(-1)>30, L, L-Lb
views=sorted(os.path.basename(p)[2:-9] for p in glob.glob(f'{base}/{vars_[0]}/{s}/v_*_mesh.png'))
for v in vars_:
    r=[]
    for vw in views:
        mm,ml,mh=load(f'{base}/{vars_[0]}/{s}/v_{vw}_mesh.png'); cm,cl,ch=load(f'{base}/{v}/{s}/v_{vw}_card.png')
        k=mm&cm
        from scipy.ndimage import binary_erosion
        k=binary_erosion(k,iterations=6)
        r.append([mh[k].std(),ch[k].std(),ml[k].std(),cl[k].std(),ml[k].mean(),cl[k].mean()])
    R=np.array(r).mean(0)
    print(f'{s} {v:8s} highpass SD mesh {R[0]:.2f} card {R[1]:.2f} (ratio {R[1]/R[0]:.2f}) | lum SD mesh {R[2]:.1f} card {R[3]:.1f} | mean mesh {R[4]:.1f} card {R[5]:.1f}')
