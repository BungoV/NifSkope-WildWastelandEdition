"""AUDIT1 step 3: decode every .lodj of a tree with tests/spells/lodj_read.py
and cross-check the cache against the .lodi it was written beside.

The INCR1 invariant this measures is not "the cache parses". It is that the
cache is a faithful record of the same bake: every chunk's own header names the
chunk its filename names, the `end` trailer agrees with the three section
counts, and the placements summed over the chunks equal the instance count the
.lodi carries. A cache that parses but counts something else would replay a
different bake and the byte-identity of a null incremental would be luck.

usage: python lodj_sweep.py <bake tree>
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SPELLS = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'tests', 'spells')
sys.path.insert(0, SPELLS)
import lodj_read                                        # noqa: E402

FAILS = [0]
NAME = re.compile(r'\.(-?\d+)\.(-?\d+)\.lodj$')


def check(name, cond, note=''):
    if cond:
        print('  ok   %s%s' % (name, (' ' + note) if note else ''))
    else:
        FAILS[0] += 1
        print('  FAIL %s%s' % (name, (' ' + note) if note else ''))


def lodi_instance_count(path):
    """0x58, and it is read through the decoder's own header parse rather than
    from a remembered offset. The first draft of this reader hard-coded 0x80 and
    reported 98,304 instances against a cache of 3,526 -- a FAIL that was mine,
    not the product's. An offset copied out of a note is a guess; the decoder
    beside it is the contract."""
    import lodgen_native_decode as D
    return D.read_lodi(path)['header']['instanceCount']


def refute(tree):
    """Copy the tree's caches, drop ONE placement row from one of them, and
    require the sweep to notice. Three of the five checks are structural and
    would pass on the damaged file; J2 (the `end` trailer) and J4 (the total
    against the .lodi) are the two that must not."""
    import shutil
    import tempfile
    t = tempfile.mkdtemp()
    try:
        dst = os.path.join(t, 'tree')
        shutil.copytree(tree, dst)
        victim = None
        for root, _, names in os.walk(dst):
            for n in sorted(names):
                if n.endswith('.lodj'):
                    d = open(os.path.join(root, n), 'rb').read().decode('utf-8')
                    if d.count('\np\t') > 1:
                        victim = os.path.join(root, n)
                        break
            if victim:
                break
        lines = open(victim, 'rb').read().decode('utf-8').split('\n')
        for i, l in enumerate(lines):
            if l.startswith('p' + chr(9)):
                del lines[i]
                break
        open(victim, 'wb').write('\n'.join(lines).encode('utf-8'))
        print('  refuter: one placement row dropped from %s'
              % os.path.basename(victim))
        FAILS[0] = 0
        rc = sweep(dst)
        print('  refuter: the sweep went %s'
              % ('RED -- caught' if rc else 'GREEN -- NOT CAUGHT'))
        return 0 if rc else 1
    finally:
        shutil.rmtree(t, ignore_errors=True)


def main():
    if sys.argv[1] == '--refute':
        return refute(sys.argv[2])
    return sweep(sys.argv[1])


def sweep(tree):
    files, lodi = [], None
    for root, _, names in os.walk(tree):
        for n in sorted(names):
            if n.endswith('.lodj'):
                files.append(os.path.join(root, n))
            elif n.endswith('.lodi'):
                lodi = os.path.join(root, n)
    check('J0 the tree has .lodj files to read', bool(files),
          '(%d)' % len(files))
    if not files:
        return 1

    total = 0
    badName = badEnd = badLit = 0
    for f in files:
        c = lodj_read.read(f)
        m = NAME.search(os.path.basename(f))
        if not m or int(m.group(1)) != c.cx or int(m.group(2)) != c.cy:
            badName += 1
        n = len(c.placements)
        total += n
        if not c.agrees():
            badEnd += 1
        # every lit row must name an objectIndex some placement carries
        idx = set(p['objectIndex'] for p in c.placements)
        if any(l['objectIndex'] not in idx for l in c.lit):
            badLit += 1

    check('J1 every .lodj header names the chunk its filename names',
          badName == 0, '(%d file(s), %d disagreed)' % (len(files), badName))
    check('J2 every `end` trailer equals the three section counts',
          badEnd == 0, '(%d disagreed)' % badEnd)
    check('J3 every lighting row names an objectIndex the chunk placed',
          badLit == 0, '(%d chunk(s) broke it)' % badLit)

    if lodi:
        want = lodi_instance_count(lodi)
        check('J4 the placements summed over the chunks equal the .lodi '
              'instance count', total == want,
              '(cache %d, .lodi %d)' % (total, want))
    print('%d checks, %d failures' % (4 + (1 if lodi else 0), FAILS[0]))
    return 1 if FAILS[0] else 0


if __name__ == '__main__':
    sys.exit(main())
