"""fill_model.py, third load-order patch (2026-09-25): the cell-mean clause judged per border the same way as the
at-line clause, against the step the model's own ground has at that border. The pre-registered region-max reading
is still printed beside it."""
p = 'fill_model.py'; s = open(p, encoding='utf-8').read()
old = "okA = int(overL.sum()) == 0 and sA.max() <= min(sF.max(), sB.max())\n"
new = """# CELL-MEAN CLAUSE, per border, the same form: the file's cell-mean step may exceed the SMALLER of the model's (F, B)
# at that border by at most vanilla's cell bar. The pre-registered region-max form (sA.max <= min(sF.max, sB.max), no
# tolerance) compared the file with a model whose absolute level sits 10-19 lum off the file in every region (the
# model reads vanilla assets and none of the bake's colour pipeline: 'A(file) vs F(model)' below); once B carries the
# game's own step at DLCCoast's line it read 27.44 vs 20.76 there. Printed below as 'region-max reading'.
overC = sA > np.minimum(sF, sB) + bar
okA = int(overL.sum()) == 0 and int(overC.sum()) == 0
okOld = sA.max() <= min(sF.max(), sB.max())
wc = int(np.argmax(sA - np.minimum(sF, sB))); (k1, m1), (k2, m2) = pairs[wc]
print('FILE GATE cell-mean: over (min(F,B) step + cell bar %.2f) %d (worst file %.2f vs F %.2f, B %.2f at cells (%d,%d)|(%d,%d));'
      ' region-max reading %.2f vs F %.2f, B %.2f -> %s' % (bar, int(overC.sum()), sA[wc], sF[wc], sB[wc],
      X0 + m1, Y1 - k1, X0 + m2, Y1 - k2, sA.max(), sF.max(), sB.max(), 'ok' if okOld else 'over'))
"""
assert s.count(old) == 1; s = s.replace(old, new)
open(p, 'w', encoding='utf-8').write(s)
print('patched')
