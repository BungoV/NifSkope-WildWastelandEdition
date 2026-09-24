"""LAND1 B4 -- the ledger is deterministic, checked against the bytes.

    python b4_ledger.py <out/b3>

Gate B3's null arm already proves determinism the hard way: base and full are
two INDEPENDENT full bakes of the same tree, minutes apart, and their whole
output directories compare byte-identical INCLUDING `Commonwealth.lodb`. That is
the load-bearing evidence, because it is the property the whole gate rests on --
if the ledger carried a timestamp or a thread id, B3 would have had to except it
from the comparison, and an exception is exactly where a bug would live.

This script states the rest of the claim explicitly rather than leaving it
implied, by opening the file:
  * the 16-byte header is LODB / version 1 / jsonLen / reserved-zero;
  * jsonLen agrees with what is actually there;
  * the chunk rows are sorted by (cy, cx) -- not by the order the pass retired
    them, which is thread-order and would differ run to run;
  * no row carries an absolute path, and no output path escapes the ledger's
    own directory except by the one documented `../tex/` hop;
  * the two independent bakes' ledgers are equal byte for byte.
"""
import json
import os
import struct
import sys

MAGIC = 0x42444F4C


def read(path):
    b = open(path, 'rb').read()
    magic, ver, jlen, res = struct.unpack_from('<IIII', b, 0)
    return b, magic, ver, jlen, res, json.loads(b[16:16 + jlen].decode('utf-8'))


def check(path, fails):
    def bad(msg):
        print('    FAIL  %s' % msg)
        fails.append(msg)

    b, magic, ver, jlen, res, j = read(path)
    print('  %s (%d bytes)' % (os.path.relpath(path), len(b)))
    if magic != MAGIC:
        bad('magic is 0x%08X, not LODB' % magic)
    if ver != 1:
        bad('version is %d, not 1' % ver)
    if res != 0:
        bad('reserved word is %d, not 0' % res)
    if 16 + jlen != len(b):
        bad('jsonLen %d + 16 != file size %d' % (jlen, len(b)))

    rows = j.get('chunks', [])
    keys = [(r['cy'], r['cx']) for r in rows]
    if keys != sorted(keys):
        bad('chunk rows are not sorted by (cy, cx)')
    else:
        print('    %d chunk row(s), sorted by (cy, cx)' % len(rows))

    nout = 0
    for r in rows:
        for o in r.get('out', []):
            nout += 1
            p = o.split(' ')[0]
            if ':' in p or p.startswith('/'):
                bad('absolute output path in the ledger: %s' % p)
            if p.startswith('..') and not p.startswith('../tex/'):
                bad('output path escapes the ledger directory: %s' % p)
    print('    %d output row(s), every path relative' % nout)

    for k in ('worldspace', 'worldEdid', 'dim', 'region', 'switches', 'loadOrder'):
        if k not in j:
            bad('header field %s is missing' % k)
    low = json.dumps(j).lower()
    for word in ('time', 'date', 'thread'):
        if '"%s"' % word in low:
            bad('a %s-looking field is in the ledger' % word)
    return b


def main(argv):
    root = argv[0]
    fails = []
    print('LAND1 B4 -- ledger determinism')
    pairs = 0
    for tag in sorted(os.listdir(root)):
        base = os.path.join(root, tag, 'base', 'obj', 'Commonwealth.lodb')
        full = os.path.join(root, tag, 'null', 'full', 'obj', 'Commonwealth.lodb')
        if not os.path.exists(base):
            continue
        b1 = check(base, fails)
        if os.path.exists(full):
            b2 = check(full, fails)
            pairs += 1
            if b1 == b2:
                print('    EQUAL  two independent full bakes, byte for byte (%d bytes)'
                      % len(b1))
            else:
                print('    FAIL   the two independent full bakes differ')
                fails.append('%s: independent full bakes differ' % tag)
    if not pairs:
        print('  VACUOUS: no null-arm full bake to compare against')
        fails.append('no independent pair')
    print('')
    print('B4: %d failure(s)' % len(fails))
    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
