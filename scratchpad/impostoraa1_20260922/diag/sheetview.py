import sys, numpy as np
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from impostor_bc_decode import load_dds
from PIL import Image
root='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfin1_20260922/res512'
out='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostoraa1_20260922/diag'
for tag,fid in [('blast_n4','000531b3'),('maple_n4','0004a074'),('rock_n4','000211a3'),('blast_n8','000531b3')]:
    img,(w,h),fc=load_dds(f'{root}/{tag}/cards/{fid}_oct_d.DDS')
    a=img[...,3]; rgb=img[...,:3]
    # composite over magenta where a>=128/255 else dark
    cov=a>=128/255.
    vis=np.where(cov[...,None],rgb,np.array([0.1,0.1,0.15]))
    Image.fromarray((vis*255).astype(np.uint8)).save(f'{out}/{tag}_sheet_cut.png')
    png=np.asarray(Image.open(f'{root}/{tag}/cards/{fid}_oct_albedo.png').convert('RGBA')).astype(float)
    print(tag,w,h,fc,'dds cov>=128',cov.mean().round(4),'png raw a>=16',(png[...,3]>=16).mean().round(4),'png a>=128',(png[...,3]>=128).mean().round(4))
