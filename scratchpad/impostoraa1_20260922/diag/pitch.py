import sys, glob, os, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from impostor_bc_decode import load_dds
from PIL import Image, ImageFilter
from scipy.ndimage import binary_erosion
R='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfin1_20260922/res512'
base='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostoraa1_20260922/diag/stage'
s,fid,N=sys.argv[1],sys.argv[2],int(sys.argv[3])
img,(W,H),_=load_dds(f'{R}/{s}/cards/{fid}_oct_d.DDS'); fw,fh=W//N,H//N
views=sorted(glob.glob(f'{base}/bk_near/{s}/v_*_mesh.png'))
# frame (0,0) width vs mesh px height ratio via heights (vertical extent is robust)
fr=img[:fh,:fw,3]>=0.5; ys=np.nonzero(fr.any(1))[0]; th=ys.max()-ys.min()+1
m=np.asarray(Image.open(f'{base}/bk_near/{s}/v_az{views[0].split("_az")[1][:3]}_el'+views[0].split('_el')[1][:2]+'_mesh.png').convert('RGB')).astype(int)
bg=m[2,2]; mk=np.abs(m-bg).sum(-1)>30; my=np.nonzero(mk.any(1))[0]; mh=my.max()-my.min()+1
f=mh/th; print(f'{s}: frame0 covered height {th} texels, mesh grab {mh} px -> {f:.2f} screen px per sheet texel')
# ceiling: mesh render reduced to the texel pitch and back
def hp(L): 
    im=Image.fromarray(np.clip(L,0,255).astype(np.uint8)); return L-np.asarray(im.filter(ImageFilter.GaussianBlur(4))).astype(float)
r=[]
for p in views:
    a=np.asarray(Image.open(p).convert('RGB')).astype(float); L=a@[0.2126,0.7152,0.0722]
    mk=binary_erosion(np.abs(a-a[2,2]).sum(-1)>30,iterations=6)
    im=Image.fromarray(L.astype(np.uint8)); w,h=im.size
    red=im.resize((max(1,int(w/f)),max(1,int(h/f))),Image.BOX).resize((w,h),Image.BILINEAR)
    r.append([hp(L)[mk].std(),hp(np.asarray(red).astype(float))[mk].std()])
r=np.array(r).mean(0); print(f'  highpass SD mesh {r[0]:.2f}; mesh reduced to the sheet pitch and back {r[1]:.2f} (ratio {r[1]/r[0]:.2f})')
