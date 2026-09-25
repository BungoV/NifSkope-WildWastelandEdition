"""SEAM1 gate G2 (pre-registered): the fix may move ONLY texels that paint the fallback.
  G2a  msn (role 2) and height (role 4) sheets byte-identical in EVERY tile (the law is colour/mask/cover only)
  G2b  colour (role 1) and mask (role 5) byte-identical in every tile the fix cannot reach (untouched_tiles.pkl:
       every quadrant of tile + ring has a BTXT, no NULL-LTEX layer)
  G2c  the fix DOES move colour somewhere (else it is not wired) -- the floor
usage: g2_compare.py <before VT.2.lodt> <after VT.2.lodt>"""
import sys, pickle
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread
A, B = vtread.Vt(sys.argv[1]), vtread.Vt(sys.argv[2])
untouched = set(pickle.load(open('E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/untouched_tiles.pkl', 'rb')))
roles = {s: A.sheets[s]['role'] for s in range(A.sheetCount)}
diff = {r: [0, 0] for r in roles.values()}; diffU = {1: [0, 0], 5: [0, 0]}; nU = 0; ntile = 0
for i in range(A.tileCount):
    pa, pb = A.payload(i), B.payload(i)
    if pa is None and pb is None: continue
    ty, tx = divmod(i, A.tilesX); cx0 = A.west + tx * A.levelDim; cy0 = A.north - (ty + 1) * A.levelDim + 1
    ntile += 1; u = (cx0, cy0) in untouched; nU += u
    if pa is None or pb is None:
        for r in diff: diff[r][1] += 1
        continue
    ca, cb = bool(A.tFlags[i] & 2), bool(B.tFlags[i] & 2)
    for s, r in roles.items():
        oa = A.sheetOffset(ca, s, 0); ob = B.sheetOffset(cb, s, 0)
        na = sum(A.sheetMipBytes(s, m, ca) for m in range(A.mips)); nb = sum(B.sheetMipBytes(s, m, cb) for m in range(B.mips))
        same = na == nb and pa[oa:oa + na] == pb[ob:ob + nb]
        diff[r][0 if same else 1] += 1
        if u and r in diffU: diffU[r][0 if same else 1] += 1
print('tiles compared %d (untouched %d); per role [identical, differ]: %s' % (ntile, nU, diff))
g2a = diff.get(2, [0, 0])[1] == 0 and diff.get(4, [0, 0])[1] == 0
g2b = diffU[1][1] == 0 and diffU[5][1] == 0 and nU > 0
g2c = diff[1][1] > 0
print('G2a msn+height identical everywhere: %s | G2b colour+mask identical on %d untouched tiles: %s %s | G2c colour moved somewhere: %s' % (
    'GREEN' if g2a else 'RED', nU, 'GREEN' if g2b else 'RED', diffU, 'GREEN' if g2c else 'RED'))
