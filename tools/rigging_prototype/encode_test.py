import struct
import nifparse

def load_skin_raw(path):
    """Return per-vertex list of 4 (rawHalfBits, boneName_or_None) preserving slot order+bits."""
    data,hdr,strings,blocks=nifparse.parse(path)
    sits=next(b for b in blocks if b[1]=='BSSubIndexTriShape')
    o=sits[2]
    def U32():
        nonlocal o; v=struct.unpack_from('<I',data,o)[0]; o+=4; return v
    def U16():
        nonlocal o; v=struct.unpack_from('<H',data,o)[0]; o+=2; return v
    U32(); ne=U32(); [U32() for _ in range(ne)]; U32();U32(); o+=12+36+4; U32(); o+=16; U32();U32();U32()
    vdesc=struct.unpack_from('<Q',data,o)[0]; o+=8; ntri=U32(); nv=U16(); ds=U32()
    stride=(vdesc&0xF)*4; skinoff=((vdesc>>28)&0xF)*4; vstart=o
    inst=next(b for b in blocks if b[1]=='BSSkin::Instance'); io=inst[2]; nb=struct.unpack_from('<I',data,io+8)[0]
    names=[strings[struct.unpack_from('<I',data,blocks[struct.unpack_from('<I',data,io+12+4*k)[0]][2])[0]] for k in range(nb)]
    verts=[]
    for i in range(nv):
        vb=vstart+i*stride+skinoff
        slots=[]
        for k in range(4):
            hb=struct.unpack_from('<H',data,vb+2*k)[0]
            bi=data[vb+8+k]
            slots.append((hb,bi))
        verts.append((slots,names))
    return verts,names,nv

def load_remap_blob(path):
    data,hdr,strings,blocks=nifparse.parse(path)
    fbnames=None
    inst=next(b for b in blocks if b[1]=='BSSkin::Instance'); io=inst[2]; nb=struct.unpack_from('<I',data,io+8)[0]
    fbnames=[strings[struct.unpack_from('<I',data,blocks[struct.unpack_from('<I',data,io+12+4*k)[0]][2])[0]] for k in range(nb)]
    rd=next(b for b in blocks if b[1]=='NiBinaryExtraData' and strings[struct.unpack_from('<I',data,b[2])[0]]=='CustomizationRemapData')
    ps=struct.unpack_from('<I',data,rd[2]+4)[0]
    return data[rd[2]+8:rd[2]+8+ps], fbnames

base='E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/'
verts,nnames,nv = load_skin_raw(base+'BaseFemaleHead.nif')
vanilla, fbnames = load_remap_blob(base+'BaseFemaleHead_faceBones.nif')

# build name -> faceBones-remap index. faceBones skin list [0..67]; appended new bones start at 68.
name2idx={nm:i for i,nm in enumerate(fbnames)}
appended=[]
def remap_index(nm):
    if nm in name2idx: return name2idx[nm]
    if nm not in appended: appended.append(nm)
    return len(fbnames)+appended.index(nm)

# encode: keep normal-head slot order + exact half bits; replace bone index via name mapping
out=bytearray()
for i in range(nv):
    slots,names=verts[i]
    rec=bytearray(12)
    idxbytes=bytearray(4)
    for k,(hb,bi) in enumerate(slots):
        struct.pack_into('<H',rec,k*2,hb)
        w=hb & 0x7fff  # magnitude nonzero?
        nm=names[bi]
        idxbytes[k]= remap_index(nm) if (hb!=0) else 0
    rec[8:12]=idxbytes
    out+=rec

print("appended new bones (in encounter order):",appended)
print(f"encoded {len(out)}B vanilla {len(vanilla)}B  equal={bytes(out)==vanilla}")
if bytes(out)!=vanilla:
    # count differing records + show first few
    diff=[i for i in range(nv) if out[i*12:i*12+12]!=vanilla[i*12:i*12+12]]
    print(f"differing records: {len(diff)}/{nv}")
    for i in diff[:6]:
        print(f"  v{i} enc={bytes(out[i*12:i*12+12]).hex(' ')}")
        print(f"       van={vanilla[i*12:i*12+12].hex(' ')}")
