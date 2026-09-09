/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodvfile.h"

/* Only for LODL_MAGIC: this validator must be able to NAME the LANDSCAPE
 * file when it is handed one, because `.lodt` meant that format until
 * 2026-09-09 and means this one now. */
#include "lodtfile.h"

#include <QFile>

#include <cstring>

/* The container is described in docs/LODGEN_TERRAIN_VT.md. Everything here is
 * layout: the header, the fixed-stride tile table, the streaming writer and
 * the validator. There is ONE validator, shared by the writer's own read-back,
 * by the CLI and by anything that opens a .lodt later, because a rule that
 * exists in two implementations is a rule that will disagree with itself. */

namespace
{

/* LODTEX_MAGIC ('LDTX') is in lodvfile.h, because src/lodtfile.cpp refuses
 * it by name; LODL_MAGIC ('LODT') is in lodtfile.h for the same reason. */
constexpr quint32 LODV_VERSION = 1;
constexpr quint32 LODV_HEADER_BYTES = 256;
constexpr quint32 LODV_TABLE_STRIDE = 24;
constexpr quint64 LODV_PAYLOAD_ALIGN = 4096;

inline void put8( unsigned char * p, quint8 v ) { p[0] = v; }

inline void put16( unsigned char * p, quint16 v )
{
	p[0] = quint8( v );
	p[1] = quint8( v >> 8 );
}

inline void put32( unsigned char * p, quint32 v )
{
	for ( int i = 0; i < 4; i++ )
		p[i] = quint8( v >> ( i * 8 ) );
}

inline void put64( unsigned char * p, quint64 v )
{
	for ( int i = 0; i < 8; i++ )
		p[i] = quint8( v >> ( i * 8 ) );
}

inline quint16 get16( const unsigned char * p )
{
	return quint16( quint16( p[0] ) | ( quint16( p[1] ) << 8 ) );
}

inline quint32 get32( const unsigned char * p )
{
	quint32 v = 0;
	for ( int i = 0; i < 4; i++ )
		v |= quint32( p[i] ) << ( i * 8 );
	return v;
}

inline quint64 get64( const unsigned char * p )
{
	quint64 v = 0;
	for ( int i = 0; i < 8; i++ )
		v |= quint64( p[i] ) << ( i * 8 );
	return v;
}

inline float getF32( const unsigned char * p )
{
	const quint32 b = get32( p );
	float f = 0.0f;
	std::memcpy( &f, &b, 4 );
	return f;
}

inline void putF32( unsigned char * p, float f )
{
	quint32 b = 0;
	std::memcpy( &b, &f, 4 );
	put32( p, b );
}

quint64 alignUp( quint64 v, quint64 a )
{
	const quint64 r = v % a;
	return r ? v + ( a - r ) : v;
}

bool validFormat( quint16 f, bool height )
{
	if ( height )
		return f == LODV_DXGI_R16_UNORM;
	return f == LODV_DXGI_BC1_UNORM || f == LODV_DXGI_BC1_UNORM_SRGB
		|| f == LODV_DXGI_BC3_UNORM || f == LODV_DXGI_BC3_UNORM_SRGB;
}

//! Serialise the header fields into 256 bytes. `indexCrc32` is left zero; the
//! writer patches it after the table is known.
void writeHeaderBytes( unsigned char * h, const LodvHeaderFields & f,
	quint64 fileBytes, quint64 tableOffset, quint64 payloadOffset, quint32 tileCount )
{
	std::memset( h, 0, LODV_HEADER_BYTES );
	put32( h + 0x00, LODTEX_MAGIC );
	put32( h + 0x04, LODV_VERSION );
	put32( h + 0x08, LODV_HEADER_BYTES );
	put32( h + 0x0C, f.flags );
	put64( h + 0x10, fileBytes );
	put64( h + 0x18, tableOffset );
	put64( h + 0x20, payloadOffset );
	put64( h + 0x28, f.vhgtCorpusHash );
	put64( h + 0x30, f.paintCorpusHash );
	const QByteArray edid = f.worldspaceEdid.toLatin1();
	std::memcpy( h + 0x38, edid.constData(), size_t( qMin<qsizetype>( edid.size(), 31 ) ) );
	const qint16 rect[8] = { f.south, f.west, f.north, f.east,
		f.worldSouth, f.worldWest, f.worldNorth, f.worldEast };
	for ( int i = 0; i < 8; i++ )
		put16( h + 0x58 + i * 2, quint16( rect[i] ) );
	put16( h + 0x68, f.levelDim );
	put16( h + 0x6A, f.levelIndex );
	put16( h + 0x6C, f.levelCount );
	put16( h + 0x6E, f.tilesX );
	put16( h + 0x70, f.tilesY );
	put16( h + 0x72, f.contentTexels );
	put16( h + 0x74, f.borderTexels );
	put16( h + 0x76, f.storedTexels );
	put8( h + 0x78, f.mipCount );
	put8( h + 0x79, f.sheetCount );
	put8( h + 0x7A, f.anisoSupported );
	put8( h + 0x7B, f.compression );
	put32( h + 0x7C, tileCount );
	putF32( h + 0x80, f.coverNormalisation );
	putF32( h + 0x84, f.tintStrength );
	for ( int i = 0; i < 8; i++ )
		put16( h + 0x88 + i * 2, f.levelDims[i] );
	// 0x98 indexCrc32 stays zero here, 0x9C reserved0 zero
	for ( int i = 0; i < 4; i++ ) {
		unsigned char * s = h + 0xA0 + i * 8;
		put16( s + 0, f.sheets[i].dxgiFormat );
		put16( s + 2, f.sheets[i].dxgiFormatCover );
		put8( s + 4, f.sheets[i].role );
		put8( s + 5, f.sheets[i].colorSpace );
	}
	// 0xC0..0xFF reserved, already zero
}

bool readHeaderBytes( const unsigned char * h, LodvHeaderFields & f,
	quint64 & fileBytes, quint64 & tableOffset, quint64 & payloadOffset,
	quint32 & tileCount, quint32 & indexCrc )
{
	fileBytes = get64( h + 0x10 );
	tableOffset = get64( h + 0x18 );
	payloadOffset = get64( h + 0x20 );
	f.flags = get32( h + 0x0C );
	f.vhgtCorpusHash = get64( h + 0x28 );
	f.paintCorpusHash = get64( h + 0x30 );
	{
		int n = 0;
		while ( n < 32 && h[0x38 + n] )
			n++;
		f.worldspaceEdid = QString::fromLatin1( reinterpret_cast<const char *>( h + 0x38 ), n );
	}
	qint16 rect[8];
	for ( int i = 0; i < 8; i++ )
		rect[i] = qint16( get16( h + 0x58 + i * 2 ) );
	f.south = rect[0]; f.west = rect[1]; f.north = rect[2]; f.east = rect[3];
	f.worldSouth = rect[4]; f.worldWest = rect[5]; f.worldNorth = rect[6]; f.worldEast = rect[7];
	f.levelDim = get16( h + 0x68 );
	f.levelIndex = get16( h + 0x6A );
	f.levelCount = get16( h + 0x6C );
	f.tilesX = get16( h + 0x6E );
	f.tilesY = get16( h + 0x70 );
	f.contentTexels = get16( h + 0x72 );
	f.borderTexels = get16( h + 0x74 );
	f.storedTexels = get16( h + 0x76 );
	f.mipCount = h[0x78];
	f.sheetCount = h[0x79];
	f.anisoSupported = h[0x7A];
	f.compression = h[0x7B];
	tileCount = get32( h + 0x7C );
	f.coverNormalisation = getF32( h + 0x80 );
	f.tintStrength = getF32( h + 0x84 );
	for ( int i = 0; i < 8; i++ )
		f.levelDims[i] = get16( h + 0x88 + i * 2 );
	indexCrc = get32( h + 0x98 );
	for ( int i = 0; i < 4; i++ ) {
		const unsigned char * s = h + 0xA0 + i * 8;
		f.sheets[i].dxgiFormat = get16( s + 0 );
		f.sheets[i].dxgiFormatCover = get16( s + 2 );
		f.sheets[i].role = s[4];
		f.sheets[i].colorSpace = s[5];
	}
	return true;
}

} // namespace

