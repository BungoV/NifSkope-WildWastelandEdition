import struct,sys
nif=open(sys.argv[1],'rb').read()
p=nif.index(b"\n")+1
ver,endian,user,nb=struct.unpack_from("<IBII",nif,p);p+=13
bsver,=struct.unpack_from("<I",nif,p);p+=4
for _ in range(4): p+=1+nif[p]
nt,=struct.unpack_from("<H",nif,p);p+=2
types=[]
for _ in range(nt):
    n,=struct.unpack_from("<I",nif,p);p+=4;types.append(nif[p:p+n].decode());p+=n
idx=struct.unpack_from("<%dH"%nb,nif,p);p+=2*nb
sizes=struct.unpack_from("<%dI"%nb,nif,p);p+=4*nb
ns,ml=struct.unpack_from("<II",nif,p);p+=8
strs=[]
for _ in range(ns):
    n,=struct.unpack_from("<I",nif,p);p+=4;strs.append(nif[p:p+n].decode(errors='replace'));p+=n
ng,=struct.unpack_from("<I",nif,p);p+=4+4*ng
body=p
for i in range(nb): print(i,types[idx[i]],sizes[i])
print(strs)
