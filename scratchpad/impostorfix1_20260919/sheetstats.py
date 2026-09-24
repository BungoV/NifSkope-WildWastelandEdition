from PIL import Image
import numpy as np, json, glob, os
FX='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/'
print('subject      span   halfW  partialFrac  |depth|p99  p99/halfW  hmax  depth@hmax')
for tag in ['blast_n4','blast_n8','rock_n4','dead_n4','maple_n4']:
    d=FX+tag+'/cards/'
    lm=glob.glob(d+'*_oct.lodm')
    if not lm: print(tag,'no lodm'); continue
    raw=open(lm[0],'rb').read()
    j=json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    span=j['depthSpan']; halfW=j['half'][0]
    alb=glob.glob(d+'*_oct_albedo.png')[0]; nrm=glob.glob(d+'*_oct_normal.png')[0]
    a=np.asarray(Image.open(alb).convert('RGBA')); n=np.asarray(Image.open(nrm).convert('RGBA'))
    cov=a[:,:,3].astype(np.float64); h=n[:,:,2].astype(np.float64)
    m=cov>=16
    partial=((cov>=16)&(cov<250)).sum()/max(m.sum(),1)
    dep=np.abs((h[m]/255.0-0.5)*span)
    p99=np.percentile(dep,99); hm=h[m].max()
    print('%-10s %6.0f %7.1f %11.3f %11.1f %10.2f %5.0f %11.1f'%(tag,span,halfW,partial,p99,p99/halfW,hm,(hm/255-0.5)*span))
