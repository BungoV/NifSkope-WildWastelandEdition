"""Compare two bake trees byte for byte, the bake record NORMALISED.

    python incr_compare.py <full-dir> <incr-dir> [--base <base-dir>] [--tag NAME]

Prints one verdict block:

    files           N in full, M in incr, K only in one
    differ          the list, with the first differing offset
    record          the .lodb, normalised by tests/spells/lodb_read.py, line diff
    floor           when --base is given: how many files the edit MOVED between
                    base and full.  An empty floor means the arm is VACUOUS.

Exit code 0 when incr == full (record normalised) AND, with --base, the floor is
non-empty.  1 otherwise.  The record is compared with the ONE reader in the tree
(lane BAKEREC1's rule), never with a parser written here.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))

import lodb_read  # noqa: E402


def walk(root):
    out = {}
    # THE HARNESS'S OWN DROPPINGS ARE NOT OUTPUTS. The bake writes its stdout
    # beside the tree as bake.log; it carries wall-clock seconds and a peak
    # working set, so it can never be byte-equal across two runs and it made
    # every comparison report one guaranteed difference that meant nothing.
    SKIP = ('bake.log',)
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            if fn in SKIP:
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, root).replace(os.sep, '/')
            out[rel] = p
    return out


def sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as fh:
        for blk in iter(lambda: fh.read(1 << 20), b''):
            h.update(blk)
    return h.hexdigest()


def first_diff(a, b):
    with open(a, 'rb') as fa, open(b, 'rb') as fb:
        A, B = fa.read(), fb.read()
    n = min(len(A), len(B))
    for i in range(n):
        if A[i] != B[i]:
            return i, len(A), len(B)
    return n, len(A), len(B)


def record_of(files):
    for rel in files:
        if rel.endswith('.lodb'):
            return rel
    return None


def norm_lines(path):
    return lodb_read.normalise_file(path).split(chr(10))


def main(argv):
    full = argv[0]
    incr = argv[1]
    base = None
    tag = 'arm'
    i = 2
    while i < len(argv):
        if argv[i] == '--base':
            base = argv[i + 1]
            i += 2
        elif argv[i] == '--tag':
            tag = argv[i + 1]
            i += 2
        else:
            i += 1

    F, I = walk(full), walk(incr)
    onlyF = sorted(set(F) - set(I))
    onlyI = sorted(set(I) - set(F))
    both = sorted(set(F) & set(I))

    differ = []
    rec_rel = record_of(F) or record_of(I)
    rec_lines = []
    for rel in both:
        if rel == rec_rel:
            a, b = norm_lines(F[rel]), norm_lines(I[rel])
            if a != b:
                differ.append(rel + ' (record, NORMALISED)')
                for k in range(max(len(a), len(b))):
                    la = a[k] if k < len(a) else '<eof>'
                    lb = b[k] if k < len(b) else '<eof>'
                    if la != lb:
                        rec_lines.append('      full: ' + la)
                        rec_lines.append('      incr: ' + lb)
                        if len(rec_lines) >= 40:
                            rec_lines.append('      ... truncated')
                            break
            continue
        if sha1(F[rel]) != sha1(I[rel]):
            off, la, lb = first_diff(F[rel], I[rel])
            differ.append('%s (first differing byte %d; %d vs %d bytes)' % (rel, off, la, lb))

    floor = []
    if base:
        B = walk(base)
        for rel in sorted(set(F) & set(B)):
            if rel == rec_rel:
                continue
            if sha1(F[rel]) != sha1(B[rel]):
                floor.append(rel)
        for rel in sorted(set(F) ^ set(B)):
            if rel != rec_rel:
                floor.append(rel + ' (only one side)')

    print('  [%s] files: full %d, incr %d' % (tag, len(F), len(I)))
    if onlyF:
        print('    only in full (%d): %s' % (len(onlyF), ', '.join(onlyF[:8])))
    if onlyI:
        print('    only in incr (%d): %s' % (len(onlyI), ', '.join(onlyI[:8])))
    if differ:
        print('    DIFFER (%d):' % len(differ))
        for d in differ[:12]:
            print('      ' + d)
        for line in rec_lines:
            print(line)
    else:
        print('    every file byte-identical (record normalised)')
    if base is not None:
        print('    floor: the edit moved %d file(s) of the full bake%s'
              % (len(floor), (': ' + ', '.join(floor[:6])) if floor else ''))

    ok = not differ and not onlyF and not onlyI
    if base is not None and not floor:
        print('    VACUOUS: the edit moved nothing, so identity proves nothing')
        ok = False
    print('    %s %s' % ('PASS' if ok else 'FAIL', tag))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