quint32 lodvSheetMipBytes( const LodvHeaderFields & h, int sheet, int mip, bool cover )
{
	if ( sheet < 0 || sheet >= int( h.sheetCount ) || mip < 0 || mip >= int( h.mipCount ) )
		return 0;
	const quint32 s = quint32( h.storedTexels ) >> mip;
	if ( h.sheets[sheet].role == LODV_ROLE_HEIGHT )
		return s * s * 2;
	const quint16 fmt = ( h.sheets[sheet].role == LODV_ROLE_DATA && cover )
		? h.sheets[sheet].dxgiFormatCover : h.sheets[sheet].dxgiFormat;
	const quint32 blockBytes = ( fmt == LODV_DXGI_BC3_UNORM || fmt == LODV_DXGI_BC3_UNORM_SRGB ) ? 16 : 8;
	return ( s / 4 ) * ( s / 4 ) * blockBytes;
}

quint32 lodvTileRawBytes( const LodvHeaderFields & h, bool cover )
{
	quint32 n = 0;
	for ( int s = 0; s < int( h.sheetCount ); s++ )
		for ( int m = 0; m < int( h.mipCount ); m++ )
			n += lodvSheetMipBytes( h, s, m, cover );
	return n;
}

quint32 lodvCrc32( const unsigned char * p, qsizetype n, quint32 seed )
{
	static quint32 table[256];
	static bool built = false;
	if ( !built ) {
		for ( quint32 i = 0; i < 256; i++ ) {
			quint32 c = i;
			for ( int k = 0; k < 8; k++ )
				c = ( c & 1 ) ? ( 0xEDB88320U ^ ( c >> 1 ) ) : ( c >> 1 );
			table[i] = c;
		}
		built = true;
	}
	quint32 c = seed ^ 0xFFFFFFFFU;
	for ( qsizetype i = 0; i < n; i++ )
		c = table[( c ^ p[i] ) & 0xFF] ^ ( c >> 8 );
	return c ^ 0xFFFFFFFFU;
}

