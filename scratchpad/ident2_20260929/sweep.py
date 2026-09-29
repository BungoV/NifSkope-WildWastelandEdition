"""IDENT1: one line per (tolerance, cap) from a group dump -- the numbers the knob choice rests on.
usage: python sweep.py <dump> "<tol,...>" "<cap,...>"
Columns: groups, cap refusals, widest MULTI-piece group (a single piece wider than the cap cannot be split and is
listed apart), landmark groups + top group (landmark pieces / other pieces), row houses near (-5975,-15112)."""
import sys, collections
import groups

ROW = (-5975.0, -15112.0, 1500.0)


def rowhouses(P, roots, gb):
    q = [p for p in P if 'resnc' in p['name'].lower() and abs(p['x'] - ROW[0]) < ROW[2] and abs(p['y'] - ROW[1]) < ROW[2]]
    gs = collections.Counter(roots[p['i']] for p in q)
    r, k = gs.most_common(1)[0]
    g = gb[r]
    return len(q), len(gs), k, round(g[2] - g[0]), round(g[3] - g[1])


def span(P, words):
    q = [p for p in P if any(w in p['name'].lower() for w in words)]
    return round(max(p['hi'][0] for p in q) - min(p['lo'][0] for p in q)), round(max(p['hi'][1] for p in q) - min(p['lo'][1] for p in q))


def main():
    h, P, C = groups.load(sys.argv[1])
    tols = [float(t) for t in sys.argv[2].split(',')]
    caps = [float(c) for c in sys.argv[3].split(',')]
    print('Diamond City pieces span %d x %d u; Trinity %d x %d u' % (span(P, ['\\dext', '/dext', 'diamondcity']) + span(P, ['churchtrin', 'trinity'])))
    print('tol cap | groups refused | widest multi-piece (n, X x Y) | single pieces over cap | tower E | tower W | Trinity | Diamond City | row houses (pieces, groups, top n, X x Y)')
    for t in tols:
        for c in caps:
            roots, gb, ref = groups.run(P, C, t, c)
            mem, hist = groups.summary(P, roots, gb)
            multi = [(r, v) for r, v in mem.items() if len(v) > 1]
            w = max(multi, key=lambda kv: max(gb[kv[0]][2] - gb[kv[0]][0], gb[kv[0]][3] - gb[kv[0]][1]))
            g = gb[w[0]]
            over = sum(1 for r, v in mem.items() if len(v) == 1 and c > 0 and max(gb[r][2] - gb[r][0], gb[r][3] - gb[r][1]) > c)
            lm = groups.landmarks(P, roots, mem, gb)

            def L(k):
                d = lm[k]
                top = d['top'][0]
                return '%d grp, top %d/%d+%d' % (d['groups'], top['n'], top['landmarkPieces'], top['otherPieces'])
            rh = rowhouses(P, roots, gb)
            print('%4g %5d | %5d %6d | %4d, %5d x %5d | %3d | %s | %s | %s | %s | %d, %d, %d, %d x %d' % (
                t, c, len(mem), ref, len(w[1]), g[2] - g[0], g[3] - g[1], over,
                L('hub_tower_east'), L('hub_tower_west'), L('trinity_church'), L('diamond_city'), *rh))
            sys.stdout.flush()


if __name__ == '__main__':
    main()
