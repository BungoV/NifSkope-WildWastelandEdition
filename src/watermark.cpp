/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "watermark.h"

#include "watercurves.h"        // lane WATER6: WaterRasterLayer, the PNG codec

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QHash>                 // lane WATER7: X2b's ring, keyed by texel
#include <QImage>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <vector>
#include <QList>
#include <QPointF>
#include <functional>
#include <limits>

/* =========================================================================
 *  Marking water direction: the strokes are the source, the planes are derived
 *
 *  Read watermark.h first; this file is the implementation of what it states.
 *
 *  THE ONE THING TO KNOW. Nothing here edits a plane. `solve()` builds a field
 *  per marked body and `save()` re-derives the whole flow plane from
 *  (body-ID plane + body table + that field), which is the same function the
 *  WRITER computes -- so a document nobody has marked re-derives to the
 *  writer's own bytes, and the harness asserts exactly that before it trusts
 *  anything else. The body-ID and shore planes are copied through verbatim,
 *  rebased, because nothing this tool does can move a body's SHAPE.
 * ========================================================================= */

namespace {

// ---- the version-3 header fields this file reads and patches ---------------
constexpr qsizetype kHdrV3      = 0xF8;
constexpr qsizetype kOffSect    = 0x44;
constexpr qsizetype kOffSize    = 0x90;
constexpr qsizetype kOffBody    = 0xA0;
constexpr qsizetype kOffBodyN   = 0xA8;
constexpr qsizetype kOffStride  = 0xAC;
constexpr qsizetype kOffName    = 0xB0;
constexpr qsizetype kOffNameLen = 0xB8;
constexpr qsizetype kOffIdRate  = 0xBC;
constexpr qsizetype kOffId      = 0xC0;
constexpr qsizetype kOffFlowRt  = 0xC8;
constexpr qsizetype kOffFlowEnc = 0xCC;
constexpr qsizetype kOffFlow    = 0xD0;
constexpr qsizetype kOffShoreRt = 0xD8;
constexpr qsizetype kOffShoreQ  = 0xDC;
constexpr qsizetype kOffShore   = 0xE0;
constexpr qsizetype kOffStroke  = 0xE8;
constexpr qsizetype kOffStrokeL = 0xF0;
constexpr qsizetype kOffDye     = 0xF4;   //!< the reserved word: the dye plane (WATER4)
constexpr quint32 kSectDye = LODL_SECT_DYE;
constexpr int kBodyRecord = 48;
constexpr double kTwoPi = 6.283185307179586;
//! World units a cell edge. FO4's, and the same constant the writer uses.
constexpr double kCellUnits = 4096.0;

inline quint16 rd16( const char * p ) { return quint16( quint8( p[0] ) | ( quint8( p[1] ) << 8 ) ); }
inline quint32 rd32( const char * p )
{
	return quint32( quint8( p[0] ) ) | ( quint32( quint8( p[1] ) ) << 8 )
		| ( quint32( quint8( p[2] ) ) << 16 ) | ( quint32( quint8( p[3] ) ) << 24 );
}
inline quint64 rd64( const char * p )
{
	return quint64( rd32( p ) ) | ( quint64( rd32( p + 4 ) ) << 32 );
}

//! Little-endian append, the writer's own order. One place, so a field cannot drift.
struct Buf
{
	QByteArray b;
	void u8( quint8 v ) { b.append( char( v ) ); }
	void u16( quint16 v ) { u8( quint8( v & 0xFF ) ); u8( quint8( v >> 8 ) ); }
	void u32( quint32 v ) { u16( quint16( v & 0xFFFF ) ); u16( quint16( v >> 16 ) ); }
	void u64( quint64 v ) { u32( quint32( v & 0xFFFFFFFFULL ) ); u32( quint32( v >> 32 ) ); }
	void f32( float v ) { quint32 t; std::memcpy( &t, &v, 4 ); u32( t ); }
	qsizetype size() const { return b.size(); }
};

inline void patch32( QByteArray & h, qsizetype at, quint32 v )
{
	for ( int i = 0; i < 4; i++ )
		h[at + i] = char( ( v >> ( 8 * i ) ) & 0xFF );
}
inline void patch64( QByteArray & h, qsizetype at, quint64 v )
{
	for ( int i = 0; i < 8; i++ )
		h[at + i] = char( ( v >> ( 8 * i ) ) & 0xFF );
}
inline float rdf32( const char * p )
{
	const quint32 t = rd32( p );
	float f;
	std::memcpy( &f, &t, 4 );
	return f;
}

/*! The plane container's packer -- the TWIN of `lodtPackPlane` in lodtfile.cpp.
 *
 *  It is a twin and not a call because lane BUILD4 held that file open while
 *  this one was written (the brief's file rule), and a twin is only safe if
 *  something proves the two agree: the harness re-packs an UNMARKED file's flow
 *  plane and compares it to the bytes the writer put there, byte for byte. If
 *  the two ever drift, that gate goes red on the next run. Retiring the twin
 *  into a shared header is listed as owed in the lane report. */
QByteArray packPlane( int tilesX, int tilesY, int tileEdge, int bytesPerSample,
	quint64 base, const std::function<void( int tx, int ty, quint8 * out )> & fill,
	qint64 * uniformTiles )
{
	const qint64 nTiles = qint64( tilesX ) * tilesY;
	const qint64 hdr = 32;
	const qint64 dirBytes = nTiles * 16;
	Buf head;
	head.u32( quint32( tilesX ) );
	head.u32( quint32( tilesY ) );
	head.u32( quint32( tileEdge ) );
	head.u32( quint32( bytesPerSample ) );
	head.u64( base + quint64( hdr ) );
	head.u64( base + quint64( hdr + dirBytes ) );

	Buf dir, data;
	quint64 pos = base + quint64( hdr + dirBytes );
	const qsizetype tileBytes = qsizetype( tileEdge ) * tileEdge * bytesPerSample;
	std::vector<quint8> raw( size_t( tileBytes ), quint8( 0 ) );
	qint64 uniform = 0;
	for ( int ty = 0; ty < tilesY; ty++ ) {
		for ( int tx = 0; tx < tilesX; tx++ ) {
			fill( tx, ty, raw.data() );
			bool same = true;
			for ( qsizetype k = bytesPerSample; k < tileBytes && same; k += bytesPerSample )
				for ( int c = 0; c < bytesPerSample; c++ )
					if ( raw[size_t( k + c )] != raw[size_t( c )] ) {
						same = false;
						break;
					}
			if ( same ) {
				quint32 v = 0;
				for ( int c = 0; c < bytesPerSample; c++ )
					v |= quint32( raw[size_t( c )] ) << ( 8 * c );
				dir.u64( 0 );
				dir.u32( 0 );
				dir.u32( v );
				uniform++;
				continue;
			}
			QByteArray z = qCompress( QByteArray( reinterpret_cast<const char *>( raw.data() ),
				int( tileBytes ) ), 9 );
			z.remove( 0, 4 );
			dir.u64( pos );
			dir.u32( quint32( z.size() ) );
			dir.u32( quint32( tileBytes ) );
			data.b.append( z );
			pos += quint64( z.size() );
		}
	}
	if ( uniformTiles )
		*uniformTiles = uniform;
	QByteArray out = head.b;
	out.append( dir.b );
	out.append( data.b );
	return out;
}

/*! Copy a plane container's bytes with every ABSOLUTE offset rebased.
 *
 *  A plane store carries its directory and data offsets as absolute file
 *  positions, so moving a section by N bytes is not a memcpy -- it is a memcpy
 *  plus 2 + tiles patches. A uniform tile's `offset` field is 0 and stays 0:
 *  it is not an offset, it is the absent one. */
QByteArray rebasePlane( const QByteArray & bytes, quint64 oldBase, quint64 newBase )
{
	QByteArray out = bytes;
	if ( out.size() < 32 )
		return out;
	const qint64 delta = qint64( newBase ) - qint64( oldBase );
	const quint32 tilesX = rd32( out.constData() );
	const quint32 tilesY = rd32( out.constData() + 4 );
	patch64( out, 16, quint64( qint64( rd64( out.constData() + 16 ) ) + delta ) );
	patch64( out, 24, quint64( qint64( rd64( out.constData() + 24 ) ) + delta ) );
	const qint64 n = qint64( tilesX ) * tilesY;
	for ( qint64 i = 0; i < n; i++ ) {
		const qsizetype at = qsizetype( 32 + i * 16 );
		if ( at + 16 > out.size() )
			break;
		const quint32 csize = rd32( out.constData() + at + 8 );
		if ( !csize )
			continue;           // uniform: the offset field is not an offset
		patch64( out, at, quint64( qint64( rd64( out.constData() + at ) ) + delta ) );
	}
	return out;
}

//! Squared distance from a point to a segment, and the segment's unit tangent.
double distToSegment( double px, double py, double ax, double ay, double bx, double by,
	double & tx, double & ty )
{
	const double vx = bx - ax, vy = by - ay;
	const double len2 = vx * vx + vy * vy;
	double t = 0.0;
	if ( len2 > 0.0 )
		t = std::max( 0.0, std::min( 1.0, ( ( px - ax ) * vx + ( py - ay ) * vy ) / len2 ) );
	const double cx = ax + t * vx, cy = ay + t * vy;
	const double len = std::sqrt( len2 );
	if ( len > 0.0 ) {
		tx = vx / len;
		ty = vy / len;
	} else {
		tx = 0.0;
		ty = 0.0;
	}
	const double dx = px - cx, dy = py - cy;
	return std::sqrt( dx * dx + dy * dy );
}

} // namespace

/*! One marked body's solved field, on the BODY plane's own grid.
 *
 *  It is stored per body and not as one world-sized array for the reason the
 *  header states: the Commonwealth's body plane is 75 MB and a person marks one
 *  river at a time. The Charles is 352 x 576 texels -- 200 KB of direction and
 *  50 KB of confidence. */
struct WaterMarkDoc::Field
{
	int px0 = 0, py0 = 0, w = 0, h = 0;
	std::vector<quint8> mask;      //!< 1 = this body
	std::vector<float> vx, vy;     //!< the solved direction, unit length in the mask
	std::vector<quint8> conf;      //!< 0..15
	std::vector<quint8> speed;     //!< 0..15, the flow word's nibble
	bool zero = false;             //!< locked to still water
	bool windowed = false;         //!< a cut around the constraints, not the whole bbox
	bool dyeOnly = false;          //!< a receiver's field: dye is written, flow words are not
	WaterFlowGrid grid;            //!< the compacted system over `mask`
	std::vector<double> phi;       //!< the potential, per grid cell
	std::vector<quint32> dye;      //!< the dye word per texel, 0 = none
	size_t at( int px, int py ) const { return size_t( py - py0 ) * size_t( w ) + size_t( px - px0 ); }
	bool holds( int px, int py ) const
	{ return px >= px0 && py >= py0 && px < px0 + w && py < py0 + h; }
};

WaterMarkDoc::WaterMarkDoc() = default;

WaterMarkDoc::~WaterMarkDoc()
{
	qDeleteAll( fields );
	fields.clear();
	delete reader;
	delete lodl;
}

// =========================================================================
//  open
// =========================================================================

bool WaterMarkDoc::open( const QString & path, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	qDeleteAll( fields );
	fields.clear();
	delete reader;
	reader = nullptr;
	delete lodl;
	lodl = new LodtFile();
	opened = false;
	dirty = false;
	table.clear();
	names.clear();
	lockZero.clear();
	marks.clear();
	filePath = path;

	QString err;
	if ( !lodl->open( path, &err ) )
		return fail( err );
	if ( lodl->headerVersion() < 3 )
		return fail( QStringLiteral( "%1 is a version %2 landscape file; marking water needs "
			"version 3 (write it with --water-bodies)" )
			.arg( QFileInfo( path ).fileName() ).arg( lodl->headerVersion() ) );
	if ( lodl->bodyCount() <= 0 )
		return fail( QStringLiteral( "%1 carries no water body table" )
			.arg( QFileInfo( path ).fileName() ) );
	if ( !readTail( error ) )
		return false;

	table.resize( lodl->bodyCount() );
	names.resize( lodl->bodyCount() );
	lockZero.fill( quint8( 0 ), lodl->bodyCount() );
	for ( int i = 1; i <= lodl->bodyCount(); i++ ) {
		LodtWaterBody b;
		if ( !lodl->waterBody( i, b ) )
			return fail( QStringLiteral( "the body table stops at %1 of %2 records" )
				.arg( i - 1 ).arg( lodl->bodyCount() ) );
		table[i - 1] = b;
		names[i - 1] = lodl->bodyName( b );
	}
	if ( !decodeStrokes( lodl->strokeStore(), error ) )
		return false;
	syncLocks();      // the store is the source; the locks are read back out of it

	/* The repack gate's left-hand side, kept from the moment of opening: the
	 * body table exactly as the writer wrote it. encodeTable() must reproduce
	 * these bytes before a single edit, or the twin has drifted. */
	{
		QFile f( path );
		if ( !f.open( QIODevice::ReadOnly ) )
			return fail( QStringLiteral( "could not re-open %1" ).arg( path ) );
		f.seek( qint64( oBody ) );
		originalTable = f.read( qint64( table.size() ) * bodyStride );
	}
	/* And the same table as RECORDS, for solve() to restore the fields it
	 *  derives.  Taken here, after the locks and the strokes are read and
	 *  before anything has solved, so it is the writer's own answer. */
	tableAtOpen = table;

	reader = new QFile( path );
	if ( !reader->open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "could not open %1 for the planes" ).arg( path ) );
	if ( !readPlane( *reader, oId, 2, idPlane, error ) )
		return false;
	if ( !readPlane( *reader, oFlow, 2, flowPlane, error ) )
		return false;
	if ( oShore && !readPlane( *reader, oShore, 1, shorePlane, error ) )
		return false;
	dyePlane = Plane();
	if ( oDye && !readPlane( *reader, oDye, 4, dyePlane, error ) )
		return false;
	opened = true;
	return true;
}

/*! The header fields from 0xA0, and the ORDER the sections must be in.
 *
 *  This tool rewrites everything from the body table to the end of the file, so
 *  it has to know that the body table really is the first of them. A file whose
 *  sections are in another order is REFUSED BY NAME rather than rearranged --
 *  no such file exists today, and silently reordering one would be a rewrite
 *  nobody asked for. */
bool WaterMarkDoc::readTail( QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "could not open %1" ).arg( filePath ) );
	const QByteArray h = f.read( kHdrV3 );
	if ( h.size() < kHdrV3 )
		return fail( QStringLiteral( "%1 is shorter than a version-3 header" ).arg( filePath ) );
	const char * p = h.constData();
	sect = rd32( p + kOffSect );
	oBody = rd64( p + kOffBody );
	bodyStride = rd32( p + kOffStride );
	oName = rd64( p + kOffName );
	nameLen = rd32( p + kOffNameLen );
	idRate = rd32( p + kOffIdRate );
	oId = rd64( p + kOffId );
	flowRate = rd32( p + kOffFlowRt );
	flowEnc = rd32( p + kOffFlowEnc );
	oFlow = rd64( p + kOffFlow );
	shoreRate = rd32( p + kOffShoreRt );
	shoreQuantum = rd32( p + kOffShoreQ );
	oShore = rd64( p + kOffShore );
	oStroke = rd64( p + kOffStroke );
	strokeLen = rd32( p + kOffStrokeL );
	oDye = ( sect & kSectDye ) ? quint64( rd32( p + kOffDye ) ) : quint64( 0 );
	if ( ( sect & kSectDye ) && !oDye )
		return fail( QStringLiteral( "section dye is declared present but its offset is empty" ) );

	if ( bodyStride != kBodyRecord )
		return fail( QStringLiteral( "the body table's records are %1 bytes; this tool writes %2" )
			.arg( bodyStride ).arg( kBodyRecord ) );
	if ( flowEnc != 0 )
		return fail( QStringLiteral( "the flow plane uses encoding %1; this tool knows 0 "
			"(direction 8 / speed 4 / confidence 4)" ).arg( flowEnc ) );
	const quint64 tableEnd = oBody + quint64( lodl->bodyCount() ) * bodyStride;
	if ( oName && oName != tableEnd )
		return fail( QStringLiteral( "the body name blob is at 0x%1, not immediately after the "
			"table at 0x%2; this tool rewrites the tail in the writer's own order" )
			.arg( oName, 0, 16 ).arg( tableEnd, 0, 16 ) );
	const quint64 sizeOnDisk = quint64( QFileInfo( filePath ).size() );
	if ( oDye && !( oDye > oFlow && oDye > oShore && oDye < sizeOnDisk ) )
		return fail( QStringLiteral( "the dye plane at 0x%1 is not after the other planes; this "
			"tool rewrites the tail in the writer's own order and refuses another" )
			.arg( oDye, 0, 16 ) );
	if ( !( oBody < oStroke && oStroke < oId && oId < oFlow
		&& ( !oShore || ( oFlow < oShore && oShore < sizeOnDisk ) ) ) )
		return fail( QStringLiteral( "the water sections are ordered body 0x%1, stroke 0x%2, "
			"id 0x%3, flow 0x%4, shore 0x%5; this tool rewrites the tail in the writer's own "
			"order and refuses another" ).arg( oBody, 0, 16 ).arg( oStroke, 0, 16 )
			.arg( oId, 0, 16 ).arg( oFlow, 0, 16 ).arg( oShore, 0, 16 ) );
	return true;
}

// =========================================================================
//  the stroke store (spec_water.md 3.7)
// =========================================================================

bool WaterMarkDoc::decodeStrokes( const QByteArray & raw, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	marks.clear();
	if ( raw.isEmpty() )
		return true;                      // no store at all: nobody has marked anything
	if ( raw.size() < 4 )
		return fail( QStringLiteral( "the stroke store is %1 bytes; its count alone is 4" )
			.arg( raw.size() ) );
	const quint32 count = rd32( raw.constData() );
	qsizetype at = 4;
	for ( quint32 i = 0; i < count; i++ ) {
		if ( at + 4 > raw.size() )
			return fail( QStringLiteral( "the stroke store ends inside stroke %1 of %2" )
				.arg( i + 1 ).arg( count ) );
		const quint32 recBytes = rd32( raw.constData() + at );
		if ( recBytes < 20 || at + qsizetype( recBytes ) > raw.size() )
			return fail( QStringLiteral( "stroke %1 of %2 declares %3 bytes, which does not fit "
				"the %4-byte store" ).arg( i + 1 ).arg( count ).arg( recBytes ).arg( raw.size() ) );
		const char * p = raw.constData() + at;
		WaterStroke s;
		s.body = rd16( p + 4 );
		s.kind = quint8( p[6] );
		s.flags = quint8( p[7] );
		s.speed = rdf32( p + 8 );
		s.width = rdf32( p + 12 );
		const quint16 n = rd16( p + 16 );
		if ( 20 + qsizetype( n ) * 8 > qsizetype( recBytes ) )
			return fail( QStringLiteral( "stroke %1 says %2 points, which do not fit its %3 bytes" )
				.arg( i + 1 ).arg( n ).arg( recBytes ) );
		s.pts.reserve( n );
		for ( int k = 0; k < n; k++ ) {
			WaterStrokePoint pt;
			pt.x = rdf32( p + 20 + k * 8 );
			pt.y = rdf32( p + 24 + k * 8 );
			s.pts.append( pt );
		}
		if ( s.kind == WaterStroke::DyePin && 20 + qsizetype( n ) * 8 + 4 <= qsizetype( recBytes ) )
			for ( int c = 0; c < 4; c++ )
				s.colour[c] = quint8( p[20 + n * 8 + c] );
		{
			// lane WATER5: whatever follows is kept verbatim (weights, a raster payload)
			const qsizetype base = 20 + qsizetype( n ) * 8 + ( s.kind == WaterStroke::DyePin ? 4 : 0 );
			if ( qsizetype( recBytes ) > base )
				s.extra = QByteArray( p + base, int( qsizetype( recBytes ) - base ) );
		}
		marks.append( s );
		at += qsizetype( recBytes );
	}
	return true;
}

QByteArray WaterMarkDoc::encodeStrokes() const
{
	Buf s;
	s.u32( quint32( marks.size() ) );
	for ( const WaterStroke & m : marks ) {
		Buf r;
		const quint32 recBytes = quint32( 20 + m.pts.size() * 8
			+ ( m.kind == WaterStroke::DyePin ? 4 : 0 ) + m.extra.size() );
		r.u32( recBytes );
		r.u16( m.body );
		r.u8( m.kind );
		r.u8( m.flags );
		r.f32( m.speed );
		r.f32( m.width );
		r.u16( quint16( m.pts.size() ) );
		r.u16( 0 );                       // reserved
		for ( const WaterStrokePoint & p : m.pts ) {
			r.f32( p.x );
			r.f32( p.y );
		}
		if ( m.kind == WaterStroke::DyePin )
			for ( int c = 0; c < 4; c++ )
				r.u8( m.colour[c] );
		r.b.append( m.extra );          // lane WATER5: the trailing bytes, verbatim
		s.b.append( r.b );
	}
	return s.b;
}

QByteArray WaterMarkDoc::encodeTable() const
{
	/* Byte for byte what lodtBuildWater's table loop writes, which is why the
	 * harness can compare an unedited document's encode against the file's own
	 * bytes and call any difference a defect. */
	/* LANE WATER7, red 5b of BUILD10: OFFSET 0 IS THE "NO NAME" SPELLING.
	 *
	 * `LodtFile::bodyName` opens with `if ( !b.nameOffset ... ) return
	 * QString();`, and the generator's own table loop writes a literal 0 for
	 * every body ("name offset: unnamed", src/lodtfile.cpp). So 0 means
	 * nameless to every reader that ships -- and this encoder started its
	 * running offset AT 0, which handed the first named body in any file the
	 * marking tool saves the one offset that cannot be told from "no name".
	 * That affects any file saved with names, not only the harness's.
	 *
	 * The fix is one reserved byte at the head of the name blob (encodeNames
	 * below) rather than a new sentinel, because the sentinel is already
	 * shipped in every reader and cannot be recalled. A document with NO names
	 * writes no blob at all and every offset stays 0, so an unedited document
	 * still re-encodes to the file's own bytes -- which is the property this
	 * function exists to have. */
	Buf t;
	quint32 nameAt = 1;
	for ( int i = 0; i < table.size(); i++ ) {
		const LodtWaterBody & b = table[i];
		t.u16( quint16( i + 1 ) );
		t.u8( b.cls );
		t.u8( b.flags );
		t.f32( b.waterHeight );
		t.u32( b.watrForm );
		t.u32( b.area );
		t.u16( quint16( b.x0 ) );
		t.u16( quint16( b.y0 ) );
		t.u16( quint16( b.x1 ) );
		t.u16( quint16( b.y1 ) );
		t.u16( b.source );
		t.u16( b.outlet );
		t.f32( b.flowX );
		t.f32( b.flowY );
		t.u8( b.colour[0] );
		t.u8( b.colour[1] );
		t.u8( b.colour[2] );
		t.u8( b.colour[3] );
		t.u8( b.confidence );
		t.u8( b.flowSource );
		t.u16( 0 );
		if ( names[i].isEmpty() ) {
			t.u32( 0 );
		} else {
			t.u32( nameAt );
			nameAt += quint32( names[i].toUtf8().size() + 1 );
		}
	}
	return t.b;
}

QByteArray WaterMarkDoc::encodeNames() const
{
	QByteArray blob;
	for ( const QString & n : names ) {
		if ( n.isEmpty() )
			continue;
		/* The reserved first byte (lane WATER7, red 5b). It is written once,
		 * lazily, so a document with no names writes NO blob and its header's
		 * name length stays 0 -- the unedited-file byte identity above depends
		 * on that. encodeTable's running offset starts at 1 to match. */
		if ( blob.isEmpty() )
			blob.append( char( 0 ) );
		blob.append( n.toUtf8() );
		blob.append( char( 0 ) );
	}
	return blob;
}

// =========================================================================
//  the plane container, as it sits on disk
// =========================================================================

bool WaterMarkDoc::readPlane( QFile & f, quint64 at, int bps, Plane & p, QString * error ) const
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !at )
		return fail( QStringLiteral( "a plane section is declared present and its offset is 0" ) );
	f.seek( qint64( at ) );
	const QByteArray head = f.read( 32 );
	if ( head.size() < 32 )
		return fail( QStringLiteral( "the plane store at 0x%1 has no head" ).arg( at, 0, 16 ) );
	p.headAt = at;
	p.tilesX = int( rd32( head.constData() ) );
	p.tilesY = int( rd32( head.constData() + 4 ) );
	p.tileEdge = int( rd32( head.constData() + 8 ) );
	p.bps = int( rd32( head.constData() + 12 ) );
	p.dirAt = rd64( head.constData() + 16 );
	p.dataAt = rd64( head.constData() + 24 );
	if ( p.bps != bps )
		return fail( QStringLiteral( "the plane store at 0x%1 says %2 bytes a sample, not %3" )
			.arg( at, 0, 16 ).arg( p.bps ).arg( bps ) );
	if ( p.tilesX <= 0 || p.tilesY <= 0 || p.tileEdge <= 0 )
		return fail( QStringLiteral( "the plane store at 0x%1 declares %2 x %3 tiles of %4" )
			.arg( at, 0, 16 ).arg( p.tilesX ).arg( p.tilesY ).arg( p.tileEdge ) );
	const qint64 n = qint64( p.tilesX ) * p.tilesY;
	f.seek( qint64( p.dirAt ) );
	p.dir = f.read( n * 16 );
	if ( p.dir.size() != n * 16 )
		return fail( QStringLiteral( "the plane directory at 0x%1 is short" ).arg( p.dirAt, 0, 16 ) );
	/* The container's own extent, which is what the verbatim copy needs: the
	 * head, the directory, and every payload that follows it. */
	quint64 end = p.dataAt;
	for ( qint64 i = 0; i < n; i++ ) {
		const char * e = p.dir.constData() + i * 16;
		const quint32 csize = rd32( e + 8 );
		if ( !csize )
			continue;
		end = qMax( end, rd64( e ) + csize );
	}
	p.bytes = end - at;
	p.ok = true;
	return true;
}

