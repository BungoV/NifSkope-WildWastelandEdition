/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodifile.h"
#include "lodofile.h"
#include "io/lodvfile.h"

#include <QDir>
#include <QSet>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <tuple>
#include <unordered_map>

/* The 256-byte header (docs/LODGEN_NATIVE_LODO_LODI.md 4). */
namespace
{

constexpr int H_MAGIC = 0x00, H_VERSION = 0x04, H_FLAGS = 0x08, H_HCRC = 0x0C;
constexpr int H_PLUGIN = 0x10, H_OBJECT = 0x18, H_IDENTITY = 0x20;
constexpr int H_EDID = 0x28, H_EDID_BYTES = 32;
constexpr int H_WEST = 0x48, H_SOUTH = 0x4A, H_EAST = 0x4C, H_NORTH = 0x4E;
constexpr int H_CELLS = 0x50, H_STRIDE = 0x52, H_CHUNKS = 0x54, H_INSTANCES = 0x58;
constexpr int H_PRESENT = 0x5C, H_MAXINST = 0x60, H_ICRC = 0x64;
constexpr int H_OFF_CHUNKS = 0x68, H_OFF_CELLS = 0x70, H_OFF_INST = 0x78, H_OFF_COLD = 0x80;
constexpr int H_FILEBYTES = 0x88;
//! v2: the load-order hash took the first reserved word; the pad started at 0x98.
constexpr int H_LOADORDER = 0x90;
/*! v3 took the room the v2 contract's section 12 named for the occluders:
 *  0x98 the box table's offset, 0xA0 the per-cell range blob's, 0xA8 the box
 *  count, 0xAC the stride, 0xAE the per-cell cap. The pad now starts at 0xB0. */
constexpr int H_OFF_OCC = 0x98, H_OFF_OCCRANGE = 0xA0, H_OCCCOUNT = 0xA8;
constexpr int H_OCCSTRIDE = 0xAC, H_OCCPERCELL = 0xAE, H_RESERVED_B0 = 0xB0;
/*! v4 took the room v3's section 12 named: 0xB0 the aggregate table's offset,
 *  0xB8 the covered-instance blob's, 0xC0 the aggregate count, 0xC4 the covered
 *  count, 0xC8 the stride, 0xCA the azimuths a sheet, 0xCC the switch threshold
 *  in reference pixels, 0xD0 the band ratio. The pad then starts at 0xD4 -- but
 *  ONLY on a version-4 file. A version-3 file's pad still starts at 0xB0 and
 *  every one of these words is zero in it, which is what keeps the module's off
 *  value byte-identical to the bake before this lane. */
constexpr int H_OFF_AGG = 0xB0, H_OFF_COVERED = 0xB8, H_AGGCOUNT = 0xC0;
constexpr int H_COVCOUNT = 0xC4, H_AGGSTRIDE = 0xC8, H_AGGVIEWS = 0xCA;
constexpr int H_AGGSWITCH = 0xCC, H_AGGBAND = 0xD0, H_RESERVED_D4 = 0xD4;
/*! v5 (lane NATIVE1c) takes the 44 bytes section 12 left at 0xD4..0xFF: four
 *  u32 per-MNAM-slot instance totals at 0xD4, then the placement-AO blob's
 *  offset, count and stride. The pad now starts at 0xF1. */
constexpr int H_SLOTINST = 0xD4, H_OFF_PAO = 0xE4, H_PAOCOUNT = 0xEC;
constexpr int H_PAOSTRIDE = 0xF0, H_RESERVED_F1 = 0xF1;
//! v6 (2026-09-18): the vertex-AO blob's offset and size; the pad is then 0xF1..0xF3 only.
constexpr int H_OFF_VAO = 0xF4, H_VAOBYTES = 0xFC, H_RESERVED_F4 = 0xF4;
/*! v7 (2026-09-18, lane LODIV7): the 256-byte header was FULL -- 0xF1..0xF3 was
 *  all that was left of it, three bytes where v7 needs twenty-four. So a v7
 *  file's header BLOCK is 512 bytes and the new words live in the second half:
 *  0x100 the group table's offset, 0x108 its count, 0x10C its stride, then
 *  0x110 the vertex-sky stream's offset and 0x118 its size. The pad is then
 *  0x11C..0x1FF. This costs no file bytes and moves no payload -- the first
 *  payload is 4,096-aligned, so 0x100..0xFFF was already zero pad in every
 *  .lodi ever written -- and on a v3..v6 file the header crc window is still
 *  exactly the 256 bytes it always was. */
constexpr int H_OFF_GROUP = 0x100, H_GROUPCOUNT = 0x108, H_GROUPSTRIDE = 0x10C;
constexpr int H_OFF_VSKY = 0x110, H_VSKYBYTES = 0x118, H_RESERVED_11C = 0x11C;
/*! v8 (2026-09-18, lane HORIZON1): the per-vertex HORIZON stream, s4.11. Its
 *  five words live in the room v7's own contract left reserved -- 0x11C the
 *  stream's offset, 0x124 its size, 0x128 the azimuths (= bytes a vertex),
 *  0x12A the far-march steps a azimuth as cast, 0x12C the march reach in world
 *  units. The pad is then 0x130..0x1FF. The header BLOCK stays 512 bytes and
 *  no payload moves, because the stream is written after everything else; so
 *  `--lodi-v7` -- the module off -- writes the version-7 file byte for byte. */
constexpr int H_OFF_VHOR = 0x11C, H_VHORBYTES = 0x124, H_HORAZ = 0x128;
constexpr int H_HORSTEPS = 0x12A, H_HORREACH = 0x12C, H_RESERVED_130 = 0x130;
constexpr float SQRT_HALF = 0.70710678118654752f;

//! Bit pattern of a float, for the header's two f32 words.
quint32 f32Bits( float v )
{
	quint32 u = 0;
	std::memcpy( &u, &v, 4 );
	return u;
}
float f32Of( quint32 u )
{
	float v = 0.0f;
	std::memcpy( &v, &u, 4 );
	return v;
}

template <typename T> void putLE( QByteArray & b, int off, T v )
{
	for ( size_t i = 0; i < sizeof( T ); i++ )
		b[off + int( i )] = char( ( quint64( v ) >> ( 8 * i ) ) & 0xFF );
}
template <typename T> T getLE( const unsigned char * p )
{
	quint64 v = 0;
	for ( size_t i = 0; i < sizeof( T ); i++ )
		v |= quint64( p[i] ) << ( 8 * i );
	return T( v );
}
quint64 alignUp( quint64 v, quint64 a )
{
	return ( v + a - 1 ) / a * a;
}

QString nameOtherMagic( quint32 m )
{
	if ( m == LODO_MAGIC ) return QStringLiteral( "a .lodo object library (LODO)" );
	if ( m == LODTEX_MAGIC ) return QStringLiteral( "a .lodt terrain texture level (LDTX)" );
	if ( m == 0x54444F4CU ) return QStringLiteral( "a .lodl landscape file (LODT)" );
	if ( m == 0x4D444F4CU ) return QStringLiteral( "a .lodm material sidecar (LODM)" );
	if ( m == 0x20534444U ) return QStringLiteral( "a DDS texture" );
	if ( m == LODTEX_MAGIC_RETIRED_LODV ) return QStringLiteral( "a retired .lodv container (LODV)" );
	return QString( "unknown magic 0x%1" ).arg( m, 8, 16, QChar( '0' ) );
}

//! Matrix (row-major) -> quaternion (w, x, y, z), the same arithmetic as Matrix::toQuat.
void matToQuat( const float m[9], float q[4] )
{
	auto M = [&]( int r, int c ) { return m[r * 3 + c]; };
	const float trace = M( 0, 0 ) + M( 1, 1 ) + M( 2, 2 );
	if ( trace > 0.0f ) {
		float root = std::sqrt( trace + 1.0f );
		q[0] = root * 0.5f;
		root = 0.5f / root;
		q[1] = ( M( 2, 1 ) - M( 1, 2 ) ) * root;
		q[2] = ( M( 0, 2 ) - M( 2, 0 ) ) * root;
		q[3] = ( M( 1, 0 ) - M( 0, 1 ) ) * root;
	} else {
		int i = ( M( 1, 1 ) > M( 0, 0 ) ) ? 1 : 0;
		if ( M( 2, 2 ) > M( i, i ) )
			i = 2;
		const int next[3] = { 1, 2, 0 };
		const int j = next[i], k = next[j];
		float root = std::sqrt( M( i, i ) - M( j, j ) - M( k, k ) + 1.0f );
		q[i + 1] = root * 0.5f;
		root = 0.5f / root;
		q[0] = ( M( k, j ) - M( j, k ) ) * root;
		q[j + 1] = ( M( j, i ) + M( i, j ) ) * root;
		q[k + 1] = ( M( k, i ) + M( i, k ) ) * root;
	}
	const float l = std::sqrt( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] );
	for ( int i = 0; i < 4; i++ )
		q[i] /= l;
}

void quatToMat( const float q[4], float m[9] )
{
	const float w = q[0], x = q[1], y = q[2], z = q[3];
	m[0] = 1 - 2 * ( y * y + z * z ); m[1] = 2 * ( x * y - z * w );     m[2] = 2 * ( x * z + y * w );
	m[3] = 2 * ( x * y + z * w );     m[4] = 1 - 2 * ( x * x + z * z ); m[5] = 2 * ( y * z - x * w );
	m[6] = 2 * ( x * z - y * w );     m[7] = 2 * ( y * z + x * w );     m[8] = 1 - 2 * ( x * x + y * y );
}

} // namespace

/* ---------------------------------------------------------------- packing */

void lodiPackRotation( const float m[9], quint16 out[3] )
{
	float q[4];
	matToQuat( m, q );
	int big = 0;
	for ( int i = 1; i < 4; i++ )
		if ( std::fabs( q[i] ) > std::fabs( q[big] ) )
			big = i;
	if ( q[big] < 0.0f )
		for ( int i = 0; i < 4; i++ )
			q[i] = -q[i];
	quint64 bits = quint64( big );          // bits 0-1: which component was dropped
	int shift = 2;
	for ( int i = 0; i < 4; i++ ) {
		if ( i == big )
			continue;
		const float f = ( q[i] / SQRT_HALF + 1.0f ) * 0.5f * 32767.0f;
		const quint64 u = quint64( std::clamp( int( std::lround( f ) ), 0, 32767 ) );
		bits |= u << shift;
		shift += 15;
	}
	out[0] = quint16( bits & 0xFFFF );
	out[1] = quint16( ( bits >> 16 ) & 0xFFFF );
	out[2] = quint16( ( bits >> 32 ) & 0xFFFF );
}

void lodiUnpackRotation( const quint16 in[3], float quat[4], float m[9] )
{
	const quint64 bits = quint64( in[0] ) | ( quint64( in[1] ) << 16 ) | ( quint64( in[2] ) << 32 );
	const int big = int( bits & 3 );
	float q[4] = { 0, 0, 0, 0 };
	int shift = 2;
	float ss = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		if ( i == big )
			continue;
		const float u = float( ( bits >> shift ) & 0x7FFF );
		q[i] = ( u / 32767.0f * 2.0f - 1.0f ) * SQRT_HALF;
		ss += q[i] * q[i];
		shift += 15;
	}
	q[big] = std::sqrt( std::max( 0.0f, 1.0f - ss ) );
	if ( quat )
		for ( int i = 0; i < 4; i++ )
			quat[i] = q[i];
	if ( m )
		quatToMat( q, m );
}

int lodiChunkOf( float v )
{
	return int( std::floor( v / LODI_CHUNK_UNITS ) );
}

int lodiCellOf( float x, float y, int chunkX, int chunkY )
{
	const int lx = std::clamp( int( std::floor( ( x - float( chunkX ) * LODI_CHUNK_UNITS ) / LODI_CELL_UNITS ) ), 0, LODI_CHUNK_CELLS - 1 );
	const int ly = std::clamp( int( std::floor( ( y - float( chunkY ) * LODI_CHUNK_UNITS ) / LODI_CELL_UNITS ) ), 0, LODI_CHUNK_CELLS - 1 );
	return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;
}

bool lodiCellAgrees( int storedCell, float x, float y, int chunkX, int chunkY, QString * why )
{
	const float fx = x - float( chunkX ) * LODI_CHUNK_UNITS;
	const float fy = y - float( chunkY ) * LODI_CHUNK_UNITS;
	const int lx = std::clamp( int( std::floor( fx / LODI_CELL_UNITS ) ), 0, LODI_CHUNK_CELLS - 1 );
	const int ly = std::clamp( int( std::floor( fy / LODI_CELL_UNITS ) ), 0, LODI_CHUNK_CELLS - 1 );
	const int derived = ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;
	if ( derived == storedCell )
		return true;
	const int slx = storedCell % LODI_CHUNK_CELLS;
	const int sly = LODI_CHUNK_CELLS - 1 - storedCell / LODI_CHUNK_CELLS;
	const int dax = std::abs( slx - lx ), day = std::abs( sly - ly );
	float line = -1.0f;
	if ( dax == 1 && day == 0 )
		line = std::fabs( fx - float( std::max( slx, lx ) ) * LODI_CELL_UNITS );
	else if ( day == 1 && dax == 0 )
		line = std::fabs( fy - float( std::max( sly, ly ) ) * LODI_CELL_UNITS );
	if ( line >= 0.0f && line <= LODI_CELL_QUANT_TOL )
		return true;
	if ( why ) {
		*why = QString( "it is stored in cell %1 but its position (%2, %3 in the chunk box) derives cell %4; %5" )
			.arg( storedCell ).arg( double( fx ), 0, 'f', 4 ).arg( double( fy ), 0, 'f', 4 ).arg( derived )
			.arg( line < 0.0f
				? QString( "the two cells are not neighbours on one axis" )
				: QString( "it is %1 u from the cell line, past the %2 u the quantiser could have moved it" )
					.arg( double( line ), 0, 'f', 6 ).arg( double( LODI_CELL_QUANT_TOL ), 0, 'f', 6 ) );
	}
	return false;
}

quint32 lodiChunkIndex( const LodiHeader & h, int chunkX, int chunkY )
{
	const quint32 w = quint32( h.chunkEast - h.chunkWest + 1 );
	return quint32( h.chunkNorth - chunkY ) * w + quint32( chunkX - h.chunkWest );
}

void lodiChunkAt( const LodiHeader & h, quint32 index, int * chunkX, int * chunkY )
{
	const quint32 w = quint32( h.chunkEast - h.chunkWest + 1 );
	if ( chunkX ) *chunkX = h.chunkWest + int( index % w );
	if ( chunkY ) *chunkY = h.chunkNorth - int( index / w );
}

void lodiDecodePosition( const LodiHeader & h, quint32 chunkIndex, const LodiChunk & c,
	const LodiInstance & r, float xyz[3] )
{
	int cx, cy;
	lodiChunkAt( h, chunkIndex, &cx, &cy );
	xyz[0] = lodoDequantU16( r.pos[0], float( cx ) * LODI_CHUNK_UNITS, LODI_CHUNK_UNITS );
	xyz[1] = lodoDequantU16( r.pos[1], float( cy ) * LODI_CHUNK_UNITS, LODI_CHUNK_UNITS );
	xyz[2] = lodoDequantU16( r.pos[2], c.zMin, c.zExtent );
}

/* ---------------------------------------------------------------- writer */

