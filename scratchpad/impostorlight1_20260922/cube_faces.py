# per-face table on the cube control: mesh vs card lit luma grouped by the mesh's view-space normal (s1_mv mesh), fix and old
import glob, os, numpy as np
from PIL import Image
BG=np.array([43,45,49]); W=np.array([.2126,.7152,.0722])
def im(p): return np.asarray(Image.open(p).convert('RGB')).astype(float)
C='cubectl'
rows={}
for m in sorted(glob.glob(C+'/s1_mv/blast_n4/*_mesh.png')):
    b=os.path.basename(m); nm=im(m); nc=im(m.replace('_mesh','_card'))
    k=(np.abs(nm-BG).sum(-1)>12)&(np.abs(nc-BG).sum(-1)>12)
    n=nm[k]/255*2-1; z=np.round(n[:,2],1)
    for lab in ('fix','r0','gloss','lambert'):
        ml=im('%s/%s/blast_n4/%s'%(C,lab,b))[k]@W; cl=im('%s/%s/blast_n4/%s'%(C,lab,b.replace('_mesh','_card')))[k]@W
        al=im('%s/s2/blast_n4/%s'%(C,b))[k]@W
        for zz in np.unique(z):
            s=z==zz
            if s.sum()<2000: continue
            r=rows.setdefault((lab,zz),[0,0,0,0]); r[0]+=ml[s].sum(); r[1]+=cl[s].sum(); r[2]+=s.sum(); r[3]+=al[s].sum()
for (lab,zz),r in sorted(rows.items()):
    print('%s  view-z %.1f  mesh %.1f  card %.1f  ratio %.3f  mesh albedo %.1f  px %d'%(lab,zz,r[0]/r[2],r[1]/r[2],r[1]/r[0],r[3]/r[2],r[2]))