bool WaterMarkDoc::tileOf( QFile & f, const Plane & p, int tx, int ty,
	QByteArray & raw, quint32 & uniform, bool & isUniform ) const
{
	raw.clear();
	uniform = 0;
	isUniform = false;
	if ( tx < 0 || ty < 0 || tx >= p.tilesX || ty >= p.tilesY )
		return false;
	const qint64 k = qint64( ty ) * p.tilesX + tx;
	const char * e = p.dir.constData() + k * 16;
	const quint64 off = rd64( e );
	const quint32 csize = rd32( e + 8 );
	const quint32 usize = rd32( e + 12 );
	if ( !csize ) {
		uniform = usize;
		isUniform = true;
		return true;
	}
	f.seek( qint64( off ) );
	QByteArray z = f.read( qint64( csize ) );
	if ( z.size() != qint64( csize ) )
		return false;
	/* qUncompress wants the four-byte size prefix qCompress writes; the file
	 * stores a plain zlib stream so any consumer can inflate it. */
	QByteArray withLen;
	withLen.resize( 4 );
	for ( int i = 0; i < 4; i++ )
		withLen[i] = char( ( usize >> ( 8 * ( 3 - i ) ) ) & 0xFF );
	withLen.append( z );
	raw = qUncompress( withLen );
	return raw.size() == qint64( usize );
}

quint16 WaterMarkDoc::idAtTexel( int px, int py ) const
{
	if ( !idPlane.ok || !reader )
		return 0;
	const int e = idPlane.tileEdge;
	const int tx = px / e, ty = py / e;
	if ( px < 0 || py < 0 || tx >= idPlane.tilesX || ty >= idPlane.tilesY )
		return 0;
	const quint64 key = ( quint64( ty ) << 32 ) | quint32( tx );
	auto it = idTiles.constFind( key );
	if ( it == idTiles.constEnd() ) {
		QByteArray raw;
		quint32 uni = 0;
		bool isUni = false;
		if ( !tileOf( *reader, idPlane, tx, ty, raw, uni, isUni ) )
			return 0;
		if ( isUni ) {
			raw.resize( 2 );
			raw[0] = char( uni & 0xFF );
			raw[1] = char( ( uni >> 8 ) & 0xFF );
			/* a two-byte tile means "uniform" to the lookup below, which is the
			 * only place that reads it */
		}
		if ( idTiles.size() > 64 ) {
			for ( int i = 0; i < 32 && !idTileOrder.isEmpty(); i++ )
				idTiles.remove( idTileOrder.takeFirst() );
		}
		idTiles.insert( key, raw );
		idTileOrder.append( key );
		it = idTiles.constFind( key );
	}
	const QByteArray & raw = it.value();
	if ( raw.size() == 2 )
		return rd16( raw.constData() );
	const qsizetype at = ( qsizetype( py % e ) * e + ( px % e ) ) * 2;
	if ( at + 2 > raw.size() )
		return 0;
	return rd16( raw.constData() + at );
}

// =========================================================================
//  the body table
// =========================================================================

bool WaterMarkDoc::body( int id, LodtWaterBody & out ) const
{
	if ( id < 1 || id > table.size() )
		return false;
	out = table[id - 1];
	return true;
}

QString WaterMarkDoc::bodyName( int id ) const
{
	if ( id < 1 || id > names.size() )
		return QString();
	return names[id - 1];
}

QVector<quint32> WaterMarkDoc::waterForms() const
{
	QVector<quint32> f;
	if ( !lodl )
		return f;
	/* The forms are the ones the FILE interned, plus the worldspace default --
	 * which the writer deliberately does not intern, because that is what makes
	 * "0xFFFF = inherited" distinguishable. Zero-authoring: no list of water
	 * types is written down anywhere in this tree. */
	if ( lodl->hasDefaultWater() )
		f.append( lodl->defaultWaterType() );
	for ( int i = 0; i < lodl->watrCount(); i++ ) {
		const quint32 w = lodl->watrForm( i );
		if ( w && !f.contains( w ) )
			f.append( w );
	}
	return f;
}

void WaterMarkDoc::setBodyName( int id, const QString & name )
{
	if ( id < 1 || id > names.size() || names[id - 1] == name )
		return;
	names[id - 1] = name;
	table[id - 1].flags |= 1u << 0;
	dirty = true;
}

void WaterMarkDoc::setBodyClass( int id, int cls )
{
	if ( id < 1 || id > table.size() )
		return;
	LodtWaterBody & b = table[id - 1];
	if ( cls < 0 ) {
		b.flags &= quint8( ~( 1u << 5 ) );      // back to the classifier's own answer
	} else {
		b.cls = quint8( qBound( 0, cls, 2 ) );
		b.flags |= quint8( ( 1u << 5 ) | ( 1u << 0 ) );
	}
	dirty = true;
}

void WaterMarkDoc::setBodyColour( int id, quint8 r, quint8 g, quint8 b, bool on )
{
	if ( id < 1 || id > table.size() )
		return;
	LodtWaterBody & rec = table[id - 1];
	rec.colour[0] = r;
	rec.colour[1] = g;
	rec.colour[2] = b;
	rec.colour[3] = on ? quint8( 255 ) : quint8( 0 );
	if ( on )
		rec.flags |= quint8( ( 1u << 2 ) | ( 1u << 0 ) );
	else
		rec.flags &= quint8( ~( 1u << 2 ) );
	dirty = true;
}

void WaterMarkDoc::setBodyForm( int id, quint32 form )
{
	if ( id < 1 || id > table.size() || !form || table[id - 1].watrForm == form )
		return;
	table[id - 1].watrForm = form;
	table[id - 1].flags |= 1u << 0;
	dirty = true;
}

/*! Locked-to-still is a MARK, not a table bit: it is written into the stroke
 *  store as a one-point `ZeroFlow`, at the centre of the body's bounding box,
 *  so it survives every re-derivation the way a stroke does. */
void WaterMarkDoc::setBodyLockZero( int id, bool on )
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return;
	for ( int i = marks.size() - 1; i >= 0; i-- )
		if ( marks[i].kind == WaterStroke::ZeroFlow && int( marks[i].body ) == id )
			marks.removeAt( i );
	if ( on ) {
		WaterStroke s;
		s.body = quint16( id );
		s.kind = WaterStroke::ZeroFlow;
		s.flags = WaterStroke::SetsSpeed;
		s.speed = 0.0f;
		s.width = 0.0f;
		WaterStrokePoint p;
		p.x = float( ( double( b.x0 ) + double( b.x1 ) + 1.0 ) * 0.5 * kCellUnits );
		p.y = float( ( double( b.y0 ) + double( b.y1 ) + 1.0 ) * 0.5 * kCellUnits );
		s.pts.append( p );
		marks.append( s );
	}
	syncLocks();
	dirty = true;
}

bool WaterMarkDoc::bodyLockZero( int id ) const
{
	return id >= 1 && id <= lockZero.size() && lockZero[id - 1];
}

void WaterMarkDoc::syncLocks()
{
	lockZero.fill( quint8( 0 ), table.size() );
	for ( const WaterStroke & s : marks )
		if ( s.kind == WaterStroke::ZeroFlow && s.enabled()
			&& int( s.body ) >= 1 && int( s.body ) <= lockZero.size() )
			lockZero[int( s.body ) - 1] = 1;
}

// =========================================================================
//  geometry
// =========================================================================

double WaterMarkDoc::worldPerTexel() const
{
	return idRate > 0 ? kCellUnits / double( idRate ) : kCellUnits;
}

void WaterMarkDoc::worldBounds( double & x0, double & y0, double & x1, double & y1 ) const
{
	x0 = double( lodl ? lodl->cellMinX() : 0 ) * kCellUnits;
	y0 = double( lodl ? lodl->cellMinY() : 0 ) * kCellUnits;
	x1 = double( lodl ? lodl->cellMaxX() + 1 : 0 ) * kCellUnits;
	y1 = double( lodl ? lodl->cellMaxY() + 1 : 0 ) * kCellUnits;
}

void WaterMarkDoc::worldToTexel( double wx, double wy, int & px, int & py ) const
{
	const double u = worldPerTexel();
	px = int( std::floor( ( wx - double( lodl->cellMinX() ) * kCellUnits ) / u ) );
	py = int( std::floor( ( wy - double( lodl->cellMinY() ) * kCellUnits ) / u ) );
}

void WaterMarkDoc::texelToWorld( int px, int py, double & wx, double & wy ) const
{
	const double u = worldPerTexel();
	wx = double( lodl->cellMinX() ) * kCellUnits + ( double( px ) + 0.5 ) * u;
	wy = double( lodl->cellMinY() ) * kCellUnits + ( double( py ) + 0.5 ) * u;
}

/*! A segment on dry land, MEASURED.
 *
 *  The obvious candidate -- a corner of the worldspace -- is open sea on
 *  the Commonwealth, so a control written on it tests nothing and reads as
 *  a failure of the refusal it was meant to prove.  This walks the world on
 *  a half-cell grid and takes the first place where the start, the middle
 *  and the end of a quarter-cell segment are all dry. */
bool WaterMarkDoc::dryStroke( double & x0, double & y0, double & x1, double & y1 ) const
{
	x0 = y0 = x1 = y1 = 0.0;
	if ( !opened )
		return false;
	double wx0 = 0.0, wy0 = 0.0, wx1 = 0.0, wy1 = 0.0;
	worldBounds( wx0, wy0, wx1, wy1 );
	const double step = kCellUnits * 0.5;
	const double len = kCellUnits * 0.25;
	for ( double y = wy0 + step; y < wy1; y += step ) {
		for ( double x = wx0 + step; x + len < wx1; x += step ) {
			if ( bodyAtWorld( x, y ) || bodyAtWorld( x + len * 0.5, y )
				|| bodyAtWorld( x + len, y ) )
				continue;
			x0 = x;
			y0 = y;
			x1 = x + len;
			y1 = y;
			return true;
		}
	}
	return false;
}

quint16 WaterMarkDoc::bodyAtWorld( double wx, double wy ) const
{
	if ( !opened )
		return 0;
	int px = 0, py = 0;
	worldToTexel( wx, wy, px, py );
	return idAtTexel( px, py );
}

// =========================================================================
//  the strokes
// =========================================================================

bool WaterMarkDoc::addStroke( const WaterStroke & in, QString * message )
{
	auto say = [message]( const QString & m ) {
		if ( message )
			*message = m;
	};
	if ( in.kind == 10 ) {
		/* A raster layer (lane WATER5, watercurves.h): no points and no body,
		 * the payload is the mark.  The solve skips it (no points) until
		 * CHANGE_NEEDED H3 teaches it authority where painted. */
		marks.append( in );
		dirty = true;
		say( QStringLiteral( "raster layer stored (%1 bytes)" ).arg( in.extra.size() ) );
		return true;
	}
	if ( in.pts.isEmpty() ) {
		say( QStringLiteral( "that stroke has no points" ) );
		return false;
	}
	WaterStroke s = in;
	/* The FIRST POINT THAT LANDS ON WATER names the body, not strictly the
	 * first point: a drag that begins a few units off the bank is the same
	 * mark the user meant, and the canvas used to buy that forgiveness by
	 * throwing dry points away before this function ever saw them -- which
	 * also turned a stroke drawn entirely on land into "that stroke has no
	 * points", a sentence that says nothing about what went wrong. */
	if ( !s.body )
		for ( const WaterStrokePoint & p : s.pts ) {
			s.body = bodyAtWorld( double( p.x ), double( p.y ) );
			if ( s.body )
				break;
		}
	if ( !s.body ) {
		/* THE CONTROL, and it is a refusal in words on purpose. A constraint
		 * that names no body constrains nothing, and storing it would leave the
		 * next reader to work out why the planes did not move. */
		say( QStringLiteral( "that stroke touches no water anywhere along it -- it is on "
			"dry land, so it names no body of water; nothing was stored" ) );
		return false;
	}
	int outside = 0;
	for ( const WaterStrokePoint & p : s.pts )
		if ( bodyAtWorld( double( p.x ), double( p.y ) ) != s.body )
			outside++;
	marks.append( s );
	dirty = true;
	LodtWaterBody b;
	body( int( s.body ), b );
	if ( outside )
		say( QStringLiteral( "%1 of %2 points fell outside body %3 and will be ignored by the "
			"solve; the stroke is stored as drawn" ).arg( outside ).arg( s.pts.size() ).arg( s.body ) );
	else
		say( QStringLiteral( "stroke on body %1 (%2 points)" ).arg( s.body ).arg( s.pts.size() ) );
	return true;
}

void WaterMarkDoc::removeStroke( int index )
{
	if ( index < 0 || index >= marks.size() )
		return;
	marks.removeAt( index );
	dirty = true;
}

void WaterMarkDoc::clearStrokes()
{
	if ( marks.isEmpty() )
		return;
	marks.clear();
	dirty = true;
}

QVector<int> WaterMarkDoc::strokesOfBody( int id ) const
{
	QVector<int> out;
	for ( int i = 0; i < marks.size(); i++ )
		if ( int( marks[i].body ) == id )
			out.append( i );
	return out;
}

// =========================================================================
//  the solve  (spec_water.md 4.3)
// =========================================================================

// =========================================================================
//  the solver core  (lane WATER4; the method is stated in watermark.h)
// =========================================================================

void WaterFlowGrid::build( int width, int height, const std::vector<quint8> & wet,
	const std::vector<float> & k )
{
	w = width;
	h = height;
	idx.assign( size_t( w ) * size_t( h ), -1 );
	cx.clear();
	cy.clear();
	fi.clear();
	fj.clear();
	kf.clear();
	n = 0;
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const size_t at = size_t( y ) * size_t( w ) + size_t( x );
			if ( !wet[at] )
				continue;
			idx[at] = n++;
			cx.push_back( x );
			cy.push_back( y );
		}
	auto kAt = [&]( int x, int y ) -> double {
		const size_t at = size_t( y ) * size_t( w ) + size_t( x );
		const double v = at < k.size() ? double( k[at] ) : 1.0;
		return v > 1e-9 ? v : 1e-9;
	};
	// east faces first, then north faces: velocity() relies on the order
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x + 1 < w; x++ ) {
			const int a = idx[size_t( y ) * size_t( w ) + size_t( x )];
			const int b = idx[size_t( y ) * size_t( w ) + size_t( x ) + 1];
			if ( a < 0 || b < 0 )
				continue;
			fi.push_back( a );
			fj.push_back( b );
			const double ka = kAt( x, y ), kb = kAt( x + 1, y );
			kf.push_back( 2.0 * ka * kb / ( ka + kb ) );
		}
	nE = int( fi.size() );
	for ( int y = 0; y + 1 < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const int a = idx[size_t( y ) * size_t( w ) + size_t( x )];
			const int b = idx[size_t( y + 1 ) * size_t( w ) + size_t( x )];
			if ( a < 0 || b < 0 )
				continue;
			fi.push_back( a );
			fj.push_back( b );
			const double ka = kAt( x, y ), kb = kAt( x, y + 1 );
			kf.push_back( 2.0 * ka * kb / ( ka + kb ) );
		}
	diag.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		diag[size_t( fi[f] )] += kf[f];
		diag[size_t( fj[f] )] += kf[f];
	}
	/* The connected pieces, over the faces.  A body is often several: the
	 * writer's bridge rule joins pieces up to two texels apart, so the mask
	 * has islets and coves that touch nothing, and each is a pure-Neumann
	 * system of its own whose sources must balance on their own. */
	std::vector<int> parent( size_t( n ), 0 );
	for ( int i = 0; i < n; i++ )
		parent[size_t( i )] = i;
	auto find = [&]( int i ) {
		while ( parent[size_t( i )] != i ) {
			parent[size_t( i )] = parent[size_t( parent[size_t( i )] )];
			i = parent[size_t( i )];
		}
		return i;
	};
	for ( size_t f = 0; f < fi.size(); f++ ) {
		const int a = find( fi[f] ), b = find( fj[f] );
		if ( a != b )
			parent[size_t( a )] = b;
	}
	comp.assign( size_t( n ), -1 );
	nComp = 0;
	std::vector<int> label( size_t( n ), -1 );
	for ( int i = 0; i < n; i++ ) {
		const int r = find( i );
		if ( label[size_t( r )] < 0 )
			label[size_t( r )] = nComp++;
		comp[size_t( i )] = label[size_t( r )];
	}
}

bool WaterFlowGrid::solve( const std::vector<double> & bIn, const std::vector<quint8> & dirichlet,
	std::vector<double> & phi, int & iterations, double & residual, double tol, int cap ) const
{
	iterations = 0;
	residual = 0.0;
	phi.assign( size_t( n ), 0.0 );
	if ( n <= 0 )
		return false;
	const bool haveD = !dirichlet.empty();
	std::vector<double> b( bIn );
	b.resize( size_t( n ), 0.0 );
	bool anyD = false;
	if ( haveD )
		for ( int i = 0; i < n; i++ )
			if ( dirichlet[size_t( i )] ) {
				b[size_t( i )] = 0.0;
				anyD = true;
			}
	{
		/* consistency PER PIECE: a piece with no Dirichlet cell is a pure-
		 * Neumann system of its own, singular, and solvable only when its
		 * sources balance -- so its mean is taken out of it */
		std::vector<double> sum( size_t( std::max( 1, nComp ) ), 0.0 );
		std::vector<int> cnt( size_t( std::max( 1, nComp ) ), 0 );
		std::vector<quint8> hasD( size_t( std::max( 1, nComp ) ), 0 );
		for ( int i = 0; i < n; i++ ) {
			const int c = comp.empty() ? 0 : comp[size_t( i )];
			sum[size_t( c )] += b[size_t( i )];
			cnt[size_t( c )]++;
			if ( anyD && dirichlet[size_t( i )] )
				hasD[size_t( c )] = 1;
		}
		for ( int i = 0; i < n; i++ ) {
			const int c = comp.empty() ? 0 : comp[size_t( i )];
			if ( !hasD[size_t( c )] && cnt[size_t( c )] > 0 )
				b[size_t( i )] -= sum[size_t( c )] / double( cnt[size_t( c )] );
		}
	}
	double nb = 0.0;
	for ( int i = 0; i < n; i++ )
		nb += b[size_t( i )] * b[size_t( i )];
	nb = std::sqrt( nb );
	if ( nb <= 0.0 )
		return true;                       // nothing drives it: phi = 0, u = 0
	auto matvec = [&]( const std::vector<double> & x, std::vector<double> & y ) {
		for ( int i = 0; i < n; i++ )
			y[size_t( i )] = diag[size_t( i )] * x[size_t( i )];
		for ( size_t f = 0; f < fi.size(); f++ ) {
			const size_t a = size_t( fi[f] ), c = size_t( fj[f] );
			y[a] -= kf[f] * x[c];
			y[c] -= kf[f] * x[a];
		}
		if ( anyD )
			for ( int i = 0; i < n; i++ )
				if ( dirichlet[size_t( i )] )
					y[size_t( i )] = diag[size_t( i )] * x[size_t( i )];
	};
	std::vector<double> r( b ), z( size_t( n ), 0.0 ), p( size_t( n ), 0.0 ), Ap( size_t( n ), 0.0 );
	double rz = 0.0;
	// an isolated texel has no face and no equation: its preconditioner is 0
	std::vector<double> Minv( size_t( n ), 0.0 );
	for ( int i = 0; i < n; i++ )
		Minv[size_t( i )] = diag[size_t( i )] > 0.0 ? 1.0 / diag[size_t( i )] : 0.0;
	for ( int i = 0; i < n; i++ ) {
		z[size_t( i )] = r[size_t( i )] * Minv[size_t( i )];
		p[size_t( i )] = z[size_t( i )];
		rz += r[size_t( i )] * z[size_t( i )];
	}
	double res = 1.0;
	int it = 0;
	while ( it < cap && res > tol ) {
		matvec( p, Ap );
		double pAp = 0.0;
		for ( int i = 0; i < n; i++ )
			pAp += p[size_t( i )] * Ap[size_t( i )];
		if ( !( std::fabs( pAp ) > 0.0 ) )
			break;
		const double alpha = rz / pAp;
		double rr = 0.0;
		for ( int i = 0; i < n; i++ ) {
			phi[size_t( i )] += alpha * p[size_t( i )];
			r[size_t( i )] -= alpha * Ap[size_t( i )];
			rr += r[size_t( i )] * r[size_t( i )];
		}
		if ( !anyD ) {
			double mean = 0.0;
			for ( int i = 0; i < n; i++ )
				mean += phi[size_t( i )];
			mean /= double( n );
			for ( int i = 0; i < n; i++ )
				phi[size_t( i )] -= mean;
		}
		double rz2 = 0.0;
		for ( int i = 0; i < n; i++ ) {
			z[size_t( i )] = r[size_t( i )] * Minv[size_t( i )];
			rz2 += r[size_t( i )] * z[size_t( i )];
		}
		const double beta = rz2 / rz;
		for ( int i = 0; i < n; i++ )
			p[size_t( i )] = z[size_t( i )] + beta * p[size_t( i )];
		rz = rz2;
		it++;
		res = std::sqrt( rr ) / nb;
	}
	iterations = it;
	residual = res;
	return true;
}

void WaterFlowGrid::faceFlux( const std::vector<double> & phi, std::vector<double> & F ) const
{
	F.assign( fi.size(), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ )
		F[f] = -kf[f] * ( phi[size_t( fj[f] )] - phi[size_t( fi[f] )] );
}

void WaterFlowGrid::velocity( const std::vector<double> & phi, std::vector<double> & ux,
	std::vector<double> & uy ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	ux.assign( size_t( n ), 0.0 );
	uy.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		const double v = 0.5 * F[f] / kf[f];
		if ( int( f ) < nE ) {
			ux[size_t( fi[f] )] += v;
			ux[size_t( fj[f] )] += v;
		} else {
			uy[size_t( fi[f] )] += v;
			uy[size_t( fj[f] )] += v;
		}
	}
}

void WaterFlowGrid::divergence( const std::vector<double> & phi, std::vector<double> & div ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	div.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		div[size_t( fi[f] )] += F[f];
		div[size_t( fj[f] )] -= F[f];
	}
}

void WaterFlowGrid::dye( const std::vector<double> & phi, const std::vector<double> & held,
	double halfTexels, std::vector<double> & c ) const
{
	c.assign( size_t( n ), 0.0 );
	if ( n <= 0 )
		return;
	std::vector<double> F, ux, uy;
	faceFlux( phi, F );
	velocity( phi, ux, uy );
	// inflow lists per destination cell: (face, upwind cell, |flux|)
	std::vector<int> head( size_t( n ), -1 ), next( fi.size(), -1 ), from( fi.size(), -1 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		if ( F[f] == 0.0 )
			continue;
		const int src = F[f] > 0.0 ? fi[f] : fj[f];
		const int dst = F[f] > 0.0 ? fj[f] : fi[f];
		from[f] = src;
		next[f] = head[size_t( dst )];
		head[size_t( dst )] = int( f );
	}
	std::vector<int> order( size_t( n ), 0 );
	for ( int i = 0; i < n; i++ )
		order[size_t( i )] = i;
	std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) {
		return phi[size_t( a )] > phi[size_t( b )];
	} );
	const double L = halfTexels > 1e-6 ? halfTexels : 1e-6;
	for ( int i : order ) {
		if ( i < int( held.size() ) && held[size_t( i )] >= 0.0 ) {
			c[size_t( i )] = held[size_t( i )];
			continue;
		}
		double acc = 0.0, tot = 0.0;
		for ( int f = head[size_t( i )]; f >= 0; f = next[size_t( f )] ) {
			const double m = std::fabs( F[size_t( f )] );
			acc += m * c[size_t( from[size_t( f )] )];
			tot += m;
		}
		if ( tot <= 0.0 )
			continue;
		const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
		// the mean chord through a unit cell along the flow: 1 / (|cos| + |sin|)
		const double chord = sp > 0.0
			? sp / ( std::fabs( ux[size_t( i )] ) + std::fabs( uy[size_t( i )] ) ) : 1.0;
		c[size_t( i )] = acc / tot * std::pow( 0.5, chord / L );
	}
}

bool WaterFlowGrid::trace( const std::vector<double> & phi, double x, double y,
	const std::vector<quint8> & stop, int maxCells ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	std::vector<double> vW( size_t( n ), 0.0 ), vE( size_t( n ), 0.0 ),
		vS( size_t( n ), 0.0 ), vN( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		const double v = F[f] / kf[f];
		if ( int( f ) < nE ) {
			vE[size_t( fi[f] )] = v;
			vW[size_t( fj[f] )] = v;
		} else {
			vN[size_t( fi[f] )] = v;
			vS[size_t( fj[f] )] = v;
		}
	}
	int ix = int( std::floor( x ) ), iy = int( std::floor( y ) );
	double fx = x - ix, fy = y - iy;
	const double inf = std::numeric_limits<double>::infinity();
	auto exitTime = [&]( double f, double v0, double v1, double & target ) -> double {
		const double v = v0 + ( v1 - v0 ) * f;
		if ( v > 0.0 )
			target = 1.0;
		else if ( v < 0.0 )
			target = 0.0;
		else
			return inf;
		const double a = v1 - v0;
		const double vt = v0 + a * target;
		if ( vt * v <= 0.0 )
			return inf;                    // the velocity turns before the face
		if ( std::fabs( a ) < 1e-14 )
			return ( target - f ) / v;
		return std::log( vt / v ) / a;
	};
	auto advance = [&]( double f, double v0, double v1, double t ) -> double {
		const double a = v1 - v0;
		if ( std::fabs( a ) < 1e-14 )
			return f + v0 * t;
		return f + ( v0 + a * f ) * ( std::exp( a * t ) - 1.0 ) / a;
	};
	for ( int step = 0; step < maxCells; step++ ) {
		if ( ix < 0 || iy < 0 || ix >= w || iy >= h )
			return false;
		const size_t at = size_t( iy ) * size_t( w ) + size_t( ix );
		const int i = idx[at];
		if ( i < 0 )
			return false;
		if ( at < stop.size() && stop[at] )
			return true;
		double gx = fx, gy = fy;
		const double tx = exitTime( fx, vW[size_t( i )], vE[size_t( i )], gx );
		const double ty = exitTime( fy, vS[size_t( i )], vN[size_t( i )], gy );
		if ( tx == inf && ty == inf )
			return false;                  // a sink or a stagnation point
		const double t = std::min( tx, ty );
		if ( tx <= ty ) {
			fy = std::min( 1.0, std::max( 0.0, advance( fy, vS[size_t( i )], vN[size_t( i )], t ) ) );
			ix += gx == 1.0 ? 1 : -1;
			fx = gx == 1.0 ? 0.0 : 1.0;
		} else {
			fx = std::min( 1.0, std::max( 0.0, advance( fx, vW[size_t( i )], vE[size_t( i )], t ) ) );
			iy += gy == 1.0 ? 1 : -1;
			fy = gy == 1.0 ? 0.0 : 1.0;
		}
	}
	return false;
}

