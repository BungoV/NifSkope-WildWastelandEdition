/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodofile.h"
#include "lodifile.h"
#include "lodgenao.h"
#include "io/lodvfile.h"

#include "meshoptimizer/src/meshoptimizer.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

/* The byte layout of the 256-byte header (docs/LODGEN_NATIVE_LODO_LODI.md 3).
 * Every offset below is the file's, and lodoRead checks the reserved words at
 * 0xAC and 0xB8..0xFF are zero. */
namespace
{

constexpr float LODO_PI = 3.14159265358979f;
constexpr int H_MAGIC = 0x00, H_VERSION = 0x04, H_FLAGS = 0x08, H_HCRC = 0x0C;
constexpr int H_PLUGIN = 0x10, H_OBJECT = 0x18, H_MODEL = 0x20, H_CARD = 0x28;
constexpr int H_EDID = 0x30, H_EDID_BYTES = 32;
constexpr int H_BASES = 0x50, H_MESHES = 0x54, H_CLUSTERS = 0x58, H_MATERIALS = 0x5C;
constexpr int H_VERTICES = 0x60, H_MAXCLUSTERS = 0x64, H_MAXTRIS = 0x68, H_VSTRIDE = 0x6A;
constexpr int H_STRBYTES = 0x6C;
constexpr int H_OFF_BASES = 0x70, H_OFF_MESHES = 0x78, H_OFF_CLUSTERS = 0x80, H_OFF_MATERIALS = 0x88;
constexpr int H_OFF_LOCAL = 0x90, H_OFF_VERTS = 0x98, H_OFF_STRINGS = 0xA0;
constexpr int H_ICRC = 0xA8, H_RESERVED_AC = 0xAC, H_FILEBYTES = 0xB0;
//! v2: the load-order hash took the first reserved word; the pad started at 0xC0.
constexpr int H_LOADORDER = 0xB8;
/*! v3 took the room the v2 contract's section 12 named for the ladder:
 *  0xC0 the parallel table's offset, 0xC8 its stride, 0xCC the deepest level,
 *  0xCD the grouping target. The pad now starts at 0xCE. */
constexpr int H_OFF_CLUSTERLODS = 0xC0, H_CLUSTERLOD_STRIDE = 0xC8;
constexpr int H_LEVELMAX = 0xCC, H_LADDERGROUP = 0xCD;
/*! v4 (lane NATIVE1c) takes one word of the room section 12 left at 0xCE..0xFF:
 *  0xD0 `cardCount`. 0xCE..0xCF stay RESERVED rather than being used, so the
 *  u32 lands 4-byte aligned in the file exactly as every other u32 does. The
 *  pad is now 0xCE..0xCF plus 0xD4..0xFF, and both halves are checked zero. */
constexpr int H_RESERVED_CE = 0xCE, H_CARDCOUNT = 0xD0, H_RESERVED_D4 = 0xD4;
/*! v5 (lane SEAM1, W4) takes the next twelve bytes of that pad: 0xD4 the colour
 *  stream's row count, 0xD8 its offset. Both are zero on a library with no
 *  coloured mesh, so a v4 file is a v5 file without colour. The pad is now
 *  0xCE..0xCF plus 0xE0..0xFF. */
constexpr int H_COLOURCOUNT = 0xD4, H_OFF_COLOURS = 0xD8, H_RESERVED_E0 = 0xE0;

template <typename T> void putLE( QByteArray & b, int off, T v )
{
	// little-endian regardless of host: byte by byte
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
/* `getF32` lived here for the v5 subdivision words and went with them (lane
 * HORIZONOUT, 2026-09-19). Nothing else in this file reads a float from the
 * header, so an unused reader is one more thing that can go stale. */

quint64 alignUp( quint64 v, quint64 a )
{
	return ( v + a - 1 ) / a * a;
}

//! The magic another reader in this family would recognise, for the refusal text.
QString nameOtherMagic( quint32 m )
{
	if ( m == LODI_MAGIC ) return QStringLiteral( "a .lodi instance table (LODI)" );
	if ( m == LODTEX_MAGIC ) return QStringLiteral( "a .lodt terrain texture level (LDTX)" );
	if ( m == 0x54444F4CU ) return QStringLiteral( "a .lodl landscape file (LODT)" );
	if ( m == 0x4D444F4CU ) return QStringLiteral( "a .lodm material sidecar (LODM)" );
	if ( m == 0x20534444U ) return QStringLiteral( "a DDS texture" );
	if ( m == LODTEX_MAGIC_RETIRED_LODV ) return QStringLiteral( "a retired .lodv container (LODV)" );
	return QString( "unknown magic 0x%1" ).arg( m, 8, 16, QChar( '0' ) );
}

QString foldPath( const QString & s )
{
	QString t = s.toLower();
	t.replace( QChar( '/' ), QChar( '\\' ) );
	return t;
}

} // namespace

/* ---------------------------------------------------------------- strings */

quint32 LodoLibrary::addString( const QString & s )
{
	if ( strings.isEmpty() )
		strings.append( '\0' );
	if ( s.isEmpty() )
		return 0;
	const quint32 off = quint32( strings.size() );
	strings.append( s.toUtf8() );
	strings.append( '\0' );
	return off;
}

QString LodoLibrary::stringAt( quint32 off ) const
{
	if ( off >= quint32( strings.size() ) )
		return QString();
	const char * p = strings.constData() + off;
	const qsizetype room = strings.size() - qsizetype( off );
	const qsizetype n = qsizetype( strnlen( p, size_t( room ) ) );
	return QString::fromUtf8( p, n );
}

/* ---------------------------------------------------------------- packing */

quint32 lodoPackOct12( const float n[3] )
{
	const float s = std::fabs( n[0] ) + std::fabs( n[1] ) + std::fabs( n[2] );
	float u = s > 0.0f ? n[0] / s : 0.0f, v = s > 0.0f ? n[1] / s : 0.0f;
	if ( n[2] < 0.0f ) {
		const float u2 = ( 1.0f - std::fabs( v ) ) * ( u >= 0.0f ? 1.0f : -1.0f );
		const float v2 = ( 1.0f - std::fabs( u ) ) * ( v >= 0.0f ? 1.0f : -1.0f );
		u = u2;
		v = v2;
	}
	const int qu = std::clamp( int( std::lround( ( u + 1.0f ) * 0.5f * 4095.0f ) ), 0, 4095 );
	const int qv = std::clamp( int( std::lround( ( v + 1.0f ) * 0.5f * 4095.0f ) ), 0, 4095 );
	return quint32( qu ) | ( quint32( qv ) << 12 );
}

void lodoUnpackOct12( quint32 packed, float n[3] )
{
	float u = float( packed & 0xFFF ) / 4095.0f * 2.0f - 1.0f;
	float v = float( ( packed >> 12 ) & 0xFFF ) / 4095.0f * 2.0f - 1.0f;
	float z = 1.0f - std::fabs( u ) - std::fabs( v );
	if ( z < 0.0f ) {
		const float u2 = ( 1.0f - std::fabs( v ) ) * ( u >= 0.0f ? 1.0f : -1.0f );
		const float v2 = ( 1.0f - std::fabs( u ) ) * ( v >= 0.0f ? 1.0f : -1.0f );
		u = u2;
		v = v2;
	}
	const float l = std::sqrt( u * u + v * v + z * z );
	n[0] = u / l;
	n[1] = v / l;
	n[2] = z / l;
}

void lodoPackOct16( const float n[3], quint16 out[2] )
{
	const float s = std::fabs( n[0] ) + std::fabs( n[1] ) + std::fabs( n[2] );
	float u = s > 0.0f ? n[0] / s : 0.0f, v = s > 0.0f ? n[1] / s : 0.0f;
	if ( n[2] < 0.0f ) {
		const float u2 = ( 1.0f - std::fabs( v ) ) * ( u >= 0.0f ? 1.0f : -1.0f );
		const float v2 = ( 1.0f - std::fabs( u ) ) * ( v >= 0.0f ? 1.0f : -1.0f );
		u = u2;
		v = v2;
	}
	out[0] = quint16( std::clamp( int( std::lround( ( u + 1.0f ) * 0.5f * 65535.0f ) ), 0, 65535 ) );
	out[1] = quint16( std::clamp( int( std::lround( ( v + 1.0f ) * 0.5f * 65535.0f ) ), 0, 65535 ) );
}

void lodoUnpackOct16( const quint16 in[2], float n[3] )
{
	float u = float( in[0] ) / 65535.0f * 2.0f - 1.0f;
	float v = float( in[1] ) / 65535.0f * 2.0f - 1.0f;
	float z = 1.0f - std::fabs( u ) - std::fabs( v );
	if ( z < 0.0f ) {
		const float u2 = ( 1.0f - std::fabs( v ) ) * ( u >= 0.0f ? 1.0f : -1.0f );
		const float v2 = ( 1.0f - std::fabs( u ) ) * ( v >= 0.0f ? 1.0f : -1.0f );
		u = u2;
		v = v2;
	}
	const float l = std::sqrt( u * u + v * v + z * z );
	n[0] = u / l;
	n[1] = v / l;
	n[2] = z / l;
}

namespace
{
//! The reference frame (a, b) about a unit normal, from the normal alone.
void tangentFrame( const float n[3], float a[3], float b[3] )
{
	// the helper axis least aligned with n, so the cross product never vanishes
	float h[3] = { 0.0f, 0.0f, 1.0f };
	if ( std::fabs( n[2] ) > 0.9f ) {
		h[0] = 1.0f; h[2] = 0.0f;
	}
	a[0] = n[1] * h[2] - n[2] * h[1];
	a[1] = n[2] * h[0] - n[0] * h[2];
	a[2] = n[0] * h[1] - n[1] * h[0];
	const float l = std::sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] );
	a[0] /= l; a[1] /= l; a[2] /= l;
	b[0] = n[1] * a[2] - n[2] * a[1];
	b[1] = n[2] * a[0] - n[0] * a[2];
	b[2] = n[0] * a[1] - n[1] * a[0];
}
}

quint8 lodoPackTangent( const float n[3], const float t[3], bool flipHanded )
{
	float a[3], b[3];
	tangentFrame( n, a, b );
	const float ta = t[0] * a[0] + t[1] * a[1] + t[2] * a[2];
	const float tb = t[0] * b[0] + t[1] * b[1] + t[2] * b[2];
	const float ang = std::atan2( tb, ta );           // (-pi, pi]
	int q = int( std::lround( ( ang + LODO_PI ) / ( 2.0f * LODO_PI ) * 128.0f ) ) & 127;
	return quint8( q | ( flipHanded ? 0x80 : 0 ) );
}

void lodoUnpackTangent( const float n[3], quint8 packed, float t[3], bool * flipHanded )
{
	float a[3], b[3];
	tangentFrame( n, a, b );
	const float ang = float( packed & 127 ) / 128.0f * 2.0f * LODO_PI - LODO_PI;
	const float c = std::cos( ang ), s = std::sin( ang );
	for ( int i = 0; i < 3; i++ )
		t[i] = c * a[i] + s * b[i];
	if ( flipHanded )
		*flipHanded = ( packed & 0x80 ) != 0;
}

quint16 lodoQuantU16( float v, float lo, float extent )
{
	if ( !( extent > 0.0f ) )
		return 0;
	const float f = ( v - lo ) / extent;
	return quint16( std::clamp( int( std::lround( f * 65535.0f ) ), 0, 65535 ) );
}

float lodoDequantU16( quint16 q, float lo, float extent )
{
	return lo + float( q ) / 65535.0f * extent;
}

quint64 lodoFnv1a64( const void * p, size_t n, quint64 h )
{
	const unsigned char * b = static_cast<const unsigned char *>( p );
	for ( size_t i = 0; i < n; i++ ) {
		h ^= b[i];
		h *= Q_UINT64_C( 0x100000001B3 );
	}
	return h;
}

quint64 lodoIdentityOf( quint32 headerCrc32, quint64 modelCorpusHash, quint64 objectCorpusHash )
{
	unsigned char buf[20];
	for ( int i = 0; i < 4; i++ ) buf[i] = ( headerCrc32 >> ( 8 * i ) ) & 0xFF;
	for ( int i = 0; i < 8; i++ ) buf[4 + i] = ( modelCorpusHash >> ( 8 * i ) ) & 0xFF;
	for ( int i = 0; i < 8; i++ ) buf[12 + i] = ( objectCorpusHash >> ( 8 * i ) ) & 0xFF;
	return lodoFnv1a64( buf, sizeof( buf ) );
}

/* ---------------------------------------------------------------- meshes */

namespace
{

/*! Edges used by exactly ONE triangle, over a topology WELDED by the quantised
 *  library position. Welding by position is what makes the source count and
 *  the emitted count comparable: the source is split into shapes and the
 *  emitted mesh is split into clusters, and neither split may change the
 *  silhouette. `key(v)` returns the welded vertex id of triangle corner `v`. */
template <typename KeyFn>
quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )
{
	std::unordered_map<quint64, quint32> use;
	use.reserve( tris.size() );
	for ( size_t t = 0; t + 2 < tris.size(); t += 3 ) {
		const quint32 v[3] = { key( tris[t] ), key( tris[t + 1] ), key( tris[t + 2] ) };
		for ( int k = 0; k < 3; k++ ) {
			const quint32 a = v[k], b = v[( k + 1 ) % 3];
			if ( a == b )
				continue;                   // a degenerate edge is not a silhouette
			const quint64 e = a < b ? ( quint64( a ) << 32 ) | b : ( quint64( b ) << 32 ) | a;
			use[e]++;
		}
	}
	quint32 n = 0;
	for ( const auto & kv : use )
		if ( kv.second == 1 )
			n++;
	return n;
}

//! The welded key of a quantised position triple, packed into one u64.
inline quint64 lodoWeldKey( quint16 x, quint16 y, quint16 z )
{
	return ( quint64( x ) << 32 ) | ( quint64( y ) << 16 ) | quint64( z );
}

/* ---- v3, the ladder's geometry (lane NATIVE1b) ---- */

//! One cluster-local vertex, as every emitter hands it over.
struct LodoEmitVert
{
	float pos[3] = { 0.0f, 0.0f, 0.0f };
	float nrm[3] = { 0.0f, 0.0f, 1.0f };
	float tan[3] = { 1.0f, 0.0f, 0.0f };
	float uv[2] = { 0.0f, 0.0f };
	quint8 sway = 0;
	quint8 ao = 255;
	quint32 rgba = LODO_COLOUR_NONE;    //!< v5: R low byte; NONE when the shape carries no colour
};

//! v5: a source vertex's packed colour, R in the low byte; NONE when the shape carries none.
inline quint32 lodoSrcRgba( const LodoSrcShape & s, size_t v )
{
	if ( s.rgba.size() < ( v + 1 ) * 4 )
		return LODO_COLOUR_NONE;
	const quint8 * c = &s.rgba[v * 4];
	return quint32( c[0] ) | ( quint32( c[1] ) << 8 ) | ( quint32( c[2] ) << 16 ) | ( quint32( c[3] ) << 24 );
}

//! Squared distance from p to the triangle (a, b, c). Ericson, Real-Time Collision Detection 5.1.5.
float lodoPointTriDist2( const float p[3], const float a[3], const float b[3], const float c[3] )
{
	float ab[3], ac[3], ap[3];
	for ( int k = 0; k < 3; k++ ) {
		ab[k] = b[k] - a[k];
		ac[k] = c[k] - a[k];
		ap[k] = p[k] - a[k];
	}
	auto dot = []( const float u[3], const float v[3] ) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
	const float d1 = dot( ab, ap ), d2 = dot( ac, ap );
	float q[3];
	if ( d1 <= 0.0f && d2 <= 0.0f ) {
		for ( int k = 0; k < 3; k++ ) q[k] = a[k];
	} else {
		float bp[3], cp[3];
		for ( int k = 0; k < 3; k++ ) { bp[k] = p[k] - b[k]; cp[k] = p[k] - c[k]; }
		const float d3 = dot( ab, bp ), d4 = dot( ac, bp );
		if ( d3 >= 0.0f && d4 <= d3 ) {
			for ( int k = 0; k < 3; k++ ) q[k] = b[k];
		} else {
			const float vc = d1 * d4 - d3 * d2;
			const float d5 = dot( ab, cp ), d6 = dot( ac, cp );
			if ( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f ) {
				const float t = d1 - d3 != 0.0f ? d1 / ( d1 - d3 ) : 0.0f;
				for ( int k = 0; k < 3; k++ ) q[k] = a[k] + t * ab[k];
			} else if ( d6 >= 0.0f && d5 <= d6 ) {
				for ( int k = 0; k < 3; k++ ) q[k] = c[k];
			} else {
				const float vb = d5 * d2 - d1 * d6;
				if ( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f ) {
					const float t = d2 - d6 != 0.0f ? d2 / ( d2 - d6 ) : 0.0f;
					for ( int k = 0; k < 3; k++ ) q[k] = a[k] + t * ac[k];
				} else {
					const float va = d3 * d6 - d5 * d4;
					if ( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f ) {
						const float den = ( d4 - d3 ) + ( d5 - d6 );
						const float t = den != 0.0f ? ( d4 - d3 ) / den : 0.0f;
						for ( int k = 0; k < 3; k++ ) q[k] = b[k] + t * ( c[k] - b[k] );
					} else {
						const float den = va + vb + vc;
						const float v = den != 0.0f ? vb / den : 0.0f;
						const float w = den != 0.0f ? vc / den : 0.0f;
						for ( int k = 0; k < 3; k++ ) q[k] = a[k] + v * ab[k] + w * ac[k];
					}
				}
			}
		}
	}
	float d2sum = 0.0f;
	for ( int k = 0; k < 3; k++ ) {
		const float d = p[k] - q[k];
		d2sum += d * d;
	}
	return d2sum;
}

