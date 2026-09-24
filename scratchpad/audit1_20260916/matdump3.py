"""AUDIT1: which MODEL carries each tree material, and how many triangles of it."""
import sys, re
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from lodgen_native_decode import read_lodo
L = read_lodo(sys.argv[1]); sa = L['string_at']
want = re.compile(r'Landscape.(Trees|TreeElmAtlas|TreeSMarsh|Plants)', re.I)
pairs = {}
for c in L['clusters']:
    m = L['materials'][c['materialId']]
    s = sa(m['lodmStringOffset'])
    if not want.search(s):
        continue
    model = sa(L['meshes'][c['meshId']]['modelStringOffset'])
    k = (s, model)
    pairs[k] = pairs.get(k, 0) + c['triangleCount']
for (s, model), t in sorted(pairs.items(), key=lambda kv: -kv[1])[:30]:
    print('%8d tri  %-46s  %s' % (t, s.split(chr(92))[-1], model))
