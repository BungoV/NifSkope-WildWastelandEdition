"""IDENT2 follow-up: the footprint outline offline = convex hull (X/Y) of the name-matched pieces' box corners.
usage: python fp_hull.py <dump> <landmarks.txt> [margin] [show_over_max]
Per landmark: pieces NOT in the landmark's group whose box overlaps the hull, with how far their box leaves the
hull (0 = fully inside). Trees/plants skipped (tree flag, no triangles, or a vegetation model path)."""
import sys, collections, math, groups
def model(n):
    a = n.find(' ('); q = n[a+2:-1].lower().replace(chr(92), '/')
    for p in ('meshes/', 'lod/'):
        if q.startswith(p): q = q[len(p):]
    return q
VEG = ('landscape/trees/', 'landscape/plants/', 'landscape/vines/', 'landscape/grass/', 'plants/', 'trees/')
def veg(p): return p['tree'] or not p['tris'] or model(p['name']).startswith(VEG)
def hull(pts):
    pts = sorted(set(pts))
    def cr(o, a, b): return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cr(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cr(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]   # counter-clockwise
def outside(H, q):
    """signed: max over edges of the distance past the edge line (<=0 inside)."""
    d = -1e18
    for k in range(len(H)):
        a, b = H[k], H[(k+1) % len(H)]
        ex, ey = b[0]-a[0], b[1]-a[1]; L = math.hypot(ex, ey)
        # outward normal of a CCW polygon = (ey, -ex)
        d = max(d, ((q[0]-a[0])*ey - (q[1]-a[1])*ex) / L)
    return d
def corners(p): return [(p['lo'][0], p['lo'][1]), (p['hi'][0], p['lo'][1]), (p['hi'][0], p['hi'][1]), (p['lo'][0], p['hi'][1])]
def load_rules(path):
    rules = []
    for line in open(path, encoding='utf-8'):
        t = line.strip()
        if not t or t.startswith('#'): continue
        f = [x.strip() for x in t.split('|')]
        c = [float(v) for v in f[2].split()] if len(f) > 2 and f[2] else None
        rules.append((f[0], [x for x in f[1].split(';') if x], c))
    return rules


def sat_overlap(H, p):
    """AABB (X/Y) of piece p vs convex polygon H: True when they overlap (separating-axis test)."""
    xs = [x for x, _ in H]; ys = [y for _, y in H]
    if max(xs) < p['lo'][0] or min(xs) > p['hi'][0] or max(ys) < p['lo'][1] or min(ys) > p['hi'][1]:
        return False
    cs = corners(p)
    for k in range(len(H)):
        a, b = H[k], H[(k+1) % len(H)]
        nx, ny = b[1]-a[1], -(b[0]-a[0])
        if min((q[0]-a[0])*nx + (q[1]-a[1])*ny for q in cs) > 0:
            return False
    return True


def classify(P, rule, M, byroot=None):
    """Group-level footprint rule (the emitter's): -> (named, hull, rows[(worst over, group root, pieces, kind)]).
    A contact group NOT the landmark's, with a drawn piece overlapping the hull, joins when every drawn piece's
    box stays within M of the hull; else it is refused whole."""
    name, pre, c = rule
    mem = [p for p in P if p['tris'] and not p['tree'] and model(p['name']).startswith(tuple(pre)) and (c is None or math.hypot(p['x']-c[0], p['y']-c[1]) <= c[2])]
    H = hull([q for p in mem for q in corners(p)])
    if byroot is None:
        byroot = collections.defaultdict(list)
        for p in P: byroot[p['root']].append(p)
    lmroots = set(p['root'] for p in mem); rows = []
    for rt, g in byroot.items():
        if rt in lmroots: continue
        d = [p for p in g if p['tris'] and not p['tree']]
        if not d or not any(sat_overlap(H, p) for p in d): continue
        worst = max(max(outside(H, q) for q in corners(p)) for p in d)
        kind = 'veg' if any(veg(p) for p in g) else ('join' if worst <= M else 'refuse')
        rows.append((worst, rt, d, kind))
    return mem, H, rows


if __name__ == '__main__':
    h, P, C = groups.load(sys.argv[1]); M = float(sys.argv[3]) if len(sys.argv) > 3 else 128
    for rule in load_rules(sys.argv[2]):
        mem, H, rows = classify(P, rule, M)
        cnt = collections.Counter(k for *_, k in rows)
        print('%s: %d named, hull %d points; groups overlapping %s, pieces joining %d' % (rule[0], len(mem), len(H), dict(cnt), sum(len(d) for w, r, d, k in rows if k == 'join')))
        for w, rt, d, k in sorted(rows, key=lambda t: -t[0]):
            if k != 'join':
                print('   %s group %d: %d pieces, worst %.0f u outside, e.g. %s' % (k, rt, len(d), w, model(d[0]['name'])))
