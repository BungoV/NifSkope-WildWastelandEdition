"""BAKE2: the vanilla fill reads Bethesda's dim-4 sheets on the WORLDSPACE's own LOD grid, not always on
multiples of 4. Far Harbor's LODSettings\\DLC03FarHarbor.LOD says SW cell -73,-59 (its dim-4 files sit at
x = 3, y = 1 mod 4); the Commonwealth, pre-war and Nuka-World say -96,-96 / -96,-96 / -32,-32 (phase 0,0: byte
for byte today's addressing). The phase is read from <vanilla root>/LODSettings/<WS>.LOD (int16 left, int16
bottom, the engine's own file); absent = 0,0 and the census says so. Anchors asserted once; CR count kept."""
import os
R = 'E:/Projects/NifskopeWWE-bake2/'
DRY = os.environ.get('DRY') == '1'

def patch(path, pairs):
    b = open(R + path, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old); assert n == 1, (path, n, old[:80]); s = s.replace(old, new)
    out = s.encode('utf-8'); assert out.count(b'\r') == cr, path
    if not DRY: open(R + path, 'wb').write(out)
    print('%s: %d edits%s' % (path, len(pairs), ' (dry)' if DRY else ''))

patch('src/lodgen.cpp', [
("""	qint64 noLandCells = 0, noLandRingCells = 0, texelsNoLand = 0;
""",
"""	qint64 noLandCells = 0, noLandRingCells = 0, texelsNoLand = 0;
	int gridX = 0, gridY = 0;                   //!< vanilla dim-4 grid phase, cells 0..3 (LODSettings SW cell mod 4)
	QString gridSource = QStringLiteral( "default" );
"""),
("""		auto floorDiv = []( int a, int b ) { return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b ); };
		for ( int cr = floorDiv( gy0, 512 ); cr <= floorDiv( gy1, 512 ); cr++ ) {
			for ( int cc = floorDiv( gx0, 512 ); cc <= floorDiv( gx1, 512 ); cc++ ) {
				// chunk row cr spans world y [-(cr + 1) * 16384, -cr * 16384): SW cell y = -(cr + 1) * 4
				const std::vector<quint8> * t = sheet( cc * 4, -( cr + 1 ) * 4 );
				if ( !t )
					continue;
				const int tx0 = qMax( gx0, cc * 512 ), tx1 = qMin( gx1, cc * 512 + 511 );
				const int ty0 = qMax( gy0, cr * 512 ), ty1 = qMin( gy1, cr * 512 + 511 );
				for ( int gy = ty0; gy <= ty1; gy++ ) {
					for ( int gx = tx0; gx <= tx1; gx++ ) {
						const quint8 * c = t->data() + ( size_t( gy - cr * 512 ) * 512 + ( gx - cc * 512 ) ) * 3;
""",
"""		auto floorDiv = []( int a, int b ) { return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b ); };
		// the worldspace's own dim-4 grid: chunk (cc, cr) has SW cell (gridX + 4 cc, gridY - 4 (cr + 1));
		// its texels start at gx = gridX * 128 + 512 cc, gy = 512 cr - gridY * 128 (phase 0,0 = the old grid)
		const int ox = gridX * 128, oy = gridY * 128;
		for ( int cr = floorDiv( gy0 + oy, 512 ); cr <= floorDiv( gy1 + oy, 512 ); cr++ ) {
			for ( int cc = floorDiv( gx0 - ox, 512 ); cc <= floorDiv( gx1 - ox, 512 ); cc++ ) {
				const std::vector<quint8> * t = sheet( gridX + cc * 4, gridY - ( cr + 1 ) * 4 );
				if ( !t )
					continue;
				const int bx = ox + cc * 512, by = cr * 512 - oy;
				const int tx0 = qMax( gx0, bx ), tx1 = qMin( gx1, bx + 511 );
				const int ty0 = qMax( gy0, by ), ty1 = qMin( gy1, by + 511 );
				for ( int gy = ty0; gy <= ty1; gy++ ) {
					for ( int gx = tx0; gx <= tx1; gx++ ) {
						const quint8 * c = t->data() + ( size_t( gy - by ) * 512 + ( gx - bx ) ) * 3;
"""),
("""		"noLandRingCells=%20 texelsNoLand=%21 root=%22" )""",
"""		"noLandRingCells=%20 texelsNoLand=%21 grid=%22,%23(%24) root=%25" )"""),
("""		.arg( F.vanillaLoaded ).arg( F.noLandCells ).arg( F.noLandRingCells ).arg( F.texelsNoLand )
		.arg( lodgenVanillaLodRoot() );""",
"""		.arg( F.vanillaLoaded ).arg( F.noLandCells ).arg( F.noLandRingCells ).arg( F.texelsNoLand )
		.arg( F.gridX ).arg( F.gridY ).arg( F.gridSource ).arg( lodgenVanillaLodRoot() );"""),
("""		fill.on = true;
		fill.ws = ws;
		lodgenVtFillPainted(""",
"""		fill.on = true;
		fill.ws = ws;
		{
			// the engine's own LOD grid for this worldspace: LODSettings\\<WS>.LOD, int16 left, int16 bottom
			QFile lf( QDir( lodgenVanillaLodRoot() ).filePath( QStringLiteral( "LODSettings/%1.LOD" ).arg( ws ) ) );
			if ( !lodgenVanillaLodRoot().isEmpty() && lf.open( QIODevice::ReadOnly ) ) {
				const QByteArray b = lf.read( 16 );
				if ( b.size() >= 4 ) {
					const int left = qint16( quint8( b[0] ) | ( quint8( b[1] ) << 8 ) );
					const int bottom = qint16( quint8( b[2] ) | ( quint8( b[3] ) << 8 ) );
					fill.gridX = ( ( left % 4 ) + 4 ) % 4;
					fill.gridY = ( ( bottom % 4 ) + 4 ) % 4;
					fill.gridSource = QStringLiteral( "LODSettings %1,%2" ).arg( left ).arg( bottom );
				}
			}
		}
		lodgenVtFillPainted("""),
])
