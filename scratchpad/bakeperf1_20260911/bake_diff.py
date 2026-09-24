"""BAKEPERF1 gate P2/P4: WHOLE-REGION BYTE IDENTITY between two bake output trees.

    python bake_diff.py <dirA> <dirB> [--label TEXT]

Every file under each tree except the run's own log is hashed; the two sets are
compared by relative path AND by content. The verdict prints the file COUNT, so
a comparison over an empty tree reads as the red it is rather than as a pass.

Exit 0 only when: both sides hold the same non-empty set of paths and every
pair is byte-identical.
"""
import hashlib
import os
import sys

SKIP = {'bake.log', 'bake.log.err'}


def tree(root):
    out = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            if name in SKIP:
                continue
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace('\\', '/')
            with open(full, 'rb') as f:
                h = hashlib.sha256()
                while True:
                    b = f.read(1 << 20)
                    if not b:
                        break
                    h.update(b)
            out[rel] = (os.path.getsize(full), h.hexdigest())
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    label = ''
    if '--label' in sys.argv:
        label = sys.argv[sys.argv.index('--label') + 1]
    ta, tb = tree(a), tree(b)
    onlyA = sorted(set(ta) - set(tb))
    onlyB = sorted(set(tb) - set(ta))
    both = sorted(set(ta) & set(tb))
    differ = [p for p in both if ta[p] != tb[p]]

    print('IDENTITY %s' % (label or '%s vs %s' % (a, b)))
    print('  A: %d file(s)   B: %d file(s)   compared: %d' % (len(ta), len(tb), len(both)))
    total = sum(v[0] for v in ta.values())
    print('  A total bytes: %d' % total)
    for p in onlyA[:20]:
        print('  ONLY-IN-A %s' % p)
    for p in onlyB[:20]:
        print('  ONLY-IN-B %s' % p)
    for p in differ[:20]:
        print('  DIFFER    %s  (%d/%s vs %d/%s)' % (p, ta[p][0], ta[p][1][:12], tb[p][0], tb[p][1][:12]))

    # THE FLOOR: a comparison over nothing is not a pass
    if not both:
        print('  RESULT FAIL - nothing was compared (the floor)')
        return 2
    if onlyA or onlyB or differ:
        print('  RESULT FAIL - %d only-in-A, %d only-in-B, %d differ'
              % (len(onlyA), len(onlyB), len(differ)))
        return 1
    print('  RESULT PASS - %d file(s), %d bytes, byte-identical' % (len(both), total))
    return 0


if __name__ == '__main__':
    sys.exit(main())
