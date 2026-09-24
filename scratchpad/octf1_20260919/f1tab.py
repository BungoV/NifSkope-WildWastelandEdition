import math
exec(open('f1sim.py').read().split('SS = 12')[0])
SS = 12
def measure(cov_thr, dilate=0):
    out = {}
    for j in range(n):
        for i in range(n):
            r, up = axes(i, j, n)
            hr = half*(abs(r[0])+abs(r[1])+abs(r[2])); hu = half*(abs(up[0])+abs(up[1])+abs(up[2]))
            px = 2.0*hr*tw/(2.0*fw); py = 2.0*hu*th/(2.0*fh)
            pts=[]
            for sx in (-half,half):
                for sy in (-half,half):
                    for sz in (-half,half):
                        pts.append((sx*r[0]+sy*r[1]+sz*r[2], sx*up[0]+sy*up[1]+sz*up[2]))
            P=hull(pts); txs=2.0*fw/tw; tys=2.0*fh/th
            m=[[0]*tw for _ in range(th)]
            for yi in range(th):
                y0=-fh+yi*tys
                for xi in range(tw):
                    x0=-fw+xi*txs; c=0
                    for a in range(SS):
                        for b in range(SS):
                            if inside(P,x0+(a+0.5)/SS*txs,y0+(b+0.5)/SS*tys): c+=1
                    m[yi][xi]=1 if c/(SS*SS)>=cov_thr else 0
            for _ in range(dilate):
                m2=[[0]*tw for _ in range(th)]
                for y in range(th):
                    for x in range(tw):
                        if any(m[y+dy][x+dx] for dy in(-1,0,1) for dx in(-1,0,1) if 0<=y+dy<th and 0<=x+dx<tw): m2[y][x]=1
                m=m2
            xs=[x for row in m for x,v in enumerate(row) if v]; ys=[y for y,row in enumerate(m) if any(row)]
            out[(i,j)]=(px,py,max(xs)-min(xs)+1,max(ys)-min(ys)+1)
    return out
sim = measure(16/255.0)
log = {}
for ln in open(r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919/gate_lodgen_oct.txt'):
    f = ln.split('|')
    if len(f)==4 and f[0].strip().replace(' ','').isdigit() and len(f[0].split())==2:
        i,j = (int(t) for t in f[0].split()); mm = f[2].split()
        log[(i,j)] = (int(mm[0]), int(mm[1]))
agree = sum(1 for k in log if (sim[k][2],sim[k][3])==log[k])
print('frames compared: %d; simulated mask == logged bake mask on %d' % (len(log), agree))
for k in sorted(log, key=lambda k:(k[1],k[0])):
    if (sim[k][2],sim[k][3])!=log[k]: print('  differ %d,%d: sim %d,%d  bake %d,%d  pred %.2f,%.2f'%(k[0],k[1],sim[k][2],sim[k][3],log[k][0],log[k][1],sim[k][0],sim[k][1]))
print('worst sim   %.2f'%max(max(abs(v[2]-v[0]),abs(v[3]-v[1])) for v in sim.values()))
print('worst bake  %.2f'%max(max(abs(log[k][0]-sim[k][0]),abs(log[k][1]-sim[k][1])) for k in log))
d1 = measure(16/255.0, dilate=1)
print('RED CONTROL, mask dilated one ring: worst %.2f'%max(max(abs(v[2]-v[0]),abs(v[3]-v[1])) for v in d1.values()))
