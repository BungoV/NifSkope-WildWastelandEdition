"""Load an FO4 BSSubIndexTriShape: positions, triangles, per-vertex skin (by bone name)."""
import struct
import numpy as np
import nifparse

def half_arr(u16arr):
    u = u16arr.astype(np.uint32)
    s = (u >> 15) & 1
    e = (u >> 10) & 0x1f
    f = u & 0x3ff
    val = np.where(e == 0, (f/1024.0)*2.0**-14,
          np.where(e == 31, np.inf, (1.0+f/1024.0)*2.0**(e.astype(np.int32)-15)))
    return np.where(s == 1, -val, val).astype(np.float64)

def load_shape(path, shape_type='BSSubIndexTriShape'):
    data,hdr,strings,blocks = nifparse.parse(path)
    sits = next(b for b in blocks if b[1]==shape_type)
    o = sits[2]
    def U32():
        nonlocal o; v=struct.unpack_from('<I',data,o)[0]; o+=4; return v
    def U16():
        nonlocal o; v=struct.unpack_from('<H',data,o)[0]; o+=2; return v
    def F():
        nonlocal o; v=struct.unpack_from('<f',data,o)[0]; o+=4; return v
    name=U32(); ne=U32(); [U32() for _ in range(ne)]
    U32(); U32()  # controller, flags
    trans=np.array([F(),F(),F()])
    rot=np.array([F() for _ in range(9)]).reshape(3,3)
    scale=F()
    U32()  # collision
    o+=16  # bounding sphere
    U32();U32();U32()  # skin, shader, alpha
    vdesc=struct.unpack_from('<Q',data,o)[0]; o+=8
    ntri=U32(); nv=U16(); dsize=U32()
    stride=(vdesc & 0xF)*4
    skinoff=((vdesc>>28)&0xF)*4
    fullprec=bool((vdesc>>44)&0x400)
    vstart=o
    # bone names
    inst=next(b for b in blocks if b[1]=='BSSkin::Instance'); io=inst[2]
    nb=struct.unpack_from('<I',data,io+8)[0]
    names=[strings[struct.unpack_from('<I',data,blocks[struct.unpack_from('<I',data,io+12+4*k)[0]][2])[0]] for k in range(nb)]

    # vertex block as bytes -> numpy
    vbytes=np.frombuffer(data[vstart:vstart+nv*stride], dtype=np.uint8).reshape(nv,stride)
    # positions at offset 0
    if fullprec:
        pos=vbytes[:,0:12].copy().view(np.float32).astype(np.float64)
    else:
        pos=half_arr(vbytes[:,0:6].copy().view(np.uint16))
    # apply shape transform -> common (node) space
    pos = trans + scale*(pos @ rot.T)
    # skin: weights (4 half at skinoff) + indices (4 byte at skinoff+8)
    wbits=vbytes[:,skinoff:skinoff+8].copy().view(np.uint16)   # (nv,4)
    weights=half_arr(wbits)
    idx=vbytes[:,skinoff+8:skinoff+12].astype(np.int32)        # (nv,4)
    # triangles
    tstart=vstart+nv*stride
    tris=np.frombuffer(data[tstart:tstart+ntri*6], dtype=np.uint16).reshape(ntri,3).astype(np.int64)

    # per-vertex skin as list of dict{boneName:weight}
    skin=[]
    for i in range(nv):
        d={}
        for k in range(4):
            w=weights[i,k]
            if w>0.0005:
                bn=names[idx[i,k]] if idx[i,k]<len(names) else f'#{idx[i,k]}'
                d[bn]=d.get(bn,0.0)+w
        skin.append(d)
    return dict(path=path, nv=nv, pos=pos, tris=tris, skin=skin, names=names,
               stride=stride, skinoff=skinoff, fullprec=fullprec, ntri=ntri)