/*! The largest distance from any VERTEX of `from` to the triangle soup `to`.
 *  Both are index triples into `pos`. Vertex sampling is what every practical
 *  simplification error is measured with; it is an UNDER-estimate of the true
 *  Hausdorff distance, which is why the caller takes it in BOTH directions and
 *  keeps the larger, and why the number is called a deviation and not a bound
 *  on every point of the surface. */
float lodoMaxVertToSoup( const std::vector<quint32> & from, const std::vector<quint32> & to,
	const std::vector<float> & pos )
{
	if ( from.empty() || to.size() < 3 )
		return 0.0f;
	std::vector<quint32> pts( from );
	std::sort( pts.begin(), pts.end() );
	pts.erase( std::unique( pts.begin(), pts.end() ), pts.end() );
	float worst = 0.0f;
	for ( quint32 v : pts ) {
		const float * p = &pos[size_t( v ) * 3];
		float best = 3.4e38f;
		for ( size_t t = 0; t + 2 < to.size(); t += 3 ) {
			const float d2 = lodoPointTriDist2( p, &pos[size_t( to[t] ) * 3],
				&pos[size_t( to[t + 1] ) * 3], &pos[size_t( to[t + 2] ) * 3] );
			if ( d2 < best )
				best = d2;
			if ( best <= 0.0f )
				break;
		}
		worst = std::max( worst, best );
	}
	return std::sqrt( worst );
}

//! The two-sided vertex-sampled deviation between two triangle soups.
float lodoSoupDeviation( const std::vector<quint32> & a, const std::vector<quint32> & b,
	const std::vector<float> & pos )
{
	return std::max( lodoMaxVertToSoup( a, b, pos ), lodoMaxVertToSoup( b, a, pos ) );
}

/*! v4 (lane NATIVE1c): THE SILHOUETTE MEASUREMENT.
 *
 *  The outline a triangle soup presents from LODO_SILHOUETTE_VIEWS azimuths
 *  around Z AT THE HORIZON -- the views a far LOD is actually seen from -- as a
 *  covered-cell count per view on a fixed LODO_SILHOUETTE_GRID x GRID
 *  orthographic grid. It is a COVERAGE count and not a projected area: a
 *  simplification that thins a crown into a lattice loses coverage even when
 *  its triangles still sum to the same area, and it is the crown going away
 *  that bungo saw.
 *
 *  Both soups of a comparison are rasterised over the SAME box -- the mesh's
 *  own AABB -- so the ratio is a ratio of the same pixels. The box, not the
 *  soup's own extent, is the whole point: a soup that SHRANK must read as a
 *  loss, and one measured in its own box would read as 1.0 however small it
 *  got. A degenerate box yields zeros, which the caller reads as "no opinion"
 *  and passes.
 *
 *  Winding is ignored (no backface test): a silhouette is what is covered from
 *  the view, and FO4's LOD meshes carry two-sided leaf quads whose winding is
 *  meaningless. */
void lodoSilhouetteCoverage( const std::vector<quint32> & tris, const std::vector<float> & pos,
	const float aabbMin[3], const float aabbExtent[3], quint32 cover[LODO_SILHOUETTE_VIEWS] )
{
	constexpr int G = LODO_SILHOUETTE_GRID;
	for ( int v = 0; v < LODO_SILHOUETTE_VIEWS; v++ )
		cover[v] = 0;
	// the horizontal half-extent every azimuth must fit: half the XY diagonal
	const float hx = 0.5f * aabbExtent[0], hy = 0.5f * aabbExtent[1];
	const float R = std::sqrt( hx * hx + hy * hy );
	const float zSpan = aabbExtent[2];
	if ( !( R > 0.0f ) || !( zSpan > 0.0f ) || tris.size() < 3 )
		return;
	const float cx = aabbMin[0] + hx, cy = aabbMin[1] + hy;
	std::vector<unsigned char> bits( size_t( G ) * G );
	for ( int view = 0; view < LODO_SILHOUETTE_VIEWS; view++ ) {
		const double a = 2.0 * 3.14159265358979323846 * double( view ) / double( LODO_SILHOUETTE_VIEWS );
		const float ca = float( std::cos( a ) ), sa = float( std::sin( a ) );
		std::fill( bits.begin(), bits.end(), (unsigned char) 0 );
		for ( size_t t = 0; t + 2 < tris.size(); t += 3 ) {
			float px[3], py[3];
			for ( int k = 0; k < 3; k++ ) {
				const float * q = &pos[size_t( tris[t + k] ) * 3];
				// u: the horizontal axis of this azimuth, about the box centre
				const float u = ( q[0] - cx ) * ca + ( q[1] - cy ) * sa;
				px[k] = ( u + R ) / ( 2.0f * R ) * float( G );
				py[k] = ( q[2] - aabbMin[2] ) / zSpan * float( G );
			}
			int x0 = int( std::floor( std::min( px[0], std::min( px[1], px[2] ) ) ) );
			int x1 = int( std::ceil( std::max( px[0], std::max( px[1], px[2] ) ) ) );
			int y0 = int( std::floor( std::min( py[0], std::min( py[1], py[2] ) ) ) );
			int y1 = int( std::ceil( std::max( py[0], std::max( py[1], py[2] ) ) ) );
			x0 = std::max( x0, 0 ); y0 = std::max( y0, 0 );
			x1 = std::min( x1, G ); y1 = std::min( y1, G );
			const float e = ( px[1] - px[0] ) * ( py[2] - py[0] ) - ( py[1] - py[0] ) * ( px[2] - px[0] );
			if ( std::fabs( e ) < 1.0e-12f ) {
				/* A degenerate or edge-on triangle still OCCUPIES its cells: a
				 * leaf quad seen exactly edge-on must not vanish from the count,
				 * or a crown would read as lost at one azimuth by luck. Mark the
				 * cells its own segment crosses. */
				for ( int k = 0; k < 3; k++ ) {
					const int j = ( k + 1 ) % 3;
					const int steps = std::max( 1, int( std::ceil( std::max( std::fabs( px[j] - px[k] ),
						std::fabs( py[j] - py[k] ) ) ) ) );
					for ( int s = 0; s <= steps; s++ ) {
						const float f = float( s ) / float( steps );
						const int xi = int( px[k] + ( px[j] - px[k] ) * f );
						const int yi = int( py[k] + ( py[j] - py[k] ) * f );
						if ( xi >= 0 && xi < G && yi >= 0 && yi < G )
							bits[size_t( yi ) * G + xi] = 1;
					}
				}
				continue;
			}
			const float inv = 1.0f / e;
			for ( int y = y0; y < y1; y++ ) {
				const float sy = float( y ) + 0.5f;
				for ( int x = x0; x < x1; x++ ) {
					if ( bits[size_t( y ) * G + x] )
						continue;
					const float sx = float( x ) + 0.5f;
					const float w0 = ( ( px[1] - sx ) * ( py[2] - sy ) - ( py[1] - sy ) * ( px[2] - sx ) ) * inv;
					const float w1 = ( ( px[2] - sx ) * ( py[0] - sy ) - ( py[2] - sy ) * ( px[0] - sx ) ) * inv;
					const float w2 = 1.0f - w0 - w1;
					if ( w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f )
						bits[size_t( y ) * G + x] = 1;
				}
			}
		}
		quint32 n = 0;
		for ( unsigned char c : bits )
			n += c;
		cover[view] = n;
	}
}

/*! The WORST-view ratio of one soup's silhouette to another's, over the same
 *  box. The worst view and not the mean: a tree that keeps its crown from seven
 *  azimuths and loses it from the eighth is still a stump from the eighth, and
 *  a mean would hide it. Views where the reference covers nothing are skipped;
 *  when the reference covers nothing anywhere the answer is 1.0 (no opinion). */
float lodoSilhouetteRatio( const std::vector<quint32> & level, const std::vector<quint32> & full,
	const std::vector<float> & pos, const float aabbMin[3], const float aabbExtent[3] )
{
	quint32 cf[LODO_SILHOUETTE_VIEWS], cl[LODO_SILHOUETTE_VIEWS];
	lodoSilhouetteCoverage( full, pos, aabbMin, aabbExtent, cf );
	lodoSilhouetteCoverage( level, pos, aabbMin, aabbExtent, cl );
	float worst = 1.0f;
	bool any = false;
	for ( int v = 0; v < LODO_SILHOUETTE_VIEWS; v++ ) {
		if ( !cf[v] )
			continue;
		any = true;
		worst = std::min( worst, float( cl[v] ) / float( cf[v] ) );
	}
	return any ? worst : 1.0f;
}

/*! Append ONE cluster of ANY level: the 16-byte row, the 48-byte ladder row,
 *  the 48-byte local-index slot and the cluster-local vertex copies. Every
 *  cluster in the library goes through this one function, so the bounding
 *  sphere, the normal cone and the size class are computed in exactly one place
 *  and a coarse cluster cannot drift from a fine one.
 *
 *  The ladder row leaves here as a ROOT with error 0; the ladder links it and
 *  sets its error afterwards. Returns the cluster's index. */
quint32 lodoEmitCluster( LodoLibrary & lib, const LodoMesh & mesh, float meshRadius, quint16 meshId,
	quint16 materialId, quint8 level, const std::vector<LodoEmitVert> & verts, const std::vector<quint8> & idx )
{
	LodoCluster c;
	std::memset( &c, 0, sizeof( c ) );
	c.vertexBase = quint32( lib.vertices.size() );
	c.vertexCount = quint8( verts.size() );
	c.triangleCount = quint8( idx.size() / 3 );
	c.materialId = materialId;
	c.meshId = meshId;
	c.flags = c.triangleCount <= 4 ? LODO_SIZE_4 : c.triangleCount <= 8 ? LODO_SIZE_8 : LODO_SIZE_16;

	float cmn[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, cmx[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	for ( const LodoEmitVert & v : verts )
		for ( int k = 0; k < 3; k++ ) {
			cmn[k] = std::min( cmn[k], v.pos[k] );
			cmx[k] = std::max( cmx[k], v.pos[k] );
		}
	float centre[3];
	for ( int k = 0; k < 3; k++ ) {
		centre[k] = 0.5f * ( cmn[k] + cmx[k] );
		const float f = mesh.aabbExtent[k] > 0.0f ? ( centre[k] - mesh.aabbMin[k] ) / mesh.aabbExtent[k] : 0.0f;
		c.boundCentre[k] = quint8( std::clamp( int( std::lround( f * 255.0f ) ), 0, 255 ) );
	}
	float r2 = 0.0f;
	for ( const LodoEmitVert & v : verts ) {
		float d2 = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			const float d = v.pos[k] - centre[k];
			d2 += d * d;
		}
		r2 = std::max( r2, d2 );
	}
	const float radius = std::sqrt( r2 );
	const float rf = meshRadius > 0.0f ? radius / meshRadius : 0.0f;
	c.boundRadius = quint8( std::clamp( int( std::ceil( rf * 255.0f ) ), 0, 255 ) );   // ceil: conservative

	/* THE SPHERE AND THE CONE DESCRIBE THE STORED GEOMETRY, not the geometry
	 * that walked in. The library keeps positions as u16 into the mesh AABB, so
	 * a consumer's triangle is up to half a quantum away from ours on every
	 * axis; a sphere fitted to the FLOAT positions then misses its own stored
	 * vertices, and a cone fitted to the float face normals excludes stored
	 * faces. Measured on the nine-chunk region before this was fixed: worst
	 * sphere overshoot 0.058 u, worst cone cosine deficit 0.002033. Both are
	 * quantisation, and both are gone the moment the description is computed
	 * from the SAME numbers the reader will read. */
	std::vector<float> qp( verts.size() * 3, 0.0f );
	for ( size_t v = 0; v < verts.size(); v++ )
		for ( int k = 0; k < 3; k++ )
			qp[v * 3 + k] = lodoDequantU16( lodoQuantU16( verts[v].pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] ),
				mesh.aabbMin[k], mesh.aabbExtent[k] );
	float qmn[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, qmx[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	for ( size_t v = 0; v < verts.size(); v++ )
		for ( int k = 0; k < 3; k++ ) {
			qmn[k] = std::min( qmn[k], qp[v * 3 + k] );
			qmx[k] = std::max( qmx[k], qp[v * 3 + k] );
		}
	float qcentre[3];
	for ( int k = 0; k < 3; k++ )
		qcentre[k] = 0.5f * ( qmn[k] + qmx[k] );
	float qr2 = 0.0f;
	for ( size_t v = 0; v < verts.size(); v++ ) {
		float d2 = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			const float d = qp[v * 3 + k] - qcentre[k];
			d2 += d * d;
		}
		qr2 = std::max( qr2, d2 );
	}
	const float qradius = std::sqrt( qr2 );

	LodoClusterLod L;
	std::memset( &L, 0, sizeof( L ) );
	for ( int k = 0; k < 3; k++ )
		L.centre[k] = qcentre[k];
	/* The stored radius is the measured one widened by a relative ulp and an
	 * absolute floor: the farthest vertex sits EXACTLY on an un-widened sphere
	 * and a containment test in float would then read it as outside half the
	 * time. A cluster with one degenerate vertex still gets a positive radius,
	 * which the reader requires. */
	L.radius = qradius * 1.000001f + 1.0e-4f;
	L.level = level;
	L.geometricError = 0.0f;
	L.parentError = LODO_ERROR_ROOT;
	L.parentFirst = LODO_NO_PARENT;
	L.parentCount = 0;
	L.sourceTriangles = c.triangleCount;

	/* THE NORMAL CONE. The axis is the AREA-WEIGHTED sum of the face normals
	 * (the unnormalised cross products), which is the direction a cluster's
	 * biggest faces agree on rather than the direction its smallest slivers
	 * vote for. The half-angle is then the WORST face against the axis as the
	 * reader will DECODE it, so the quantisation of the axis is already inside
	 * the stored cosine and the containment gate holds on the decoded numbers
	 * and not on ours. A cluster whose normals span more than a hemisphere --
	 * a leaf card, a crossed quad, a two-sided shape -- gets no cone at all and
	 * says so in `LODO_CLUSTER_CONE_OPEN`, never a cosine that quietly excludes
	 * its own triangles. */
	double ax[3] = { 0.0, 0.0, 0.0 };
	std::vector<float> faceN( idx.size(), 0.0f );       // 3 per triangle
	for ( size_t t = 0; t + 2 < idx.size(); t += 3 ) {
		const LodoEmitVert & A = verts[idx[t]];
		const float * qa = &qp[size_t( idx[t] ) * 3];
		const float * qb = &qp[size_t( idx[t + 1] ) * 3];
		const float * qc = &qp[size_t( idx[t + 2] ) * 3];
		float e1[3], e2[3], n[3];
		for ( int k = 0; k < 3; k++ ) {
			e1[k] = qb[k] - qa[k];
			e2[k] = qc[k] - qa[k];
		}
		n[0] = e1[1] * e2[2] - e1[2] * e2[1];
		n[1] = e1[2] * e2[0] - e1[0] * e2[2];
		n[2] = e1[0] * e2[1] - e1[1] * e2[0];
		const float l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		if ( l > 0.0f ) {
			for ( int k = 0; k < 3; k++ ) {
				faceN[t + k] = n[k] / l;
				ax[k] += double( n[k] );
			}
		} else {
			// a degenerate triangle has no plane: it votes with its own vertex normal
			for ( int k = 0; k < 3; k++ ) {
				faceN[t + k] = A.nrm[k];
				ax[k] += double( A.nrm[k] );
			}
		}
	}
	bool coneOpen = true;
	const double al = std::sqrt( ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2] );
	if ( al > 1.0e-12 ) {
		float axis[3];
		for ( int k = 0; k < 3; k++ )
			axis[k] = float( ax[k] / al );
		quint16 packed[2];
		lodoPackOct16( axis, packed );
		float dec[3];
		lodoUnpackOct16( packed, dec );
		float cosMin = 1.0f;
		for ( size_t t = 0; t + 2 < faceN.size(); t += 3 )
			cosMin = std::min( cosMin, dec[0] * faceN[t] + dec[1] * faceN[t + 1] + dec[2] * faceN[t + 2] );
		/* THE MARGIN, and why it grew (lane NATIVE1c, 2026-09-16, measured).
		 * `qp` holds the quantised positions as float32. An independent reader
		 * dequantises the same u16 in double, so its face normals differ from
		 * ours by the float32 rounding of the coordinate -- which scales with
		 * the coordinate. On the v3 library that was a 47.7-triangle LOD mesh
		 * in a small box and the 1e-5 margin covered it; on the near-model
		 * library of `--library near` the boxes are the real models' and
		 * `lodgen_native_cut.py` measured the worst disagreement at 1.21e-4,
		 * twelve times the old margin, and the containment gate went red.
		 * The margin is now 1.0e-3 -- eight times the measured worst -- and the
		 * open threshold moves with it so a kept cone still stores a cosine
		 * above zero, which the reader requires. A WIDER cone is the safe
		 * direction: it culls less, never more. */
		constexpr float CONE_MARGIN = 1.0e-3f;
		if ( cosMin > 2.0f * CONE_MARGIN ) {
			coneOpen = false;
			L.coneAxis[0] = packed[0];
			L.coneAxis[1] = packed[1];
			L.coneCos = std::max( CONE_MARGIN, cosMin - CONE_MARGIN );
		}
	}
	if ( coneOpen ) {
		c.flags |= LODO_CLUSTER_CONE_OPEN;
		L.coneAxis[0] = 0;
		L.coneAxis[1] = 0;
		L.coneCos = -1.0f;
	}

	for ( const LodoEmitVert & v : verts ) {
		LodoVertex lv;
		for ( int k = 0; k < 3; k++ )
			lv.pos[k] = lodoQuantU16( v.pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] );
		for ( int k = 0; k < 2; k++ )
			lv.uv[k] = lodoQuantU16( v.uv[k], mesh.uvMin[k], mesh.uvExtent[k] );
		const quint32 oct = lodoPackOct12( v.nrm );
		lv.nrm[0] = quint8( oct & 0xFF );
		lv.nrm[1] = quint8( ( oct >> 8 ) & 0xFF );
		lv.nrm[2] = quint8( ( oct >> 16 ) & 0xFF );
		float un[3];
		lodoUnpackOct12( oct, un );      // the roll is measured about the normal the READER has
		lv.tangent = lodoPackTangent( un, v.tan, false );
		lv.sway = v.sway;
		lv.selfAO = v.ao;
		lib.vertices.push_back( lv );
		lib.colours.push_back( v.rgba );
	}
	const size_t at = lib.localIndices.size();
	lib.localIndices.resize( at + LODO_LOCAL_INDEX_BYTES, LODO_LOCAL_INDEX_NONE );
	std::memcpy( &lib.localIndices[at], idx.data(), idx.size() );
	const quint32 ci = quint32( lib.clusters.size() );
	lib.clusters.push_back( c );
	lib.clusterLods.push_back( L );
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

