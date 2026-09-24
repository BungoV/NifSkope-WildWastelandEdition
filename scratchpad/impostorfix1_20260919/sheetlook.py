from PIL import Image
import numpy as np, os
F='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/blast_n4/cards/'
a=np.asarray(Image.open(F+'000531b3_oct_albedo.png').convert('RGBA')).astype(np.int32)
n=np.asarray(Image.open(F+'000531b3_oct_normal.png').convert('RGBA')).astype(np.int32)
N=4; fw,fh=48,128
span=3072.0; halfW,halfH=135.314,360.837
print('per-frame: covTexels  h(range on covered)  worldDepth(range)  maxThrow/halfW')
rows=[]
for j in range(N):
    for i in range(N):
        A=a[j*fh:(j+1)*fh, i*fw:(i+1)*fw]
        H=n[j*fh:(j+1)*fh, i*fw:(i+1)*fw,2]
        cov=A[:,:,3]
        m=cov>=16
        if m.sum()==0: print(i,j,'EMPTY'); continue
        hs=H[m]
        d=(hs/255.0-0.5)*span
        rows.append((i,j,int(m.sum()),int(hs.min()),int(hs.max()),d.min(),d.max()))
        print('f(%d,%d) cov=%5d  h=%3d..%3d  depth=%8.1f..%8.1f  |maxd|/halfW=%6.2f'%(i,j,m.sum(),hs.min(),hs.max(),d.min(),d.max(),max(abs(d.min()),abs(d.max()))/halfW))
# background height
bg=n[:,:,2][a[:,:,3]<16]
print('background(cov<16) height unique:',np.unique(bg)[:10],'count',bg.size)
