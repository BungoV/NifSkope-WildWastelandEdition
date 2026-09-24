import sys, os
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/clamp_20260910')
from edgeband import decode
A, w, h = decode(sys.argv[1]); B, w2, h2 = decode(sys.argv[2])
assert (w, h) == (w2, h2)
n = 0; near = {'N': 0, 'S': 0, 'E': 0, 'W': 0, 'interior': 0}; maxd = 0
rows = {}; cols = {}
for y in range(h):
    for x in range(w):
        if A[y*w+x] == B[y*w+x]: continue
        n += 1
        dN, dS, dW, dE = y, h-1-y, x, w-1-x     # row 0 = top of the sheet
        d = min(dN, dS, dW, dE); maxd = max(maxd, d)
        side = min(('N', dN), ('S', dS), ('W', dW), ('E', dE), key=lambda t: t[1])[0]
        near[side if d <= 7 else 'interior'] += 1
        rows[y] = rows.get(y, 0) + 1; cols[x] = cols.get(x, 0) + 1
print('sheet %dx%d differing %d (%.4f%%) maxd %d' % (w, h, n, 100.0*n/(w*h), maxd))
print('nearest border within 7 texels:', near)
print('rows touched (y: count), first 12:', sorted(rows.items())[:12], '... last 6:', sorted(rows.items())[-6:])
print('cols touched (x: count), first 6:', sorted(cols.items())[:6], '... last 12:', sorted(cols.items())[-12:])
