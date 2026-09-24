# The spec's whole card draw, written in numpy from docs/LODGEN_IMPOSTOR_SPEC.md
# and res/shaders/impostor_oct.frag, independent of the C++/GLSL binaries.
import numpy as np, json, glob, math
from PIL import Image
USE_DDS=False
FX='E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/'

class CardSet:
    def __init__(self, tag):
        d=FX+tag+'/cards/'
        raw=open(glob.glob(d+'*_oct.lodm')[0],'rb').read()
        j=json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
        self.N=j['oct']; self.half=np.array(j['half'],float); self.center=np.array(j['center'],float)
        self.span=float(j['depthSpan'])
        c=j.get('coverage',{}); self.covFloor=c.get('floor',0)/255.0; self.covBase=c.get('base',0)/255.0
        fo=j.get('frameOffset',None)
        self.foff=np.array(fo,float).reshape(self.N*self.N,2) if fo else np.zeros((self.N*self.N,2))
        if USE_DDS:
            import bcdec
            self.alb=bcdec.load_dds(glob.glob(d+'*_oct_d.DDS')[0])[0]
            self.nrm=bcdec.load_dds(glob.glob(d+'*_oct_n.DDS')[0])[0]
        else:
            self.alb=np.asarray(Image.open(glob.glob(d+'*_oct_albedo.png')[0]).convert('RGBA')).astype(np.float64)/255.0
            self.nrm=np.asarray(Image.open(glob.glob(d+'*_oct_normal.png')[0]).convert('RGBA')).astype(np.float64)/255.0
        self.H,self.W=self.alb.shape[:2]
        self.fw=self.W//self.N; self.fh=self.H//self.N

def normalise(v):
    n=np.linalg.norm(v); return v/n
def frameDir(i,j,N):
    u=i/(N-1)*2-1; v=j/(N-1)*2-1
    d=np.array([(u+v)/2,(u-v)/2,0.0]); d[2]=1-abs(d[0])-abs(d[1]); return normalise(d)
def frameBasis(d):
    d=normalise(np.asarray(d,float))
    elev=math.asin(max(-1,min(1,d[2]))); azim=math.atan2(d[1],d[0])
    rx=math.radians(-90+math.degrees(elev)); rz=math.radians(270-math.degrees(azim))
    sX,cX=math.sin(rx),math.cos(rx); sZ,cZ=math.sin(rz),math.cos(rz)
    right=np.array([cZ,-sZ,0.0]); up=np.array([sZ*cX,cX*cZ,-sX]); fwd=np.array([sX*sZ,sX*cZ,cX])
    return normalise(right),normalise(up),normalise(fwd)
def dirToGrid(d,N):
    e=np.array(d,float); e=normalise(e)
    if e[2]<0: e[2]=0; e=normalise(e)
    L=abs(e[0])+abs(e[1])+e[2]; x,y=e[0]/L,e[1]/L
    u,v=x+y,x-y
    a=(u+1)*0.5*(N-1); b=(v+1)*0.5*(N-1)
    return min(max(a,0),N-1), min(max(b,0),N-1)
def pickFrames(d,N):
    fi,fj=dirToGrid(d,N)
    i0=min(max(int(math.floor(fi)),0),N-2); j0=min(max(int(math.floor(fj)),0),N-2)
    a=fi-i0; b=fj-j0
    if a+b<=1.0: F=[(i0,j0,1-a-b),(i0+1,j0,a),(i0,j0+1,b)]
    else:        F=[(i0+1,j0+1,a+b-1),(i0,j0+1,1-a),(i0+1,j0,1-b)]
    return [(i,j,max(w,0.0)) for i,j,w in F]

def bilinear(img, u, v):
    """u,v in 0..1 over the whole sheet; returns (...,C)."""
    H,W=img.shape[:2]
    x=np.clip(u*W-0.5,0,W-1); y=np.clip(v*H-0.5,0,H-1)
    x0=np.floor(x).astype(int); y0=np.floor(y).astype(int)
    x1=np.minimum(x0+1,W-1); y1=np.minimum(y0+1,H-1)
    fx=(x-x0)[...,None]; fy=(y-y0)[...,None]
    return (img[y0,x0]*(1-fx)*(1-fy)+img[y0,x1]*fx*(1-fy)+img[y1,x0]*(1-fx)*fy+img[y1,x1]*fx*fy)

