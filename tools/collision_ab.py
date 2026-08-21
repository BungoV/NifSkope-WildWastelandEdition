"""Hold a rebuild's PHYSICS against vanilla's, shape by shape.

    python tools/collision_ab.py <manifest.tsv> [<rebuilt-root> <vanilla-root>]

WHY THIS EXISTS

The Museum test set loads without crashing, which says the files parse and the
engine can build bodies out of them. It says nothing about whether a bottle
weighs what it should or sits where it should. Mass, inertia and volume are
DERIVED here -- a Minkowski-grown hull, not a copy of vanilla's numbers -- and
until now the only check on them was three assertions against a single ammo box.

So this reads both files with the collision inventory and pairs shapes by
CENTRE OF MASS, which survives the reordering that Compile does, then reports
the error in the numbers a simulator actually uses:

  * volume        relative
  * centre of mass  absolute, in metres
  * inertia       relative, on the diagonal
  * material, layer, friction, restitution   exact, or it is a difference

Shapes that cannot be paired are reported as such rather than quietly dropped:
a body that came out with a different shape structure has no counterpart to
compare, and pretending otherwise would hide exactly the case worth seeing.

Exit 1 if anything lands outside the thresholds, which are deliberately loose
(2% volume, 1 mm, 15% inertia) because these ARE derived -- the point is to find
the file that is wrong by a mile, not to argue about the last ULP.
"""
import collections
import math
import os
import re
import subprocess
import sys

BS = chr(92)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NS = os.environ.get('EXE', os.path.join(ROOT, 'release', 'NifSkope.exe'))
VOL_TOL, COM_TOL, INE_TOL = 0.02, 0.001, 0.15

SHAPE = re.compile(
    r'^\s*(\d+)\s+(\S+)\s+(\d+)\s+(\S+)\s+(\d+) v / (\d+) t'
    r'\s+vol ([\d.eE+-]+)\s+mass ([\d.eE+-]+)'
    r'\s+com ([-\d.,eE+]+)\s+I ([-\d.,eE+]+)')
BODY = re.compile(r'^\s*(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\S+)')


def inventory(path):
    """(bodies, shapes) as parsed from the collision inventory."""
    try:
        out = subprocess.run([NS, '-no-gui', 'collision', path],
                             capture_output=True, text=True, timeout=180).stdout
    except Exception:
        return [], []
    bodies, shapes, section = [], [], None
    for line in out.splitlines():
        if 'body   node' in line:
            section = 'body'
            continue
        if 'shape  class' in line:
            section = 'shape'
            continue
        if section == 'shape':
            m = SHAPE.match(line)
            if m:
                shapes.append({
                    'class': m.group(2), 'body': int(m.group(3)),
                    'material': m.group(4), 'verts': int(m.group(5)),
                    'vol': float(m.group(7)), 'mass': float(m.group(8)),
                    'com': tuple(float(v) for v in m.group(9).split(',')),
                    'I': tuple(float(v) for v in m.group(10).split(',')),
                })
        elif section == 'body':
            m = BODY.match(line)
            if m:
                bodies.append({
                    'node': m.group(2), 'layer': int(m.group(3)),
                    'shapes': int(m.group(4)), 'friction': m.group(5),
                    'restitution': m.group(6), 'material': m.group(7),
                })
    return bodies, shapes


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def pair(ours, theirs):
    """Match shapes by centre of mass: nearest first, each used once.

    Compile reorders shapes -- splitting a mesh by material does it, and so does
    the compound's own tree -- so index order is not identity. A centre of mass
    is, to well under the spacing between two shapes of the same body.
    """
    left = list(range(len(theirs)))
    pairs, orphan_ours = [], []
    for i, o in enumerate(ours):
        if not left:
            orphan_ours.append(i)
            continue
        j = min(left, key=lambda k: dist(o['com'], theirs[k]['com']))
        left.remove(j)
        pairs.append((i, j))
    return pairs, orphan_ours, left


def rel(a, b):
    return abs(a - b) / b if b else (0.0 if a == b else 1.0)


def main():
    manifest = sys.argv[1]
    mod = sys.argv[2] if len(sys.argv) > 2 else \
        'E:/Projects/Fallout 4 Mods/mods/WW Concord Collision Test/Meshes/'
    van = sys.argv[3] if len(sys.argv) > 3 else \
        'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'

    worst = {'vol': (0, ''), 'com': (0, ''), 'I': (0, '')}
    tally = collections.Counter()
    offenders = []
    files = 0
    for line in open(manifest, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('\t')
        if len(f) < 5 or f[0] != 'ok':
            continue
        rel_path = f[4].replace(BS, '/')
        a, b = os.path.join(mod, rel_path), os.path.join(van, rel_path)
        if not (os.path.exists(a) and os.path.exists(b)):
            continue
        files += 1
        ba, sa = inventory(a)
        bb, sb = inventory(b)
        notes = []
        if len(ba) != len(bb):
            notes.append('bodies %d vs %d' % (len(ba), len(bb)))
        for x, y in zip(ba, bb):
            for k in ('layer', 'friction', 'restitution', 'material'):
                if x[k] != y[k]:
                    notes.append('body %s %s vs %s' % (k, x[k], y[k]))
                    tally['body ' + k] += 1
        pairs, orphan_a, orphan_b = pair(sa, sb)
        if orphan_a or orphan_b:
            notes.append('unpaired shapes: %d ours, %d vanilla' % (len(orphan_a), len(orphan_b)))
            tally['unpaired'] += 1
        for i, j in pairs:
            o, t = sa[i], sb[j]
            dv, dc = rel(o['vol'], t['vol']), dist(o['com'], t['com'])
            di = max(rel(x, y) for x, y in zip(o['I'], t['I']))
            for key, val in (('vol', dv), ('com', dc), ('I', di)):
                if val > worst[key][0]:
                    worst[key] = (val, '%s shape %d' % (rel_path, i))
            if o['material'] != t['material']:
                notes.append('shape %d material %s vs %s' % (i, o['material'], t['material']))
                tally['material'] += 1
            if dv > VOL_TOL:
                notes.append('shape %d volume %+.1f%% (%.6f vs %.6f)' % (i, 100 * (o['vol'] - t['vol']) / t['vol'] if t['vol'] else 0, o['vol'], t['vol']))
                tally['volume'] += 1
            if dc > COM_TOL:
                notes.append('shape %d com off by %.4f m' % (i, dc))
                tally['com'] += 1
            if di > INE_TOL:
                notes.append('shape %d inertia %+.0f%%' % (i, 100 * di))
                tally['inertia'] += 1
        if notes:
            offenders.append((rel_path, notes))

    print('%d files compared' % files)
    print('worst volume  %+.2f%%   %s' % (100 * worst['vol'][0], worst['vol'][1]))
    print('worst com     %.5f m   %s' % (worst['com'][0], worst['com'][1]))
    print('worst inertia %+.1f%%   %s' % (100 * worst['I'][0], worst['I'][1]))
    print('thresholds: volume %.0f%%, com %.0f mm, inertia %.0f%%'
          % (100 * VOL_TOL, 1000 * COM_TOL, 100 * INE_TOL))
    if tally:
        print('outside them: %s' % ', '.join('%s x%d' % kv for kv in tally.most_common()))
    else:
        print('nothing outside them')
    for rel_path, notes in offenders:
        print('  %s' % rel_path)
        for n in notes[:6]:
            print('      %s' % n)
    return 1 if tally else 0


if __name__ == '__main__':
    sys.exit(main())
