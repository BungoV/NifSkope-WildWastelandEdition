import numpy as np, rdmp, re
d=rdmp.load('rule_dump.bin'); F=d['forms']; pc=d['palCol']; P=len(pc)
names=[re.split(r'[\/]',F[d['palFrom'][p]][2])[-1][:26] for p in range(P)]
rock=[bool(rdmp.ROCK.search(F[d['palFrom'][p]][2])) for p in range(P)]
g=pc.mean(1)
for p in np.argsort(-g)[:10]: print(f"{names[p]:26s} rock={rock[p]} rgb={np.round(255*pc[p]).astype(int)}")
painted=d['mw'].sum(1)>0; out=~painted&(d['have']>0)
sb=np.digitize(np.degrees(np.arctan(d['s'])),rdmp.SLOPE)
rng=np.random.default_rng(1)
for k in (3,4,5,6):
  idx=np.flatnonzero(out&(sb==k)); idx=rng.choice(idx,min(20000,len(idx)),replace=False)
  V=d['V'][idx]; best=np.full(len(idx),1e9)
  for i in range(P):
    for j in range(i,P):
      dd=pc[i]-pc[j]; den=(dd*dd).sum()
      w=np.clip(((V-pc[j])*dd).sum(1)/den,0,1) if den>1e-9 else np.ones(len(idx))
      e=np.sqrt((((V-(pc[j]+dd*w[:,None]))*255)**2).sum(1)); best=np.minimum(best,e)
  print(f"bin{k} out: pure-colour best-pair rmse={best.mean():5.1f}")
