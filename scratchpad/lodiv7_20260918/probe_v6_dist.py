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
    ao=b[offInst+24*i+0x10]
    lo,hi=first[i],first[i+1]
    if hi==lo: continue
    sl=data[lo:hi]; m=sum(sl)/float(len(sl))
    rows.append((len(sl),abs(m-ao)))
d=sorted(r[1] for r in rows); t=len(d)
def pc(q): return d[min(t-1,int(q*t))]
print('n=%d  median %.2f  p90 %.2f  p95 %.2f  p99 %.2f  max %.2f'%(t,pc(.5),pc(.9),pc(.95),pc(.99),d[-1]))
for bar in (2,4,8,16,32):
    print('  within %-3d : %5d of %d = %.2f%%'%(bar,sum(1 for x in d if x<=bar),t,100.0*sum(1 for x in d if x<=bar)/t))
for lo,hi,lbl in ((0,5,'<=4 verts (flat cards)'),(5,10**9,'>4 verts')):
    g=[r[1] for r in rows if lo<=r[0]<hi]
    if g: print('  %-24s n=%5d  within2 %.2f%%  mean|diff| %.2f  max %.2f'%(lbl,len(g),100.0*sum(1 for x in g if x<=2)/len(g),sum(g)/len(g),max(g)))
