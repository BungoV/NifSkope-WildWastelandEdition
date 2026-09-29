"""IDENT1: re-run the contact join offline from a WW_LODI_GROUP_DUMP file, at any tolerance and cap, and print
the numbers the brief asks for: the size histogram, the largest groups, and the named buildings.

usage: python groups.py <dump> [tol] [cap] [--json out.json] [--pairs]
The union order is the emitter's: SCOL parts first (uncapped), then touching pairs nearest first, ties on (i, j);
a union is refused when the merged group's world X or Y extent would pass the cap (0 = no cap)."""
import sys, os, json, collections

LANDMARKS = {
    # the audit's words and centres (audit1 aud_objects.json); a centre narrows a kit shared across the city
    'hub_tower_east': (['hitext'], (3625, -24292), 2500),
    'hub_tower_west': (['hitext'], (-2271, -30152), 2500),
    # audit1: Trinity's kit is ChurchTrin*.nif, Diamond City's outside is DExt*.nif (measured model names)
    'trinity_church': (['churchtrin', 'trinity'], None, 0),
    'diamond_city': (['\\dext', '/dext', 'diamondcity'], None, 0),
}


def load(path):
    P = []
    C = []
    head = ''
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('#'):
            head = head or line.strip()
            continue
        f = line.rstrip('\n').split(' ', 17)
        if f[0] == 'P':
            P.append({'i': int(f[1]), 'ref': int(f[2], 16), 'part': int(f[3]), 'base': int(f[4], 16),
                      'tree': f[5] == '1', 'tris': int(f[6]), 'x': float(f[7]), 'y': float(f[8]), 'z': float(f[9]),
                      'lo': [float(v) for v in f[10:13]], 'hi': [float(v) for v in f[13:16]], 'root': int(f[16]),
                      'name': f[17] if len(f) > 17 else ''})
        elif f[0] == 'C':
            C.append((float(f[3]), int(f[1]), int(f[2])))
    return head, P, C


def landmark_sets(P, path):
    """IDENT2: the emitter's landmark rule, offline -- index lists of the drawn pieces each rule matches."""
    def norm(q):
        q = q.strip().lower().replace(chr(92), '/').lstrip('/')
        if q.startswith('meshes/'):
            q = q[7:]
        if q.startswith('lod/'):
            q = q[4:]
        return q
    rules = []
    for line in open(path, encoding='utf-8'):
        t = line.strip()
        if not t or t.startswith('#'):
            continue
        f = [x.strip() for x in t.split('|')]
        c = f[2].split() if len(f) > 2 else []
        rules.append(([norm(x) for x in f[1].split(';') if x.strip()], [float(v) for v in c] if c else None))
    out = [[] for _ in rules]
    for p in P:
        if not p['tris']:
            continue
        nm = p['name']; a = nm.find(' (')
        model = norm(nm[a + 2:-1]) if a >= 0 and nm.endswith(')') else ''
        if not model:
            continue
        for k, (pre, cen) in enumerate(rules):
            if not any(model.startswith(x) for x in pre):
                continue
            if cen and (p['x'] - cen[0]) ** 2 + (p['y'] - cen[1]) ** 2 > cen[2] ** 2:
                continue
            out[k].append(p['i'])
            break
    return out


def run(P, C, tol, cap, pre=()):
    n = len(P)
    uf = list(range(n))

    def find(a):
        while uf[a] != a:
            uf[a] = uf[uf[a]]
            a = uf[a]
        return a

    def join(a, b):
        a, b = find(a), find(b)
        if a != b:
            uf[max(a, b)] = min(a, b)
    first = {}
    for p in P:
        if p['part'] >= 0:
            if p['ref'] in first:
                join(first[p['ref']], p['i'])
            else:
                first[p['ref']] = p['i']
    for v in pre:
        for i in v[1:]:
            join(v[0], i)
    gb = [[p['lo'][0], p['lo'][1], p['hi'][0], p['hi'][1]] for p in P]
    for i in range(n):
        r = find(i)
        if r != i:
            g = gb[r]; h = gb[i]
            gb[r] = [min(g[0], h[0]), min(g[1], h[1]), max(g[2], h[2]), max(g[3], h[3])]
    refused = 0
    for d, i, j in sorted(C):
        if d > tol:
            break
        a, b = find(i), find(j)
        if a == b:
            continue
        g, h = gb[a], gb[b]
        m = [min(g[0], h[0]), min(g[1], h[1]), max(g[2], h[2]), max(g[3], h[3])]
        if cap > 0 and (m[2] - m[0] > cap or m[3] - m[1] > cap):
            refused += 1
            continue
        join(a, b)
        gb[min(a, b)] = m
    roots = [find(i) for i in range(n)]
    return roots, gb, refused