bool lodoAppendMesh( LodoLibrary & lib, const std::vector<LodoSrcShape> & shapesGiven,
	const QString & name, quint16 * meshId, QString * error, LodoMeshStats * stats )
{
	if ( stats )
		*stats = LodoMeshStats();
	const std::vector<LodoSrcShape> & shapesIn = shapesGiven;
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QString( "%1: %2" ).arg( name, m );
		return false;
	};
	if ( lib.meshes.size() >= size_t( LODO_NO_MESH ) )
		return fail( QStringLiteral( "the mesh table is full (65,535 rows, the u16 index)" ) );
	if ( shapesIn.empty() )
		return fail( QStringLiteral( "no shapes" ) );

	// shapes in materialId order, stable, so the clusters come out sorted
	std::vector<size_t> order( shapesIn.size() );
	for ( size_t i = 0; i < order.size(); i++ )
		order[i] = i;
	std::stable_sort( order.begin(), order.end(), [&]( size_t a, size_t b ) {
		return shapesIn[a].materialId < shapesIn[b].materialId;
	} );

	// pass 1: the AABB and the UV rect over every shape, and the index checks
	float mn[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, mx[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	float uvMn[2] = { 3.4e38f, 3.4e38f }, uvMx[2] = { -3.4e38f, -3.4e38f };
	bool anySway = false;
	size_t totalTris = 0;
	for ( size_t si = 0; si < shapesIn.size(); si++ ) {
		const LodoSrcShape & s = shapesIn[si];
		const size_t nv = s.pos.size() / 3;
		if ( nv == 0 || s.tris.empty() )
			return fail( QString( "shape %1 has %2 vertices and %3 triangles" )
				.arg( si ).arg( nv ).arg( s.tris.size() / 3 ) );
		if ( s.nrm.size() != s.pos.size() || s.tan.size() != s.pos.size() || s.uv.size() != nv * 2
			|| ( !s.sway.empty() && s.sway.size() != nv ) || ( !s.rgba.empty() && s.rgba.size() != nv * 4 )
			|| s.tris.size() % 3 )
			return fail( QString( "shape %1: attribute arrays disagree with %2 vertices" ).arg( si ).arg( nv ) );
		for ( quint32 idx : s.tris )
			if ( idx >= nv )
				return fail( QString( "shape %1: triangle index %2 past %3 vertices" ).arg( si ).arg( idx ).arg( nv ) );
		for ( size_t v = 0; v < nv; v++ ) {
			for ( int k = 0; k < 3; k++ ) {
				mn[k] = std::min( mn[k], s.pos[v * 3 + k] );
				mx[k] = std::max( mx[k], s.pos[v * 3 + k] );
			}
			for ( int k = 0; k < 2; k++ ) {
				uvMn[k] = std::min( uvMn[k], s.uv[v * 2 + k] );
				uvMx[k] = std::max( uvMx[k], s.uv[v * 2 + k] );
			}
			if ( !s.sway.empty() && s.sway[v] )
				anySway = true;
		}
		totalTris += s.tris.size() / 3;
	}
	if ( totalTris == 0 )
		return fail( QStringLiteral( "no triangles" ) );

	LodoMesh mesh;
	std::memset( &mesh, 0, sizeof( mesh ) );
	for ( int k = 0; k < 3; k++ ) {
		mesh.aabbMin[k] = mn[k];
		mesh.aabbExtent[k] = mx[k] - mn[k];
	}
	for ( int k = 0; k < 2; k++ ) {
		mesh.uvMin[k] = uvMn[k];
		mesh.uvExtent[k] = uvMx[k] - uvMn[k];
	}
	const float meshRadius = 0.5f * std::sqrt( mesh.aabbExtent[0] * mesh.aabbExtent[0]
		+ mesh.aabbExtent[1] * mesh.aabbExtent[1] + mesh.aabbExtent[2] * mesh.aabbExtent[2] );
	mesh.clusterFirst = quint32( lib.clusters.size() );
	mesh.flags = anySway ? LODO_MESH_ANY_SWAY : 0;
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
	mesh.modelStringOffset = lib.addString( name );
	const quint16 id = quint16( lib.meshes.size() );

	/* pass 1b (v2): GPU CACHE ORDER, and the numbers that prove it moved.
	 * bungo 2026-09-11 08:2x, verbatim: "vertex/triangle order for the GPU
	 * cache on every .lodo mesh". Triangles first (post-transform cache), then
	 * vertices in first-use order (fetch locality). Both are PERMUTATIONS, so
	 * the AABB, the UV rect and the welded topology above are untouched -- the
	 * boundary-edge counts below are what proves that, per mesh, on real data.
	 *
	 * meshopt_analyzeVertexCache's ACMR is the average number of vertex-shader
	 * invocations a triangle costs at the named cache size. 16 entries is the
	 * conventional post-transform figure and is also the cluster cap here, so
	 * the number reads directly as "vertices a 16-triangle cluster will
	 * fetch". Triangle-weighted across the model's shapes. */
	std::vector<LodoSrcShape> work = shapesIn;
	/* SELF-AO (2026-09-18). Every vertex's own occlusion by the model's own
	 * triangles, cast with the chunk bake's caster and rays (src/lodgenao.h,
	 * the same `ambientOcclusion` that fills a `.BTO` vertex's colour B), in
	 * MODEL space over a bin field the size of the model's footprint, rays as
	 * long as the model's half-diagonal. Constant across copies, so it lives
	 * here in the library; the per-placement byte in the `.lodi` carries what
	 * the surroundings add. Cast BEFORE the cache-order permutation so the
	 * remap below carries it like sway. A shape that arrived with its own
	 * `ao` keeps it. */
	{
		bool needCast = false;
		for ( const LodoSrcShape & s : work )
			if ( s.ao.size() != s.pos.size() / 3 )
				needCast = true;
		if ( needCast ) {
			LodgenAoScene scene;
			scene.ox = mn[0] - 1.0f;
			scene.oy = mn[1] - 1.0f;
			scene.span = std::max( mesh.aabbExtent[0], mesh.aabbExtent[1] ) + 2.0f;
			for ( const LodoSrcShape & s : work )
				for ( size_t t = 0; t + 2 < s.tris.size(); t += 3 ) {
					const float * a = &s.pos[size_t( s.tris[t] ) * 3];
					const float * b = &s.pos[size_t( s.tris[t + 1] ) * 3];
					const float * c = &s.pos[size_t( s.tris[t + 2] ) * 3];
					scene.addTriangle( Vector3( a[0], a[1], a[2] ), Vector3( b[0], b[1], b[2] ),
						Vector3( c[0], c[1], c[2] ) );
				}
			const float maxT = std::max( 8.0f, meshRadius );
			for ( LodoSrcShape & s : work ) {
				const size_t nv = s.pos.size() / 3;
				if ( s.ao.size() == nv )
					continue;
				s.ao.assign( nv, 255 );
				for ( size_t v = 0; v < nv; v++ ) {
					const Vector3 p( s.pos[v * 3], s.pos[v * 3 + 1], s.pos[v * 3 + 2] );
					const Vector3 n( s.nrm[v * 3], s.nrm[v * 3 + 1], s.nrm[v * 3 + 2] );
					const float ao = scene.ambientOcclusion( p, n, maxT );
					s.ao[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, ao ) ) * 255.0f ) );
				}
			}
		}
	}
	const bool cacheOrder = ( lib.flags & LODO_FLAG_CACHE_ORDER ) != 0;
	{
		double acmrB = 0.0, acmrA = 0.0, atvrB = 0.0, atvrA = 0.0;
		size_t triW = 0;
		std::vector<unsigned int> tmp, remap;
		for ( LodoSrcShape & s : work ) {
			const size_t nv = s.pos.size() / 3, nt = s.tris.size() / 3;
			const meshopt_VertexCacheStatistics cb = meshopt_analyzeVertexCache( s.tris.data(), s.tris.size(), nv, 16, 0, 0 );
			const meshopt_VertexFetchStatistics fb = meshopt_analyzeVertexFetch( s.tris.data(), s.tris.size(), nv, LODO_VERTEX_STRIDE );
			acmrB += double( cb.acmr ) * double( nt );
			atvrB += double( fb.overfetch ) * double( nt );
			LodoSrcShape keep;
			if ( cacheOrder ) {
				keep = s;
				tmp.assign( s.tris.begin(), s.tris.end() );
				meshopt_optimizeVertexCache( tmp.data(), tmp.data(), tmp.size(), nv );
				remap.assign( nv, 0u );
				lodoFetchRemapWhole( remap.data(), tmp.data(), tmp.size(), nv );
				// apply the remap by hand: four parallel attribute arrays, not one interleaved buffer
				std::vector<float> pos( nv * 3 ), nrm( nv * 3 ), tan( nv * 3 ), uv( nv * 2 );
				std::vector<quint8> sway, ao, rgba;
				if ( !s.rgba.empty() )
					rgba.assign( nv * 4, 255 );
				if ( !s.sway.empty() )
					sway.assign( nv, 0 );
				if ( !s.ao.empty() )
					ao.assign( nv, 255 );
				for ( size_t v = 0; v < nv; v++ ) {
					const size_t d = remap[v];
					for ( int k = 0; k < 3; k++ ) {
						pos[d * 3 + k] = s.pos[v * 3 + k];
						nrm[d * 3 + k] = s.nrm[v * 3 + k];
						tan[d * 3 + k] = s.tan[v * 3 + k];
					}
					uv[d * 2] = s.uv[v * 2];
					uv[d * 2 + 1] = s.uv[v * 2 + 1];
					if ( !sway.empty() )
						sway[d] = s.sway[v];
					if ( !ao.empty() )
						ao[d] = s.ao[v];
					if ( !rgba.empty() )
						for ( int k = 0; k < 4; k++ )
							rgba[d * 4 + k] = s.rgba[v * 4 + k];
				}
				s.pos.swap( pos ); s.nrm.swap( nrm ); s.tan.swap( tan ); s.uv.swap( uv );
				if ( !sway.empty() )
					s.sway.swap( sway );
				if ( !ao.empty() )
					s.ao.swap( ao );
				if ( !rgba.empty() )
					s.rgba.swap( rgba );
				s.tris.assign( tmp.size(), 0u );
				for ( size_t i = 0; i < tmp.size(); i++ )
					s.tris[i] = remap[tmp[i]];
			}
			meshopt_VertexCacheStatistics ca = meshopt_analyzeVertexCache( s.tris.data(), s.tris.size(), nv, 16, 0, 0 );
			meshopt_VertexFetchStatistics fa = meshopt_analyzeVertexFetch( s.tris.data(), s.tris.size(), nv, LODO_VERTEX_STRIDE );
			/* KEEP THE BETTER ORDER. meshopt's cache optimiser is a heuristic
			 * and on 21 of the Commonwealth's 2,982 LOD meshes it read WORSE
			 * than the order Bethesda shipped. Both orders are measured
			 * already, so taking the better one costs nothing and makes "no
			 * mesh reads worse" true by construction. */
			if ( cacheOrder && ca.acmr > cb.acmr ) {
				s = keep;
				ca = cb;
				fa = fb;
			}
			// the AFTER columns describe the order actually EMITTED
			acmrA += double( ca.acmr ) * double( nt );
			atvrA += double( fa.overfetch ) * double( nt );
			triW += nt;
		}
		if ( stats && triW ) {
			stats->acmrBefore = float( acmrB / double( triW ) );
			stats->acmrAfter = float( acmrA / double( triW ) );
			stats->atvrBefore = float( atvrB / double( triW ) );
			stats->atvrAfter = float( atvrA / double( triW ) );
		}
	}

	/* The SOURCE silhouette, welded by the quantised library position so the
	 * shape split cannot invent a boundary edge that the emitted mesh then
	 * "closes". */
	quint32 boundarySrc = 0;
	{
		std::vector<quint32> allTris;
		std::vector<quint64> keys;
		std::unordered_map<quint64, quint32> weld;
		quint32 srcVerts = 0;
		for ( const LodoSrcShape & s : work ) {
			const size_t nv = s.pos.size() / 3;
			const quint32 base = quint32( keys.size() );
			for ( size_t v = 0; v < nv; v++ ) {
				const quint64 k = lodoWeldKey(
					lodoQuantU16( s.pos[v * 3], mesh.aabbMin[0], mesh.aabbExtent[0] ),
					lodoQuantU16( s.pos[v * 3 + 1], mesh.aabbMin[1], mesh.aabbExtent[1] ),
					lodoQuantU16( s.pos[v * 3 + 2], mesh.aabbMin[2], mesh.aabbExtent[2] ) );
				keys.push_back( k );
				auto ins = weld.emplace( k, quint32( weld.size() ) );
				(void) ins;
			}
			for ( quint32 t : s.tris )
				allTris.push_back( base + t );
			srcVerts += quint32( nv );
		}
		boundarySrc = lodoBoundaryEdges( allTris, [&]( quint32 v ) { return weld[keys[v]]; } );
		if ( stats ) {
			stats->srcVertices = srcVerts;
			stats->triangles = quint32( totalTris );
			stats->boundarySource = boundarySrc;
		}
	}
	/* v4 (lane NATIVE1c), mesh flags bit 2: WATERTIGHT. Zero boundary edges over
	 * the position weld -- the SAME measurement the occluder-box fit already
	 * makes -- written into the file so a consumer can pick shadow casters and
	 * occluders from the `.lodo` alone. */
	if ( boundarySrc == 0 )
		mesh.flags |= LODO_MESH_WATERTIGHT;

	/* pass 2: LEVEL 0 per material, then that material's LADDER (v3).
	 *
	 * The material is the outer run because a simplification group may never
	 * cross one: two materials are two textures, and an edge collapse across
	 * them drags one texture's geometry onto the other's. `order` is already
	 * material-sorted and stable, so walking it in runs gives the v3 cluster
	 * sort law -- (meshId, materialId, level, first triangle) -- by
	 * construction, with no index remap afterwards.
	 *
	 * LEVEL 0 IS EMITTED EXACTLY AS v2 EMITTED IT: the same per-shape greedy
	 * walk, the same caps, the same flush between shapes, the same attributes
	 * straight off the source shape. The ladder is appended after it. */
	const quint32 vertsBeforeMesh = quint32( lib.vertices.size() );
	const bool wantLadder = ( lib.flags & LODO_FLAG_LADDER ) != 0;
	std::vector<int> local;          // source vertex -> local index in the open cluster
	std::vector<quint32> members;    // local index -> source vertex
	std::vector<quint8> idx;         // 3 per triangle, local
	quint32 clustersL0 = 0, clustersLadder = 0;
	quint8 levelsMax = 0;
	float maxErr = 0.0f;
	quint32 weldedTotal = 0, uvConflicts = 0;
	quint32 groupsFormed = 0, refSmall = 0, refNoCut = 0, refFlat = 0, errExact = 0, errBounded = 0;
	quint32 refSilhouette = 0;
	quint32 refFoliage = 0, lvlRefSilhouette = 0;
	float silWorst = 1.0f;

	size_t oiRun = 0;
	while ( oiRun < order.size() ) {
		const quint16 matId = work[order[oiRun]].materialId;
		size_t oiEnd = oiRun;
		while ( oiEnd < order.size() && work[order[oiEnd]].materialId == matId )
			oiEnd++;

		/* THE WELD, for this material only: one entry per distinct QUANTISED
		 * library position over every shape the material covers. The ladder
		 * simplifies on it because an edge collapse cannot cross a split
		 * vertex, and the library stores two vertices at one quantised position
		 * as the same three numbers anyway. Attributes come from the FIRST
		 * contributor; a weld that merged two different UVs is COUNTED
		 * (`weldUvConflicts`) rather than assumed harmless, and it reaches only
		 * levels 1 and up -- level 0 never uses this array. */
		std::vector<float> wpos;
		std::vector<LodoEmitVert> wv;
		std::unordered_map<quint64, quint32> wmap;
		std::vector<std::vector<quint32>> weldOf( oiEnd - oiRun );
		for ( size_t k = oiRun; k < oiEnd; k++ ) {
			const LodoSrcShape & s = work[order[k]];
			const size_t nv = s.pos.size() / 3;
			weldOf[k - oiRun].assign( nv, 0 );
			for ( size_t v = 0; v < nv; v++ ) {
				const quint64 key = lodoWeldKey(
					lodoQuantU16( s.pos[v * 3], mesh.aabbMin[0], mesh.aabbExtent[0] ),
					lodoQuantU16( s.pos[v * 3 + 1], mesh.aabbMin[1], mesh.aabbExtent[1] ),
					lodoQuantU16( s.pos[v * 3 + 2], mesh.aabbMin[2], mesh.aabbExtent[2] ) );
				auto it = wmap.find( key );
				if ( it != wmap.end() ) {
					const quint32 w = it->second;
					if ( std::fabs( wv[w].uv[0] - s.uv[v * 2] ) > ( 1.0f / 256.0f )
						|| std::fabs( wv[w].uv[1] - s.uv[v * 2 + 1] ) > ( 1.0f / 256.0f ) )
						uvConflicts++;
					weldOf[k - oiRun][v] = w;
					continue;
				}
				LodoEmitVert e;
				for ( int c2 = 0; c2 < 3; c2++ ) {
					e.pos[c2] = s.pos[v * 3 + c2];
					e.nrm[c2] = s.nrm[v * 3 + c2];
					e.tan[c2] = s.tan[v * 3 + c2];
				}
				e.uv[0] = s.uv[v * 2];
				e.uv[1] = s.uv[v * 2 + 1];
				e.sway = s.sway.empty() ? 0 : s.sway[v];
				e.ao = s.ao.empty() ? 255 : s.ao[v];
				e.rgba = lodoSrcRgba( s, v );
				const quint32 w = quint32( wv.size() );
				wv.push_back( e );
				for ( int c2 = 0; c2 < 3; c2++ )
					wpos.push_back( e.pos[c2] );
				wmap.emplace( key, w );
				weldOf[k - oiRun][v] = w;
			}
		}
		weldedTotal += quint32( wv.size() );

		// the current level: cluster indices, their triangles (welded), and the
		// FULL-DETAIL triangles each of them stands for
		std::vector<quint32> curClusters;
		std::vector<std::vector<quint32>> curTris;
		std::vector<std::vector<quint32>> curCover;
		std::vector<quint32> l0Tris;         // this material's level-0 triangles, welded triples
		{
			size_t shapeLocal = 0;
			auto flush = [&]( const LodoSrcShape & s ) {
				if ( idx.empty() )
					return;
				std::vector<LodoEmitVert> cv;
				cv.reserve( members.size() );
				for ( quint32 v : members ) {
					LodoEmitVert e;
					for ( int c2 = 0; c2 < 3; c2++ ) {
						e.pos[c2] = s.pos[v * 3 + c2];
						e.nrm[c2] = s.nrm[v * 3 + c2];
						e.tan[c2] = s.tan[v * 3 + c2];
					}
					e.uv[0] = s.uv[v * 2];
					e.uv[1] = s.uv[v * 2 + 1];
					e.sway = s.sway.empty() ? 0 : s.sway[v];
					e.ao = s.ao.empty() ? 255 : s.ao[v];
					e.rgba = lodoSrcRgba( s, v );
					cv.push_back( e );
				}
				std::vector<quint32> tris, cover;
				for ( size_t t = 0; t + 2 < idx.size(); t += 3 ) {
					cover.push_back( quint32( l0Tris.size() / 3 ) );
					for ( int k = 0; k < 3; k++ ) {
						const quint32 w = weldOf[shapeLocal][members[idx[t + k]]];
						l0Tris.push_back( w );
						tris.push_back( w );
					}
				}
				const quint32 ci = lodoEmitCluster( lib, mesh, meshRadius, id, s.materialId, 0, cv, idx );
				curClusters.push_back( ci );
				curTris.push_back( tris );
				curCover.push_back( cover );
				clustersL0++;
				for ( quint32 v : members )
					local[v] = -1;
				members.clear();
				idx.clear();
			};
			for ( size_t k = oiRun; k < oiEnd; k++ ) {
				shapeLocal = k - oiRun;
				const LodoSrcShape & s = work[order[k]];
				local.assign( s.pos.size() / 3, -1 );
				for ( size_t t = 0; t < s.tris.size(); t += 3 ) {
					const quint32 tri[3] = { s.tris[t], s.tris[t + 1], s.tris[t + 2] };
					int fresh = 0;
					for ( int k2 = 0; k2 < 3; k2++ ) {
						bool seen = local[tri[k2]] >= 0;
						for ( int j = 0; j < k2 && !seen; j++ )
							seen = tri[j] == tri[k2];
						if ( !seen )
							fresh++;
					}
					if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS || members.size() + size_t( fresh ) > LODO_CLUSTER_MAX_VERTS )
						flush( s );
					for ( int k2 = 0; k2 < 3; k2++ ) {
						if ( local[tri[k2]] < 0 ) {
							local[tri[k2]] = int( members.size() );
							members.push_back( tri[k2] );
						}
						idx.push_back( quint8( local[tri[k2]] ) );
					}
				}
				flush( s );
			}
		}

		/* THE LADDER. Group the level's clusters, simplify each group as one
		 * mesh with its border LOCKED, re-split the result under the same caps,
		 * and repeat. A group that cannot be formed is not an error: its
		 * clusters simply stay ROOTS, and the reason is counted by name. */
		/* v4, THE FOLIAGE REFUSAL (lane NATIVE1c, bungo 2026-09-11 16:1x over
		 * native1b's ladder.png: "Hm, that tree LOD becomes a stump there").
		 * An ALPHA-TESTED material of a TREE is leaf cards: crossed quads whose
		 * outline lives in the texture's alpha, not in the geometry. Collapsing
		 * an edge there does not coarsen a crown, it deletes a quad -- and the
		 * crown goes to fragments and then to nothing, which is the stump. A
		 * tree's far representation is its CARD by his ruling, so the ladder
		 * does not try. The refusal is per MATERIAL: the same tree's TRUNK, and
		 * every opaque material of every other model, ladders exactly as before.
		 *
		 * `--native-ladder-foliage` is the exact way back. */
		bool foliage = false;
		if ( !lib.ladderFoliage && matId < lib.materials.size() ) {
			const LodoMaterial & lm = lib.materials[matId];
			foliage = lm.alphaThreshold != 0 && ( lm.flags & LODO_MAT_TREE ) != 0;
		}
		if ( foliage )
			refFoliage += quint32( curClusters.size() );
		if ( wantLadder && !foliage ) {
			auto splitClusters = [&]( const std::vector<quint32> & tris, quint16 mat2, quint8 lvl,
				std::vector<quint32> * outIdxs, std::vector<std::vector<quint32>> * outTris )
			{
				std::vector<int> lc( wv.size(), -1 );
				std::vector<quint32> mem2, tri2;
				std::vector<quint8> idx2;
				auto flush2 = [&]() {
					if ( idx2.empty() )
						return;
					std::vector<LodoEmitVert> cv;
					cv.reserve( mem2.size() );
					for ( quint32 v : mem2 )
						cv.push_back( wv[v] );
					outIdxs->push_back( lodoEmitCluster( lib, mesh, meshRadius, id, mat2, lvl, cv, idx2 ) );
					outTris->push_back( tri2 );
					for ( quint32 v : mem2 )
						lc[v] = -1;
					mem2.clear();
					idx2.clear();
					tri2.clear();
				};
				for ( size_t t = 0; t + 2 < tris.size(); t += 3 ) {
					const quint32 tri[3] = { tris[t], tris[t + 1], tris[t + 2] };
					int fresh = 0;
					for ( int k2 = 0; k2 < 3; k2++ ) {
						bool seen = lc[tri[k2]] >= 0;
						for ( int j = 0; j < k2 && !seen; j++ )
							seen = tri[j] == tri[k2];
						if ( !seen )
							fresh++;
					}
					if ( idx2.size() / 3 >= LODO_CLUSTER_MAX_TRIS || mem2.size() + size_t( fresh ) > LODO_CLUSTER_MAX_VERTS )
						flush2();
					for ( int k2 = 0; k2 < 3; k2++ ) {
						if ( lc[tri[k2]] < 0 ) {
							lc[tri[k2]] = int( mem2.size() );
							mem2.push_back( tri[k2] );
						}
						idx2.push_back( quint8( lc[tri[k2]] ) );
						tri2.push_back( tri[k2] );
					}
				}
				flush2();
			};

			/* v4 (lane NATIVE1c): the roots this material has already dropped.
			 * A level's CUT -- what a consumer draws when it selects that level
			 * -- is the level's own clusters PLUS every cluster that stayed a
			 * root below it, because a ladder is PARTIAL wherever a group
			 * refuses (docs 11, deviation 11). The silhouette gate measures the
			 * CUT, never the level's fragment. */
			std::vector<quint32> rootTris;

			quint8 level = 0;
			while ( level < quint8( LODO_LADDER_MAX_LEVEL ) && !curClusters.empty() ) {
				/* THE ROLLBACK POINT. The level is built first and judged
				 * afterwards, because the cut cannot be measured before it
				 * exists; a level the silhouette gate refuses is then UNDONE --
				 * every table truncated to its length here, every parent link
				 * put back to a root, every counter restored -- so a refused
				 * level leaves no trace in the file at all. */
				const size_t snapClusters = lib.clusters.size();
				const size_t snapLods = lib.clusterLods.size();
				const size_t snapLocal = lib.localIndices.size();
				const size_t snapVerts = lib.vertices.size();
				const quint32 snapLadder = clustersLadder, snapFormed = groupsFormed;
				const quint32 snapSmall = refSmall, snapNoCut = refNoCut, snapFlat = refFlat;
				const quint32 snapSil = refSilhouette, snapExact = errExact, snapBounded = errBounded;
				const float snapMaxErr = maxErr;
				std::vector<unsigned char> consumed( curClusters.size(), 0 );

				size_t levelTris = 0;
				for ( const std::vector<quint32> & t : curTris )
					levelTris += t.size() / 3;
				if ( levelTris <= size_t( LODO_LADDER_MIN_TRIS ) ) {
					refSmall++;
					break;
				}
				std::vector<unsigned int> part( curClusters.size(), 0u );
				size_t nParts = 1;
				if ( curClusters.size() > 1 ) {
					std::vector<unsigned int> flat, counts;
					for ( const std::vector<quint32> & t : curTris ) {
						counts.push_back( (unsigned int) t.size() );
						flat.insert( flat.end(), t.begin(), t.end() );
					}
					nParts = meshopt_partitionClusters( part.data(), flat.data(), flat.size(),
						counts.data(), counts.size(), wpos.data(), wv.size(), sizeof( float ) * 3,
						size_t( LODO_LADDER_GROUP ) );
				}
				std::vector<quint32> nextClusters;
				std::vector<std::vector<quint32>> nextTris, nextCover;
				bool anyFormed = false;
				for ( size_t g = 0; g < nParts; g++ ) {
					std::vector<size_t> mem;
					for ( size_t i = 0; i < curClusters.size(); i++ )
						if ( size_t( part[i] ) == g )
							mem.push_back( i );
					if ( mem.empty() )
						continue;
					std::vector<quint32> gTris, gCover;
					float childErr = 0.0f;
					for ( size_t i : mem ) {
						gTris.insert( gTris.end(), curTris[i].begin(), curTris[i].end() );
						gCover.insert( gCover.end(), curCover[i].begin(), curCover[i].end() );
						childErr = std::max( childErr, lib.clusterLods[curClusters[i]].geometricError );
					}
					if ( gTris.size() / 3 <= size_t( LODO_LADDER_MIN_TRIS ) ) {
						refSmall++;
						continue;
					}
					/* LOCK the group's border with the REST OF THIS LEVEL. A
					 * vertex another group still uses may not move: if it did,
					 * a consumer that replaced this group and kept the
					 * neighbour would see a crack along the shared edge. This
					 * is the whole reason a ladder is built on GROUPS and not
					 * on clusters. */
					std::vector<unsigned char> lockv( wv.size(), 0 );
					for ( size_t i = 0; i < curClusters.size(); i++ ) {
						if ( size_t( part[i] ) == g )
							continue;
						for ( quint32 v : curTris[i] )
							lockv[v] = 1;
					}
					const size_t target = std::max( size_t( LODO_LADDER_MIN_TRIS ) * 3,
						( gTris.size() / 2 / 3 ) * 3 );
					std::vector<unsigned int> dst( gTris.size(), 0u );
					float rel = 0.0f;
					const size_t n = meshopt_simplifyWithAttributes( dst.data(), gTris.data(), gTris.size(),
						wpos.data(), wv.size(), sizeof( float ) * 3, nullptr, 0, nullptr, 0,
						lockv.data(), target, 1.0f, 0, &rel );
					if ( n < 3 || n >= gTris.size() ) {
						refNoCut++;
						continue;
					}
					dst.resize( n );
					const std::vector<quint32> outSoup( dst.begin(), dst.end() );
					/* THE SHADOW-CASTER RULE, at the ladder's own level. bungo
					 * 2026-09-11 08:3x: "a far shadow cast by a LOD tower behind
					 * me will cover the area I'm at" -- so a simplification may
					 * never open a hole. A locked-border collapse can split a
					 * fan into two shells and RAISE the boundary-edge count;
					 * measured on the nine-chunk region before this refusal
					 * existed, fourteen meshes had a coarse level above level
					 * 0's count and two of them went from WATERTIGHT to four and
					 * eight edges. The group is refused; its clusters stay roots
					 * and the mesh keeps whatever levels it already earned. */
					const auto identityKey = []( quint32 v ) { return v; };
					if ( lodoBoundaryEdges( outSoup, identityKey ) > lodoBoundaryEdges( gTris, identityKey ) ) {
						refSilhouette++;
						continue;
					}
					/* THE ERROR, against FULL detail and never against the
					 * parent, so one stored number meets one tolerance. Under
					 * LODO_ERROR_EXACT_TRIS full-detail triangles it is measured
					 * directly; above it the chain bound (child + this step)
					 * serves, which the triangle inequality makes an upper
					 * bound. Which rule served is counted. */
					float E = 0.0f;
					if ( gCover.size() <= size_t( LODO_ERROR_EXACT_TRIS ) ) {
						std::vector<quint32> fullSoup;
						fullSoup.reserve( gCover.size() * 3 );
						for ( quint32 t : gCover )
							for ( int k2 = 0; k2 < 3; k2++ )
								fullSoup.push_back( l0Tris[size_t( t ) * 3 + k2] );
						E = lodoSoupDeviation( fullSoup, outSoup, wpos );
						errExact++;
					} else {
						E = childErr + lodoSoupDeviation( gTris, outSoup, wpos );
						errBounded++;
					}
					E = std::max( E, childErr );
					if ( !( E > childErr ) ) {
						// the error did not grow: the brief's own stopping rule,
						// and what keeps every chain STRICTLY increasing
						refFlat++;
						continue;
					}
					std::vector<quint32> outIdxs;
					std::vector<std::vector<quint32>> outTris;
					splitClusters( outSoup, matId, quint8( level + 1 ), &outIdxs, &outTris );
					if ( outIdxs.empty() ) {
						refNoCut++;
						continue;
					}
					/* COVERAGE. Every full-detail triangle under the group goes
					 * to the ONE output cluster whose sphere centre is nearest
					 * its centroid, so `sourceTriangles` over any cut sums to
					 * the mesh's own level-0 triangle count exactly -- the
					 * partition invariant a reader can check without
					 * reconstructing a single vertex. */
					std::vector<std::vector<quint32>> outCover( outIdxs.size() );
					for ( quint32 t : gCover ) {
						float cen[3] = { 0.0f, 0.0f, 0.0f };
						for ( int k2 = 0; k2 < 3; k2++ ) {
							const quint32 v = l0Tris[size_t( t ) * 3 + k2];
							for ( int c2 = 0; c2 < 3; c2++ )
								cen[c2] += wpos[size_t( v ) * 3 + c2] / 3.0f;
						}
						size_t best = 0;
						float bestD = 3.4e38f;
						for ( size_t j = 0; j < outIdxs.size(); j++ ) {
							const LodoClusterLod & Lo = lib.clusterLods[outIdxs[j]];
							float d2 = 0.0f;
							for ( int c2 = 0; c2 < 3; c2++ ) {
								const float d = cen[c2] - Lo.centre[c2];
								d2 += d * d;
							}
							if ( d2 < bestD ) {
								bestD = d2;
								best = j;
							}
						}
						outCover[best].push_back( t );
					}
					for ( size_t j = 0; j < outIdxs.size(); j++ ) {
						LodoClusterLod & Lo = lib.clusterLods[outIdxs[j]];
						Lo.geometricError = E;
						Lo.sourceTriangles = quint32( outCover[j].size() );
					}
					for ( size_t i : mem ) {
						LodoClusterLod & Lc = lib.clusterLods[curClusters[i]];
						Lc.parentError = E;
						Lc.parentFirst = outIdxs.front();
						Lc.parentCount = quint16( outIdxs.size() );
						consumed[i] = 1;
					}
					maxErr = std::max( maxErr, E );
					clustersLadder += quint32( outIdxs.size() );
					groupsFormed++;
					anyFormed = true;
					for ( size_t j = 0; j < outIdxs.size(); j++ ) {
						nextClusters.push_back( outIdxs[j] );
						nextTris.push_back( outTris[j] );
						nextCover.push_back( outCover[j] );
					}
				}
				if ( !anyFormed )
					break;

				/* v4, THE SILHOUETTE GATE (lane NATIVE1c). The CUT at the new
				 * level -- the roots dropped below it, the clusters this level
				 * did not consume, and the level's own output -- must keep at
				 * least `lib.silhouetteMin` of LEVEL 0's silhouette from the
				 * worst of LODO_SILHOUETTE_VIEWS horizon azimuths. A building
				 * may shrink; a tree may never become a stump. A ratio of 0
				 * turns the gate off, which is the exact way back to the v3
				 * ladder. */
				if ( lib.silhouetteMin > 0.0f ) {
					std::vector<quint32> cut = rootTris;
					for ( size_t i = 0; i < curClusters.size(); i++ )
						if ( !consumed[i] )
							cut.insert( cut.end(), curTris[i].begin(), curTris[i].end() );
					for ( const std::vector<quint32> & t : nextTris )
						cut.insert( cut.end(), t.begin(), t.end() );
					const float ratio = lodoSilhouetteRatio( cut, l0Tris, wpos,
						mesh.aabbMin, mesh.aabbExtent );
					if ( ratio < lib.silhouetteMin ) {
						// UNDO the level in full, then stop laddering this material
						for ( size_t i = 0; i < curClusters.size(); i++ ) {
							LodoClusterLod & Lc = lib.clusterLods[curClusters[i]];
							Lc.parentError = LODO_ERROR_ROOT;
							Lc.parentFirst = LODO_NO_PARENT;
							Lc.parentCount = 0;
						}
						lib.clusters.resize( snapClusters );
						lib.clusterLods.resize( snapLods );
						lib.localIndices.resize( snapLocal );
						lib.vertices.resize( snapVerts );
						lib.colours.resize( snapVerts );
						clustersLadder = snapLadder; groupsFormed = snapFormed;
						refSmall = snapSmall; refNoCut = snapNoCut; refFlat = snapFlat;
						refSilhouette = snapSil; errExact = snapExact; errBounded = snapBounded;
						maxErr = snapMaxErr;
						lvlRefSilhouette++;
						break;
					}
					silWorst = std::min( silWorst, ratio );
				}

				// the clusters this level did not consume are roots from here on
				for ( size_t i = 0; i < curClusters.size(); i++ )
					if ( !consumed[i] )
						rootTris.insert( rootTris.end(), curTris[i].begin(), curTris[i].end() );

				level = quint8( level + 1 );
				levelsMax = std::max( levelsMax, level );
				curClusters.swap( nextClusters );
				curTris.swap( nextTris );
				curCover.swap( nextCover );
			}
		}
		oiRun = oiEnd;
	}
	if ( lib.clusters.size() - mesh.clusterFirst > 0xFFFF )
		return fail( QStringLiteral( "more than 65,535 clusters in one mesh" ) );
	mesh.clusterCount = quint16( lib.clusters.size() - mesh.clusterFirst );
	mesh.clusterCountL0 = quint16( clustersL0 );
	mesh.levelCount = quint8( levelsMax + 1 );
	mesh.reserved = 0;

	/* The EMITTED silhouette, at LEVEL 0 and at the coarsest level: the same
	 * welded topology, rebuilt from the clusters exactly as a consumer will
	 * draw them. A cluster copies its vertices, so this count is only
	 * comparable because both sides weld by the quantised position. Level 0 is
	 * the one the shadow-caster refusal reads -- it must equal the source's,
	 * because level 0 is a permutation and not a decimation. The coarsest
	 * level's count is REPORTED beside it: a simplified level may close a hole
	 * but must never open one. */
	if ( stats ) {
		stats->emittedVertices = quint32( lib.vertices.size() ) - vertsBeforeMesh;
		{
			double sum = 0.0;
			quint32 dark = 0;
			for ( size_t v = vertsBeforeMesh; v < lib.vertices.size(); v++ ) {
				sum += lib.vertices[v].selfAO;
				if ( lib.vertices[v].selfAO < 128 )
					dark++;
			}
			const size_t n = lib.vertices.size() - vertsBeforeMesh;
			stats->selfAoMean = n ? float( sum / ( 255.0 * double( n ) ) ) : 1.0f;
			stats->selfAoDark = dark;
		}
		auto boundaryAtLevel = [&]( quint8 lvl ) {
			std::vector<quint32> allTris;
			std::vector<quint64> keys;
			std::unordered_map<quint64, quint32> weld;
			for ( size_t c = mesh.clusterFirst; c < lib.clusters.size(); c++ ) {
				if ( lib.clusterLods[c].level != lvl )
					continue;
				const LodoCluster & cl = lib.clusters[c];
				const quint32 base = quint32( keys.size() );
				for ( quint32 v = 0; v < cl.vertexCount; v++ ) {
					const LodoVertex & lv = lib.vertices[cl.vertexBase + v];
					const quint64 k = lodoWeldKey( lv.pos[0], lv.pos[1], lv.pos[2] );
					keys.push_back( k );
					weld.emplace( k, quint32( weld.size() ) );
				}
				const quint8 * li = &lib.localIndices[c * LODO_LOCAL_INDEX_BYTES];
				for ( quint32 t = 0; t < quint32( cl.triangleCount ) * 3; t++ )
					allTris.push_back( base + li[t] );
			}
			return lodoBoundaryEdges( allTris, [&]( quint32 v ) { return weld[keys[v]]; } );
		};
		stats->boundaryEmitted = boundaryAtLevel( 0 );
		stats->boundaryCoarsest = levelsMax ? boundaryAtLevel( levelsMax ) : stats->boundaryEmitted;
		stats->clustersL0 = clustersL0;
		stats->clustersLadder = clustersLadder;
		stats->levelCount = quint8( levelsMax + 1 );
		stats->groupsFormed = groupsFormed;
		stats->groupsRefusedSmall = refSmall;
		stats->groupsRefusedNoCut = refNoCut;
		stats->groupsRefusedFlatErr = refFlat;
		stats->groupsRefusedSilhouette = refSilhouette;
		stats->groupsRefusedFoliage = refFoliage;
		stats->levelsRefusedSilhouette = lvlRefSilhouette;
		stats->silhouetteWorst = silWorst;
		stats->errorsExact = errExact;
		stats->errorsBounded = errBounded;
		stats->maxError = maxErr;
		stats->weldedVertices = weldedTotal;
		stats->weldUvConflicts = uvConflicts;
	}

	lib.meshes.push_back( mesh );
	if ( meshId )
		*meshId = id;
	return true;
}

