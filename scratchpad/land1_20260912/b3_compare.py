"""LAND1 gate B3 -- the comparison half.

    python b3_compare.py <base> <full> <incr> <label>

<full> is a full bake of the edited inputs; <incr> is a dirty-chunk rebake on
top of a copy of <base>.  The gate is `incr == full`, every file, every byte.

<base> is here for the FLOOR.  `incr == full` is trivially true for an edit
that changed nothing, so the arm is only meaningful when `base != full` -- when
the edit actually moved the full bake's bytes.  An arm whose floor is empty is
printed VACUOUS, never PASS: a check that cannot fail on its input is not a
check, and this one would have passed on a plugin edit that reached no chunk at
all.
"""
import hashlib
import os
import sys


def tree(root):
    out = {}
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            p = os.path.join(dirpath, f)
            rel = os.path.relpath(p, root).replace('\\', '/')
            if rel.endswith('bake.log'):
                continue          # the log is not an artefact of the bake
            with open(p, 'rb') as fh:
                out[rel] = (os.path.getsize(p),
                            hashlib.sha1(fh.read()).hexdigest())
    return out


def diff(a, b):
    onlyA = sorted(set(a) - set(b))
    onlyB = sorted(set(b) - set(a))
    changed = sorted(k for k in set(a) & set(b) if a[k] != b[k])
    return onlyA, onlyB, changed


def main(argv):
    base, full, incr, label = argv[:4]
    # edit = an input was changed, so the full bake MUST have moved.
    # null = nothing was changed, so it must NOT have moved and the
    #        incremental run must rebake nothing yet still match.
    # lost = an OUTPUT was deleted from the copy; the inputs are untouched, so
    #        the full bake is unmoved and what is on trial is whether the
    #        rebake puts the missing file back byte for byte.
    mode = argv[4] if len(argv) > 4 else 'edit'
    B, F, I = tree(base), tree(full), tree(incr)

    fOnly, bOnly, fChanged = diff(B, F)
    floor = len(fOnly) + len(bOnly) + len(fChanged)

    onlyF, onlyI, changed = diff(F, I)
    ident = len(onlyF) + len(onlyI) + len(changed) == 0

    print('    files: base %d, full %d, incr %d' % (len(B), len(F), len(I)))
    print('    floor: the edit moved %d file(s) of the full bake' % floor)
    for k in fChanged[:6]:
        print('           %s  %s -> %s' % (k, B[k][1][:10], F[k][1][:10]))
    if floor and (fOnly or bOnly):
        print('           (%d only in base, %d only in full)' % (len(fOnly), len(bOnly)))

    if not ident:
        print('    DIFFERS: %d only in full, %d only in incr, %d changed'
              % (len(onlyF), len(onlyI), len(changed)))
        for k in (onlyF + onlyI + changed)[:12]:
            print('           %s' % k)

    want = (floor > 0) if mode == 'edit' else (floor == 0)
    if not want:
        verdict = ('VACUOUS (the edit changed nothing; identity proves nothing)'
                   if mode == 'edit'
                   else 'BROKEN CONTROL (nothing was edited, yet the full bake moved)')
    elif ident:
        verdict = 'PASS'
    else:
        verdict = 'FAIL'
    print('    B3 %-16s %s' % (label, verdict))
    return 0 if (ident and want) else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
