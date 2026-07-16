// Standalone decoder harness: runs hknpDecode on a raw blob (no GUI/GL) to
// isolate decode crashes from rendering. Build via tools/build_test.sh.
#include "gl/hknpdecode.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>
#include <cstdio>

int main( int argc, char ** argv )
{
	if ( argc < 2 ) {
		printf( "usage: test_hkdecode <blob.bin>\n" );
		return 1;
	}
	QFile f( argv[1] );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		printf( "cannot open %s\n", argv[1] );
		return 1;
	}
	QByteArray data = f.readAll();
	printf( "blob %lld bytes\n", (long long) data.size() );

	HknpSystem sys = hknpDecode( data );
	printf( "valid=%d shapes=%d error='%s' unknown=%d\n",
		sys.valid, (int) sys.shapes.size(),
		sys.error.toLatin1().constData(), (int) sys.unknownShapes.size() );

	for ( int si = 0; si < sys.shapes.size(); si++ ) {
		const HknpShape & s = sys.shapes.at( si );
		int nv = s.verts.size();
		int maxIdx = -1;
		for ( const Triangle & t : s.tris )
			maxIdx = std::max( { maxIdx, (int) t[0], (int) t[1], (int) t[2] } );
		printf( "  shape %d: %s verts=%d tris=%d maxTriIdx=%d hasTransform=%d prim=%d\n",
			si, s.className.toLatin1().constData(), nv, (int) s.tris.size(),
			maxIdx, s.hasTransform, s.primType );
		if ( maxIdx >= nv )
			printf( "    *** OUT-OF-RANGE triangle index (max %d >= verts %d) ***\n", maxIdx, nv );
	}
	printf( "done\n" );
	return 0;
}
