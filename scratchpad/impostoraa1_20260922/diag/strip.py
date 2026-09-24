import sys, numpy as np
from PIL import Image
base='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostoraa1_20260922/diag/stage'
subj=sys.argv[1]; view=sys.argv[2]; vars_=sys.argv[3].split(','); out=sys.argv[4]
ims=[Image.open(f'{base}/{vars_[0]}/{subj}/v_{view}_mesh.png').convert('RGB')]
ims+= [Image.open(f'{base}/{v}/{subj}/v_{view}_card.png').convert('RGB') for v in vars_]
# crop to union bbox of non-bg
arrs=[np.asarray(i).astype(int) for i in ims]
bg=arrs[0][2,2]
m=np.zeros(arrs[0].shape[:2],bool)
for a in arrs: m|=np.abs(a-bg).sum(-1)>30
ys,xs=np.nonzero(m); y0,y1,x0,x1=ys.min()-8,ys.max()+8,xs.min()-8,xs.max()+8
cr=[i.crop((x0,y0,x1,y1)) for i in ims]
W=sum(c.width for c in cr); H=cr[0].height
o=Image.new('RGB',(W,H)); x=0
for c in cr: o.paste(c,(x,0)); x+=c.width
s=900/H if H>900 else 1; o=o.resize((int(W*s),int(H*s)))
o.save(out); print(out,o.size)
