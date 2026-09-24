"""BC3 encode exactly as src/lodgen.cpp does it (lodgenWriteDds mip 0 +
lodgenEncodeBC1Block with allowPunch = !bc3 = false), so a channel-assignment
question can be answered through a REAL encode/decode round trip."""
import numpy as np

def _bc3_alpha_rt(a):
    """a: (H,W) uint8 -> the alpha the GPU decodes back."""
    H,W=a.shape; bh,bw=(H+3)//4,(W+3)//4
    pad=np.zeros((bh*4,bw*4),np.int32)
    for y in range(bh*4):
        for x in range(bw*4):
            pass
    ys=np.minimum(np.arange(bh*4),H-1); xs=np.minimum(np.arange(bw*4),W-1)
    pad=a[np.ix_(ys,xs)].astype(np.int32)
    blk=pad.reshape(bh,4,bw,4).transpose(0,2,1,3).reshape(-1,16)
    aMax=blk.max(1); aMin=blk.min(1)
    pal=np.zeros((blk.shape[0],8),np.int32)
    pal[:,0]=aMax; pal[:,1]=aMin
    for k in range(1,7):
        pal[:,k+1]=((7-k)*aMax+k*aMin)//7            # integer division, as the C++ does
    d=np.abs(blk[:,:,None]-pal[:,None,:])
    idx=d.argmin(2)
    out=np.take_along_axis(pal,idx,1)
    out=out.reshape(bh,bw,4,4).transpose(0,2,1,3).reshape(bh*4,bw*4)
    return out[:H,:W].astype(np.uint8)

def _pack565(r,g,b):
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)

def _bc1_rt(rgb):
    """rgb: (H,W,3) uint8, 4-colour mode only (allowPunch false). Returns the
       decoded rgb. Endpoints = the block's min/max LUMINANCE texels, as the
       C++ picks them."""
    H,W,_=rgb.shape; bh,bw=(H+3)//4,(W+3)//4
    ys=np.minimum(np.arange(bh*4),H-1); xs=np.minimum(np.arange(bw*4),W-1)
    pad=rgb[np.ix_(ys,xs)].astype(np.int32)
    blk=pad.reshape(bh,4,bw,4,3).transpose(0,2,1,3,4).reshape(-1,16,3)
    L=0.299*blk[...,0]+0.587*blk[...,1]+0.114*blk[...,2]
    iLo=L.argmin(1); iHi=L.argmax(1)
    n=blk.shape[0]; ar=np.arange(n)
    cHi=blk[ar,iHi]; cLo=blk[ar,iLo]
    c0=_pack565(cHi[:,0],cHi[:,1],cHi[:,2]); c1=_pack565(cLo[:,0],cLo[:,1],cLo[:,2])
    sw=c0<c1
    t0=np.where(sw,c1,c0); t1=np.where(sw,c0,c1)
    def unpack(c):
        return np.stack([((c>>11)&31)*255.0/31,((c>>5)&63)*255.0/63,(c&31)*255.0/31],-1)
    p0=unpack(t0); p1=unpack(t1)
    eq=(t0==t1)
    pal=np.stack([p0,p1,(2*p0+p1)/3,(p0+2*p1)/3],1)          # (n,4,3)
    d=((blk[:,:,None,:]-pal[:,None,:,:])**2).sum(-1)
    idx=d.argmin(2)
    idx=np.where(eq[:,None],0,idx)
    out=np.take_along_axis(pal,idx[...,None].repeat(3,-1),1)
    out=out.reshape(bh,bw,4,4,3).transpose(0,2,1,3,4).reshape(bh*4,bw*4,3)
    return np.clip(out[:H,:W],0,255)

def bc3_roundtrip(rgba):
    """rgba: (H,W,4) float 0..1 -> the values a GPU sampler returns, float 0..1."""
    q=np.clip(np.round(rgba*255),0,255).astype(np.uint8)
    a=_bc3_alpha_rt(q[...,3]).astype(np.float64)/255.0
    c=_bc1_rt(q[...,:3])/255.0
    return np.concatenate([c,a[...,None]],-1)