// =========================================================================
//  the solve  (spec_water.md 4.3, as rebuilt by lane WATER4)
// =========================================================================

namespace {

//! Conductance boost ON a stroke: a quartic bump to x1 at its half-width.
constexpr double kStrokeBoost = 4.0;
//! Passes of the in-mask 3x3 vector average over the written DIRECTION.
constexpr int kSmoothPasses = 8;
//! Below this fraction of the mean speed the water is slack and its direction is continued.
constexpr double kSlackFraction = 0.02;
//! The depth floor, world units: a texel is never a perfect insulator.
constexpr double kDepthFloor = 8.0;
//! A body whose bounding box exceeds this many texels is solved in a WINDOW.
constexpr qint64 kWindowMax = qint64( 1 ) << 20;
//! The window's margin around its constraints, texels (8 cells at 32).
constexpr int kWindowMargin = 256;
//! How far a contact with another body may be, texels (the writer's `near`).
constexpr int kContactReach = 64;

//! A chamfer distance (3-4, thirds of a texel) from `seed` over a grid.
void chamfer( int w, int h, const std::vector<quint8> & seed, std::vector<float> & dist )
{
	const size_t n = size_t( w ) * size_t( h );
	dist.assign( n, 1e9f );
	for ( size_t i = 0; i < n; i++ )
		if ( seed[i] )
			dist[i] = 0.0f;
	auto relax = [&]( int x, int y, int nx, int ny, float wgt ) {
		if ( nx < 0 || ny < 0 || nx >= w || ny >= h )
			return;
		const size_t at = size_t( y ) * size_t( w ) + size_t( x );
		const size_t nat = size_t( ny ) * size_t( w ) + size_t( nx );
		dist[at] = std::min( dist[at], dist[nat] + wgt );
	};
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			relax( x, y, x - 1, y, 1.0f );
			relax( x, y, x, y - 1, 1.0f );
			relax( x, y, x - 1, y - 1, 1.41421f );
			relax( x, y, x + 1, y - 1, 1.41421f );
		}
	for ( int y = h - 1; y >= 0; y-- )
		for ( int x = w - 1; x >= 0; x-- ) {
			relax( x, y, x + 1, y, 1.0f );
			relax( x, y, x, y + 1, 1.0f );
			relax( x, y, x + 1, y + 1, 1.41421f );
			relax( x, y, x - 1, y + 1, 1.41421f );
		}
}

} // namespace

/*! One body's field.
 *
 *  THE STROKE IS NOT A HELD DISC ANY MORE.  Lane WATER3's fill wrote every
 *  segment's tangent over a capsule of the stroke's half-width and solved
 *  only the slivers in between; on the Charles that tiled the river with 39
 *  discs of radius 12-15 texels and 20-45 degree seams (lane WATER4's report,
 *  section 1), which is what bungo saw.  Here the stroke does three things,
 *  none of them a held value: its ends are where the water enters and leaves
 *  when nothing else says so (a pin, or the table's own outlet / source
 *  contact); its path raises the conductance underneath it (x4), so the flow
 *  prefers the channel the user drew where a body offers more than one; and
 *  its tangent is compared with the solved flow underneath it -- if the two
 *  disagree over the stroke's length, the stroke wins over the contacts and
 *  the solve is re-run with the stroke's ends alone. */
bool WaterMarkDoc::solveBody( int id, WaterMarkSolve & st, QString * note )
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return false;
	auto sayNote = [&]( const QString & m ) {
		if ( note && note->isEmpty() )
			*note = m;
	};
	const double u = worldPerTexel();
	// ---- the body's bounding box, in body-plane texels ------------------
	const int bx0 = ( int( b.x0 ) - lodl->cellMinX() ) * int( idRate );
	const int by0 = ( int( b.y0 ) - lodl->cellMinY() ) * int( idRate );
	const int bw = ( int( b.x1 ) - int( b.x0 ) + 1 ) * int( idRate );
	const int bh = ( int( b.y1 ) - int( b.y0 ) + 1 ) * int( idRate );
	if ( bw <= 0 || bh <= 0 )
		return false;

	if ( lockZero[id - 1] ) {
		/* bungo's rule, taken literally: a lake nobody connected to a river
		 * has NO flow, and that beats the form's NAM0 -- which is the only
		 * reason a still lake had a velocity at all.  No arrays: the word is
		 * 0 wherever the body is. */
		Field * F = new Field();
		F->zero = true;
		fields.insert( id, F );
		st.bodiesSolved++;
		return true;
	}

	// ---- the constraints, in world units --------------------------------
	/* lane WATER6 (C1): `wa` / `wb` are the per-point speed weights of this
	 * segment's two endpoints; 1 is "the curve's own speed". */
	struct Seg { double ax, ay, bx, by, halfW; double wa = 1.0, wb = 1.0; };
	QVector<Seg> segs;
	QVector<QPointF> srcPins, snkPins, firsts, lasts;
	double sumSpeed = 0.0;
	int nSpeed = 0;
	double maxWidth = 0.0;
	double cx0 = 1e30, cy0 = 1e30, cx1 = -1e30, cy1 = -1e30;   // constraint bbox, world
	auto grow = [&]( double x, double y, double r ) {
		cx0 = std::min( cx0, x - r );
		cy0 = std::min( cy0, y - r );
		cx1 = std::max( cx1, x + r );
		cy1 = std::max( cy1, y + r );
	};
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() || int( s.body ) != id || s.pts.isEmpty() )
			continue;
		const double halfW = double( s.width ) * 0.5 > 0.0 ? double( s.width ) * 0.5 : u;
		if ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin ) {
			/* lane WATER6 (C1): one float a point in the record's trailing bytes
			 * (hook-up H2, lane WATER5) is that point's speed weight.  A record
			 * with none, or with a value that is not a finite 0..64, reads 1 --
			 * which is the unweighted solve, byte for byte (gate X1a). */
			auto weightAt = [&s]( int k ) -> double {
				if ( s.extra.size() < qsizetype( k + 1 ) * 4 )
					return 1.0;
				float v = 1.0f;
				memcpy( &v, s.extra.constData() + qsizetype( k ) * 4, 4 );
				return ( std::isfinite( v ) && v >= 0.0f && v <= 64.0f ) ? double( v ) : 1.0;
			};
			if ( s.pts.size() < 2 ) {
				/* lane WATER6 (C2), bungo: "a one-point curve = a pin".  It carries
				 * no direction, so it enters as a SOURCE disc of its own width --
				 * the same treatment a stroke's FIRST point already gets.  A sink is
				 * what the store's OutletPin (kind 5) already means. */
				maxWidth = std::max( maxWidth, double( s.width ) );
				srcPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );
				grow( s.pts.first().x, s.pts.first().y, halfW );
				if ( s.flags & WaterStroke::SetsSpeed ) {
					sumSpeed += double( s.speed );
					nSpeed++;
				}
				st.strokes++;
				continue;
			}
			maxWidth = std::max( maxWidth, double( s.width ) );
			for ( int k = 0; k + 1 < s.pts.size(); k++ ) {
				segs.append( Seg{ double( s.pts[k].x ), double( s.pts[k].y ),
					double( s.pts[k + 1].x ), double( s.pts[k + 1].y ), halfW,
					weightAt( k ), weightAt( k + 1 ) } );
				grow( s.pts[k].x, s.pts[k].y, halfW );
				grow( s.pts[k + 1].x, s.pts[k + 1].y, halfW );
			}
			firsts.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			lasts.append( QPointF( s.pts.last().x, s.pts.last().y ) );
			if ( s.flags & WaterStroke::SetsSpeed ) {
				sumSpeed += double( s.speed );
				nSpeed++;
			}
			st.strokes++;
		} else if ( s.kind == WaterStroke::SourcePin ) {
			srcPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			grow( s.pts.first().x, s.pts.first().y, halfW );
			st.strokes++;
		} else if ( s.kind == WaterStroke::OutletPin ) {
			snkPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			grow( s.pts.first().x, s.pts.first().y, halfW );
			st.strokes++;
		} else if ( s.kind == WaterStroke::DyePin || s.kind == WaterStroke::DyeMouth ) {
			grow( s.pts.first().x, s.pts.first().y, halfW );
		}
	}

	// ---- the window: the whole bbox, or a cut around the constraints ----
	Field * F = new Field();
	F->px0 = bx0;
	F->py0 = by0;
	F->w = bw;
	F->h = bh;
	if ( qint64( bw ) * qint64( bh ) > kWindowMax ) {
		if ( !( cx1 >= cx0 ) ) {
			delete F;
			sayNote( QStringLiteral( "body %1 spans %2 x %3 texels and carries no mark to "
				"solve around" ).arg( id ).arg( bw ).arg( bh ) );
			return false;
		}
		int wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
		worldToTexel( cx0, cy0, wx0, wy0 );
		worldToTexel( cx1, cy1, wx1, wy1 );
		wx0 = std::max( bx0, wx0 - kWindowMargin );
		wy0 = std::max( by0, wy0 - kWindowMargin );
		wx1 = std::min( bx0 + bw - 1, wx1 + kWindowMargin );
		wy1 = std::min( by0 + bh - 1, wy1 + kWindowMargin );
		F->px0 = wx0;
		F->py0 = wy0;
		F->w = wx1 - wx0 + 1;
		F->h = wy1 - wy0 + 1;
		F->windowed = true;
	}
	const int W = F->w, H = F->h;
	const size_t n = size_t( W ) * size_t( H );
	F->mask.assign( n, 0 );
	F->vx.assign( n, 0.0f );
	F->vy.assign( n, 0.0f );
	F->conf.assign( n, 0 );
	F->speed.assign( n, 8 );
	F->dye.assign( n, 0 );
	std::vector<quint8> cut( n, 0 );
	size_t wetCount = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const size_t at = size_t( y ) * size_t( W ) + size_t( x );
			if ( idAtTexel( F->px0 + x, F->py0 + y ) != quint16( id ) )
				continue;
			F->mask[at] = 1;
			wetCount++;
			if ( F->windowed && ( x == 0 || y == 0 || x == W - 1 || y == H - 1 ) ) {
				// a wet texel on the window's edge whose neighbour beyond is the same body
				const int ox = x == 0 ? -1 : ( x == W - 1 ? 1 : 0 );
				const int oy = y == 0 ? -1 : ( y == H - 1 ? 1 : 0 );
				if ( idAtTexel( F->px0 + x + ox, F->py0 + y + oy ) == quint16( id ) )
					cut[at] = 1;
			}
		}
	if ( !wetCount ) {
		delete F;
		return false;
	}

	// ---- the conductance: the water depth, floored ----------------------
	std::vector<float> k( n, 1.0f );
	{
		const int spc = lodl->samplesPerCell();
		const int step = idRate > 0 ? std::max( 1, spc / int( idRate ) ) : 1;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				const float hgt = lodl->height( ( F->px0 + x ) * step, ( F->py0 + y ) * step );
				const double d = double( b.waterHeight ) - double( hgt );
				k[at] = float( std::max( kDepthFloor, d ) );
			}
	}

	// ---- the strokes' paths: held for the confidence, boosted for the flow
	std::vector<quint8> held( n, 0 );
	std::vector<float> boost( n, 1.0f );
	/* lane WATER6 (C1): the weight of the NEAREST segment over this texel, 1
	 * where no segment reaches.  It multiplies the speed before the body's
	 * mean is taken, so a weighted reach reads faster without the direction
	 * being asked to change. */
	std::vector<float> wgt( n, 1.0f );
	std::vector<float> wgtDist( n, 3.4e38f );
	auto texelOf = [&]( double wx, double wy, int & x, int & y ) {
		int px = 0, py = 0;
		worldToTexel( wx, wy, px, py );
		x = px - F->px0;
		y = py - F->py0;
	};
	for ( const Seg & seg : segs ) {
		int qx0 = 0, qy0 = 0, qx1 = 0, qy1 = 0;
		texelOf( std::min( seg.ax, seg.bx ) - seg.halfW, std::min( seg.ay, seg.by ) - seg.halfW, qx0, qy0 );
		texelOf( std::max( seg.ax, seg.bx ) + seg.halfW, std::max( seg.ay, seg.by ) + seg.halfW, qx1, qy1 );
		qx0 = std::max( qx0, 0 );
		qy0 = std::max( qy0, 0 );
		qx1 = std::min( qx1, W - 1 );
		qy1 = std::min( qy1, H - 1 );
		for ( int y = qy0; y <= qy1; y++ )
			for ( int x = qx0; x <= qx1; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				double wx = 0, wy = 0;
				texelToWorld( F->px0 + x, F->py0 + y, wx, wy );
				double tx = 0, ty = 0;
				const double dd = distToSegment( wx, wy, seg.ax, seg.ay, seg.bx, seg.by, tx, ty );
				if ( dd > seg.halfW )
					continue;
				held[at] = 1;
				/* a SMOOTH preference: a step in k refracts the flow at its edge,
				 * and that edge was a seam in the prototype's picture */
				const double q = 1.0 - ( dd / seg.halfW ) * ( dd / seg.halfW );
				/* lane WATER6 (C1): the weight interpolated along THIS segment.  At
				 * w = 1 the expression below is the old one character for character,
				 * which is what makes gate X1a a byte gate. */
				const double segLen2 = ( seg.bx - seg.ax ) * ( seg.bx - seg.ax )
					+ ( seg.by - seg.ay ) * ( seg.by - seg.ay );
				const double tt = segLen2 > 0.0
					? std::max( 0.0, std::min( 1.0, ( ( tx - seg.ax ) * ( seg.bx - seg.ax )
						+ ( ty - seg.ay ) * ( seg.by - seg.ay ) ) / segLen2 ) ) : 0.0;
				const double sw = seg.wa + tt * ( seg.wb - seg.wa );
				if ( float( dd ) < wgtDist[at] ) {
					wgtDist[at] = float( dd );
					wgt[at] = float( sw );
				}
				boost[at] = float( std::max( double( boost[at] ),
					1.0 + ( kStrokeBoost * sw - 1.0 ) * q * q ) );
			}
	}
	int heldHere = 0;
	for ( size_t i = 0; i < n; i++ ) {
		k[i] = float( k[i] * boost[i] );
		if ( held[i] )
			heldHere++;
	}
	st.constrained += heldHere;
	if ( !heldHere && !segs.isEmpty() )
		sayNote( QStringLiteral( "no texel of body %1 lies under its strokes, so its "
			"direction is unchanged" ).arg( id ) );

	// ---- the sources and the sinks --------------------------------------
	// every wet texel of this body within `radius` texels of a world point --
	// a point sink is a singularity whose neighbours all point at it, so an
	// end or a pin is a DISC, and the nearest texel only when the disc is empty
	auto discOf = [&]( double wx, double wy, double radius, std::vector<size_t> & out ) {
		int x = 0, y = 0;
		texelOf( wx, wy, x, y );
		const int r = std::max( 1, int( radius + 0.5 ) );
		for ( int dy = -r; dy <= r; dy++ )
			for ( int dx = -r; dx <= r; dx++ ) {
				if ( dx * dx + dy * dy > r * r )
					continue;
				const int nx = x + dx, ny = y + dy;
				if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
					continue;
				const size_t at = size_t( ny ) * size_t( W ) + size_t( nx );
				if ( F->mask[at] )
					out.push_back( at );
			}
	};
	const double endRadius = std::max( 2.0, ( maxWidth > 0.0 ? maxWidth * 0.5 : 4.0 * u ) / u );
	// nearest wet texel of this body to a world point, within `reach` texels
	auto nearestWet = [&]( double wx, double wy, int reach ) -> long {
		int x = 0, y = 0;
		texelOf( wx, wy, x, y );
		long best = -1;
		double bestD = 1e30;
		for ( int dy = -reach; dy <= reach; dy++ )
			for ( int dx = -reach; dx <= reach; dx++ ) {
				const int nx = x + dx, ny = y + dy;
				if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
					continue;
				const size_t at = size_t( ny ) * size_t( W ) + size_t( nx );
				if ( !F->mask[at] )
					continue;
				const double d = double( dx ) * dx + double( dy ) * dy;
				if ( d < bestD ) {
					bestD = d;
					best = long( at );
				}
			}
		return best;
	};
	// the texels of this body that TOUCH (or nearly touch) body `other`
	auto contact = [&]( quint16 other, std::vector<size_t> & out ) {
		out.clear();
		if ( !other || other == quint16( id ) )
			return;
		const int pad = kContactReach;
		const int PW = W + 2 * pad, PH = H + 2 * pad;
		std::vector<quint8> seed( size_t( PW ) * size_t( PH ), 0 );
		bool any = false;
		for ( int y = 0; y < PH; y++ )
			for ( int x = 0; x < PW; x++ )
				if ( idAtTexel( F->px0 - pad + x, F->py0 - pad + y ) == other ) {
					seed[size_t( y ) * size_t( PW ) + size_t( x )] = 1;
					any = true;
				}
		if ( !any )
			return;
		std::vector<float> dist;
		chamfer( PW, PH, seed, dist );
		float dmin = 1e9f;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( F->mask[at] )
					dmin = std::min( dmin, dist[size_t( y + pad ) * size_t( PW ) + size_t( x + pad )] );
			}
		if ( dmin > float( kContactReach ) )
			return;
		const float band = std::max( 2.5f, dmin + 0.5f );
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( F->mask[at]
					&& dist[size_t( y + pad ) * size_t( PW ) + size_t( x + pad )] <= band )
					out.push_back( at );
			}
	};
	std::vector<size_t> sources, sinks, contactSrc, contactSnk;
	for ( const QPointF & p : srcPins ) {
		discOf( p.x(), p.y(), endRadius, sources );
		const long at = sources.empty() ? nearestWet( p.x(), p.y(), 4 ) : -1;
		if ( at >= 0 )
			sources.push_back( size_t( at ) );
	}
	for ( const QPointF & p : snkPins ) {
		discOf( p.x(), p.y(), endRadius, sinks );
		const long at = sinks.empty() ? nearestWet( p.x(), p.y(), 4 ) : -1;
		if ( at >= 0 )
			sinks.push_back( size_t( at ) );
	}
	contact( b.outlet, contactSnk );
	contact( b.source, contactSrc );
	bool usedStrokeEnds = false;
	auto strokeEnds = [&]( std::vector<size_t> & s, std::vector<size_t> & t ) {
		s.clear();
		t.clear();
		for ( const QPointF & p : firsts ) {
			const size_t before = s.size();
			discOf( p.x(), p.y(), endRadius, s );
			const long at = s.size() == before ? nearestWet( p.x(), p.y(), 8 ) : -1;
			if ( at >= 0 )
				s.push_back( size_t( at ) );
		}
		for ( const QPointF & p : lasts ) {
			const size_t before = t.size();
			discOf( p.x(), p.y(), endRadius, t );
			const long at = t.size() == before ? nearestWet( p.x(), p.y(), 8 ) : -1;
			if ( at >= 0 )
				t.push_back( size_t( at ) );
		}
	};
	{
		std::vector<size_t> es, et;
		strokeEnds( es, et );
		if ( sinks.empty() )
			sinks = contactSnk;
		if ( sources.empty() )
			sources = contactSrc;
		if ( sinks.empty() ) {
			sinks = et;
			usedStrokeEnds = !et.empty();
		}
		if ( sources.empty() ) {
			sources = es;
			usedStrokeEnds = usedStrokeEnds || !es.empty();
		}
	}
	if ( sources.empty() && sinks.empty() ) {
		delete F;
		sayNote( QStringLiteral( "body %1 has no inflow or outflow anywhere -- no pin, no "
			"stroke, and the table names no body it drains into -- so nothing drives a "
			"flow and it keeps the direction the classifier gave it" ).arg( id ) );
		return false;
	}

	// ---- the potential ---------------------------------------------------
	WaterFlowGrid & G = F->grid;
	G.build( W, H, F->mask, k );
	std::vector<double> rhs, phi, dirichlet;
	std::vector<quint8> dset;
	bool anyCut = false;
	for ( size_t i = 0; i < n; i++ )
		if ( cut[i] )
			anyCut = true;
	if ( anyCut ) {
		dset.assign( size_t( G.n ), 0 );
		for ( size_t i = 0; i < n; i++ )
			if ( cut[i] && G.idx[i] >= 0 )
				dset[size_t( G.idx[i] )] = 1;
	}
	auto fillRhs = [&]( const std::vector<size_t> & src, const std::vector<size_t> & snk ) {
		rhs.assign( size_t( G.n ), 0.0 );
		if ( !src.empty() ) {
			for ( size_t at : src )
				if ( G.idx[at] >= 0 )
					rhs[size_t( G.idx[at] )] += 1.0 / double( src.size() );
		} else if ( !anyCut ) {
			for ( int i = 0; i < G.n; i++ )
				rhs[size_t( i )] += 1.0 / double( G.n );      // rain: the catchment
		}
		if ( !snk.empty() ) {
			for ( size_t at : snk )
				if ( G.idx[at] >= 0 )
					rhs[size_t( G.idx[at] )] -= 1.0 / double( snk.size() );
		} else if ( !anyCut ) {
			for ( int i = 0; i < G.n; i++ )
				rhs[size_t( i )] -= 1.0 / double( G.n );      // seepage everywhere
		}
	};
	int it = 0;
	double res = 0.0;
	fillRhs( sources, sinks );
	G.solve( rhs, dset, phi, it, res );
	std::vector<double> ux, uy;
	G.velocity( phi, ux, uy );

	// ---- the strokes' authority: do they agree with what the banks gave? -
	double agreeSum = 0.0;
	int agreeN = 0;
	auto agreement = [&]() {
		agreeSum = 0.0;
		agreeN = 0;
		for ( const Seg & seg : segs ) {
			const double len = std::hypot( seg.bx - seg.ax, seg.by - seg.ay );
			if ( len <= 0.0 )
				continue;
			const double tx = ( seg.bx - seg.ax ) / len, ty = ( seg.by - seg.ay ) / len;
			const int steps = std::max( 1, int( len / u ) );
			for ( int q = 0; q <= steps; q++ ) {
				const double t = double( q ) / steps;
				int x = 0, y = 0;
				texelOf( seg.ax + t * ( seg.bx - seg.ax ), seg.ay + t * ( seg.by - seg.ay ), x, y );
				if ( x < 0 || y < 0 || x >= W || y >= H )
					continue;
				const int i = G.idx[size_t( y ) * size_t( W ) + size_t( x )];
				if ( i < 0 )
					continue;
				const double m = std::hypot( ux[size_t( i )], uy[size_t( i )] );
				if ( m <= 0.0 )
					continue;
				agreeSum += ( ux[size_t( i )] * tx + uy[size_t( i )] * ty ) / m;
				agreeN++;
			}
		}
	};
	agreement();
	if ( agreeN && agreeSum / agreeN < 0.0 && !usedStrokeEnds ) {
		/* The stroke says the water goes the OTHER way from what the table's
		 * contacts gave -- a mis-detected drain, or a user who knows better.
		 * The stroke is the authority: solve again from its ends alone. */
		std::vector<size_t> es, et;
		strokeEnds( es, et );
		if ( !es.empty() && !et.empty() ) {
			fillRhs( es, et );
			G.solve( rhs, dset, phi, it, res );
			G.velocity( phi, ux, uy );
			agreement();
			usedStrokeEnds = true;
			sayNote( QStringLiteral( "body %1's stroke runs against the table's own "
				"drain; the stroke won" ).arg( id ) );
		}
	}
	if ( agreeN ) {
		const double a = agreeSum / agreeN;
		st.strokeAgreement = std::min( st.strokeAgreement, a );
	}
	st.iterations = std::max( st.iterations, it );
	st.residual = std::max( st.residual, res );
	F->phi = phi;

	// ---- direction and speed, per texel ----------------------------------
	double meanSpeed = 0.0;
	qint64 wet = 0;
	for ( int i = 0; i < G.n; i++ ) {
		const size_t wat = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );
		meanSpeed += std::hypot( ux[size_t( i )], uy[size_t( i )] ) * double( wgt[wat] );
		wet++;
	}
	if ( wet )
		meanSpeed /= double( wet );
	/* THE WRITTEN DIRECTION.  The solve's direction stands where the water
	 * moves and is not a bank texel; slack water (below kSlackFraction of the
	 * mean speed: dead-end coves, the water past the sink) and the bank
	 * texels -- whose face-averaged velocity is biased by the staircase, 12
	 * degrees on the synthetic island -- are CONTINUED from it by the
	 * harmonic fill (tangent to the banks by construction); then the whole
	 * unit field is low-passed by kSmoothPasses in-mask 3x3 vector averages.
	 * The prototype measured why: within three texels of a staircase bank
	 * the raw direction jumps 22-25 degrees at its 99th percentile, six
	 * texels in it is 5.6.  The SPEED and the flux are the solve's own and
	 * are not touched by any of this. */
	{
		std::vector<float> dxv( n, 0.0f ), dyv( n, 0.0f );
		std::vector<quint8> fixed( n, 0 );
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
			const size_t at = size_t( y ) * size_t( W ) + size_t( x );
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			bool bank = x == 0 || y == 0 || x == W - 1 || y == H - 1;
			if ( !bank )
				bank = !F->mask[at - 1] || !F->mask[at + 1] || !F->mask[at - size_t( W )]
					|| !F->mask[at + size_t( W )];
			if ( sp >= kSlackFraction * meanSpeed && sp > 1e-300 && !bank ) {
				dxv[at] = float( ux[size_t( i )] / sp );
				dyv[at] = float( uy[size_t( i )] / sp );
				fixed[at] = 1;
			}
		}
		/* the continuation is SEEDED by a breadth-first walk from the fixed
		 * texels -- every free texel takes the vector of the texel it was reached
		 * from -- so a pond behind a one-texel neck starts with a direction
		 * instead of waiting for a relaxation whose changes fall under the
		 * tolerance before they reach it (the prototype left two such ponds at
		 * zero); the SOR then smooths what the walk laid down */
		{
			std::vector<int> queue;
			std::vector<quint8> seen( n, 0 );
			queue.reserve( n );
			for ( size_t at = 0; at < n; at++ )
				if ( fixed[at] ) {
					seen[at] = 1;
					queue.push_back( int( at ) );
				}
			for ( size_t q = 0; q < queue.size(); q++ ) {
				const int at = queue[q];
				const int x = at % W, y = at / W;
				const int dx4[4] = { -1, 1, 0, 0 }, dy4[4] = { 0, 0, -1, 1 };
				for ( int k4 = 0; k4 < 4; k4++ ) {
					const int nx = x + dx4[k4], ny = y + dy4[k4];
					if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
						continue;
					const size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
					if ( !F->mask[nat] || seen[nat] )
						continue;
					seen[nat] = 1;
					dxv[nat] = dxv[size_t( at )];
					dyv[nat] = dyv[size_t( at )];
					queue.push_back( int( nat ) );
				}
			}
		}
		// the continuation: red-black SOR on the free texels
		{
			const double omega = 1.9, tol = 1e-4;
			for ( int it2 = 0; it2 < 4000; it2++ ) {
				double worst = 0.0;
				for ( int phase = 0; phase < 2; phase++ )
					for ( int y = 0; y < H; y++ )
						for ( int x = ( y + phase ) & 1; x < W; x += 2 ) {
							const size_t at = size_t( y ) * size_t( W ) + size_t( x );
							if ( !F->mask[at] || fixed[at] )
								continue;
							double sx = 0.0, sy = 0.0;
							int kk = 0;
							const int dx4[4] = { -1, 1, 0, 0 }, dy4[4] = { 0, 0, -1, 1 };
							for ( int q = 0; q < 4; q++ ) {
								const int nx = x + dx4[q], ny = y + dy4[q];
								if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
									continue;
								const size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
								if ( !F->mask[nat] )
									continue;
								sx += dxv[nat];
								sy += dyv[nat];
								kk++;
							}
							if ( !kk )
								continue;
							const double nvx = dxv[at] + omega * ( sx / kk - dxv[at] );
							const double nvy = dyv[at] + omega * ( sy / kk - dyv[at] );
							worst = std::max( worst, std::max( std::fabs( nvx - dxv[at] ), std::fabs( nvy - dyv[at] ) ) );
							dxv[at] = float( nvx );
							dyv[at] = float( nvy );
						}
				if ( worst < tol )
					break;
			}
		}
		// the low-pass: kSmoothPasses in-mask 3x3 vector averages, corners at half weight
		std::vector<float> tx2( n ), ty2( n );
		for ( int pass = 0; pass < kSmoothPasses; pass++ ) {
			for ( int y = 0; y < H; y++ )
				for ( int x = 0; x < W; x++ ) {
					const size_t at = size_t( y ) * size_t( W ) + size_t( x );
					if ( !F->mask[at] ) {
						tx2[at] = 0.0f;
						ty2[at] = 0.0f;
						continue;
					}
					double sx = 0.0, sy = 0.0, wsum = 0.0;
					for ( int dy = -1; dy <= 1; dy++ )
						for ( int dx = -1; dx <= 1; dx++ ) {
							const int nx = x + dx, ny = y + dy;
							if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
								continue;
							const size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
							if ( !F->mask[nat] )
								continue;
							const double wgt = ( dx == 0 || dy == 0 ) ? 1.0 : 0.5;
							sx += wgt * dxv[nat];
							sy += wgt * dyv[nat];
							wsum += wgt;
						}
					tx2[at] = float( wsum > 0.0 ? sx / wsum : 0.0 );
					ty2[at] = float( wsum > 0.0 ? sy / wsum : 0.0 );
				}
			dxv.swap( tx2 );
			dyv.swap( ty2 );
		}
		for ( size_t at = 0; at < n; at++ ) {
			if ( !F->mask[at] )
				continue;
			/* any non-zero magnitude names a direction: a pond behind a one-texel
			 * neck is reached by the continuation at 1e-12 and is still a direction */
			const double m = std::hypot( double( dxv[at] ), double( dyv[at] ) );
			F->vx[at] = float( m > 0.0 ? dxv[at] / m : 0.0 );
			F->vy[at] = float( m > 0.0 ? dyv[at] / m : 0.0 );
		}
	}
	double mx = 0.0, my = 0.0;
	for ( int i = 0; i < G.n; i++ ) {
		const size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );
		const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] ) * double( wgt[at] );
		mx += F->vx[at];
		my += F->vy[at];
		/* The speed nibble: 8 is the body's mean, 15 is 1.9x it (the format's
		 * own quantum), so a narrows that doubles the speed saturates at 15
		 * and says so here rather than wrapping. */
		const double q = meanSpeed > 0.0 ? 8.0 * sp / meanSpeed : 8.0;
		F->speed[at] = quint8( std::max( 0, std::min( 15, int( q + 0.5 ) ) ) );
	}

	// ---- confidence: the geodesic distance from the user's own marks -----
	{
		double halfLife = maxWidth;
		double hlTexels = ( halfLife > 0.0 ? halfLife : 4.0 * u ) / u;
		hlTexels = std::max( 2.0, hlTexels );
		std::vector<quint8> seed( held );
		for ( size_t at : sources )
			seed[at] = 1;
		for ( size_t at : sinks )
			seed[at] = 1;
		std::vector<float> dist( n, 1e9f );
		for ( size_t i = 0; i < n; i++ )
			if ( seed[i] && F->mask[i] )
				dist[i] = 0.0f;
		auto relax = [&]( int x, int y, int nx, int ny, float wgt ) {
			if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
				return;
			const size_t at = size_t( y ) * size_t( W ) + size_t( x );
			const size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
			if ( !F->mask[at] || !F->mask[nat] )
				return;
			dist[at] = std::min( dist[at], dist[nat] + wgt );
		};
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				relax( x, y, x - 1, y, 1.0f );
				relax( x, y, x, y - 1, 1.0f );
				relax( x, y, x - 1, y - 1, 1.41421f );
				relax( x, y, x + 1, y - 1, 1.41421f );
			}
		for ( int y = H - 1; y >= 0; y-- )
			for ( int x = W - 1; x >= 0; x-- ) {
				relax( x, y, x + 1, y, 1.0f );
				relax( x, y, x, y + 1, 1.0f );
				relax( x, y, x + 1, y + 1, 1.41421f );
				relax( x, y, x - 1, y + 1, 1.41421f );
			}
		for ( size_t i = 0; i < n; i++ ) {
			if ( !F->mask[i] || dist[i] > 1e8f ) {
				F->conf[i] = 0;
				continue;
			}
			const double c = 15.0 * std::pow( 0.5, double( dist[i] ) / hlTexels );
			F->conf[i] = quint8( std::max( 0, std::min( 15, int( c + 0.5 ) ) ) );
		}
	}

	// ---- the body's new mean, and the record's derived fields ------------
	double speedWorld = nSpeed ? sumSpeed / nSpeed : 0.0;
	const double meanMag = std::sqrt( double( b.flowX ) * b.flowX + double( b.flowY ) * b.flowY );
	if ( speedWorld <= 0.0 )
		speedWorld = meanMag > 0.0 ? meanMag : 0.25;
	const double mm = std::hypot( mx, my );
	LodtWaterBody & rec = table[id - 1];
	if ( mm > 1e-6 ) {
		rec.flowX = float( mx / mm * speedWorld );
		rec.flowY = float( my / mm * speedWorld );
	} else {
		rec.flowX = 0.0f;
		rec.flowY = 0.0f;
	}
	rec.flowSource = 4;
	/* Bit 1 ("flow from a stroke") only.  Bit 0 ("user-edited") belongs to
	 * the explicit setters -- name, class, colour -- because solve() cannot
	 * un-set it when the stroke is removed without wiping THEIR edit, and a
	 * bit that can be set but never cleared breaks the undo gate. */
	rec.flags |= quint8( 1u << 1 );
	{
		double cs = 0.0;
		qint64 cnt = 0;
		for ( size_t i = 0; i < n; i++ )
			if ( F->mask[i] ) {
				cs += F->conf[i];
				cnt++;
			}
		rec.confidence = quint8( std::max( 0, std::min( 255,
			int( cnt ? cs / double( cnt ) / 15.0 * 255.0 + 0.5 : 0.0 ) ) ) );
	}
	// a source/outlet pin that lands on ANOTHER body states the graph
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() || s.pts.isEmpty() || int( s.body ) != id )
			continue;
		if ( s.kind == WaterStroke::SourcePin ) {
			const quint16 o = bodyAtWorld( double( s.pts.first().x ), double( s.pts.first().y ) );
			if ( o && int( o ) != id )
				rec.source = o;
		}
		if ( s.kind == WaterStroke::OutletPin ) {
			const quint16 o = bodyAtWorld( double( s.pts.last().x ), double( s.pts.last().y ) );
			if ( o && int( o ) != id )
				rec.outlet = o;
		}
	}
	fields.insert( id, F );
	st.bodiesSolved++;
	return true;
}

