"""Cell load gate (lane PERFLOAD1, 2026-11-10): fails when a cell load got slower.

usage: python -I cell_load_gate.py --new <speed.txt>... [--ref <speed.txt>...] [--slack 10]

Each file is a WW_CELL_SPEED_DUMP stage table (CELLSPEED v1). The reference is the FASTEST of the reference tables,
the new side is the MEDIAN of the new tables (skill ww-cell-open-speed-gate): one lucky new run cannot pass and one
unlucky reference cannot hide a loss. Without --ref the tables in cell_load_ref/ beside this file are the reference
(Goodneighbor 5x5 with the 11-09 flight's switches, see its README.txt).

LOAD = the sum of the stage walls up to "views rebuilt (completeLoading)" (the time until the window can paint).
FAIL when: the document counts differ (not the same load), or median new LOAD > fastest reference LOAD * (1 + slack %).
Stages that grew by more than the slack and by more than 500 ms are listed, the gate decides on LOAD only.
Exit code 0 = PASS, 1 = FAIL, 2 = bad input.
"""
import glob
import os
import statistics
import sys

AFTER = 'after the load'
COUNTS = ('placements', 'placements drawn', 'model loads', 'shapes written', 'vertices written', 'triangles written',
          'document blocks', 'textures named by materials')


def read(path):
    stages, counts = [], {}
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.read().splitlines()
    if not lines or not lines[0].startswith('CELLSPEED v1'):
        raise ValueError('%s: not a CELLSPEED v1 table' % path)
    for l in lines[1:]:
        p = l.split('\t')
        if p[0] == 'stage' and len(p) >= 4 and not p[1].startswith(AFTER):
            stages.append((p[1], float(p[2]), float(p[3])))
        elif p[0] == 'count' and len(p) >= 3:
            counts[p[1].strip()] = int(p[2])
    if not stages:
        raise ValueError('%s: no stage rows' % path)
    return {'path': path, 'stages': stages, 'counts': counts, 'load': sum(s[1] for s in stages)}


def main(argv):
    ref, new, slack, into = [], [], 10.0, None
    for a in argv:
        if a == '--ref':
            into = ref
        elif a == '--new':
            into = new
        elif a == '--slack':
            into = 'slack'
        elif into == 'slack':
            slack, into = float(a), None
        elif into is not None:
            into.append(a)
        else:
            print(__doc__)
            return 2
    if not ref:
        ref = sorted(glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cell_load_ref', '*.speed.txt')))
    if not ref or not new:
        print('cell load gate: needs --new tables and reference tables (%d ref, %d new)' % (len(ref), len(new)))
        return 2
    try:
        R = [read(p) for p in ref]
        N = [read(p) for p in new]
    except (OSError, ValueError) as e:
        print('cell load gate: %s' % e)
        return 2

    bad = []
    base = R[0]['counts']
    for t in R[1:] + N:
        for k in COUNTS:
            if t['counts'].get(k) != base.get(k):
                bad.append('%s: %s %s, reference %s' % (os.path.basename(os.path.dirname(t['path'])) or t['path'], k,
                                                       t['counts'].get(k), base.get(k)))
    names = [s[0] for s in R[0]['stages']]
    print('cell load gate: %d reference table(s), %d new; slack %.0f %%' % (len(R), len(N), slack))
    print('%-46s %12s %12s %9s' % ('stage (wall ms)', 'fastest ref', 'median new', 'change'))
    grew = []
    for i, name in enumerate(names):
        r = min(t['stages'][i][1] for t in R if i < len(t['stages']) and t['stages'][i][0] == name)
        ns = [t['stages'][i][1] for t in N if i < len(t['stages']) and t['stages'][i][0] == name]
        if not ns:
            bad.append('stage "%s" is missing in the new tables' % name)
            continue
        n = statistics.median(ns)
        print('%-46s %12.0f %12.0f %+8.0f' % (name, r, n, n - r))
        if n - r > 500 and n > r * (1 + slack / 100.0):
            grew.append('%s +%.0f ms' % (name, n - r))
    rl = min(t['load'] for t in R)
    nl = statistics.median(t['load'] for t in N)
    print('%-46s %12.0f %12.0f %+8.0f  (%.2fx)' % ('LOAD (until the window can paint)', rl, nl, nl - rl, nl / rl if rl else 0))
    if grew:
        print('stages that grew: ' + '; '.join(grew))
    ok = True
    if bad:
        ok = False
        print('COUNTS FAIL (not the same load): ' + ' | '.join(bad[:6]))
    else:
        print('COUNTS PASS (%s)' % ', '.join('%s %s' % (k, base.get(k)) for k in COUNTS[:1] + COUNTS[3:7]))
    if nl > rl * (1 + slack / 100.0):
        ok = False
        print('LOAD FAIL: median new %.1f s is slower than the fastest reference %.1f s by more than %.0f %%' % (
            nl / 1000.0, rl / 1000.0, slack))
    else:
        print('LOAD PASS: median new %.1f s vs fastest reference %.1f s' % (nl / 1000.0, rl / 1000.0))
    print('CELL LOAD GATE %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
