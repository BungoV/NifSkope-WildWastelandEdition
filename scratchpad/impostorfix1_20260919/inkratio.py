# Registration-free invariant: how many pixels does the card ink vs the mesh,
# in the SAME viewport, same camera. Background test copied from compose_orbit.py.
from PIL import Image
import numpy as np, os, sys
O='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/orbit/'
def sil(p):
    a=np.asarray(Image.open(p).convert('RGB')).astype(np.int32)
    bg=a[0,0]
    return (np.abs(a-bg).sum(2)>24)
for tag in ['blast_n4','blast_n8','rock_n4','dead_n4','maple_n4']:
    d=O+tag
    if not os.path.isdir(d): continue
    print('== '+tag)
    for el in (15,45):
        rs=[]
        for az in range(0,360,30):
            mp='%s/v_az%03d_el%02d_mesh.png'%(d,az,el); cp='%s/v_az%03d_el%02d_card.png'%(d,az,el)
            if not os.path.exists(mp): continue
            m=sil(mp); c=sil(cp)
            inter=(m&c).sum(); union=(m|c).sum()
            rs.append((az,m.sum(),c.sum(),c.sum()/max(m.sum(),1),inter/max(union,1),inter/max(c.sum(),1)))
        if not rs: continue
        print(' elev %d:  mean card/mesh ink ratio %.2f   mean IoU %.3f   mean precision(card ink on mesh) %.3f'%(
            el, np.mean([r[3] for r in rs]), np.mean([r[4] for r in rs]), np.mean([r[5] for r in rs])))
        print('   az:ratio ' + ' '.join('%d:%.2f'%(r[0],r[3]) for r in rs))
