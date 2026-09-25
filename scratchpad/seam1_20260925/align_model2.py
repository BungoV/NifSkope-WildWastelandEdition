p='fill_model.py'; s=open(p,newline='').read()
rep=[
("""v2 = vtread.Vt(r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt')""",
"""# A = the shipped bake by default; VT2=<a VT.2.lodt> reads another (the post-build gate: VT2 = the fill bake)
v2 = vtread.Vt(os.environ.get('VT2', r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'))
print('A read from', v2.path)"""),
("""for name, Fx in (('B engine default', B), ('F fill', F), ('V vanilla', V)):
    c = cellmean(Fx)""","""for name, Fx in (('A (file)', A), ('B engine default', B), ('F fill', F), ('V vanilla', V)):
    c = cellmean(Fx)"""),
]
for a,b in rep:
    a=a.replace('\n','\r\n'); b=b.replace('\n','\r\n')
    assert s.count(a)==1,a[:60]; s=s.replace(a,b)
s=s.rstrip('\r\n')+'\r\n'+"""# model vs file on the UNPAINTED samples (meaningful only when VT2 is a fill bake): the C++ against its proof\r
upm = ~pm\r
print('A(file) vs F(model) on unpainted samples: mean |dlum| %.2f, p95 %.2f; vs B: mean %.2f' % (\r
    np.abs(lum(A) - lum(F))[upm].mean(), np.percentile(np.abs(lum(A) - lum(F))[upm], 95), np.abs(lum(A) - lum(B))[upm].mean()))\r
"""
open(p,'w',newline='').write(s)
