# transfer SLOPE: card rise / mesh rise over the mesh's luma deciles (1.0 = the card follows the mesh's shading exactly)
import glob, os, sys, numpy as np
from PIL import Image
BG=np.array([43,45,49]); W=np.array([.2126,.7152,.0722])
def im(p): return np.asarray(Image.open(p).convert('RGB')).astype(float)
def run(lit, nrm):
    lm=[];lc=[]
    for m in sorted(glob.glob(nrm+'/*_mesh.png')):
        b=os.path.basename(m); k=(np.abs(im(m)-BG).sum(-1)>12)&(np.abs(im(m.replace('_mesh','_card'))-BG).sum(-1)>12)
        lm.append(im(lit+'/'+b)[k]@W); lc.append(im(lit+'/'+b.replace('_mesh','_card'))[k]@W)
    lm=np.concatenate(lm); lc=np.concatenate(lc)
    q=np.percentile(lm,np.linspace(0,100,11)); idx=np.clip(np.searchsorted(q,lm,side='right')-1,0,9)
    dm=[lm[idx==i].mean() for i in range(10)]; dc=[lc[idx==i].mean() for i in range(10)]
    mr=np.mean(dm[5:])-np.mean(dm[:5]); cr=np.mean(dc[5:])-np.mean(dc[:5])
    return mr, cr
for t in sys.argv[1:]:
    out=[]
    for lab,lit in (('old','diag/s6'),('rough1','diag/final/s6'),('lambert','diag/fix_lambert/s6'),('gloss','diag/fix_oren/s6'),('r0','diag/r0b/s6')):
        if not os.path.isdir(lit+'/'+t): continue
        mr,cr=run(lit+'/'+t,'diag/final/s1_lit/'+t); out.append('%s %.2f'%(lab,cr/mr))
    print(t,'mesh rise %.1f'%mr,' slope '+'  '.join(out))