/*! The plumes.
 *
 *  bungo: *"a factory that's releasing toxic sludge into a river, or river
 *  flowing into an ocean and the river and the ocean may have slightly
 *  different color"*.  A DyePin's plume rides its body's own solved field
 *  from the pin downstream; a DyeMouth mark carries a river's water into
 *  the body it drains to, which gets a field of its own cut around the mouth
 *  -- the mouth as the source, the window's cut edges as the far field --
 *  that writes DYE ONLY: its flow words stay the writer's, its record is not
 *  touched, so a sea that nobody stroked keeps its zero flow and the undo
 *  gate keeps its byte identity. */
void WaterMarkDoc::solveDye( WaterMarkSolve & st )
{
	const double u = worldPerTexel();
	const double halfTexels = dyeHalfDistance() / u;
	int pinIndex = 0;
	auto texelIn = [&]( const Field * F, double wx, double wy, int & x, int & y ) {
		int px = 0, py = 0;
		worldToTexel( wx, wy, px, py );
		x = px - F->px0;
		y = py - F->py0;
		return x >= 0 && y >= 0 && x < F->w && y < F->h;
	};
	auto paint = [&]( Field * F, const std::vector<double> & c, quint32 source ) {
		qint64 painted = 0;
		for ( int i = 0; i < F->grid.n; i++ ) {
			const size_t at = size_t( F->grid.cy[size_t( i )] ) * size_t( F->w )
				+ size_t( F->grid.cx[size_t( i )] );
			const int wgt = int( std::max( 0.0, std::min( 1.0, c[size_t( i )] ) ) * 255.0 + 0.5 );
			if ( wgt <= 0 )
				continue;
			const int have = int( ( F->dye[at] >> 16 ) & 0xFF );
			if ( wgt > have ) {
				F->dye[at] = ( source & 0xFFFF ) | ( quint32( wgt ) << 16 );
				painted++;
			}
		}
		return painted;
	};
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() )
			continue;
		if ( s.kind == WaterStroke::DyePin ) {
			const int n = pinIndex++;
			if ( s.pts.isEmpty() || !s.body )
				continue;
			auto it = fields.find( int( s.body ) );
			if ( it == fields.end() ) {
				QString note;
				if ( !solveBody( int( s.body ), st, &note ) ) {
					if ( st.note.isEmpty() )
						st.note = QStringLiteral( "dye pin %1: %2" ).arg( n ).arg( note );
					continue;
				}
				it = fields.find( int( s.body ) );
			}
			Field * F = it.value();
			if ( F->zero || F->phi.empty() )
				continue;
			std::vector<double> held( size_t( F->grid.n ), -1.0 );
			int x = 0, y = 0;
			if ( !texelIn( F, s.pts.first().x, s.pts.first().y, x, y ) )
				continue;
			const double strength = std::max( 0.0, std::min( 1.0, double( s.speed ) ) );
			const int r = std::max( 1, int( double( s.width ) * 0.5 / u + 0.5 ) );
			int seeded = 0;
			for ( int dy = -r; dy <= r; dy++ )
				for ( int dx = -r; dx <= r; dx++ ) {
					if ( dx * dx + dy * dy > r * r )
						continue;
					const int nx = x + dx, ny = y + dy;
					if ( nx < 0 || ny < 0 || nx >= F->w || ny >= F->h )
						continue;
					const int i = F->grid.idx[size_t( ny ) * size_t( F->w ) + size_t( nx )];
					if ( i < 0 )
						continue;
					held[size_t( i )] = strength;
					seeded++;
				}
			if ( !seeded )
				continue;
			std::vector<double> c;
			F->grid.dye( F->phi, held, halfTexels, c );
			const qint64 painted = paint( F, c, quint32( 0x8000 | ( n & 0x7FFF ) ) );
			if ( painted ) {
				st.dyeBodies++;
				st.dyeTexels += painted;
			}
		} else if ( s.kind == WaterStroke::DyeMouth ) {
			const int river = int( s.body );
			auto rit = fields.find( river );
			if ( rit == fields.end() || rit.value()->zero || rit.value()->mask.empty() )
				continue;
			const Field * R = rit.value();
			LodtWaterBody rb;
			body( river, rb );
			const quint16 other = rb.outlet;
			if ( !other || int( other ) == river ) {
				if ( st.note.isEmpty() )
					st.note = QStringLiteral( "body %1 drains into nothing the table knows, so "
						"its dye has nowhere to go" ).arg( river );
				continue;
			}
			// the receiving texels: `other`'s texels within 2.5 texels of the river
			std::vector<std::pair<int, int>> mouth;   // plane texels of `other`
			int mx0 = 1 << 30, my0 = 1 << 30, mx1 = -1, my1 = -1;
			for ( int y = 0; y < R->h; y++ )
				for ( int x = 0; x < R->w; x++ ) {
					if ( !R->mask[size_t( y ) * size_t( R->w ) + size_t( x )] )
						continue;
					for ( int dy = -2; dy <= 2; dy++ )
						for ( int dx = -2; dx <= 2; dx++ ) {
							if ( dx * dx + dy * dy > 6 )
								continue;
							const int px = R->px0 + x + dx, py = R->py0 + y + dy;
							if ( idAtTexel( px, py ) != other )
								continue;
							mouth.push_back( std::make_pair( px, py ) );
							mx0 = std::min( mx0, px );
							my0 = std::min( my0, py );
							mx1 = std::max( mx1, px );
							my1 = std::max( my1, py );
						}
				}
			if ( mouth.empty() ) {
				if ( st.note.isEmpty() )
					st.note = QStringLiteral( "body %1 and the body it drains into (%2) do not "
						"touch, so its dye has no mouth to leave by" ).arg( river ).arg( other );
				continue;
			}
			Field * O = nullptr;
			auto oit = fields.find( int( other ) );
			bool covers = false;
			if ( oit != fields.end() && !oit.value()->zero && !oit.value()->phi.empty() ) {
				O = oit.value();
				covers = O->holds( mx0, my0 ) && O->holds( mx1, my1 );
			}
			if ( !covers ) {
				if ( O && !O->dyeOnly )
					continue;        // the user's own field on the receiver does not reach the mouth
				// a DYE-ONLY field for the receiver, cut around the mouth
				delete O;
				fields.remove( int( other ) );
				O = new Field();
				O->dyeOnly = true;
				LodtWaterBody ob;
				body( int( other ), ob );
				const int obx0 = ( int( ob.x0 ) - lodl->cellMinX() ) * int( idRate );
				const int oby0 = ( int( ob.y0 ) - lodl->cellMinY() ) * int( idRate );
				const int obx1 = obx0 + ( int( ob.x1 ) - int( ob.x0 ) + 1 ) * int( idRate ) - 1;
				const int oby1 = oby0 + ( int( ob.y1 ) - int( ob.y0 ) + 1 ) * int( idRate ) - 1;
				const int margin = int( 4.0 * halfTexels ) + 16;
				O->px0 = std::max( obx0, mx0 - margin );
				O->py0 = std::max( oby0, my0 - margin );
				O->w = std::min( obx1, mx1 + margin ) - O->px0 + 1;
				O->h = std::min( oby1, my1 + margin ) - O->py0 + 1;
				O->windowed = ( O->px0 > obx0 || O->py0 > oby0
					|| O->px0 + O->w - 1 < obx1 || O->py0 + O->h - 1 < oby1 );
				const size_t on = size_t( O->w ) * size_t( O->h );
				O->mask.assign( on, 0 );
				O->vx.assign( on, 0.0f );
				O->vy.assign( on, 0.0f );
				O->conf.assign( on, 0 );
				O->speed.assign( on, 8 );
				O->dye.assign( on, 0 );
				std::vector<quint8> cut( on, 0 );
				std::vector<float> k( on, 1.0f );
				const int spc = lodl->samplesPerCell();
				const int step = idRate > 0 ? std::max( 1, spc / int( idRate ) ) : 1;
				for ( int y = 0; y < O->h; y++ )
					for ( int x = 0; x < O->w; x++ ) {
						const size_t at = size_t( y ) * size_t( O->w ) + size_t( x );
						if ( idAtTexel( O->px0 + x, O->py0 + y ) != other )
							continue;
						O->mask[at] = 1;
						const float hgt = lodl->height( ( O->px0 + x ) * step, ( O->py0 + y ) * step );
						k[at] = float( std::max( kDepthFloor, double( ob.waterHeight ) - double( hgt ) ) );
						if ( O->windowed && ( x == 0 || y == 0 || x == O->w - 1 || y == O->h - 1 ) ) {
							const int ox = x == 0 ? -1 : ( x == O->w - 1 ? 1 : 0 );
							const int oy = y == 0 ? -1 : ( y == O->h - 1 ? 1 : 0 );
							if ( idAtTexel( O->px0 + x + ox, O->py0 + y + oy ) == other )
								cut[at] = 1;
						}
					}
				O->grid.build( O->w, O->h, O->mask, k );
				std::vector<double> rhs( size_t( O->grid.n ), 0.0 );
				std::vector<quint8> dset;
				bool anyCut = false;
				for ( size_t i = 0; i < on; i++ )
					if ( cut[i] && O->grid.idx[i] >= 0 )
						anyCut = true;
				if ( anyCut ) {
					dset.assign( size_t( O->grid.n ), 0 );
					for ( size_t i = 0; i < on; i++ )
						if ( cut[i] && O->grid.idx[i] >= 0 )
							dset[size_t( O->grid.idx[i] )] = 1;
				}
				int seeded = 0;
				for ( const auto & m : mouth ) {
					const int x = m.first - O->px0, y = m.second - O->py0;
					if ( x < 0 || y < 0 || x >= O->w || y >= O->h )
						continue;
					const int i = O->grid.idx[size_t( y ) * size_t( O->w ) + size_t( x )];
					if ( i >= 0 ) {
						rhs[size_t( i )] += 1.0;
						seeded++;
					}
				}
				if ( !seeded || O->grid.n <= 0 ) {
					delete O;
					continue;
				}
				for ( int i = 0; i < O->grid.n; i++ )
					rhs[size_t( i )] /= double( seeded );
				if ( !anyCut )
					for ( int i = 0; i < O->grid.n; i++ )
						rhs[size_t( i )] -= 1.0 / double( O->grid.n );   // a lake: seepage everywhere
				int it = 0;
				double res = 0.0;
				O->grid.solve( rhs, dset, O->phi, it, res );
				st.iterations = std::max( st.iterations, it );
				st.residual = std::max( st.residual, res );
				fields.insert( int( other ), O );
			}
			std::vector<double> held( size_t( O->grid.n ), -1.0 );
			const double strength = std::max( 0.0, std::min( 1.0, double( s.speed ) ) );
			int seeded = 0;
			for ( const auto & m : mouth ) {
				const int x = m.first - O->px0, y = m.second - O->py0;
				if ( x < 0 || y < 0 || x >= O->w || y >= O->h )
					continue;
				const int i = O->grid.idx[size_t( y ) * size_t( O->w ) + size_t( x )];
				if ( i >= 0 ) {
					held[size_t( i )] = strength;
					seeded++;
				}
			}
			if ( !seeded )
				continue;
			std::vector<double> c;
			O->grid.dye( O->phi, held, halfTexels, c );
			const qint64 painted = paint( O, c, quint32( river ) );
			if ( painted ) {
				st.dyeBodies++;
				st.dyeTexels += painted;
			}
		}
	}
}