def coverageOf(a,cs):
    if cs.covBase<=0: return np.where(a<16/255.0,0.0,a)
    out=np.clip(cs.covFloor+(a-cs.covBase)*(1-cs.covFloor)/(1-cs.covBase),cs.covFloor,1.0)
    return np.where(a<cs.covBase,0.0,out)

def render(cs, d, res=(192,512), *, parallax=True, nframes=3, useOffset=True,
           decode=True, clampStep=None, covGate=False, thresh=None, covPre=False):
    """Returns (ink mask, alpha) over the card quad, x right, y up."""
    W,H=res
    xs=(np.arange(W)+0.5)/W*2-1; ys=1-(np.arange(H)+0.5)/H*2
    X,Y=np.meshgrid(xs*cs.half[0], ys*cs.half[1])
    rC,uC,fC=frameBasis(d)
    P=X[...,None]*rC+Y[...,None]*uC                      # r = p - center
    ray=-fC                                              # away from the camera
    frames=sorted(pickFrames(d,cs.N),key=lambda t:-t[2])[:nframes]
    if nframes==1: frames=[(frames[0][0],frames[0][1],1.0)]
    alpha=np.zeros((H,W)); wsum=np.zeros((H,W))
    thr = cs.covFloor if thresh is None else thresh
    for (i,j,w) in frames:
        if w<=0: continue
        rk,uk,fk=frameBasis(frameDir(i,j,cs.N))
        off=cs.foff[j*cs.N+i] if useOffset else np.zeros(2)
        def uvof(Q):
            st=np.stack([Q@rk-off[0], Q@uk-off[1]],-1)
            dd=Q@fk
            uv=np.stack([st[...,0]/(2*cs.half[0])+0.5, 0.5-st[...,1]/(2*cs.half[1])],-1)
            uv=np.clip(uv,0,1)
            return (np.stack([(i+uv[...,0])/cs.N,(j+uv[...,1])/cs.N],-1), dd)
        uv,dd=uvof(P)
        uv0=uv
        Q=P
        if parallax:
            h=bilinear(cs.nrm,uv[...,0],uv[...,1])[...,2]
            want=-(h-0.5)*cs.span
            denom=float(np.dot(ray,fk))
            if abs(denom)>0.15:
                t=(want-dd)/denom
                if covGate:
                    a0=bilinear(cs.alb,uv[...,0],uv[...,1])[...,3]
                    t=np.where(a0>=(0.98 if cs.covBase<=0 else 250/255.0),t,0.0)
                if clampStep is not None:
                    lim=clampStep
                    t=np.clip(t,-lim,lim)
                Q=P+ray[None,None,:]*t[...,None]
                uv,dd=uvof(Q)
        a=bilinear(cs.alb,uv[...,0],uv[...,1])[...,3]
        if covPre:
            a=bilinear(cs.alb,uv0[...,0],uv0[...,1])[...,3]
        cov=coverageOf(a,cs) if decode else a
        alpha+=cov*w; wsum+=w
    return alpha>=thr, alpha

def truth(cs12, d, res, half, ):
    """A near-photograph of the mesh from d: the nearest N=12 frame, placed by its
       own frameOffset/half, rasterised on the same world grid as the card quad."""
    W,H=res
    fi,fj=dirToGrid(d,cs12.N); i=int(round(fi)); j=int(round(fj))
    off=cs12.foff[j*cs12.N+i]
    xs=(np.arange(W)+0.5)/W*2-1; ys=1-(np.arange(H)+0.5)/H*2
    X,Y=np.meshgrid(xs*half[0], ys*half[1])
    st=np.stack([X-off[0],Y-off[1]],-1)
    uv=np.stack([st[...,0]/(2*cs12.half[0])+0.5, 0.5-st[...,1]/(2*cs12.half[1])],-1)
    inside=(uv[...,0]>=0)&(uv[...,0]<=1)&(uv[...,1]>=0)&(uv[...,1]<=1)
    uv=np.clip(uv,0,1)
    U=np.stack([(i+uv[...,0])/cs12.N,(j+uv[...,1])/cs12.N],-1)
    a=bilinear(cs12.alb,U[...,0],U[...,1])[...,3]
    return (coverageOf(a,cs12)>=cs12.covFloor)&inside, (i,j)

def iou(a,b):
    u=(a|b).sum(); return (a&b).sum()/u if u else 0.0
