"""BAKE1: the impostor candidate lister reads the persistent overlay too, as the chunk builder does
(src/lodgen.cpp gathers refrs + persistentRefrsIn). Anchor count asserted == 1, CR count unchanged."""
import sys
path = 'E:/Projects/NifskopeWWE-bake1/src/nifcli.cpp'
CHECK = '--check' in sys.argv
data = open(path, 'rb').read()
cr0 = data.count(b'\r')
OLD = b"""		for ( int cy = region[1]; cy <= region[3]; cy++ ) {
			for ( int cx = region[0]; cx <= region[2]; cx++ ) {
				for ( const EsmRefr & r : world.refrs( cx, cy ) ) {
					if ( r.initiallyDisabled || r.deleted || !r.base )
						continue;
					if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
						for ( const EsmScolPart & part : world.scolParts( r.base ) )
							consider( part.base );
					} else {
						consider( r.base );
					}
				}
			}
		}
		return 0;
"""
NEW = b"""		auto considerRef = [&]( const EsmRefr & r ) {
			if ( r.initiallyDisabled || r.deleted || !r.base )
				return;
			if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
				for ( const EsmScolPart & part : world.scolParts( r.base ) )
					consider( part.base );
			} else {
				consider( r.base );
			}
		};
		for ( int cy = region[1]; cy <= region[3]; cy++ )
			for ( int cx = region[0]; cx <= region[2]; cx++ )
				for ( const EsmRefr & r : world.refrs( cx, cy ) )
					considerRef( r );
		/* The persistent overlay, as the chunk builder reads it (lodgen.cpp
		 * gathers refrs + persistentRefrsIn): a tree placed persistent is
		 * baked into the chunk, so it needs a card like any other. BNS
		 * Trees.esp places 21,073 of its 22,827 REFRs there (lane BAKE1). */
		for ( const EsmRefr & r : world.persistentRefrsIn(
				float( region[0] ) * 4096.0f, float( region[1] ) * 4096.0f,
				float( region[2] + 1 ) * 4096.0f, float( region[3] + 1 ) * 4096.0f ) )
			considerRef( r );
		return 0;
"""
n = data.count(OLD)
assert n == 1, 'anchor matched %d times' % n
data = data.replace(OLD, NEW)
assert data.count(b'\r') == cr0, 'CR count moved'
if CHECK:
    print('nifcli.cpp ok (check only)')
else:
    with open(path, 'wb') as f:
        f.write(data)
    print('nifcli.cpp patched')
