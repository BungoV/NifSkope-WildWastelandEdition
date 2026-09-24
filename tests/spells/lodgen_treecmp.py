#!/usr/bin/env python3
"""PERF1 -- byte identity of two bake output trees, EVERY file.

    python treecmp.py <dirA> <dirB> [--record-mask] [--subst FROM TO] ...

Walks both trees, compares the full relative path set and then every file's
SHA-256. Prints one line per difference and a verdict line. `bake.log` is the
lane's own transcript of the run, not an output of the bake, and is skipped by
name (it is written by the shell, not by the generator).

`--record-mask` compares a `.lodb` through the tree's ONE record reader
(tests/spells/lodb_read.py), so the five volatile fields are masked, and ALSO
masks exactly the RUN IDENTITY of the two arms:

  * the value line of the `--threads` / `--chunk-threads` switches, and
  * the `threads N, chunk threads N bound by W` and `chunk workers N` clauses
    of the `bake census:` line.

Those record WHAT WAS ASKED, not what came out; a gate that refused to mask
them could never compare a 1-thread arm with an 8-thread arm at all. Every
other byte of the record -- every chunk digest, every output digest, every
resource kind and order, the plugin stack, the switch digest -- still compares.

`--build-mask` additionally masks the record's HEADER line, whose last field is
the generator exe's byte size. Two different exes necessarily differ there, so
it is needed for the rung-vs-new arm and must NEVER be passed for the 1-vs-N
arm, where the exe is the same file and any difference is a defect.

`--drop-record-line PREFIX` removes every normalised record line that starts
with PREFIX from BOTH sides before hashing, and PRINTS each line it dropped so
that the concession is read rather than assumed. It exists for exactly one
shape of comparison: an old exe against a new one that writes a census line the
old exe never wrote. It is a line-set concession, not a masking one -- a
dropped line is not compared at all -- so a gate that uses it owes a separate
assertion about the line it dropped. Leg (c) of tests/spells/lodgen_perf.sh
drops `census\tnative-library:` and then asserts the new side says `rebuilt`
with the right reason, in the same leg.

`--subst FROM TO` rewrites FROM to TO inside the record text before hashing,
for the case where the two arms necessarily sit at two absolute paths. The
gate in tests/spells/lodgen_perf.sh does NOT need it: it bakes both arms into
the SAME out-dir (BAKEREC1's lesson, MISTAKES.md 2026-09-17 01:1x) and moves
the first tree aside.
"""
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tests', 'spells'))

SKIP = ('bake.log',)


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for blk in iter(lambda: fh.read(1 << 20), b''):
            h.update(blk)
    return h.hexdigest()


def record_text(path, substs, build_mask=False, drops=()):
    import lodb_read
    text = lodb_read.normalise_file(path)
    for a, b in substs:
        text = text.replace(a, b)
    out = []
    dropped = []
    prev_switch = None
    for line in text.split('\n'):
        f = line.split('\t')
        if f[0] == 'switch' and prev_switch in ('--threads', '--chunk-threads'):
            line = 'switch\t<run thread count>'
        if build_mask and f[0] == 'lodb':
            line = '	'.join(f[:-1] + ['<generator exe size>'])
        if line.startswith('census\tbake census:'):
            line = re.sub(r'threads \d+, chunk threads \d+ bound by \w+',
                          'threads <run>, chunk threads <run> bound by <run>', line)
            line = re.sub(r'chunk workers \d+', 'chunk workers <run>', line)
        prev_switch = f[1] if (f[0] == 'switch' and len(f) > 1) else None
        if any(line.startswith(d) for d in drops):
            dropped.append(line)
            continue
        out.append(line)
    for d in dropped:
        print('   DROPPED %s: %s' % (os.path.basename(path), d[:300]))
    return '\n'.join(out)


def walk(root):
    got = {}
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            if f in SKIP and os.path.dirname(os.path.relpath(
                    os.path.join(dirpath, f), root)) == '':
                continue
            p = os.path.join(dirpath, f)
            got[os.path.relpath(p, root).replace('\\', '/')] = p
    return got


def main():
    argv = sys.argv[1:]
    a, b = argv[0], argv[1]
    rest = argv[2:]
    mask = '--record-mask' in rest
    build_mask = '--build-mask' in rest
    substs = []
    drops = []
    i = 0
    while i < len(rest):
        if rest[i] == '--subst':
            substs.append((rest[i + 1], rest[i + 2]))
            i += 3
        elif rest[i] == '--drop-record-line':
            drops.append(rest[i + 1].replace('\\t', '\t'))
            i += 2
        else:
            i += 1
    A, B = walk(a), walk(b)
    diffs = 0
    for p in sorted(set(A) - set(B)):
        print('ONLY-A  %s' % p)
        diffs += 1
    for p in sorted(set(B) - set(A)):
        print('ONLY-B  %s' % p)
        diffs += 1
    same = 0
    for p in sorted(set(A) & set(B)):
        if mask and p.lower().endswith('.lodb'):
            ta = record_text(A[p], substs, build_mask, drops)
            tb = record_text(B[p], substs, build_mask, drops)
            ha = hashlib.sha256(ta.encode('utf-8')).hexdigest()
            hb = hashlib.sha256(tb.encode('utf-8')).hexdigest()
            kind = 'RECORD'
            if ha != hb:
                for x, y in zip(ta.split('\n'), tb.split('\n')):
                    if x != y:
                        print('   recA: %s' % x[:300])
                        print('   recB: %s' % y[:300])
        else:
            ha, hb = sha(A[p]), sha(B[p])
            kind = 'FILE'
        if ha != hb:
            print('DIFFER  %s (%s)  %s != %s' % (p, kind, ha[:16], hb[:16]))
            diffs += 1
        else:
            same += 1
    print('COMPARED %d file(s); %d identical; %d difference(s)'
          % (len(set(A) | set(B)), same, diffs))
    print('VERDICT %s' % ('IDENTICAL' if diffs == 0 else 'DIFFER'))
    return 0 if diffs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
