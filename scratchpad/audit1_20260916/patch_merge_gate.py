"""AUDIT1 step 6: lodgen_merge.sh's A-line check is stale, not the product.

MEASURED (scratchpad/audit1_20260916/mergedbg): the gate bakes without
--identity, and since DEFAULTS1 (2026-09-12) a default .BTO carries no UV 2
channel at all, so every A line's layer claim is compared against an EMPTY
vertex layer set and can never match.  The same bake with --identity gives
5 of 5 A lines matching their vertices exactly.  src/lodgen.cpp:5149 shows the
product means it: `shapesWithoutUv2++;  // a profile without the extra
channels: the manifest still says`.

This patch spells --identity on the gate's two bakes (the layer-in-UV2
contract is what check 2 exists to measure), adds a check that the channel is
really there, and adds a refuter so a passing check is not an empty loop.
"""
import os
import re
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_merge.sh'
s = open(P, encoding='utf-8', newline='').read()
orig = s


def sub(old, new, count=1):
    global s
    n = s.count(old)
    assert n == count, 'anchor %r found %d times (want %d)' % (old[:60], n, count)
    s = s.replace(old, new)


# 1. the header: say which profile the layer half needs and why
sub("""#   2. every A line names an existing block, with layer -1 where a merged
#      shape spans layers, and every vertex of such a shape carries an
#      integer layer inside its set's layer count""",
    """#   2. every A line names an existing block, with layer -1 where a merged
#      shape spans layers, and every vertex of such a shape carries an
#      integer layer inside its set's layer count.  Both bakes spell
#      --identity: since DEFAULTS1 (2026-09-12) object identity is OFF by
#      default and a default chunk has no UV 2 channel at all, so the layer
#      claim would be compared against an empty set (lodgen.cpp:5149 writes
#      the A line either way, by design).  A companion check asserts the
#      channel is present, and a refuter doctors one claim to prove the
#      comparison can still fail.""")

# 2. the two bakes
sub("""--out-dir "$W/$mode/obj" --tex-dir "$W/$mode/tex" --data-root "$DATA" --arrays --atlas $flag""",
    """--out-dir "$W/$mode/obj" --tex-dir "$W/$mode/tex" --data-root "$DATA" --arrays --atlas --identity $flag""")

# 3. shapes() reports whether the UV2 channel exists
sub('        out.append((i, nv, nt, nseg, layers))',
    '        out.append((i, nv, nt, nseg, layers, hasUv2))')
sub('''    """every BSSubIndexTriShape: (block, numVerts, numTris, segments, uv2 layers set, stride)"""''',
    '''    """every BSSubIndexTriShape: (block, numVerts, numTris, segments, uv2 layers set, has uv2)"""''')

# 4. the predicate, once, so the refuter runs the same code
sub('''def readLodm(path):''',
    '''def layerOk(layers, layer, n):
    """the A line's claim against the layers actually stored on the vertices"""
    if layer == -1:
        return len(layers) >= 2 and all(float(l).is_integer() and 0 <= l < n for l in layers)
    return layers == {float(layer)}

def readLodm(path):''')

# 5. counters
sub("badSeg = 0; over = 0; badA = 0; mixed = 0; badLayer = 0; checkedMixed = 0; holes = 0",
    "badSeg = 0; over = 0; badA = 0; mixed = 0; badLayer = 0; checkedMixed = 0; holes = 0\n"
    "noUv2 = 0; claims = []")

sub("""    for s in s2:
        if s[3] != 16: badSeg += 1
        if s[1] > 65535: over += 1""",
    """    for s in s2:
        if s[3] != 16: badSeg += 1
        if s[1] > 65535: over += 1
        if not s[5]: noUv2 += 1""")

# 6. the claim loop through the one predicate
sub("""        if layer == -1:
            mixed += 1
            checkedMixed += 1
            if not (len(layers) >= 2 and all(float(l).is_integer() and 0 <= l < n for l in layers)): badLayer += 1
        else:
            if layers != {float(layer)}: badLayer += 1""",
    """        claims.append((set(layers), layer, n))
        if layer == -1:
            mixed += 1
            checkedMixed += 1
        if not layerOk(layers, layer, n): badLayer += 1""")

# 7. the channel check and the refuter, beside the check they guard
sub("""check('every A line names an existing block and its layer matches the vertices (%d per-vertex shapes)' % mixed, badA == 0 and badLayer == 0)""",
    """check('every merged shape carries the UV 2 layer channel (%d without)' % noUv2, noUv2 == 0)
check('every A line names an existing block and its layer matches the vertices (%d per-vertex shapes)' % mixed, badA == 0 and badLayer == 0)
# the refuter: the same predicate on a doctored claim (a per-vertex shape told
# it is single-layer, a single-layer one told it is per-vertex) must refuse
# every one, or the check above is an empty loop
refused = sum(1 for lay, lv, n in claims if not layerOk(lay, 0 if lv == -1 else -1, n))
check('the layer comparison can fail (%d of %d doctored claims refused)' % (refused, len(claims)), len(claims) > 0 and refused == len(claims))""")

assert s != orig
blocks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)
assert blocks, 'no PYEOF block found'
for i, blk in enumerate(blocks):
    compile(blk, '<block %d>' % i, 'exec')
    print('python block %d compiles' % i)
assert s.count('\r') == orig.count('\r'), 'line endings moved'
d = os.path.dirname(P)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(s)
f.close()
os.replace(f.name, P)
print('patched %s  %d -> %d bytes, CR %d' % (P, len(orig), len(s), s.count('\r')))