/* ---------------------------------------------------------------- writer -- */

struct LodvWriter::Impl
{
	QFile f;
	LodvHeaderFields fields;
	std::vector<LodvTileEntry> table;
	quint64 tableOffset = LODV_HEADER_BYTES;
	quint64 payloadOffset = 0;
	quint64 cursor = 0;
	int written = 0;
	int present = 0;
	bool open = false;
};

LodvWriter::LodvWriter() : d( new Impl ) {}

LodvWriter::~LodvWriter()
{
	if ( d && d->f.isOpen() )
		d->f.close();
}

bool LodvWriter::begin( const QString & path, const LodvHeaderFields & fields, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( fields.worldspaceEdid.isEmpty() || fields.worldspaceEdid.size() > 31 )
		return fail( QString( "worldspace %1 has a %2-character EDID; the container stores 32 bytes "
			"with a terminator, so this worldspace cannot be named in a .lodt" )
			.arg( fields.worldspaceEdid ).arg( fields.worldspaceEdid.size() ) );
	if ( fields.tilesX < 1 || fields.tilesY < 1 )
		return fail( QStringLiteral( "a level with no tiles" ) );
	d->fields = fields;
	d->table.assign( size_t( fields.tilesX ) * size_t( fields.tilesY ), LodvTileEntry() );
	d->tableOffset = LODV_HEADER_BYTES;
	d->payloadOffset = alignUp( d->tableOffset + quint64( LODV_TABLE_STRIDE ) * d->table.size(),
		LODV_PAYLOAD_ALIGN );
	d->cursor = d->payloadOffset;
	d->written = 0;
	d->present = 0;
	d->f.setFileName( path );
	if ( !d->f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QString( "could not open %1 for writing" ).arg( path ) );
	// reserve the header and the table; every reserved byte is zero, and the
	// pad up to payloadOffset stays zero, which is what makes two runs match
	QByteArray zero( qsizetype( d->payloadOffset ), '\0' );
	if ( d->f.write( zero ) != zero.size() )
		return fail( QStringLiteral( "short write reserving the tile table" ) );
	d->open = true;
	return true;
}

bool LodvWriter::addTile( const QByteArray & raw, bool cover, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !d->open )
		return fail( QStringLiteral( "addTile before begin" ) );
	if ( size_t( d->written ) >= d->table.size() )
		return fail( QStringLiteral( "more tiles than the table holds" ) );
	QByteArray stored = raw;
	if ( d->fields.compression == 1 ) {
		/* qCompress emits a 4-byte big-endian raw size followed by a plain
		 * RFC 1950 stream; the tail IS that stream, at zlib's default window
		 * (CINFO 7) with FDICT clear, which is exactly what the consumer's
		 * hand-written header-only inflater accepts. */
		const QByteArray z = qCompress( raw, 6 );
		if ( z.size() <= 4 )
			return fail( QStringLiteral( "compression failed" ) );
		stored = z.mid( 4 );
	}
	const quint64 at = alignUp( d->cursor, LODV_PAYLOAD_ALIGN );
	if ( at > d->cursor ) {
		const QByteArray pad( qsizetype( at - d->cursor ), '\0' );
		if ( d->f.write( pad ) != pad.size() )
			return fail( QStringLiteral( "short write padding a payload" ) );
	}
	if ( d->f.write( stored ) != stored.size() )
		return fail( QStringLiteral( "short write of a tile payload" ) );
	LodvTileEntry & e = d->table[size_t( d->written )];
	e.offset = at;
	e.storedBytes = quint32( stored.size() );
	e.rawBytes = quint32( raw.size() );
	e.crc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( stored.constData() ),
		stored.size() );
	e.flags = quint16( LODV_TILE_PRESENT | ( cover ? LODV_TILE_COVER : 0 ) );
	e.reserved = 0;
	d->cursor = at + quint64( stored.size() );
	d->written++;
	d->present++;
	return true;
}