bool WaterMarkDoc::solve( WaterMarkSolve * out, QString * error )
{
	WaterMarkSolve st;
	if ( error )
		error->clear();
	if ( !opened ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	qDeleteAll( fields );
	fields.clear();

	/* PUT BACK WHAT AN EARLIER SOLVE DERIVED.  The strokes are the source, so
	 * flowX, flowY, flowSource, confidence, source, outlet and flag bit 1 are
	 * re-derived from scratch every time and never accumulated.  Without this
	 * a body that was marked once kept its stroke's mean for the rest of the
	 * document's life, and since the flow plane is derived from that mean,
	 * removing the stroke could not reproduce the file it started from -- gate
	 * P3 failed by 1,021,405 bytes, the sea's share of the plane.  The fields
	 * the PANEL owns -- name, class, colour, water form and flag bits 0, 2
	 * and 5 -- are deliberately not touched here. */
	for ( int i = 0; i < table.size() && i < tableAtOpen.size(); i++ ) {
		const LodtWaterBody & o = tableAtOpen[i];
		LodtWaterBody & r = table[i];
		r.flowX = o.flowX;
		r.flowY = o.flowY;
		r.flowSource = o.flowSource;
		r.confidence = o.confidence;
		r.source = o.source;
		r.outlet = o.outlet;
		r.flags = quint8( ( r.flags & ~quint8( 1u << 1 ) ) | ( o.flags & quint8( 1u << 1 ) ) );
	}

	// which bodies carry a constraint at all (the dye knob names no body)
	QSet<int> want;
	for ( const WaterStroke & s : marks )
		if ( s.enabled() && s.body && s.kind != WaterStroke::DyeKnob )
			want.insert( int( s.body ) );
	for ( int i = 1; i <= lockZero.size(); i++ )
		if ( lockZero[i - 1] )
			want.insert( i );

	QElapsedTimer clock;
	clock.start();
	QString note;
	QList<int> ids = want.values();
	std::sort( ids.begin(), ids.end() );
	for ( int id : ids )
		solveBody( id, st, &note );
	solveDye( st );
	st.solveSeconds = double( clock.nsecsElapsed() ) / 1e9;
	if ( st.note.isEmpty() )
		st.note = note;

	// how many texels the field actually moved, over the marked bodies
	for ( auto it = fields.constBegin(); it != fields.constEnd(); ++it ) {
		const Field * F = it.value();
		if ( F->dyeOnly )
			continue;
		const quint16 autoWord = automaticWord( quint16( it.key() ) );
		if ( F->zero ) {
			if ( autoWord )
				st.changedTexels += qint64( table[it.key() - 1].area );
			continue;
		}
		for ( int y = 0; y < F->h; y++ )
			for ( int x = 0; x < F->w; x++ ) {
				const size_t at = size_t( y ) * size_t( F->w ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				if ( flowWordAt( F->px0 + x, F->py0 + y ) != autoWord )
					st.changedTexels++;
			}
	}
	if ( st.note.isEmpty() ) {
		st.note = st.bodiesSolved
			? QStringLiteral( "%1 stroke(s) on %2 body(ies): %3 texels under them, %4 changed, "
				"%5 iterations, residual %6, %7 s%8" )
				.arg( st.strokes ).arg( st.bodiesSolved ).arg( st.constrained )
				.arg( st.changedTexels ).arg( st.iterations )
				.arg( st.residual, 0, 'g', 3 ).arg( st.solveSeconds, 0, 'f', 2 )
				.arg( st.dyeTexels ? QStringLiteral( "; dye on %1 texels" ).arg( st.dyeTexels )
					: QString() )
			: QStringLiteral( "no strokes: every body keeps the direction the classifier gave it" );
	}
	dirty = true;
	if ( out )
		*out = st;
	return true;
}

// ---- the dye marks and the dye plane ---------------------------------------

void WaterMarkDoc::setBodyDyeMouth( int id, bool on, float strength )
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return;
	for ( int i = marks.size() - 1; i >= 0; i-- )
		if ( marks[i].kind == WaterStroke::DyeMouth && int( marks[i].body ) == id )
			marks.removeAt( i );
	if ( on ) {
		WaterStroke s;
		s.body = quint16( id );
		s.kind = WaterStroke::DyeMouth;
		s.flags = WaterStroke::SetsSpeed;
		s.speed = std::max( 0.0f, std::min( 1.0f, strength ) );
		s.width = 0.0f;
		WaterStrokePoint p;
		p.x = float( ( double( b.x0 ) + double( b.x1 ) + 1.0 ) * 0.5 * kCellUnits );
		p.y = float( ( double( b.y0 ) + double( b.y1 ) + 1.0 ) * 0.5 * kCellUnits );
		s.pts.append( p );
		marks.append( s );
	}
	dirty = true;
}

bool WaterMarkDoc::bodyDyeMouth( int id, float * strength ) const
{
	for ( const WaterStroke & s : marks )
		if ( s.kind == WaterStroke::DyeMouth && s.enabled() && int( s.body ) == id ) {
			if ( strength )
				*strength = s.speed;
			return true;
		}
	return false;
}

double WaterMarkDoc::dyeHalfDistance() const
{
	for ( const WaterStroke & s : marks )
		if ( s.kind == WaterStroke::DyeKnob && s.enabled() && s.width > 0.0f )
			return double( s.width );
	return kDyeHalfDistanceDefault;
}

void WaterMarkDoc::setDyeHalfDistance( double worldUnits )
{
	for ( int i = marks.size() - 1; i >= 0; i-- )
		if ( marks[i].kind == WaterStroke::DyeKnob )
			marks.removeAt( i );
	if ( worldUnits > 0.0 && std::fabs( worldUnits - kDyeHalfDistanceDefault ) > 0.5 ) {
		WaterStroke s;
		s.body = 0;
		s.kind = WaterStroke::DyeKnob;
		s.flags = 0;
		s.speed = 0.0f;
		s.width = float( worldUnits );
		marks.append( s );
	}
	dirty = true;
}

bool WaterMarkDoc::hasDye() const
{
	for ( const WaterStroke & s : marks )
		if ( s.enabled() && ( s.kind == WaterStroke::DyePin || s.kind == WaterStroke::DyeMouth ) )
			return true;
	return false;
}

quint32 WaterMarkDoc::dyeWordOf( int px, int py, quint16 id ) const
{
	if ( !id )
		return 0;
	auto it = fields.constFind( int( id ) );
	if ( it == fields.constEnd() )
		return 0;
	const Field * F = it.value();
	if ( F->zero || F->dye.empty() || !F->holds( px, py ) )
		return 0;
	const size_t at = F->at( px, py );
	return F->mask[at] ? F->dye[at] : 0;
}

quint32 WaterMarkDoc::dyeWordAt( int px, int py ) const
{
	return dyeWordOf( px, py, idAtTexel( px, py ) );
}

/*! The word the WRITER puts on every texel of a body nobody marked.
 *
 *  Reproduced here rather than read from the file, so the re-derivation is a
 *  function of (id plane, body table) exactly as the writer's is -- which is
 *  what the repack-identity gate measures. */
quint16 WaterMarkDoc::automaticWord( quint16 id ) const
{
	if ( !id || int( id ) > table.size() )
		return 0;
	const LodtWaterBody & b = table[id - 1];
	const double m = std::sqrt( double( b.flowX ) * b.flowX + double( b.flowY ) * b.flowY );
	if ( m <= 0.0 )
		return 0;
	double a = std::atan2( double( b.flowY ), double( b.flowX ) );
	if ( a < 0.0 )
		a += kTwoPi;
	const int dir = int( a / kTwoPi * 256.0 + 0.5 ) & 0xFF;
	return quint16( dir | ( 8 << 8 ) );
}

quint16 WaterMarkDoc::flowWordAt( int px, int py ) const
{
	syncRasters();
	return flowWordOf( px, py, idAtTexel( px, py ) );
}

quint16 WaterMarkDoc::flowWordOf( int px, int py, quint16 id ) const
{
	if ( !id )
		return 0;
	/* Lane WATER6 (C3): an imported raster layer is the AUTHORITY where it is
	 * painted -- the last one painted wins.  The solve is untouched: this is a
	 * question about the word the document WRITES, not about the field it
	 * solved.  The cache is refreshed at the top of every read pass. */
	for ( int i = rasterCache.size() - 1; i >= 0; i-- ) {
		quint16 w = 0;
		if ( rasterCache[i].wordAt( px, py, w ) )
			return w;
	}
	auto it = fields.constFind( int( id ) );
	if ( it != fields.constEnd() ) {
		const Field * F = it.value();
		if ( F->dyeOnly )
			return automaticWord( id );
		if ( F->zero )
			return 0;
		if ( F->holds( px, py ) ) {
			const size_t at = F->at( px, py );
			if ( F->mask[at] ) {
				const double vx = F->vx[at], vy = F->vy[at];
				if ( std::fabs( vx ) < 1e-6 && std::fabs( vy ) < 1e-6 )
					return automaticWord( id );
				double a = std::atan2( vy, vx );
				if ( a < 0.0 )
					a += kTwoPi;
				const int dir = int( a / kTwoPi * 256.0 + 0.5 ) & 0xFF;
				return quint16( dir | ( quint16( F->speed[at] & 0xF ) << 8 )
					| ( quint16( F->conf[at] & 0xF ) << 12 ) );
			}
		}
	}
	return automaticWord( id );
}

/*! Lane WATER6: the kind-10 raster layers, decoded from the marks when they
 *  differ from what is cached.  Called at the top of every read pass. */
void WaterMarkDoc::syncRasters() const
{
	QVector<QByteArray> now;
	for ( const WaterStroke & s : marks )
		if ( s.kind == 10 && s.enabled() )
			now.append( s.extra );
	if ( now == rasterSrc )
		return;
	rasterSrc = now;
	rasterCache.clear();
	for ( const QByteArray & p : now ) {
		WaterRasterLayer r;
		QString why;
		if ( r.fromPayload( p, &why ) )
			rasterCache.append( r );
	}
}

bool WaterMarkDoc::sweep( const std::function<void( int, int, quint16, quint16, quint16 )> & cb,
	QString * error ) const
{
	syncRasters();
	if ( !opened || !reader ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not open %1" ).arg( filePath );
		return false;
	}
	const int e = idPlane.tileEdge;
	QByteArray raw;
	quint32 uni = 0;
	bool isUni = false;
	for ( int ty = 0; ty < idPlane.tilesY; ty++ ) {
		for ( int tx = 0; tx < idPlane.tilesX; tx++ ) {
			if ( !tileOf( f, idPlane, tx, ty, raw, uni, isUni ) ) {
				if ( error )
					*error = QStringLiteral( "tile %1,%2 of the body plane would not inflate" )
						.arg( tx ).arg( ty );
				return false;
			}
			for ( int j = 0; j < e; j++ ) {
				for ( int i = 0; i < e; i++ ) {
					const quint16 id = isUni ? quint16( uni )
						: rd16( raw.constData() + ( qsizetype( j ) * e + i ) * 2 );
					const int px = tx * e + i, py = ty * e + j;
					cb( px, py, id, automaticWord( id ), flowWordOf( px, py, id ) );
				}
			}
		}
	}
	return true;
}

bool WaterMarkDoc::setFlowRate( int samplesPerCell, QString * error )
{
	const int spc = lodl ? lodl->samplesPerCell() : 0;
	if ( samplesPerCell <= 0 || !spc || spc % samplesPerCell ) {
		if ( error )
			*error = QStringLiteral( "a plane rate must divide the file's own %1 samples a cell; "
				"%2 does not" ).arg( spc ).arg( samplesPerCell );
		return false;
	}
	if ( int( flowRate ) != samplesPerCell ) {
		flowRate = quint32( samplesPerCell );
		dirty = true;
	}
	return true;
}

/*! The mean flow direction of body `id`, read back OUT OF THE FILE.
 *
 *  The flow plane is tiled per CELL exactly as the body plane is, so tile
 *  (tx,ty) of one is tile (tx,ty) of the other and both are inflated once
 *  per tile. The sample -> body-texel mapping is packFlowPlane's own, on
 *  purpose: an instrument that mapped differently from the packer would
 *  measure a plane nobody wrote. */
bool WaterMarkDoc::meanFileFlow( int id, bool onlyMarked, double & degrees,
	qint64 & samples, QString * error ) const
{
	degrees = 0.0;
	samples = 0;
	if ( !opened || !flowPlane.ok || !idPlane.ok ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	const int spc = lodl->samplesPerCell();
	const int fe = flowPlane.tileEdge;
	const int ie = idPlane.tileEdge;
	if ( fe <= 0 || ie <= 0 || spc % fe || spc % ie ) {
		if ( error )
			*error = QStringLiteral( "plane rates %1 / %2 do not divide %3 samples a cell" )
				.arg( fe ).arg( ie ).arg( spc );
		return false;
	}
	const int flowStep = spc / fe, idStep = spc / ie;
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not open %1" ).arg( filePath );
		return false;
	}
	QByteArray fRaw, iRaw;
	quint32 fUni = 0, iUni = 0;
	bool fIsUni = false, iIsUni = false;
	double sx = 0.0, sy = 0.0;
	const int tilesX = qMin( flowPlane.tilesX, idPlane.tilesX );
	const int tilesY = qMin( flowPlane.tilesY, idPlane.tilesY );
	for ( int ty = 0; ty < tilesY; ty++ ) {
		for ( int tx = 0; tx < tilesX; tx++ ) {
			if ( !tileOf( f, flowPlane, tx, ty, fRaw, fUni, fIsUni )
				|| !tileOf( f, idPlane, tx, ty, iRaw, iUni, iIsUni ) ) {
				if ( error )
					*error = QStringLiteral( "tile %1,%2 would not inflate" ).arg( tx ).arg( ty );
				return false;
			}
			for ( int j = 0; j < fe; j++ ) {
				const int lj = ( j * flowStep ) / idStep;
				for ( int i = 0; i < fe; i++ ) {
					const int li = ( i * flowStep ) / idStep;
					const quint16 bid = iIsUni ? quint16( iUni )
						: rd16( iRaw.constData() + ( qsizetype( lj ) * ie + li ) * 2 );
					if ( int( bid ) != id )
						continue;
					const quint16 word = fIsUni ? quint16( fUni )
						: rd16( fRaw.constData() + ( qsizetype( j ) * fe + i ) * 2 );
					if ( onlyMarked && word == automaticWord( bid ) )
						continue;
					const double ang = double( word & 0xFF ) / 256.0 * kTwoPi;
					sx += std::cos( ang );
					sy += std::sin( ang );
					samples++;
				}
			}
		}
	}
	degrees = std::atan2( sy, sx ) * 180.0 / 3.14159265358979;
	return samples > 0;
}

// =========================================================================
//  save
// =========================================================================

/*! The flow plane, re-derived, as the bytes that would sit at `base`.
 *
 *  ONE function serves the save and the identity gate, which is the whole point:
 *  the gate compares these bytes with the ones the WRITER put in the file, so a
 *  drift between this file's packer and lodtfile.cpp's shows up as a byte
 *  difference and not as a picture nobody looked at. */
QByteArray WaterMarkDoc::packFlowPlane( quint64 base, QString * error ) const
{
	syncRasters();
	if ( !opened || !idPlane.ok ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return QByteArray();
	}
	const int spc = lodl->samplesPerCell();
	const int fr = int( flowRate );
	const int idStep = spc / int( idRate );
	const int flowStep = spc / fr;
	QFile idf( filePath );
	if ( !idf.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not re-open %1 for the body plane" ).arg( filePath );
		return QByteArray();
	}
	QByteArray idRaw;
	quint32 idUni = 0;
	bool idIsUni = false;
	int cachedTx = -1, cachedTy = -1;
	bool bad = false;
	auto idAt = [&]( int px, int py ) -> quint16 {
		const int e = idPlane.tileEdge;
		const int tx = px / e, ty = py / e;
		if ( px < 0 || py < 0 || tx >= idPlane.tilesX || ty >= idPlane.tilesY )
			return 0;
		if ( tx != cachedTx || ty != cachedTy ) {
			if ( !tileOf( idf, idPlane, tx, ty, idRaw, idUni, idIsUni ) ) {
				bad = true;
				return 0;
			}
			cachedTx = tx;
			cachedTy = ty;
		}
		if ( idIsUni )
			return quint16( idUni );
		return rd16( idRaw.constData() + ( qsizetype( py % e ) * e + ( px % e ) ) * 2 );
	};
	qint64 uniform = 0;
	QByteArray plane = packPlane( idPlane.tilesX, idPlane.tilesY, fr, 2, base,
		[&]( int tx, int ty, quint8 * dst ) {
			for ( int j = 0; j < fr; j++ ) {
				const qint64 gy = qint64( ty ) * spc + qint64( j ) * flowStep;
				for ( int i = 0; i < fr; i++ ) {
					const qint64 gx = qint64( tx ) * spc + qint64( i ) * flowStep;
					const int px = int( gx / idStep ), py = int( gy / idStep );
					const quint16 id = idAt( px, py );
					const quint16 word = id ? flowWordOf( px, py, id ) : quint16( 0 );
					dst[( j * fr + i ) * 2] = quint8( word & 0xFF );
					dst[( j * fr + i ) * 2 + 1] = quint8( word >> 8 );
				}
			}
		}, &uniform );
	if ( bad ) {
		if ( error )
			*error = QStringLiteral( "a tile of the body-ID plane would not inflate" );
		return QByteArray();
	}
	return plane;
}

/*! The dye plane: the same packer, 4 bytes a sample, at the flow plane's
 *  rate, from dyeWordOf() -- the function the panel paints with. */
QByteArray WaterMarkDoc::packDyePlane( quint64 base, QString * error ) const
{
	if ( !opened || !idPlane.ok ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return QByteArray();
	}
	const int spc = lodl->samplesPerCell();
	const int fr = int( flowRate );
	const int idStep = spc / int( idRate );
	const int flowStep = spc / fr;
	QFile idf( filePath );
	if ( !idf.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not re-open %1 for the body plane" ).arg( filePath );
		return QByteArray();
	}
	QByteArray idRaw;
	quint32 idUni = 0;
	bool idIsUni = false;
	int cachedTx = -1, cachedTy = -1;
	bool bad = false;
	auto idAt = [&]( int px, int py ) -> quint16 {
		const int e = idPlane.tileEdge;
		const int tx = px / e, ty = py / e;
		if ( px < 0 || py < 0 || tx >= idPlane.tilesX || ty >= idPlane.tilesY )
			return 0;
		if ( tx != cachedTx || ty != cachedTy ) {
			if ( !tileOf( idf, idPlane, tx, ty, idRaw, idUni, idIsUni ) ) {
				bad = true;
				return 0;
			}
			cachedTx = tx;
			cachedTy = ty;
		}
		if ( idIsUni )
			return quint16( idUni );
		return rd16( idRaw.constData() + ( qsizetype( py % e ) * e + ( px % e ) ) * 2 );
	};
	qint64 uniform = 0;
	QByteArray plane = packPlane( idPlane.tilesX, idPlane.tilesY, fr, 4, base,
		[&]( int tx, int ty, quint8 * dst ) {
			for ( int j = 0; j < fr; j++ ) {
				const qint64 gy = qint64( ty ) * spc + qint64( j ) * flowStep;
				for ( int i = 0; i < fr; i++ ) {
					const qint64 gx = qint64( tx ) * spc + qint64( i ) * flowStep;
					const int px = int( gx / idStep ), py = int( gy / idStep );
					const quint16 id = idAt( px, py );
					const quint32 word = id ? dyeWordOf( px, py, id ) : quint32( 0 );
					for ( int c = 0; c < 4; c++ )
						dst[( j * fr + i ) * 4 + c] = quint8( ( word >> ( 8 * c ) ) & 0xFF );
				}
			}
		}, &uniform );
	if ( bad ) {
		if ( error )
			*error = QStringLiteral( "a tile of the body-ID plane would not inflate" );
		return QByteArray();
	}
	return plane;
}

bool WaterMarkDoc::tableRepackMatches() const
{
	return !originalTable.isEmpty() && encodeTable() == originalTable;
}

bool WaterMarkDoc::flowRepackMatches( qint64 * differingBytes, QString * error ) const
{
	if ( differingBytes )
		*differingBytes = -1;
	if ( !opened || !flowPlane.ok ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	const QByteArray ours = packFlowPlane( oFlow, error );
	if ( ours.isEmpty() )
		return false;
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not open %1" ).arg( filePath );
		return false;
	}
	f.seek( qint64( oFlow ) );
	const QByteArray theirs = f.read( qint64( flowPlane.bytes ) );
	qint64 diff = qAbs( qint64( ours.size() ) - qint64( theirs.size() ) );
	const qint64 common = qMin( qint64( ours.size() ), qint64( theirs.size() ) );
	for ( qint64 i = 0; i < common; i++ )
		if ( ours.at( int( i ) ) != theirs.at( int( i ) ) )
			diff++;
	if ( differingBytes )
		*differingBytes = diff;
	return diff == 0;
}

bool WaterMarkDoc::copyRange( QFile & in, QFile & out, quint64 at, quint64 bytes,
	QString * error ) const
{
	in.seek( qint64( at ) );
	quint64 left = bytes;
	while ( left ) {
		const qint64 want = qint64( qMin<quint64>( left, 4u << 20 ) );
		const QByteArray chunk = in.read( want );
		if ( chunk.size() != want ) {
			if ( error )
				*error = QStringLiteral( "the file ends %1 bytes early at 0x%2" )
					.arg( left ).arg( at, 0, 16 );
			return false;
		}
		if ( out.write( chunk ) != chunk.size() ) {
			if ( error )
				*error = QStringLiteral( "could not write %1 bytes" ).arg( chunk.size() );
			return false;
		}
		left -= quint64( chunk.size() );
	}
	return true;
}

/*! Write the file: everything up to the body table byte for byte, then the tail
 *  re-derived.
 *
 *  The original is renamed aside rather than deleted (`.bak-watermark`), which
 *  is this feature's zero-effort way back (CONSTITUTION rule 7). */
bool WaterMarkDoc::save( QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !opened )
		return fail( QStringLiteral( "no landscape file is open" ) );

	const QString tmpPath = filePath + QStringLiteral( ".tmp-watermark" );
	QFile::remove( tmpPath );
	QFile in( filePath );
	if ( !in.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "could not read %1" ).arg( filePath ) );
	QFile out( tmpPath );
	if ( !out.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QStringLiteral( "could not write %1" ).arg( tmpPath ) );

	QByteArray hdr = in.read( kHdrV3 );
	if ( hdr.size() != kHdrV3 )
		return fail( QStringLiteral( "%1 has no version-3 header" ).arg( filePath ) );
	// the header is rewritten at the end; write a placeholder so offsets are real
	if ( out.write( hdr ) != hdr.size() )
		return fail( QStringLiteral( "could not write the header" ) );
	if ( !copyRange( in, out, quint64( kHdrV3 ), oBody - quint64( kHdrV3 ), error ) )
		return false;

	quint64 pos = oBody;
	const QByteArray tableBytes = encodeTable();
	if ( out.write( tableBytes ) != tableBytes.size() )
		return fail( QStringLiteral( "could not write the body table" ) );
	patch64( hdr, kOffBody, pos );
	patch32( hdr, kOffBodyN, quint32( table.size() ) );
	patch32( hdr, kOffStride, quint32( kBodyRecord ) );
	pos += quint64( tableBytes.size() );

	const QByteArray nameBytes = encodeNames();
	patch64( hdr, kOffName, nameBytes.isEmpty() ? quint64( 0 ) : pos );
	patch32( hdr, kOffNameLen, quint32( nameBytes.size() ) );
	if ( !nameBytes.isEmpty() ) {
		if ( out.write( nameBytes ) != nameBytes.size() )
			return fail( QStringLiteral( "could not write the body name blob" ) );
		pos += quint64( nameBytes.size() );
	}

	const QByteArray strokeBytes = encodeStrokes();
	patch64( hdr, kOffStroke, pos );
	patch32( hdr, kOffStrokeL, quint32( strokeBytes.size() ) );
	if ( out.write( strokeBytes ) != strokeBytes.size() )
		return fail( QStringLiteral( "could not write the stroke store" ) );
	pos += quint64( strokeBytes.size() );

	// the body-ID plane: verbatim, rebased. Nothing here can move a body's shape.
	{
		in.seek( qint64( oId ) );
		const QByteArray bytes = in.read( qint64( idPlane.bytes ) );
		if ( bytes.size() != qint64( idPlane.bytes ) )
			return fail( QStringLiteral( "the body-ID plane is short on disk" ) );
		const QByteArray moved = rebasePlane( bytes, oId, pos );
		if ( out.write( moved ) != moved.size() )
			return fail( QStringLiteral( "could not write the body-ID plane" ) );
		patch32( hdr, kOffIdRate, idRate );
		patch64( hdr, kOffId, pos );
		pos += quint64( moved.size() );
	}

	// the flow plane: RE-DERIVED, tile by tile, from the id plane and the fields
	{
		QString perr;
		const QByteArray plane = packFlowPlane( pos, &perr );
		if ( plane.isEmpty() )
			return fail( perr );
		if ( out.write( plane ) != plane.size() )
			return fail( QStringLiteral( "could not write the flow plane" ) );
		patch32( hdr, kOffFlowRt, flowRate );
		patch32( hdr, kOffFlowEnc, 0 );
		patch64( hdr, kOffFlow, pos );
		pos += quint64( plane.size() );
	}

	// the shore plane: verbatim, rebased, for the same reason as the id plane
	if ( oShore && shorePlane.ok ) {
		in.seek( qint64( oShore ) );
		const QByteArray bytes = in.read( qint64( shorePlane.bytes ) );
		if ( bytes.size() != qint64( shorePlane.bytes ) )
			return fail( QStringLiteral( "the shore plane is short on disk" ) );
		const QByteArray moved = rebasePlane( bytes, oShore, pos );
		if ( out.write( moved ) != moved.size() )
			return fail( QStringLiteral( "could not write the shore plane" ) );
		patch32( hdr, kOffShoreRt, shoreRate );
		patch32( hdr, kOffShoreQ, shoreQuantum );
		patch64( hdr, kOffShore, pos );
		pos += quint64( moved.size() );
	}

	/* the dye plane: written only when the store carries a dye mark, so a
	 * file nobody dyed -- and a file whose dye was undone -- keeps the
	 * writer's own bytes; referenced from the reserved word at 0xF4 */
	if ( hasDye() ) {
		if ( pos > quint64( 0xFFFFFFFFu ) )
			return fail( QStringLiteral( "the dye plane would sit past 4 GB, which the version-3 "
				"header's 32-bit word cannot address" ) );
		QString perr;
		const QByteArray plane = packDyePlane( pos, &perr );
		if ( plane.isEmpty() )
			return fail( perr );
		if ( out.write( plane ) != plane.size() )
			return fail( QStringLiteral( "could not write the dye plane" ) );
		patch32( hdr, kOffDye, quint32( pos ) );
		sect |= kSectDye;
		pos += quint64( plane.size() );
	} else {
		patch32( hdr, kOffDye, 0 );
		sect &= ~kSectDye;
	}
	patch64( hdr, kOffSize, pos );
	patch32( hdr, kOffSect, sect );
	out.seek( 0 );
	if ( out.write( hdr ) != hdr.size() )
		return fail( QStringLiteral( "could not rewrite the header" ) );
	out.close();
	in.close();

	/* The reader holds the file open, and Windows will not rename over an open
	 * handle. Close it, move the original aside, put the new file in its place,
	 * and re-open. The .bak-watermark is the way back. */
	delete reader;
	reader = nullptr;
	delete lodl;
	lodl = nullptr;
	opened = false;
	const QString bak = filePath + QStringLiteral( ".bak-watermark" );
	QFile::remove( bak );
	if ( !QFile::rename( filePath, bak ) ) {
		QFile::remove( tmpPath );
		return fail( QStringLiteral( "could not move %1 aside" ).arg( filePath ) );
	}
	if ( !QFile::rename( tmpPath, filePath ) ) {
		QFile::rename( bak, filePath );
		return fail( QStringLiteral( "could not put the new %1 in place" ).arg( filePath ) );
	}
	/* Re-open the file we just wrote, and take the strokes and the locks back
	 * OUT of it rather than out of memory. That is what makes "save then reopen
	 * gives the same strokes" a property of the file and not of this object. */
	const QString keep = filePath;
	const int wrote = marks.size();
	/* open() re-takes tableAtOpen, and this is a re-open of a file we have
	 * just marked -- so without carrying the snapshot across, "the table as
	 * the GENERATOR wrote it" would silently become "the table as this
	 * session last saved it", and removing a stroke would restore the stroke.
	 * That is exactly what gate P3 caught, by 480 bytes. */
	const QVector<LodtWaterBody> genTable = tableAtOpen;
	QString err;
	if ( !open( keep, &err ) )
		return fail( QStringLiteral( "the file was written but will not re-open: %1" ).arg( err ) );
	if ( marks.size() != wrote )
		return fail( QStringLiteral( "%1 strokes were written and %2 read back" )
			.arg( wrote ).arg( marks.size() ) );
	if ( genTable.size() == tableAtOpen.size() )
		tableAtOpen = genTable;
	/* Re-solve from the strokes that came back, so what the panel shows after a
	 * save is what the file now holds. The solve is idempotent on its own
	 * output -- the strokes and the speeds are unchanged, so it lands on the
	 * same field -- and the round-trip gate is exactly that claim. */
	{
		WaterMarkSolve st;
		QString serr;
		solve( &st, &serr );
	}
	dirty = false;
	return true;
}

QString WaterMarkDoc::describeBody( int id ) const
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return QStringLiteral( "no body selected" );
	static const char * kCls[3] = { "sea", "river", "lake" };
	static const char * kSrc[5] = { "none", "the water form's own velocity", "the river bed",
		"a lower body nearby", "a stroke" };
	const QString nm = bodyName( id );
	const QString dot = QStringLiteral( "  -  " );
	QString s = QStringLiteral( "body %1" ).arg( id );
	if ( !nm.isEmpty() )
		s += QStringLiteral( " \"%1\"" ).arg( nm );
	s += dot + QStringLiteral( "%1" ).arg( QLatin1String( kCls[qBound( 0, int( b.cls ), 2 )] ) );
	s += dot + QStringLiteral( "%L1 texels" ).arg( b.area );
	s += dot + QStringLiteral( "plane %1" ).arg( double( b.waterHeight ), 0, 'f', 1 );
	s += dot + QStringLiteral( "water form %1" ).arg( b.watrForm, 8, 16, QLatin1Char( '0' ) );
	s += dot + QStringLiteral( "flow from %1" )
		.arg( QLatin1String( kSrc[qBound( 0, int( b.flowSource ), 4 )] ) );
	if ( b.colour[3] )
		s += dot + QStringLiteral( "colour override" );
	return s;
}

// =========================================================================
//  the harness (the headless half of WW_WATER_MARK_TEST)
// =========================================================================

/*! Every case carries the floor that stops an empty implementation passing,
 *  and the refuter is run BEFORE the check it protects is believed. */
// =========================================================================
//  the flow gates  (lane WATER4, pre-registered in its report section 0)
// =========================================================================

