import struct,sys
for f in ["PreviewPlane01","PreviewSphere01"]:
    src=open('nifinfo.py').read().split("for i in range(nb)")[0].replace("sys.argv[1]","r'E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Preview\%s.nif'"%f)
    g={};exec(src,g)
    off=g['body']+sum(g['sizes'][:4]); b=g['nif'][off:off+15]
    name,nex,ctrl,flags,th=struct.unpack_from("<iIiHB",b,0)
    print(f,hex(flags),th)
