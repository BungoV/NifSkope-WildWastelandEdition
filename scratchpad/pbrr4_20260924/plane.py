import struct,numpy as np
nif=open(r"E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Preview\PreviewPlane01.nif",'rb').read()
# find BSTriShape body offset: reuse nifinfo parse
exec(open('nifinfo.py').read().split("for i in range(nb)")[0].replace("sys.argv[1]","r'E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Preview\PreviewPlane01.nif'"))
off=body+sizes[0]
b=nif[off:off+sizes[1]]
tr=struct.unpack_from("<3f",b,16); rot=struct.unpack_from("<9f",b,28); sc=struct.unpack_from("<f",b,64)
print("T",tr,"R",rot,"S",sc)
vd,=struct.unpack_from("<Q",b,100); ntri,=struct.unpack_from("<I",b,108); nv,=struct.unpack_from("<H",b,112); ds,=struct.unpack_from("<I",b,114)
print(hex(vd),ntri,nv,ds)
st=20; p=118
for i in range(nv):
    v=b[p+i*st:p+(i+1)*st]
    pos=np.frombuffer(v[0:8],dtype=np.float16); uv=np.frombuffer(v[8:12],dtype=np.float16)
    print(pos,uv,list(v[12:16]))
print(struct.unpack_from("<6H",b,p+nv*st))
