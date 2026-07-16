import numpy as np, sys
import nifshape
from collections import defaultdict

def closest_pt_tri(p, a, b, c):
    ab=b-a; ac=c-a; ap=p-a
    d1=ab@ap; d2=ac@ap
    if d1<=0 and d2<=0: return a,(1.,0.,0.)
    bp=p-b; d3=ab@bp; d4=ac@bp
    if d3>=0 and d4<=d3: return b,(0.,1.,0.)
    vc=d1*d4-d3*d2
    if vc<=0 and d1>=0 and d3<=0:
        v=d1/(d1-d3); return a+v*ab,(1-v,v,0.)
    cp=p-c; d5=ab@cp; d6=ac@cp
    if d6>=0 and d5<=d6: return c,(0.,0.,1.)
    vb=d5*d2-d1*d6
    if vb<=0 and d2>=0 and d6<=0:
        w=d2/(d2-d6); return a+w*ac,(1-w,0.,w)
    va=d3*d6-d5*d4
    if va<=0 and (d4-d3)>=0 and (d5-d6)>=0:
        w=(d4-d3)/((d4-d3)+(d5-d6)); return b+w*(c-b),(0.,1-w,w)
    den=1.0/(va+vb+vc); v=vb*den; w=vc*den
    return a+ab*v+ac*w,(1-v-w,v,w)

def build_incident(tris, nv):
    inc=defaultdict(list)
    for t,(i,j,k) in enumerate(tris):
        inc[i].append(t); inc[j].append(t); inc[k].append(t)
    return inc

def normalize_top4(d):
    items=sorted(d.items(), key=lambda kv:-kv[1])[:4]
    s=sum(w for _,w in items)
    if s<=0: return {}
    return {b:w/s for b,w in items}

def transfer(donor, target, mode='face'):
    D=donor['pos']; T=target['pos']; tris=donor['tris']; dskin=donor['skin']
    inc=build_incident(tris, donor['nv'])
    out=[]; dists=[]
    for ti in range(target['nv']):
        p=T[ti]
        # nearest donor vertex
        nv=int(np.argmin(((D-p)**2).sum(1)))
        if mode=='vertex':
            out.append(normalize_top4(dict(dskin[nv]))); dists.append(np.linalg.norm(D[nv]-p)); continue
        # refine over triangles incident to nearest vertex
        best=None
        for t in inc[nv]:
            ia,ib,ic=tris[t]
            cp,bary=closest_pt_tri(p, D[ia],D[ib],D[ic])
            dd=np.dot(cp-p,cp-p)
            if best is None or dd<best[0]: best=(dd,t,bary)
        dd,t,bary=best; ia,ib,ic=tris[t]
        bl018=defaultdict(float)
        for bcoef,vi in zip(bary,(ia,ib,ic)):
            for b,w in dskin[vi].items(): bl018[b]+=bcoef*w
        out.append(normalize_top4(bl018)); dists.append(np.sqrt(dd))
    return out, np.array(dists)

def compare(pred, actual):
    """per-vertex L1 weight error + dominant-bone match, over union of bones."""
    l1=[]; dommatch=0; nboth=0
    for a,b in zip(pred,actual):
        if not b: continue
        nboth+=1
        keys=set(a)|set(b)
        l1.append(sum(abs(a.get(k,0)-b.get(k,0)) for k in keys))
        da=max(a,key=a.get) if a else None
        db=max(b,key=b.get) if b else None
        if da==db: dommatch+=1
    return np.mean(l1), dommatch/nboth, nboth

base='E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/'
head=nifshape.load_shape(base+'BaseFemaleHead_faceBones.nif')

print("=== TEST A: self-transfer (head -> head), expect near-identity ===")
for mode in ('vertex','face'):
    pred,dist=transfer(head,head,mode)
    meanl1,dom,n=compare(pred,head['skin'])
    print(f"  mode={mode:6s}  meanL1={meanl1:.4f}  dominant-bone match={dom*100:.1f}%  maxSnapDist={dist.max():.3g}")

print("\n=== TEST B: head -> hairline (vs hairline's vanilla skin) ===")
hair=nifshape.load_shape(base+'Beards/BigBeard01_Hairline_faceBones.nif')
print(f"  donor head nv={head['nv']} bbox={head['pos'].min(0).round(1)}..{head['pos'].max(0).round(1)}")
print(f"  target hair nv={hair['nv']} bbox={hair['pos'].min(0).round(1)}..{hair['pos'].max(0).round(1)}")
for mode in ('vertex','face'):
    pred,dist=transfer(head,hair,mode)
    meanl1,dom,n=compare(pred,hair['skin'])
    print(f"  mode={mode:6s}  meanL1={meanl1:.4f}  dominant match={dom*100:.1f}%  medianSnap={np.median(dist):.3g} maxSnap={dist.max():.3g}")