/* ---- the same append, split so a worker can run it (lane PERF1) ---------- */

bool lodoStageMesh( LodoLibrary & staged, const LodoLibrary & like,
	const std::vector<LodoSrcShape> & shapes, const QString & name,
	QString * error, LodoMeshStats * stats )
{
	/* EXACTLY what `lodoAppendMesh` READS out of the library it appends to, and
	 * nothing else. If a future field is added to that read set and not to this
	 * one, the byte-identity gate goes red on the first region that exercises
	 * it -- which is the whole reason the seed is written out field by field
	 * here instead of copying `like` wholesale. */
	staged = LodoLibrary();
	staged.flags = like.flags;
	staged.ladderFoliage = like.ladderFoliage;
	staged.silhouetteMin = like.silhouetteMin;
	staged.materials = like.materials;
	staged.worldspaceEdid = like.worldspaceEdid;
	quint16 id = 0;
	return lodoAppendMesh( staged, shapes, name, &id, error, stats );
}

bool lodoMergeStagedMesh( LodoLibrary & lib, const LodoLibrary & staged,
	const QString & name, quint16 * meshId, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QString( "%1: %2" ).arg( name, m );
		return false;
	};
	if ( staged.meshes.size() != 1 )
		return fail( QStringLiteral( "the staged library does not hold exactly one mesh" ) );
	if ( lib.meshes.size() >= size_t( LODO_NO_MESH ) )
		return fail( QStringLiteral( "the mesh table is full (65,535 rows, the u16 index)" ) );

	const quint32 clusterBase = quint32( lib.clusters.size() );
	const quint32 vertexBase  = quint32( lib.vertices.size() );
	const quint16 id = quint16( lib.meshes.size() );

	for ( LodoCluster c : staged.clusters ) {
		c.vertexBase += vertexBase;
		c.meshId = id;
		lib.clusters.push_back( c );
	}
	for ( LodoClusterLod L : staged.clusterLods ) {
		if ( L.parentFirst != LODO_NO_PARENT )
			L.parentFirst += clusterBase;
		lib.clusterLods.push_back( L );
	}
	lib.localIndices.insert( lib.localIndices.end(),
		staged.localIndices.begin(), staged.localIndices.end() );
	lib.vertices.insert( lib.vertices.end(), staged.vertices.begin(), staged.vertices.end() );
	// v5: the parallel colours, padded first if `lib` had none so far
	if ( lib.colours.size() != size_t( vertexBase ) )
		lib.colours.resize( size_t( vertexBase ), LODO_COLOUR_NONE );
	if ( staged.colours.size() == staged.vertices.size() )
		lib.colours.insert( lib.colours.end(), staged.colours.begin(), staged.colours.end() );
	else
		lib.colours.resize( lib.vertices.size(), LODO_COLOUR_NONE );

	LodoMesh mesh = staged.meshes[0];
	mesh.clusterFirst += clusterBase;
	/* The string goes into the REAL table, at whatever offset it lands: the
	 * staged table's offset means nothing here. `addString` is the one place
	 * that decides, so an identical name still de-duplicates identically. */
	mesh.modelStringOffset = lib.addString( name );
	lib.meshes.push_back( mesh );
	if ( meshId )
		*meshId = id;
	return true;
}