bool LodvWriter::addAbsent( QString * error )
{
	if ( !d->open || size_t( d->written ) >= d->table.size() ) {
		if ( error )
			*error = QStringLiteral( "addAbsent out of range" );
		return false;
	}
	d->table[size_t( d->written )] = LodvTileEntry();
	d->written++;
	return true;
}

bool LodvWriter::finish( QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !d->open )
		return fail( QStringLiteral( "finish before begin" ) );
	if ( size_t( d->written ) != d->table.size() )
		return fail( QString( "%1 tiles written, %2 in the table" )
			.arg( d->written ).arg( qulonglong( d->table.size() ) ) );
	if ( d->present == 0 )
		return fail( QStringLiteral( "every tile is absent; that is a broken bake, not an empty world" ) );

	QByteArray tableBytes( qsizetype( quint64( LODV_TABLE_STRIDE ) * d->table.size() ), '\0' );
	unsigned char * t = reinterpret_cast<unsigned char *>( tableBytes.data() );
	for ( size_t i = 0; i < d->table.size(); i++ ) {
		const LodvTileEntry & e = d->table[i];
		unsigned char * p = t + i * LODV_TABLE_STRIDE;
		if ( !( e.flags & LODV_TILE_PRESENT ) )
			continue;           // an absent entry's 24 bytes stay ALL zero
		put64( p + 0x00, e.offset );
		put32( p + 0x08, e.storedBytes );
		put32( p + 0x0C, e.rawBytes );
		put32( p + 0x10, e.crc32 );
		put16( p + 0x14, e.flags );
		put16( p + 0x16, e.reserved );
	}

	unsigned char hdr[LODV_HEADER_BYTES];
	writeHeaderBytes( hdr, d->fields, d->cursor, d->tableOffset, d->payloadOffset,
		quint32( d->table.size() ) );
	/* indexCrc32 covers the header with the field ZEROED plus the whole table.
	 * Per-payload CRCs cannot see offset aliasing: a flipped bit in an offset
	 * points the reader at another tile's payload, whose own CRC is valid, and
	 * it loads the wrong tile and never notices. */
	quint32 crc = lodvCrc32( hdr, LODV_HEADER_BYTES );
	crc = lodvCrc32( reinterpret_cast<const unsigned char *>( tableBytes.constData() ),
		tableBytes.size(), crc );
	put32( hdr + 0x98, crc );

	if ( !d->f.seek( 0 ) || d->f.write( reinterpret_cast<const char *>( hdr ), LODV_HEADER_BYTES )
		!= LODV_HEADER_BYTES )
		return fail( QStringLiteral( "could not patch the header" ) );
	if ( !d->f.seek( qint64( d->tableOffset ) )
		|| d->f.write( tableBytes ) != tableBytes.size() )
		return fail( QStringLiteral( "could not patch the tile table" ) );
	d->f.close();
	d->open = false;
	return true;
}

int LodvWriter::tilesWritten() const { return d->written; }
int LodvWriter::tilesPresent() const { return d->present; }
quint64 LodvWriter::fileBytes() const { return d->cursor; }

/* ------------------------------------------------------------- validator -- */

