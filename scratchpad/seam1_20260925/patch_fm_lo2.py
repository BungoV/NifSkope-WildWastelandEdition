"""fill_model.py, second load-order patch (coordinator order 16:2x (B), 2026-09-25): the model's B reads the LAND of
the plugin that wins each cell (lo_model.py), the plant pushes the step OUTWARD, and the at-line clause is judged
against the step the game's own ground (B) already has at that line."""
p = 'fill_model.py'; s = open(p, encoding='utf-8').read()
def rep(old, new):
    global s
    assert s.count(old) == 1, old[:60]
    s = s.replace(old, new)
rep("import law_predict as lp, lodgen_terrain_model as tm, vanilla_tiles as vt, vtread\n",
"""import law_predict as lp, lodgen_terrain_model as tm, vanilla_tiles as vt, vtread
# LAND=lo (default): B composites each cell's LAND from the plugin that WINS it in his load order, with LTEX/TXST from
# Fallout4.esm + the DLC masters and a material-backed layer's diffuse read from its .bgsm (lo_model.py). Before, B
# read Fallout4.esm's LAND everywhere, so on DLCCoast's 253 cells it modelled ground the game never draws, and it
# painted every material-backed layer flat grey. LAND=esm = the old model, to reproduce an old verdict.
LAND = os.environ.get('LAND', 'lo')
if LAND != 'esm':
    import lo_model
    LOSTATS = lo_model.setup(lp, tm)
    print('B model: LAND from the load-order winner, official LTEX/TXST, .bgsm diffuse')
else:
    print('B model: Fallout4.esm LAND only (old)')
""")
rep("cache_f = 'fill_%s_B.npy' % TAG\n", "cache_f = 'fill_%s%s_B.npy' % (TAG, '' if LAND == 'esm' else '_Blo')\n")
rep("""    pc = cand[0]
    msk = (np.floor(ys / 4096).astype(int) == pc[1])[:, None] & (np.floor(xs / 4096).astype(int) == pc[0])[None, :]
    A[msk] = np.clip(A[msk] + dlp, 0, 255)
    print('PLANTED +%.0f lum in A at unpainted border cell %s' % (dlp, pc))
""", """    pc = cand[0]
    cm = lambda c: (np.floor(ys / 4096).astype(int) == c[1])[:, None] & (np.floor(xs / 4096).astype(int) == c[0])[None, :]
    nb = next((pc[0] + a, pc[1] + b) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))
              if (pc[0] + a, pc[1] + b) in P and X0 <= pc[0] + a <= X1 and Y0 <= pc[1] + b <= Y1)
    # OUTWARD: the plant widens the step to its painted neighbour (a +40 on a cell already darker than its painted
    # neighbour would NARROW that step, and a gate on the step could not see it)
    sg = 1.0 if (A[cm(pc)] @ LW).mean() >= (A[cm(nb)] @ LW).mean() else -1.0
    msk = cm(pc)
    A[msk] = np.clip(A[msk] + sg * dlp, 0, 255)
    print('PLANTED %+.0f lum in A at unpainted border cell %s (outward from painted %s)' % (sg * dlp, pc, nb))
""")
rep("""okA = int((lA > lbar).sum()) == 0 and sA.max() <= min(sF.max(), sB.max())
wi = int(np.argmax(lA)); (j1, i1), (j2, i2) = pairs[wi]
print('FILE GATE: at-line over line bar %d (max %.2f at cells (%d,%d)|(%d,%d), bar %.2f); cell-mean max %.2f vs F %.2f, B %.2f -> %s' % (
    int((lA > lbar).sum()), lA.max(), X0 + i1, Y1 - j1, X0 + i2, Y1 - j2, lbar, sA.max(), sF.max(), sB.max(),
    'GREEN' if okA else 'RED'))
""", """# AT-LINE CLAUSE, per border (load-order patch, 2026-09-25): the file's step may exceed the step the game's own ground
# (B, the engine-default law over the WINNING LAND) has at that same line by at most vanilla's line bar. A plugin that
# paints a bright block beside unpainted ground (DLCCoast: LDriedGrass01 at x 11-16, y 29-35) is a step the game draws
# up close too; the fill leaves it (w = 0 at the line by definition) and must not add to it. Wherever B has no step
# this is the old clause (lA <= lbar).
lB = line_steps(B)
overL = lA > lB + lbar
okA = int(overL.sum()) == 0 and sA.max() <= min(sF.max(), sB.max())
wi = int(np.argmax(lA - lB)); (j1, i1), (j2, i2) = pairs[wi]
print('FILE GATE: at-line over (B step + line bar) %d (worst file %.2f vs B %.2f at cells (%d,%d)|(%d,%d), bar %.2f; '
      'vanilla-bar-only reading %d over); cell-mean max %.2f vs F %.2f, B %.2f -> %s' % (
    int(overL.sum()), lA[wi], lB[wi], X0 + i1, Y1 - j1, X0 + i2, Y1 - j2, lbar, int((lA > lbar).sum()),
    sA.max(), sF.max(), sB.max(), 'GREEN' if okA else 'RED'))
if LAND != 'esm':
    print('B model: %d material layers read from .bgsm; unresolved %d %s' % (
        LOSTATS['bgsm'], len(LOSTATS['unresolved']), LOSTATS['unresolved'][:3]))
""")
open(p, 'w', encoding='utf-8').write(s)
print('patched')
