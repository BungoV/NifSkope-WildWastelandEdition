import refcard as R, numpy as np, math, sys
cs=R.CardSet('blast_n4'); c12=R.CardSet('blast_n12')
def dirOf(az,el):
    a=math.radians(az); e=math.radians(el)
    return np.array([math.cos(e)*math.cos(a), math.cos(e)*math.sin(a), math.sin(e)])
views=[(az,el) for el in (15,45) for az in range(0,360,30)]
res=(192,512)
VARIANTS=[
 ('SPEC as shipped (3 frames, parallax, offset, decode)', dict()),
 ('no parallax',                                          dict(parallax=False)),
 ('nearest frame only, parallax on',                      dict(nframes=1)),
 ('nearest frame only, no parallax',                      dict(nframes=1,parallax=False)),
 ('no frameOffset',                                       dict(useOffset=False)),
 ('coverage NOT decoded',                                 dict(decode=False)),
 ('parallax clamped to 1 half-width (135)',               dict(clampStep=135.0)),
 ('parallax clamped to 0.25 half-width (34)',             dict(clampStep=34.0)),
 ('parallax only where coverage is FULL',                 dict(covGate=True)),
 ('clamped 135 + full-coverage gate',                     dict(clampStep=135.0,covGate=True)),
]
T={}
for (az,el) in views:
    d=dirOf(az,el); T[(az,el)]=R.truth(c12,d,res,cs.half)[0]
print('truth = nearest N=12 frame placed by its own frameOffset  (<=4 deg from the view)')
print('truth mean inked fraction of quad: %.4f'%np.mean([t.mean() for t in T.values()]))
print()
print('%-52s  IoU15   IoU45   mean   inkFrac  ink/truth'%'variant')
for name,kw in VARIANTS:
    a15=[];a45=[];fr=[];rt=[]
    for (az,el) in views:
        d=dirOf(az,el)
        m,_=R.render(cs,d,res=res,**kw)
        v=R.iou(m,T[(az,el)])
        (a15 if el==15 else a45).append(v)
        fr.append(m.mean()); rt.append(m.sum()/max(T[(az,el)].sum(),1))
    print('%-52s  %.3f   %.3f   %.3f   %.4f   %.2f'%(name,np.mean(a15),np.mean(a45),np.mean(a15+a45),np.mean(fr),np.mean(rt)))