bool lodvValidate( const QString & path, LodvHeaderFields * fieldsOut,
	std::vector<LodvTileEntry> * tableOut, bool payloadCheck, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QString( "could not open %1" ).arg( path ) );
	const quint64 actual = quint64( f.size() );
	if ( actual < LODV_HEADER_BYTES )                                    // rule 1
		return fail( QStringLiteral( "refused: file smaller than the 256-byte header" ) );
	QByteArray hb = f.read( LODV_HEADER_BYTES );
	if ( hb.size() != LODV_HEADER_BYTES )
		return fail( QStringLiteral( "refused: short read of the header" ) );
	const unsigned char * h = reinterpret_cast<const unsigned char *>( hb.constData() );
	if ( get32( h + 0x00 ) != LODTEX_MAGIC ) {                           // rule 2
		/* NAME what the file actually is. `.lodt` named the LANDSCAPE format
		 * until 2026-09-09 and `.lodv` named this one, so both are files a
		 * reader will really be handed, and "bad magic" would tell whoever is
		 * holding one nothing about which mistake they made. */
		const quint32 m = get32( h + 0x00 );
		if ( m == LODL_MAGIC )
			return fail( QStringLiteral( "refused: this is the whole-worldspace LANDSCAPE "
				"file (magic LODT), which is called .lodl since 2026-09-09; .lodt names "
				"the terrain texture sheets now -- open it as .lodl" ) );
		if ( m == LODTEX_MAGIC_RETIRED_LODV )
			return fail( QStringLiteral( "refused: this is a retired .lodv container "
				"(magic LODV); the terrain texture sheets are .lodt with magic LDTX "
				"since 2026-09-09 -- re-bake it" ) );
		return fail( QStringLiteral( "refused: magic is not LDTX" ) );
	}
	if ( get32( h + 0x04 ) != LODV_VERSION )                             // rule 3
		return fail( QString( "refused: version %1, this reader knows 1" ).arg( get32( h + 0x04 ) ) );
	if ( get32( h + 0x08 ) != LODV_HEADER_BYTES )                        // rule 4
		return fail( QStringLiteral( "refused: headerBytes is not 256" ) );

	LodvHeaderFields fd;
	quint64 fileBytes = 0, tableOffset = 0, payloadOffset = 0;
	quint32 tileCount = 0, indexCrc = 0;
	readHeaderBytes( h, fd, fileBytes, tableOffset, payloadOffset, tileCount, indexCrc );

	if ( fileBytes != actual )                                           // rule 5
		return fail( QString( "refused: fileBytes says %1, the file is %2" )
			.arg( fileBytes ).arg( actual ) );
	{                                                                     // rule 6
		int n = 0;
		while ( n < 32 && h[0x38 + n] )
			n++;
		if ( n == 32 || n == 0 )
			return fail( QStringLiteral( "refused: worldspaceEdid is empty or not NUL-terminated" ) );
		for ( int i = 0; i < n; i++ )
			if ( h[0x38 + i] < 0x20 || h[0x38 + i] > 0x7E )
				return fail( QStringLiteral( "refused: worldspaceEdid holds a non-printable byte" ) );
	}
	if ( fd.north < fd.south || fd.east < fd.west                        // rule 7
		|| fd.worldNorth < fd.worldSouth || fd.worldEast < fd.worldWest
		|| fd.west > fd.worldWest || fd.south > fd.worldSouth
		|| fd.east < fd.worldEast || fd.north < fd.worldNorth )
		return fail( QStringLiteral( "refused: the padded rectangle does not contain the world rectangle" ) );
	{                                                                     // rule 8
		const int d = int( fd.levelDim );
		if ( d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32 )
			return fail( QString( "refused: levelDim %1 is not 1, 2, 4, 8, 16 or 32" ).arg( d ) );
		auto trueMod = []( int v, int m ) { const int r = v % m; return r < 0 ? r + m : r; };
		if ( trueMod( fd.west, d ) != 0 || trueMod( fd.south, d ) != 0 )
			return fail( QStringLiteral( "refused: west or south is not a multiple of levelDim" ) );
	}
	{                                                                     // rule 9
		const int d = int( fd.levelDim );
		const int spanX = int( fd.east ) - int( fd.west ) + 1;
		const int spanY = int( fd.north ) - int( fd.south ) + 1;
		if ( spanX % d != 0 || spanY % d != 0 )
			return fail( QStringLiteral( "refused: the padded span is not a whole number of tiles" ) );
		if ( int( fd.tilesX ) != spanX / d || int( fd.tilesY ) != spanY / d
			|| fd.tilesX < 1 || fd.tilesY < 1 )
			return fail( QStringLiteral( "refused: tilesX or tilesY disagrees with the rectangle" ) );
	}
	if ( quint64( tileCount ) != quint64( fd.tilesX ) * quint64( fd.tilesY ) )   // rule 10
		return fail( QStringLiteral( "refused: tileCount is not tilesX * tilesY" ) );
	{                                                                     // rule 11
		const int c = int( fd.contentTexels );
		if ( c < 128 || c > 1024 || ( c & ( c - 1 ) ) != 0 )
			return fail( QStringLiteral( "refused: contentTexels is not a power of two in 128..1024" ) );
		if ( fd.borderTexels % 4 != 0 )
			return fail( QStringLiteral( "refused: borderTexels is not a multiple of 4" ) );
		if ( int( fd.storedTexels ) != c + 2 * int( fd.borderTexels ) )
			return fail( QStringLiteral( "refused: storedTexels is not content + 2 * border" ) );
	}
	{                                                                     // rule 12
		const int m = int( fd.mipCount );
		if ( m < 1 )
			return fail( QStringLiteral( "refused: mipCount is zero" ) );
		const int b = int( fd.borderTexels );
		if ( ( b >> ( m - 1 ) ) % 4 != 0 || ( ( b >> ( m - 1 ) ) << ( m - 1 ) ) != b
			|| ( int( fd.contentTexels ) >> ( m - 1 ) ) < 4 )
			return fail( QString( "refused: border %1 cannot carry %2 mips; the border halves at "
				"every mip and must stay a multiple of 4" ).arg( b ).arg( m ) );
	}
	{                                                                     // rule 13
		if ( fd.sheetCount < 1 || fd.sheetCount > 4 )
			return fail( QStringLiteral( "refused: sheetCount outside 1..4" ) );
		bool seen[5] = { false, false, false, false, false };
		for ( int i = 0; i < int( fd.sheetCount ); i++ ) {
			const LodvSheetDesc & s = fd.sheets[i];
			if ( s.role == LODV_ROLE_UNUSED || s.role > LODV_ROLE_HEIGHT )
				return fail( QString( "refused: sheet %1 has role %2" ).arg( i ).arg( s.role ) );
			if ( seen[s.role] )
				return fail( QString( "refused: role %1 appears twice" ).arg( s.role ) );
			seen[s.role] = true;
			const bool height = ( s.role == LODV_ROLE_HEIGHT );
			if ( !validFormat( s.dxgiFormat, height ) || !validFormat( s.dxgiFormatCover, height ) )
				return fail( QString( "refused: sheet %1 has dxgiFormat %2 / %3" )
					.arg( i ).arg( s.dxgiFormat ).arg( s.dxgiFormatCover ) );
			if ( s.colorSpace > 1 )
				return fail( QString( "refused: sheet %1 has colorSpace %2" ).arg( i ).arg( s.colorSpace ) );
			if ( s.role != LODV_ROLE_DATA && s.dxgiFormatCover != s.dxgiFormat )
				return fail( QString( "refused: sheet %1 is not the data sheet and its "
					"dxgiFormatCover differs" ).arg( i ) );
		}
		for ( int i = int( fd.sheetCount ); i < 4; i++ )
			if ( fd.sheets[i].role != 0 || fd.sheets[i].dxgiFormat != 0
				|| fd.sheets[i].dxgiFormatCover != 0 || fd.sheets[i].colorSpace != 0 )
				return fail( QString( "refused: sheet %1 is past sheetCount and not zero" ).arg( i ) );
	}
	if ( fd.compression > 1 )                                            // rule 14
		return fail( QString( "refused: compression %1 is not 0 or 1" ).arg( fd.compression ) );
	{                                                                     // rule 15
		if ( tableOffset < LODV_HEADER_BYTES || ( tableOffset % 8 ) != 0 )
			return fail( QStringLiteral( "refused: tileTableOffset is below the header or not 8-aligned" ) );
		if ( tableOffset + quint64( LODV_TABLE_STRIDE ) * quint64( tileCount ) > payloadOffset )
			return fail( QStringLiteral( "refused: the tile table runs into payloadOffset" ) );
		if ( payloadOffset > fileBytes || ( payloadOffset % LODV_PAYLOAD_ALIGN ) != 0 )
			return fail( QStringLiteral( "refused: payloadOffset is past the end or not 4096-aligned" ) );
	}
	if ( !( fd.flags & LODV_FLAG_ROW_ORDER_NORTH_UP ) )                  // rule 19
		return fail( QStringLiteral( "refused: flags bit 0 ROW_ORDER_NORTH_UP is clear and no "
			"other row order is defined" ) );
	{                                                                     // rule 21
		const int bAtCoarsest = int( fd.borderTexels ) >> ( int( fd.mipCount ) - 1 );
		if ( int( fd.anisoSupported ) > 2 * bAtCoarsest )
			return fail( QString( "refused: anisoSupported %1 needs a border of at least %2 at the "
				"coarsest stored mip" ).arg( fd.anisoSupported ).arg( ( fd.anisoSupported + 1 ) / 2 ) );
	}
	{                                                                     // rule 22
		int prefix = 0;
		while ( prefix < 8 && fd.levelDims[prefix] )
			prefix++;
		for ( int i = prefix; i < 8; i++ )
			if ( fd.levelDims[i] )
				return fail( QStringLiteral( "refused: levelDims has a hole" ) );
		for ( int i = 1; i < prefix; i++ )
			if ( fd.levelDims[i] <= fd.levelDims[i - 1] )
				return fail( QStringLiteral( "refused: levelDims is not strictly ascending" ) );
		if ( prefix != int( fd.levelCount ) )
			return fail( QString( "refused: levelCount %1 but levelDims has %2 entries" )
				.arg( fd.levelCount ).arg( prefix ) );
		if ( fd.levelIndex >= fd.levelCount )
			return fail( QStringLiteral( "refused: levelIndex is not inside levelCount" ) );
		if ( fd.levelDims[fd.levelIndex] != fd.levelDim )
			return fail( QStringLiteral( "refused: levelDims[levelIndex] is not levelDim" ) );
	}

	// the table
	if ( !f.seek( qint64( tableOffset ) ) )
		return fail( QStringLiteral( "refused: cannot seek to the tile table" ) );
	const qint64 tableBytes = qint64( LODV_TABLE_STRIDE ) * qint64( tileCount );
	QByteArray tb = f.read( tableBytes );
	if ( tb.size() != tableBytes )
		return fail( QStringLiteral( "refused: short read of the tile table" ) );
	{                                                                     // rule 20
		QByteArray hz = hb;
		unsigned char * hp = reinterpret_cast<unsigned char *>( hz.data() );
		put32( hp + 0x98, 0 );
		quint32 crc = lodvCrc32( hp, LODV_HEADER_BYTES );
		crc = lodvCrc32( reinterpret_cast<const unsigned char *>( tb.constData() ), tb.size(), crc );
		if ( crc != indexCrc )
			return fail( QString( "refused: indexCrc32 %1, recomputed %2" )
				.arg( indexCrc, 8, 16, QChar( '0' ) ).arg( crc, 8, 16, QChar( '0' ) ) );
	}

	// static_cast, not size_t(...): the functional cast makes this a most vexing
	// parse - a function declaration taking an unnamed size_t, not a vector.
	std::vector<LodvTileEntry> table( static_cast<size_t>( tileCount ) );
	int present = 0;
	const unsigned char * tp = reinterpret_cast<const unsigned char *>( tb.constData() );
	for ( quint32 i = 0; i < tileCount; i++ ) {                          // rule 16
		const unsigned char * p = tp + size_t( i ) * LODV_TABLE_STRIDE;
		LodvTileEntry & e = table[size_t( i )];
		e.offset = get64( p + 0x00 );
		e.storedBytes = get32( p + 0x08 );
		e.rawBytes = get32( p + 0x0C );
		e.crc32 = get32( p + 0x10 );
		e.flags = get16( p + 0x14 );
		e.reserved = get16( p + 0x16 );
		const bool flagged = ( e.flags & LODV_TILE_PRESENT ) != 0;
		if ( flagged != ( e.offset != 0 ) )
			return fail( QString( "refused: tile %1 PRESENT disagrees with offset" ).arg( i ) );
		if ( !flagged ) {
			for ( int k = 0; k < int( LODV_TABLE_STRIDE ); k++ )
				if ( p[k] )
					return fail( QString( "refused: absent tile %1 has non-zero table bytes" ).arg( i ) );
			continue;
		}
		present++;
		if ( e.flags & ~quint16( LODV_TILE_PRESENT | LODV_TILE_COVER ) )
			return fail( QString( "refused: tile %1 has an unknown flag bit" ).arg( i ) );
		if ( e.reserved )
			return fail( QString( "refused: tile %1 has a non-zero reserved field" ).arg( i ) );
		if ( e.offset < payloadOffset || ( e.offset % LODV_PAYLOAD_ALIGN ) != 0 )
			return fail( QString( "refused: tile %1 offset is below payloadOffset or not 4096-aligned" ).arg( i ) );
		if ( e.storedBytes == 0 || e.offset + e.storedBytes > fileBytes )
			return fail( QString( "refused: tile %1 storedBytes runs past the end" ).arg( i ) );
		const quint32 want = lodvTileRawBytes( fd, ( e.flags & LODV_TILE_COVER ) != 0 );
		if ( e.rawBytes != want )
			return fail( QString( "refused: tile %1 rawBytes %2, the header implies %3" )
				.arg( i ).arg( e.rawBytes ).arg( want ) );
		if ( fd.compression == 0 && e.storedBytes != e.rawBytes )
			return fail( QString( "refused: tile %1 is uncompressed and storedBytes != rawBytes" ).arg( i ) );
	}
	if ( present == 0 )                                                  // rule 17
		return fail( QStringLiteral( "refused: every tile is absent; a level with no tiles is a "
			"broken bake, not an empty world" ) );

	if ( payloadCheck ) {
		for ( quint32 i = 0; i < tileCount; i++ ) {
			const LodvTileEntry & e = table[size_t( i )];
			if ( !( e.flags & LODV_TILE_PRESENT ) )
				continue;
			if ( !f.seek( qint64( e.offset ) ) )
				return fail( QString( "refused: cannot seek to tile %1" ).arg( i ) );
			const QByteArray p = f.read( qint64( e.storedBytes ) );
			if ( p.size() != qint64( e.storedBytes ) )
				return fail( QString( "refused: short read of tile %1" ).arg( i ) );
			if ( fd.compression == 1 ) {                                 // rule 16b
				if ( p.size() < 2 )
					return fail( QString( "refused: tile %1 is too short for a zlib header" ).arg( i ) );
				const quint8 b0 = quint8( p[0] ), b1 = quint8( p[1] );
				if ( ( b0 & 0x0F ) != 8 || ( b0 >> 4 ) > 7 || ( b1 & 0x20 )
					|| ( ( quint32( b0 ) << 8 | b1 ) % 31 ) != 0 )
					return fail( QString( "refused: tile %1 is not a CM=8 CINFO<=7 FDICT-clear "
						"zlib stream" ).arg( i ) );
			}
			const quint32 crc = lodvCrc32( reinterpret_cast<const unsigned char *>( p.constData() ),
				p.size() );
			if ( crc != e.crc32 )
				return fail( QString( "refused: tile %1 crc32 %2, recomputed %3" )
					.arg( i ).arg( e.crc32, 8, 16, QChar( '0' ) ).arg( crc, 8, 16, QChar( '0' ) ) );
		}
	}

	if ( fieldsOut )
		*fieldsOut = fd;
	if ( tableOut )
		*tableOut = table;
	if ( error )
		error->clear();
	return true;
}

