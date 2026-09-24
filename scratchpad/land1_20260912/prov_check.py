"""Re-find every LAND1 provenance anchor in today's sources and re-take the hashes.

The block was stamped against the Part A exe; Part B then edited two of the three
files, so every line number in it is a claim about a file that has since moved.
An anchor that still matches at a NEW line is a line number to correct.  An
anchor that matches nowhere, or twice, is a claim to re-examine by hand -- which
is the whole point of stamping anchor TEXT beside the number rather than the
number alone.
"""
import hashlib
import io
import os
import re
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
DOC = os.path.join(ROOT, 'docs/LODGEN_TERRAIN_VT.md')

SRC = ['src/lodgen.cpp', 'src/lodgen.h', 'src/nifcli.cpp']


def lines_of(rel):
    b = open(os.path.join(ROOT, rel), 'rb').read()
    return b.decode('utf-8', 'replace').split('\n')


def main():
    doc = open(DOC, encoding='utf-8').read().split('\n')
    # the LAND1 block runs from its heading to the next '### ' or EOF
    start = None
    for i, ln in enumerate(doc):
        if ln.startswith('### LAND1, 2026-09-12'):
            start = i
            break
    if start is None:
        print('LAND1 block not found')
        return 1
    end = len(doc)
    for i in range(start + 1, len(doc)):
        if doc[i].startswith('### '):
            end = i
            break
    print('LAND1 block: doc lines %d..%d' % (start + 1, end))

    cache = {}
    for rel in SRC:
        cache[rel] = lines_of(rel)

    row = re.compile(r'^\| (.+?) \| `([^`]+)` \| (.+) \|$')
    bad = 0
    for i in range(start, end):
        m = row.match(doc[i])
        if not m:
            continue
        claim, loc, anchors = m.groups()
        if ':' not in loc:
            continue
        fname, nums = loc.split(':', 1)
        rel = 'src/' + fname
        if rel not in cache:
            continue
        nums = [int(x) for x in re.findall(r'\d+', nums)]
        # anchors: one or more `...` separated by ' / '
        texts = re.findall(r'`([^`]*)`', anchors)
        texts = [t.replace('\\|', '|') for t in texts]
        if not texts:
            continue
        found = []
        for t in texts:
            hits = [k + 1 for k, ln in enumerate(cache[rel]) if t in ln]
            found.append(hits)
        flat = []
        okall = True
        for t, hits, want in zip(texts, found, nums + [None] * 8):
            if len(hits) == 1:
                flat.append(hits[0])
                if want is not None and hits[0] != want:
                    okall = False
            elif len(hits) == 0:
                flat.append(None)
                okall = False
            else:
                # more than one hit: prefer the one nearest the stated number
                if want is not None:
                    near = min(hits, key=lambda h: abs(h - want))
                    flat.append(near)
                else:
                    flat.append(hits[0])
                okall = False
        status = 'OK ' if okall and len(flat) == len(nums) else 'MOVED'
        if status != 'OK ':
            bad += 1
        print('%s %-14s stated %-14s found %-20s %s'
              % (status, fname, ','.join(str(n) for n in nums),
                 ','.join('?' if f is None else str(f) for f in flat),
                 claim[:52]))
    print('')
    print('| file | sha256 (16) | bytes | lines |')
    print('|---|---|---|---|')
    for rel in SRC:
        b = open(os.path.join(ROOT, rel), 'rb').read()
        n = b.count(b'\n')
        print('| `%s` | `%s` | %s | %d |'
              % (rel, hashlib.sha256(b).hexdigest()[:16],
                 '{:,}'.format(len(b)), n))
    print('')
    print('%d row(s) need a new line number' % bad)
    return 0


if __name__ == '__main__':
    sys.exit(main())
