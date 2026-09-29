"""IDENT2 follow-up: the PER-PIECE footprint rule offline (the coordinator's words): a piece joins when its box
overlaps the outline and no corner leaves it by more than M. Shows, per landmark, how many join, and every
contact group (dump root, pre-footprint dump) the rule SPLITS: pieces joined vs left, with names.
usage: python fp_piece.py <dump> <landmarks.txt> [M]"""
import sys, collections, groups, fp_hull as FH
h, P, C = groups.load(sys.argv[1]); rules = FH.load_rules(sys.argv[2]); M = float(sys.argv[3]) if len(sys.argv) > 3 else 256
byroot = collections.defaultdict(list)
for p in P: byroot[p['root']].append(p)
for rule in rules:
    mem, H, rows = FH.classify(P, rule, M, byroot)
    lmroots = set(p['root'] for p in mem); memi = set(p['i'] for p in mem)
    J = []; R = []
    for p in P:
        if p['i'] in memi or p['root'] in lmroots or not p['tris'] or FH.veg(p): continue
        if not FH.sat_overlap(H, p): continue
        w = max(FH.outside(H, q) for q in FH.corners(p))
        (J if w <= M else R).append((w, p))
    jr = collections.Counter(p['root'] for w, p in J); rr = collections.Counter(p['root'] for w, p in R)
    print('\n%s: margin %g: %d pieces join (%d groups), %d pieces refused; worst joined %.0f, least refused %.0f' % (
        rule[0], M, len(J), len(jr), len(R), max([w for w, p in J] or [0]), min([w for w, p in R] or [0])))
    for rt in sorted(jr):
        g = [p for p in byroot[rt] if p['tris']]
        if len(g) > jr[rt]:
            js = [p for w, p in J if p['root'] == rt]
            print('  SPLIT group %d: %d of %d drawn pieces join: %s' % (rt, jr[rt], len(g), '; '.join(sorted(set(FH.model(p['name']).split('/')[-1] for p in js)))[:300]))
    for w, p in sorted(R, key=lambda t: t[0])[:6]:
        print('  refused %5.0f u  root %d  %s' % (w, p['root'], FH.model(p['name'])))