def summary(P, roots, gb):
    mem = collections.defaultdict(list)
    for p in P:
        mem[roots[p['i']]].append(p)
    sizes = collections.Counter()
    bins = [(1, 1), (2, 4), (5, 16), (17, 64), (65, 256), (257, 1024), (1025, 10 ** 9)]
    for v in mem.values():
        for lo, hi in bins:
            if lo <= len(v) <= hi:
                sizes['%d-%d' % (lo, hi) if hi < 10 ** 9 else '>%d' % (lo - 1)] += 1
    return mem, [(k, sizes.get(k, 0)) for k in ['1-1', '2-4', '5-16', '17-64', '65-256', '257-1024', '>1024']]


def describe(root, v, gb):
    g = gb[root]
    mods = collections.Counter(os.path.basename(p['name'].split('(')[-1].rstrip(')')) for p in v).most_common(3)
    return {'root': root, 'n': len(v), 'spanX': round(g[2] - g[0]), 'spanY': round(g[3] - g[1]),
            'topZ': round(max(p['hi'][2] for p in v)), 'centre': [round((g[0] + g[2]) / 2), round((g[1] + g[3]) / 2)],
            'models': mods}


def landmarks(P, roots, mem, gb):
    out = {}
    for key, (words, centre, rad) in LANDMARKS.items():
        hit = []
        for p in P:
            nm = p['name'].lower()
            if not any(w in nm for w in words):
                continue
            if centre and ((p['x'] - centre[0]) ** 2 + (p['y'] - centre[1]) ** 2) ** 0.5 > rad:
                continue
            hit.append(p)
        gs = collections.Counter(roots[p['i']] for p in hit)
        rows = []
        for r, k in gs.most_common(8):
            d = describe(r, mem[r], gb)
            d['landmarkPieces'] = k
            d['otherPieces'] = len(mem[r]) - k
            rows.append(d)
        out[key] = {'pieces': len(hit), 'groups': len(gs), 'top': rows}
    return out


def main():
    argv = list(sys.argv[1:])
    jout = None
    if '--json' in argv:
        k = argv.index('--json'); jout = argv[k + 1]; del argv[k:k + 2]
    a = [x for x in argv if not x.startswith('--')]
    path = a[0]
    tol = float(a[1]) if len(a) > 1 else 2.0
    cap = float(a[2]) if len(a) > 2 else 0.0
    head, P, C = load(path)
    roots, gb, refused = run(P, C, tol, cap)
    mem, hist = summary(P, roots, gb)
    elig = sum(1 for p in P if p['tris'] and not p['tree'])
    out = {'dump': head, 'tol': tol, 'cap': cap, 'placements': len(P), 'eligible': elig, 'pairsWithin32': len(C),
           'groups': len(mem), 'capRefused': refused, 'histogram': hist}
    big = sorted(mem.items(), key=lambda kv: -len(kv[1]))[:10]
    out['largest'] = [describe(r, v, gb) for r, v in big]
    wide = sorted(mem.items(), key=lambda kv: -max(gb[kv[0]][2] - gb[kv[0]][0], gb[kv[0]][3] - gb[kv[0]][1]))[:10]
    out['widest'] = [describe(r, v, gb) for r, v in wide]
    out['landmarks'] = landmarks(P, roots, mem, gb)
    if '--pairs' in sys.argv:
        dh = collections.Counter()
        for d, i, j in C:
            b = 0 if d == 0 else (1 if d <= 0.5 else (2 if d <= 1 else (3 if d <= 2 else (4 if d <= 4 else (5 if d <= 8 else (6 if d <= 16 else 7))))))
            dh[b] += 1
        out['pairDistance'] = {k: dh.get(i, 0) for i, k in enumerate(['0', '<=0.5', '<=1', '<=2', '<=4', '<=8', '<=16', '<=32'])}
    js = json.dumps(out, indent=1)
    if jout:
        open(jout, 'w').write(js)
    print(js)


if __name__ == '__main__':
    main()
