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
    # D3D10 BC4/BC3 alpha ramps. BOTH were one step short until 2026-09-19
    # (lane IMPOSTORFIX3): the eight-level branch ran k=1..5 with (6-k) and
    # never assigned index 7; the six-level branch ran k=1..3 with (4-k) and
    # never assigned index 5. Both read LOW, so the gate that selects covered
    # texels by this alpha silently skipped the texels it was meant to test.
    for k in range(1,7): pal[:,k+1]=np.where(big,((7-k)*a0+k*a1)/7,0)
    for k in range(1,5): pal[:,k+1]=np.where(big,pal[:,k+1],((5-k)*a0+k*a1)/5)
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
    if fourcc==b'DX10':
        # Lane IMPOSTORDEPTH2 (2026-09-23): the card `_n` is DX10 BC7_UNORM (dxgi 98).
        # BC7 is decoded by Pillow, a decoder independent of the tree's encoder; a
        # DX10 BC3 / BC1 (dxgi 77 / 71, the arrays' layer 0) reads through the paths below.
        dxgi=struct.unpack_from('<I',b,128)[0]
        if dxgi==98:
            from PIL import Image
            img=np.asarray(Image.open(path).convert('RGBA')).astype(np.float64)/255.0
            return img[:ht,:wd], (wd,ht), b'BC7 '
        if dxgi not in (77,71): raise RuntimeError('DX10 dxgi %d' % dxgi)
        fourcc=b'DXT5' if dxgi==77 else b'DXT1'
        b=b[:128]+b[148:]
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


def known_answer():
    """A hand-built BC3 alpha block carrying every index 0..7, decoded against
    the D3D10 ramp written out by hand. Returns (rows, worst) where rows is
    [(index, got, want)] -- the gate prints them and fails on any disagreement.

    The block: a0 = 255, a1 = 0 (a0 > a1, so the EIGHT-level ramp), then six
    bytes of 3-bit indices, texel i taking index i & 7, so all eight appear
    twice. 16 bytes total, the same shape a .DDS carries."""
    blk = bytearray(8)
    blk[0] = 255; blk[1] = 0
    bits = 0
    for i in range(15, -1, -1):
        bits = (bits << 3) | (i & 7)
    for i in range(6):
        blk[2+i] = (bits >> (8*i)) & 0xFF
    a0, a1 = 255.0, 0.0
    want = [a0, a1] + [((7-k)*a0 + k*a1)/7 for k in range(1, 7)]
    got = _bc3_alpha(np.frombuffer(bytes(blk), np.uint8).reshape(1, 8))[0].reshape(16)*255.0
    rows, worst = [], 0.0
    for i in range(16):
        k = i & 7
        rows.append((k, float(got[i]), want[k]))
        worst = max(worst, abs(float(got[i]) - want[k]))
    return rows, worst


def known_answer_six():
    """The same for the SIX-level ramp. a0 = 40, a1 = 200 -- NOT a0 = 0,
    because with a0 = 0 the wrong coefficient (4-k) and the right one (5-k)
    multiply zero and agree, and the row would only catch the missing index 5.
    Indices 6 and 7 are the hard 0 and 255 the format defines."""
    blk = bytearray(8)
    blk[0] = 40; blk[1] = 200
    bits = 0
    for i in range(15, -1, -1):
        bits = (bits << 3) | (i & 7)
    for i in range(6):
        blk[2+i] = (bits >> (8*i)) & 0xFF
    a0, a1 = 40.0, 200.0
    want = [a0, a1] + [((5-k)*a0 + k*a1)/5 for k in range(1, 5)] + [0.0, 255.0]
    got = _bc3_alpha(np.frombuffer(bytes(blk), np.uint8).reshape(1, 8))[0].reshape(16)*255.0
    rows, worst = [], 0.0
    for i in range(16):
        k = i & 7
        rows.append((k, float(got[i]), want[k]))
        worst = max(worst, abs(float(got[i]) - want[k]))
    return rows, worst


if __name__ == '__main__':
    nbad = 0
    for name, fn in (('eight-level (a0 > a1)', known_answer), ('six-level (a0 <= a1)', known_answer_six)):
        rows, worst = fn()
        seen = {}
        for k, g, w in rows:
            seen[k] = (g, w)
        print('%s: worst error %.2f levels of 255' % (name, worst))
        for k in sorted(seen):
            g, w = seen[k]
            flag = '' if abs(g - w) <= 0.51 else '   <-- WRONG'
            print('   index %d  decoded %7.2f  D3D %7.2f%s' % (k, g, w, flag))
            if abs(g - w) > 0.51:
                nbad += 1
    print('known-answer: %d of 16 ramp entries wrong' % nbad)
    raise SystemExit(1 if nbad else 0)
