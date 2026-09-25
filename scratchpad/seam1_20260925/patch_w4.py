"""W4: .lodo v5, the optional per-vertex colour stream. Anchored patches, every anchor asserted once."""
R = 'E:/Projects/NifskopeWWE-seam1/'


def patch(path, pairs):
    s = open(R + path, newline='', encoding='utf-8').read()
    cr = s.count('\r')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, (path, n, old[:90])
        s = s.replace(old, new)
    assert s.count('\r') == cr
    open(R + path, 'w', newline='', encoding='utf-8').write(s)
    print('ok', path)


# ------------------------------------------------------------------ lodofile.h
if 'LODO_VERSION_NO_COLOUR' not in open(R + 'src/lodofile.h').read(): patch('src/lodofile.h', [
('''constexpr quint32 LODO_VERSION = 4;
/* VERSION 5 (lane HORIZON3, 2026-09-19) WAS THE SUBDIVIDED LIBRARY, cut so
 * that the per-vertex horizon stream of `.lodi` v8 had a vertex where a far
 * shadow's edge fell. The baked-horizon route was dropped the same day (lane
 * HORIZONOUT, on bungo's word) and no exe ever wrote a version 5 file, so
 * neither the writer nor the reader carries one: this format's only version
 * is 4. The history paragraph is in docs/LODGEN_NATIVE_LODO_LODI.md. */
''',
'''/*! v5 (2026-09-25, lane SEAM1, W4 -- bungo's ruling): the OPTIONAL per-vertex
 *  COLOUR STREAM. A mesh whose source carries BOTH a vertex-colour channel and
 *  the shader's Vertex_Colors flag (SLSF2 bit 5), which is exactly when the
 *  game applies it, gets LODO_MESH_VERTEX_COLOUR and one RGBA8 row per library
 *  vertex of its range in a blob written LAST, after the strings; header 0xD4
 *  counts the rows and 0xD8 is the blob's offset. RGB and A are stored as the
 *  source has them, never premultiplied, and they stay two channels: RGB tints
 *  the diffuse, A is an opacity factor ONLY where the source shader also sets
 *  Vertex_Alpha (LODO_MESH_VERTEX_ALPHA) -- on every Fallout4.esm LOD shape the
 *  census reached, it does not (28 of 28, scratchpad/seam1_20260925/w4_alpha.py).
 *
 *  A library with no coloured mesh writes 0 at 0xD4 and 0xD8 and no blob, so its
 *  bytes are a v4 file's with the version word changed -- the version is outside
 *  headerCrc32 (which starts at 0x10), so nothing else moves, not even the
 *  `.lodi`'s lodoIdentity. For the same reason a v4 file is READ as a v5 file
 *  without colour: v5 EXTENDS the layout into v4's reserved-zero pad and
 *  reinterprets nothing, unlike v3 -> v4.
 *
 *  VERSION 5 was once the subdivided library (lane HORIZON3, 2026-09-19), dropped
 *  the same day (lane HORIZONOUT); no exe ever wrote that file, so the number is
 *  free. The history paragraph is in docs/LODGEN_NATIVE_LODO_LODI.md 3.7. */
constexpr quint32 LODO_VERSION = 5;
//! The one earlier version this reader accepts: v5's layout with no colour stream.
constexpr quint32 LODO_VERSION_NO_COLOUR = 4;
//! v5: a colour row, RGBA8 in byte order R, G, B, A.
constexpr quint32 LODO_COLOUR_STRIDE = 4;
//! v5: the in-memory colour of a vertex no coloured shape gave one -- white, opaque.
constexpr quint32 LODO_COLOUR_NONE = 0xFFFFFFFFU;
'''),
('''enum LodoMeshFlags { LODO_MESH_ANY_ALPHA = 1, LODO_MESH_ANY_SWAY = 2, LODO_MESH_WATERTIGHT = 4 };''',
'''/*! v5, mesh flags bit 3: the mesh has rows in the colour stream (a source
 *  shape carried a colour channel AND Vertex_Colors). Bit 4: a coloured shape's
 *  shader also sets Vertex_Alpha (SLSF1 bit 3), so the row's A is an opacity
 *  factor; without it A is carried but a consumer does not apply it. Bit 4 is
 *  never set without bit 3. */
enum LodoMeshFlags { LODO_MESH_ANY_ALPHA = 1, LODO_MESH_ANY_SWAY = 2, LODO_MESH_WATERTIGHT = 4,
	LODO_MESH_VERTEX_COLOUR = 8, LODO_MESH_VERTEX_ALPHA = 16 };'''),
('''	quint32 cardCount = 0;
	quint32 indexCrc32 = 0;''',
'''	quint32 cardCount = 0;
	/*! v5, 0xD4: rows in the colour stream -- the vertex ranges of the meshes
	 *  flagged LODO_MESH_VERTEX_COLOUR, in mesh-table order. 0 = no stream. */
	quint32 colourVertexCount = 0;
	//! v5, 0xD8: offset of the colour stream, the LAST payload; 0 exactly when the count is 0.
	quint64 offColours = 0;
	quint32 indexCrc32 = 0;'''),
('''	std::vector<LodoVertex> vertices;
	QByteArray strings;                 //!< NUL-terminated UTF-8, offset 0 = ""
''',
'''	std::vector<LodoVertex> vertices;
	/*! v5: one RGBA8 per library vertex, PARALLEL to `vertices` (R in the low
	 *  byte), LODO_COLOUR_NONE where no coloured shape gave one. Only the rows of
	 *  meshes flagged LODO_MESH_VERTEX_COLOUR reach the file; the reader fills
	 *  the rest with LODO_COLOUR_NONE. Empty is accepted by the writer as "all
	 *  none" for libraries built by hand. */
	std::vector<quint32> colours;
	QByteArray strings;                 //!< NUL-terminated UTF-8, offset 0 = ""
'''),
('''	std::vector<quint8> ao;
	std::vector<quint32> tris;  //!< 3 per triangle
	quint16 materialId = 0;
};''',
'''	std::vector<quint8> ao;
	/*! v5: 4 per vertex, R G B A as the source stores them. EMPTY unless the
	 *  source shape has BOTH a vertex-colour channel and the Vertex_Colors shader
	 *  flag -- the game's own condition for applying it (bungo, W4 ruling). */
	std::vector<quint8> rgba;
	//! v5: the source shader also sets Vertex_Alpha, so A is an opacity factor (meaningful with `rgba` only).
	bool vertexAlpha = false;
	std::vector<quint32> tris;  //!< 3 per triangle
	quint16 materialId = 0;
};'''),
])

