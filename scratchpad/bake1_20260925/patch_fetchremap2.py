"""BAKE1 (second form of patch_fetchremap.py): the unused-vertex repair moves into its own NOINLINE helper,
so lodoAppendMesh's body changes by one callee name and nothing else. The inline form moved two vanilla
self-AO bytes (codegen of the ray caster inlined in the same function), measured 2026-09-25.
Anchors asserted == 1, CR count unchanged."""
import sys
path = 'E:/Projects/NifskopeWWE-bake1/src/lodofile.cpp'
CHECK = '--check' in sys.argv
data = open(path, 'rb').read()
cr0 = data.count(b'\r')
OLD1 = b"""				size_t used = meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );
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
NEW1 = b"""				lodoFetchRemapWhole( remap.data(), tmp.data(), tmp.size(), nv );
"""
OLD2 = b"""	lib.clusterLods.push_back( L );
	return ci;
}

} // namespace
"""
NEW2 = b"""	lib.clusterLods.push_back( L );
	return ci;
}

/* meshopt_optimizeVertexFetchRemap, made a PERMUTATION. A vertex no triangle
 * uses comes back as ~0u, and lodoAppendMesh's hand-applied remap then wrote
 * pos[~0u * 3]: the segfault lane BAKE1 hit on BNS Trees' LOD models
 * (2026-09-25). Unused vertices take the slots after the used ones, in source
 * order; a mesh with none (all of vanilla) keeps exactly the remap it had.
 * NOINLINE on purpose: written inline, the change moved the caller's code
 * generation and two vanilla self-AO bytes with it. */
__attribute__(( noinline )) size_t lodoFetchRemapWhole( unsigned int * remap, const unsigned int * tris,
	size_t indexCount, size_t nv )
{
	size_t used = meshopt_optimizeVertexFetchRemap( remap, tris, indexCount, nv );
	for ( size_t v = 0; v < nv; v++ )
		if ( remap[v] == ~0u )
			remap[v] = unsigned( used++ );
	return used;
}

} // namespace
"""
for old, new in ((OLD1, NEW1), (OLD2, NEW2)):
    n = data.count(old)
    assert n == 1, 'anchor matched %d times: %r' % (n, old[:50])
    data = data.replace(old, new)
assert data.count(b'\r') == cr0, 'CR count moved'
if CHECK:
    print('lodofile.cpp ok (check only)')
else:
    with open(path, 'wb') as f:
        f.write(data)
    print('lodofile.cpp patched')