bool lodiWrite( const QString & path, const LodiSrcSet & set, LodiHeader * headerOut,
	LodiWriteStats * stats, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	const QByteArray edid = set.worldspaceEdid.toUtf8();
	if ( edid.size() >= H_EDID_BYTES )
		return fail( QString( "worldspace editor ID '%1' is %2 bytes; the header holds 31 + NUL (refused, never truncated)" )
			.arg( set.worldspaceEdid ).arg( edid.size() ) );
	if ( !( set.flags & LODI_FLAG_ROW_ORDER_NORTH_UP ) )
		return fail( QStringLiteral( "flags: ROW_ORDER_NORTH_UP is clear; no other row order is defined" ) );
	if ( set.flags & ~LODI_FLAGS_KNOWN )
		return fail( QString( "flags 0x%1 has reserved bits set" ).arg( set.flags, 0, 16 ) );
	if ( set.lodoIdentity == 0 && !( set.flags & LODI_FLAG_NOLIB ) )
		return fail( QStringLiteral( "lodoIdentity is 0 without the NOLIB flag" ) );
	if ( set.lodoIdentity != 0 && ( set.flags & LODI_FLAG_NOLIB ) )
		return fail( QStringLiteral( "NOLIB is set but lodoIdentity is not 0" ) );

	// the two u16 refusals and the chunk extent, over the whole set, before a byte is written
	const size_t n = set.instances.size();
	if ( n > 0xFFFFFFFFULL )
		return fail( QStringLiteral( "more than 2^32 instances" ) );
	std::vector<int> cxs( n ), cys( n );
	int west = 0, east = 0, south = 0, north = 0;
	float maxScale = 0.0f;
	quint32 maxBase = 0;
	for ( size_t i = 0; i < n; i++ ) {
		const LodiSrcInstance & r = set.instances[i];
		if ( !( r.scale <= LODI_SCALE_MAX_WIDE ) || r.scale < 0.0f )
			return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 .. %5 (8 + the u16/8192 ceiling, v10); refused, not clamped" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( r.scolPart ).arg( r.baseName ).arg( double( r.scale ) ).arg( double( LODI_SCALE_MAX_WIDE ), 0, 'f', 5 ) );
		if ( r.flags & LODI_INST_SCALE_WIDE )
			return fail( QString( "ref 0x%1: the caller set instance flag bit 7 (SCALE_WIDE); the writer alone decides it from the scale" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ) );
		if ( r.baseId > LODI_BASE_MAX )
			return fail( QString( "base %1 (ref 0x%2): baseId %3 is past the u16 base table (65,535); refused" )
				.arg( r.baseName ).arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( r.baseId ) );
		if ( r.flags & ~LODI_INST_FLAGS_KNOWN )
			return fail( QString( "ref 0x%1: instance flags 0x%2 set reserved bits" ).arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( r.flags, 0, 16 ) );
		if ( !std::isfinite( r.pos[0] ) || !std::isfinite( r.pos[1] ) || !std::isfinite( r.pos[2] ) )
			return fail( QString( "ref 0x%1: position is not finite" ).arg( r.refFormId, 8, 16, QChar( '0' ) ) );
		cxs[i] = lodiChunkOf( r.pos[0] );
		cys[i] = lodiChunkOf( r.pos[1] );
		if ( cxs[i] < -32768 || cxs[i] > 32767 || cys[i] < -32768 || cys[i] > 32767 )
			return fail( QString( "ref 0x%1 at (%2, %3) falls in chunk (%4, %5), outside the i16 chunk grid" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( double( r.pos[0] ) ).arg( double( r.pos[1] ) ).arg( cxs[i] ).arg( cys[i] ) );
		if ( i == 0 ) {
			west = east = cxs[i];
			south = north = cys[i];
		} else {
			west = std::min( west, cxs[i] ); east = std::max( east, cxs[i] );
			south = std::min( south, cys[i] ); north = std::max( north, cys[i] );
		}
		maxScale = std::max( maxScale, r.scale );
		maxBase = std::max( maxBase, r.baseId );
	}
	const quint64 chunkCount = n ? quint64( east - west + 1 ) * quint64( north - south + 1 ) : 0;
	if ( chunkCount > LODI_MAX_CHUNKS ) {
		// name the extreme chunk: the one farthest from the set's centre
		size_t far = 0;
		double best = -1.0;
		for ( size_t i = 0; i < n; i++ ) {
			const double d = std::fabs( cxs[i] - 0.5 * ( west + east ) ) + std::fabs( cys[i] - 0.5 * ( south + north ) );
			if ( d > best ) { best = d; far = i; }
		}
		return fail( QString( "dense chunk table would be %1 x %2 = %3 chunks, past the 65,536 cap; the extreme chunk is (%4, %5), ref 0x%6" )
			.arg( east - west + 1 ).arg( north - south + 1 ).arg( chunkCount ).arg( cxs[far] ).arg( cys[far] )
			.arg( set.instances[far].refFormId, 8, 16, QChar( '0' ) ) );
	}

	LodiHeader h;
	h.flags = set.flags;
	h.pluginCorpusHash = set.pluginCorpusHash;
	h.objectCorpusHash = set.objectCorpusHash;
	h.lodoIdentity = set.lodoIdentity;
	h.loadOrderHash = set.loadOrderHash;
	h.worldspaceEdid = set.worldspaceEdid;
	h.chunkWest = qint16( west ); h.chunkEast = qint16( east );
	h.chunkSouth = qint16( south ); h.chunkNorth = qint16( north );
	h.chunkCount = quint32( chunkCount );
	h.instanceCount = quint32( n );

	/* THE ONE SORT LAW (v2, lodifile.h): chunk, cell, drawKey, ref, part.
	 * The cell stays outermost because the 8-byte cell row states ONE
	 * (first, count) run and a mesh-major order inside a chunk would leave a
	 * cell in up to sixteen runs; the mesh/material rank sorts inside it. */
	std::vector<quint32> chunkIdx( n ), cellIdx( n );
	for ( size_t i = 0; i < n; i++ ) {
		chunkIdx[i] = lodiChunkIndex( h, cxs[i], cys[i] );
		cellIdx[i] = quint32( lodiCellOf( set.instances[i].pos[0], set.instances[i].pos[1], cxs[i], cys[i] ) );
	}
	std::vector<size_t> order( n );
	std::iota( order.begin(), order.end(), size_t( 0 ) );
	std::stable_sort( order.begin(), order.end(), [&]( size_t a, size_t b ) {
		const LodiSrcInstance & A = set.instances[a];
		const LodiSrcInstance & B = set.instances[b];
		return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )
			< std::make_tuple( chunkIdx[b], cellIdx[b], B.drawKey, B.refFormId, B.scolPart );
	} );

	// the chunk table, the z range from origins, the cell ranges
	std::vector<LodiChunk> chunks;
	chunks.resize( static_cast<size_t>( chunkCount ) );
	std::memset( chunks.data(), 0, chunks.size() * sizeof( LodiChunk ) );
	std::vector<LodiCellRange> cells;
	std::vector<LodiInstance> inst( n );
	std::vector<LodiCold> cold( n );
	std::vector<quint8> pao;                    //!< v5: one placement-AO byte an instance, or empty
	if ( set.placementAo )
		pao.assign( n, LODI_PLACEMENT_AO_UNMEASURED );
	std::vector<quint32> cellSlot( n, 0 );      //!< v3: which cell row each sorted instance landed in
	quint32 present = 0, maxInst = 0;
	for ( size_t k = 0; k < n; ) {
		const quint32 ci = chunkIdx[order[k]];
		size_t e = k;
		float zMin = 3.4e38f, zMax = -3.4e38f, maxR = 0.0f;
		while ( e < n && chunkIdx[order[e]] == ci ) {
			const LodiSrcInstance & r = set.instances[order[e]];
			zMin = std::min( zMin, r.pos[2] );
			zMax = std::max( zMax, r.pos[2] );
			// the quantised scale, because that is the one the consumer reads
			const float qs = lodiScaleQuantised( r.scale );
			maxR = std::max( maxR, r.boundRadius * qs );
			e++;
		}
		LodiChunk & c = chunks[ci];
		c.instanceFirst = quint32( k );
		c.instanceCount = quint32( e - k );
		c.zMin = zMin;
		c.zExtent = zMax - zMin;
		c.maxBoundRadius = maxR;
		c.cellRangeOffset = quint32( cells.size() );
		cells.resize( cells.size() + LODI_CHUNK_CELLS * LODI_CHUNK_CELLS, LodiCellRange{ 0, 0 } );
		int cx, cy;
		lodiChunkAt( h, ci, &cx, &cy );
		for ( size_t i = k; i < e; i++ ) {
			const LodiSrcInstance & r = set.instances[order[i]];
			LodiInstance & q = inst[i];
			q.pos[0] = lodoQuantU16( r.pos[0], float( cx ) * LODI_CHUNK_UNITS, LODI_CHUNK_UNITS );
			q.pos[1] = lodoQuantU16( r.pos[1], float( cy ) * LODI_CHUNK_UNITS, LODI_CHUNK_UNITS );
			q.pos[2] = lodoQuantU16( r.pos[2], c.zMin, c.zExtent );
			lodiPackRotation( r.rot, q.rot );
			q.scale = lodiScaleWord( r.scale );
			q.baseId = quint16( r.baseId );
			q.ao = r.ao; q.sky = r.sky; q.ground = r.ground; q.seed = r.seed;
			q.flags = quint16( r.flags | ( lodiScaleIsWide( r.scale ) ? LODI_INST_SCALE_WIDE : 0 ) );
			q.drawKey = r.drawKey;
			cold[i].refFormId = r.refFormId;
			cold[i].scolPart = r.scolPart;
			cold[i].identity = r.identity;
			/* v5: the placement-AO byte at the instance's OWN index, and the
			 * per-slot tally. Both are taken in the SORTED order, so the blob
			 * is joinable to the instance blob by index with no search, exactly
			 * as the cold blob is. */
			if ( !pao.empty() )
				pao[i] = r.placementAo;
			if ( r.mnamSlot < 4 )
				h.slotInstances[r.mnamSlot]++;
			cellSlot[i] = c.cellRangeOffset + cellIdx[order[i]];
			LodiCellRange & cr = cells[cellSlot[i]];
			if ( cr.instanceCount == 0 )
				cr.instanceFirst = quint32( i );
			cr.instanceCount++;
		}
		c.crc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( &inst[k] ), qsizetype( ( e - k ) * sizeof( LodiInstance ) ) );
		c.crc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( &cold[k] ), qsizetype( ( e - k ) * sizeof( LodiCold ) ), c.crc32 );
		present++;
		maxInst = std::max( maxInst, c.instanceCount );
		k = e;
	}
	h.presentChunks = present;
	h.maxInstancesPerChunk = maxInst;

	/* v3: THE OCCLUDER BOXES, per cell, largest first.
	 *
	 * The emitter offers a box for any placement whose own LOD mesh it could
	 * fit one inside; the WRITER chooses, because only the writer knows the
	 * cell partition. Ordering is world VOLUME descending, then the instance
	 * index ascending -- the second key is not decoration, it is what makes two
	 * writes of one set byte-identical when two boxes measure the same volume. */
	std::vector<LodiOccluderRange> occRanges( cells.size(), LodiOccluderRange{ 0, 0 } );
	std::vector<LodiOccluder> occ;
	quint32 occCandidates = 0, occDropped = 0, cellsWithOcc = 0, cellsPopulated = 0;
	{
		std::vector<std::vector<std::pair<double, quint32>>> perCell( cells.size() );
		for ( size_t i = 0; i < n; i++ ) {
			const LodiSrcInstance & r = set.instances[order[i]];
			if ( !r.hasOccluder )
				continue;
			const float qs = lodiScaleQuantised( r.scale );
			const double vol = 8.0 * double( r.occHalf[0] * qs ) * double( r.occHalf[1] * qs ) * double( r.occHalf[2] * qs );
			if ( !( vol > 0.0 ) )
				continue;
			occCandidates++;
			perCell[cellSlot[i]].push_back( std::make_pair( vol, quint32( i ) ) );
		}
		for ( size_t s = 0; s < cells.size(); s++ ) {
			if ( cells[s].instanceCount )
				cellsPopulated++;
			std::vector<std::pair<double, quint32>> & v = perCell[s];
			if ( v.empty() )
				continue;
			std::sort( v.begin(), v.end(), []( const std::pair<double, quint32> & a, const std::pair<double, quint32> & b ) {
				if ( a.first != b.first )
					return a.first > b.first;
				return a.second < b.second;
			} );
			occRanges[s].occluderFirst = quint32( occ.size() );
			const size_t keep = std::min<size_t>( v.size(), LODI_OCCLUDERS_PER_CELL );
			occDropped += quint32( v.size() - keep );
			for ( size_t j = 0; j < keep; j++ ) {
				const quint32 ii = v[j].second;
				const LodiSrcInstance & r = set.instances[order[ii]];
				const float qs = lodiScaleQuantised( r.scale );
				LodiOccluder b;
				std::memset( &b, 0, sizeof( b ) );
				for ( int k = 0; k < 3; k++ ) {
					b.centre[k] = r.pos[k] + qs * ( r.rot[k * 3] * r.occCentre[0]
						+ r.rot[k * 3 + 1] * r.occCentre[1] + r.rot[k * 3 + 2] * r.occCentre[2] );
					/* 0.999: the stored rotation is the quantised one (worst
					 * 0.0073 deg) and the consumer builds the box's axes from
					 * it, so the box is pulled in by more than that rotation
					 * can move a corner. An occluder that leaves its object is
					 * worse than no occluder. */
					b.halfExtent[k] = r.occHalf[k] * qs * 0.999f;
				}
				b.rot[0] = inst[ii].rot[0];
				b.rot[1] = inst[ii].rot[1];
				b.rot[2] = inst[ii].rot[2];
				b.flags = 1;            // fitted inside a watertight mesh; the only rule that emits one today
				b.instanceIndex = ii;
				b.meshId = r.occMeshId;
				occ.push_back( b );
			}
			occRanges[s].occluderCount = quint32( keep );
			if ( keep )
				cellsWithOcc++;
		}
	}
	h.occluderCount = quint32( occ.size() );

	/* v4: THE AGGREGATE ROWS AND THE COVERED BLOB (bungo 2026-09-11 08:3x).
	 *
	 * The emitter names its covered instances by SOURCE index; only the writer
	 * knows the sort, so the remap lives here. Every covered index is checked
	 * against the set, against its own aggregate's cell, and for duplication
	 * across aggregates -- an instance covered twice would be suppressed twice
	 * and the count identity gate would be measuring a number nothing else
	 * agrees with. */
	std::vector<LodiAggregate> aggs;
	std::vector<quint32> covered;
	if ( !set.aggregates.empty() ) {
		std::vector<quint32> written( n, 0 );       // source index -> written index
		for ( size_t i = 0; i < n; i++ )
			written[order[i]] = quint32( i );
		std::vector<quint8> claimed( n, 0 );
		std::vector<size_t> aggOrder( set.aggregates.size() );
		std::iota( aggOrder.begin(), aggOrder.end(), size_t( 0 ) );
		/* Cell order, north-up row-major like everything else in this file, so
		 * two writes of one set are byte-identical and a consumer can bisect. */
		std::stable_sort( aggOrder.begin(), aggOrder.end(), [&]( size_t a, size_t b ) {
			const LodiSrcAggregate & A = set.aggregates[a];
			const LodiSrcAggregate & B = set.aggregates[b];
			return std::make_tuple( -A.cellY, A.cellX ) < std::make_tuple( -B.cellY, B.cellX );
		} );
		for ( size_t ai = 0; ai < aggOrder.size(); ai++ ) {
			const LodiSrcAggregate & a = set.aggregates[aggOrder[ai]];
			if ( a.flags & ~LODI_AGG_FLAGS_KNOWN )
				return fail( QString( "aggregate cell (%1, %2): flags 0x%3 set reserved bits" )
					.arg( a.cellX ).arg( a.cellY ).arg( a.flags, 0, 16 ) );
			if ( !( a.flags & LODI_AGG_HEIGHT ) )
				return fail( QString( "aggregate cell (%1, %2): HEIGHT is clear, and every aggregate sheet carries height (bungo's far-shadow ruling of 08:2x)" )
					.arg( a.cellX ).arg( a.cellY ) );
			if ( a.covered.empty() )
				return fail( QString( "aggregate cell (%1, %2) stands for no instance at all" )
					.arg( a.cellX ).arg( a.cellY ) );
			if ( !( a.half[0] > 0.0f ) || !( a.half[1] > 0.0f ) )
				return fail( QString( "aggregate cell (%1, %2): half extent %2 x %3 is not positive" )
					.arg( a.cellX ).arg( a.cellY ).arg( double( a.half[0] ) ).arg( double( a.half[1] ) ) );
			if ( !( a.depthSpan > 0.0f ) )
				return fail( QString( "aggregate cell (%1, %2): depthSpan is not positive, so the height channel decodes to nothing" )
					.arg( a.cellX ).arg( a.cellY ) );
			if ( a.views < 2 )
				return fail( QString( "aggregate cell (%1, %2): %3 views; a card needs at least two azimuths to blend between" )
					.arg( a.cellX ).arg( a.cellY ).arg( a.views ) );
			if ( ai && a.views != aggs.front().views )
				return fail( QString( "aggregate cell (%1, %2): %3 views against %4 on the first row; the view count is a FILE constant (header 0xCA)" )
					.arg( a.cellX ).arg( a.cellY ).arg( a.views ).arg( aggs.front().views ) );
			LodiAggregate row;
			std::memset( &row, 0, sizeof( row ) );
			for ( int k = 0; k < 3; k++ )
				row.centre[k] = a.centre[k];
			row.half[0] = a.half[0];
			row.half[1] = a.half[1];
			row.depthSpan = a.depthSpan;
			row.boundRadius = a.boundRadius;
			row.cellX = qint16( a.cellX );
			row.cellY = qint16( a.cellY );
			row.views = a.views;
			row.flags = a.flags;
			row.identity = LODI_AGG_IDENTITY_BIT | quint32( ai );
			row.coveredFirst = quint32( covered.size() );
			std::vector<quint32> mine;
			mine.reserve( a.covered.size() );
			for ( quint32 src : a.covered ) {
				if ( src >= n )
					return fail( QString( "aggregate cell (%1, %2) covers instance %3, past the %4 in the set" )
						.arg( a.cellX ).arg( a.cellY ).arg( src ).arg( n ) );
				if ( claimed[src] )
					return fail( QString( "aggregate cell (%1, %2) covers instance %3 (ref 0x%4), which another aggregate already covers" )
						.arg( a.cellX ).arg( a.cellY ).arg( src )
						.arg( set.instances[src].refFormId, 8, 16, QChar( '0' ) ) );
				const LodiSrcInstance & r = set.instances[src];
				const int icx = int( std::floor( r.pos[0] / LODI_CELL_UNITS ) );
				const int icy = int( std::floor( r.pos[1] / LODI_CELL_UNITS ) );
				if ( icx != a.cellX || icy != a.cellY )
					return fail( QString( "aggregate cell (%1, %2) covers ref 0x%3, which stands in cell (%4, %5)" )
						.arg( a.cellX ).arg( a.cellY ).arg( r.refFormId, 8, 16, QChar( '0' ) )
						.arg( icx ).arg( icy ) );
				claimed[src] = 1;
				mine.push_back( written[src] );
			}
			std::sort( mine.begin(), mine.end() );
			row.coveredCount = quint32( mine.size() );
			covered.insert( covered.end(), mine.begin(), mine.end() );
			aggs.push_back( row );
		}
	}
	h.aggregateCount = quint32( aggs.size() );
	h.coveredCount = quint32( covered.size() );
	h.aggregateViews = aggs.empty() ? quint16( 0 ) : aggs.front().views;
	h.aggSwitchPx = aggs.empty() ? 0.0f : set.aggSwitchPx;
	h.aggBandRatio = aggs.empty() ? 0.0f : set.aggBandRatio;
	if ( !aggs.empty() ) {
		if ( !( h.aggSwitchPx > 0.0f ) )
			return fail( QStringLiteral( "aggSwitchPx is not positive; the cross-fade band would have no threshold to sit above" ) );
		if ( !( h.aggBandRatio > 1.0f ) )
			return fail( QString( "aggBandRatio %1 is not above 1; the band would be empty and the swap would be a pop" )
				.arg( double( h.aggBandRatio ) ) );
	}
	/* v6: THE VERTEX-AO BLOB, u32 first[n + 1] then the bytes, both in the
	 * SORTED instance order so it joins the instance blob by index. */
	std::vector<quint8> vao;
	if ( set.vertexAo ) {
		if ( !set.placementAo )
			return fail( QStringLiteral( "vertex AO without placement AO: version 6 is a superset of version 5" ) );
		quint64 total = 0;
		for ( size_t i = 0; i < n; i++ )
			total += set.instances[order[i]].vertexAo.size();
		if ( total > 0xFFFFFFFFull - 4ull * ( n + 1 ) )
			return fail( QString( "vertex-AO blob of %1 bytes does not fit a u32 size word" ).arg( total ) );
		vao.resize( 4 * ( n + 1 ) + size_t( total ) );
		quint32 cursor = 0;
		for ( size_t i = 0; i <= n; i++ ) {
			std::memcpy( &vao[4 * i], &cursor, 4 );
			if ( i == n )
				break;
			const std::vector<quint8> & v = set.instances[order[i]].vertexAo;
			if ( !v.empty() )
				std::memcpy( &vao[4 * ( n + 1 ) + cursor], v.data(), v.size() );
			cursor += quint32( v.size() );
		}
	}
	/* v7 (a): THE GROUP TABLE. The emitter hands in a GLOBAL groupKey an
	 * instance; the writer turns those into ids DENSE PER CHUNK from 0, because
	 * only the writer knows the sort and the chunk partition. A component that
	 * straddles a chunk line therefore becomes two groups, one a side -- the
	 * bake never sees such a house whole, and inventing a joint across the seam
	 * would be a number with nothing behind it. */
	std::vector<quint16> grp;
	quint32 groupsTotal = 0, groupedPlacements = 0, largestGroup = 0, singletonGroups = 0;
	if ( set.group ) {
		if ( !set.vertexAo )
			return fail( QStringLiteral( "group table without vertex AO: version 7 is a superset of version 6" ) );
		grp.assign( n, 0 );
		for ( size_t ci = 0; ci < chunks.size(); ci++ ) {
			const LodiChunk & c = chunks[ci];
			if ( c.instanceCount == 0 )
				continue;
			// key -> dense id, in the sorted order, so the ids are a function of the file
			std::unordered_map<quint32, quint32> dense;
			std::vector<quint32> members;
			for ( quint32 i = 0; i < c.instanceCount; i++ ) {
				const quint32 key = set.instances[order[c.instanceFirst + i]].groupKey;
				quint32 id;
				if ( key == LODI_GROUP_ALONE ) {
					id = quint32( members.size() );
					members.push_back( 0 );
				} else {
					auto it = dense.find( key );
					if ( it == dense.end() ) {
						id = quint32( members.size() );
						dense.emplace( key, id );
						members.push_back( 0 );
					} else {
						id = it->second;
					}
				}
				if ( id > 65535 )
					return fail( QString( "chunk %1 needs %2 groups; the group word is a u16 and stops at 65,536" )
						.arg( ci ).arg( quint64( id ) + 1 ) );
				members[id]++;
				grp[c.instanceFirst + i] = quint16( id );
			}
			groupsTotal += quint32( members.size() );
			for ( quint32 m : members ) {
				largestGroup = std::max( largestGroup, m );
				if ( m == 1 )
					singletonGroups++;
				else
					groupedPlacements += m;
			}
		}
	}
	/* v7 (b): THE VERTEX-SKY STREAM, s4.8's layout byte for byte. */
	std::vector<quint8> vsky;
	if ( set.vertexSky ) {
		if ( !set.vertexAo )
			return fail( QStringLiteral( "vertex sky without vertex AO: version 7 is a superset of version 6" ) );
		quint64 total = 0;
		for ( size_t i = 0; i < n; i++ )
			total += set.instances[order[i]].vertexSky.size();
		if ( total > 0xFFFFFFFFull - 4ull * ( n + 1 ) )
			return fail( QString( "vertex-sky stream of %1 bytes does not fit a u32 size word" ).arg( total ) );
		vsky.resize( 4 * ( n + 1 ) + size_t( total ) );
		quint32 cursor = 0;
		for ( size_t i = 0; i <= n; i++ ) {
			std::memcpy( &vsky[4 * i], &cursor, 4 );
			if ( i == n )
				break;
			const LodiSrcInstance & r = set.instances[order[i]];
			/* A placement's two streams stand for the SAME vertices of the SAME
			 * mesh. A length that disagrees means one of the two casts ran on a
			 * different mesh, and that is a refusal, not a shrug. */
			if ( !r.vertexSky.empty() && r.vertexSky.size() != r.vertexAo.size() )
				return fail( QString( "instance %1 (%2) has %3 sky bytes but %4 AO bytes; the two streams are one vertex population" )
					.arg( i ).arg( r.baseName ).arg( r.vertexSky.size() ).arg( r.vertexAo.size() ) );
			if ( !r.vertexSky.empty() )
				std::memcpy( &vsky[4 * ( n + 1 ) + cursor], r.vertexSky.data(), r.vertexSky.size() );
			cursor += quint32( r.vertexSky.size() );
		}
	}
	/* THE VERSION IS DECIDED BY WHAT THE FILE CARRIES (the contract's version
	 * table, docs/LODGEN_NATIVE_LODO_LODI.md s3.7 and its
	 * Deviation 6): no aggregate row means version 3 and a header pad that
	 * starts at 0xB0, i.e. byte for byte what this writer wrote before. */
	h.version = aggs.empty() ? LODI_VERSION : LODI_VERSION_AGGREGATE;
	/* v5 (lane NATIVE1c): the placement-AO module is the outer switch. Its off
	 * value leaves the line above untouched, so a bake without it writes the
	 * version 3 or 4 file it wrote before, byte for byte. A v5 header is a
	 * SUPERSET of a v4 one -- the aggregate words are present and may be all
	 * zero -- so the two modules do not have to be armed together. */
	if ( set.vertexAo ) {
		h.version = LODI_VERSION_VERTEX_AO;
		h.vertexAoBytes = quint32( vao.size() );
	}
	/* v7: the version rises when EITHER table is present, so `--lodi-v6` -- both
	 * switches off -- leaves the line above and every byte below untouched. */
	if ( set.group || set.vertexSky ) {
		h.version = LODI_VERSION_GROUP_SKY;
		if ( set.group ) {
			h.groupCount = groupsTotal;
			h.groupStride = LODI_GROUP_STRIDE;
		}
		if ( set.vertexSky )
			h.vertexSkyBytes = quint32( vsky.size() );
	}
	if ( set.placementAo ) {
		if ( !set.vertexAo )
			h.version = LODI_VERSION_PLACEMENT_AO;
		h.placementAoCount = h.instanceCount;
		h.placementAoStride = 1;
		if ( quint64( h.slotInstances[0] ) + h.slotInstances[1] + h.slotInstances[2] + h.slotInstances[3]
			!= quint64( h.instanceCount ) )
			return fail( QString( "the four MNAM-slot totals sum to %1 but the file holds %2 instances" )
				.arg( quint64( h.slotInstances[0] ) + h.slotInstances[1] + h.slotInstances[2] + h.slotInstances[3] )
				.arg( h.instanceCount ) );
	}

	/* v9, THE SCRAPPABLE BIT (lane HORIZON3, 2026-09-19). No table, no offset,
	 * no header word: it is bit 6 of the flags the instance record has carried
	 * since v1, and the version word is the only thing that moves.
	 *
	 * THE GUARD, and it is why this is not two lines. Version 9 IMPLIES version
	 * 7's 512-byte header block (lane HORIZONOUT, 2026-09-19: 9 is the v7 layout
	 * plus this bit, and NOT a superset of the retired version 8). A bake run
	 * with `--lodi-v6` has a 256-byte header, so writing 9 on it would claim a
	 * layout the file does not have. On such a file the bit is DROPPED rather
	 * than written into a version that would be a lie -- and dropped visibly,
	 * because `scrappableWritten` comes back 0 and the caller's census says 0
	 * where the rule said otherwise. */
	quint32 scrappableWritten = 0;
	for ( const LodiInstance & r : inst )
		if ( r.flags & LODI_INST_SCRAPPABLE )
			scrappableWritten++;
	if ( scrappableWritten ) {
		if ( h.version >= LODI_VERSION_GROUP_SKY ) {
			h.version = LODI_VERSION_SCRAPPABLE;
		} else {
			for ( LodiInstance & r : inst )
				r.flags = quint16( r.flags & quint16( ~quint16( LODI_INST_SCRAPPABLE ) ) );
			scrappableWritten = 0;
		}
	}
	/* v10, THE WIDE-SCALE BIT (lane BAKE2, 2026-09-25): like v9, no table and no
	 * header word, only bit 7 of the instance flags and the version word. It
	 * rises ONLY when an instance carries the bit, so a file whose scales all
	 * fit stays the v7/v9 file it was, byte for byte. v10 is the v9 layout (bit
	 * 6 keeps its meaning), so it needs v7's header block: a pre-v7 bake with a
	 * wide scale is REFUSED -- dropping the bit would misplace the object by 8x
	 * its size, and no pre-v7 version can say it. */
	quint32 wideScaleWritten = 0;
	for ( const LodiInstance & r : inst )
		if ( r.flags & LODI_INST_SCALE_WIDE )
			wideScaleWritten++;
	if ( wideScaleWritten ) {
		if ( h.version < LODI_VERSION_GROUP_SKY )
			return fail( QString( "%1 instance(s) carry a scale above %2, which needs version %3 (the v7 header "
				"block); this bake asked for a version-%4 file (--lodi-v6?). Refused, not clamped" )
				.arg( wideScaleWritten ).arg( double( LODI_SCALE_MAX ), 0, 'f', 5 )
				.arg( LODI_VERSION_WIDE_SCALE ).arg( h.version ) );
		h.version = LODI_VERSION_WIDE_SCALE;
	}
	/* v11, THE INITIALLY-DISABLED BIT (lane NEAR1, 2026-09-26): bit 8 and the
	 * version word, nothing else. It rises ONLY when an instance carries the bit,
	 * so every file that carries none -- every far-field file -- is the v7/v9/v10
	 * file it was, byte for byte. Like v10 it needs v7's header block, and a
	 * pre-v7 set carrying it is refused rather than dropped. */
	quint32 disabledWritten = 0;
	for ( const LodiInstance & r : inst )
		if ( r.flags & LODI_INST_INITIALLY_DISABLED )
			disabledWritten++;
	if ( disabledWritten ) {
		if ( h.version < LODI_VERSION_GROUP_SKY )
			return fail( QString( "%1 instance(s) carry the initially-disabled bit, which needs version %2 (the v7 "
				"header block); this set asked for a version-%3 file. Refused, not dropped" )
				.arg( disabledWritten ).arg( LODI_VERSION_INITIALLY_DISABLED ).arg( h.version ) );
		h.version = LODI_VERSION_INITIALLY_DISABLED;
	}
	const quint32 headerBytes = lodiHeaderBytes( h.version );
	QByteArray file;
	file.resize( qsizetype( headerBytes ) );
	std::memset( file.data(), 0, size_t( headerBytes ) );
	auto payload = [&]( const void * p, quint64 bytes ) {
		const quint64 start = quint64( file.size() );
		const quint64 at = alignUp( start, LODI_PAYLOAD_ALIGN );
		file.resize( qsizetype( at + bytes ) );
		if ( at > start )
			std::memset( file.data() + qsizetype( start ), 0, size_t( at - start ) );   // Qt 6 resize() does not zero
		if ( bytes )
			std::memcpy( file.data() + qsizetype( at ), p, size_t( bytes ) );
		return at;
	};
	h.offChunks = payload( chunks.data(), chunks.size() * sizeof( LodiChunk ) );
	h.offCellRanges = payload( cells.data(), cells.size() * sizeof( LodiCellRange ) );
	h.offInstances = payload( inst.data(), inst.size() * sizeof( LodiInstance ) );
	h.offCold = payload( cold.data(), cold.size() * sizeof( LodiCold ) );
	h.offOccluders = payload( occ.data(), occ.size() * sizeof( LodiOccluder ) );
	h.offOccluderRanges = payload( occRanges.data(), occRanges.size() * sizeof( LodiOccluderRange ) );
	/* v4: the two aggregate payloads go LAST and are written only when there is
	 * something to write, so a version-3 file's byte layout is untouched. */
	if ( !aggs.empty() ) {
		h.offAggregates = payload( aggs.data(), aggs.size() * sizeof( LodiAggregate ) );
		h.offCovered = payload( covered.data(), covered.size() * sizeof( quint32 ) );
	}
	// v5: the AO blob goes last of all, so no earlier payload's offset moves
	if ( !pao.empty() )
		h.offPlacementAo = payload( pao.data(), quint64( pao.size() ) );
	// v6: the vertex-AO blob after it
	if ( !vao.empty() )
		h.offVertexAo = payload( vao.data(), quint64( vao.size() ) );
	// v7: the group table then the sky stream, after everything, so nothing moves
	if ( !grp.empty() )
		h.offGroup = payload( grp.data(), quint64( grp.size() ) * sizeof( quint16 ) );
	if ( !vsky.empty() )
		h.offVertexSky = payload( vsky.data(), quint64( vsky.size() ) );
	h.fileBytes = quint64( file.size() );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( chunks.data() ), qsizetype( chunks.size() * sizeof( LodiChunk ) ) );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( cells.data() ), qsizetype( cells.size() * sizeof( LodiCellRange ) ), h.indexCrc32 );
	// v3: the occluder table and its ranges join indexCrc32 (contract 4.5)
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( occ.data() ), qsizetype( occ.size() * sizeof( LodiOccluder ) ), h.indexCrc32 );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( occRanges.data() ), qsizetype( occRanges.size() * sizeof( LodiOccluderRange ) ), h.indexCrc32 );
	/* v4: the aggregate table and the covered blob join indexCrc32, in that
	 * order, AFTER the occluders (contract 4.6). An empty pair folds zero bytes
	 * in and leaves a version-3 file's CRC exactly where it was. */
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( aggs.data() ), qsizetype( aggs.size() * sizeof( LodiAggregate ) ), h.indexCrc32 );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( covered.data() ), qsizetype( covered.size() * sizeof( quint32 ) ), h.indexCrc32 );
	/* v5: the AO blob joins indexCrc32 last. An absent blob folds zero bytes in
	 * and leaves a version-3 or version-4 file's CRC exactly where it was. */
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( pao.data() ), qsizetype( pao.size() ), h.indexCrc32 );
	// v6: the vertex-AO blob joins after it; absent, zero bytes fold in
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vao.data() ), qsizetype( vao.size() ), h.indexCrc32 );
	/* v7: the group table then the sky stream join last, in that order. Both
	 * absent, zero bytes fold in and a v3..v6 file's CRC does not move. */
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( grp.data() ), qsizetype( grp.size() * sizeof( quint16 ) ), h.indexCrc32 );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vsky.data() ), qsizetype( vsky.size() ), h.indexCrc32 );

	putLE<quint32>( file, H_MAGIC, LODI_MAGIC );
	putLE<quint32>( file, H_VERSION, h.version );
	putLE<quint32>( file, H_FLAGS, h.flags );
	putLE<quint64>( file, H_PLUGIN, h.pluginCorpusHash );
	putLE<quint64>( file, H_OBJECT, h.objectCorpusHash );
	putLE<quint64>( file, H_IDENTITY, h.lodoIdentity );
	std::memcpy( file.data() + H_EDID, edid.constData(), size_t( edid.size() ) );
	putLE<quint16>( file, H_WEST, quint16( h.chunkWest ) );
	putLE<quint16>( file, H_SOUTH, quint16( h.chunkSouth ) );
	putLE<quint16>( file, H_EAST, quint16( h.chunkEast ) );
	putLE<quint16>( file, H_NORTH, quint16( h.chunkNorth ) );
	putLE<quint16>( file, H_CELLS, LODI_CHUNK_CELLS );
	putLE<quint16>( file, H_STRIDE, LODI_INSTANCE_STRIDE );
	putLE<quint32>( file, H_CHUNKS, h.chunkCount );
	putLE<quint32>( file, H_INSTANCES, h.instanceCount );
	putLE<quint32>( file, H_PRESENT, h.presentChunks );
	putLE<quint32>( file, H_MAXINST, h.maxInstancesPerChunk );
	putLE<quint32>( file, H_ICRC, h.indexCrc32 );
	putLE<quint64>( file, H_OFF_CHUNKS, h.offChunks );
	putLE<quint64>( file, H_OFF_CELLS, h.offCellRanges );
	putLE<quint64>( file, H_OFF_INST, h.offInstances );
	putLE<quint64>( file, H_OFF_COLD, h.offCold );
	putLE<quint64>( file, H_FILEBYTES, h.fileBytes );
	putLE<quint64>( file, H_LOADORDER, h.loadOrderHash );
	putLE<quint64>( file, H_OFF_OCC, h.offOccluders );
	putLE<quint64>( file, H_OFF_OCCRANGE, h.offOccluderRanges );
	putLE<quint32>( file, H_OCCCOUNT, h.occluderCount );
	putLE<quint16>( file, H_OCCSTRIDE, LODI_OCCLUDER_STRIDE );
	putLE<quint16>( file, H_OCCPERCELL, LODI_OCCLUDERS_PER_CELL );
	if ( !aggs.empty() ) {
		putLE<quint64>( file, H_OFF_AGG, h.offAggregates );
		putLE<quint64>( file, H_OFF_COVERED, h.offCovered );
		putLE<quint32>( file, H_AGGCOUNT, h.aggregateCount );
		putLE<quint32>( file, H_COVCOUNT, h.coveredCount );
		putLE<quint16>( file, H_AGGSTRIDE, LODI_AGGREGATE_STRIDE );
		putLE<quint16>( file, H_AGGVIEWS, h.aggregateViews );
		putLE<quint32>( file, H_AGGSWITCH, f32Bits( h.aggSwitchPx ) );
		putLE<quint32>( file, H_AGGBAND, f32Bits( h.aggBandRatio ) );
	}
	// v5 (lane NATIVE1c): the four slot totals and the AO blob's three words
	if ( !pao.empty() ) {
		for ( int k = 0; k < 4; k++ )
			putLE<quint32>( file, H_SLOTINST + k * 4, h.slotInstances[k] );
		putLE<quint64>( file, H_OFF_PAO, h.offPlacementAo );
		putLE<quint32>( file, H_PAOCOUNT, h.placementAoCount );
		putLE<quint8>( file, H_PAOSTRIDE, h.placementAoStride );
	}
	if ( !vao.empty() ) {
		putLE<quint64>( file, H_OFF_VAO, h.offVertexAo );
		putLE<quint32>( file, H_VAOBYTES, h.vertexAoBytes );
	}
	if ( !grp.empty() ) {
		putLE<quint64>( file, H_OFF_GROUP, h.offGroup );
		putLE<quint32>( file, H_GROUPCOUNT, h.groupCount );
		putLE<quint16>( file, H_GROUPSTRIDE, h.groupStride );
	}
	if ( !vsky.empty() ) {
		putLE<quint64>( file, H_OFF_VSKY, h.offVertexSky );
		putLE<quint32>( file, H_VSKYBYTES, h.vertexSkyBytes );
	}
	/* The crc window is 0x10 .. headerBytes - 1. On a v3..v6 file headerBytes is
	 * 256 and the window is the one it always was, which is what keeps a v6
	 * bake byte-identical across this lane. */
	h.headerCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( file.constData() ) + H_PLUGIN, qsizetype( headerBytes ) - H_PLUGIN );
	putLE<quint32>( file, H_HCRC, h.headerCrc32 );

	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QString( "cannot write %1" ).arg( path ) );
	if ( f.write( file ) != file.size() )
		return fail( QString( "short write to %1" ).arg( path ) );
	f.close();
	if ( headerOut )
		*headerOut = h;
	if ( stats ) {
		stats->instances = h.instanceCount;
		stats->chunks = h.chunkCount;
		stats->presentChunks = present;
		stats->maxInstancesPerChunk = maxInst;
		stats->fileBytes = h.fileBytes;
		stats->maxScale = maxScale;
		stats->wideScaleInstances = wideScaleWritten;
		stats->maxBaseId = maxBase;
		stats->occluders = quint32( occ.size() );
		stats->occluderCandidates = occCandidates;
		stats->cellsWithOccluder = cellsWithOcc;
		stats->cellsPopulated = cellsPopulated;
		stats->occludersDropped = occDropped;
		stats->aggregates = h.aggregateCount;
		stats->coveredInstances = h.coveredCount;
		stats->aggregateViews = h.aggregateViews;
		stats->groups = groupsTotal;
		stats->groupedPlacements = groupedPlacements;
		stats->largestGroup = largestGroup;
		stats->singletonGroups = singletonGroups;
		stats->vertexSkyBytes = h.vertexSkyBytes;
		stats->vertexSkyPlacements = 0;
		for ( size_t i = 0; i < n; i++ )
			if ( !set.instances[i].vertexSky.empty() )
				stats->vertexSkyPlacements++;
		stats->version = h.version;
	}
	return true;
}

