"""Which sheet differs between two layers that name the same textures?  pairdiff.py <Objects dir> <class> <layerA> <layerB> ..."""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gates import layer_slice

objs, cls = sys.argv[1], sys.argv[2]
pairs = [(int(sys.argv[i]), int(sys.argv[i + 1])) for i in range(3, len(sys.argv), 2)]
cache = {}
for a, b in pairs:
    out = []
    for s in ('_d', '_n', '_gsaos'):
        p = os.path.join(objs, 'Commonwealth.LodgenArrays.%s%s.DDS' % (cls, s))
        sa, _ = layer_slice(cache, p, a)
        sb, _ = layer_slice(cache, p, b)
        nb = sum(1 for k in range(0, len(sa), 16) if sa[k:k + 16] != sb[k:k + 16])
        # mip 0 only: the first quarter-plus of the slice is mip 0 for a square class (w*h/16 blocks)
        out.append('%s %d/%d blocks differ' % (s, nb, len(sa) // 16))
    print('%s layers %d vs %d: %s' % (cls, a, b, '; '.join(out)))