QStringList lodvDescribe( const LodvHeaderFields & h, const std::vector<LodvTileEntry> & table )
{
	QStringList out;
	auto kv = [&out]( const char * k, const QString & v ) {
		QString line = QString::fromLatin1( k );
		line += QChar( ' ' );
		line += v;
		out << line;
	};
	auto kvi = [&kv]( const char * k, qint64 v ) { kv( k, QString::number( v ) ); };
	kv( "magic", QStringLiteral( "LDTX" ) );
	kvi( "version", 1 );
	kvi( "headerBytes", 256 );
	kvi( "flags", h.flags );
	kv( "worldspace", h.worldspaceEdid );
	kvi( "south", h.south );
	kvi( "west", h.west );
	kvi( "north", h.north );
	kvi( "east", h.east );
	kvi( "worldSouth", h.worldSouth );
	kvi( "worldWest", h.worldWest );
	kvi( "worldNorth", h.worldNorth );
	kvi( "worldEast", h.worldEast );
	kvi( "levelDim", h.levelDim );
	kvi( "levelIndex", h.levelIndex );
	kvi( "levelCount", h.levelCount );
	kvi( "tilesX", h.tilesX );
	kvi( "tilesY", h.tilesY );
	kvi( "contentTexels", h.contentTexels );
	kvi( "borderTexels", h.borderTexels );
	kvi( "storedTexels", h.storedTexels );
	kvi( "mipCount", h.mipCount );
	kvi( "sheetCount", h.sheetCount );
	kvi( "anisoSupported", h.anisoSupported );
	kvi( "compression", h.compression );
	kvi( "tileCount", qint64( table.size() ) );
	kv( "coverNormalisation", QString::number( double( h.coverNormalisation ), 'f', 1 ) );
	kv( "tintStrength", QString::number( double( h.tintStrength ), 'f', 3 ) );
	{
		QStringList d;
		for ( int i = 0; i < 8; i++ )
			d << QString::number( h.levelDims[i] );
		kv( "levelDims", d.join( QChar( ',' ) ) );
	}
	kv( "vhgtCorpusHash", QStringLiteral( "0x" )
		+ QString::number( h.vhgtCorpusHash, 16 ).toUpper().rightJustified( 16, QChar( '0' ) ) );
	kv( "paintCorpusHash", QStringLiteral( "0x" )
		+ QString::number( h.paintCorpusHash, 16 ).toUpper().rightJustified( 16, QChar( '0' ) ) );
	for ( int i = 0; i < int( h.sheetCount ); i++ )
		out << QString( "sheet %1 role %2 dxgi %3 dxgiWithCover %4 colorSpace %5" )
			.arg( i ).arg( h.sheets[i].role ).arg( h.sheets[i].dxgiFormat )
			.arg( h.sheets[i].dxgiFormatCover ).arg( h.sheets[i].colorSpace );
	int present = 0, cover = 0;
	quint64 stored = 0;
	for ( const LodvTileEntry & e : table ) {
		if ( !( e.flags & LODV_TILE_PRESENT ) )
			continue;
		present++;
		if ( e.flags & LODV_TILE_COVER )
			cover++;
		stored += e.storedBytes;
	}
	kvi( "present", present );
	kvi( "coverTiles", cover );
	kvi( "storedBytesTotal", qint64( stored ) );
	return out;
}
