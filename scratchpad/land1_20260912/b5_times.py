"""LAND1 B5 -- what the incremental path actually costs, from the gate's own logs.

    python b5_times.py <out/b3>

No extra bakes: gate B3 already ran a full bake and a dirty rebake of the same
region for every arm, so the wall-clock comparison is sitting in the logs it
wrote.  Reported as a RATIO of chunks as well as of seconds, because seconds are
a property of this machine and the ratio is a property of the feature.

The ledger's own cost is the honest part of the answer: every ordinary bake now
pays for one input digest per chunk whether or not anybody ever runs
--incremental, so the base bake's time is the price of admission.
"""
import os
import re
import sys

STAGE = re.compile(r'^stage times: landscape ([\d.]+) s, meshes ([\d.]+) s, '
                   r'textures ([\d.]+) s, impostors ([\d.]+) s')
CENSUS = re.compile(r'^incremental: (\d+) of (\d+) chunks dirty')
LEDGER = re.compile(r'^ledger: (\d+) chunk row')


def read(path):
    t = None
    dirty = None
    rows = None
    if not os.path.exists(path):
        return None
    for line in open(path, encoding='utf-8', errors='replace'):
        m = STAGE.match(line)
        if m:
            t = sum(float(x) for x in m.groups())
        m = CENSUS.match(line)
        if m:
            dirty = (int(m.group(1)), int(m.group(2)))
        m = LEDGER.match(line)
        if m:
            rows = int(m.group(1))
    return {'secs': t, 'dirty': dirty, 'rows': rows}


def main(argv):
    root = argv[0]
    print('%-18s %-9s %-9s %-11s %s'
          % ('arm', 'full s', 'incr s', 'chunks', 'seconds saved'))
    for tag in sorted(os.listdir(root)):
        tdir = os.path.join(root, tag)
        if not os.path.isdir(tdir) or tag == 'esm':
            continue
        base = read(os.path.join(tdir, 'base', 'bake.log'))
        if base:
            print('%-18s %-9s %-9s %-11s %s'
                  % (tag + '/base', '%.1f' % base['secs'], '-',
                     '%d rows' % (base['rows'] or 0), '(the full bake)'))
        for kind in sorted(os.listdir(tdir)):
            if kind == 'base':
                continue
            f = read(os.path.join(tdir, kind, 'full', 'bake.log'))
            i = read(os.path.join(tdir, kind, 'incr', 'bake.log'))
            if not f or not i or f['secs'] is None or i['secs'] is None:
                continue
            d = i['dirty'] or (0, 0)
            saved = f['secs'] - i['secs']
            print('%-18s %-9.1f %-9.1f %-11s %+.1f s  (%.0f %%)'
                  % (tag + '/' + kind, f['secs'], i['secs'],
                     '%d of %d' % d, saved,
                     100.0 * saved / f['secs'] if f['secs'] else 0.0))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