/* ---------------------------------------------------------------- reader */

bool lodiRead( const QString & path, LodiHeader * header, LodiTable * table,
	bool payloadCheck, QString * error )
{
	auto refuse = [&]( const QString & m ) {
		if ( error )
			*error = QString( "%1: %2" ).arg( QFileInfo( path ).fileName(), m );
		return false;
	};
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return refuse( QStringLiteral( "cannot open" ) );
	const QByteArray file = f.readAll();
	f.close();
	if ( file.size() < qsizetype( LODI_HEADER_BYTES ) )
		return refuse( QString( "%1 bytes, shorter than the 256-byte header" ).arg( file.size() ) );
	const unsigned char * p = reinterpret_cast<const unsigned char *>( file.constData() );

	const quint32 magic = getLE<quint32>( p + H_MAGIC );
	if ( magic != LODI_MAGIC )
		return refuse( QString( "not a .lodi instance table: the magic says %1" ).arg( nameOtherMagic( magic ) ) );
	LodiHeader h;
	h.version = getLE<quint32>( p + H_VERSION );
	if ( h.version == 1 )
		return refuse( QStringLiteral( "version 1: the v1 record's word at 0x16 is reserved-zero where v2 "
			"carries drawKey, and the v1 cold record's is reserved-zero where v2 carries the stock identity. "
			"Re-bake; this reader knows version 3 only" ) );
	if ( h.version == 2 )
		return refuse( QStringLiteral( "version 2: a v2 instance table has NO occluder tables (header 0x98 and "
			"0xA0 were reserved), so every cell would read as occluding nothing and the chunk rejection would "
			"silently do no work. Re-bake; this reader knows versions 3 and 4" ) );
	/* v3 AND v4 are both accepted, and that is not the same licence v3 refused
	 * v2 under: a v3 file read as v4 is UNAMBIGUOUS -- zero aggregates, both
	 * offsets 0 -- because aggregation is a module whose off value had to stay
	 * byte-identical (lodifile.h, contract 11 Deviation 6). What a v3 file may
	 * NOT do is carry aggregate words, and the reserved-byte sweep below is
	 * what says so. */
	if ( h.version != LODI_VERSION && h.version != LODI_VERSION_AGGREGATE
		&& h.version != LODI_VERSION_PLACEMENT_AO && h.version != LODI_VERSION_VERTEX_AO
		&& h.version != LODI_VERSION_GROUP_SKY && h.version != LODI_VERSION_HORIZON
		&& h.version != LODI_VERSION_SCRAPPABLE && h.version != LODI_VERSION_WIDE_SCALE
		&& h.version != LODI_VERSION_INITIALLY_DISABLED )
		return refuse( QString( "version %1; this reader knows %2, %3, %4, %5, %6, %7, %8 and %9" )
			.arg( h.version ).arg( LODI_VERSION ).arg( LODI_VERSION_AGGREGATE )
			.arg( LODI_VERSION_PLACEMENT_AO ).arg( LODI_VERSION_VERTEX_AO )
			.arg( LODI_VERSION_GROUP_SKY ).arg( LODI_VERSION_HORIZON )
			.arg( LODI_VERSION_SCRAPPABLE ).arg( LODI_VERSION_WIDE_SCALE ) );
	/* v7's header BLOCK is 512 bytes. On every older version this is still 256
	 * and the crc window below is the one it always was. */
	const quint32 headerBytes = lodiHeaderBytes( h.version );
	if ( file.size() < qsizetype( headerBytes ) )
		return refuse( QString( "%1 bytes, shorter than the %2-byte version-%3 header" )
			.arg( file.size() ).arg( headerBytes ).arg( h.version ) );
	h.headerCrc32 = getLE<quint32>( p + H_HCRC );
	const quint32 hcrc = lodvCrc32( p + H_PLUGIN, qsizetype( headerBytes ) - H_PLUGIN );
	if ( hcrc != h.headerCrc32 )
		return refuse( QString( "headerCrc32 0x%1 does not match the header's bytes (0x%2)" )
			.arg( h.headerCrc32, 8, 16, QChar( '0' ) ).arg( hcrc, 8, 16, QChar( '0' ) ) );
	h.flags = getLE<quint32>( p + H_FLAGS );
	if ( !( h.flags & LODI_FLAG_ROW_ORDER_NORTH_UP ) )
		return refuse( QStringLiteral( "flags: ROW_ORDER_NORTH_UP is clear (the .lodl mirror trap)" ) );
	if ( h.flags & ~LODI_FLAGS_KNOWN )
		return refuse( QString( "flags 0x%1 has reserved bits set" ).arg( h.flags, 0, 16 ) );
	h.pluginCorpusHash = getLE<quint64>( p + H_PLUGIN );
	h.objectCorpusHash = getLE<quint64>( p + H_OBJECT );
	h.lodoIdentity = getLE<quint64>( p + H_IDENTITY );
	if ( h.lodoIdentity == 0 && !( h.flags & LODI_FLAG_NOLIB ) )
		return refuse( QStringLiteral( "lodoIdentity is 0 without the NOLIB flag" ) );
	if ( h.lodoIdentity != 0 && ( h.flags & LODI_FLAG_NOLIB ) )
		return refuse( QStringLiteral( "NOLIB is set but lodoIdentity is not 0" ) );
	{
		const char * e = file.constData() + H_EDID;
		const size_t n = strnlen( e, H_EDID_BYTES );
		if ( n >= size_t( H_EDID_BYTES ) )
			return refuse( QStringLiteral( "worldspace editor ID is not NUL-terminated within 32 bytes" ) );
		h.worldspaceEdid = QString::fromUtf8( e, qsizetype( n ) );
	}
	h.chunkWest = qint16( getLE<quint16>( p + H_WEST ) );
	h.chunkSouth = qint16( getLE<quint16>( p + H_SOUTH ) );
	h.chunkEast = qint16( getLE<quint16>( p + H_EAST ) );
	h.chunkNorth = qint16( getLE<quint16>( p + H_NORTH ) );
	h.chunkCells = getLE<quint16>( p + H_CELLS );
	h.instanceStride = getLE<quint16>( p + H_STRIDE );
	if ( h.chunkCells != LODI_CHUNK_CELLS )
		return refuse( QString( "chunkCells %1; this reader knows 4" ).arg( h.chunkCells ) );
	if ( h.instanceStride != LODI_INSTANCE_STRIDE )
		return refuse( QString( "instanceStride %1; this reader knows 24" ).arg( h.instanceStride ) );
	h.chunkCount = getLE<quint32>( p + H_CHUNKS );
	h.instanceCount = getLE<quint32>( p + H_INSTANCES );
	h.presentChunks = getLE<quint32>( p + H_PRESENT );
	h.maxInstancesPerChunk = getLE<quint32>( p + H_MAXINST );
	h.indexCrc32 = getLE<quint32>( p + H_ICRC );
	h.offChunks = getLE<quint64>( p + H_OFF_CHUNKS );
	h.offCellRanges = getLE<quint64>( p + H_OFF_CELLS );
	h.offInstances = getLE<quint64>( p + H_OFF_INST );
	h.offCold = getLE<quint64>( p + H_OFF_COLD );
	h.fileBytes = getLE<quint64>( p + H_FILEBYTES );
	h.loadOrderHash = getLE<quint64>( p + H_LOADORDER );
	h.offOccluders = getLE<quint64>( p + H_OFF_OCC );
	h.offOccluderRanges = getLE<quint64>( p + H_OFF_OCCRANGE );
	h.occluderCount = getLE<quint32>( p + H_OCCCOUNT );
	h.occluderStride = getLE<quint16>( p + H_OCCSTRIDE );
	h.maxOccludersPerCell = getLE<quint16>( p + H_OCCPERCELL );
	if ( h.occluderStride != LODI_OCCLUDER_STRIDE )
		return refuse( QString( "occluderStride %1; this reader knows %2" ).arg( h.occluderStride ).arg( LODI_OCCLUDER_STRIDE ) );
	if ( h.maxOccludersPerCell == 0 )
		return refuse( QStringLiteral( "maxOccludersPerCell is 0; a file with no occluders still states its cap" ) );
	/* v4: the aggregate words, and the pad that starts after them. On a v3 file
	 * the pad still starts at 0xB0, so the SAME sweep is what refuses a v3 file
	 * that carries an aggregate table -- it reports the first non-zero byte by
	 * its own offset and names it. */
	// each version is a superset of the one below: every rule runs on the ones above
	/* v9 adds no header word and no table -- one instance flag bit and nothing
	 * else -- so every v8 rule below is also a v9 rule and the header layout is
	 * v8's exactly. */
	/* v8 is the RETIRED baked-horizon version and is the ONLY version that
	 * carries the stream (lane HORIZONOUT, 2026-09-19). v9 is the v7 layout plus
	 * the scrappable flag bit and carries no stream, so it is v7-shaped here. */
	const bool v8 = ( h.version == LODI_VERSION_HORIZON );
	const bool v7 = ( h.version == LODI_VERSION_GROUP_SKY ) || v8
		|| ( h.version == LODI_VERSION_SCRAPPABLE ) || ( h.version == LODI_VERSION_WIDE_SCALE )
		|| ( h.version == LODI_VERSION_INITIALLY_DISABLED );
	const bool v6 = ( h.version == LODI_VERSION_VERTEX_AO ) || v7;
	const bool v5 = ( h.version == LODI_VERSION_PLACEMENT_AO ) || v6;
	const int padFrom = v5 ? H_RESERVED_F1
		: ( h.version == LODI_VERSION_AGGREGATE ) ? H_RESERVED_D4 : H_RESERVED_B0;
	if ( v5 ) {
		/* v5, THE PLACEMENT-AO BLOB and the four slot totals. A v5 header is a
		 * SUPERSET of a v4 one, so the aggregate words are read here too and
		 * may legitimately be all zero -- that is a v5 file with no aggregate,
		 * which v4 could not express. */
		for ( int k = 0; k < 4; k++ )
			h.slotInstances[k] = getLE<quint32>( p + H_SLOTINST + k * 4 );
		h.offPlacementAo = getLE<quint64>( p + H_OFF_PAO );
		h.placementAoCount = getLE<quint32>( p + H_PAOCOUNT );
		h.placementAoStride = p[H_PAOSTRIDE];
		if ( h.placementAoStride != 1 )
			return refuse( QString( "placementAoStride %1; this reader knows 1" ).arg( h.placementAoStride ) );
		if ( h.offPlacementAo == 0 || h.placementAoCount == 0 )
			return refuse( QStringLiteral( "version 5 with no placement-AO blob (offset or count 0). The AO byte "
				"is what version 5 IS: a bake without it is written at version 3 or 4, which is what makes the "
				"module's off value byte-identical" ) );
		if ( v6 ) {
			h.offVertexAo = getLE<quint64>( p + H_OFF_VAO );
			h.vertexAoBytes = getLE<quint32>( p + H_VAOBYTES );
			if ( h.offVertexAo == 0 || h.vertexAoBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
				return refuse( QString( "version 6 with no vertex-AO blob (offset %1, %2 bytes; the offsets alone take %3). "
					"The blob is what version 6 IS: a bake without it is written at version 5" )
					.arg( h.offVertexAo ).arg( h.vertexAoBytes ).arg( 4ull * ( quint64( h.instanceCount ) + 1 ) ) );
		}
		if ( v7 ) {
			/* v7: the group table and the vertex-sky stream. Version 7 IS one of
			 * the two; a bake with neither is written at version 6, which is
			 * what makes `--lodi-v6` byte-identical. */
			h.offGroup = getLE<quint64>( p + H_OFF_GROUP );
			h.groupCount = getLE<quint32>( p + H_GROUPCOUNT );
			h.groupStride = getLE<quint16>( p + H_GROUPSTRIDE );
			h.offVertexSky = getLE<quint64>( p + H_OFF_VSKY );
			h.vertexSkyBytes = getLE<quint32>( p + H_VSKYBYTES );
			if ( !v8 && h.offGroup == 0 && h.offVertexSky == 0 )
				return refuse( QStringLiteral( "version 7 carrying neither a group table (header 0x100) nor a "
					"vertex-sky stream (header 0x110). Version 7 IS one of the two; a bake with neither is "
					"written at version 6, which is what makes the way back byte-identical" ) );
			if ( h.offGroup ) {
				if ( h.groupStride != LODI_GROUP_STRIDE )
					return refuse( QString( "groupStride %1; this reader knows %2" )
						.arg( h.groupStride ).arg( LODI_GROUP_STRIDE ) );
				if ( h.groupCount == 0 && h.instanceCount != 0 )
					return refuse( QStringLiteral( "a group table is present but groupCount is 0; every placement "
						"is in some group, if only its own" ) );
			} else if ( h.groupCount != 0 || h.groupStride != 0 ) {
				return refuse( QString( "no group table (header 0x100 is 0) but groupCount %1 / groupStride %2 "
					"say otherwise" ).arg( h.groupCount ).arg( h.groupStride ) );
			}
			if ( h.offVertexSky ) {
				if ( h.vertexSkyBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
					return refuse( QString( "vertex-sky stream of %1 bytes cannot hold its own %2 offset words" )
						.arg( h.vertexSkyBytes ).arg( quint64( h.instanceCount ) + 1 ) );
			} else if ( h.vertexSkyBytes != 0 ) {
				return refuse( QString( "no vertex-sky stream (header 0x110 is 0) but vertexSkyBytes is %1" )
					.arg( h.vertexSkyBytes ) );
			}
			if ( v8 ) {
				/* v8, RETIRED: the horizon stream and the three knobs the cast
				 * used. Nothing in this tree writes this any more, and nothing
				 * reads the PAYLOAD; the words are still parsed and still checked
				 * so that a v8 file met in the wild is either opened honestly or
				 * refused by name, never mis-read. */
				h.offVertexHorizon = getLE<quint64>( p + H_OFF_VHOR );
				h.vertexHorizonBytes = getLE<quint32>( p + H_VHORBYTES );
				h.horizonAzimuths = getLE<quint16>( p + H_HORAZ );
				h.horizonSteps = getLE<quint16>( p + H_HORSTEPS );
				h.horizonReach = f32Of( getLE<quint32>( p + H_HORREACH ) );
				if ( h.offVertexHorizon == 0 )
					return refuse( QStringLiteral( "version 8 with no vertex-horizon stream (header 0x11C is 0). "
						"The stream is what version 8 IS: a bake without it is written at version 7" ) );
				if ( h.vertexHorizonBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
					return refuse( QString( "vertex-horizon stream of %1 bytes cannot hold its own %2 offset words" )
						.arg( h.vertexHorizonBytes ).arg( quint64( h.instanceCount ) + 1 ) );
				if ( h.horizonAzimuths == 0 )
					return refuse( QStringLiteral( "horizonAzimuths is 0 (header 0x128); the bin count is the "
						"stream's stride and a stride of zero has no layout" ) );
				if ( !( h.horizonReach > 0.0f ) || !std::isfinite( h.horizonReach ) )
					return refuse( QString( "horizonReach %1 (header 0x12C) is not positive finite world units" )
						.arg( double( h.horizonReach ) ) );
			} else if ( getLE<quint64>( p + H_OFF_VHOR ) != 0 || getLE<quint32>( p + H_VHORBYTES ) != 0
				|| getLE<quint16>( p + H_HORAZ ) != 0 ) {
				/* A version-7 file carrying version-8 words. The 0x11C..0x1FF
				 * pad sweep below would catch it, but by OFFSET and not by
				 * name, and "reserved header byte at 0x11c" would send the next
				 * person looking at the wrong feature. */
				return refuse( QString( "version %1 carrying version-8 header words (horizon stream at 0x11C = %2, "
					"%3 bytes, %4 azimuths at 0x128). Versions 7 and 9 reserve 0x11C..0x1FF and write zeros there" )
					.arg( h.version ).arg( getLE<quint64>( p + H_OFF_VHOR ) )
					.arg( getLE<quint32>( p + H_VHORBYTES ) ).arg( getLE<quint16>( p + H_HORAZ ) ) );
			}
		} else if ( getLE<quint64>( p + H_OFF_GROUP ) != 0 || getLE<quint64>( p + H_OFF_VSKY ) != 0 ) {
			/* A v5 or v6 file whose header block ends at 0x100 cannot be
			 * carrying these words. Named, not left to the pad sweep, for the
			 * same reason the v5 words are (see just below). */
			return refuse( QString( "version %1 carrying version-7 header words (group table at 0x100 = %2, "
				"vertex-sky stream at 0x110 = %3). Versions 3 to 6 have a 256-byte header and end at 0x100" )
				.arg( h.version ).arg( getLE<quint64>( p + H_OFF_GROUP ) ).arg( getLE<quint64>( p + H_OFF_VSKY ) ) );
		}
		if ( h.placementAoCount != h.instanceCount )
			return refuse( QString( "placementAoCount %1 but the file holds %2 instances; the blob is parallel "
				"to the instance blob, one byte each" ).arg( h.placementAoCount ).arg( h.instanceCount ) );
		const quint64 slotSum = quint64( h.slotInstances[0] ) + h.slotInstances[1]
			+ h.slotInstances[2] + h.slotInstances[3];
		if ( slotSum != quint64( h.instanceCount ) )
			return refuse( QString( "the four MNAM-slot totals sum to %1 but the file holds %2 instances" )
				.arg( slotSum ).arg( h.instanceCount ) );
		h.offAggregates = getLE<quint64>( p + H_OFF_AGG );
		h.offCovered = getLE<quint64>( p + H_OFF_COVERED );
		h.aggregateCount = getLE<quint32>( p + H_AGGCOUNT );
		h.coveredCount = getLE<quint32>( p + H_COVCOUNT );
		h.aggregateStride = getLE<quint16>( p + H_AGGSTRIDE );
		h.aggregateViews = getLE<quint16>( p + H_AGGVIEWS );
		h.aggSwitchPx = f32Of( getLE<quint32>( p + H_AGGSWITCH ) );
		h.aggBandRatio = f32Of( getLE<quint32>( p + H_AGGBAND ) );
		if ( h.aggregateCount && h.aggregateStride != LODI_AGGREGATE_STRIDE )
			return refuse( QString( "aggregateStride %1; this reader knows %2" )
				.arg( h.aggregateStride ).arg( LODI_AGGREGATE_STRIDE ) );
	} else if ( getLE<quint64>( p + H_OFF_PAO ) != 0 || getLE<quint32>( p + H_PAOCOUNT ) != 0 ) {
		/* A v3 or v4 file that carries the v5 words. The reserved sweep below
		 * would catch it too, but by offset and not by NAME, and a reader that
		 * reported "reserved byte at 0xE4" for a placement-AO blob would send
		 * the next person looking in the wrong place. */
		return refuse( QString( "version %1 with a placement-AO blob at header 0xE4 (offset %2, count %3). "
			"The AO byte is a version-5 field; versions 3 and 4 reserve 0xD4..0xFF and write zeros there" )
			.arg( h.version ).arg( getLE<quint64>( p + H_OFF_PAO ) ).arg( getLE<quint32>( p + H_PAOCOUNT ) ) );
	}
	/* v4, AND A v5 FILE THAT CARRIES AGGREGATES (lane AUDIT1, 2026-09-17).
	 * These three rules sat behind the version word alone, so the day the bake
	 * started writing version 5 -- placement AO is on by default -- an
	 * aggregate in a v5 file stopped being asked whether it has two azimuths,
	 * a positive switch distance and a band above 1. The tabs builder below
	 * already asks the question the right way round; this is the same one.
	 * The `aggregateCount == 0` refusal inside stays a v4 rule, and still is
	 * one: a v5 file with no aggregate does not come in here at all. */
	if ( h.version == LODI_VERSION_AGGREGATE || ( v5 && h.aggregateCount ) ) {
		h.offAggregates = getLE<quint64>( p + H_OFF_AGG );
		h.offCovered = getLE<quint64>( p + H_OFF_COVERED );
		h.aggregateCount = getLE<quint32>( p + H_AGGCOUNT );
		h.coveredCount = getLE<quint32>( p + H_COVCOUNT );
		h.aggregateStride = getLE<quint16>( p + H_AGGSTRIDE );
		h.aggregateViews = getLE<quint16>( p + H_AGGVIEWS );
		h.aggSwitchPx = f32Of( getLE<quint32>( p + H_AGGSWITCH ) );
		h.aggBandRatio = f32Of( getLE<quint32>( p + H_AGGBAND ) );
		if ( h.aggregateStride != LODI_AGGREGATE_STRIDE )
			return refuse( QString( "aggregateStride %1; this reader knows %2" )
				.arg( h.aggregateStride ).arg( LODI_AGGREGATE_STRIDE ) );
		if ( h.aggregateCount == 0 )
			return refuse( QStringLiteral( "version 4 with aggregateCount 0; a file with no aggregate is written at version 3, "
				"because that is what makes the module's off value byte-identical" ) );
		if ( h.aggregateViews < 2 )
			return refuse( QString( "aggregateViews %1; a card needs at least two azimuths to blend between" ).arg( h.aggregateViews ) );
		if ( !( h.aggSwitchPx > 0.0f ) )
			return refuse( QStringLiteral( "aggSwitchPx is not positive; the cross-fade band has no threshold to sit above" ) );
		if ( !( h.aggBandRatio > 1.0f ) )
			return refuse( QString( "aggBandRatio %1 is not above 1; the band would be empty and the swap a pop" )
				.arg( double( h.aggBandRatio ) ) );
	}
	/* The v6 pad is 0xF1..0xF3 alone; on v7 the sweep continues over the second
	 * half of the block, 0x11C..0x1FF, which is the reserved room this lane did
	 * not spend. */
	for ( int i = padFrom; i < ( v6 ? H_RESERVED_F4 : int( LODI_HEADER_BYTES ) ); i++ )
		if ( p[i] != 0 )
			return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 2, 16, QChar( '0' ) ) );
	if ( v7 )
		for ( int i = v8 ? H_RESERVED_130 : H_RESERVED_11C; i < int( LODI_HEADER_BYTES_V7 ); i++ )
			if ( p[i] != 0 )
				return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 3, 16, QChar( '0' ) ) );
	if ( h.fileBytes != quint64( file.size() ) )
		return refuse( QString( "fileBytes %1 but the file is %2 bytes" ).arg( h.fileBytes ).arg( file.size() ) );
	if ( h.chunkEast < h.chunkWest || h.chunkNorth < h.chunkSouth ) {
		if ( h.instanceCount != 0 || h.chunkCount != 0 )
			return refuse( QStringLiteral( "chunk extent is inverted" ) );
	} else {
		const quint64 want = quint64( h.chunkEast - h.chunkWest + 1 ) * quint64( h.chunkNorth - h.chunkSouth + 1 );
		if ( want > LODI_MAX_CHUNKS )
			return refuse( QString( "chunk extent implies %1 chunks, past the 65,536 cap" ).arg( want ) );
		if ( h.chunkCount != want && !( h.instanceCount == 0 && h.chunkCount == 0 ) )
			return refuse( QString( "chunkCount %1 but the extent %2..%3 x %4..%5 implies %6" )
				.arg( h.chunkCount ).arg( h.chunkWest ).arg( h.chunkEast ).arg( h.chunkSouth ).arg( h.chunkNorth ).arg( want ) );
	}
	if ( h.presentChunks > h.chunkCount )
		return refuse( QString( "presentChunks %1 > chunkCount %2" ).arg( h.presentChunks ).arg( h.chunkCount ) );

	struct Tab { const char * name; quint64 off; quint64 bytes; };
	const quint64 cellBytes = quint64( h.presentChunks ) * LODI_CHUNK_CELLS * LODI_CHUNK_CELLS * sizeof( LodiCellRange );
	const quint64 occRangeBytes = quint64( h.presentChunks ) * LODI_CHUNK_CELLS * LODI_CHUNK_CELLS * sizeof( LodiOccluderRange );
	std::vector<Tab> tabs = {
		{ "chunk table", h.offChunks, quint64( h.chunkCount ) * sizeof( LodiChunk ) },
		{ "cell-range blob", h.offCellRanges, cellBytes },
		{ "instance blob", h.offInstances, quint64( h.instanceCount ) * sizeof( LodiInstance ) },
		{ "cold blob", h.offCold, quint64( h.instanceCount ) * sizeof( LodiCold ) },
		{ "occluder table", h.offOccluders, quint64( h.occluderCount ) * sizeof( LodiOccluder ) },
		{ "occluder range blob", h.offOccluderRanges, occRangeBytes } };
	int iAgg = -1, iPao = -1, iVao = -1, iGrp = -1, iVsky = -1, iVhor = -1;   //!< where the optional payloads landed in `tabs`, or -1
	if ( h.version == LODI_VERSION_AGGREGATE || ( v5 && h.aggregateCount ) ) {
		iAgg = int( tabs.size() );
		tabs.push_back( { "aggregate table", h.offAggregates,
			quint64( h.aggregateCount ) * sizeof( LodiAggregate ) } );
		tabs.push_back( { "covered-instance blob", h.offCovered,
			quint64( h.coveredCount ) * sizeof( quint32 ) } );
	}
	// v5: the AO blob is the LAST payload, one byte an instance
	if ( v5 ) {
		iPao = int( tabs.size() );
		tabs.push_back( { "placement-AO blob", h.offPlacementAo, quint64( h.placementAoCount ) } );
	}
	// v6: the vertex-AO blob after it
	if ( v6 ) {
		iVao = int( tabs.size() );
		tabs.push_back( { "vertex-AO blob", h.offVertexAo, quint64( h.vertexAoBytes ) } );
	}
	// v7: the group table then the sky stream, in the order the writer laid them
	if ( v7 && h.offGroup ) {
		iGrp = int( tabs.size() );
		tabs.push_back( { "group table", h.offGroup, quint64( h.instanceCount ) * sizeof( quint16 ) } );
	}
	if ( v7 && h.offVertexSky ) {
		iVsky = int( tabs.size() );
		tabs.push_back( { "vertex-sky stream", h.offVertexSky, quint64( h.vertexSkyBytes ) } );
	}
	if ( v8 && h.offVertexHorizon ) {
		iVhor = int( tabs.size() );
		tabs.push_back( { "vertex-horizon stream", h.offVertexHorizon, quint64( h.vertexHorizonBytes ) } );
	}
	quint64 prevEnd = headerBytes;
	for ( const Tab & t : tabs ) {
		if ( t.off % LODI_PAYLOAD_ALIGN )
			return refuse( QString( "%1 offset %2 is not 4,096-aligned" ).arg( t.name ).arg( t.off ) );
		if ( t.off < prevEnd )
			return refuse( QString( "%1 offset %2 is not in table order (previous payload ends at %3)" ).arg( t.name ).arg( t.off ).arg( prevEnd ) );
		/* NOT `t.off + t.bytes > h.fileBytes` (lane AUDIT1, 2026-09-17): that
		 * sum is two quint64 and it WRAPS, so a 4,096-aligned offset near 2^64
		 * passed the only bounds test this reader has, and the pad walk below
		 * then indexed p[i] from prevEnd all the way to it. Asked the other way
		 * round -- the size first, so the subtraction cannot go negative -- the
		 * same question cannot overflow. */
		if ( t.bytes > h.fileBytes || t.off > h.fileBytes - t.bytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );
		if ( payloadCheck )
			for ( quint64 i = prevEnd; i < t.off; i++ )
				if ( p[i] != 0 )
					return refuse( QString( "pad byte at %1 before the %2 is not zero" ).arg( i ).arg( t.name ) );
		prevEnd = t.off + t.bytes;
	}
	if ( payloadCheck ) {
		quint32 icrc = lodvCrc32( p + tabs[0].off, qsizetype( tabs[0].bytes ) );
		icrc = lodvCrc32( p + tabs[1].off, qsizetype( tabs[1].bytes ), icrc );
		icrc = lodvCrc32( p + tabs[4].off, qsizetype( tabs[4].bytes ), icrc );
		icrc = lodvCrc32( p + tabs[5].off, qsizetype( tabs[5].bytes ), icrc );
		if ( iAgg >= 0 ) {
			icrc = lodvCrc32( p + tabs[iAgg].off, qsizetype( tabs[iAgg].bytes ), icrc );
			icrc = lodvCrc32( p + tabs[iAgg + 1].off, qsizetype( tabs[iAgg + 1].bytes ), icrc );
		}
		// v5: the AO blob joins last, exactly as the writer folds it
		if ( iPao >= 0 )
			icrc = lodvCrc32( p + tabs[iPao].off, qsizetype( tabs[iPao].bytes ), icrc );
		if ( iVao >= 0 )
			icrc = lodvCrc32( p + tabs[iVao].off, qsizetype( tabs[iVao].bytes ), icrc );
		// v7: the group table then the sky stream, exactly as the writer folds them
		if ( iGrp >= 0 )
			icrc = lodvCrc32( p + tabs[iGrp].off, qsizetype( tabs[iGrp].bytes ), icrc );
		if ( iVsky >= 0 )
			icrc = lodvCrc32( p + tabs[iVsky].off, qsizetype( tabs[iVsky].bytes ), icrc );
		// v8: the horizon stream joins after the sky stream, as the writer folds it
		if ( iVhor >= 0 )
			icrc = lodvCrc32( p + tabs[iVhor].off, qsizetype( tabs[iVhor].bytes ), icrc );
		if ( icrc != h.indexCrc32 )
			return refuse( QString( "indexCrc32 0x%1 does not match the chunk table + cell ranges + occluders%2%3 (0x%4)" )
				.arg( h.indexCrc32, 8, 16, QChar( '0' ) )
				.arg( iAgg >= 0 ? QStringLiteral( " + aggregates + covered" ) : QString() )
				.arg( iPao >= 0 ? QStringLiteral( " + placement AO" ) : QString() )
				.arg( icrc, 8, 16, QChar( '0' ) ) );
	}

	LodiTable T;
	T.chunks.resize( h.chunkCount );
	T.cellRanges.resize( size_t( h.presentChunks ) * LODI_CHUNK_CELLS * LODI_CHUNK_CELLS );
	T.instances.resize( h.instanceCount );
	T.cold.resize( h.instanceCount );
	T.occluders.resize( h.occluderCount );
	T.occluderRanges.resize( size_t( h.presentChunks ) * LODI_CHUNK_CELLS * LODI_CHUNK_CELLS );
	if ( h.chunkCount ) std::memcpy( T.chunks.data(), p + h.offChunks, tabs[0].bytes );
	if ( cellBytes ) std::memcpy( T.cellRanges.data(), p + h.offCellRanges, tabs[1].bytes );
	if ( h.instanceCount ) std::memcpy( T.instances.data(), p + h.offInstances, tabs[2].bytes );
	if ( h.instanceCount ) std::memcpy( T.cold.data(), p + h.offCold, tabs[3].bytes );
	if ( h.occluderCount ) std::memcpy( T.occluders.data(), p + h.offOccluders, tabs[4].bytes );
	if ( occRangeBytes ) std::memcpy( T.occluderRanges.data(), p + h.offOccluderRanges, tabs[5].bytes );
	if ( iAgg >= 0 ) {
		T.aggregates.resize( h.aggregateCount );
		T.covered.resize( h.coveredCount );
		if ( h.aggregateCount ) std::memcpy( T.aggregates.data(), p + h.offAggregates, tabs[iAgg].bytes );
		if ( h.coveredCount ) std::memcpy( T.covered.data(), p + h.offCovered, tabs[iAgg + 1].bytes );
	}
	if ( iPao >= 0 ) {
		T.placementAo.resize( h.placementAoCount );
		if ( h.placementAoCount ) std::memcpy( T.placementAo.data(), p + h.offPlacementAo, tabs[iPao].bytes );
	}
	if ( iVao >= 0 ) {
		/* v6: the offsets are read and CHECKED -- monotone, and the last one
		 * lands exactly on the blob's end -- before a byte of AO is trusted. */
		const size_t nOff = size_t( h.instanceCount ) + 1;
		T.vertexAoFirst.resize( nOff );
		const unsigned char * vb = p + h.offVertexAo;
		for ( size_t i = 0; i < nOff; i++ ) {
			T.vertexAoFirst[i] = getLE<quint32>( vb + 4 * i );
			if ( i && T.vertexAoFirst[i] < T.vertexAoFirst[i - 1] )
				return refuse( QString( "vertex-AO offset %1 (%2) is below offset %3 (%4)" )
					.arg( i ).arg( T.vertexAoFirst[i] ).arg( i - 1 ).arg( T.vertexAoFirst[i - 1] ) );
		}
		const quint64 dataBytes = quint64( h.vertexAoBytes ) - 4ull * nOff;
		if ( T.vertexAoFirst[0] != 0 || quint64( T.vertexAoFirst[nOff - 1] ) != dataBytes )
			return refuse( QString( "vertex-AO offsets run %1..%2 but the blob carries %3 AO bytes after its %4 offsets" )
				.arg( T.vertexAoFirst[0] ).arg( T.vertexAoFirst[nOff - 1] ).arg( dataBytes ).arg( nOff ) );
		T.vertexAo.resize( size_t( dataBytes ) );
		if ( dataBytes ) std::memcpy( T.vertexAo.data(), vb + 4 * nOff, size_t( dataBytes ) );
	}
	if ( iGrp >= 0 ) {
		T.group.resize( h.instanceCount );
		if ( h.instanceCount ) std::memcpy( T.group.data(), p + h.offGroup, tabs[iGrp].bytes );
		/* THE DENSE RULE, and it is checked whether or not `payloadCheck` is on,
		 * because a group id out of its chunk's range is not a slow consistency
		 * question -- it is a value a viewer would hash into a colour. A chunk
		 * holding C groups uses exactly {0 .. C-1} and uses every one of them. */
		quint64 sum = 0;
		for ( size_t ci = 0; ci < T.chunks.size(); ci++ ) {
			const LodiChunk & c = T.chunks[ci];
			if ( c.instanceCount == 0 )
				continue;
			if ( quint64( c.instanceFirst ) + c.instanceCount > h.instanceCount )
				continue;   // the chunk walk below reports this one properly
			std::vector<bool> used( c.instanceCount, false );
			quint32 hi = 0;
			for ( quint32 i = 0; i < c.instanceCount; i++ ) {
				const quint16 g = T.group[c.instanceFirst + i];
				if ( g >= c.instanceCount )
					return refuse( QString( "group id %1 at instance %2 is past chunk %3's %4 placements; ids are "
						"dense per chunk from 0" ).arg( g ).arg( c.instanceFirst + i ).arg( ci ).arg( c.instanceCount ) );
				used[g] = true;
				hi = std::max( hi, quint32( g ) );
			}
			for ( quint32 g = 0; g <= hi; g++ )
				if ( !used[g] )
					return refuse( QString( "chunk %1 uses group ids up to %2 but never uses %3; ids are dense "
						"per chunk from 0" ).arg( ci ).arg( hi ).arg( g ) );
			sum += quint64( hi ) + 1;
		}
		if ( sum != quint64( h.groupCount ) )
			return refuse( QString( "groupCount %1 but the chunks' group counts sum to %2" )
				.arg( h.groupCount ).arg( sum ) );
	}
	if ( iVsky >= 0 ) {
		/* v7: the same three offset rules s4.8 states for the AO stream, asked
		 * of the sky stream in the same words. */
		const size_t nOff = size_t( h.instanceCount ) + 1;
		T.vertexSkyFirst.resize( nOff );
		const unsigned char * sb = p + h.offVertexSky;
		for ( size_t i = 0; i < nOff; i++ ) {
			T.vertexSkyFirst[i] = getLE<quint32>( sb + 4 * i );
			if ( i && T.vertexSkyFirst[i] < T.vertexSkyFirst[i - 1] )
				return refuse( QString( "vertex-sky offset %1 (%2) is below offset %3 (%4)" )
					.arg( i ).arg( T.vertexSkyFirst[i] ).arg( i - 1 ).arg( T.vertexSkyFirst[i - 1] ) );
		}
		const quint64 dataBytes = quint64( h.vertexSkyBytes ) - 4ull * nOff;
		if ( T.vertexSkyFirst[0] != 0 || quint64( T.vertexSkyFirst[nOff - 1] ) != dataBytes )
			return refuse( QString( "vertex-sky offsets run %1..%2 but the stream carries %3 sky bytes after its %4 offsets" )
				.arg( T.vertexSkyFirst[0] ).arg( T.vertexSkyFirst[nOff - 1] ).arg( dataBytes ).arg( nOff ) );
		T.vertexSky.resize( size_t( dataBytes ) );
		if ( dataBytes ) std::memcpy( T.vertexSky.data(), sb + 4 * nOff, size_t( dataBytes ) );
		/* The two streams stand for one vertex population. A placement with a
		 * sky slice and an AO slice of a different length means one of the two
		 * casts ran on a different mesh. */
		if ( iVao >= 0 )
			for ( size_t i = 0; i < size_t( h.instanceCount ); i++ ) {
				const quint32 sn = T.vertexSkyFirst[i + 1] - T.vertexSkyFirst[i];
				const quint32 an = T.vertexAoFirst[i + 1] - T.vertexAoFirst[i];
				if ( sn && sn != an )
					return refuse( QString( "instance %1 has %2 sky bytes but %3 AO bytes; the two streams are "
						"one vertex population" ).arg( i ).arg( sn ).arg( an ) );
			}
	}
	if ( iVhor >= 0 ) {
		/* v8's RETIRED per-vertex horizon stream, SKIPPED BY LENGTH (lane
		 * HORIZONOUT, 2026-09-19). The payload is never copied -- nothing in
		 * this tree consumes it -- but its offset table is still walked, because
		 * skipping a region is only honest if the region is the size it claims
		 * to be. The offsets live on the stack and die here; `LodiTable` carries
		 * no horizon vector at all.
		 *
		 * The stream's extent is ALSO still in `tabs`, so the payload sweep and
		 * `indexCrc32` still cover it and the note line still names it. A v8 file
		 * therefore opens, reports honestly what it holds, and is refused BY NAME
		 * if the stream is malformed -- it is never quietly mis-read. */
		const size_t nOff = size_t( h.instanceCount ) + 1;
		const unsigned char * hb = p + h.offVertexHorizon;
		std::vector<quint32> hfirst( nOff );
		for ( size_t i = 0; i < nOff; i++ ) {
			hfirst[i] = getLE<quint32>( hb + 4 * i );
			if ( i && hfirst[i] < hfirst[i - 1] )
				return refuse( QString( "vertex-horizon offset %1 (%2) is below offset %3 (%4)" )
					.arg( i ).arg( hfirst[i] ).arg( i - 1 ).arg( hfirst[i - 1] ) );
		}
		const quint64 dataBytes = quint64( h.vertexHorizonBytes ) - 4ull * nOff;
		if ( hfirst[0] != 0 || quint64( hfirst[nOff - 1] ) != dataBytes )
			return refuse( QString( "vertex-horizon offsets run %1..%2 but the stream carries %3 horizon bytes "
				"after its %4 offsets" ).arg( hfirst[0] ).arg( hfirst[nOff - 1] )
				.arg( dataBytes ).arg( nOff ) );
		/* One vertex population, A bytes each. A slice that is not exactly A
		 * times the AO slice means one of the two casts ran on a different
		 * mesh, and a consumer indexing vertex v at v * A would read another
		 * vertex's bins without ever knowing. */
		if ( iVao >= 0 )
			for ( size_t i = 0; i < size_t( h.instanceCount ); i++ ) {
				const quint32 hn = hfirst[i + 1] - hfirst[i];
				const quint32 an = T.vertexAoFirst[i + 1] - T.vertexAoFirst[i];
				if ( hn && hn != an * quint32( h.horizonAzimuths ) )
					return refuse( QString( "instance %1 has %2 horizon bytes but %3 AO bytes x %4 azimuths = %5; "
						"the two streams are one vertex population" ).arg( i ).arg( hn ).arg( an )
						.arg( h.horizonAzimuths ).arg( an * quint32( h.horizonAzimuths ) ) );
			}
	}

	if ( payloadCheck ) {
		quint32 present = 0, covered = 0, maxInst = 0, nextFirst = 0, occCursor = 0;
		for ( size_t ci = 0; ci < T.chunks.size(); ci++ ) {
			const LodiChunk & c = T.chunks[ci];
			if ( c.reserved )
				return refuse( QString( "chunk %1: reserved word is not zero" ).arg( ci ) );
			if ( c.instanceCount == 0 ) {
				static const LodiChunk zero = {};
				if ( std::memcmp( &c, &zero, sizeof( c ) ) != 0 )
					return refuse( QString( "chunk %1 is absent (0 instances) but not all-zero" ).arg( ci ) );
				continue;
			}
			if ( c.instanceFirst != nextFirst )
				return refuse( QString( "chunk %1: instanceFirst %2 but the previous present chunk ends at %3 (the blob must be in chunk order)" )
					.arg( ci ).arg( c.instanceFirst ).arg( nextFirst ) );
			if ( quint64( c.instanceFirst ) + c.instanceCount > h.instanceCount )
				return refuse( QString( "chunk %1: instances %2 + %3 past instanceCount %4" ).arg( ci ).arg( c.instanceFirst ).arg( c.instanceCount ).arg( h.instanceCount ) );
			if ( c.cellRangeOffset != present * LODI_CHUNK_CELLS * LODI_CHUNK_CELLS )
				return refuse( QString( "chunk %1: cellRangeOffset %2, expected %3 (16 per present chunk, table order)" )
					.arg( ci ).arg( c.cellRangeOffset ).arg( present * 16 ) );
			if ( !( c.zExtent >= 0.0f ) || !std::isfinite( c.zMin ) || !std::isfinite( c.zExtent ) )
				return refuse( QString( "chunk %1: z range %2 + %3 is not a range" ).arg( ci ).arg( double( c.zMin ) ).arg( double( c.zExtent ) ) );
			quint32 crc = lodvCrc32( p + h.offInstances + quint64( c.instanceFirst ) * sizeof( LodiInstance ), qsizetype( c.instanceCount * sizeof( LodiInstance ) ) );
			crc = lodvCrc32( p + h.offCold + quint64( c.instanceFirst ) * sizeof( LodiCold ), qsizetype( c.instanceCount * sizeof( LodiCold ) ), crc );
			if ( crc != c.crc32 )
				return refuse( QString( "chunk %1: crc32 0x%2 does not match its records (0x%3)" ).arg( ci ).arg( c.crc32, 8, 16, QChar( '0' ) ).arg( crc, 8, 16, QChar( '0' ) ) );
			// the 16 cell ranges partition the chunk, in cell order
			quint32 sum = 0, cursor = c.instanceFirst;
			/* Which cell row each instance of this chunk landed in: THE
			 * FILE'S OWN ANSWER, and the one the order check below reads.
			 * The writer sorted on the cell of the FLOAT position and a
			 * reader only ever has the quantised one, so re-deriving it
			 * here and arguing with the file is a rule stricter than the
			 * format; `lodiCellAgrees` is the check that stays. */
			std::vector<quint8> cellOfIndex( c.instanceCount, 0 );
			for ( int k = 0; k < LODI_CHUNK_CELLS * LODI_CHUNK_CELLS; k++ ) {
				const LodiCellRange & cr = T.cellRanges[c.cellRangeOffset + k];
				if ( cr.instanceCount == 0 )
					continue;
				if ( cr.instanceFirst != cursor )
					return refuse( QString( "chunk %1 cell %2: range starts at %3, expected %4 (cells partition the chunk in order)" ).arg( ci ).arg( k ).arg( cr.instanceFirst ).arg( cursor ) );
				for ( quint32 i = cr.instanceFirst; i < cr.instanceFirst + cr.instanceCount; i++ )
					if ( i >= c.instanceFirst && i - c.instanceFirst < c.instanceCount )
						cellOfIndex[i - c.instanceFirst] = quint8( k );
				cursor += cr.instanceCount;
				sum += cr.instanceCount;
			}
			if ( sum != c.instanceCount )
				return refuse( QString( "chunk %1: cell ranges sum to %2, chunk holds %3" ).arg( ci ).arg( sum ).arg( c.instanceCount ) );
			/* v3: the 16 OCCLUDER ranges walk the box table in the same cell
			 * order, and every box must name an instance OF ITS OWN CELL. That
			 * last rule is the one with teeth: a box is only an occluder
			 * because it sits inside a particular object, so a row that points
			 * at an object in another cell is a box in the wrong place. */
			for ( int k = 0; k < LODI_CHUNK_CELLS * LODI_CHUNK_CELLS; k++ ) {
				const LodiOccluderRange & orr = T.occluderRanges[c.cellRangeOffset + k];
				const LodiCellRange & cr = T.cellRanges[c.cellRangeOffset + k];
				if ( orr.occluderCount == 0 ) {
					if ( orr.occluderFirst != 0 )
						return refuse( QString( "chunk %1 cell %2: an empty occluder range must be all-zero, "
							"occluderFirst is %3" ).arg( ci ).arg( k ).arg( orr.occluderFirst ) );
					continue;
				}
				if ( orr.occluderCount > h.maxOccludersPerCell )
					return refuse( QString( "chunk %1 cell %2: %3 occluders, past the header's cap of %4" )
						.arg( ci ).arg( k ).arg( orr.occluderCount ).arg( h.maxOccludersPerCell ) );
				if ( orr.occluderFirst != occCursor )
					return refuse( QString( "chunk %1 cell %2: occluders start at %3, expected %4 (cells walk the "
						"box table in order)" ).arg( ci ).arg( k ).arg( orr.occluderFirst ).arg( occCursor ) );
				if ( quint64( orr.occluderFirst ) + orr.occluderCount > h.occluderCount )
					return refuse( QString( "chunk %1 cell %2: occluders %3 + %4 past occluderCount %5" )
						.arg( ci ).arg( k ).arg( orr.occluderFirst ).arg( orr.occluderCount ).arg( h.occluderCount ) );
				for ( quint32 b = orr.occluderFirst; b < orr.occluderFirst + orr.occluderCount; b++ ) {
					const LodiOccluder & o = T.occluders[b];
					if ( o.reserved )
						return refuse( QString( "occluder %1: reserved word is not zero" ).arg( b ) );
					if ( o.flags & ~quint16( 1 ) )
						return refuse( QString( "occluder %1: flags 0x%2 set reserved bits" ).arg( b ).arg( o.flags, 0, 16 ) );
					if ( !( o.flags & 1 ) )
						return refuse( QString( "occluder %1: bit0 is clear, so the file does not say what rule "
							"fitted this box" ).arg( b ) );
					for ( int k2 = 0; k2 < 3; k2++ ) {
						if ( !std::isfinite( o.centre[k2] ) )
							return refuse( QString( "occluder %1: centre is not finite" ).arg( b ) );
						if ( !( o.halfExtent[k2] > 0.0f ) || !std::isfinite( o.halfExtent[k2] ) )
							return refuse( QString( "occluder %1: half extent %2 on axis %3 is not positive; a box "
								"with no thickness occludes nothing" ).arg( b ).arg( double( o.halfExtent[k2] ) ).arg( k2 ) );
					}
					if ( o.instanceIndex >= h.instanceCount )
						return refuse( QString( "occluder %1: instanceIndex %2 past instanceCount %3" )
							.arg( b ).arg( o.instanceIndex ).arg( h.instanceCount ) );
					if ( o.instanceIndex < cr.instanceFirst || o.instanceIndex >= cr.instanceFirst + cr.instanceCount )
						return refuse( QString( "occluder %1 is listed in chunk %2 cell %3 but names instance %4, "
							"which is not one of that cell's %5 instances at %6" ).arg( b ).arg( ci ).arg( k )
							.arg( o.instanceIndex ).arg( cr.instanceCount ).arg( cr.instanceFirst ) );
				}
				occCursor += orr.occluderCount;
			}
			// rows: reserved, flags, the maxBoundRadius law, the (ref, part) order
			int cx, cy;
			lodiChunkAt( h, quint32( ci ), &cx, &cy );
			for ( quint32 i = c.instanceFirst; i < c.instanceFirst + c.instanceCount; i++ ) {
				const LodiInstance & r = T.instances[i];
				if ( r.flags & ~LODI_INST_FLAGS_KNOWN )
					return refuse( QString( "instance %1: flags 0x%2 set reserved bits" ).arg( i ).arg( r.flags, 0, 16 ) );
				/* v9: below version 9 bit 6 is RESERVED-ZERO, and a reader that
				 * shrugged at it would read a file written by a newer bake as
				 * though the bit meant nothing. The version word is the only
				 * thing that distinguishes the two files, so it is the only
				 * thing there is to check. */
				if ( ( r.flags & LODI_INST_SCRAPPABLE ) && h.version < LODI_VERSION_SCRAPPABLE )
					return refuse( QString( "instance %1 carries the scrappable bit (0x%2) in a version-%3 file, "
						"where that bit is reserved zero; version %4 is the one that means it" )
						.arg( i ).arg( int( LODI_INST_SCRAPPABLE ), 0, 16 ).arg( h.version )
						.arg( LODI_VERSION_SCRAPPABLE ) );
				// v10: bit 7 is reserved zero below version 10, for the same reason
				if ( ( r.flags & LODI_INST_SCALE_WIDE ) && h.version < LODI_VERSION_WIDE_SCALE )
					return refuse( QString( "instance %1 carries the wide-scale bit (0x%2) in a version-%3 file, "
						"where that bit is reserved zero; version %4 is the one that means it" )
						.arg( i ).arg( int( LODI_INST_SCALE_WIDE ), 0, 16 ).arg( h.version )
						.arg( LODI_VERSION_WIDE_SCALE ) );
				// v11: bit 8 is reserved zero below version 11, for the same reason
				if ( ( r.flags & LODI_INST_INITIALLY_DISABLED ) && h.version < LODI_VERSION_INITIALLY_DISABLED )
					return refuse( QString( "instance %1 carries the initially-disabled bit (0x%2) in a version-%3 file, "
						"where that bit is reserved zero; version %4 is the one that means it" )
						.arg( i ).arg( int( LODI_INST_INITIALLY_DISABLED ), 0, 16 ).arg( h.version )
						.arg( LODI_VERSION_INITIALLY_DISABLED ) );
				/* bungo 2026-09-11 08:0x item 3, the per-instance bound radius:
				 * the runtime takes base.boundRadius x scale, so a zero scale
				 * is a zero radius and the object is culled at every distance.
				 * The base table already refuses boundRadius 0; this is the
				 * other half, and it is a REFUSAL, not a clamp. */
				if ( r.scale == 0 && !( r.flags & LODI_INST_SCALE_WIDE ) )
					return refuse( QString( "instance %1 (ref 0x%2): scale is 0, so base.boundRadius x scale is 0 "
						"and the screen-size test can never select it" ).arg( i ).arg( T.cold[i].refFormId, 8, 16, QChar( '0' ) ) );
				/* THE CELL AN INSTANCE IS IN is the one the cell-range table
				 * states; the stored position only CHECKS it, inside the
				 * quantiser's ambiguity band (lodiCellAgrees, lodifile.h). */
				const int kb = int( cellOfIndex[i - c.instanceFirst] );
				{
					float pb[3];
					lodiDecodePosition( h, quint32( ci ), c, r, pb );
					QString why;
					if ( !lodiCellAgrees( kb, pb[0], pb[1], cx, cy, &why ) )
						return refuse( QString( "instance %1 (ref 0x%2): %3" ).arg( i )
							.arg( T.cold[i].refFormId, 8, 16, QChar( '0' ) ).arg( why ) );
				}
				if ( i > c.instanceFirst ) {
					const LodiCold & a = T.cold[i - 1];
					const LodiCold & b = T.cold[i];
					const int ka = int( cellOfIndex[i - 1 - c.instanceFirst] );
					if ( std::make_tuple( ka, T.instances[i - 1].drawKey, a.refFormId, a.scolPart )
						> std::make_tuple( kb, r.drawKey, b.refFormId, b.scolPart ) )
						return refuse( QString( "instance %1: out of (cell, drawKey, ref, part) order after instance %2" ).arg( i ).arg( i - 1 ) );
				}
			}
			present++;
			covered += c.instanceCount;
			nextFirst = c.instanceFirst + c.instanceCount;
			maxInst = std::max( maxInst, c.instanceCount );
		}
		if ( present != h.presentChunks )
			return refuse( QString( "presentChunks %1 but %2 chunks hold instances" ).arg( h.presentChunks ).arg( present ) );
		if ( covered != h.instanceCount )
			return refuse( QString( "chunks cover %1 instances of %2" ).arg( covered ).arg( h.instanceCount ) );
		if ( maxInst != h.maxInstancesPerChunk )
			return refuse( QString( "maxInstancesPerChunk %1 but the largest chunk holds %2" ).arg( h.maxInstancesPerChunk ).arg( maxInst ) );

		/* v4: THE AGGREGATES. Every rule here has teeth because the runtime
		 * SUPPRESSES the covered instances: a wrong covered list hides trees
		 * that nothing draws in their place. */
		/* THE AGGREGATE PAYLOAD IS GATED BY THE AGGREGATE, NOT BY THE VERSION
		 * WORD (lane AUDIT1, 2026-09-17). `h.aggregateCount != 0` is exactly
		 * the question the tabs builder asks as `iAgg >= 0`: v3 never reads the
		 * word (0), v4 refuses a zero count by name, v5 carries the count.
		 * Behind `version == 4` every rule below -- the identity bit, the cell
		 * order, the coveredFirst partition, the index range and the double
		 * cover -- went silent on every default bake the moment placement AO
		 * made the files version 5. */
		if ( h.aggregateCount ) {
			std::vector<quint8> claimed( h.instanceCount, 0 );
			quint32 cursor = 0;
			for ( size_t ai = 0; ai < T.aggregates.size(); ai++ ) {
				const LodiAggregate & a = T.aggregates[ai];
				if ( a.flags & ~LODI_AGG_FLAGS_KNOWN )
					return refuse( QString( "aggregate %1: flags 0x%2 set reserved bits" ).arg( ai ).arg( a.flags, 0, 16 ) );
				if ( !( a.flags & LODI_AGG_HEIGHT ) )
					return refuse( QString( "aggregate %1 (cell %2, %3): HEIGHT is clear; every aggregate sheet carries "
						"height, because the far shadows are cast from it" ).arg( ai ).arg( a.cellX ).arg( a.cellY ) );
				if ( a.identity != ( LODI_AGG_IDENTITY_BIT | quint32( ai ) ) )
					return refuse( QString( "aggregate %1 (cell %2, %3): identity 0x%4, expected 0x%5 -- one identity per "
						"aggregate, in the top-bit-set space so it can never collide with an instance index" )
						.arg( ai ).arg( a.cellX ).arg( a.cellY ).arg( a.identity, 8, 16, QChar( '0' ) )
						.arg( LODI_AGG_IDENTITY_BIT | quint32( ai ), 8, 16, QChar( '0' ) ) );
				if ( a.views != h.aggregateViews )
					return refuse( QString( "aggregate %1: %2 views against the header's %3" )
						.arg( ai ).arg( a.views ).arg( h.aggregateViews ) );
				if ( !( a.half[0] > 0.0f ) || !( a.half[1] > 0.0f ) )
					return refuse( QString( "aggregate %1 (cell %2, %3): half extent is not positive" )
						.arg( ai ).arg( a.cellX ).arg( a.cellY ) );
				if ( !( a.depthSpan > 0.0f ) )
					return refuse( QString( "aggregate %1 (cell %2, %3): depthSpan is not positive, so the height "
						"channel decodes to nothing" ).arg( ai ).arg( a.cellX ).arg( a.cellY ) );
				if ( !( a.boundRadius > 0.0f ) )
					return refuse( QString( "aggregate %1 (cell %2, %3): boundRadius is not positive, so the "
						"screen-size test can never select it" ).arg( ai ).arg( a.cellX ).arg( a.cellY ) );
				for ( int k = 0; k < 3; k++ )
					if ( !std::isfinite( a.centre[k] ) )
						return refuse( QString( "aggregate %1: centre is not finite" ).arg( ai ) );
				if ( ai ) {
					const LodiAggregate & prev = T.aggregates[ai - 1];
					if ( std::make_tuple( -int( prev.cellY ), int( prev.cellX ) )
						>= std::make_tuple( -int( a.cellY ), int( a.cellX ) ) )
						return refuse( QString( "aggregate %1 (cell %2, %3) is not after aggregate %4 (cell %5, %6) in "
							"north-up cell order, or repeats its cell" ).arg( ai ).arg( a.cellX ).arg( a.cellY )
							.arg( ai - 1 ).arg( prev.cellX ).arg( prev.cellY ) );
				}
				if ( a.coveredCount == 0 )
					return refuse( QString( "aggregate %1 (cell %2, %3) stands for no instance" )
						.arg( ai ).arg( a.cellX ).arg( a.cellY ) );
				if ( a.coveredFirst != cursor )
					return refuse( QString( "aggregate %1: coveredFirst %2, expected %3 (the aggregates partition the "
						"covered blob in order)" ).arg( ai ).arg( a.coveredFirst ).arg( cursor ) );
				if ( quint64( a.coveredFirst ) + a.coveredCount > h.coveredCount )
					return refuse( QString( "aggregate %1: covered %2 + %3 past coveredCount %4" )
						.arg( ai ).arg( a.coveredFirst ).arg( a.coveredCount ).arg( h.coveredCount ) );
				for ( quint32 j = a.coveredFirst; j < a.coveredFirst + a.coveredCount; j++ ) {
					const quint32 idx = T.covered[j];
					if ( idx >= h.instanceCount )
						return refuse( QString( "aggregate %1: covered instance %2 past instanceCount %3" )
							.arg( ai ).arg( idx ).arg( h.instanceCount ) );
					if ( j > a.coveredFirst && idx <= T.covered[j - 1] )
						return refuse( QString( "aggregate %1: covered instance %2 is not after %3 (the list is "
							"ascending so a consumer can bisect it)" ).arg( ai ).arg( idx ).arg( T.covered[j - 1] ) );
					if ( claimed[idx] )
						return refuse( QString( "aggregate %1: instance %2 is already covered by another aggregate; "
							"a doubly covered instance would be suppressed twice" ).arg( ai ).arg( idx ) );
					claimed[idx] = 1;
					/* THE RULE WITH TEETH: an aggregate stands for ITS OWN
					 * cell's trees. A covered instance somewhere else is a
					 * forest being hidden by a card that does not draw it. */
					const quint32 cci = [&]() {
						for ( size_t c2 = 0; c2 < T.chunks.size(); c2++ )
							if ( T.chunks[c2].instanceCount && idx >= T.chunks[c2].instanceFirst
								&& idx < T.chunks[c2].instanceFirst + T.chunks[c2].instanceCount )
								return quint32( c2 );
						return quint32( 0xFFFFFFFFU );
					}();
					if ( cci == 0xFFFFFFFFU )
						return refuse( QString( "aggregate %1: covered instance %2 is in no chunk" ).arg( ai ).arg( idx ) );
					int cx2, cy2;
					lodiChunkAt( h, cci, &cx2, &cy2 );
					float pw[3];
					lodiDecodePosition( h, cci, T.chunks[cci], T.instances[idx], pw );
					const int icx = int( std::floor( pw[0] / LODI_CELL_UNITS ) );
					const int icy = int( std::floor( pw[1] / LODI_CELL_UNITS ) );
					if ( icx != a.cellX || icy != a.cellY )
						return refuse( QString( "aggregate %1 (cell %2, %3) covers instance %4, which stands in cell "
							"(%5, %6)" ).arg( ai ).arg( a.cellX ).arg( a.cellY ).arg( idx ).arg( icx ).arg( icy ) );
				}
				cursor += a.coveredCount;
			}
			if ( cursor != h.coveredCount )
				return refuse( QString( "the aggregates cover %1 entries of the blob's %2" ).arg( cursor ).arg( h.coveredCount ) );
		}
	}
	if ( header )
		*header = h;
	if ( table )
		*table = std::move( T );
	return true;
}

