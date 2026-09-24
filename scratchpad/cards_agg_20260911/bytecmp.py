#!/usr/bin/env python3
"""Byte-identity comparator for gate A5, with its own RED control.

Compares two bake trees file by file: every file in either tree, by relative
path, by size and by content. The comparator is SHOWN RED FIRST -- on one
flipped byte and on one missing file -- because a comparator that has never
failed is not a comparator (CONSTITUTION 4).
"""

import hashlib
import os
import shutil
import sys
import tempfile


def walk(root):
    out = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for f in filenames:
            p = os.path.join(dirpath, f)
            rel = os.path.relpath(p, root).replace('\\', '/')
            out[rel] = p
    return out


def digest(p):
    h = hashlib.sha256()
    with open(p, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def compare(a, b, label):
    A, B = walk(a), walk(b)
    only_a = sorted(set(A) - set(B))
    only_b = sorted(set(B) - set(A))
    both = sorted(set(A) & set(B))
    differ = []
    for rel in both:
        if os.path.getsize(A[rel]) != os.path.getsize(B[rel]) or digest(A[rel]) != digest(B[rel]):
            differ.append(rel)
    print(f'{label}: {len(both)} files in both, {len(differ)} differ, '
          f'{len(only_a)} only in A, {len(only_b)} only in B')
    for rel in differ[:10]:
        print(f'  DIFFER {rel}')
    for rel in (only_a + only_b)[:10]:
        print(f'  ONLY   {rel}')
    return len(differ) == 0 and not only_a and not only_b


def main():
    a, b = sys.argv[1], sys.argv[2]

    # --- the two RED controls, run FIRST ---------------------------------
    tmp = tempfile.mkdtemp(prefix='bytecmp_')
    try:
        c = os.path.join(tmp, 'c')
        shutil.copytree(a, c)
        files = sorted(walk(c).items())
        assert files, 'the control needs at least one file'
        victim = files[len(files) // 2][1]
        with open(victim, 'r+b') as fh:
            fh.seek(0, os.SEEK_END)
            n = fh.tell()
            fh.seek(n // 2)
            byte = fh.read(1)
            fh.seek(n // 2)
            fh.write(bytes([byte[0] ^ 0x01]))
        red1 = compare(a, c, 'CONTROL 1 (one flipped byte -- must be RED)')
        os.remove(files[0][1])
        red2 = compare(a, c, 'CONTROL 2 (a missing file -- must be RED)')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if red1 or red2:
        print('FAIL: the comparator did not go red on its own controls')
        return 2

    ok = compare(a, b, 'SUBJECT')
    print('RESULT', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
