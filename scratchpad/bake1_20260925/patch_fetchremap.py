"""BAKE1: the .lodo cache-order remap crashed on a mesh with vertices no triangle uses (BNS Trees' LOD models).
meshopt_optimizeVertexFetchRemap leaves such a vertex at ~0u, and the hand-applied remap wrote pos[~0u*3].
Give every unused vertex the next free slot after the used ones, in source order: a true permutation again,
and a mesh whose every vertex is used (all of vanilla) gets exactly the remap it had. Anchor asserted == 1."""
import sys
path = 'E:/Projects/NifskopeWWE-bake1/src/lodofile.cpp'
CHECK = '--check' in sys.argv
data = open(path, 'rb').read()
cr0 = data.count(b'\r')
OLD = b"""				remap.assign( nv, 0u );
				meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );
"""
NEW = b"""				remap.assign( nv, 0u );
				size_t used = meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );
				/* A vertex no triangle uses comes back as ~0u, and the loop
				 * below would write pos[~0u * 3]: the segfault lane BAKE1 hit on
				 * BNS Trees' LOD models (2026-09-25). Unused vertices take the
				 * slots after the used ones, in source order, so the remap is a
				 * permutation again; a mesh with none (all of vanilla) keeps the
				 * remap it had, byte for byte. */
				for ( size_t v = 0; v < nv; v++ )
					if ( remap[v] == ~0u )
						remap[v] = unsigned( used++ );
"""
n = data.count(OLD)
assert n == 1, 'anchor matched %d times' % n
data = data.replace(OLD, NEW)
assert data.count(b'\r') == cr0, 'CR count moved'
if CHECK:
    print('lodofile.cpp ok (check only)')
else:
    with open(path, 'wb') as f:
        f.write(data)
    print('lodofile.cpp patched')