QStringList lodiDescribe( const LodiHeader & h, const LodiTable * table )
{
	QStringList out;
	out << QString( "magic LODI" ) << QString( "version %1" ).arg( h.version )
		<< QString( "flags 0x%1" ).arg( h.flags, 0, 16 )
		<< QString( "headerCrc32 0x%1" ).arg( h.headerCrc32, 8, 16, QChar( '0' ) )
		<< QString( "pluginCorpusHash 0x%1" ).arg( h.pluginCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "objectCorpusHash 0x%1" ).arg( h.objectCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "lodoIdentity 0x%1" ).arg( h.lodoIdentity, 16, 16, QChar( '0' ) )
		<< QString( "loadOrderHash 0x%1" ).arg( h.loadOrderHash, 16, 16, QChar( '0' ) )
		<< QString( "worldspace %1" ).arg( h.worldspaceEdid )
		<< QString( "chunkWest %1" ).arg( h.chunkWest ) << QString( "chunkSouth %1" ).arg( h.chunkSouth )
		<< QString( "chunkEast %1" ).arg( h.chunkEast ) << QString( "chunkNorth %1" ).arg( h.chunkNorth )
		<< QString( "chunkCells %1" ).arg( h.chunkCells ) << QString( "instanceStride %1" ).arg( h.instanceStride )
		<< QString( "chunkCount %1" ).arg( h.chunkCount ) << QString( "instanceCount %1" ).arg( h.instanceCount )
		<< QString( "presentChunks %1" ).arg( h.presentChunks ) << QString( "maxInstancesPerChunk %1" ).arg( h.maxInstancesPerChunk )
		<< QString( "indexCrc32 0x%1" ).arg( h.indexCrc32, 8, 16, QChar( '0' ) )
		<< QString( "offChunks %1" ).arg( h.offChunks ) << QString( "offCellRanges %1" ).arg( h.offCellRanges )
		<< QString( "offInstances %1" ).arg( h.offInstances ) << QString( "offCold %1" ).arg( h.offCold )
		<< QString( "offOccluders %1" ).arg( h.offOccluders )
		<< QString( "offOccluderRanges %1" ).arg( h.offOccluderRanges )
		<< QString( "occluderCount %1" ).arg( h.occluderCount )
		<< QString( "slotInstances0 %1" ).arg( h.slotInstances[0] )
		<< QString( "slotInstances1 %1" ).arg( h.slotInstances[1] )
		<< QString( "slotInstances2 %1" ).arg( h.slotInstances[2] )
		<< QString( "slotInstances3 %1" ).arg( h.slotInstances[3] )
		<< QString( "offPlacementAo %1" ).arg( h.offPlacementAo )
		<< QString( "placementAoCount %1" ).arg( h.placementAoCount )
		<< QString( "placementAoStride %1" ).arg( h.placementAoStride )
		<< QString( "offVertexAo %1" ).arg( h.offVertexAo )
		<< QString( "vertexAoBytes %1" ).arg( h.vertexAoBytes )
		<< QString( "offGroup %1" ).arg( h.offGroup )
		<< QString( "groupCount %1" ).arg( h.groupCount )
		<< QString( "groupStride %1" ).arg( h.groupStride )
		<< QString( "offVertexSky %1" ).arg( h.offVertexSky )
		<< QString( "vertexSkyBytes %1" ).arg( h.vertexSkyBytes )
		<< QString( "offVertexHorizon %1" ).arg( h.offVertexHorizon )
		<< QString( "vertexHorizonBytes %1" ).arg( h.vertexHorizonBytes )
		<< QString( "horizonAzimuths %1" ).arg( h.horizonAzimuths )
		<< QString( "horizonSteps %1" ).arg( h.horizonSteps )
		<< QString( "horizonReach %1" ).arg( double( h.horizonReach ), 0, 'f', 1 )
		<< QString( "occluderStride %1" ).arg( h.occluderStride )
		<< QString( "maxOccludersPerCell %1" ).arg( h.maxOccludersPerCell )
		<< QString( "offAggregates %1" ).arg( h.offAggregates )
		<< QString( "offCovered %1" ).arg( h.offCovered )
		<< QString( "aggregateCount %1" ).arg( h.aggregateCount )
		<< QString( "coveredCount %1" ).arg( h.coveredCount )
		/* v4 AND v5 (lane AUDIT1, 2026-09-17): both read the word at 0xC8, so
		 * both report it. NOT an unguarded `h.aggregateStride` -- the field
		 * defaults to LODI_AGGREGATE_STRIDE rather than to 0, so a version-3
		 * file, which never reads the word, would report 48 out of thin air.
		 * Its five siblings below print straight because their defaults are 0. */
		<< QString( "aggregateStride %1" ).arg(
			( h.version == LODI_VERSION_AGGREGATE || h.version >= LODI_VERSION_PLACEMENT_AO )
				? h.aggregateStride : quint16( 0 ) )
		<< QString( "aggregateViews %1" ).arg( h.aggregateViews )
		<< QString( "aggSwitchPx %1" ).arg( double( h.aggSwitchPx ), 0, 'f', 3 )
		<< QString( "aggBandRatio %1" ).arg( double( h.aggBandRatio ), 0, 'f', 3 )
		<< QString( "fileBytes %1" ).arg( h.fileBytes );
	if ( table ) {
		quint32 scol = 0, trees = 0;
		for ( size_t i = 0; i < table->cold.size(); i++ ) {
			if ( table->cold[i].scolPart >= 0 )
				scol++;
			if ( table->instances[i].flags & LODI_INST_MIRRORED )
				trees++;
		}
		QSet<quint16> keys, ids;
		quint16 maxKey = 0;
		for ( size_t i = 0; i < table->instances.size(); i++ ) {
			keys.insert( table->instances[i].drawKey );
			maxKey = std::max( maxKey, table->instances[i].drawKey );
			ids.insert( table->cold[i].identity );
		}
		quint32 cellsWithOcc = 0, cellsPop = 0;
		for ( size_t s = 0; s < table->occluderRanges.size() && s < table->cellRanges.size(); s++ ) {
			if ( table->cellRanges[s].instanceCount )
				cellsPop++;
			if ( table->occluderRanges[s].occluderCount )
				cellsWithOcc++;
		}
		double occVol = 0.0;
		for ( const LodiOccluder & o : table->occluders )
			occVol += 8.0 * double( o.halfExtent[0] ) * double( o.halfExtent[1] ) * double( o.halfExtent[2] );
		out << QString( "scolParts %1" ).arg( scol ) << QString( "mirrored %1" ).arg( trees )
			<< QString( "distinctDrawKeys %1" ).arg( keys.size() ) << QString( "maxDrawKey %1" ).arg( maxKey )
			<< QString( "distinctIdentities %1" ).arg( ids.size() )
			<< QString( "cellsPopulated %1" ).arg( cellsPop )
			<< QString( "cellsWithOccluder %1" ).arg( cellsWithOcc )
			<< QString( "occludersPerPopulatedCell %1" ).arg( cellsPop
				? double( table->occluders.size() ) / double( cellsPop ) : 0.0, 0, 'f', 3 )
			<< QString( "meanOccluderVolume %1" ).arg( table->occluders.empty() ? 0.0
				: occVol / double( table->occluders.size() ), 0, 'f', 1 );
		/* v4: the aggregates, as numbers that MOVE. `aggregateCoveredMin/Max`
		 * are the count-identity gate's own readings -- the bake states how many
		 * trees it photographed into each sheet and these are what the FILE
		 * says, read back from the bytes. */
		quint32 covMin = 0, covMax = 0;
		double covSum = 0.0, halfWSum = 0.0;
		for ( size_t i = 0; i < table->aggregates.size(); i++ ) {
			const LodiAggregate & a = table->aggregates[i];
			covMin = i ? std::min( covMin, a.coveredCount ) : a.coveredCount;
			covMax = std::max( covMax, a.coveredCount );
			covSum += a.coveredCount;
			halfWSum += a.half[0];
		}
		out << QString( "aggregates %1" ).arg( table->aggregates.size() )
			<< QString( "aggregateCovered %1" ).arg( table->covered.size() )
			<< QString( "aggregateCoveredMin %1" ).arg( covMin )
			<< QString( "aggregateCoveredMax %1" ).arg( covMax )
			<< QString( "aggregateCoveredMean %1" ).arg( table->aggregates.empty() ? 0.0
				: covSum / double( table->aggregates.size() ), 0, 'f', 2 )
			<< QString( "aggregateMeanHalfW %1" ).arg( table->aggregates.empty() ? 0.0
				: halfWSum / double( table->aggregates.size() ), 0, 'f', 1 );
		/* v5: the placement-AO blob, as numbers that MOVE. 0xFF is NOT AO 255,
		 * it is NOT MEASURED, so it is counted apart and kept out of the mean. */
		quint32 aoMeasured = 0, aoUnmeasured = 0, aoMin = 0, aoMax = 0;
		double aoSum = 0.0;
		for ( quint8 v : table->placementAo ) {
			if ( v == LODI_PLACEMENT_AO_UNMEASURED ) { aoUnmeasured++; continue; }
			aoMin = aoMeasured ? std::min<quint32>( aoMin, v ) : v;
			aoMax = std::max<quint32>( aoMax, v );
			aoSum += v;
			aoMeasured++;
		}
		out << QString( "placementAoMeasured %1" ).arg( aoMeasured )
			<< QString( "placementAoUnmeasured %1" ).arg( aoUnmeasured )
			<< QString( "placementAoMin %1" ).arg( aoMin )
			<< QString( "placementAoMax %1" ).arg( aoMax )
			<< QString( "placementAoMean %1" ).arg( aoMeasured ? aoSum / double( aoMeasured ) : 0.0, 0, 'f', 2 );
		/* v6: the vertex-AO blob as numbers that MOVE: instances with a range,
		 * bytes, the mean, and how many bytes are below 128. */
		{
			quint32 withRange = 0, dark = 0;
			double vSum = 0.0;
			for ( size_t i = 0; i + 1 < table->vertexAoFirst.size(); i++ )
				if ( table->vertexAoFirst[i + 1] > table->vertexAoFirst[i] )
					withRange++;
			for ( quint8 v : table->vertexAo ) {
				vSum += v;
				if ( v < 128 )
					dark++;
			}
			out << QString( "vertexAoInstances %1" ).arg( withRange )
				<< QString( "vertexAoBytesTotal %1" ).arg( table->vertexAo.size() )
				<< QString( "vertexAoMean %1" ).arg( table->vertexAo.empty() ? 0.0 : vSum / double( table->vertexAo.size() ), 0, 'f', 2 )
				<< QString( "vertexAoDark %1" ).arg( dark );
		}
		/* v7 (a): the group table. `groups` is the header's own word; the other
		 * three are counted here from the ids, so a table that says one thing
		 * and holds another shows as a disagreement rather than as silence. */
		if ( !table->group.empty() ) {
			quint32 grouped = 0, largest = 0, singletons = 0;
			for ( const LodiChunk & c : table->chunks ) {
				if ( c.instanceCount == 0 || quint64( c.instanceFirst ) + c.instanceCount > table->group.size() )
					continue;
				std::vector<quint32> members;
				for ( quint32 i = 0; i < c.instanceCount; i++ ) {
					const quint16 g = table->group[c.instanceFirst + i];
					if ( g >= members.size() )
						members.resize( size_t( g ) + 1, 0 );
					members[g]++;
				}
				for ( quint32 m : members ) {
					largest = std::max( largest, m );
					if ( m == 1 )
						singletons++;
					else
						grouped += m;
				}
			}
			out << QString( "groups %1" ).arg( h.groupCount )
				<< QString( "groupedPlacements %1" ).arg( grouped )
				<< QString( "largestGroup %1" ).arg( largest )
				<< QString( "singletonGroups %1" ).arg( singletons );
		}
		/* v7 (b): the vertex-sky stream, the same four numbers s4.8's stream
		 * reports, plus the mean the 0x11 byte is compared against. */
		if ( !table->vertexSkyFirst.empty() ) {
			quint32 withRange = 0, open = 0;
			double sSum = 0.0;
			for ( size_t i = 0; i + 1 < table->vertexSkyFirst.size(); i++ )
				if ( table->vertexSkyFirst[i + 1] > table->vertexSkyFirst[i] )
					withRange++;
			for ( quint8 v : table->vertexSky ) {
				sSum += v;
				if ( v >= 128 )
					open++;
			}
			out << QString( "vertexSkyPlacements %1" ).arg( withRange )
				<< QString( "vertexSkyBytesTotal %1" ).arg( table->vertexSky.size() )
				<< QString( "vertexSkyMean %1" ).arg( table->vertexSky.empty() ? 0.0 : sSum / double( table->vertexSky.size() ), 0, 'f', 2 )
				<< QString( "vertexSkyOpen %1" ).arg( open );
		}
		/* v8's retired horizon stream is not read into the table any more (lane
		 * HORIZONOUT), so there is nothing here to read back. The five header
		 * words above still print, which is what tells a person a v8 file in
		 * front of them carries one. */
	}
	return out;
}