namespace {

constexpr double kPiD = 3.14159265358979;

/*! F1, F2, F3, F4, F6 and F7 on SYNTHETIC masks, through WaterFlowGrid alone
 *  -- no file, no body table.  Each is a known-answer control of the method:
 *  continuity in a narrows, parting round an island, a closed lake, a lake
 *  with one outlet, a river's plume into a sea, a dye pin's plume.  The
 *  numbers are the ones the lane report's section 0 registered before any of
 *  this existed. */
void waterFlowGates( const std::function<void( const QString &, bool )> & check,
	const std::function<void( const QString & )> & say )
{
	constexpr double kPi = 3.14159265358979;
	auto wetSolve = [&]( int w, int h, const std::vector<quint8> & wet,
		const std::vector<double> & rhsByTexel, const std::vector<quint8> & dirByTexel,
		WaterFlowGrid & G, std::vector<double> & phi, int & it, double & res ) {
		std::vector<float> k( size_t( w ) * size_t( h ), 1.0f );
		G.build( w, h, wet, k );
		std::vector<double> b( size_t( G.n ), 0.0 );
		std::vector<quint8> d;
		bool anyD = false;
		for ( size_t at = 0; at < wet.size(); at++ ) {
			const int i = G.idx[at];
			if ( i < 0 )
				continue;
			b[size_t( i )] = rhsByTexel.empty() ? 0.0 : rhsByTexel[at];
			if ( !dirByTexel.empty() && dirByTexel[at] )
				anyD = true;
		}
		if ( anyD ) {
			d.assign( size_t( G.n ), 0 );
			for ( size_t at = 0; at < wet.size(); at++ )
				if ( G.idx[at] >= 0 && dirByTexel[at] )
					d[size_t( G.idx[at] )] = 1;
		}
		G.solve( b, d, phi, it, res );
	};
	auto columnFlux = [&]( const WaterFlowGrid & G, const std::vector<double> & F, int x,
		int y0, int y1 ) {
		double s = 0.0;
		for ( int f = 0; f < G.nE; f++ ) {
			const int i = G.fi[size_t( f )];
			if ( G.cx[size_t( i )] == x && G.cy[size_t( i )] >= y0 && G.cy[size_t( i )] < y1 )
				s += F[size_t( f )];
		}
		return s;
	};

	// ---- F1: continuity ---------------------------------------------------
	{
		const int w = 256, h = 32;
		std::vector<quint8> wet( size_t( w ) * h, 0 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ )
			for ( int x = 0; x < w; x++ )
				wet[size_t( y ) * w + x] = ( x < 128 || ( y >= 8 && y < 24 ) ) ? 1 : 0;
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / 32.0;
			if ( y >= 8 && y < 24 )
				rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / 16.0;
		}
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, F;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		G.velocity( phi, ux, uy );
		G.faceFlux( phi, F );
		double s64 = 0.0, s192 = 0.0;
		int n64 = 0, n192 = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( G.cx[size_t( i )] == 64 ) {
				s64 += sp;
				n64++;
			}
			if ( G.cx[size_t( i )] == 192 ) {
				s192 += sp;
				n192++;
			}
		}
		s64 /= std::max( 1, n64 );
		s192 /= std::max( 1, n192 );
		const double ratio = s64 > 0.0 ? s192 / s64 : 0.0;
		check( QStringLiteral( "F1 continuity: a channel that narrows to half its width doubles "
			"its speed (x=64 %1, x=192 %2, ratio %3, needs 1.90..2.10; %4 iterations, residual "
			"%5)" ).arg( s64, 0, 'g', 5 ).arg( s192, 0, 'g', 5 ).arg( ratio, 0, 'f', 4 )
			.arg( it ).arg( res, 0, 'g', 2 ), ratio >= 1.90 && ratio <= 2.10 );
		double fmean = 0.0, fl[10];
		for ( int q = 0; q < 10; q++ ) {
			fl[q] = columnFlux( G, F, 16 + 24 * q, 0, h );
			fmean += fl[q];
		}
		fmean /= 10.0;
		double dev = 0.0;
		for ( int q = 0; q < 10; q++ )
			dev = std::max( dev, std::fabs( fl[q] - fmean ) / std::fabs( fmean ) );
		check( QStringLiteral( "F1 flux is the same through 10 cross-sections within 3 percent "
			"(max deviation %1)" ).arg( dev, 0, 'g', 3 ), dev < 0.03 );
	}

	// ---- F2: the island ---------------------------------------------------
	{
		const int w = 256, h = 128;
		const double R = 8.0, cxI = 128.0, cyI = 64.0;
		std::vector<quint8> wet( size_t( w ) * h, 1 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ )
			for ( int x = 0; x < w; x++ ) {
				const double dx = x + 0.5 - cxI, dy = y + 0.5 - cyI;
				if ( dx * dx + dy * dy < R * R )
					wet[size_t( y ) * w + x] = 0;
			}
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / h;
			rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / h;
		}
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, F, div;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		G.velocity( phi, ux, uy );
		G.faceFlux( phi, F );
		G.divergence( phi, div );
		const double north = columnFlux( G, F, 128, 64, h ), south = columnFlux( G, F, 128, 0, 64 );
		check( QStringLiteral( "F2 the flow parts round the island and rejoins: north %1 + south "
			"%2 = %3 of the inflow 1 (within 1 percent, halves within 2 percent; %4 iterations, "
			"residual %5)" ).arg( north, 0, 'f', 4 ).arg( south, 0, 'f', 4 )
			.arg( north + south, 0, 'f', 4 ).arg( it ).arg( res, 0, 'g', 2 ),
			std::fabs( north + south - 1.0 ) < 0.01 && std::fabs( north - south ) < 0.02 );
		double worstDiv = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const size_t at = size_t( G.cy[size_t( i )] ) * w + size_t( G.cx[size_t( i )] );
			worstDiv = std::max( worstDiv, std::fabs( div[size_t( i )] - rhs[at] ) );
		}
		check( QStringLiteral( "F2 mass balance at every wet cell to 1e-6 of the inflow (worst "
			"%1)" ).arg( worstDiv, 0, 'g', 3 ), worstDiv < 1e-6 );
		double worstNorm = 0.0;
		int bankN = 0;
		double Uinf = 0.0;
		int nInf = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( std::fabs( x + 0.5 - cxI ) > 3 * R ) {
				Uinf += sp;
				nInf++;
				if ( y == 0 || y == h - 1 ) {
					worstNorm = std::max( worstNorm, sp > 0.0 ? std::fabs( uy[size_t( i )] ) / sp : 0.0 );
					bankN++;
				}
			}
		}
		Uinf /= std::max( 1, nInf );
		check( QStringLiteral( "F2 straight-bank tangency: the normal component is below sin(1 deg) "
			"at every bank texel (worst %1 over %2 texels)" ).arg( worstNorm, 0, 'g', 3 ).arg( bankN ),
			worstNorm < std::sin( kPi / 180.0 ) );
		double sumD = 0.0, maxD = 0.0;
		int nD = 0, skipped = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
			const double dx = x + 0.5 - cxI, dy = y + 0.5 - cyI;
			if ( std::fabs( dx ) >= 2 * R || std::fabs( dy ) >= 2 * R )
				continue;
			bool bank = false;
			const int nx[4] = { x - 1, x + 1, x, x };
			const int ny[4] = { y, y, y - 1, y + 1 };
			for ( int q = 0; q < 4; q++ )
				if ( nx[q] >= 0 && ny[q] >= 0 && nx[q] < w && ny[q] < h
					&& !wet[size_t( ny[q] ) * w + nx[q]] )
					bank = true;
			if ( !bank )
				continue;
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( sp <= 0.05 * Uinf ) {
				skipped++;
				continue;
			}
			const double r = std::hypot( dx, dy ), th = std::atan2( dy, dx );
			const double ur = Uinf * ( 1.0 - R * R / ( r * r ) ) * std::cos( th );
			const double ut = -Uinf * ( 1.0 + R * R / ( r * r ) ) * std::sin( th );
			const double ax = ur * std::cos( th ) - ut * std::sin( th );
			const double ay = ur * std::sin( th ) + ut * std::cos( th );
			const double am = std::hypot( ax, ay );
			const double c = std::max( -1.0, std::min( 1.0,
				( ux[size_t( i )] * ax + uy[size_t( i )] * ay ) / ( sp * am ) ) );
			const double d = std::acos( c ) * 180.0 / kPi;
			sumD += d;
			maxD = std::max( maxD, d );
			nD++;
		}
		const double meanD = nD ? sumD / nD : 0.0;
		check( QStringLiteral( "F2 island bank direction against the analytic cylinder: mean %1, "
			"max %2 degrees over %3 bank texels (needs mean < 5, max < 15; %4 stagnation texels "
			"skipped)" ).arg( meanD, 0, 'f', 2 ).arg( maxD, 0, 'f', 2 ).arg( nD ).arg( skipped ),
			meanD < 5.0 && maxD < 15.0 );
	}

	// ---- F3 and F4: the lakes --------------------------------------------
	{
		const double R = 32.0;
		const int n = int( 2 * R + 8 );
		const double c = n / 2.0;
		std::vector<quint8> wet( size_t( n ) * n, 0 );
		for ( int y = 0; y < n; y++ )
			for ( int x = 0; x < n; x++ ) {
				const double dx = x + 0.5 - c, dy = y + 0.5 - c;
				wet[size_t( y ) * n + x] = dx * dx + dy * dy < R * R ? 1 : 0;
			}
		{
			WaterFlowGrid G;
			std::vector<double> phi, ux, uy;
			int it = 0;
			double res = 0.0;
			wetSolve( n, n, wet, std::vector<double>(), std::vector<quint8>(), G, phi, it, res );
			G.velocity( phi, ux, uy );
			double mx = 0.0;
			for ( int i = 0; i < G.n; i++ )
				mx = std::max( mx, std::hypot( ux[size_t( i )], uy[size_t( i )] ) );
			check( QStringLiteral( "F3 a lake with no outlet and no mark: max speed = 0 exactly "
				"(%1, %2 iterations)" ).arg( mx, 0, 'g', 3 ).arg( it ), mx == 0.0 );
		}
		{
			std::vector<quint8> outlet( size_t( n ) * n, 0 );
			std::vector<double> rhs( size_t( n ) * n, 0.0 );
			int nOut = 0;
			for ( int y = int( c ) - 1; y <= int( c ) + 1; y++ ) {
				int xm = -1;
				for ( int x = 0; x < n; x++ )
					if ( wet[size_t( y ) * n + x] )
						xm = x;
				if ( xm >= 0 ) {
					outlet[size_t( y ) * n + xm] = 1;
					nOut++;
				}
			}
			int nWet = 0;
			for ( size_t at = 0; at < wet.size(); at++ )
				if ( wet[at] )
					nWet++;
			for ( size_t at = 0; at < wet.size(); at++ ) {
				if ( wet[at] )
					rhs[at] += 1.0 / nWet;
				if ( outlet[at] )
					rhs[at] -= 1.0 / nOut;
			}
			WaterFlowGrid G;
			std::vector<double> phi, ux, uy;
			int it = 0;
			double res = 0.0;
			wetSolve( n, n, wet, rhs, std::vector<quint8>(), G, phi, it, res );
			G.velocity( phi, ux, uy );
			const double ox = c + R, oy = c;
			int away = 0, inner = 0;
			double cosSum = 0.0;
			for ( int i = 0; i < G.n; i++ ) {
				const size_t at = size_t( G.cy[size_t( i )] ) * n + size_t( G.cx[size_t( i )] );
				if ( outlet[at] )
					continue;
				const double tx = ox - ( G.cx[size_t( i )] + 0.5 ), ty = oy - ( G.cy[size_t( i )] + 0.5 );
				const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] ), tm = std::hypot( tx, ty );
				const double d = sp > 0.0 && tm > 0.0
					? ( ux[size_t( i )] * tx + uy[size_t( i )] * ty ) / ( sp * tm ) : 0.0;
				if ( d < 0.0 )
					away++;
				cosSum += d;
				inner++;
			}
			check( QStringLiteral( "F4 a lake with one outlet: 0 texels point away from it (%1 of %2 "
				"do; mean cosine %3; %4 iterations, residual %5)" ).arg( away ).arg( inner )
				.arg( inner ? cosSum / inner : 0.0, 0, 'f', 3 ).arg( it ).arg( res, 0, 'g', 2 ),
				away == 0 );
			int seeds = 0, reached = 0;
			for ( int sy = 0; sy < 8; sy++ )
				for ( int sx = 0; sx < 8; sx++ ) {
					const int x = int( 4 + ( n - 8 ) * sx / 7.0 ), y = int( 4 + ( n - 8 ) * sy / 7.0 );
					if ( !wet[size_t( y ) * n + x] )
						continue;
					seeds++;
					if ( G.trace( phi, x + 0.5, y + 0.5, outlet, int( 4 * 2 * R * 4 ) ) )
						reached++;
				}
			check( QStringLiteral( "F4 every streamline reaches the outlet (%1 of %2 seeds, within 4 "
				"diameters)" ).arg( reached ).arg( seeds ), seeds > 0 && reached == seeds );
		}
	}

	// ---- F7: the dye pin --------------------------------------------------
	{
		const int w = 256, h = 16;
		std::vector<quint8> wet( size_t( w ) * h, 1 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / h;
			rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / h;
		}
		WaterFlowGrid G;
		std::vector<double> phi, held, c;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		held.assign( size_t( G.n ), -1.0 );
		for ( int y = 0; y < h; y++ )
			held[size_t( G.idx[size_t( y ) * w + 32] )] = 1.0;
		G.dye( phi, held, 32.0, c );
		double c64 = 0.0, c128 = 0.0, up = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )];
			if ( x == 64 )
				c64 += c[size_t( i )] / h;
			if ( x == 128 )
				c128 += c[size_t( i )] / h;
			if ( x < 32 )
				up = std::max( up, c[size_t( i )] );
		}
		check( QStringLiteral( "F7 a dye pin's weight one half-distance downstream is 1/2 (%1)" )
			.arg( c64, 0, 'f', 4 ), std::fabs( c64 - 0.5 ) < 0.05 );
		check( QStringLiteral( "F7 upstream of the pin the weight is 0 (max %1)" ).arg( up, 0, 'g', 3 ),
			up == 0.0 );
		check( QStringLiteral( "F7 three half-distances downstream it is 1/8 (%1)" )
			.arg( c128, 0, 'f', 4 ), std::fabs( c128 - 0.125 ) < 0.0125 );
	}

	// ---- F6: the plume ----------------------------------------------------
	{
		const int W = 224, H = 128;
		std::vector<quint8> sea( size_t( W ) * H, 0 ), cut( size_t( W ) * H, 0 );
		std::vector<double> rhs( size_t( W ) * H, 0.0 );
		for ( int y = 0; y < H; y++ )
			for ( int x = 96; x < W; x++ ) {
				sea[size_t( y ) * W + x] = 1;
				if ( y == 0 || y == H - 1 || x == W - 1 )
					cut[size_t( y ) * W + x] = 1;
			}
		for ( int y = 56; y < 72; y++ )
			rhs[size_t( y ) * W + 96] += 1.0 / 16.0;
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, held, c;
		int it = 0;
		double res = 0.0;
		wetSolve( W, H, sea, rhs, cut, G, phi, it, res );
		G.velocity( phi, ux, uy );
		double mx = 0.0, my = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const double dx = G.cx[size_t( i )] + 0.5 - 96.0, dy = G.cy[size_t( i )] + 0.5 - 64.0;
			if ( std::hypot( dx, dy ) < 32.0 ) {
				mx += ux[size_t( i )];
				my += uy[size_t( i )];
			}
		}
		const double predDir = std::atan2( my, mx ) * 180.0 / kPi, predLen = 96.0;
		say( QStringLiteral( "F6 predicted from the flow before the dye: plume direction %1 degrees, "
			"1/8 length %2 texels" ).arg( predDir, 0, 'f', 1 ).arg( predLen, 0, 'f', 0 ) );
		held.assign( size_t( G.n ), -1.0 );
		for ( int y = 56; y < 72; y++ )
			held[size_t( G.idx[size_t( y ) * W + 96] )] = 1.0;
		G.dye( phi, held, 32.0, c );
		double measLen = -1.0;
		for ( int x = 97; x < W; x++ ) {
			const double c0 = c[size_t( G.idx[size_t( 64 ) * W + x - 1] )];
			const double c1 = c[size_t( G.idx[size_t( 64 ) * W + x] )];
			if ( c1 < 0.125 ) {
				const double f = ( c0 - 0.125 ) / ( c0 - c1 );
				measLen = ( x - 1 - 96 ) + f + 0.5;
				break;
			}
		}
		double tot = 0.0, cxs = 0.0, cys = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			tot += c[size_t( i )];
			cxs += c[size_t( i )] * ( G.cx[size_t( i )] + 0.5 );
			cys += c[size_t( i )] * ( G.cy[size_t( i )] + 0.5 );
		}
		const double measDir = tot > 0.0 ? std::atan2( cys / tot - 64.0, cxs / tot - 96.0 ) * 180.0 / kPi : 999.0;
		double dd = std::fabs( measDir - predDir );
		if ( dd > 180.0 )
			dd = 360.0 - dd;
		check( QStringLiteral( "F6 the plume's 1/8 length measured after the dye is within 10 percent "
			"of the prediction (%1 against %2 texels; %3 iterations, residual %4)" )
			.arg( measLen, 0, 'f', 1 ).arg( predLen, 0, 'f', 0 ).arg( it ).arg( res, 0, 'g', 2 ),
			measLen > 0.0 && std::fabs( measLen - predLen ) <= 0.1 * predLen );
		check( QStringLiteral( "F6 the plume's direction (its weight centroid, %1 degrees) is within "
			"9 degrees of the prediction (%2)" ).arg( measDir, 0, 'f', 1 ).arg( predDir, 0, 'f', 1 ),
			dd <= 9.0 );
	}
}

/*! F5's instrument, in C++: the spatial structure of the flow direction over
 *  one body as this document would WRITE it.  Seam-bounded constant-direction
 *  patches (>= 64 texels, the pre-registered definition), the 99th percentile
 *  of the angle difference between 4-adjacent wet texels, and the seam
 *  fraction.  The same numbers scratchpad/water4_20260910/disc_metric.py
 *  reads back out of the file. */
struct FlowStructure
{
	qint64 wet = 0, pairs = 0;
	double p99 = 0.0, seamFraction = 0.0;
	int patches = 0;
	double patchMaxRadius = 0.0;
};

FlowStructure flowStructure( const WaterMarkDoc & doc, const LodtWaterBody & b )
{
	FlowStructure out;
	int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
	doc.worldToTexel( double( b.x0 ) * 4096.0, double( b.y0 ) * 4096.0, px0, py0 );
	doc.worldToTexel( ( double( b.x1 ) + 1.0 ) * 4096.0, ( double( b.y1 ) + 1.0 ) * 4096.0, px1, py1 );
	const int W = px1 - px0 + 1, H = py1 - py0 + 1;
	if ( W <= 0 || H <= 0 )
		return out;
	std::vector<qint16> dir( size_t( W ) * size_t( H ), -1 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			double wx = 0, wy = 0;
			doc.texelToWorld( px0 + x, py0 + y, wx, wy );
			if ( doc.bodyAtWorld( wx, wy ) != b.id )
				continue;
			dir[size_t( y ) * W + x] = qint16( doc.flowWordAt( px0 + x, py0 + y ) & 0xFF );
			out.wet++;
		}
	auto adiff = [&]( int a, int c ) {
		int d = std::abs( a - c );
		d = std::min( d, 256 - d );
		return d * 360.0 / 256.0;
	};
	std::vector<double> diffs;
	qint64 seams = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int a = dir[size_t( y ) * W + x];
			if ( a < 0 )
				continue;
			if ( x + 1 < W && dir[size_t( y ) * W + x + 1] >= 0 ) {
				const double d = adiff( a, dir[size_t( y ) * W + x + 1] );
				diffs.push_back( d );
				if ( d > 10.0 )
					seams++;
			}
			if ( y + 1 < H && dir[size_t( y + 1 ) * W + x] >= 0 ) {
				const double d = adiff( a, dir[size_t( y + 1 ) * W + x] );
				diffs.push_back( d );
				if ( d > 10.0 )
					seams++;
			}
		}
	out.pairs = qint64( diffs.size() );
	if ( !diffs.empty() ) {
		std::sort( diffs.begin(), diffs.end() );
		out.p99 = diffs[size_t( std::min( diffs.size() - 1, size_t( diffs.size() * 0.99 ) ) )];
		out.seamFraction = double( seams ) / double( diffs.size() );
	}
	// components of identical direction, 4-connected, by union-find
	std::vector<int> parent( size_t( W ) * size_t( H ), 0 );
	for ( size_t i = 0; i < parent.size(); i++ )
		parent[i] = int( i );
	std::function<int( int )> find = [&]( int i ) {
		while ( parent[size_t( i )] != i ) {
			parent[size_t( i )] = parent[size_t( parent[size_t( i )] )];
			i = parent[size_t( i )];
		}
		return i;
	};
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int i = y * W + x;
			if ( dir[size_t( i )] < 0 )
				continue;
			if ( x + 1 < W && dir[size_t( i ) + 1] == dir[size_t( i )] )
				parent[size_t( find( i ) )] = find( i + 1 );
			if ( y + 1 < H && dir[size_t( i ) + W] == dir[size_t( i )] )
				parent[size_t( find( i ) )] = find( i + W );
		}
	std::vector<int> area( parent.size(), 0 ), bnd( parent.size(), 0 ), seamy( parent.size(), 0 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int i = y * W + x;
			if ( dir[size_t( i )] < 0 )
				continue;
			const int r = find( i );
			area[size_t( r )]++;
			const int nx[4] = { x - 1, x + 1, x, x }, ny[4] = { y, y, y - 1, y + 1 };
			for ( int q = 0; q < 4; q++ ) {
				if ( nx[q] < 0 || ny[q] < 0 || nx[q] >= W || ny[q] >= H )
					continue;
				const int j = ny[q] * W + nx[q];
				if ( dir[size_t( j )] < 0 || find( j ) == r )
					continue;
				bnd[size_t( r )]++;
				if ( adiff( dir[size_t( i )], dir[size_t( j )] ) > 5.0 )
					seamy[size_t( r )]++;
			}
		}
	for ( size_t r = 0; r < area.size(); r++ ) {
		if ( area[r] < 64 || bnd[r] == 0 )
			continue;
		if ( double( seamy[r] ) / double( bnd[r] ) > 0.5 ) {
			out.patches++;
			out.patchMaxRadius = std::max( out.patchMaxRadius, std::sqrt( area[r] / kPiD ) );
		}
	}
	return out;
}

/*! The 16 directions of `tests/fixtures/flowmap_directx_4x4.png`, in image
 *  reading order (row 0 is the image's TOP row).  The PNG was written from
 *  the CONVENTION by `scratchpad/build10_20260910/make_directx_fixture.py`,
 *  not by this program's encoder, so gate X5c can fail even when the codec
 *  round-trips itself perfectly. */
const int kDirectXFixtureDirs[16] = { 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240 };

/*! Lane WATER6's gates (X1 .. X5), pre-registered in
 *  `scratchpad/lane_water6_report.md` section 0 before this function existed.
 *
 *  X1  a per-point weight of exactly 1 reproduces the unweighted solve byte
 *      for byte (the FLOOR), a ramped weight does not, and it reads faster
 *      where the weight is;
 *  X2  a ONE-POINT curve is consumed as a source (it was skipped before), and
 *      the water round it points away from it;
 *  X3  an imported raster layer is the authority where painted, the solve
 *      fills the rest, and removing the layer puts every word back;
 *  X5  the flow PNG's DirectX convention: the four cardinals by hand, the
 *      65,536-word round trip, and a CHECKED-IN test image. */
