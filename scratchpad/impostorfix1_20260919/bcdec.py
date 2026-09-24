import numpy as np, struct
def _bc1_colour(blk):  # blk: (n,8) uint8 -> (n,4,4,3) float 0..1
    n=blk.shape[0]
    c0=blk[:,0].astype(np.uint16)|(blk[:,1].astype(np.uint16)<<8)
    c1=blk[:,2].astype(np.uint16)|(blk[:,3].astype(np.uint16)<<8)
    def rgb(c):
        r=((c>>11)&31)*255.0/31; g=((c>>5)&63)*255.0/63; b=(c&31)*255.0/31
        return np.stack([r,g,b],-1)
    a=rgb(c0); b=rgb(c1)
    pal=np.zeros((n,4,3))
    pal[:,0]=a; pal[:,1]=b
    big=c0>c1
    pal[:,2]=np.where(big[:,None],(2*a+b)/3,(a+b)/2)
    pal[:,3]=np.where(big[:,None],(a+2*b)/3,0.0)
    bits=(blk[:,4].astype(np.uint32)|(blk[:,5].astype(np.uint32)<<8)|
          (blk[:,6].astype(np.uint32)<<16)|(blk[:,7].astype(np.uint32)<<24))
    idx=np.stack([(bits>>(2*k))&3 for k in range(16)],-1)  # (n,16)
    out=np.take_along_axis(pal,idx[...,None].repeat(3,-1),1)
    return out.reshape(n,4,4,3)/255.0
def _bc3_alpha(blk):   # blk: (n,8) uint8 -> (n,4,4) float
    n=blk.shape[0]
    a0=blk[:,0].astype(np.float64); a1=blk[:,1].astype(np.float64)
    pal=np.zeros((n,8))
    pal[:,0]=a0; pal[:,1]=a1
    big=a0>a1
    for k in range(1,6): pal[:,k+1]=np.where(big,((6-k)*a0+k*a1)/7,0)
    for k in range(1,4): pal[:,k+1]=np.where(big,pal[:,k+1],((4-k)*a0+k*a1)/5)
    pal[:,6]=np.where(big,pal[:,6],0.0); pal[:,7]=np.where(big,pal[:,7],255.0)
    bits=np.zeros(n,np.uint64)
    for i in range(6): bits|=blk[:,2+i].astype(np.uint64)<<np.uint64(8*i)
    idx=np.stack([((bits>>np.uint64(3*k))&np.uint64(7)).astype(np.int64) for k in range(16)],-1)
    out=np.take_along_axis(pal,idx,1)
    return out.reshape(n,4,4)/255.0
def load_dds(path):
    b=open(path,'rb').read()
    assert b[:4]==b'DDS '
    h=struct.unpack('<31I',b[4:128])
    ht,wd=h[2],h[3]; fourcc=b[84:88]
    bw,bh=(wd+3)//4,(ht+3)//4
    if fourcc==b'DXT5':
        d=np.frombuffer(b[128:128+bw*bh*16],np.uint8).reshape(-1,16)
        A=_bc3_alpha(d[:,:8]); C=_bc1_colour(d[:,8:])
        img=np.concatenate([C,A[...,None]],-1)
    elif fourcc==b'DXT1':
        d=np.frombuffer(b[128:128+bw*bh*8],np.uint8).reshape(-1,8)
        C=_bc1_colour(d); img=np.concatenate([C,np.ones(C.shape[:3]+(1,))],-1)
    else: raise RuntimeError('fourcc '+repr(fourcc))
    img=img.reshape(bh,bw,4,4,4).transpose(0,2,1,3,4).reshape(bh*4,bw*4,4)
    return img[:ht,:wd], (wd,ht), fourcc
