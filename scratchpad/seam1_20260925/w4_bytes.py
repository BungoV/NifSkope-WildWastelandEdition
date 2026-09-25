"""Where the W4 G1 body bytes differ: offset, table, row, field, old vs new value. usage: w4_bytes.py <old tag> <new tag>..."""
import sys, os, struct
sys.argv += []
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location('g', HERE + '/w4_gate.py')
src = open(HERE + '/w4_gate.py').read(); i = src.index('# ------------------------------------------------------------------ G1')
ns = {'__file__': HERE + '/w4_gate.py'}; exec(src[:src.index('OLD, NEW')] + src[src.index('BS = chr(92)'):i], ns)
def tables(b):
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70)
    names = ('bases', 'meshes', 'clusters', 'materials', 'li', 'verts', 'strings')
    return sorted(zip(offs, names))
for region in ('boston', 'sanc'):
    d0 = 'w4/%s/%s/Native/FO4CSLOD/Commonwealth' % (sys.argv[1], region)
    A = ns['rd'](ns['pair'](d0)[0])
    for t in sys.argv[2:]:
        B = ns['strip'](ns['rd'](ns['pair']('w4/%s/%s/Native/FO4CSLOD/Commonwealth' % (t, region))[0]))
        d, dl = ns['diffs'](A, B)
        body = [x for x in d if x >= 0x100]
        T = tables(A)
        out = []
        for x in body:
            base, name = max((o, n) for o, n in T if o <= x)
            rowsz = {'verts': 16, 'meshes': 56, 'clusters': 16, 'li': 48, 'bases': 32, 'materials': 16}.get(name, 1)
            out.append('%s row %d byte %d: %d->%d' % (name, (x - base) // rowsz, (x - base) % rowsz, A[x], B[x]))
        print(region, t, 'body diffs', len(body), out[:8])