/* ---------------------------------------------------------------- writer */

bool lodoWrite( const QString & path, const LodoLibrary & lib, LodoHeader * headerOut, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	const QByteArray edid = lib.worldspaceEdid.toUtf8();
	if ( edid.size() >= H_EDID_BYTES )
		return fail( QString( "worldspace editor ID '%1' is %2 bytes; the header holds 31 + NUL (refused, never truncated)" )
			.arg( lib.worldspaceEdid ).arg( edid.size() ) );
	if ( ( lib.flags & ~LODO_FLAGS_KNOWN ) || !( lib.flags & LODO_FLAG_VERTEX_V1 ) )
		return fail( QString( "flags 0x%1: bit0 must be set and bits above 1 are reserved" ).arg( lib.flags, 0, 16 ) );
	if ( lib.localIndices.size() != lib.clusters.size() * LODO_LOCAL_INDEX_BYTES )
		return fail( QStringLiteral( "local-index blob is not 48 bytes per cluster" ) );
	if ( lib.clusterLods.size() != lib.clusters.size() )
		return fail( QString( "the v3 ladder table has %1 rows for %2 clusters; it is PARALLEL, one row a cluster" )
			.arg( lib.clusterLods.size() ).arg( lib.clusters.size() ) );
	QByteArray strings = lib.strings;
	if ( strings.isEmpty() )
		strings.append( '\0' );
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

	QByteArray file;
	file.resize( LODO_HEADER_BYTES );
	std::memset( file.data(), 0, LODO_HEADER_BYTES );
	/* Each payload at the next 4,096-aligned offset. QByteArray::resize() leaves
	 * new bytes UNINITIALISED in Qt 6, so the pad is zeroed by hand: a pad
	 * that carried heap garbage would make two writes of one library differ. */
	auto payload = [&]( const void * p, quint64 bytes ) {
		const quint64 start = quint64( file.size() );
		const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );
		file.resize( qsizetype( at + bytes ) );
		if ( at > start )
			std::memset( file.data() + qsizetype( start ), 0, size_t( at - start ) );
		if ( bytes )
			std::memcpy( file.data() + qsizetype( at ), p, size_t( bytes ) );
		return at;
	};
	quint32 icrc = 0;
	auto crcOver = [&]( const void * p, quint64 bytes ) {
		icrc = lodvCrc32( static_cast<const unsigned char *>( p ), qsizetype( bytes ), icrc );
	};
	LodoHeader h;
	h.flags = lib.flags;
	/* v7 (lane NEAR1): the version follows the NEAR flag and nothing else, so a
	 * far-field library -- which never sets it -- is written as v6, unchanged. */
	h.version = ( lib.flags & LODO_FLAG_NEAR ) ? LODO_VERSION_NEAR : LODO_VERSION;
	for ( size_t i = 0; i < lib.materials.size(); i++ ) {
		if ( lib.materials[i].features & ~LODO_MAT_FEATURES_KNOWN )
			return fail( QString( "material %1: features 0x%2 set reserved bits" ).arg( i ).arg( lib.materials[i].features, 0, 16 ) );
		if ( lib.materials[i].features && !( lib.flags & LODO_FLAG_NEAR ) )
			return fail( QString( "material %1: features 0x%2 on a library without the NEAR flag (v7 only)" )
				.arg( i ).arg( lib.materials[i].features, 0, 16 ) );
	}
	h.pluginCorpusHash = lib.pluginCorpusHash;
	h.objectCorpusHash = lib.objectCorpusHash;
	h.modelCorpusHash = lib.modelCorpusHash;
	h.cardCorpusHash = lib.cardCorpusHash;
	h.loadOrderHash = lib.loadOrderHash;
	h.worldspaceEdid = lib.worldspaceEdid;
	h.baseCount = quint32( lib.bases.size() );
	h.meshCount = quint32( lib.meshes.size() );
	h.clusterCount = quint32( lib.clusters.size() );
	h.materialCount = quint32( lib.materials.size() );
	h.vertexCount = quint32( lib.vertices.size() );
	h.maxClustersPerMesh = 0;
	for ( const LodoMesh & m : lib.meshes )
		h.maxClustersPerMesh = std::max<quint32>( h.maxClustersPerMesh, m.clusterCount );
	h.stringBytes = quint32( strings.size() );
	h.clusterLodStride = quint32( sizeof( LodoClusterLod ) );
	h.levelMax = 0;
	for ( const LodoClusterLod & cl : lib.clusterLods )
		h.levelMax = std::max( h.levelMax, cl.level );
	h.ladderGroup = ( lib.flags & LODO_FLAG_LADDER ) ? quint8( LODO_LADDER_GROUP ) : quint8( 0 );
	/* v4, header 0xD0: how many bases carry a card. Counted from the rows, never
	 * taken from the caller, so the header cannot disagree with the table. */
	h.cardCount = 0;
	for ( const LodoBase & b : lib.bases )
		if ( b.cardLayer != LODO_NO_CARD )
			h.cardCount++;

	h.offBases = payload( lib.bases.data(), quint64( lib.bases.size() ) * sizeof( LodoBase ) );
	crcOver( lib.bases.data(), quint64( lib.bases.size() ) * sizeof( LodoBase ) );
	h.offMeshes = payload( lib.meshes.data(), quint64( lib.meshes.size() ) * sizeof( LodoMesh ) );
	crcOver( lib.meshes.data(), quint64( lib.meshes.size() ) * sizeof( LodoMesh ) );
	h.offClusters = payload( lib.clusters.data(), quint64( lib.clusters.size() ) * sizeof( LodoCluster ) );
	crcOver( lib.clusters.data(), quint64( lib.clusters.size() ) * sizeof( LodoCluster ) );
	/* v3: the ladder table sits immediately after the clusters it parallels, and
	 * it is INSIDE `indexCrc32` -- which is what makes the v2 CRC arithmetic
	 * different from the v3 one, so a v2 file cannot be read as a v3 one even if
	 * a reader ignored the version word. */
	h.offClusterLods = payload( lib.clusterLods.data(), quint64( lib.clusterLods.size() ) * sizeof( LodoClusterLod ) );
	crcOver( lib.clusterLods.data(), quint64( lib.clusterLods.size() ) * sizeof( LodoClusterLod ) );
	h.offMaterials = payload( lib.materials.data(), quint64( lib.materials.size() ) * sizeof( LodoMaterial ) );
	crcOver( lib.materials.data(), quint64( lib.materials.size() ) * sizeof( LodoMaterial ) );
	h.offLocalIndices = payload( lib.localIndices.data(), quint64( lib.localIndices.size() ) );
	crcOver( lib.localIndices.data(), quint64( lib.localIndices.size() ) );
	h.offVertices = payload( lib.vertices.data(), quint64( lib.vertices.size() ) * sizeof( LodoVertex ) );
	crcOver( lib.vertices.data(), quint64( lib.vertices.size() ) * sizeof( LodoVertex ) );
	h.offStrings = payload( strings.constData(), quint64( strings.size() ) );
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
	h.indexCrc32 = icrc;
	h.fileBytes = quint64( file.size() );

	putLE<quint32>( file, H_MAGIC, LODO_MAGIC );
	putLE<quint32>( file, H_VERSION, h.version );
	putLE<quint32>( file, H_FLAGS, h.flags );
	putLE<quint64>( file, H_PLUGIN, h.pluginCorpusHash );
	putLE<quint64>( file, H_OBJECT, h.objectCorpusHash );
	putLE<quint64>( file, H_MODEL, h.modelCorpusHash );
	putLE<quint64>( file, H_CARD, h.cardCorpusHash );
	std::memcpy( file.data() + H_EDID, edid.constData(), size_t( edid.size() ) );
	putLE<quint32>( file, H_BASES, h.baseCount );
	putLE<quint32>( file, H_MESHES, h.meshCount );
	putLE<quint32>( file, H_CLUSTERS, h.clusterCount );
	putLE<quint32>( file, H_MATERIALS, h.materialCount );
	putLE<quint32>( file, H_VERTICES, h.vertexCount );
	putLE<quint32>( file, H_MAXCLUSTERS, h.maxClustersPerMesh );
	putLE<quint16>( file, H_MAXTRIS, LODO_CLUSTER_MAX_TRIS );
	putLE<quint16>( file, H_VSTRIDE, LODO_VERTEX_STRIDE );
	putLE<quint32>( file, H_STRBYTES, h.stringBytes );
	putLE<quint64>( file, H_OFF_BASES, h.offBases );
	putLE<quint64>( file, H_OFF_MESHES, h.offMeshes );
	putLE<quint64>( file, H_OFF_CLUSTERS, h.offClusters );
	putLE<quint64>( file, H_OFF_MATERIALS, h.offMaterials );
	putLE<quint64>( file, H_OFF_LOCAL, h.offLocalIndices );
	putLE<quint64>( file, H_OFF_VERTS, h.offVertices );
	putLE<quint64>( file, H_OFF_STRINGS, h.offStrings );
	putLE<quint32>( file, H_ICRC, h.indexCrc32 );
	putLE<quint64>( file, H_FILEBYTES, h.fileBytes );
	putLE<quint64>( file, H_LOADORDER, h.loadOrderHash );
	putLE<quint64>( file, H_OFF_CLUSTERLODS, h.offClusterLods );
	putLE<quint32>( file, H_CLUSTERLOD_STRIDE, h.clusterLodStride );
	putLE<quint8>( file, H_LEVELMAX, h.levelMax );
	putLE<quint8>( file, H_LADDERGROUP, h.ladderGroup );
	putLE<quint32>( file, H_CARDCOUNT, h.cardCount );
	putLE<quint32>( file, H_COLOURCOUNT, h.colourVertexCount );
	putLE<quint64>( file, H_OFF_COLOURS, h.offColours );
	h.headerCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( file.constData() ) + H_PLUGIN,
		LODO_HEADER_BYTES - H_PLUGIN );
	putLE<quint32>( file, H_HCRC, h.headerCrc32 );

	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QString( "cannot write %1" ).arg( path ) );
	if ( f.write( file ) != file.size() )
		return fail( QString( "short write to %1" ).arg( path ) );
	f.close();
	if ( headerOut )
		*headerOut = h;
	return true;
}

