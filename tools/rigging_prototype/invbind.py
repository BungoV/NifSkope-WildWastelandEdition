import struct, sys
import numpy as np
import nifparse

path=sys.argv[1]
data,hdr,strings,blocks=nifparse.parse(path)

def node_local(blkidx):
    """parse NiNode/shape local transform (trans,rot,scale) + children refs."""
    b=blocks[blkidx]; o=b[2]
    name=struct.unpack_from('<I',data,o)[0]; o+=4
    ne=struct.unpack_from('<I',data,o)[0]; o+=4; o+=4*ne
    o+=4  # controller
    o+=4  # flags
    trans=np.array(struct.unpack_from('<3f',data,o)); o+=12
    rot=np.array(struct.unpack_from('<9f',data,o)).reshape(3,3); o+=36
    scale=struct.unpack_from('<f',data,o)[0]; o+=4
    o+=4  # collision object
    children=[]
    if b[1] in ('NiNode','BSFadeNode'):
        nc=struct.unpack_from('<I',data,o)[0]; o+=4
        children=list(struct.unpack_from('<%dI'%nc,data,o)) if nc else []
    return name,trans,rot,scale,children

def mat4(trans,rot,scale):
    M=np.eye(4); M[:3,:3]=rot*scale; M[:3,3]=trans; return M

# build parent map + locals
loc={}; children_of={}
for i,tname,start,size in blocks:
    if tname in ('NiNode','BSFadeNode','BSSubIndexTriShape','BSTriShape'):
        nm,trans,rot,scale,ch=node_local(i)
        loc[i]=(trans,rot,scale); children_of[i]=ch
parent={}
for p,ch in children_of.items():
    for c in ch: parent[c]=p

def world(blkidx, transpose_rot=False):
    chain=[]
    x=blkidx
    while x is not None:
        chain.append(x); x=parent.get(x)
    M=np.eye(4)
    for x in reversed(chain):
        t,r,s=loc[x]
        rr=r.T if transpose_rot else r
        M=M@mat4(t,rr,s)
    return M

# mesh node
sits=next(b for b in blocks if b[1]=='BSSubIndexTriShape')[0]
# bone refs
inst=next(b for b in blocks if b[1]=='BSSkin::Instance'); io=inst[2]
nb=struct.unpack_from('<I',data,io+8)[0]
bonerefs=[struct.unpack_from('<I',data,io+12+4*k)[0] for k in range(nb)]
# stored BoneData transforms
bd=next(b for b in blocks if b[1]=='BSSkin::BoneData'); bo=bd[2]+4
stored=[]
for k in range(nb):
    v=struct.unpack_from('<17f',data,bo+k*68)
    R=np.array(v[4:13]).reshape(3,3); T=np.array(v[13:16]); S=v[16]
    stored.append(mat4(T,R,S))

for transpose_rot in (False,True):
    for invert_side in ('inv(bone)*mesh','mesh_is_identity_inv(bone)'):
        errs=[]
        meshW=world(sits,transpose_rot)
        for k in range(nb):
            bw=world(bonerefs[k],transpose_rot)
            if invert_side=='inv(bone)*mesh':
                comp=np.linalg.inv(bw)@meshW
            else:
                comp=np.linalg.inv(bw)
            errs.append(np.abs(comp-stored[k]).max())
        print(f"transpose_rot={transpose_rot!s:5s} {invert_side:26s} maxerr={max(errs):.4g} meanerr={np.mean(errs):.4g}")