# ------------------------------------------------------------------ lodofile.cpp
patch('src/lodofile.cpp', [
('''constexpr int H_RESERVED_CE = 0xCE, H_CARDCOUNT = 0xD0, H_RESERVED_D4 = 0xD4;
''',
'''constexpr int H_RESERVED_CE = 0xCE, H_CARDCOUNT = 0xD0, H_RESERVED_D4 = 0xD4;
/*! v5 (lane SEAM1, W4) takes the next twelve bytes of that pad: 0xD4 the colour
 *  stream's row count, 0xD8 its offset. Both are zero on a library with no
 *  coloured mesh, so a v4 file is a v5 file without colour. The pad is now
 *  0xCE..0xCF plus 0xE0..0xFF. */
constexpr int H_COLOURCOUNT = 0xD4, H_OFF_COLOURS = 0xD8, H_RESERVED_E0 = 0xE0;
'''),
('''	quint8 sway = 0;
	quint8 ao = 255;
};

//! Squared distance''',
'''	quint8 sway = 0;
	quint8 ao = 255;
	quint32 rgba = LODO_COLOUR_NONE;    //!< v5: R low byte; NONE when the shape carries no colour
};

//! Squared distance'''),
('''		lv.sway = v.sway;
		lv.selfAO = v.ao;
		lib.vertices.push_back( lv );
	}''',
'''		lv.sway = v.sway;
		lv.selfAO = v.ao;
		lib.vertices.push_back( lv );
		lib.colours.push_back( v.rgba );
	}'''),
('''			|| ( !s.sway.empty() && s.sway.size() != nv ) || s.tris.size() % 3 )''',
'''			|| ( !s.sway.empty() && s.sway.size() != nv ) || ( !s.rgba.empty() && s.rgba.size() != nv * 4 )
			|| s.tris.size() % 3 )'''),
('''	mesh.flags = anySway ? LODO_MESH_ANY_SWAY : 0;
''',
'''	mesh.flags = anySway ? LODO_MESH_ANY_SWAY : 0;
	/* v5: the colour stream, only for a shape the game would colour (the loader
	 * leaves `rgba` empty unless the source has the channel AND Vertex_Colors). */
	for ( const LodoSrcShape & s : shapesIn )
		if ( !s.rgba.empty() ) {
			mesh.flags |= LODO_MESH_VERTEX_COLOUR;
			if ( s.vertexAlpha )
				mesh.flags |= LODO_MESH_VERTEX_ALPHA;
		}
	/* A library appended to without colours so far (built by hand, or read from
	 * a file) gets its parallel array now, so the push in lodoEmitCluster keeps it
	 * one-for-one with `vertices`. */
	if ( lib.colours.size() != lib.vertices.size() )
		lib.colours.resize( lib.vertices.size(), LODO_COLOUR_NONE );
'''),
('''				std::vector<quint8> sway, ao;
				if ( !s.sway.empty() )
					sway.assign( nv, 0 );''',
'''				std::vector<quint8> sway, ao, rgba;
				if ( !s.rgba.empty() )
					rgba.assign( nv * 4, 255 );
				if ( !s.sway.empty() )
					sway.assign( nv, 0 );'''),
('''					if ( !ao.empty() )
						ao[d] = s.ao[v];
				}''',
'''					if ( !ao.empty() )
						ao[d] = s.ao[v];
					if ( !rgba.empty() )
						for ( int k = 0; k < 4; k++ )
							rgba[d * 4 + k] = s.rgba[v * 4 + k];
				}'''),
('''				if ( !ao.empty() )
					s.ao.swap( ao );
				s.tris.assign''',
'''				if ( !ao.empty() )
					s.ao.swap( ao );
				if ( !rgba.empty() )
					s.rgba.swap( rgba );
				s.tris.assign'''),
('''				e.sway = s.sway.empty() ? 0 : s.sway[v];
				e.ao = s.ao.empty() ? 255 : s.ao[v];
				const quint32 w = quint32( wv.size() );''',
'''				e.sway = s.sway.empty() ? 0 : s.sway[v];
				e.ao = s.ao.empty() ? 255 : s.ao[v];
				e.rgba = lodoSrcRgba( s, v );
				const quint32 w = quint32( wv.size() );'''),
('''					e.sway = s.sway.empty() ? 0 : s.sway[v];
					e.ao = s.ao.empty() ? 255 : s.ao[v];
					cv.push_back( e );''',
'''					e.sway = s.sway.empty() ? 0 : s.sway[v];
					e.ao = s.ao.empty() ? 255 : s.ao[v];
					e.rgba = lodoSrcRgba( s, v );
					cv.push_back( e );'''),
('''						lib.vertices.resize( snapVerts );
''',
'''						lib.vertices.resize( snapVerts );
						lib.colours.resize( snapVerts );
'''),
('''	lib.vertices.insert( lib.vertices.end(), staged.vertices.begin(), staged.vertices.end() );
''',
'''	lib.vertices.insert( lib.vertices.end(), staged.vertices.begin(), staged.vertices.end() );
	// v5: the parallel colours, padded first if `lib` had none so far
	if ( lib.colours.size() != size_t( vertexBase ) )
		lib.colours.resize( size_t( vertexBase ), LODO_COLOUR_NONE );
	if ( staged.colours.size() == staged.vertices.size() )
		lib.colours.insert( lib.colours.end(), staged.colours.begin(), staged.colours.end() );
	else
		lib.colours.resize( lib.vertices.size(), LODO_COLOUR_NONE );
'''),
# the writer
('''	QByteArray strings = lib.strings;
	if ( strings.isEmpty() )
		strings.append( '\\0' );
''',
'''	QByteArray strings = lib.strings;
	if ( strings.isEmpty() )
		strings.append( '\\0' );
	if ( !lib.colours.empty() && lib.colours.size() != lib.vertices.size() )
		return fail( QString( "the v5 colour array has %1 entries for %2 vertices; it is PARALLEL, one a vertex" )
			.arg( lib.colours.size() ).arg( lib.vertices.size() ) );
	/* v5: the colour stream. Each flagged mesh's vertex range, in mesh-table
	 * order; the range must be CONTIGUOUS (every mesh's clusters are appended in
	 * one run, and the reader recomputes the same ranges from the cluster rows),
	 * so a reader finds a mesh's rows by a prefix sum and no per-mesh offset is
	 * stored. Built only when some mesh is flagged: a library with no colour
	 * writes no blob and 0 at 0xD4/0xD8. */
	std::vector<quint8> colourBlob;
	for ( size_t mi = 0; mi < lib.meshes.size(); mi++ ) {
		const LodoMesh & m = lib.meshes[mi];
		if ( !( m.flags & LODO_MESH_VERTEX_COLOUR ) )
			continue;
		quint64 lo = ~quint64( 0 ), hi = 0, sum = 0;
		for ( quint64 c = m.clusterFirst; c < quint64( m.clusterFirst ) + m.clusterCount && c < lib.clusters.size(); c++ ) {
			const LodoCluster & cl = lib.clusters[size_t( c )];
			lo = std::min<quint64>( lo, cl.vertexBase );
			hi = std::max<quint64>( hi, quint64( cl.vertexBase ) + cl.vertexCount );
			sum += cl.vertexCount;
		}
		if ( sum == 0 || hi - lo != sum || hi > lib.vertices.size() )
			return fail( QString( "mesh %1 is flagged VERTEX_COLOUR but its clusters' vertices are not one "
				"contiguous range (%2..%3 holding %4)" ).arg( mi ).arg( lo ).arg( hi ).arg( sum ) );
		for ( quint64 v = lo; v < hi; v++ ) {
			const quint32 c = lib.colours.empty() ? LODO_COLOUR_NONE : lib.colours[size_t( v )];
			for ( int k = 0; k < 4; k++ )
				colourBlob.push_back( quint8( ( c >> ( 8 * k ) ) & 0xFF ) );
		}
	}
'''),
('''	h.offStrings = payload( strings.constData(), quint64( strings.size() ) );
	crcOver( strings.constData(), quint64( strings.size() ) );
	h.indexCrc32 = icrc;''',
'''	h.offStrings = payload( strings.constData(), quint64( strings.size() ) );
	crcOver( strings.constData(), quint64( strings.size() ) );
	/* v5: the colour stream LAST, so no earlier offset moves, and inside
	 * indexCrc32 only when it exists -- a library without colour hashes exactly
	 * what v4 hashed. */
	h.colourVertexCount = quint32( colourBlob.size() / LODO_COLOUR_STRIDE );
	h.offColours = 0;
	if ( !colourBlob.empty() ) {
		h.offColours = payload( colourBlob.data(), quint64( colourBlob.size() ) );
		crcOver( colourBlob.data(), quint64( colourBlob.size() ) );
	}
	h.indexCrc32 = icrc;'''),
('''	putLE<quint32>( file, H_CARDCOUNT, h.cardCount );
	h.headerCrc32''',
'''	putLE<quint32>( file, H_CARDCOUNT, h.cardCount );
	putLE<quint32>( file, H_COLOURCOUNT, h.colourVertexCount );
	putLE<quint64>( file, H_OFF_COLOURS, h.offColours );
	h.headerCrc32'''),
# the reader
('''			"no WATERTIGHT bit in its mesh flags. Re-bake; this reader knows version 4" ) );
	if ( h.version != LODO_VERSION )
		return refuse( QString( "version %1; this reader knows %2" ).arg( h.version )
			.arg( LODO_VERSION ) );''',
'''			"no WATERTIGHT bit in its mesh flags. Re-bake; this reader knows versions 4 and 5" ) );
	/* v5 EXTENDS v4 into its reserved pad and reinterprets nothing, so a v4 file
	 * is read as a v5 file without a colour stream; the pad sweep below still
	 * refuses a v4 file that carries anything at 0xD4..0xDF. */
	if ( h.version != LODO_VERSION && h.version != LODO_VERSION_NO_COLOUR )
		return refuse( QString( "version %1; this reader knows %2 and %3" ).arg( h.version )
			.arg( LODO_VERSION_NO_COLOUR ).arg( LODO_VERSION ) );'''),
('''	for ( int i = H_RESERVED_CE; i < int( LODO_HEADER_BYTES ); i++ ) {
		if ( i >= H_CARDCOUNT && i < H_RESERVED_D4 )
			continue;''',
'''	/* v5: the colour words at 0xD4/0xD8. On a v4 file they are pad and the sweep
	 * refuses them by position; on v5 the count and the offset are 0 together. */
	const bool v5 = h.version == LODO_VERSION;
	if ( v5 ) {
		h.colourVertexCount = getLE<quint32>( p + H_COLOURCOUNT );
		h.offColours = getLE<quint64>( p + H_OFF_COLOURS );
		if ( ( h.colourVertexCount == 0 ) != ( h.offColours == 0 ) )
			return refuse( QString( "colourVertexCount %1 and colour offset %2: a colour stream has both or neither" )
				.arg( h.colourVertexCount ).arg( h.offColours ) );
		if ( h.colourVertexCount > h.vertexCount )
			return refuse( QString( "colourVertexCount %1 is above the file's %2 vertices" )
				.arg( h.colourVertexCount ).arg( h.vertexCount ) );
	}
	for ( int i = H_RESERVED_CE; i < int( LODO_HEADER_BYTES ); i++ ) {
		if ( i >= H_CARDCOUNT && i < H_RESERVED_D4 )
			continue;
		if ( v5 && i >= H_COLOURCOUNT && i < H_RESERVED_E0 )
			continue;'''),
('''	struct Tab { const char * name; quint64 off; quint64 bytes; };
	const Tab tabs[8] = {''',
'''	struct Tab { const char * name; quint64 off; quint64 bytes; };
	const int nTabs = h.colourVertexCount ? 9 : 8;
	const Tab tabs[9] = {'''),
('''		{ "string blob", h.offStrings, quint64( h.stringBytes ) } };
	quint64 prevEnd = LODO_HEADER_BYTES;
	for ( const Tab & t : tabs ) {''',
'''		{ "string blob", h.offStrings, quint64( h.stringBytes ) },
		{ "colour stream (v5)", h.offColours, quint64( h.colourVertexCount ) * LODO_COLOUR_STRIDE } };
	quint64 prevEnd = LODO_HEADER_BYTES;
	for ( int ti = 0; ti < nTabs; ti++ ) {
		const Tab & t = tabs[ti];'''),
('''		quint32 icrc = 0;
		for ( const Tab & t : tabs )
			icrc = lodvCrc32( p + t.off, qsizetype( t.bytes ), icrc );''',
'''		quint32 icrc = 0;
		for ( int ti = 0; ti < nTabs; ti++ )
			icrc = lodvCrc32( p + tabs[ti].off, qsizetype( tabs[ti].bytes ), icrc );'''),
('''	if ( h.vertexCount ) std::memcpy( L.vertices.data(), p + h.offVertices, tabs[6].bytes );
	L.strings = QByteArray( file.constData() + h.offStrings, qsizetype( h.stringBytes ) );
''',
'''	if ( h.vertexCount ) std::memcpy( L.vertices.data(), p + h.offVertices, tabs[6].bytes );
	L.strings = QByteArray( file.constData() + h.offStrings, qsizetype( h.stringBytes ) );
	/* v5: the colour stream, scattered back to the parallel array by the same
	 * contiguous ranges the writer used. This walk runs with or without the
	 * payload check: the rows it reads are the ones a consumer draws, so the
	 * range arithmetic is checked every time. */
	L.colours.assign( h.vertexCount, LODO_COLOUR_NONE );
	{
		quint64 row = 0;
		for ( size_t mi = 0; mi < L.meshes.size(); mi++ ) {
			const LodoMesh & m = L.meshes[mi];
			if ( ( m.flags & LODO_MESH_VERTEX_ALPHA ) && !( m.flags & LODO_MESH_VERTEX_COLOUR ) )
				return refuse( QString( "mesh %1: VERTEX_ALPHA without VERTEX_COLOUR; A has no row to live in" ).arg( mi ) );
			if ( !( m.flags & LODO_MESH_VERTEX_COLOUR ) )
				continue;
			quint64 lo = ~quint64( 0 ), hi = 0, sum = 0;
			for ( quint64 c = m.clusterFirst; c < quint64( m.clusterFirst ) + m.clusterCount && c < L.clusters.size(); c++ ) {
				lo = std::min<quint64>( lo, L.clusters[size_t( c )].vertexBase );
				hi = std::max<quint64>( hi, quint64( L.clusters[size_t( c )].vertexBase ) + L.clusters[size_t( c )].vertexCount );
				sum += L.clusters[size_t( c )].vertexCount;
			}
			if ( sum == 0 || hi - lo != sum || hi > h.vertexCount )
				return refuse( QString( "mesh %1 is flagged VERTEX_COLOUR but its vertices are not one contiguous "
					"range (%2..%3 holding %4)" ).arg( mi ).arg( lo ).arg( hi ).arg( sum ) );
			if ( row + sum > h.colourVertexCount )
				return refuse( QString( "the colour-flagged meshes need more than the %1 rows colourVertexCount gives "
					"(mesh %2)" ).arg( h.colourVertexCount ).arg( mi ) );
			const unsigned char * cp = p + h.offColours + row * LODO_COLOUR_STRIDE;
			for ( quint64 v = lo; v < hi; v++, cp += LODO_COLOUR_STRIDE )
				L.colours[size_t( v )] = quint32( cp[0] ) | ( quint32( cp[1] ) << 8 ) | ( quint32( cp[2] ) << 16 )
					| ( quint32( cp[3] ) << 24 );
			row += sum;
		}
		if ( row != h.colourVertexCount )
			return refuse( QString( "colourVertexCount %1 but the colour-flagged meshes hold %2 vertices" )
				.arg( h.colourVertexCount ).arg( row ) );
	}
'''),
('''			if ( m.flags & ~quint16( LODO_MESH_ANY_ALPHA | LODO_MESH_ANY_SWAY | LODO_MESH_WATERTIGHT ) )''',
'''			if ( m.flags & ~quint16( LODO_MESH_ANY_ALPHA | LODO_MESH_ANY_SWAY | LODO_MESH_WATERTIGHT
					| ( v5 ? ( LODO_MESH_VERTEX_COLOUR | LODO_MESH_VERTEX_ALPHA ) : 0 ) ) )'''),
('''		<< QString( "cardCount %1" ).arg( h.cardCount )
		<< QString( "indexCrc32''',
'''		<< QString( "cardCount %1" ).arg( h.cardCount )
		<< QString( "colourVertexCount %1" ).arg( h.colourVertexCount )
		<< QString( "offColours %1" ).arg( h.offColours )
		<< QString( "indexCrc32'''),
('''		out << QString( "watertightMeshes %1" ).arg( watertight )''',
'''		/* v5: which meshes carry colour, and whether any of it is not white --
		 * the number gate W4-2 reads (a stream of all-white rows would pass a
		 * presence check and colour nothing). */
		quint64 colourMeshes = 0, alphaMeshes = 0, colourNotWhite = 0;
		QStringList colourNames;
		for ( const LodoMesh & m : lib->meshes ) {
			if ( !( m.flags & LODO_MESH_VERTEX_COLOUR ) )
				continue;
			colourMeshes++;
			if ( m.flags & LODO_MESH_VERTEX_ALPHA )
				alphaMeshes++;
			if ( colourNames.size() < 40 )
				colourNames << QFileInfo( QString( lib->stringAt( m.modelStringOffset ) ).replace( QChar( '\\\\' ), QChar( '/' ) ) ).fileName();
		}
		for ( quint32 c : lib->colours )
			if ( ( c & 0x00FFFFFFU ) != 0x00FFFFFFU )
				colourNotWhite++;
		out << QString( "colourMeshes %1" ).arg( colourMeshes )
			<< QString( "colourAlphaMeshes %1" ).arg( alphaMeshes )
			<< QString( "colourVerticesNotWhite %1" ).arg( colourNotWhite )
			<< QString( "colourMeshNames %1" ).arg( colourNames.isEmpty() ? QStringLiteral( "-" ) : colourNames.join( ',' ) );
		out << QString( "watertightMeshes %1" ).arg( watertight )'''),
])

# lodoSrcRgba helper right after LodoEmitVert
patch('src/lodofile.cpp', [
('''	quint32 rgba = LODO_COLOUR_NONE;    //!< v5: R low byte; NONE when the shape carries no colour
};
''',
'''	quint32 rgba = LODO_COLOUR_NONE;    //!< v5: R low byte; NONE when the shape carries no colour
};

//! v5: a source vertex's packed colour, R in the low byte; NONE when the shape carries none.
inline quint32 lodoSrcRgba( const LodoSrcShape & s, size_t v )
{
	if ( s.rgba.size() < ( v + 1 ) * 4 )
		return LODO_COLOUR_NONE;
	const quint8 * c = &s.rgba[v * 4];
	return quint32( c[0] ) | ( quint32( c[1] ) << 8 ) | ( quint32( c[2] ) << 16 ) | ( quint32( c[3] ) << 24 );
}
'''),
])
