import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import roadgeo as rg
R = rg.Reader()
hits = collections.defaultdict(list)
for form, r, cx, cy in R.refs_in(-12, -14, 6, 4):
    bi = R.base_info(r['base'])
    if not bi: continue
    m = bi['modl'].lower()
    if ('diamondcity' in m and 'exterior' in m) or 'fenway' in m or ('bridge' in m and 'roads' in m) or 'massbridge' in m or 'longfellow' in m:
        hits[m].append((cx, cy, round(r['pos'][0]), round(r['pos'][1]), round(r['pos'][2]), bi['type'], bi['hasLod']))
for m, l in sorted(hits.items()):
    xs = [h[2] for h in l]; ys = [h[3] for h in l]
    print('%-70s n=%3d x %7d..%7d y %7d..%7d cells %s' % (m[:70], len(l), min(xs), max(xs), min(ys), max(ys), sorted(set((h[0], h[1]) for h in l))[:6]))
