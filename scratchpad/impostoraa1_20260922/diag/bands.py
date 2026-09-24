import sys, glob, os, numpy as np
from PIL import Image
base='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostoraa1_20260922/diag/stage'
vars_=sys.argv[1].split(','); subs=sys.argv[2].split(',')
def ink(p):
    a=np.asarray(Image.open(p).convert('RGB')).astype(int); bg=a[2,2]
    return np.abs(a-bg).sum(-1)>30
for s in subs:
    views=sorted(os.path.basename(p)[2:-9] for p in glob.glob(f'{base}/{vars_[0]}/{s}/v_*_mesh.png'))
    for v in vars_:
        rows=[]
        for vw in views:
            m=ink(f'{base}/{vars_[0]}/{s}/v_{vw}_mesh.png'); c=ink(f'{base}/{v}/{s}/v_{vw}_card.png')
            ys=np.nonzero(m.any(1))[0]; y0,y1=ys.min(),ys.max()+1; e=np.linspace(y0,y1,4).astype(int)
            r=[ (m[e[k]:e[k+1]]&c[e[k]:e[k+1]]).sum()/max(1,m[e[k]:e[k+1]].sum()) for k in range(3)]
            iou=(m&c).sum()/max(1,(m|c).sum())
            rows.append(r+[iou])
        R=np.array(rows).mean(0)
        print(f'{s:9s} {v:8s} recall top {R[0]:.3f} mid {R[1]:.3f} bottom {R[2]:.3f}  iou {R[3]:.3f}  ({len(views)} views)')
