p = 'fill_model.py'; s = open(p, encoding='utf-8').read()
old = "P = pickle.load(open('w2_cells.pkl', 'rb'))['painted']\n"
assert s.count(old) == 1
s = s.replace(old, """# PAINTED SET = HIS LOAD ORDER (coordinator order 16:2x): the LAND of the last plugin that carries one, painted by
# the C++ definition (land_lo.py -> land_lo.pkl). The Fallout4.esm-only set (w2_cells.pkl) misread the north-east:
# DLCCoast.esm paints 117 Commonwealth cells (12,30 among them) that the fill therefore, correctly, left alone.
# PAINTED=esm reads the old set (to reproduce the old verdict).
P = pickle.load(open('w2_cells.pkl' if os.environ.get('PAINTED') == 'esm' else 'land_lo.pkl', 'rb'))['painted']
print('painted set:', 'Fallout4.esm only (w2_cells.pkl)' if os.environ.get('PAINTED') == 'esm' else 'load order (land_lo.pkl)', len(P))
""")
old = "# V: vanilla dim-4 mosaic"
assert s.count(old) == 1
old2 = "# masks and distance to the painted set\n"
assert s.count(old2) == 1
s = s.replace(old2, old2 + """cellx = np.floor(xs / 4096).astype(int); celly = np.floor(ys / 4096).astype(int)
# PLANT=auto[:dl] -- the refuter: add dl (default 40) luminance to the first UNPAINTED cell that borders a painted
# one in the region (east or north neighbour), in A only. The file gate must read RED on it.
if os.environ.get('PLANT'):
    dlp = float(os.environ['PLANT'].split(':')[1]) if ':' in os.environ['PLANT'] else 40.0
    cand = [(cx, cy) for cy in range(Y1, Y0 - 1, -1) for cx in range(X0, X1 + 1)
            if (cx, cy) not in P and any((cx + a, cy + b) in P and X0 <= cx + a <= X1 and Y0 <= cy + b <= Y1
                                         for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    pc = cand[0]
    msk = (np.floor(ys / 4096).astype(int) == pc[1])[:, None] & (np.floor(xs / 4096).astype(int) == pc[0])[None, :]
    A[msk] = np.clip(A[msk] + dlp, 0, 255)
    print('PLANTED +%.0f lum in A at unpainted border cell %s' % (dlp, pc))
""")
old3 = "print('painted samples identical B vs F:', bool(np.array_equal(B[pm], F[pm])))\n"
assert s.count(old3) == 1
s = s.replace(old3, old3 + """# THE FILE GATE (registered 16:3x, before the load-order run on the installed bake): with VT2 = the fill bake,
# the file's steps at the painted/unpainted line stay under vanilla's line bar (0 over), and its worst cell-mean
# border step is no worse than BOTH the model's fill F and the engine-default B.
sA, lA = border_steps(A), line_steps(A); sF, sB = border_steps(F), border_steps(B)
okA = int((lA > lbar).sum()) == 0 and sA.max() <= min(sF.max(), sB.max())
wi = int(np.argmax(lA)); (j1, i1), (j2, i2) = pairs[wi]
print('FILE GATE: at-line over line bar %d (max %.2f at cells (%d,%d)|(%d,%d), bar %.2f); cell-mean max %.2f vs F %.2f, B %.2f -> %s' % (
    int((lA > lbar).sum()), lA.max(), X0 + i1, Y1 - j1, X0 + i2, Y1 - j2, lbar, sA.max(), sF.max(), sB.max(),
    'GREEN' if okA else 'RED'))
""")
open(p, 'w', encoding='utf-8').write(s)
print('patched')