void waterWeightGates( WaterMarkDoc & doc, int river, const WaterStroke & axis,
	const std::function<void( const QString &, bool )> & check,
	const std::function<void( const QString & )> & say )
{
	QString err;
	auto hashBody = [&]( qint64 & moved, qint64 & texels ) {
		quint64 h = 1469598103934665603ull;
		moved = 0;
		texels = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16 a, quint16 n ) {
			if ( int( id ) != river )
				return;
			texels++;
			if ( a != n )
				moved++;
			h ^= n;
			h *= 1099511628211ull;
		}, &err );
		return h;
	};
	auto weighted = [&]( const WaterStroke & in, const QVector<float> & w ) {
		WaterStroke s = in;
		if ( !w.isEmpty() ) {
			QByteArray e;
			e.resize( qsizetype( w.size() ) * 4 );
			for ( int i = 0; i < w.size(); i++ ) {
				const float v = w[i];
				memcpy( e.data() + qsizetype( i ) * 4, &v, 4 );
			}
			s.extra = e;
		} else {
			s.extra.clear();
		}
		return s;
	};
	/* the mean speed nibble under the first third and the last third of the
	 * stroke -- the instrument X1b reads */
	auto thirds = [&]( double & firstThird, double & lastThird ) {
		const int n = axis.pts.size();
		const int a = std::max( 1, n / 3 );
		double s0 = 0.0, s1 = 0.0;
		qint64 c0 = 0, c1 = 0;
		for ( int i = 0; i < n; i++ ) {
			const bool early = i < a, late = i >= n - a;
			if ( !early && !late )
				continue;
			int px = 0, py = 0;
			doc.worldToTexel( double( axis.pts[i].x ), double( axis.pts[i].y ), px, py );
			const quint16 word = doc.flowWordAt( px, py );
			if ( !word )
				continue;
			const double nib = double( ( word >> 8 ) & 0xF );
			if ( early ) {
				s0 += nib;
				c0++;
			} else {
				s1 += nib;
				c1++;
			}
		}
		firstThird = c0 ? s0 / double( c0 ) : 0.0;
		lastThird = c1 ? s1 / double( c1 ) : 0.0;
	};

	say( QStringLiteral( "-- lane WATER6: weights, one-point pins, raster authority, DirectX green --" ) );

	// ---- X1: the per-point weight ------------------------------------------
	WaterMarkSolve st;
	QVector<float> ones, ramp;
	for ( int i = 0; i < axis.pts.size(); i++ ) {
		ones.append( 1.0f );
		ramp.append( float( 1.0 + 2.0 * double( i ) / double( qMax<qsizetype>( 1, axis.pts.size() - 1 ) ) ) );
	}
	doc.clearStrokes();
	doc.solve( &st, &err );
	QString msg;
	doc.addStroke( weighted( axis, QVector<float>() ), &msg );
	doc.solve( &st, &err );
	qint64 moved0 = 0, texels0 = 0;
	const quint64 hUnweighted = hashBody( moved0, texels0 );
	double u0 = 0.0, u1 = 0.0;
	thirds( u0, u1 );

	doc.clearStrokes();
	doc.addStroke( weighted( axis, ones ), &msg );
	doc.solve( &st, &err );
	qint64 moved1 = 0, texels1 = 0;
	const quint64 hOnes = hashBody( moved1, texels1 );
	check( QStringLiteral( "X1a FLOOR: a per-point weight of exactly 1 reproduces the unweighted "
		"solve word for word (hash %1 against %2 over %3 texels)" )
		.arg( hUnweighted, 16, 16, QLatin1Char( '0' ) ).arg( hOnes, 16, 16, QLatin1Char( '0' ) )
		.arg( texels1 ), hUnweighted == hOnes && texels1 > 0 );

	doc.clearStrokes();
	doc.addStroke( weighted( axis, ramp ), &msg );
	doc.solve( &st, &err );
	qint64 moved2 = 0, texels2 = 0;
	const quint64 hRamp = hashBody( moved2, texels2 );
	double w0 = 0.0, w1 = 0.0;
	thirds( w0, w1 );
	check( QStringLiteral( "X1b SIGNAL: weights ramped 1 -> 3 along the stroke change the solve "
		"(hash %1 against the unweighted %2)" ).arg( hRamp, 16, 16, QLatin1Char( '0' ) )
		.arg( hUnweighted, 16, 16, QLatin1Char( '0' ) ), hRamp != hUnweighted );
	say( QStringLiteral( "the mean speed nibble under the stroke's first third / last third: "
		"unweighted %1 / %2 (difference %3), weighted %4 / %5 (difference %6)" )
		.arg( u0, 0, 'f', 2 ).arg( u1, 0, 'f', 2 ).arg( u1 - u0, 0, 'f', 2 )
		.arg( w0, 0, 'f', 2 ).arg( w1, 0, 'f', 2 ).arg( w1 - w0, 0, 'f', 2 ) );
	check( QStringLiteral( "X1b it reads faster where the weight is: the weighted solve's "
		"last-third minus first-third nibble (%1) is greater than the unweighted one's (%2)" )
		.arg( w1 - w0, 0, 'f', 2 ).arg( u1 - u0, 0, 'f', 2 ), ( w1 - w0 ) > ( u1 - u0 ) );

	// X1c: it is LOCAL -- some texels move, not all of them
	{
		qint64 diff = 0, tot = 0;
		std::vector<quint16> before;
		doc.clearStrokes();
		doc.addStroke( weighted( axis, ones ), &msg );
		doc.solve( &st, &err );
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) == river )
				before.push_back( n );
		}, &err );
		doc.clearStrokes();
		doc.addStroke( weighted( axis, ramp ), &msg );
		doc.solve( &st, &err );
		size_t at = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			if ( at < before.size() && before[at] != n )
				diff++;
			tot++;
			at++;
		}, &err );
		const double pct = tot ? 100.0 * double( diff ) / double( tot ) : 0.0;
		check( QStringLiteral( "X1c the weight is LOCAL: %1 of %2 texels of body %3 changed "
			"(%4 percent, gate above 0 and below 100)" ).arg( diff ).arg( tot ).arg( river )
			.arg( pct, 0, 'f', 1 ), diff > 0 && diff < tot );
	}

	// ---- X2: a ONE-POINT curve is a pin -------------------------------------
	{
		WaterStroke pin;
		pin.kind = WaterStroke::Pin;
		pin.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		pin.speed = 0.5f;
		pin.width = 4096.0f;
		/* THE MID-MOST point of the centreline that is actually ON the river.
		 * The centreline is the mean position of each slice's wet texels, and on
		 * a winding reach that mean lands on the bank often enough that the
		 * harness's own stroke reports a fifth of its points outside the body.
		 * A pin on dry land is refused in words, which is correct and is not
		 * what this gate is about.
		 *
		 * CHOSEN BEFORE THE REFUTER SOLVE (lane WATER7). It depends only on the
		 * id plane, never on the flow, and X2b below needs the ring around this
		 * point measured in the no-pin state as well as the pinned one. Moving
		 * the choice up is what lets both be read from ONE solve each. */
		WaterStrokePoint p = axis.pts[axis.pts.size() / 2];
		bool wet = false;
		for ( int step = 0; step < axis.pts.size() && !wet; step++ )
			for ( int sgn = -1; sgn <= 1 && !wet; sgn += 2 ) {
				const int i = axis.pts.size() / 2 + sgn * step;
				if ( i < 0 || i >= axis.pts.size() )
					continue;
				if ( int( doc.bodyAtWorld( double( axis.pts[i].x ), double( axis.pts[i].y ) ) ) == river ) {
					p = axis.pts[i];
					wet = true;
				}
			}

		/* X2b's ring, stated once so the two sweeps cannot disagree about it:
		 * a band one texel thick at TWO PIN WIDTHS from the pin, which is
		 * outside the pin's own source disc (half a width) and inside the
		 * four widths the old disc metric used. */
		const double ringR = 2.0 * double( pin.width );
		const double ringH = 0.5 * double( pin.width ) * 0.25;   // ~1/8 width, a thin band
		auto ringKey = []( int px, int py ) -> qint64 {
			return ( qint64( px ) << 32 ) | qint64( quint32( py ) );
		};
		auto sweepRing = [&]( QHash<qint64, quint16> & into ) {
			into.clear();
			doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 n ) {
				if ( int( id ) != river || !n )
					return;
				double wx = 0.0, wy = 0.0;
				doc.texelToWorld( px, py, wx, wy );
				const double r = std::hypot( wx - double( p.x ), wy - double( p.y ) );
				if ( r < ringR - ringH || r > ringR + ringH )
					return;
				into.insert( ringKey( px, py ), n );
			}, &err );
		};

		// the refuter FIRST: with no strokes at all nothing moves
		doc.clearStrokes();
		doc.solve( &st, &err );
		qint64 movedNone = 0, texelsNone = 0;
		hashBody( movedNone, texelsNone );
		check( QStringLiteral( "X2a REFUTER: with the pin removed, 0 of body %1's texels move off "
			"the automatic word (measured %2) -- so the gate below can fail" )
			.arg( river ).arg( movedNone ), movedNone == 0 );
		QHash<qint64, quint16> ringBefore;
		sweepRing( ringBefore );

		pin.pts.append( p );
		WaterMarkSolve stp;
		const bool pinned = doc.addStroke( pin, &msg );
		check( QStringLiteral( "X2 the pin lands on the river: %1" ).arg( msg ), pinned && wet );
		doc.solve( &stp, &err );
		qint64 movedPin = 0, texelsPin = 0;
		hashBody( movedPin, texelsPin );
		say( QStringLiteral( "the one-point pin: %1" ).arg( stp.note ) );
		check( QStringLiteral( "X2a a ONE-POINT curve is consumed by the solve (%1 stroke(s) "
			"counted, %2 of %3 texels moved)" ).arg( stp.strokes ).arg( movedPin ).arg( texelsPin ),
			stp.strokes >= 1 && movedPin > 0 );

		/* X2b: IT ACTS AS A SOURCE -- THE NET FLUX THROUGH A RING.
		 *
		 * LANE WATER7, red 1 of BUILD10. The instrument this replaces compared
		 * the solved direction with the STRAIGHT-LINE radial from the pin over
		 * a DISC of four pin widths, and read 0.742 on body 2 against 0.371 on
		 * body 3 with the pin behaving identically on both: the Charles bends
		 * inside four widths, the flow follows the channel and the radial does
		 * not, so the cosine fell for a reason that has nothing to do with the
		 * pin. `lane_water6_report.md` section 6 names the replacement and why
		 * it is better -- the NET FLUX through a ring around the pin,
		 * normalised by what the pin injects, because a source's net outward
		 * flux equals its injection whatever the channel does afterwards.
		 *
		 * How that becomes a number here. Over a thin ring, sum the outward
		 * component of each wet texel's unit flow vector. Water that merely
		 * PASSES THROUGH enters on one side and leaves on the other and
		 * cancels in that sum, however the channel curves; a source does not
		 * cancel. The normaliser WATER6 asks for -- the pin's own injection --
		 * is not a quantity this store carries, so it is obtained by
		 * DIFFERENCE, which is the same conservation argument: the ring is
		 * swept once with no strokes at all and once with the pin, over
		 * exactly the same texels, and the through-flow subtracts out. The
		 * result is per-texel and dimensionless: 0 = the pin injects nothing
		 * through this ring, 1 = every texel on the ring turned to face
		 * straight out of it.
		 *
		 * The old disc number is still PRINTED beside it, so the first run
		 * that executes this can be read as a comparison of two instruments
		 * on one fixture rather than as a bare replacement. */
		QHash<qint64, quint16> ringAfter;
		sweepRing( ringAfter );
		auto outwardSum = [&]( const QHash<qint64, quint16> & ring, qint64 & counted ) -> double {
			double s = 0.0;
			counted = 0;
			for ( auto it = ring.constBegin(); it != ring.constEnd(); ++it ) {
				const int px = int( it.key() >> 32 );
				const int py = int( quint32( it.key() & 0xFFFFFFFF ) );
				double wx = 0.0, wy = 0.0;
				doc.texelToWorld( px, py, wx, wy );
				const double rx = wx - double( p.x ), ry = wy - double( p.y );
				const double r = std::hypot( rx, ry );
				if ( r < 1.0 )
					continue;
				const double ang = double( it.value() & 0xFF ) / 256.0 * kTwoPi;
				s += ( std::cos( ang ) * rx + std::sin( ang ) * ry ) / r;
				counted++;
			}
			return s;
		};
		/* Only texels the ring holds in BOTH states are counted, or the
		 * difference would also be measuring which texels carry a flow word at
		 * all. In practice the wet mask does not move, so the two sets are the
		 * same set; the intersection is taken because "in practice" is not a
		 * gate. */
		QHash<qint64, quint16> beforeCommon, afterCommon;
		for ( auto it = ringAfter.constBegin(); it != ringAfter.constEnd(); ++it )
			if ( ringBefore.contains( it.key() ) ) {
				afterCommon.insert( it.key(), it.value() );
				beforeCommon.insert( it.key(), ringBefore.value( it.key() ) );
			}
		qint64 nBefore = 0, nAfter = 0;
		const double sBefore = outwardSum( beforeCommon, nBefore );
		const double sAfter = outwardSum( afterCommon, nAfter );
		const double netBefore = nBefore ? sBefore / double( nBefore ) : 0.0;
		const double netAfter = nAfter ? sAfter / double( nAfter ) : 0.0;
		const double netDelta = netAfter - netBefore;
		say( QStringLiteral( "X2b ring at 2 pin widths (%1 world units), %2 wet texels in both "
			"states: net outward flux per texel %3 without the pin, %4 with it" )
			.arg( ringR, 0, 'f', 0 ).arg( nAfter )
			.arg( netBefore, 0, 'f', 3 ).arg( netAfter, 0, 'f', 3 ) );
		/* THE FLOOR ON THE OTHER SIDE, and it is the no-pin state itself: the
		 * same ring, the same texels, the same arithmetic, with nothing
		 * injecting. It must read below 0.2 or the metric is measuring the
		 * channel rather than the pin, and then the gate above means nothing.
		 * Registered before this was coded, with the 0.5 below. */
		check( QStringLiteral( "X2b FLOOR: with no pin the same ring's net outward flux is %1 "
			"(must be below 0.2, or the ring is reading the channel and not the pin)" )
			.arg( netBefore, 0, 'f', 3 ), nBefore > 0 && std::fabs( netBefore ) < 0.2 );
		check( QStringLiteral( "X2b it acts as a SOURCE: the pin adds %1 of net outward flux per "
			"texel through a ring at two pin widths (%2 texels; gate > 0.5; a sink would be "
			"below -0.5). Curvature cannot bias this: through-flow enters and leaves the same "
			"ring and cancels" ).arg( netDelta, 0, 'f', 3 ).arg( nAfter ),
			nAfter > 0 && netDelta > 0.5 );

		// the retired disc metric, printed only, so both can be read at once
		{
			double sum = 0.0;
			qint64 cnt = 0;
			const double reach = 4.0 * double( pin.width );
			doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 n ) {
				if ( int( id ) != river || !n )
					return;
				double wx = 0.0, wy = 0.0;
				doc.texelToWorld( px, py, wx, wy );
				const double rx = wx - double( p.x ), ry = wy - double( p.y );
				const double r = std::hypot( rx, ry );
				if ( r < 1.0 || r > reach )
					return;
				const double ang = double( n & 0xFF ) / 256.0 * kTwoPi;
				sum += ( std::cos( ang ) * rx + std::sin( ang ) * ry ) / r;
				cnt++;
			}, &err );
			say( QStringLiteral( "X2b (retired instrument, informational only) the mean cosine "
				"against the straight-line radial over a disc of four widths is %1 over %2 "
				"texels -- 0.742 on body 2 and 0.371 on body 3 was the disagreement that "
				"retired it" ).arg( cnt ? sum / double( cnt ) : 0.0, 0, 'f', 3 ).arg( cnt ) );
		}
	}

	// ---- X3: an imported raster is the authority where painted ---------------
	{
		doc.clearStrokes();
		doc.addStroke( weighted( axis, QVector<float>() ), &msg );
		doc.solve( &st, &err );
		// the body's words WITHOUT any layer: the thing X3b and X3c compare with
		std::vector<quint16> plain;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) == river )
				plain.push_back( n );
		}, &err );
		// the window round the stroke's middle point, in body-plane texels
		const WaterStrokePoint mid = axis.pts[axis.pts.size() / 2];
		int mx = 0, my = 0;
		doc.worldToTexel( double( mid.x ), double( mid.y ), mx, my );
		const int R = 24;
		WaterRasterLayer layer;
		layer.px0 = mx - R;
		layer.py0 = my - R;
		layer.w = 2 * R;
		layer.h = 2 * R;
		layer.words.fill( 0, qsizetype( layer.w ) * layer.h );
		layer.painted.fill( 0, qsizetype( layer.w ) * layer.h );
		const quint16 kMagic = quint16( 0xFF40 );   // dir 64 (north), speed 15, confidence 15
		qint64 paintedWet = 0;
		for ( int y = 0; y < layer.h; y++ )
			for ( int x = 0; x < layer.w; x++ ) {
				const qsizetype i = qsizetype( y ) * layer.w + x;
				double wx = 0.0, wy = 0.0;
				doc.texelToWorld( layer.px0 + x, layer.py0 + y, wx, wy );
				if ( int( doc.bodyAtWorld( wx, wy ) ) != river )
					continue;
				layer.words[i] = kMagic;
				layer.painted[i] = 1;
				paintedWet++;
			}
		WaterStroke rec;
		rec.kind = 10;
		rec.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		rec.extra = layer.payload();
		const bool stored = doc.addStroke( rec, &msg );
		check( QStringLiteral( "X3 the raster layer is stored (%1 painted wet texels, %2)" )
			.arg( paintedWet ).arg( msg ), stored && paintedWet > 0 );

		/* The comparison is against the LAYER-FREE SOLVE, texel by texel, and not
		 * against the layer's constant: exactly one of this river's 29,121
		 * texels outside the window solves to 0xFF40 on its own, and counting
		 * equality with a constant called that coincidence an authority leak on
		 * the first run of these gates. */
		qint64 magicIn = 0, wetIn = 0, changedOut = 0, sameOut = 0;
		size_t at = 0;
		doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			const bool inside = px >= layer.px0 && py >= layer.py0
				&& px < layer.px0 + layer.w && py < layer.py0 + layer.h;
			if ( inside ) {
				wetIn++;
				if ( n == kMagic )
					magicIn++;
			} else if ( at < plain.size() ) {
				if ( n != plain[at] )
					changedOut++;
				else
					sameOut++;
			}
			at++;
		}, &err );
		check( QStringLiteral( "X3a the raster is the AUTHORITY where painted: %1 of %2 painted "
			"texels read the layer's word" ).arg( magicIn ).arg( wetIn ),
			wetIn > 0 && magicIn == wetIn );
		check( QStringLiteral( "X3b the solve fills the REST: %1 texels outside the layer moved "
			"from the layer-free solve (gate 0) and %2 did not (floor > 0)" )
			.arg( changedOut ).arg( sameOut ), changedOut == 0 && sameOut > 0 );

		// X3c: it is an authority, not a bake -- removing it puts every word back
		for ( int i = doc.strokes().size() - 1; i >= 0; i-- )
			if ( doc.strokes()[i].kind == 10 )
				doc.removeStroke( i );
		qint64 differ = 0, same = 0;
		at = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16, quint16 n ) {
			if ( int( id ) != river )
				return;
			if ( at < plain.size() ) {
				if ( n != plain[at] )
					differ++;
				else
					same++;
			}
			at++;
		}, &err );
		check( QStringLiteral( "X3c the raster is not baked in: with the layer removed the body's "
			"words are the layer-free solve's again (%1 of %2 differ)" )
			.arg( differ ).arg( differ + same ), differ == 0 && same > 0 );
		doc.clearStrokes();
		doc.solve( &st, &err );
	}

	// ---- X5: the DirectX convention -----------------------------------------
	{
		struct Card { int dir; int r; int g; const char * name; };
		const Card cards[4] = { { 0, 255, 128, "east" }, { 64, 128, 1, "north" },
			{ 128, 1, 128, "west" }, { 192, 128, 255, "south" } };
		bool ok = true;
		QString detail;
		for ( const Card & c : cards ) {
			quint8 r = 0, g = 0, b = 0, a = 0;
			WaterCurveDoc::rgbaFromWord( quint16( c.dir ), r, g, b, a );
			if ( int( r ) != c.r || int( g ) != c.g )
				ok = false;
			detail += QStringLiteral( "%1 R%2 G%3  " ).arg( QLatin1String( c.name ) )
				.arg( int( r ) ).arg( int( g ) );
		}
		check( QStringLiteral( "X5a the DirectX cardinals (R = +X, G = +Y toward the image "
			"BOTTOM, centred on 128): %1" ).arg( detail.trimmed() ), ok );

		qint64 same = 0, total = 0;
		for ( int d = 0; d < 256; d++ )
			for ( int s = 0; s < 16; s++ )
				for ( int c = 0; c < 16; c++ ) {
					const quint16 word = quint16( d | ( s << 8 ) | ( c << 12 ) );
					quint8 r = 0, g = 0, b = 0, a = 0;
					WaterCurveDoc::rgbaFromWord( word, r, g, b, a );
					if ( !a )
						a = 1;
					total++;
					if ( WaterCurveDoc::wordFromRgba( r, g, b, a ) == word )
						same++;
				}
		check( QStringLiteral( "X5b the codec round-trips every word: %1 of %2" )
			.arg( same ).arg( total ), same == total );

		const QString fixture = QStringLiteral( "tests/fixtures/flowmap_directx_4x4.png" );
		QString found = fixture;
		if ( !QFile::exists( found ) ) {
			found = QCoreApplication::applicationDirPath() + QStringLiteral( "/../" ) + fixture;
			if ( !QFile::exists( found ) )
				found = qEnvironmentVariable( "WW_WATER_DIRECTX_FIXTURE" );
		}
		QImage img;
		const bool loaded = !found.isEmpty() && img.load( found ) && !img.isNull();
		check( QStringLiteral( "X5c the checked-in test image is readable (%1)" )
			.arg( loaded ? found : QStringLiteral( "not found: set WW_WATER_DIRECTX_FIXTURE" ) ),
			loaded && img.width() == 4 && img.height() == 4 );
		if ( loaded && img.width() == 4 && img.height() == 4 ) {
			if ( img.format() != QImage::Format_RGBA8888 )
				img = img.convertToFormat( QImage::Format_RGBA8888 );
			/* Row 0 of the IMAGE is its TOP row. The 16 texels carry the 16
			 * directions 0, 16, ... 240 in reading order, and the PNG was
			 * written from the CONVENTION by a script that shares no code with
			 * this program (see the lane report's section 0), so this gate can
			 * fail while the codec round-trips itself perfectly. */
			int wrong = 0;
			QString first;
			for ( int row = 0; row < 4; row++ )
				for ( int col = 0; col < 4; col++ ) {
					const uchar * q = img.constScanLine( row ) + qsizetype( col ) * 4;
					const quint16 w = WaterCurveDoc::wordFromRgba( q[0], q[1], q[2], q[3] );
					const int want = kDirectXFixtureDirs[row * 4 + col];
					if ( int( w & 0xFF ) != want ) {
						wrong++;
						if ( first.isEmpty() )
							first = QStringLiteral( " (first: row %1 col %2 read %3, wanted %4)" )
								.arg( row ).arg( col ).arg( int( w & 0xFF ) ).arg( want );
					}
				}
			check( QStringLiteral( "X5c the checked-in test image decodes to the documented "
				"directions: %1 of 16 wrong%2" ).arg( wrong ).arg( first ), wrong == 0 );
		}
	}
}

} // namespace

bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )
{
	QString rep;
	int checks = 0, fails = 0;
	auto check = [&]( const QString & what, bool pass ) {
		checks++;
		if ( !pass )
			fails++;
		rep += ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what
			+ QStringLiteral( "\n" );
	};
	auto say = [&]( const QString & s ) { rep += s + QStringLiteral( "\n" ); };

	WaterMarkDoc doc;
	QString err;
	if ( !doc.open( path, &err ) ) {
		if ( error )
			*error = err;
		return false;
	}
	say( QStringLiteral( "file %1  bodies %2  strokes %3  flow rate %4" )
		.arg( QFileInfo( path ).fileName() ).arg( doc.bodyCount() )
		.arg( doc.strokes().size() ).arg( doc.flowSamples() ) );

	/* ---- 1. THE TWO IDENTITY GATES -----------------------------------------
	 * Everything below rests on these: this file re-implements the writer's
	 * body-table encoder and its plane packer, and the only honest defence of a
	 * twin is that it reproduces the original's bytes on the original's own
	 * input. If either goes red, nothing after it means anything. */
	check( QStringLiteral( "the body table re-encodes to the bytes the writer wrote" ),
		doc.tableRepackMatches() );
	{
		qint64 diff = -1;
		const bool same = doc.flowRepackMatches( &diff, &err );
		check( QStringLiteral( "the flow plane re-derives to the bytes the writer wrote "
			"(%1 bytes differ)" ).arg( diff ), same );
	}
	const QByteArray beforeAll = [&]() {
		QFile f( path );
		return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray();
	}();
	check( QStringLiteral( "the file was read whole for the undo gate (%1 bytes)" )
		.arg( beforeAll.size() ), beforeAll.size() > 0 );

	/* ---- 1b. THE FLOW GATES (lane WATER4) -----------------------------------
	 * Synthetic masks through the solver core alone, before any body of this
	 * file is touched: a red gate here is the METHOD, not the file. */
	if ( qEnvironmentVariableIsSet( "WW_WATER_FLOW_TEST" ) || true ) {
		say( QStringLiteral( "-- the flow gates on synthetic masks --" ) );
		waterFlowGates( check, say );
	}

	/* ---- 2. pick the biggest river and its biggest neighbour --------------
	 * Nothing here is hard-coded to the Commonwealth: the case is "the largest
	 * river" and "the largest OTHER body of at least a quarter its area", so
	 * the same harness runs on any worldspace. On the Commonwealth those are
	 * body 3 (the Charles) and body 2 (the marsh). */
	int river = 0, neighbour = 0;
	quint32 bestArea = 0, bestOther = 0;
	for ( int i = 1; i <= doc.bodyCount(); i++ ) {
		LodtWaterBody b;
		doc.body( i, b );
		if ( b.cls == 1 && b.area > bestArea ) {
			bestArea = b.area;
			river = i;
		}
	}
	for ( int i = 1; i <= doc.bodyCount(); i++ ) {
		LodtWaterBody b;
		doc.body( i, b );
		if ( i != river && b.cls != 0 && b.area > bestOther ) {
			bestOther = b.area;
			neighbour = i;
		}
	}
	/* WW_WATER_MARK_BODY=<id> marks a NAMED body instead of the largest
	 * river.  The case itself stays "the largest river", because that is
	 * what makes this harness run on any worldspace; this exists so a
	 * before/after pair can be taken at a framing somebody already has --
	 * the Charles is body 3 on the Commonwealth and lane WATER2's flow
	 * renders are cut to its cells.  The neighbour is re-picked so it can
	 * never be the body under test. */
	{
		const QByteArray pin = qgetenv( "WW_WATER_MARK_BODY" );
		bool okPin = false;
		const int want = pin.toInt( &okPin );
		LodtWaterBody pb;
		if ( okPin && want >= 1 && doc.body( want, pb ) && pb.area > 0 ) {
			river = want;
			neighbour = 0;
			bestOther = 0;
			for ( int i = 1; i <= doc.bodyCount(); i++ ) {
				LodtWaterBody b;
				doc.body( i, b );
				if ( i != river && b.cls != 0 && b.area > bestOther ) {
					bestOther = b.area;
					neighbour = i;
				}
			}
			say( QStringLiteral( "WW_WATER_MARK_BODY=%1: marking that body instead of the "
				"largest river" ).arg( river ) );
		}
	}
	check( QStringLiteral( "the file offers a river and a second body to mark" ),
		river > 0 && neighbour > 0 && river != neighbour );
	if ( !river || !neighbour ) {
		if ( text )
			*text = rep;
		if ( error )
			*error = QStringLiteral( "no river to mark" );
		return false;
	}
	LodtWaterBody rb, nb;
	doc.body( river, rb );
	doc.body( neighbour, nb );
	say( QStringLiteral( "river  = body %1, %2 texels, cells (%3..%4, %5..%6)" )
		.arg( river ).arg( rb.area ).arg( rb.x0 ).arg( rb.x1 ).arg( rb.y0 ).arg( rb.y1 ) );
	say( QStringLiteral( "neighbour = body %1, %2 texels" ).arg( neighbour ).arg( nb.area ) );

	// ---- 3. a stroke down the river's own axis -----------------------------
	auto axisStroke = [&]( const LodtWaterBody & b ) {
		/* THE BODY'S OWN CENTRELINE, not its bounding-box diagonal.
		 *
		 * The first version of this walked the bbox diagonal and kept every
		 * point that was on ANY water: 13 of its 16 points were not on the
		 * river at all, and a bbox diagonal is within a degree of the body's
		 * own mean direction (55.5 against 54.84 on the Charles), so the
		 * stroke asked the plane for what it already said.  This slices the
		 * body across its LONGER side and takes the mean position of its own
		 * texels in each slice, which follows a winding reach. */
		WaterStroke s;
		s.kind = WaterStroke::Stroke;
		s.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		s.speed = 0.5f;
		s.width = 4096.0f;
		int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
		doc.worldToTexel( double( b.x0 ) * kCellUnits, double( b.y0 ) * kCellUnits, px0, py0 );
		doc.worldToTexel( ( double( b.x1 ) + 1.0 ) * kCellUnits,
			( double( b.y1 ) + 1.0 ) * kCellUnits, px1, py1 );
		const bool alongY = ( py1 - py0 ) >= ( px1 - px0 );
		const int a0 = alongY ? py0 : px0, a1 = alongY ? py1 : px1;
		const int b0 = alongY ? px0 : py0, b1 = alongY ? px1 : py1;
		for ( int a = a0; a <= a1; a++ ) {
			double sx = 0.0, sy = 0.0;
			qint64 n = 0;
			for ( int c = b0; c <= b1; c++ ) {
				const int px = alongY ? c : a, py = alongY ? a : c;
				double wx = 0.0, wy = 0.0;
				doc.texelToWorld( px, py, wx, wy );
				if ( doc.bodyAtWorld( wx, wy ) != b.id )
					continue;
				sx += wx;
				sy += wy;
				n++;
			}
			if ( !n )
				continue;
			WaterStrokePoint p;
			p.x = float( sx / double( n ) );
			p.y = float( sy / double( n ) );
			if ( !s.pts.isEmpty() ) {
				// one point a cell is plenty; the solve interpolates the segment
				const double dx = double( p.x ) - double( s.pts.last().x );
				const double dy = double( p.y ) - double( s.pts.last().y );
				if ( dx * dx + dy * dy < kCellUnits * kCellUnits * 0.25 )
					continue;
			}
			s.pts.append( p );
		}
		return s;
	};

	// ---- 4. THE REFUTER, run FIRST -----------------------------------------
	{
		WaterStroke s = axisStroke( nb );
		QString msg;
		const bool added = doc.addStroke( s, &msg );
		check( QStringLiteral( "the refuter's stroke lands on the neighbour: %1" ).arg( msg ),
			added && !doc.strokesOfBody( neighbour ).isEmpty() );
		WaterMarkSolve st;
		doc.solve( &st, &err );
		qint64 movedRiver = 0, movedOther = 0;
		doc.sweep( [&]( int, int, quint16 id, quint16 a, quint16 n ) {
			if ( a == n )
				return;
			if ( int( id ) == river )
				movedRiver++;
			else
				movedOther++;
		}, &err );
		check( QStringLiteral( "RED FIRST: a stroke on body %1 moves %2 of its own texels" )
			.arg( neighbour ).arg( movedOther ), movedOther > 0 );
		check( QStringLiteral( "RED FIRST: and moves 0 texels of body %1 (measured %2) -- so the "
			"isolation check below can fail" ).arg( river ).arg( movedRiver ), movedRiver == 0 );
		doc.clearStrokes();
		doc.solve( &st, &err );
	}

	/* ---- 5. ONE STROKE SENDS THE RIVER TO THE SEA -------------------------
	 * bungo's sentence, as a measurement: *"rivers end up at sea"*. The mouth
	 * is found from the file -- the river texel nearest a texel of the body it
	 * drains into -- the stroke is oriented to END there, and the gate is that
	 * the FLOW PLANE's mean direction over the whole body points at the mouth.
	 * Both the before and after angles are printed, so a reader can see which
	 * way it pointed before anybody marked it. */
	double mouthX = 0.0, mouthY = 0.0, cenX = 0.0, cenY = 0.0;
	bool haveMouth = false;
	{
		const int outlet = rb.outlet ? int( rb.outlet ) : 1;   // the sea, when nothing else
		qint64 nRiver = 0;
		double best = 1e30;
		const int pad = 8;
		int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
		doc.worldToTexel( ( double( rb.x0 ) ) * kCellUnits, ( double( rb.y0 ) ) * kCellUnits, px0, py0 );
		doc.worldToTexel( ( double( rb.x1 ) + 1.0 ) * kCellUnits,
			( double( rb.y1 ) + 1.0 ) * kCellUnits, px1, py1 );
		// centroid of the river, and the nearest outlet texel to it
		for ( int py = py0; py <= py1; py++ ) {
			for ( int px = px0; px <= px1; px++ ) {
				double wx = 0, wy = 0;
				doc.texelToWorld( px, py, wx, wy );
				if ( doc.bodyAtWorld( wx, wy ) != quint16( river ) )
					continue;
				nRiver++;
				cenX += wx;
				cenY += wy;
			}
		}
		if ( nRiver ) {
			cenX /= double( nRiver );
			cenY /= double( nRiver );
		}
		for ( int py = py0 - pad; py <= py1 + pad; py++ ) {
			for ( int px = px0 - pad; px <= px1 + pad; px++ ) {
				double wx = 0, wy = 0;
				doc.texelToWorld( px, py, wx, wy );
				if ( int( doc.bodyAtWorld( wx, wy ) ) != outlet )
					continue;
				const double d = ( wx - cenX ) * ( wx - cenX ) + ( wy - cenY ) * ( wy - cenY );
				if ( d < best ) {
					best = d;
					mouthX = wx;
					mouthY = wy;
					haveMouth = true;
				}
			}
		}
		say( QStringLiteral( "the river drains into body %1; its mouth is at (%2, %3) and its "
			"centroid at (%4, %5)" ).arg( outlet ).arg( mouthX, 0, 'f', 0 ).arg( mouthY, 0, 'f', 0 )
			.arg( cenX, 0, 'f', 0 ).arg( cenY, 0, 'f', 0 ) );
		check( QStringLiteral( "a mouth was found in the file, not assumed" ), haveMouth );
	}
	{
		WaterStroke s = axisStroke( rb );
		if ( haveMouth && s.pts.size() >= 2 ) {
			const double dA = std::hypot( double( s.pts.first().x ) - mouthX,
				double( s.pts.first().y ) - mouthY );
			const double dB = std::hypot( double( s.pts.last().x ) - mouthX,
				double( s.pts.last().y ) - mouthY );
			if ( dA < dB ) {
				QVector<WaterStrokePoint> r;
				for ( int i = s.pts.size() - 1; i >= 0; i-- )
					r.append( s.pts[i] );
				s.pts = r;
			}
		}
		QString msg;
		const bool added = doc.addStroke( s, &msg );
		check( QStringLiteral( "the stroke lands on the river: %1" ).arg( msg ), added );
		WaterMarkSolve st;
		doc.solve( &st, &err );
		say( QStringLiteral( "solve: %1" ).arg( st.note ) );
		qint64 inside = 0, outside = 0, wetRiver = 0;
		double sx = 0.0, sy = 0.0, bx = 0.0, by = 0.0;
		doc.sweep( [&]( int, int, quint16 id, quint16 a, quint16 n ) {
			if ( int( id ) == river ) {
				wetRiver++;
				const double ang = double( n & 0xFF ) / 256.0 * kTwoPi;
				sx += std::cos( ang );
				sy += std::sin( ang );
				const double ang0 = double( a & 0xFF ) / 256.0 * kTwoPi;
				bx += std::cos( ang0 );
				by += std::sin( ang0 );
			}
			if ( a == n )
				return;
			if ( int( id ) == river )
				inside++;
			else
				outside++;
		}, &err );
		check( QStringLiteral( "P1 isolation: 0 texels outside body %1 changed (measured %2)" )
			.arg( river ).arg( outside ), outside == 0 );
		const double frac = wetRiver ? double( inside ) / double( wetRiver ) : 0.0;
		check( QStringLiteral( "P1 floor: at least 60 per cent of body %1's own texels changed "
			"(measured %2 of %3 = %4 per cent)" ).arg( river ).arg( inside ).arg( wetRiver )
			.arg( frac * 100.0, 0, 'f', 1 ), frac >= 0.60 );
		LodtWaterBody after;
		doc.body( river, after );
		check( QStringLiteral( "the body's flow source moved %1 -> 4 (a stroke)" )
			.arg( rb.flowSource ), after.flowSource == 4 );
		const double deg = 180.0 / 3.14159265358979;
		const double meanAng = std::atan2( sy, sx ) * deg;
		const double beforeAng = std::atan2( by, bx ) * deg;
		say( QStringLiteral( "the flow plane's MEAN direction over body %1: %2 degrees before, "
			"%3 degrees after" ).arg( river ).arg( beforeAng, 0, 'f', 2 ).arg( meanAng, 0, 'f', 2 ) );
		if ( haveMouth ) {
			const double tx = mouthX - cenX, ty = mouthY - cenY;
			const double tm = std::hypot( tx, ty );
			const double sm = std::hypot( sx, sy );
			const double dot = tm > 0 && sm > 0 ? ( tx * sx + ty * sy ) / ( tm * sm ) : 0.0;
			const double toMouth = std::atan2( ty, tx ) * deg;
			say( QStringLiteral( "the direction from the river's centroid to its mouth is %1 "
				"degrees; the plane's mean agrees to %2 degrees" ).arg( toMouth, 0, 'f', 2 )
				.arg( std::acos( qBound( -1.0, dot, 1.0 ) ) * deg, 0, 'f', 2 ) );
			check( QStringLiteral( "\"rivers end up at sea\": the marked plane's mean direction "
				"points at the mouth (cos = %1, needs > 0)" ).arg( dot, 0, 'f', 3 ), dot > 0.0 );
			check( QStringLiteral( "F5 the mean direction still points at the mouth (cos = %1, needs "
				"> 0.9; R = %2)" ).arg( dot, 0, 'f', 3 ).arg( sm / std::max( 1.0, double( wetRiver ) ), 0, 'f', 3 ),
				dot > 0.9 );
		}
	}

	/* ---- 5a. F5 and F8 (lane WATER4): no held discs, and the cost --------
	 * The instrument is the pre-registered one: seam-bounded constant-
	 * direction patches of >= 64 texels, the 99th percentile of the angle
	 * difference between adjacent wet texels, and the seam fraction -- the
	 * same numbers scratchpad/water4_20260910/disc_metric.py reads back out
	 * of the file, where the harmonic fill measured 39 patches, p99 40.78
	 * degrees and 2.97 percent on this same body and stroke. */
	{
		const FlowStructure fs = flowStructure( doc, rb );
		say( QStringLiteral( "F5 structure over body %1: %2 wet texels, %3 adjacent pairs, p99 %4 "
			"degrees, seam fraction %5 percent, %6 seam-bounded patches (largest r_eq %7 texels)" )
			.arg( river ).arg( fs.wet ).arg( fs.pairs ).arg( fs.p99, 0, 'f', 2 )
			.arg( fs.seamFraction * 100.0, 0, 'f', 3 ).arg( fs.patches )
			.arg( fs.patchMaxRadius, 0, 'f', 1 ) );
		check( QStringLiteral( "F5 no seam-bounded constant-direction patches (measured %1; the "
			"disc fill had 39)" ).arg( fs.patches ), fs.wet > 0 && fs.patches == 0 );
		check( QStringLiteral( "F5 the 99th percentile of the adjacent angle difference is below 5 "
			"degrees (%1; the disc fill had 40.78)" ).arg( fs.p99, 0, 'f', 2 ), fs.pairs > 0 && fs.p99 < 5.0 );
		check( QStringLiteral( "F5 seams are below 0.5 percent of adjacent pairs (%1 percent; the "
			"disc fill had 2.97)" ).arg( fs.seamFraction * 100.0, 0, 'f', 3 ),
			fs.pairs > 0 && fs.seamFraction < 0.005 );
		WaterMarkSolve st2;
		doc.solve( &st2, &err );
		say( QStringLiteral( "F8 the solve: %1 s, %2 iterations, residual %3, stroke agreement %4" )
			.arg( st2.solveSeconds, 0, 'f', 3 ).arg( st2.iterations ).arg( st2.residual, 0, 'g', 3 )
			.arg( st2.strokeAgreement, 0, 'f', 3 ) );
		check( QStringLiteral( "F8 the solve of body %1 runs under 1.0 s with a residual below 1e-8 "
			"(%2 s, %3)" ).arg( river ).arg( st2.solveSeconds, 0, 'f', 3 ).arg( st2.residual, 0, 'g', 3 ),
			st2.solveSeconds < 1.0 && st2.residual < 1e-8 );
		check( QStringLiteral( "the stroke and the solved flow under it agree (mean cosine %1, "
			"needs > 0.5)" ).arg( st2.strokeAgreement, 0, 'f', 3 ), st2.strokeAgreement > 0.5 );
	}

	/* ---- 5b. P4: the stroke survives a re-derivation at another rate ------
	 * The strokes are WORLD coordinates, so the plane may be written at any
	 * rate the file's samples per cell divides. Measured as a CONSUMER sees
	 * it: the file is saved at each rate and the mean direction is read back
	 * OUT OF THE FILE. The file's own rate is restored afterwards, because
	 * the round-trip and undo gates below compare against the writer's
	 * bytes. */
	{
		const int rate0 = doc.flowSamples();
		double a0 = 0.0, a1 = 0.0, w0 = 0.0, w1 = 0.0;
		qint64 n0 = 0, n1 = 0, m0 = 0, m1 = 0;
		int rateBack = 0;
		bool got = doc.save( &err );
		if ( got ) {
			WaterMarkDoc d0;
			got = d0.open( path, &err )
				&& d0.meanFileFlow( river, true, a0, n0, &err );
			if ( got )
				d0.meanFileFlow( river, false, w0, m0, &err );
		}
		if ( got )
			got = doc.setFlowRate( 8, &err );
		if ( got ) {
			WaterMarkSolve st8;
			doc.solve( &st8, &err );
			got = doc.save( &err );
		}
		if ( got ) {
			WaterMarkDoc d1;
			got = d1.open( path, &err )
				&& d1.meanFileFlow( river, true, a1, n1, &err );
			if ( got ) {
				d1.meanFileFlow( river, false, w1, m1, &err );
				rateBack = d1.flowSamples();
			}
		}
		say( QStringLiteral( "P4: body %1 carries %2 of %3 flow samples at %4, and %5 of "
			"%6 at 8; the whole-body mean is %7 then %8 degrees" ).arg( river ).arg( n0 )
			.arg( m0 ).arg( rate0 ).arg( n1 ).arg( m1 ).arg( w0, 0, 'f', 2 )
			.arg( w1, 0, 'f', 2 ) );
		/* The floor, on the other side of the gate: the re-bake has to have
		 * HAPPENED. A plane still written at rate0, or one with no samples of
		 * this body in it, would agree with itself perfectly. */
		check( QStringLiteral( "P4 floor: the file was really re-written at 8 samples a "
			"cell (header says %1, was %2) and body %3 still has samples in it "
			"(%4 at %2, %5 at 8)" ).arg( rateBack ).arg( rate0 ).arg( river )
			.arg( n0 ).arg( n1 ),
			got && rateBack == 8 && rate0 != 8 && n0 > 0 && n1 > 0 );
		double moved = 999.0;
		if ( got ) {
			moved = std::fabs( a1 - a0 );
			if ( moved > 180.0 )
				moved = 360.0 - moved;
		}
		check( QStringLiteral( "P4 re-bake: body %1 marked at %2 samples a cell and "
			"re-written at 8 still points the same way (mean %3 -> %4 degrees, moved "
			"%5, tolerance 5)" ).arg( river ).arg( rate0 ).arg( a0, 0, 'f', 2 )
			.arg( a1, 0, 'f', 2 ).arg( moved, 0, 'f', 2 ), got && moved <= 5.0 );
		QString e2;
		if ( !doc.setFlowRate( rate0, &e2 ) )
			say( QStringLiteral( "could not restore the file's own rate: %1" ).arg( e2 ) );
		WaterMarkSolve stBack;
		doc.solve( &stBack, &e2 );
	}

	double wbx0 = 0.0, wby0 = 0.0, wbx1 = 0.0, wby1 = 0.0;
	doc.worldBounds( wbx0, wby0, wbx1, wby1 );
	// ---- 6. a stroke on dry land is refused in words ------------------------
	{
		/* The dry point is FOUND IN THE FILE.  The worldspace corner this case
		 * used to assume was dry is open sea on the Commonwealth (body 1), so
		 * the stroke was rightly accepted and the control read as a failure of
		 * the refusal it exists to prove. */
		double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		const bool haveDry = doc.dryStroke( x0, y0, x1, y1 );
		say( QStringLiteral( "the dry point is at (%1, %2) -> (%3, %4), and body %5 sits "
			"at the worldspace corner" ).arg( x0, 0, 'f', 0 ).arg( y0, 0, 'f', 0 )
			.arg( x1, 0, 'f', 0 ).arg( y1, 0, 'f', 0 )
			.arg( doc.bodyAtWorld( wbx0 + 8.0, wby0 + 8.0 ) ) );
		check( QStringLiteral( "a dry point was found in the file, not assumed" ), haveDry );
		WaterStroke s;
		s.kind = WaterStroke::Stroke;
		WaterStrokePoint a, b;
		a.x = float( x0 );
		a.y = float( y0 );
		b.x = float( x1 );
		b.y = float( y1 );
		s.pts << a << b;
		const int before = doc.strokes().size();
		QString msg;
		const bool ok = doc.addStroke( s, &msg );
		check( QStringLiteral( "a stroke on dry land is refused, in words: \"%1\"" ).arg( msg ),
			!ok && !msg.isEmpty() && doc.strokes().size() == before );
	}

	/* ---- 6b. DYE (lane WATER4) ----------------------------------------------
	 * bungo: "river flowing into an ocean and the river and the ocean may have
	 * slightly different color".  The river's water is carried past its mouth
	 * into the body it drains into as a plume that fades; the plane is written
	 * only while a dye mark exists, which is what lets section 7's undo gate
	 * stay byte-identical with the mark removed. */
	{
		doc.setBodyDyeMouth( river, true, 1.0f );
		WaterMarkSolve st;
		doc.solve( &st, &err );
		say( QStringLiteral( "dye solve: %1" ).arg( st.note ) );
		check( QStringLiteral( "the river's dye reaches the body it drains into (%1 texels on %2 "
			"receiving field(s))" ).arg( st.dyeTexels ).arg( st.dyeBodies ), st.dyeTexels > 0 );
		const double Lt = doc.dyeHalfDistance() / doc.worldPerTexel();
		int mpx = 0, mpy = 0;
		doc.worldToTexel( mouthX, mouthY, mpx, mpy );
		const int reach = int( 5.0 * Lt ) + 2;
		double nearSum = 0.0;
		qint64 nearN = 0, dyed = 0, wrongSource = 0;
		int farMax = 0;
		for ( int py = mpy - reach; py <= mpy + reach; py++ )
			for ( int px = mpx - reach; px <= mpx + reach; px++ ) {
				const quint32 wd = doc.dyeWordAt( px, py );
				if ( !wd )
					continue;
				dyed++;
				if ( int( wd & 0xFFFF ) != river )
					wrongSource++;
				const int wgt = int( ( wd >> 16 ) & 0xFF );
				const double d = std::hypot( double( px - mpx ), double( py - mpy ) );
				if ( d < 0.5 * Lt ) {
					nearSum += wgt;
					nearN++;
				}
				if ( d > 3.0 * Lt )
					farMax = std::max( farMax, wgt );
			}
		say( QStringLiteral( "dye near the mouth at (%1, %2): %3 dyed texels within %4 texels, mean "
			"weight %5 within L/2 (%6 texels), max %7 beyond 3 L" ).arg( mpx ).arg( mpy ).arg( dyed )
			.arg( reach ).arg( nearN ? nearSum / nearN : 0.0, 0, 'f', 1 ).arg( nearN ).arg( farMax ) );
		check( QStringLiteral( "near the mouth the weight is above 128 (mean %1 over %2 texels)" )
			.arg( nearN ? nearSum / nearN : 0.0, 0, 'f', 1 ).arg( nearN ), nearN > 0 && nearSum / nearN > 128.0 );
		check( QStringLiteral( "beyond three half-distances the weight has fallen to 1/8 or less "
			"(max %1, 1/8 of 255 = 32, slack to 48)" ).arg( farMax ), dyed > 0 && farMax <= 48 );
		check( QStringLiteral( "every dyed texel names the river as its source (%1 of %2 do not)" )
			.arg( wrongSource ).arg( dyed ), dyed > 0 && wrongSource == 0 );
		if ( !doc.save( &err ) ) {
			check( QStringLiteral( "the dyed file saves: %1" ).arg( err ), false );
		} else {
			LodtFile lf;
			QString lerr;
			const bool opened = lf.open( path, &lerr );
			check( QStringLiteral( "the dyed file re-opens through the .lodl reader with the dye "
				"section (bit %1, %2 samples a cell = the flow rate %3): %4" )
				.arg( ( lf.sectionFlags() & LODL_SECT_DYE ) ? 1 : 0 ).arg( lf.dyePlaneSamples() )
				.arg( doc.flowSamples() ).arg( lerr ),
				opened && ( lf.sectionFlags() & LODL_SECT_DYE ) && lf.dyePlaneSamples() == doc.flowSamples() );
			int agree = 0, total = 0;
			const int bodyS = lf.bodyIdSamples() > 0 ? lf.bodyIdSamples() : 1;
			const int dyeS = lf.dyePlaneSamples() > 0 ? lf.dyePlaneSamples() : 1;
			for ( int py = mpy - reach; py <= mpy + reach; py += 7 )
				for ( int px = mpx - reach; px <= mpx + reach; px += 7 ) {
					const quint32 ours = doc.dyeWordAt( px, py );
					const quint32 theirs = lf.dyeWordAt( px * dyeS / bodyS, py * dyeS / bodyS );
					total++;
					if ( ours == theirs )
						agree++;
				}
			check( QStringLiteral( "the dye words read back out of the file agree with the document "
				"(%1 of %2 sampled texels)" ).arg( agree ).arg( total ), total > 0 && agree == total );
		}
	}

	/* ---- 7. save, reopen, save again, and undo ----------------------------
	 * THIS REWRITES THE FILE IT WAS GIVEN, which is why the harness runs it on
	 * a copy and says so. Three properties, in the order they can fail:
	 *   the strokes come back out of the file;
	 *   saving the re-opened document reproduces the same bytes;
	 *   removing the stroke and saving reproduces the file it started from.
	 * The last is why the flow plane had to be a pure function of the id plane
	 * and the table: with the stroke gone there is nothing left to remember. */
	{
		const int wrote = doc.strokes().size();
		if ( !doc.save( &err ) ) {
			check( QStringLiteral( "the marked file saves: %1" ).arg( err ), false );
		} else {
			check( QStringLiteral( "the marked file saves and re-opens with its %1 stroke(s)" )
				.arg( wrote ), doc.strokes().size() == wrote );
			QFile f1( path );
			const QByteArray once = f1.open( QIODevice::ReadOnly )
				? f1.readAll() : QByteArray();
			f1.close();
			check( QStringLiteral( "the marked file differs from the unmarked one (%1 vs %2 bytes)" )
				.arg( once.size() ).arg( beforeAll.size() ), once != beforeAll );
			if ( !doc.save( &err ) ) {
				check( QStringLiteral( "the re-opened document saves again: %1" ).arg( err ), false );
			} else {
				QFile f2( path );
				const QByteArray twice = f2.open( QIODevice::ReadOnly )
					? f2.readAll() : QByteArray();
				f2.close();
				check( QStringLiteral( "P8 round trip: save, reopen, save is byte-identical "
					"(%1 vs %2 bytes)" ).arg( once.size() ).arg( twice.size() ), once == twice );
			}
			doc.clearStrokes();
			WaterMarkSolve st;
			doc.solve( &st, &err );
			if ( !doc.save( &err ) ) {
				check( QStringLiteral( "the undone document saves: %1" ).arg( err ), false );
			} else {
				QFile f3( path );
				const QByteArray undone = f3.open( QIODevice::ReadOnly )
					? f3.readAll() : QByteArray();
				f3.close();
				qint64 diff = qAbs( qint64( undone.size() ) - qint64( beforeAll.size() ) );
				const qint64 common = qMin( qint64( undone.size() ), qint64( beforeAll.size() ) );
				for ( qint64 i = 0; i < common; i++ )
					if ( undone.at( int( i ) ) != beforeAll.at( int( i ) ) )
						diff++;
				check( QStringLiteral( "P3 undo: removing the stroke reproduces the file it "
					"started from, byte for byte (%1 bytes differ)" ).arg( diff ), diff == 0 );
			}
		}
	}

	/* ---- lane WATER6 -------------------------------------------------------
	 * LAST, because these gates solve the river six times and clear the
	 * strokes between; they never save, so the file the P3 undo gate has just
	 * reproduced byte for byte is left exactly as it is. */
	{
		WaterStroke axis = axisStroke( rb );
		if ( axis.pts.size() >= 6 )
			waterWeightGates( doc, river, axis, check, say );
		else
			say( QStringLiteral( "lane WATER6's gates need a centreline of at least six "
				"points; body %1 gave %2, so X1..X5 did not run" ).arg( river ).arg( axis.pts.size() ) );
	}

	if ( text )
		*text = rep + QStringLiteral( "%1 checks, %2 failures\n%3\n" )
			.arg( checks ).arg( fails )
			.arg( fails ? QStringLiteral( "water mark selftest FAIL" )
				: QStringLiteral( "water mark selftest PASS" ) );
	return fails == 0;
}
