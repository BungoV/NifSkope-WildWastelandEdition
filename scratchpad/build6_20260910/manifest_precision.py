"""BUILD6: is the decoder's manifest X/Y miss (worst 0.17 > 0.125) the manifest's own
6-significant-digit print? For every row: allowed = 0.125 (the .lodi quantisation bound the
ESM leg passes at) + half the manifest's print step for that magnitude."""
import sys, math
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as D
for dim in (4, 8, 16):
    base = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build6_20260910/native/d%d/' % dim
    L = D.read_lodo(base + 'Native/Commonwealth.lodo'); T = D.read_lodi(base + 'Native/Commonwealth.lodi')
    table = {}
    for c, r in zip(T['cold'], T['instances']):
        table[(c['refFormId'], c['scolPart'])] = r
    rows = over125 = overBudget = big = 0; worst = 0.0; worstBudget = 0.0
    for line in open(base + 'obj/Commonwealth.%d.0.0.BTO.manifest.txt' % dim, encoding='utf-8'):
        t = line.split()
        if not t or not t[0].isdigit() or len(t) < 11: continue
        r = table.get((int(t[9], 16), int(t[10])))
        if r is None: continue
        rows += 1
        for col, key in ((3, 'x'), (4, 'y')):
            v = float(t[col]); d = abs(r[key] - v)
            step = 10.0 ** (math.floor(math.log10(abs(v))) - 5) if v != 0 else 0.0   # %g, 6 significant digits
            worst = max(worst, d)
            if d > 0.125 + 1e-3:
                over125 += 1
                if abs(v) >= 10000: big += 1
            if d > 0.125 + step / 2 + 1e-3:
                overBudget += 1; worstBudget = max(worstBudget, d - step / 2)
    print('dim %2d: %d rows; coords over 0.125: %d (of which |v| >= 10000, print step 0.1: %d); over 0.125 + half the print step: %d; worst %.4f; worst after the print budget %.4f'
          % (dim, rows, over125, big, overBudget, worst, worstBudget))
