"""AUDIT1 step 2: turn bake_runner.log and the bake trees into the report's
section 2 tables.  Reads only; every number comes off disk or out of the log
the run itself wrote.
"""
import os
import re
import sys

BASE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916'
LOG = os.path.join(BASE, 'bake_runner.log')
BAKE = os.path.join(BASE, 'bake')

ORDER = ['sanctuary_fo4cs', 'coast_fo4cs', 'urban_fo4cs',
         'sanctuary_keepbto', 'coast_keepbto', 'urban_keepbto',
         'sanctuary_stock', 'coast_stock', 'urban_stock',
         'sanctuary_incr']


def runs():
    """{name: {wall, peak, census, stages, native, cache, librarybuild}}"""
    out = {}
    cur = None
    for line in open(LOG, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        m = re.match(r'^=== (\S+) (.+)$', line)
        if m:
            cur = m.group(1)
            out.setdefault(cur, {'start': m.group(2)})
            continue
        if cur is None:
            continue
        d = out[cur]
        m = re.match(r'^(\S+) rc=(\d+) wall=(\d+)s peak=.(\d+) (\d+).', line)
        if m and m.group(1) == cur:
            d['rc'], d['wall'], d['peak'], d['samples'] = m.group(2), m.group(3), m.group(4), m.group(5)
        elif line.startswith('bake census:'):
            d['census'] = line
        elif line.startswith('stage times:'):
            d['stages'] = line
        elif line.startswith('native:'):
            d['native'] = line
        elif line.startswith('native cache:'):
            d['cache'] = line
        elif line.startswith('native-library-build:'):
            d['lib'] = line
        elif line.startswith('bake-record:'):
            d['record'] = line
    return out


def longpole(stages):
    m = re.search(r'landscape ([\d.]+) s, meshes ([\d.]+) s, textures ([\d.]+) s, impostors ([\d.]+) s', stages or '')
    if not m:
        return '', ''
    vals = [('landscape', float(m.group(1))), ('meshes', float(m.group(2))),
            ('textures', float(m.group(3))), ('impostors', float(m.group(4)))]
    pole = max(vals, key=lambda v: v[1])
    return ', '.join('%s %.1f' % v for v in vals), '%s (%.1f s)' % pole


def files(name):
    root = os.path.join(BAKE, name)
    got = []
    for dirpath, _, fns in os.walk(root):
        for fn in fns:
            p = os.path.join(dirpath, fn)
            got.append((os.path.relpath(p, root).replace(os.sep, '/'), os.path.getsize(p)))
    return sorted(got)


def main():
    r = runs()
    print('| bake | wall | peak working set (sampler) | samples | files | bytes |')
    print('|---|---|---|---|---|---|')
    for n in ORDER:
        d = r.get(n)
        if not d:
            continue
        fs = files(n)
        print('| `%s` | %s s | %s B (%.2f GB) | %s | %d | %d |'
              % (n, d.get('wall', '?'), d.get('peak', '?'),
                 int(d.get('peak', 0)) / 1e9, d.get('samples', '?'), len(fs),
                 sum(s for _, s in fs)))
    print('')
    print('| bake | stage times (s) | long pole |')
    print('|---|---|---|')
    for n in ORDER:
        d = r.get(n)
        if not d:
            continue
        s, pole = longpole(d.get('stages'))
        print('| `%s` | %s | %s |' % (n, s or 'no stage line', pole or '--'))
    print('')
    for n in ORDER:
        d = r.get(n)
        if not d:
            continue
        print('**`%s`**  census, verbatim:' % n)
        print('')
        print('```')
        print(d.get('census', '(no census line)'))
        print('```')
        print('')
    print('### Every output file of the three default FO4CS bakes')
    print('')
    for n in ['sanctuary_fo4cs', 'coast_fo4cs', 'urban_fo4cs']:
        fs = files(n)
        lay = [f for f in fs if f[0].startswith('FO4CSLOD/')]
        rest = [f for f in fs if not f[0].startswith('FO4CSLOD/')]
        print('`%s` -- %d files, %d bytes; %d under `FO4CSLOD/`:' % (n, len(fs), sum(s for _, s in fs), len(lay)))
        print('')
        print('| file (under `FO4CSLOD/Commonwealth/`) | bytes |')
        print('|---|---|')
        for p, s in lay:
            print('| `%s` | %d |' % (p.split('/')[-1], s))
        klass = {}
        for p, s in rest:
            k = re.sub(r'\.-?\d+\.-?\d+', '.<x>.<y>', p)
            k = re.sub(r'^tex/', 'tex/', k)
            klass.setdefault(k, []).append(s)
        print('')
        print('| outside the layout (class) | files | bytes | distinct sizes |')
        print('|---|---|---|---|')
        for k in sorted(klass):
            v = klass[k]
            print('| `%s` | %d | %d | %s |' % (k, len(v), sum(v), ', '.join(str(x) for x in sorted(set(v)))))
        print('')
    return 0


if __name__ == '__main__':
    sys.exit(main())
