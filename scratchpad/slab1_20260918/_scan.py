import re, collections, sys
rows=[l.split() for l in open('before/vt/Commonwealth.4.4.-12.BTO.manifest.txt',encoding='utf-8',errors='replace').read().splitlines() if not l.startswith('#')]
pl=[r for r in rows if len(r)==11 and r[0]!='I']
name={r[1]:r[2] for r in rows if r[0]=='I'}
pat=re.compile(r'highway|hwy|overpass|elevated|bridge|\HW', re.I)
hits={b:n for b,n in name.items() if pat.search(n)}
c=collections.Counter(); sel=[]
for r in pl:
    if r[1] in hits:
        c[hits[r[1]]]+=1; sel.append(r)
print('%d bases, %d placements'%(len(hits),len(sel)))
for n,k in c.most_common(40): print('  %4d  %s'%(k,n))
if sel:
    print('extent x %.0f..%.0f y %.0f..%.0f'%(min(float(r[3]) for r in sel),max(float(r[3]) for r in sel),min(float(r[4]) for r in sel),max(float(r[4]) for r in sel)))
    sel.sort(key=lambda r: float(r[3]))
    for r in sel[:60]:
        print('  %8.1f %9.1f z%8.1f h%7.1f ref %s %s'%(float(r[3]),float(r[4]),float(r[5]),float(r[8]),r[9],name[r[1]].split('\')[-1]))
