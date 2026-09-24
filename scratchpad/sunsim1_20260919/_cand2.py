import numpy as np, sys, os
sys.path.insert(0,'.')
from scene import Terrain, Objects, Sheet
from render import Camera, GBuffer, SunShadow
import shade as SH
from PIL import Image
ter=Terrain(); ob=Objects(verbose=False)
def g(x,y): return float(ter.atf(np.array([x]),np.array([y]))[0])
CAND={
 'F':((17600.,-48600.,2600.),(26200.,-41200.,150.),58.),
 'G':((18200.,-48000.,3400.),(26800.,-41000.,100.),58.),
 'H':((19600.,-46600.,1500.),(28000.,-40200.,300.),55.),
 'I':((30500.,-45500.,1600.),(22500.,-39500.,200.),58.),
}
W,H=560,315
ims=[]
for k,(e,t,f) in CAND.items():
    cam=Camera((e[0],e[1],g(e[0],e[1])+e[2]),(t[0],t[1],g(t[0],t[1])+t[2]),f,W,H,name=k)
    gb=GBuffer(cam,ter,ob,verbose=False)
    row=[]
    for az,el in ((120.,15.),(240.,15.)):
        ss=SunShadow(ter,ob,az,el)
        lit=np.zeros(len(gb.kind)); m=gb.kind!=0
        lit[m]=ss.lit(gb.pos[m]).astype(float)
        img=SH.to8(SH.shade(gb,lit,el,az),W,H)
        row.append(SH.stamp(img,'%s az%.0f'%(k,az),(6,4),16))
    ims.append(np.concatenate(row,axis=1))
    print(k,'sky %.0f%% ter %.0f%% obj %.0f%%'%(100*(gb.kind==0).mean(),100*gb.terfirst.mean(),100*gb.objfirst.mean()))
Image.fromarray(np.concatenate(ims,axis=0)).save('images/_cand2.png')
