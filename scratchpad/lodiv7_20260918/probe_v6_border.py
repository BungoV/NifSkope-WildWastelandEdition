import struct, sys
def le(f,b,o): return struct.unpack_from('<'+f,b,o)
b=open(sys.argv[1],'rb').read()
n=le('I',b,0x58)[0]; cc=le('I',b,0x54)[0]
west,south,east,north=le('hhhh',b,0x48)
offChunks=le('Q',b,0x68)[0]; offInst=le('Q',b,0x78)[0]
offVao=le('Q',b,0xF4)[0]; vaoBytes=le('I',b,0xFC)[0]
w=east-west+1
first=list(le('%dI'%(n+1),b,offVao)); data=b[offVao+4*(n+1):offVao+vaoBytes]
ic=[None]*n
for ci in range(cc):
    iF,iC=le('II',b,offChunks+32*ci)
    cx=west+(ci%w); cy=north-(ci//w)
    for i in range(iF,iF+iC): ic[i]=(cx,cy)
sel=(int(sys.argv[2]),int(sys.argv[3]))
rows=[]
for i in range(n):
    if ic[i]!=sel: continue
    px,py,pz=le('HHH',b,offInst+24*i)
    ao=b[offInst+24*i+0x10]
    lo,hi=first[i],first[i+1]
    if hi==lo: continue
    sl=data[lo:hi]; m=sum(sl)/float(len(sl))
    # fraction of the chunk box, 0..1; edge distance in u16 units (16384 u across)
    edge=min(px,65535-px,py,65535-py)/65535.0*16384.0
    rows.append((edge,abs(m-ao),len(sl)))
bad=[r for r in rows if r[1]>2]; good=[r for r in rows if r[1]<=2]
def sm(v): return sum(v)/len(v)
print('outliers (|diff|>2): n=%d  mean edge-dist %.0f u  median %.0f'%(len(bad),sm([r[0] for r in bad]),sorted(r[0] for r in bad)[len(bad)//2]))
print('inliers  (|diff|<=2): n=%d  mean edge-dist %.0f u  median %.0f'%(len(good),sm([r[0] for r in good]),sorted(r[0] for r in good)[len(good)//2]))
for band,lbl in (((0,512),'0-512 u of a chunk edge'),((512,2048),'512-2048 u'),((2048,99999),'>2048 u (interior)')):
    g=[r for r in rows if band[0]<=r[0]<band[1]]
    if g: print('  %-26s n=%5d  within2 %.2f%%'%(lbl,len(g),100.0*sum(1 for r in g if r[1]<=2)/len(g)))