/* ---------------------------------------------------------------- fixture */

namespace
{
void matMul( const float a[9], const float b[9], float out[9] )
{
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			out[r * 3 + c] = a[r * 3] * b[c] + a[r * 3 + 1] * b[3 + c] + a[r * 3 + 2] * b[6 + c];
}
//! The same euler composition as Matrix::fromEuler (src/data/niftypes.cpp).
void fromEuler( float x, float y, float z, float m[9] )
{
	const float sx = std::sin( x ), cx = std::cos( x ), sy = std::sin( y ), cy = std::cos( y ), sz = std::sin( z ), cz = std::cos( z );
	m[0] = cy * cz; m[1] = -cy * sz; m[2] = sy;
	m[3] = sx * sy * cz + sz * cx; m[4] = cx * cz - sx * sy * sz; m[5] = -sx * cy;
	m[6] = sx * sz - cx * sy * cz; m[7] = cx * sy * sz + sx * cz; m[8] = cx * cy;
}
}

bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QDir().mkpath( dir );
	QStringList exp;
	auto E = [&]( const QString & k, const QString & v ) { exp << QString( "expect %1 %2" ).arg( k, v ); };

	/* The library: mesh A = a unit cube (8 vertices, 12 triangles: one
	 * cluster), mesh B = a 20-triangle strip (22 vertices: 16 + 4 triangles
	 * = two clusters, the second holding 4 = size class 0). Two materials. */
	LodoLibrary lib;
	lib.worldspaceEdid = QStringLiteral( "Synthetic" );
	lib.pluginCorpusHash = Q_UINT64_C( 0x1111111111111111 );
	lib.objectCorpusHash = Q_UINT64_C( 0x2222222222222222 );
	lib.modelCorpusHash = Q_UINT64_C( 0x3333333333333333 );
	lib.cardCorpusHash = 0;
	/* v2: a known load-order hash. The fixture library deliberately does NOT
	 * set LODO_FLAG_CACHE_ORDER -- its vertex expectations are hand-derived
	 * from the source order and a permutation would make them underivable.
	 * The cache order is proved on the real worldspace bake instead, per mesh,
	 * with a floor (tests/spells/lodgen_native_fields.py). */
	lib.loadOrderHash = Q_UINT64_C( 0x4444444444444444 );
	/* v3: the LADDER is on, so the fixture exercises the grouping, the locked
	 * border, the error rule and the re-split. Its level-0 answers stay
	 * hand-derived; the levels above it are checked by INVARIANTS (monotone
	 * errors, the subtree counts summing to the level-0 triangle count, every
	 * triangle inside its sphere, every normal inside its cone), because no
	 * hand can derive a simplifier's output and pretending otherwise would make
	 * the fixture a copy of the writer. */
	lib.flags |= LODO_FLAG_LADDER;
	/* v4 (lane NATIVE1c): the fixture turns the FOLIAGE REFUSAL OFF, and here
	 * is why.  The only fixture mesh big enough to ladder is the cube, and the
	 * cube is deliberately a TREE -- it carries the sway bytes and the
	 * alpha-tested material the v2 fixture checks by hand.  With the v4
	 * default the cube is refused, the fixture levelMax drops to 0, and every
	 * ladder answer in the known-answer pair goes vacuous: the grouping, the
	 * locked border, the error rule, the re-split, the cones and the subtree
	 * partition would all pass by having nothing left to check.  So the
	 * fixture keeps its ladder, and the refusal is gated where a tree really
	 * is one -- on the real region, by tests/spells/lodgen_ladder.sh section
	 * 2, which measures the refusal moving off zero AND runs
	 * --native-ladder-foliage as its refuter.
	 */
	lib.ladderFoliage = true;
	/* And the SILHOUETTE GATE off with it, for the same reason and with a
	 * number: the cube is twelve triangles, and its one level keeps 0.2500 of
	 * level 0's outline -- measured on the fixture by the independent gate,
	 * tests/spells/lodgen_silhouette.py -- against a v4 floor of 0.70, so the
	 * gate legitimately retires the fixture ladder as well.  On the real
	 * library that same script rasterises the .lodo bytes itself and floors
	 * the number with a random-vertex-drop twin matched on triangle count.
	 * 0.0 is the documented way back, and this is its first use. */
	lib.silhouetteMin = 0.0f;
	lib.addString( QString() );
	{
		LodoMaterial m0, m1;
		std::memset( &m0, 0, sizeof( m0 ) );
		std::memset( &m1, 0, sizeof( m1 ) );
		m0.layer = LODO_NO_LAYER; m0.family = LODO_FAMILY_LEGACY; m0.alphaThreshold = 0;
		m0.lodmStringOffset = lib.addString( QStringLiteral( "textures\\lod\\synthetic_opaque_d.dds" ) );
		m1.layer = LODO_NO_LAYER; m1.family = LODO_FAMILY_LEGACY; m1.alphaThreshold = 128;
		m1.lodmStringOffset = lib.addString( QStringLiteral( "textures\\lod\\synthetic_tree_d.dds" ) );
		lib.materials.push_back( m0 );
		lib.materials.push_back( m1 );
	}
	quint16 meshA = 0, meshB = 0;
	{
		LodoSrcShape cube;
		const float c[8][3] = { { -50, -50, 0 }, { 50, -50, 0 }, { 50, 50, 0 }, { -50, 50, 0 },
			{ -50, -50, 200 }, { 50, -50, 200 }, { 50, 50, 200 }, { -50, 50, 200 } };
		for ( int v = 0; v < 8; v++ ) {
			for ( int k = 0; k < 3; k++ )
				cube.pos.push_back( c[v][k] );
			const float l = std::sqrt( c[v][0] * c[v][0] + c[v][1] * c[v][1] + ( c[v][2] - 100.0f ) * ( c[v][2] - 100.0f ) );
			cube.nrm.push_back( c[v][0] / l ); cube.nrm.push_back( c[v][1] / l ); cube.nrm.push_back( ( c[v][2] - 100.0f ) / l );
			cube.tan.push_back( 0.0f ); cube.tan.push_back( 0.0f ); cube.tan.push_back( 1.0f );
			cube.uv.push_back( float( v & 1 ) * 1.5f - 0.25f ); cube.uv.push_back( float( ( v >> 1 ) & 1 ) );
			cube.sway.push_back( quint8( c[v][2] > 100.0f ? 200 : 0 ) );
		}
		const quint32 t[12][3] = { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 },
			{ 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 } };
		for ( auto & tri : t )
			for ( quint32 idx : tri )
				cube.tris.push_back( idx );
		cube.materialId = 1;
		QString err;
		if ( !lodoAppendMesh( lib, { cube }, QStringLiteral( "meshes\\lod\\synthetic\\a_tree_lod.nif" ), &meshA, &err ) )
			return fail( err );
		LodoSrcShape strip;
		for ( int v = 0; v < 22; v++ ) {
			strip.pos.push_back( float( v / 2 ) * 10.0f ); strip.pos.push_back( float( v & 1 ) * 10.0f ); strip.pos.push_back( 0.0f );
			strip.nrm.push_back( 0.0f ); strip.nrm.push_back( 0.0f ); strip.nrm.push_back( 1.0f );
			strip.tan.push_back( 1.0f ); strip.tan.push_back( 0.0f ); strip.tan.push_back( 0.0f );
			strip.uv.push_back( float( v / 2 ) * 0.1f ); strip.uv.push_back( float( v & 1 ) );
		}
		for ( quint32 v = 0; v + 2 < 22; v++ ) {
			strip.tris.push_back( v ); strip.tris.push_back( v + 1 + ( v & 1 ) ); strip.tris.push_back( v + 2 - ( v & 1 ) );
		}
		strip.materialId = 0;
		if ( !lodoAppendMesh( lib, { strip }, QStringLiteral( "meshes\\lod\\synthetic\\b_static_lod.nif" ), &meshB, &err ) )
			return fail( err );
	}
	// note: meshes were appended in path order (a_ before b_), which the reader checks
	{
		LodoBase b[3];
		std::memset( b, 0, sizeof( b ) );
		b[0].formId = 0x00010001; b[0].flags = LODO_BASE_TREE | LODO_BASE_ANY_ALPHA | LODO_BASE_ANY_MESH;
		b[0].modelStringOffset = lib.addString( QStringLiteral( "meshes\\synthetic\\a_tree.nif" ) );
		b[0].rep[0] = meshA; b[0].rep[1] = meshA; b[0].rep[2] = meshA; b[0].rep[3] = meshA;
		b[0].cardLayer = LODO_NO_CARD; b[0].boundRadius = 216.0f;
		b[1].formId = 0x00010002; b[1].flags = LODO_BASE_ANY_MESH;
		b[1].modelStringOffset = lib.addString( QStringLiteral( "meshes\\synthetic\\b_static.nif" ) );
		b[1].rep[0] = meshB; b[1].rep[1] = meshB; b[1].rep[2] = LODO_NO_MESH; b[1].rep[3] = LODO_NO_MESH;
		b[1].cardLayer = LODO_NO_CARD; b[1].boundRadius = 101.0f;
		b[2].formId = 0x00010003; b[2].flags = LODO_BASE_ANY_MESH;
		b[2].modelStringOffset = lib.addString( QStringLiteral( "meshes\\synthetic\\c_part.nif" ) );
		b[2].rep[0] = meshA; b[2].rep[1] = LODO_NO_MESH; b[2].rep[2] = LODO_NO_MESH; b[2].rep[3] = LODO_NO_MESH;
		b[2].cardLayer = LODO_NO_CARD; b[2].boundRadius = 216.0f;
		for ( auto & x : b )
			lib.bases.push_back( x );
	}
	LodoHeader lh;
	QString err;
	const QString lodoPath = dir + QStringLiteral( "/Synthetic.lodo" );
	if ( !lodoWrite( lodoPath, lib, &lh, &err ) )
		return fail( err );
	E( QStringLiteral( "lodo.meshCount" ), QStringLiteral( "2" ) );
	E( QStringLiteral( "lodo.baseCount" ), QStringLiteral( "3" ) );
	E( QStringLiteral( "lodo.materialCount" ), QStringLiteral( "2" ) );
	/* v3: the counts that a hand can still derive are the LEVEL-0 ones. The
	 * whole-file cluster and vertex counts now include the ladder, which no
	 * hand derives, so they are no longer stated as known answers -- the ladder
	 * is checked by its invariants instead. */
	E( QStringLiteral( "lodo.level0.clusterCount" ), QStringLiteral( "3" ) );
	E( QStringLiteral( "lodo.level0.vertices" ), QStringLiteral( "8+18+6" ) );   // cube 8; strip cluster 1 = 16 tris over 18 verts, cluster 2 = 4 tris over 6
	E( QStringLiteral( "lodo.level0.triangles" ), QStringLiteral( "32" ) );
	//! predicted before the run: a 32-triangle library has somewhere to climb
	E( QStringLiteral( "lodo.levelMaxAtLeast" ), QStringLiteral( "1" ) );
	//! every ROOT's subtree count must sum to the whole library's level-0 triangles
	E( QStringLiteral( "lodo.rootSourceTriangles" ), QStringLiteral( "32" ) );
	E( QStringLiteral( "lodo.mesh0.path" ), QStringLiteral( "meshes\\lod\\synthetic\\a_tree_lod.nif" ) );
	E( QStringLiteral( "lodo.mesh0.aabb" ), QStringLiteral( "-50,-50,0,100,100,200" ) );
	E( QStringLiteral( "lodo.mesh0.uvrect" ), QStringLiteral( "-0.25,0,1.5,1" ) );
	E( QStringLiteral( "lodo.base0.formId" ), QStringLiteral( "00010001" ) );
	E( QStringLiteral( "lodo.base1.rep" ), QStringLiteral( "1,1,65535,65535" ) );
	E( QStringLiteral( "lodo.mesh1.l0cluster0.triangleCount" ), QStringLiteral( "16" ) );
	E( QStringLiteral( "lodo.mesh1.l0cluster1.triangleCount" ), QStringLiteral( "4" ) );
	E( QStringLiteral( "lodo.mesh1.l0cluster1.sizeClass" ), QStringLiteral( "0" ) );
	/* THE CONE THAT MUST OPEN, hand-derived: mesh 0's single level-0 cluster is
	 * a CLOSED CUBE, whose twelve face normals point along all six axes and
	 * span the whole sphere. No cone bounds them, so the cluster must carry
	 * CONE_OPEN, axis (0,0) and cosine -1. Mesh 1's first cluster is a flat
	 * strip, every normal +Z, so its cone is as tight as a cone gets. */
	E( QStringLiteral( "lodo.mesh0.l0cluster0.coneOpen" ), QStringLiteral( "1" ) );
	E( QStringLiteral( "lodo.mesh1.l0cluster0.coneOpen" ), QStringLiteral( "0" ) );
	//! the cube's bounding sphere: centre (0,0,100), radius sqrt(50^2+50^2+100^2)
	E( QStringLiteral( "lodo.mesh0.l0cluster0.sphere" ), QStringLiteral( "0,0,100,122.474487" ) );
	//! the strip's first cluster spans x 0..80, y 0..10, z 0
	E( QStringLiteral( "lodo.mesh1.l0cluster0.sphere" ), QStringLiteral( "40,5,0,40.311287" ) );
	E( QStringLiteral( "lodo.vertex0.pos" ), QStringLiteral( "0,0,0" ) );           // (-50,-50,0) at the AABB min
	E( QStringLiteral( "lodo.vertex6.pos" ), QStringLiteral( "65535,65535,65535" ) );
	E( QStringLiteral( "lodo.vertex4.sway" ), QStringLiteral( "200" ) );

	/* The instances. Three, in three chunks, written out of order so the sort
	 * is exercised: the tree in chunk (0,0), the static in (-1,1), the SCOL
	 * part on the exact east edge of (0,0) -- which is chunk (1,0). */
	LodiSrcSet set;
	set.worldspaceEdid = QStringLiteral( "Synthetic" );
	set.pluginCorpusHash = lib.pluginCorpusHash;
	set.objectCorpusHash = lib.objectCorpusHash;
	set.lodoIdentity = lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash );
	set.loadOrderHash = lib.loadOrderHash;
	{
		LodiSrcInstance tree;
		tree.pos[0] = 1000.5f; tree.pos[1] = 2000.25f; tree.pos[2] = 300.0f;
		float esm[9], yaw[9];
		fromEuler( -0.17453293f, -0.34906585f, -0.52359878f, esm );       // DATA (10, 20, 30) deg, negated as lodgen does
		fromEuler( 0.0f, 0.0f, 123.0f * 0.01745329f, yaw );                // treeHash % 360 = 123
		matMul( esm, yaw, tree.rot );
		tree.scale = 1.5f; tree.baseId = 0; tree.refFormId = 0x00020001; tree.scolPart = -1;
		tree.ao = 200; tree.sky = 180; tree.ground = 40; tree.seed = 0x7B;
		tree.flags = LODI_INST_MIRRORED | LODI_INST_ALPHA_TESTED;
		tree.drawKey = 0; tree.identity = 1;
		tree.boundRadius = 216.0f; tree.baseName = QStringLiteral( "a_tree" );
		LodiSrcInstance stat;
		stat.pos[0] = -5000.0f; stat.pos[1] = 17000.0f; stat.pos[2] = -100.0f;
		stat.scale = 1.0f; stat.baseId = 1; stat.refFormId = 0x00020002; stat.scolPart = -1;
		stat.ao = 255; stat.sky = 255; stat.ground = 255; stat.seed = 0;
		stat.drawKey = 1; stat.identity = 7;
		stat.boundRadius = 101.0f; stat.baseName = QStringLiteral( "b_static" );
		LodiSrcInstance part;
		part.pos[0] = 16384.0f; part.pos[1] = 0.0f; part.pos[2] = 50.0f;
		fromEuler( 0.0f, 0.0f, -1.5707963f, part.rot );
		part.scale = 0.25f; part.baseId = 2; part.refFormId = 0x00020003; part.scolPart = 2;
		part.ao = 128; part.sky = 64; part.ground = 0; part.seed = 0;
		part.flags = LODI_INST_SCOL_PART;
		part.drawKey = 0; part.identity = 9;
		part.boundRadius = 216.0f; part.baseName = QStringLiteral( "c_part" );

		/* v2: TWO more instances in the SAME chunk AND the SAME cell as the
		 * tree, so the (cell, drawKey, ref, part) order rule is EXERCISED --
		 * the v1 fixture had one instance a chunk and the rule was checked by
		 * both readers and never fired (contract page 10, "not exercised").
		 * `keyLow` carries the SMALLEST ref of the three and the HIGHER
		 * drawKey, so ref order alone would put it first and drawKey order
		 * puts it last: the fixture discriminates the two laws. Same z as the
		 * tree, so the chunk's zExtent stays 0 and the tree's own tolerance
		 * line still reads. */
		LodiSrcInstance keyLow;      // baseId 1 -> mesh B, material 0 -> drawKey 1
		keyLow.pos[0] = 1100.0f; keyLow.pos[1] = 2100.0f; keyLow.pos[2] = 300.0f;
		keyLow.scale = 1.0f; keyLow.baseId = 1; keyLow.refFormId = 0x00020000; keyLow.scolPart = -1;
		keyLow.ao = 10; keyLow.sky = 20; keyLow.ground = 30; keyLow.seed = 0;
		keyLow.drawKey = 1; keyLow.identity = 3;
		keyLow.boundRadius = 101.0f; keyLow.baseName = QStringLiteral( "b_static" );
		LodiSrcInstance keyHigh;     // baseId 2 -> mesh A, material 1 -> drawKey 0, same as the tree's
		keyHigh.pos[0] = 1200.0f; keyHigh.pos[1] = 2200.0f; keyHigh.pos[2] = 300.0f;
		keyHigh.scale = 0.5f; keyHigh.baseId = 2; keyHigh.refFormId = 0x00020004; keyHigh.scolPart = -1;
		keyHigh.ao = 40; keyHigh.sky = 50; keyHigh.ground = 60; keyHigh.seed = 0;
		keyHigh.drawKey = 0; keyHigh.identity = 2;
		keyHigh.boundRadius = 216.0f; keyHigh.baseName = QStringLiteral( "c_part" );
		/* v3: THE ONE OCCLUDER, and it is hand-derivable on purpose. keyHigh
		 * draws mesh A -- the closed cube, (-50..50, -50..50, 0..200) -- at
		 * scale 0.5 with NO rotation, so a box of half (40, 40, 90) about the
		 * cube's centre (0, 0, 100) lies wholly inside the cube, and in the
		 * world it is centred at (1200, 2200, 350) with half extents
		 * 0.5 x (40, 40, 90) x 0.999 = (19.98, 19.98, 44.955). The 0.999 is the
		 * writer's own pull-in for the quantised rotation, and the fixture
		 * states it rather than hiding it. */
		keyHigh.hasOccluder = true;
		keyHigh.occCentre[0] = 0.0f; keyHigh.occCentre[1] = 0.0f; keyHigh.occCentre[2] = 100.0f;
		keyHigh.occHalf[0] = 40.0f; keyHigh.occHalf[1] = 40.0f; keyHigh.occHalf[2] = 90.0f;
		keyHigh.occMeshId = meshA;

		set.instances.push_back( part );
		set.instances.push_back( keyLow );
		set.instances.push_back( tree );
		set.instances.push_back( keyHigh );
		set.instances.push_back( stat );
		for ( int i = 0; i < 9; i++ ) {
			exp << QString( "expect lodi.tree.rot%1 %2" ).arg( i ).arg( double( tree.rot[i] ), 0, 'f', 6 );
			exp << QString( "expect lodi.part.rot%1 %2" ).arg( i ).arg( double( part.rot[i] ), 0, 'f', 6 );
		}
	}
	LodiHeader ih;
	LodiWriteStats st;
	const QString lodiPath = dir + QStringLiteral( "/Synthetic.lodi" );
	if ( !lodiWrite( lodiPath, set, &ih, &st, &err ) )
		return fail( err );
	E( QStringLiteral( "lodi.instanceCount" ), QStringLiteral( "5" ) );
	E( QStringLiteral( "lodi.chunkExtent" ), QStringLiteral( "-1,0,1,1" ) );      // west, south, east, north
	E( QStringLiteral( "lodi.chunkCount" ), QStringLiteral( "6" ) );
	E( QStringLiteral( "lodi.presentChunks" ), QStringLiteral( "3" ) );
	E( QStringLiteral( "lodi.maxInstancesPerChunk" ), QStringLiteral( "3" ) );
	/* (-1,1) = row 0, (0,0) = index 4 with three instances in ONE cell,
	 * (1,0) = index 5. Inside index 4 the law is drawKey then ref, so the
	 * tree (0, 00020001) and keyHigh (0, 00020004) precede keyLow (1,
	 * 00020000) -- under the v1 law keyLow, the smallest ref, came first. */
	E( QStringLiteral( "lodi.order.refs" ), QStringLiteral( "00020002,00020001,00020004,00020000,00020003" ) );
	E( QStringLiteral( "lodi.order.chunks" ), QStringLiteral( "0,4,4,4,5" ) );
	E( QStringLiteral( "lodi.order.drawKeys" ), QStringLiteral( "1,0,0,1,0" ) );
	E( QStringLiteral( "lodi.order.identities" ), QStringLiteral( "7,1,2,3,9" ) );
	E( QStringLiteral( "lodi.order.cells" ), QStringLiteral( "14,12,12,12,12" ) );
	E( QStringLiteral( "lodo.flags" ), QStringLiteral( "9" ) );      // VERTEX_V1 | LADDER, no CACHE_ORDER
	E( QStringLiteral( "lodo.version" ), QString::number( LODO_VERSION ) );
	E( QStringLiteral( "lodi.version" ), QStringLiteral( "3" ) );
	E( QStringLiteral( "lodi.occluderCount" ), QStringLiteral( "1" ) );
	E( QStringLiteral( "lodi.occluder0.centre" ), QStringLiteral( "1200,2200,350" ) );
	E( QStringLiteral( "lodi.occluder0.half" ), QStringLiteral( "19.98,19.98,44.955" ) );
	E( QStringLiteral( "lodi.occluder0.instanceIndex" ), QStringLiteral( "2" ) );    // keyHigh, ref 00020004
	E( QStringLiteral( "lodi.occluder0.cell" ), QStringLiteral( "12" ) );
	E( QStringLiteral( "lodi.occluder0.meshId" ), QStringLiteral( "0" ) );     // mesh A, the closed cube
	E( QStringLiteral( "lodi.maxOccludersPerCell" ), QStringLiteral( "4" ) );
	E( QStringLiteral( "loadOrderHash" ), QStringLiteral( "4444444444444444" ) );
	E( QStringLiteral( "lodi.tree.pos" ), QStringLiteral( "1000.5,2000.25,300" ) );
	E( QStringLiteral( "lodi.tree.posTol" ), QStringLiteral( "0.125,0.125,0" ) );   // half a step in X/Y; Z extent is 0 in a one-instance chunk
	E( QStringLiteral( "lodi.tree.scale" ), QStringLiteral( "1.5" ) );
	E( QStringLiteral( "lodi.tree.baseId" ), QStringLiteral( "0" ) );
	E( QStringLiteral( "lodi.tree.flags" ), QStringLiteral( "5" ) );
	E( QStringLiteral( "lodi.tree.seed" ), QStringLiteral( "123" ) );
	E( QStringLiteral( "lodi.tree.aoSkyGround" ), QStringLiteral( "200,180,40" ) );
	E( QStringLiteral( "lodi.tree.cell" ), QStringLiteral( "12" ) );      // lx 0, ly 0 -> (3-0)*4+0
	E( QStringLiteral( "lodi.stat.pos" ), QStringLiteral( "-5000,17000,-100" ) );
	E( QStringLiteral( "lodi.stat.cell" ), QStringLiteral( "14" ) );      // x -5000 -> chunk -1, lx = 2; y 17000 -> chunk 1, ly 0 -> 12 + 2
	E( QStringLiteral( "lodi.part.pos" ), QStringLiteral( "16384,0,50" ) );
	E( QStringLiteral( "lodi.part.scale" ), QStringLiteral( "0.25" ) );
	E( QStringLiteral( "lodi.part.scolPart" ), QStringLiteral( "2" ) );
	E( QStringLiteral( "lodi.part.baseId" ), QStringLiteral( "2" ) );
	E( QStringLiteral( "lodi.rotTolDeg" ), QStringLiteral( "0.02" ) );
	E( QStringLiteral( "lodi.identityMatchesLodo" ), QStringLiteral( "1" ) );
	E( QStringLiteral( "lodi.chunk4.maxBoundRadius" ), QStringLiteral( "324" ) );
	E( QStringLiteral( "lodi.chunk4.instanceCount" ), QStringLiteral( "3" ) );

	QFile ef( dir + QStringLiteral( "/Synthetic.expect.txt" ) );
	if ( !ef.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QStringLiteral( "cannot write Synthetic.expect.txt" ) );
	ef.write( ( exp.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
	ef.close();
	if ( report ) {
		*report << QString( "wrote %1 (%2 bytes) and %3 (%4 bytes)" ).arg( lodoPath ).arg( lh.fileBytes ).arg( lodiPath ).arg( ih.fileBytes );
		*report << exp;
	}
	return true;
}
