"""SEAM1 G2 population: dim-2 VT tiles the fix CANNOT move -- every quadrant of every cell in the tile AND its one-cell
ring (the quadrant cross-fade and the ring bake read it) carries a BTXT, and no ATXT layer names LTEX 0."""
import pickle
d = pickle.load(open('fo4esm_cw.pkl', 'rb')); lands = d['lands']
def clean(cx, cy):
    L = lands.get((cx, cy))
    if not L: return True    # no LAND: nothing painted, nothing to fall back to
    return all(L['base']) and all(l['ltex'] for q in L['layers'] for l in q)
nb = sum(1 for L in lands.values() for b in L['base'] if not b); nq = 4 * len(lands)
nn = sum(1 for L in lands.values() for q in L['layers'] for l in q if not l['ltex'])
na = sum(1 for L in lands.values() for q in L['layers'] for l in q)
print('Commonwealth LAND quadrants with no BTXT: %d of %d (%.1f%%); NULL-LTEX layers %d of %d' % (nb, nq, 100.0 * nb / nq, nn, na))
ok = []
painted = 0
for cy0 in range(-96, 96, 2):
    for cx0 in range(-96, 96, 2):
        cells = [(cx0 + x, cy0 + y) for x in range(-1, 3) for y in range(-1, 3)]
        own = [(cx0 + x, cy0 + y) for x in range(2) for y in range(2)]
        if not any(lands.get(c) and any(lands[c]['layers']) for c in own): continue
        painted += 1
        if all(clean(*c) for c in cells): ok.append((cx0, cy0))
print('painted dim-2 tiles', painted, '; untouched by the fix (tile + ring clean):', len(ok), ok[:12])
pickle.dump(ok, open('untouched_tiles.pkl', 'wb'))
