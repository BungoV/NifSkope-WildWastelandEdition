p='fill_model.py'; s=open(p,newline='').read(); cr=s.count('\r')
rep=[
("""overlap = pm & near_out
ring_out = (~pm) & (d <= RING * 4096)
""","""overlap = pm & near_out
# ring = unpainted cells with a painted cell within RING cells, Chebyshev -- the C++ definition
near_in = np.array([[any((cx + a, cy + b) in P for a in range(-RING, RING + 1) for b in range(-RING, RING + 1))
                     for cx in cellx] for cy in celly])
ring_out = (~pm) & near_in
"""),
("""if os.environ.get('GAIN_CAP'): ga = min(ga, float(os.environ['GAIN_CAP']))""",
"""ga = min(ga, float(os.environ.get('GAIN_CAP', '1')))   # the C++ caps at 1 (vanilla's baked relief light)"""),
("""cvm = cellmean(V)
steps_v = np.concatenate([np.abs(np.diff(cvm, axis=0)).ravel(), np.abs(np.diff(cvm, axis=1)).ravel()])
bar = float(np.percentile(steps_v, 99))
ro_c = ring_out.reshape(H // n, n, W // n, n)[:, 0, :, 0]
dl = float(np.percentile(np.abs(cellmean(B) - cellmean(TV))[ro_c], 95))
band = max(1, math.ceil(dl / bar)) * 4096.0
""","""cvm = cellmean(V)
# bar = p99 of vanilla adjacent cell-mean steps over pairs with BOTH cells in overlap or ring (the C++ population)
ro_c = ring_out.reshape(H // n, n, W // n, n)[:, 0, :, 0]
role = ovc | ro_c
sy = np.abs(np.diff(cvm, axis=0))[role[1:, :] & role[:-1, :]]
sx = np.abs(np.diff(cvm, axis=1))[role[:, 1:] & role[:, :-1]]
steps_v = np.concatenate([sy.ravel(), sx.ravel()])
bar = float(np.percentile(steps_v, 99))
# band: p95 over the ring of |lum B cell mean - lum T(V cell mean)|, as the C++ computes it
dl = float(np.percentile(np.abs(cellmean(B) - lum(T(cellrgb(V))))[ro_c], 95))
band = min(8, max(1, math.ceil(dl / bar))) * 4096.0
"""),
]
for a,b in rep:
    a=a.replace('\n','\r\n'); b=b.replace('\n','\r\n')
    assert s.count(a)==1,a[:60]; s=s.replace(a,b)
open(p,'w',newline='').write(s)
print(s.count('\r')-cr, 'CR added')
