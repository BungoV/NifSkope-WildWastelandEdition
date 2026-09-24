"""LAND1 gate A4 -- what the new switches do NOT move.

Arm A (the gate):  the NEW exe with no new switch, against the rung
                   release/NifSkope.before_land1.exe, on all FOURTEEN sheets of
                   the frozen split -- every file of every tile, byte for byte.
                   Both arms carry --road-detail 1, which is the shipped
                   default, so the comparison is of the sampler and nothing else.

Arm B (the floor that must fire): the comparator is shown RED on a single
                   flipped byte and on a missing file BEFORE arm A's green is
                   believed (CONSTITUTION 4).  A comparator that cannot fail is
                   not a comparator.

    python a4_gate.py  ->  logs/a4_gate.txt
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')
SEL = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
VAL = [(-24, -24), (-12, -20), (4, -24), (20, -24), (-20, 4), (-8, 4), (12, 8)]
L = []


def say(s):
    L.append(s)
    print(s)


def tree(root):
    """every file under root -> relative path : sha1, with its size."""
    out = {}
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f == 'bake.log':
                continue
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, root).replace(chr(92), '/')
            b = open(p, 'rb').read()
            out[rel] = (hashlib.sha1(b).hexdigest(), len(b))
    return out


def compare(a, b):
    """-> (nfiles, differing, only_in_a, only_in_b)"""
    ka, kb = set(a), set(b)
    diff = [k for k in sorted(ka & kb) if a[k][0] != b[k][0]]
    return len(ka & kb), diff, sorted(ka - kb), sorted(kb - ka)


def main():
    say('LAND1 gate A4 -- every new switch OFF == the rung, all fourteen sheets')
    say('rung  release/NifSkope.before_land1.exe (2026-09-12 06:31:05)')
    say('test  release/NifSkope.exe with --road-detail 1 and NO new switch')
    say('')
    tot = 0
    bad = 0
    for cx, cy in SEL + VAL:
        tag = 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
        ra = os.path.join(OUT, 'ls_rung', tag)
        rb = os.path.join(OUT, 'ls_off', tag)
        if not (os.path.isdir(ra) and os.path.isdir(rb)):
            say('  %-9s REFUSED: a tile is missing' % ('%d,%d' % (cx, cy)))
            bad += 1
            continue
        A, B = tree(ra), tree(rb)
        n, d, oa, ob = compare(A, B)
        tot += n
        if d or oa or ob:
            bad += 1
            say('  %-9s %3d files  RED: %d differ, %d only in rung, %d only in test'
                % ('%d,%d' % (cx, cy), n, len(d), len(oa), len(ob)))
            for k in d[:4]:
                say('            %s  %s vs %s' % (k, A[k][0][:12], B[k][0][:12]))
        else:
            say('  %-9s %3d files  identical' % ('%d,%d' % (cx, cy), n))
    say('')
    say('arm A: %d tiles, %d files compared, %d tiles RED' % (14, tot, bad))
    if tot < 14 * 5:
        say('REFUSED: %d files is too few for fourteen tiles -- an empty tree '
            'cannot pass this gate' % tot)
        bad += 1

    # arm B -- the floor that must fire
    say('')
    say('arm B, the floor: the comparator on a deliberately broken pair')
    tag = 'r_-20_24_-17_27'
    A = tree(os.path.join(OUT, 'ls_rung', tag))
    B = dict(A)
    k = sorted(x for x in B if x.endswith('.DDS'))[0]
    B[k] = ('0' * 40, B[k][1])                      # one flipped byte
    n, d, oa, ob = compare(A, B)
    f1 = len(d) == 1
    say('  one flipped byte in %-34s -> %d differing   %s'
        % (k, len(d), 'RED as it must be' if f1 else 'THE COMPARATOR IS BLIND'))
    B = dict(A)
    B.pop(k)
    n, d, oa, ob = compare(A, B)
    f2 = len(oa) == 1
    say('  that file removed                                   -> %d only in rung  %s'
        % (len(oa), 'RED as it must be' if f2 else 'THE COMPARATOR IS BLIND'))
    say('')
    verdict = (bad == 0) and f1 and f2
    say('A4 %s' % ('GREEN' if verdict else 'RED'))
    with open(os.path.join(HERE, 'logs', 'a4_gate.txt'), 'w', newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    return 0 if verdict else 1


if __name__ == '__main__':
    sys.exit(main())
