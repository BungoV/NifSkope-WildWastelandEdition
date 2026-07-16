import struct, sys
import nifparse
sys.argv=['x',sys.argv[1]]
data,hdr,strings,blocks=nifparse.parse(sys.argv[1])

def half(u):
    e=(u>>10)&0x1f; f=u&0x3ff
    v=(f/1024.0)*2**-14 if e==0 else ((1+f/1024.0)*2**(e-15) if e!=31 else float('inf'))
    return -v if (u>>15)&1 else v

# ---- locate SITS and parse fully to vertex data ----
sits=next(b for b in blocks if b[1]=='BSSubIndexTriShape')
o=sits[2]
def U32():
    global o; v=struct.unpack_from('<I',data,o)[0]; o+=4; return v
def U16():
    global o; v=struct.unpack_from('<H',data,o)[0]; o+=2; return v
def U64():
    global o; v=struct.unpack_from('<Q',data,o)[0]; o+=8; return v
name=U32(); ne=U32(); refs=[U32() for _ in range(ne)]
ctrl=U32(); flags=U32(); o+=12+36+4; col=U32(); o+=16
skin=U32(); shader=U32(); alpha=U32()
vdesc=U64(); ntri=U32(); nv=U16(); dsize=U32()
vstart=o
stride=(vdesc & 0xF)*4
skinoff=((vdesc>>28)&0xF)*4
print(f"numVerts={nv} stride={stride} skinOffset={skinoff} vdataStart={vstart}")

# ---- resolve bone names via BSSkin::Instance bone refs -> NiNode names ----
inst=next(b for b in blocks if b[1]=='BSSkin::Instance')
io=inst[2]
sroot=struct.unpack_from('<I',data,io)[0]
bdref=struct.unpack_from('<I',data,io+4)[0]
nbones=struct.unpack_from('<I',data,io+8)[0]
bonerefs=[struct.unpack_from('<I',data,io+12+4*k)[0] for k in range(nbones)]
def nodename(blkidx):
    b=blocks[blkidx]
    if b[1] not in ('NiNode','BSFadeNode'): return f"<{b[1]}#{blkidx}>"
    ni=struct.unpack_from('<I',data,b[2])[0]
    return strings[ni] if ni<len(strings) else f"#{ni}"
bonenames=[nodename(r) for r in bonerefs]
print(f"skin bones={nbones}; index0={bonenames[0]!r} index2={bonenames[2]!r} index7={bonenames[7]!r}")

# ---- CustomizationRemapData ----
rd=next(b for b in blocks if b[1]=='NiBinaryExtraData' and strings[struct.unpack_from('<I',data,b[2])[0]]=='CustomizationRemapData')
ps=struct.unpack_from('<I',data,rd[2]+4)[0]
remap=data[rd[2]+8:rd[2]+8+ps]

# ---- compare per-vertex in-mesh skin vs remap ----
identical=0; diffw=0; diffidx=0; examples=[]
for i in range(nv):
    vb=vstart+i*stride+skinoff
    vw=[half(struct.unpack_from('<H',data,vb+2*k)[0]) for k in range(4)]
    vidx=list(data[vb+8:vb+12])
    rb=i*12
    rw=[half(struct.unpack_from('<H',remap,rb+2*k)[0]) for k in range(4)]
    ridx=list(remap[rb+8:rb+12])
    same_i = vidx==ridx
    same_w = all(abs(a-b)<0.002 for a,b in zip(vw,rw))
    if same_i and same_w: identical+=1
    else:
        if not same_w: diffw+=1
        if not same_i: diffidx+=1
        if len(examples)<8:
            examples.append((i,vw,vidx,rw,ridx))
print(f"\nvertices identical(skin==remap): {identical}/{nv}")
print(f"  weight-differs: {diffw}  index-differs: {diffidx}")
for i,vw,vidx,rw,ridx in examples:
    print(f"  v{i}: MESHskin w={[round(x,3) for x in vw]} idx={vidx}")
    print(f"        REMAP   w={[round(x,3) for x in rw]} idx={ridx}")