/* ---------------------------------------------------------------- reader */

bool lodoRead( const QString & path, LodoHeader * header, LodoLibrary * lib,
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
	if ( file.size() < qsizetype( LODO_HEADER_BYTES ) )
		return refuse( QString( "%1 bytes, shorter than the 256-byte header" ).arg( file.size() ) );
	const unsigned char * p = reinterpret_cast<const unsigned char *>( file.constData() );

	const quint32 magic = getLE<quint32>( p + H_MAGIC );
	if ( magic != LODO_MAGIC )
		return refuse( QString( "not a .lodo object library: the magic says %1" ).arg( nameOtherMagic( magic ) ) );
	LodoHeader h;
	h.version = getLE<quint32>( p + H_VERSION );
	if ( h.version == 1 )
		return refuse( QStringLiteral( "version 1: the v1 vertex blob is in SOURCE order and carries no "
			"loadOrderHash (header 0xB8 was reserved). Re-bake; this reader knows version 4 only" ) );
	if ( h.version == 2 )
		return refuse( QStringLiteral( "version 2: a v2 library has NO cluster ladder table (header 0xC0 was "
			"reserved), so every cluster would read geometricError 0 and parentError 0 and a consumer would "
			"draw the whole library at full detail at every distance. Re-bake; this reader knows version 4" ) );
	/* v4 (lane NATIVE1c) refuses v3 BY NAME for the same kind of reason: the
	 * base row is REINTERPRETED, not extended. A v3 base's `crossPx16[0..1]`
	 * are two screen-size steps in 1/16 px and a v4 reader takes those same
	 * four bytes as a little-endian `fullTriangles`, so a v3 base with steps
	 * (16, 0) reads as a base with 16 full-detail triangles -- a plausible
	 * number and silently wrong. Nothing about the byte count changes, so only
	 * the version word can tell the two apart. */
	if ( h.version == 3 )
		return refuse( QStringLiteral( "version 3: a v3 base row's first four bytes of crossPx16 are two "
			"screen-size steps in 1/16 px, and this reader takes them as the base's full-detail triangle "
			"count -- the same bytes, a different meaning, so a v3 file read as v4 would report a plausible "
			"and wrong triangle count for every base. A v3 file also carries no cardCount at header 0xD0 and "
			"no WATERTIGHT bit in its mesh flags. Re-bake; this reader knows versions 4 to 6" ) );
	/* v5 EXTENDS v4 into its reserved pad and reinterprets nothing, so a v4 file
	 * is read as a v5 file without a colour stream; the pad sweep below still
	 * refuses a v4 file that carries anything at 0xD4..0xDF. v6 (lane SWAP1)
	 * names the base row's last word `materialSwap`; a v4/v5 file is read as v6
	 * with no variant rows (that word forced to 0 below). */
	if ( h.version != LODO_VERSION && h.version != LODO_VERSION_NO_SWAP && h.version != LODO_VERSION_NO_COLOUR
		&& h.version != LODO_VERSION_NEAR )
		return refuse( QString( "version %1; this reader knows %2 to %3" ).arg( h.version )
			.arg( LODO_VERSION_NO_COLOUR ).arg( LODO_VERSION_NEAR ) );
	h.headerCrc32 = getLE<quint32>( p + H_HCRC );
	const quint32 hcrc = lodvCrc32( p + H_PLUGIN, LODO_HEADER_BYTES - H_PLUGIN );
	if ( hcrc != h.headerCrc32 )
		return refuse( QString( "headerCrc32 0x%1 does not match the header's bytes (0x%2)" )
			.arg( h.headerCrc32, 8, 16, QChar( '0' ) ).arg( hcrc, 8, 16, QChar( '0' ) ) );
	h.flags = getLE<quint32>( p + H_FLAGS );
	if ( !( h.flags & LODO_FLAG_VERTEX_V1 ) )
		return refuse( QStringLiteral( "flags bit0 (vertex layout v1) is clear" ) );
	if ( h.flags & ~LODO_FLAGS_KNOWN )
		return refuse( QString( "flags 0x%1 has reserved bits set" ).arg( h.flags, 0, 16 ) );
	/* v7 (lane NEAR1): the NEAR flag and version 7 come together or not at all */
	if ( ( ( h.flags & LODO_FLAG_NEAR ) != 0 ) != ( h.version == LODO_VERSION_NEAR ) )
		return refuse( QString( "version %1 with flags 0x%2: the NEAR flag (16) is set exactly on a version-7 file" )
			.arg( h.version ).arg( h.flags, 0, 16 ) );
	h.pluginCorpusHash = getLE<quint64>( p + H_PLUGIN );
	h.objectCorpusHash = getLE<quint64>( p + H_OBJECT );
	h.modelCorpusHash = getLE<quint64>( p + H_MODEL );
	h.cardCorpusHash = getLE<quint64>( p + H_CARD );
	{
		const char * e = file.constData() + H_EDID;
		const size_t n = strnlen( e, H_EDID_BYTES );
		if ( n >= size_t( H_EDID_BYTES ) )
			return refuse( QStringLiteral( "worldspace editor ID is not NUL-terminated within 32 bytes" ) );
		h.worldspaceEdid = QString::fromUtf8( e, qsizetype( n ) );
	}
	h.baseCount = getLE<quint32>( p + H_BASES );
	h.meshCount = getLE<quint32>( p + H_MESHES );
	h.clusterCount = getLE<quint32>( p + H_CLUSTERS );
	h.materialCount = getLE<quint32>( p + H_MATERIALS );
	h.vertexCount = getLE<quint32>( p + H_VERTICES );
	h.maxClustersPerMesh = getLE<quint32>( p + H_MAXCLUSTERS );
	h.clusterMaxTris = getLE<quint16>( p + H_MAXTRIS );
	h.vertexStride = getLE<quint16>( p + H_VSTRIDE );
	if ( h.clusterMaxTris != LODO_CLUSTER_MAX_TRIS )
		return refuse( QString( "clusterMaxTris %1; this reader knows 16" ).arg( h.clusterMaxTris ) );
	if ( h.vertexStride != LODO_VERTEX_STRIDE )
		return refuse( QString( "vertexStride %1; this reader knows 16" ).arg( h.vertexStride ) );
	h.stringBytes = getLE<quint32>( p + H_STRBYTES );
	h.offBases = getLE<quint64>( p + H_OFF_BASES );
	h.offMeshes = getLE<quint64>( p + H_OFF_MESHES );
	h.offClusters = getLE<quint64>( p + H_OFF_CLUSTERS );
	h.offMaterials = getLE<quint64>( p + H_OFF_MATERIALS );
	h.offLocalIndices = getLE<quint64>( p + H_OFF_LOCAL );
	h.offVertices = getLE<quint64>( p + H_OFF_VERTS );
	h.offStrings = getLE<quint64>( p + H_OFF_STRINGS );
	h.indexCrc32 = getLE<quint32>( p + H_ICRC );
	if ( getLE<quint32>( p + H_RESERVED_AC ) != 0 )
		return refuse( QStringLiteral( "reserved word at 0xAC is not zero" ) );
	h.fileBytes = getLE<quint64>( p + H_FILEBYTES );
	h.loadOrderHash = getLE<quint64>( p + H_LOADORDER );
	h.offClusterLods = getLE<quint64>( p + H_OFF_CLUSTERLODS );
	h.clusterLodStride = getLE<quint32>( p + H_CLUSTERLOD_STRIDE );
	h.levelMax = p[H_LEVELMAX];
	h.ladderGroup = p[H_LADDERGROUP];
	if ( h.clusterLodStride != quint32( sizeof( LodoClusterLod ) ) )
		return refuse( QString( "clusterLodStride %1; this reader knows %2" )
			.arg( h.clusterLodStride ).arg( sizeof( LodoClusterLod ) ) );
	if ( h.levelMax > quint8( LODO_LADDER_MAX_LEVEL ) )
		return refuse( QString( "levelMax %1 is past the format's %2" ).arg( h.levelMax ).arg( LODO_LADDER_MAX_LEVEL ) );
	if ( ( h.flags & LODO_FLAG_LADDER ) && h.ladderGroup == 0 )
		return refuse( QStringLiteral( "the LADDER flag is set but ladderGroup is 0: the file does not say what "
			"grouping built it" ) );
	if ( !( h.flags & LODO_FLAG_LADDER ) && ( h.ladderGroup != 0 || h.levelMax != 0 ) )
		return refuse( QString( "the LADDER flag is clear but ladderGroup is %1 and levelMax %2; with no ladder "
			"both are 0" ).arg( h.ladderGroup ).arg( h.levelMax ) );
	/* v4: `cardCount` at 0xD0, and the pad is now the two halves around it. */
	h.cardCount = getLE<quint32>( p + H_CARDCOUNT );
	if ( h.cardCount > h.baseCount )
		return refuse( QString( "cardCount %1 is above the file's %2 bases; a card belongs to a base" )
			.arg( h.cardCount ).arg( h.baseCount ) );
	/* v5: the colour words at 0xD4/0xD8. On a v4 file they are pad and the sweep
	 * refuses them by position; on v5 the count and the offset are 0 together. */
	const bool v5 = h.version >= LODO_VERSION_NO_SWAP;
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
			continue;
		if ( p[i] != 0 )
			return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 2, 16, QChar( '0' ) ) );
	}
	if ( h.fileBytes != quint64( file.size() ) )
		return refuse( QString( "fileBytes %1 but the file is %2 bytes" ).arg( h.fileBytes ).arg( file.size() ) );

	struct Tab { const char * name; quint64 off; quint64 bytes; };
	const int nTabs = h.colourVertexCount ? 9 : 8;
	const Tab tabs[9] = {
		{ "base table", h.offBases, quint64( h.baseCount ) * sizeof( LodoBase ) },
		{ "mesh table", h.offMeshes, quint64( h.meshCount ) * sizeof( LodoMesh ) },
		{ "cluster table", h.offClusters, quint64( h.clusterCount ) * sizeof( LodoCluster ) },
		{ "cluster ladder table", h.offClusterLods, quint64( h.clusterCount ) * sizeof( LodoClusterLod ) },
		{ "material table", h.offMaterials, quint64( h.materialCount ) * sizeof( LodoMaterial ) },
		{ "local-index blob", h.offLocalIndices, quint64( h.clusterCount ) * LODO_LOCAL_INDEX_BYTES },
		{ "vertex blob", h.offVertices, quint64( h.vertexCount ) * sizeof( LodoVertex ) },
		{ "string blob", h.offStrings, quint64( h.stringBytes ) },
		{ "colour stream (v5)", h.offColours, quint64( h.colourVertexCount ) * LODO_COLOUR_STRIDE } };
	quint64 prevEnd = LODO_HEADER_BYTES;
	for ( int ti = 0; ti < nTabs; ti++ ) {
		const Tab & t = tabs[ti];
		if ( t.off % LODO_PAYLOAD_ALIGN )
			return refuse( QString( "%1 offset %2 is not 4,096-aligned" ).arg( t.name ).arg( t.off ) );
		if ( t.off < prevEnd )
			return refuse( QString( "%1 offset %2 is not in table order (previous payload ends at %3)" )
				.arg( t.name ).arg( t.off ).arg( prevEnd ) );
		/* NOT `t.off + t.bytes > h.fileBytes` (lane AUDIT1, 2026-09-17): the
		 * sum of two quint64 WRAPS, and a 4,096-aligned offset near 2^64 then
		 * passes the only bounds test this reader has. The size is tested
		 * first, so the subtraction below cannot go negative. */
		if ( t.bytes > h.fileBytes || t.off > h.fileBytes - t.bytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );
		if ( payloadCheck )
			for ( quint64 i = prevEnd; i < t.off; i++ )
				if ( p[i] != 0 )
					return refuse( QString( "pad byte at %1 before the %2 is not zero" ).arg( i ).arg( t.name ) );
		prevEnd = t.off + t.bytes;
	}
	if ( h.stringBytes == 0 || p[h.offStrings] != 0 )
		return refuse( QStringLiteral( "string blob does not start with the empty string" ) );
	if ( p[h.offStrings + h.stringBytes - 1] != 0 )
		return refuse( QStringLiteral( "string blob is not NUL-terminated" ) );
	if ( payloadCheck ) {
		quint32 icrc = 0;
		for ( int ti = 0; ti < nTabs; ti++ )
			icrc = lodvCrc32( p + tabs[ti].off, qsizetype( tabs[ti].bytes ), icrc );
		if ( icrc != h.indexCrc32 )
			return refuse( QString( "indexCrc32 0x%1 does not match the tables (0x%2)" )
				.arg( h.indexCrc32, 8, 16, QChar( '0' ) ).arg( icrc, 8, 16, QChar( '0' ) ) );
	}

	LodoLibrary L;
	L.flags = h.flags;
	/* v2's loadOrderHash was NOT restored here until lane PERF1 read a library
	 * back and wrote a `.lodi` whose header 0x90 came out ZERO. Every other
	 * corpus hash was already carried over; this one was simply missed when v2
	 * added it at header 0xB8. Refuter: revert this line and the reuse arm of
	 * tests/spells/lodgen_perf.sh goes red on the `.lodi`, 12 bytes apart. */
	L.loadOrderHash = h.loadOrderHash;
	L.pluginCorpusHash = h.pluginCorpusHash;
	L.objectCorpusHash = h.objectCorpusHash;
	L.modelCorpusHash = h.modelCorpusHash;
	L.cardCorpusHash = h.cardCorpusHash;
	L.worldspaceEdid = h.worldspaceEdid;
	L.bases.resize( h.baseCount );
	L.meshes.resize( h.meshCount );
	L.clusters.resize( h.clusterCount );
	L.clusterLods.resize( h.clusterCount );
	L.materials.resize( h.materialCount );
	L.localIndices.resize( size_t( h.clusterCount ) * LODO_LOCAL_INDEX_BYTES );
	L.vertices.resize( h.vertexCount );
	if ( h.baseCount ) std::memcpy( L.bases.data(), p + h.offBases, tabs[0].bytes );
	/* v6: before v6 the base row's last word was `crossPx16[0..1]`, always
	 * written 0 and never a material swap; take it as 0 whatever it holds. */
	if ( h.version < LODO_VERSION )
		for ( LodoBase & b : L.bases )
			b.materialSwap = 0;
	/* CARDLINK1 (2026-09-24): `cardCount` is REDUNDANT on purpose, like
	 * `fullTriangles`, so the reader RECOUNTS it from the base rows instead of
	 * believing it -- with or without the payload check, because a consumer
	 * sizes its card pass from this word. And a card needs a provenance: a
	 * base that names a card layer in a file whose `cardCorpusHash` is 0 says
	 * which arrays it indexes into nowhere. */
	{
		quint32 cardsInRows = 0;
		for ( const LodoBase & b : L.bases )
			if ( b.cardLayer != LODO_NO_CARD )
				cardsInRows++;
		if ( cardsInRows != h.cardCount )
			return refuse( QString( "cardCount %1 but %2 base row(s) name a card layer" )
				.arg( h.cardCount ).arg( cardsInRows ) );
		if ( cardsInRows > 0 && h.cardCorpusHash == 0 )
			return refuse( QString( "%1 base row(s) name a card layer but cardCorpusHash is 0: no card "
				"arrays are named for them" ).arg( cardsInRows ) );
	}
	if ( h.meshCount ) std::memcpy( L.meshes.data(), p + h.offMeshes, tabs[1].bytes );
	if ( h.clusterCount ) std::memcpy( L.clusters.data(), p + h.offClusters, tabs[2].bytes );
	if ( h.clusterCount ) std::memcpy( L.clusterLods.data(), p + h.offClusterLods, tabs[3].bytes );
	if ( h.materialCount ) std::memcpy( L.materials.data(), p + h.offMaterials, tabs[4].bytes );
	if ( h.clusterCount ) std::memcpy( L.localIndices.data(), p + h.offLocalIndices, tabs[5].bytes );
	if ( h.vertexCount ) std::memcpy( L.vertices.data(), p + h.offVertices, tabs[6].bytes );
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

	if ( payloadCheck ) {
		// rows: ranges, reserved fields, the never-0 radius, and the sort keys
		quint32 maxClusters = 0;
		for ( size_t i = 0; i < L.meshes.size(); i++ ) {
			const LodoMesh & m = L.meshes[i];
			if ( m.reserved )
				return refuse( QString( "mesh %1: reserved byte is not zero" ).arg( i ) );
			if ( m.levelCount == 0 )
				return refuse( QString( "mesh %1: levelCount is 0; every mesh has at least level 0" ).arg( i ) );
			if ( m.clusterCountL0 == 0 || m.clusterCountL0 > m.clusterCount )
				return refuse( QString( "mesh %1: clusterCountL0 %2 against clusterCount %3" )
					.arg( i ).arg( m.clusterCountL0 ).arg( m.clusterCount ) );
			if ( quint64( m.clusterFirst ) + m.clusterCount > h.clusterCount )
				return refuse( QString( "mesh %1: clusters %2 + %3 past clusterCount %4" ).arg( i ).arg( m.clusterFirst ).arg( m.clusterCount ).arg( h.clusterCount ) );
			if ( m.modelStringOffset >= h.stringBytes )
				return refuse( QString( "mesh %1: modelStringOffset %2 past stringBytes" ).arg( i ).arg( m.modelStringOffset ) );
			if ( m.flags & ~quint16( LODO_MESH_ANY_ALPHA | LODO_MESH_ANY_SWAY | LODO_MESH_WATERTIGHT
					| ( v5 ? ( LODO_MESH_VERTEX_COLOUR | LODO_MESH_VERTEX_ALPHA ) : 0 ) ) )
				return refuse( QString( "mesh %1: reserved flag bits set" ).arg( i ) );
			maxClusters = std::max<quint32>( maxClusters, m.clusterCount );
			{
				// v3: the two summary words are REDUNDANT on purpose, so they
				// are checked against the rows rather than believed
				quint32 nL0 = 0;
				quint8 lvMax = 0;
				for ( quint64 c = m.clusterFirst; c < quint64( m.clusterFirst ) + m.clusterCount; c++ ) {
					if ( L.clusterLods[size_t( c )].level == 0 )
						nL0++;
					lvMax = std::max( lvMax, L.clusterLods[size_t( c )].level );
				}
				if ( nL0 != m.clusterCountL0 )
					return refuse( QString( "mesh %1: clusterCountL0 says %2 but %3 of its rows are level 0" )
						.arg( i ).arg( m.clusterCountL0 ).arg( nL0 ) );
				if ( quint8( lvMax + 1 ) != m.levelCount )
					return refuse( QString( "mesh %1: levelCount says %2 but its deepest level is %3" )
						.arg( i ).arg( m.levelCount ).arg( lvMax ) );
			}
			if ( i > 0 && foldPath( L.stringAt( L.meshes[i - 1].modelStringOffset ) ) >= foldPath( L.stringAt( m.modelStringOffset ) ) )
				return refuse( QString( "mesh table is not sorted by model path at row %1" ).arg( i ) );
		}
		if ( maxClusters != h.maxClustersPerMesh )
			return refuse( QString( "maxClustersPerMesh %1 but the mesh table's largest is %2" ).arg( h.maxClustersPerMesh ).arg( maxClusters ) );
		for ( size_t i = 0; i < L.clusters.size(); i++ ) {
			const LodoCluster & c = L.clusters[i];
			if ( c.vertexCount == 0 || c.vertexCount > LODO_CLUSTER_MAX_VERTS )
				return refuse( QString( "cluster %1: vertexCount %2 (1..48)" ).arg( i ).arg( c.vertexCount ) );
			if ( c.triangleCount == 0 || c.triangleCount > LODO_CLUSTER_MAX_TRIS )
				return refuse( QString( "cluster %1: triangleCount %2 (1..16)" ).arg( i ).arg( c.triangleCount ) );
			if ( quint64( c.vertexBase ) + c.vertexCount > h.vertexCount )
				return refuse( QString( "cluster %1: vertices %2 + %3 past vertexCount %4" ).arg( i ).arg( c.vertexBase ).arg( c.vertexCount ).arg( h.vertexCount ) );
			if ( c.meshId >= h.meshCount )
				return refuse( QString( "cluster %1: meshId %2 past meshCount %3" ).arg( i ).arg( c.meshId ).arg( h.meshCount ) );
			if ( c.materialId >= h.materialCount )
				return refuse( QString( "cluster %1: materialId %2 past materialCount %3" ).arg( i ).arg( c.materialId ).arg( h.materialCount ) );
			if ( c.flags & ~LODO_CLUSTER_FLAGS_KNOWN )
				return refuse( QString( "cluster %1: reserved flag bits set" ).arg( i ) );
			const quint16 want = c.triangleCount <= 4 ? LODO_SIZE_4 : c.triangleCount <= 8 ? LODO_SIZE_8 : LODO_SIZE_16;
			if ( ( c.flags & 3 ) != want )
				return refuse( QString( "cluster %1: size class %2 for %3 triangles (want %4)" ).arg( i ).arg( c.flags & 3 ).arg( c.triangleCount ).arg( want ) );
			const quint8 * li = &L.localIndices[i * LODO_LOCAL_INDEX_BYTES];
			for ( int k = 0; k < int( LODO_LOCAL_INDEX_BYTES ); k++ ) {
				if ( k < c.triangleCount * 3 ) {
					if ( li[k] >= c.vertexCount )
						return refuse( QString( "cluster %1: local index %2 >= vertexCount %3" ).arg( i ).arg( li[k] ).arg( c.vertexCount ) );
				} else if ( li[k] != LODO_LOCAL_INDEX_NONE ) {
					return refuse( QString( "cluster %1: local index slot %2 past triangleCount is not 0xFF" ).arg( i ).arg( k ) );
				}
			}
			const LodoMesh & m = L.meshes[c.meshId];
			if ( i < m.clusterFirst || i >= quint64( m.clusterFirst ) + m.clusterCount )
				return refuse( QString( "cluster %1 names mesh %2 but lies outside that mesh's cluster range" ).arg( i ).arg( c.meshId ) );
			/* v3: LEVEL joins the sort law between the material and the first
			 * triangle, so a mesh's ladder is contiguous per material and a
			 * consumer that wants full detail alone reads a run. */
			const LodoClusterLod & cl = L.clusterLods[i];
			if ( i > 0 ) {
				const LodoCluster & q = L.clusters[i - 1];
				const LodoClusterLod & ql = L.clusterLods[i - 1];
				if ( q.meshId > c.meshId
					|| ( q.meshId == c.meshId && q.materialId > c.materialId )
					|| ( q.meshId == c.meshId && q.materialId == c.materialId && ql.level > cl.level ) )
					return refuse( QString( "cluster table is not sorted by (meshId, materialId, level) at row %1" ).arg( i ) );
			}

			/* ---- v3: the ladder row that parallels this cluster ---- */
			if ( cl.reserved0 || cl.reserved1 )
				return refuse( QString( "cluster %1: a reserved field of its ladder row is not zero" ).arg( i ) );
			if ( !( cl.radius > 0.0f ) )
				return refuse( QString( "cluster %1: bounding-sphere radius is %2 (never 0)" ).arg( i ).arg( double( cl.radius ) ) );
			if ( cl.level > h.levelMax )
				return refuse( QString( "cluster %1: level %2 past the header's levelMax %3" ).arg( i ).arg( cl.level ).arg( h.levelMax ) );
			if ( cl.level == 0 && cl.geometricError != 0.0f )
				return refuse( QString( "cluster %1 is level 0 but its geometricError is %2; full detail deviates "
					"from full detail by nothing" ).arg( i ).arg( double( cl.geometricError ) ) );
			if ( cl.level == 0 && cl.sourceTriangles != c.triangleCount )
				return refuse( QString( "cluster %1 is level 0 but sourceTriangles %2 is not its own triangleCount %3" )
					.arg( i ).arg( cl.sourceTriangles ).arg( c.triangleCount ) );
			if ( cl.level > 0 && !( cl.geometricError > 0.0f ) )
				return refuse( QString( "cluster %1 is level %2 but its geometricError is %3; a level is only built "
					"when the error GROWS" ).arg( i ).arg( cl.level ).arg( double( cl.geometricError ) ) );
			const bool isRoot = ( cl.parentFirst == LODO_NO_PARENT );
			if ( isRoot != ( cl.parentCount == 0 ) || isRoot != ( cl.parentError == LODO_ERROR_ROOT ) )
				return refuse( QString( "cluster %1: parentFirst 0x%2, parentCount %3 and parentError %4 do not "
					"agree about whether it is a root" ).arg( i ).arg( cl.parentFirst, 8, 16, QChar( '0' ) )
					.arg( cl.parentCount ).arg( double( cl.parentError ) ) );
			/* MONOTONICITY, and it is a refusal: a cut compares ONE stored error
			 * against ONE tolerance, so a child that deviates MORE than the
			 * parent that replaces it would be drawn at a distance where its own
			 * parent was already good enough -- or dropped where it was needed. */
			if ( !( cl.geometricError <= cl.parentError ) )
				return refuse( QString( "cluster %1: geometricError %2 is larger than its parentError %3; the "
					"ladder's errors must never decrease upward" ).arg( i )
					.arg( double( cl.geometricError ) ).arg( double( cl.parentError ) ) );
			if ( !isRoot ) {
				if ( quint64( cl.parentFirst ) + cl.parentCount > h.clusterCount )
					return refuse( QString( "cluster %1: parent range %2 + %3 past clusterCount %4" )
						.arg( i ).arg( cl.parentFirst ).arg( cl.parentCount ).arg( h.clusterCount ) );
				const LodoCluster & pc = L.clusters[cl.parentFirst];
				const LodoClusterLod & pl = L.clusterLods[cl.parentFirst];
				if ( pc.meshId != c.meshId || pc.materialId != c.materialId )
					return refuse( QString( "cluster %1: its parent %2 belongs to mesh %3 material %4, not %5/%6; "
						"a group never crosses a mesh or a material" ).arg( i ).arg( cl.parentFirst )
						.arg( pc.meshId ).arg( pc.materialId ).arg( c.meshId ).arg( c.materialId ) );
				if ( pl.level != cl.level + 1 )
					return refuse( QString( "cluster %1 at level %2 names a parent at level %3; a parent is exactly "
						"one level coarser" ).arg( i ).arg( cl.level ).arg( pl.level ) );
				if ( pl.geometricError != cl.parentError )
					return refuse( QString( "cluster %1: parentError %2 but its parent's own geometricError is %3" )
						.arg( i ).arg( double( cl.parentError ) ).arg( double( pl.geometricError ) ) );
			}
			/* THE CONE, and its refusal. `LODO_CLUSTER_CONE_OPEN` is what a
			 * cluster says when its normals span more than a hemisphere; a cone
			 * that quietly excluded its own triangles would backface-cull a leaf
			 * card that is facing the camera. */
			if ( c.flags & LODO_CLUSTER_CONE_OPEN ) {
				if ( cl.coneAxis[0] || cl.coneAxis[1] || cl.coneCos != -1.0f )
					return refuse( QString( "cluster %1 is CONE_OPEN but carries an axis (%2, %3) and cosine %4" )
						.arg( i ).arg( cl.coneAxis[0] ).arg( cl.coneAxis[1] ).arg( double( cl.coneCos ) ) );
			} else if ( !( cl.coneCos > 0.0f ) || cl.coneCos > 1.0f ) {
				return refuse( QString( "cluster %1: cone cosine %2 is outside (0, 1]; a cone that wide is OPEN and "
					"says so in the flag" ).arg( i ).arg( double( cl.coneCos ) ) );
			}
		}
		{
			quint8 lvMax = 0;
			for ( const LodoClusterLod & cl : L.clusterLods )
				lvMax = std::max( lvMax, cl.level );
			if ( lvMax != h.levelMax )
				return refuse( QString( "levelMax %1 but the ladder table's deepest level is %2" )
					.arg( h.levelMax ).arg( lvMax ) );
		}
		for ( size_t i = 0; i < L.materials.size(); i++ ) {
			const LodoMaterial & m = L.materials[i];
			if ( m.features && h.version < LODO_VERSION_NEAR )
				return refuse( QString( "material %1: reserved byte is not zero" ).arg( i ) );
			if ( m.features & ~LODO_MAT_FEATURES_KNOWN )
				return refuse( QString( "material %1: features 0x%2 set reserved bits (5..7)" ).arg( i ).arg( m.features, 0, 16 ) );
			if ( m.layer != LODO_NO_LAYER && m.layer >= LODO_LAYER_CAP )
				return refuse( QString( "material %1: layer %2 is not < 2048 (the D3D11 array-axis limit)" ).arg( i ).arg( m.layer ) );
			if ( m.family > LODO_FAMILY_PBR )
				return refuse( QString( "material %1: family %2 is neither legacy nor pbr" ).arg( i ).arg( m.family ) );
			if ( m.lodmStringOffset >= h.stringBytes )
				return refuse( QString( "material %1: lodmStringOffset past stringBytes" ).arg( i ) );
			if ( i > 0 ) {
				const LodoMaterial & q = L.materials[i - 1];
				const auto key = [&]( const LodoMaterial & x ) {
					return std::make_tuple( x.family, x.arrayClass, x.arraySet, x.layer, foldPath( L.stringAt( x.lodmStringOffset ) ) );
				};
				if ( key( q ) > key( m ) )
					return refuse( QString( "material table is not sorted at row %1" ).arg( i ) );
			}
		}
		for ( size_t i = 0; i < L.bases.size(); i++ ) {
			const LodoBase & b = L.bases[i];
			/* v6: sorted by (formId, materialSwap) strictly, so a base's plain
			 * row (swap 0) comes first and each variant after it once. */
			if ( i > 0 && ( L.bases[i - 1].formId > b.formId
					|| ( L.bases[i - 1].formId == b.formId && L.bases[i - 1].materialSwap >= b.materialSwap ) ) )
				return refuse( QString( "base table is not sorted by (formId, materialSwap) ascending at row %1 "
					"(0x%2/0x%3 after 0x%4/0x%5)" ).arg( i ).arg( b.formId, 8, 16, QChar( '0' ) )
					.arg( b.materialSwap, 8, 16, QChar( '0' ) ).arg( L.bases[i - 1].formId, 8, 16, QChar( '0' ) )
					.arg( L.bases[i - 1].materialSwap, 8, 16, QChar( '0' ) ) );
			if ( ( ( b.flags & LODO_BASE_SWAPPED ) != 0 ) != ( b.materialSwap != 0 ) )
				return refuse( QString( "base 0x%1: the SWAPPED flag and materialSwap 0x%2 disagree (set together or "
					"neither; a file before v6 has no variant row)" ).arg( b.formId, 8, 16, QChar( '0' ) )
					.arg( b.materialSwap, 8, 16, QChar( '0' ) ) );
			if ( b.materialSwap && ( i == 0 || L.bases[i - 1].formId != b.formId ) )
				return refuse( QString( "base 0x%1: variant row for swap 0x%2 has no plain row of its base before it" )
					.arg( b.formId, 8, 16, QChar( '0' ) ).arg( b.materialSwap, 8, 16, QChar( '0' ) ) );
			bool anyMesh = false;
			for ( int k = 0; k < 4; k++ ) {
				if ( b.rep[k] == LODO_NO_MESH )
					continue;
				if ( b.rep[k] >= h.meshCount )
					return refuse( QString( "base 0x%1: rep[%2] = %3 past meshCount %4" ).arg( b.formId, 8, 16, QChar( '0' ) ).arg( k ).arg( b.rep[k] ).arg( h.meshCount ) );
				anyMesh = true;
			}
			if ( !anyMesh && b.cardLayer == LODO_NO_CARD )
				return refuse( QString( "base 0x%1 has no mesh in any slot and no card" ).arg( b.formId, 8, 16, QChar( '0' ) ) );
			if ( !( b.boundRadius > 0.0f ) )
				return refuse( QString( "base 0x%1: boundRadius is %2 (never 0)" ).arg( b.formId, 8, 16, QChar( '0' ) ).arg( double( b.boundRadius ) ) );
			if ( b.modelStringOffset >= h.stringBytes )
				return refuse( QString( "base 0x%1: modelStringOffset past stringBytes" ).arg( b.formId, 8, 16, QChar( '0' ) ) );
			if ( b.flags & ~quint16( LODO_BASE_TREE | LODO_BASE_ANY_ALPHA | LODO_BASE_ANY_MESH | LODO_BASE_SWAPPED ) )
				return refuse( QString( "base 0x%1: reserved flag bits set" ).arg( b.formId, 8, 16, QChar( '0' ) ) );
			/* v4: `fullTriangles` is REDUNDANT on purpose -- the rows hold the
			 * same number -- so the reader RECOUNTS it from the distinct meshes
			 * the slots name instead of believing it, exactly as it recounts
			 * `clusterCountL0` and `levelCount` (docs 3.2). */
			if ( anyMesh ) {
				quint16 seen[4] = { LODO_NO_MESH, LODO_NO_MESH, LODO_NO_MESH, LODO_NO_MESH };
				int ns = 0;
				quint32 tris = 0;
				for ( int k = 0; k < 4; k++ ) {
					if ( b.rep[k] == LODO_NO_MESH )
						continue;
					bool dup = false;
					for ( int j = 0; j < ns; j++ )
						dup = dup || seen[j] == b.rep[k];
					if ( dup )
						continue;
					seen[ns++] = b.rep[k];
					const LodoMesh & bm = L.meshes[b.rep[k]];
					for ( quint64 c = bm.clusterFirst; c < quint64( bm.clusterFirst ) + bm.clusterCount; c++ )
						if ( L.clusterLods[size_t( c )].level == 0 )
							tris += L.clusters[size_t( c )].triangleCount;
				}
				if ( b.fullTriangles != tris )
					return refuse( QString( "base 0x%1: fullTriangles %2 but its distinct meshes hold %3 "
						"level-0 triangles" ).arg( b.formId, 8, 16, QChar( '0' ) ).arg( b.fullTriangles ).arg( tris ) );
			} else if ( b.fullTriangles != 0 ) {
				return refuse( QString( "base 0x%1: no mesh in any slot but fullTriangles is %2" )
					.arg( b.formId, 8, 16, QChar( '0' ) ).arg( b.fullTriangles ) );
			}
		}
	}
	if ( header )
		*header = h;
	if ( lib )
		*lib = std::move( L );
	return true;
}

