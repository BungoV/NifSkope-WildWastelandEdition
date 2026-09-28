"""TILING5 byte compare of two bake folders (out/<arm>/<tag>).
    python t5_cmp.py A B      e.g.  out/id_rung/boston out/id_c2/boston
Walks every file under A and B, compares bytes, prints per-class identical/total and
the list of differing or missing files.  Exit 0 only when every terrain sheet
(tex/*.DDS) and every VT file (mod/**/*.lodt|*.lodm) is identical.
"""
import os
import sys
import hashlib


def files(root):
    out = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(d, f)
            out[os.path.relpath(p, root).replace('\\', '/')] = p
    return out


def h(p):
    with open(p, 'rb') as f:
        return hashlib.sha1(f.read()).hexdigest()


def cls(rel):
    lo = rel.lower()
    if lo.startswith('tex/') and lo.endswith('.dds'):
        return 'sheet'
    if lo.endswith('.lodt') or lo.endswith('.lodm'):
        return 'vt'
    if lo.endswith('.btr') or lo.endswith('.bto') or lo.endswith('.txt') and 'manifest' in lo:
        return 'mesh'
    return 'other'


def main():
    A, B = sys.argv[1], sys.argv[2]
    fa, fb = files(A), files(B)
    stats = {}
    diff = []
    for rel in sorted(set(fa) | set(fb)):
        c = cls(rel)
        s = stats.setdefault(c, [0, 0])
        s[1] += 1
        if rel not in fa or rel not in fb:
            diff.append((c, rel, 'missing in ' + ('A' if rel not in fa else 'B')))
            continue
        if h(fa[rel]) == h(fb[rel]):
            s[0] += 1
        else:
            diff.append((c, rel, 'differs'))
    for c, (same, tot) in sorted(stats.items()):
        print('%-6s %d of %d identical' % (c, same, tot))
    for c, rel, why in diff:
        print('  %-6s %s  %s' % (c, rel, why))
    bad = [d for d in diff if d[0] in ('sheet', 'vt')]
    print('VERDICT', 'PASS' if not bad and stats.get('sheet', [0, 0])[1] > 0 else 'FAIL')
    sys.exit(0 if not bad and stats.get('sheet', [0, 0])[1] > 0 else 1)


if __name__ == '__main__':
    main()
