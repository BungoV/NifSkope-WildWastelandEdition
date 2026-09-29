"""IDENT2 follow-up: which pieces lie in/near a landmark's footprint and are not in its group (dump analysis)."""
import sys, collections, math, groups
h, P, C = groups.load(sys.argv[1])
rules = {'dc': ('architecture/diamondcity/', (-12500, -25700, 7000)),
         'west': ('architecture/buildings/hightech/hitext', (-2271, -30152, 2500)),
         'east': ('architecture/buildings/hightech/hitext', (3625, -24292, 2500)),
         'trin': ('architecture/unique/trinitychurch/', None)}
def model(n):
    a = n.find(' ('); q = n[a+2:-1].lower().replace(chr(92), '/')
    for p in ('meshes/', 'lod/'):
        if q.startswith(p): q = q[len(p):]
    return q
which = sys.argv[2]; pre, cen = rules[which]; M = float(sys.argv[3]) if len(sys.argv) > 3 else 64
mem = [p for p in P if p['tris'] and model(p['name']).startswith(pre) and (cen is None or math.hypot(p['x']-cen[0], p['y']-cen[1]) <= cen[2])]
lo = [min(p['lo'][k] for p in mem) for k in range(3)]; hi = [max(p['hi'][k] for p in mem) for k in range(3)]
print(which, len(mem), 'named pieces; bounds x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f' % (lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
roots = collections.Counter(p['root'] for p in mem); R = roots.most_common(1)[0][0]
out = []
for p in P:
    if p['root'] == R: continue
    # xy overlap with the bounds box at all?
    if p['hi'][0] < lo[0] or p['lo'][0] > hi[0] or p['hi'][1] < lo[1] or p['lo'][1] > hi[1]: continue
    over = max(lo[0]-p['lo'][0], p['hi'][0]-hi[0], lo[1]-p['lo'][1], p['hi'][1]-hi[1], 0)
    out.append((over, p))
out.sort(key=lambda t: t[0])
cnt = collections.Counter()
for over, p in out:
    kind = 'tree' if p['tree'] else ('notris' if not p['tris'] else 'piece')
    cnt[(kind, over <= M)] += 1
    if kind == 'piece' and over <= float(sys.argv[4] if len(sys.argv) > 4 else 2000):
        print('  over %6.0f  size %5.0fx%5.0fx%5.0f  z %5.0f..%5.0f  root %d  %s' % (over, p['hi'][0]-p['lo'][0], p['hi'][1]-p['lo'][1], p['hi'][2]-p['lo'][2], p['lo'][2], p['hi'][2], p['root'], model(p['name'])))
print(cnt)