QStringList lodoDescribe( const LodoHeader & h, const LodoLibrary * lib )
{
	QStringList out;
	out << QString( "magic LODO" ) << QString( "version %1" ).arg( h.version )
		<< QString( "flags 0x%1" ).arg( h.flags, 0, 16 )
		<< QString( "headerCrc32 0x%1" ).arg( h.headerCrc32, 8, 16, QChar( '0' ) )
		<< QString( "pluginCorpusHash 0x%1" ).arg( h.pluginCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "objectCorpusHash 0x%1" ).arg( h.objectCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "modelCorpusHash 0x%1" ).arg( h.modelCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "cardCorpusHash 0x%1" ).arg( h.cardCorpusHash, 16, 16, QChar( '0' ) )
		<< QString( "loadOrderHash 0x%1" ).arg( h.loadOrderHash, 16, 16, QChar( '0' ) )
		<< QString( "lodoIdentity 0x%1" ).arg( lodoIdentityOf( h.headerCrc32, h.modelCorpusHash, h.objectCorpusHash ), 16, 16, QChar( '0' ) )
		<< QString( "worldspace %1" ).arg( h.worldspaceEdid )
		<< QString( "bases %1" ).arg( h.baseCount ) << QString( "meshes %1" ).arg( h.meshCount )
		<< QString( "clusters %1" ).arg( h.clusterCount ) << QString( "materials %1" ).arg( h.materialCount )
		<< QString( "vertices %1" ).arg( h.vertexCount ) << QString( "maxClustersPerMesh %1" ).arg( h.maxClustersPerMesh )
		<< QString( "clusterMaxTris %1" ).arg( h.clusterMaxTris ) << QString( "vertexStride %1" ).arg( h.vertexStride )
		<< QString( "stringBytes %1" ).arg( h.stringBytes )
		<< QString( "offBases %1" ).arg( h.offBases ) << QString( "offMeshes %1" ).arg( h.offMeshes )
		<< QString( "offClusters %1" ).arg( h.offClusters ) << QString( "offMaterials %1" ).arg( h.offMaterials )
		<< QString( "offLocalIndices %1" ).arg( h.offLocalIndices ) << QString( "offVertices %1" ).arg( h.offVertices )
		<< QString( "offStrings %1" ).arg( h.offStrings )
		<< QString( "offClusterLods %1" ).arg( h.offClusterLods )
		<< QString( "clusterLodStride %1" ).arg( h.clusterLodStride )
		<< QString( "levelMax %1" ).arg( h.levelMax )
		<< QString( "ladderGroup %1" ).arg( h.ladderGroup )
		<< QString( "cardCount %1" ).arg( h.cardCount )
		<< QString( "colourVertexCount %1" ).arg( h.colourVertexCount )
		<< QString( "offColours %1" ).arg( h.offColours )
		<< QString( "indexCrc32 0x%1" ).arg( h.indexCrc32, 8, 16, QChar( '0' ) )
		<< QString( "fileBytes %1" ).arg( h.fileBytes );
	if ( lib ) {
		quint64 tris = 0;
		for ( const LodoCluster & c : lib->clusters )
			tris += c.triangleCount;
		out << QString( "triangles %1" ).arg( tris );
		out << QString( "indexPadding %1" ).arg( lib->clusters.empty() ? 0.0
			: double( lib->clusters.size() * LODO_LOCAL_INDEX_BYTES ) / double( tris * 3 ), 0, 'f', 3 );
		/* v3: the ladder, per level, so a reader of the CLI's output can see
		 * what the file actually carries without a decoder. */
		quint64 l0Tris = 0, coneOpen = 0, roots = 0;
		std::vector<quint64> perLevel( size_t( h.levelMax ) + 1, 0 );
		std::vector<quint64> triLevel( size_t( h.levelMax ) + 1, 0 );
		double errSum = 0.0;
		float errMax = 0.0f;
		for ( size_t i = 0; i < lib->clusters.size() && i < lib->clusterLods.size(); i++ ) {
			const LodoClusterLod & cl = lib->clusterLods[i];
			if ( size_t( cl.level ) < perLevel.size() ) {
				perLevel[cl.level]++;
				triLevel[cl.level] += lib->clusters[i].triangleCount;
			}
			if ( cl.level == 0 )
				l0Tris += lib->clusters[i].triangleCount;
			if ( lib->clusters[i].flags & LODO_CLUSTER_CONE_OPEN )
				coneOpen++;
			if ( cl.parentFirst == LODO_NO_PARENT )
				roots++;
			errSum += double( cl.geometricError );
			errMax = std::max( errMax, cl.geometricError );
		}
		out << QString( "level0Triangles %1" ).arg( l0Tris )
			<< QString( "coneOpenClusters %1" ).arg( coneOpen )
			<< QString( "rootClusters %1" ).arg( roots )
			<< QString( "meanGeometricError %1" ).arg( lib->clusterLods.empty() ? 0.0
				: errSum / double( lib->clusterLods.size() ), 0, 'f', 4 )
			<< QString( "maxGeometricError %1" ).arg( double( errMax ), 0, 'f', 4 );
		for ( size_t lv = 0; lv < perLevel.size(); lv++ )
			out << QString( "level %1 clusters %2 triangles %3" ).arg( lv ).arg( perLevel[lv] ).arg( triLevel[lv] );
		/* v4: the two new per-row words, read back from the library the reader
		 * just parsed. `watertightMeshes` counts the mesh rows whose source
		 * soup had no boundary edge; `baseFullTriangles` is the sum over bases
		 * of the full-detail triangle count (v4's reading of v3's `crossPx16[0..1]`). */
		quint64 watertight = 0;
		for ( const LodoMesh & m : lib->meshes )
			if ( m.flags & LODO_MESH_WATERTIGHT )
				watertight++;
		quint64 fullTris = 0, basesWithFull = 0;
		for ( const LodoBase & bs : lib->bases ) {
			fullTris += bs.fullTriangles;
			if ( bs.fullTriangles )
				basesWithFull++;
		}
		/* v5: which meshes carry colour, and whether any of it is not white --
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
				colourNames << QFileInfo( QString( lib->stringAt( m.modelStringOffset ) ).replace( QChar( '\\' ), QChar( '/' ) ) ).fileName();
		}
		for ( quint32 c : lib->colours )
			if ( ( c & 0x00FFFFFFU ) != 0x00FFFFFFU )
				colourNotWhite++;
		out << QString( "colourMeshes %1" ).arg( colourMeshes )
			<< QString( "colourAlphaMeshes %1" ).arg( alphaMeshes )
			<< QString( "colourVerticesNotWhite %1" ).arg( colourNotWhite )
			<< QString( "colourMeshNames %1" ).arg( colourNames.isEmpty() ? QStringLiteral( "-" ) : colourNames.join( ',' ) );
		/* v6: the material-swap variant rows (lane SWAP1). */
		quint64 variantRows = 0;
		QSet<quint32> variantSwaps;
		for ( const LodoBase & bs : lib->bases )
			if ( bs.materialSwap ) {
				variantRows++;
				variantSwaps.insert( bs.materialSwap );
			}
		out << QString( "materialSwapRows %1" ).arg( variantRows )
			<< QString( "materialSwapForms %1" ).arg( variantSwaps.size() );
		out << QString( "watertightMeshes %1" ).arg( watertight )
			<< QString( "baseFullTriangles %1" ).arg( fullTris )
			<< QString( "basesWithFullTriangles %1" ).arg( basesWithFull );
	}
	return out;
}
