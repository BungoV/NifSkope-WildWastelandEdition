#!/usr/bin/env python3
"""One number for a whole bake output tree (lane PANEL1, 2026-09-12).

    python tests/spells/lodgen_tree_digest.py <dir> [<dir2>]

With one directory it prints `<sha1> <files> <bytes>`. With two it also prints
every path that differs and exits 1 if any does, so a byte-identity gate can be
a command rather than a paragraph.

The digest covers the RELATIVE PATH and the CONTENT of every file, in path
order, so two trees with the same digest hold the same files with the same
bytes -- a file renamed, added or dropped moves it as surely as a changed byte
does.
"""
import hashlib
import os
import sys


def walk(root):
    out = {}
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            p = os.path.join(dirpath, f)
            out[os.path.relpath(p, root).replace('\\', '/')] = p
    return out


def digest(root):
    files = walk(root)
    h = hashlib.sha1()
    total = 0
    for rel in sorted(files):
        h.update(rel.encode('utf-8'))
        with open(files[rel], 'rb') as fh:
            b = fh.read()
        total += len(b)
        h.update(b)
    return h.hexdigest(), len(files), total


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    a = argv[0]
    ha, na, ba = digest(a)
    print('%s  %s  %d files  %d bytes' % (ha, a, na, ba))
    if len(argv) < 2:
        return 0
    b = argv[1]
    hb, nb, bb = digest(b)
    print('%s  %s  %d files  %d bytes' % (hb, b, nb, bb))
    fa, fb = walk(a), walk(b)
    diff = 0
    for rel in sorted(set(fa) | set(fb)):
        if rel not in fa:
            print('  only in %s: %s' % (b, rel))
            diff += 1
        elif rel not in fb:
            print('  only in %s: %s' % (a, rel))
            diff += 1
        else:
            da = open(fa[rel], 'rb').read()
            db = open(fb[rel], 'rb').read()
            if da != db:
                print('  differs: %s (%d vs %d bytes)' % (rel, len(da), len(db)))
                diff += 1
    if diff:
        print('DIFFERENT: %d files' % diff)
        return 1
    print('IDENTICAL: %d files, %d bytes, sha1 %s' % (na, ba, ha))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
