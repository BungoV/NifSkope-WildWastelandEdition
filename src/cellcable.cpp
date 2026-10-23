#include "cellcable.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "esmdata.h"
#include "nativeemit.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

/* lane CABLES1: the cable tube, the game's way (cellcable.h has the measured rules). Every constant below is
 * a measured game value: the sag share 1.4, the 48 curve samples, the 4 % segment, the 8-segment floor, the
 * 0.1-unit step of the per-length tile sum. */

namespace
{
constexpr float kSagShare = 1.4f;        // the game's sag setting
constexpr int kCurveSamples = 48;
constexpr float kSegmentShare = 0.04f;   // the game's segment-length setting (share of the curve's length)
constexpr int kMinSegments = 8;
constexpr float kTileStep = 0.1f;

struct V3
{
	float x = 0, y = 0, z = 0;
};
inline V3 operator+( V3 a, V3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-( V3 a, V3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*( V3 a, float k ) { return { a.x * k, a.y * k, a.z * k }; }
inline float dot( V3 a, V3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross( V3 a, V3 b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float len( V3 a ) { return std::sqrt( dot( a, a ) ); }
inline V3 unit( V3 a, V3 fallback )
{
	const float l = len( a );
	return l > 1.0e-6f ? a * ( 1.0f / l ) : fallback;
}

QString texPath( const QString & p )
{
	if ( p.isEmpty() )
		return p;
	QString s = QString( p ).replace( '/', '\\' );
	if ( !s.startsWith( QLatin1String( "textures\\" ), Qt::CaseInsensitive ) )
		s.prepend( QLatin1String( "textures\\" ) );
	return s;
}
}

bool wwCellCablesOn()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_CELL_CABLES_RED" ) != "off" );
	return on;
}

bool wwCellCablesWind()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_CELL_CABLES_RED" ) != "nowind" );
	return on;
}

bool wwCellCableShape( const EsmBendable & look, const WwCableSpec & spec, NativeSrcShape * out, WwCableBuilt * built )
{
	if ( !look.exists || !out )
		return false;
	const V3 A { -spec.half[0], -spec.half[1], -spec.half[2] };
	const V3 B { spec.half[0], spec.half[1], spec.half[2] };
	const float L = len( B - A );
	if ( !( L > 1.0e-3f ) )
		return false;

	// the sag: the Bezier's middle point drops by |B - A| * 1.4 * slack, unless the cable hangs straight down
	V3 M = ( A + B ) * 0.5f;
	const bool vertical = A.x == B.x && A.y == B.y;
	if ( !vertical )
		M.z -= L * kSagShare * spec.slack;

	// the curve, sampled the game's way, and its length
	V3 pts[kCurveSamples];
	float cum[kCurveSamples];
	for ( int i = 0; i < kCurveSamples; i++ ) {
		const float t = float( i ) / float( kCurveSamples - 1 );
		const float u = 1.0f - t;
		pts[i] = A * ( u * u ) + M * ( 2.0f * u * t ) + B * ( t * t );
		cum[i] = i ? cum[i - 1] + len( pts[i] - pts[i - 1] ) : 0.0f;
	}
	const float curveLen = cum[kCurveSamples - 1];
	float segLen = kSegmentShare * curveLen;
	int segments = segLen > 0.0f ? int( std::floor( curveLen / segLen ) ) : 0;
	if ( segments < kMinSegments ) {
		segments = kMinSegments;
		segLen = curveLen / float( kMinSegments );
	}
	auto at = [&]( float d ) -> V3 {   // the point at distance d along the curve
		if ( d <= 0.0f )
			return pts[0];
		for ( int i = 1; i < kCurveSamples; i++ ) {
			if ( cum[i] >= d ) {
				const float span = cum[i] - cum[i - 1];
				const float f = span > 0.0f ? ( d - cum[i - 1] ) / span : 0.0f;
				return pts[i - 1] + ( pts[i] - pts[i - 1] ) * f;
			}
		}
		return pts[kCurveSamples - 1];
	};
	const int rings = segments + 1;
	std::vector<V3> c( static_cast<size_t>( rings ) );
	for ( int k = 0; k < segments; k++ )
		c[size_t( k )] = at( float( k ) * segLen );
	c[size_t( segments )] = B;

	// the tiles along the cable
	float tiles = look.tiles;
	if ( look.tilesRelative ) {
		const float a = kSagShare * L * spec.slack / ( L * L );
		float sum = 0.0f;
		for ( float x = 0.0f; x < L; x += kTileStep ) {
			const float dy = a * ( x + kTileStep ) * ( x + kTileStep ) - a * x * x;
			sum += std::sqrt( dy * dy + kTileStep * kTileStep );
		}
		tiles = look.tiles * 2.0f * sum;
	}

	// the wind's weight per ring
	const bool wind = look.windSensibility > 0.0f;
	std::vector<float> w( size_t( rings ), 1.0f );
	if ( wind ) {
		const V3 ab = B - A;
		const float ab2 = dot( ab, ab );
		float lo = 3.4e38f, hi = -3.4e38f;
		for ( int k = 0; k < rings; k++ ) {
			const V3 ap = c[size_t( k )] - A;
			const float t = dot( ap, ab ) / ab2;
			float v;
			if ( spec.detached )
				v = t;
			else
				v = len( ap - ab * t ) / L;
			w[size_t( k )] = v;
			lo = std::min( lo, v );
			hi = std::max( hi, v );
		}
		const float k = spec.thickness > 0.0f ? spec.slack * look.windSensibility / spec.thickness : 0.0f;
		for ( float & v : w )
			v = hi > lo ? std::clamp( ( v - lo ) * k / ( hi - lo ), 0.0f, 1.0f ) : 0.0f;
	}

	// the tube
	const int slices = std::max( 3, look.slices );
	const float radius = spec.thickness * 0.5f;
	const bool textured = !look.diffuse.isEmpty();
	quint8 rgb[3];
	for ( int i = 0; i < 3; i++ )
		rgb[i] = textured ? 255 : quint8( std::lround( std::clamp( look.color[i], 0.0f, 1.0f ) * 255.0f ) );
	LodoSrcShape & g = out->geom;
	g = LodoSrcShape();
	const size_t nv = size_t( rings ) * size_t( slices + 1 );
	g.pos.reserve( nv * 3 );
	g.nrm.reserve( nv * 3 );
	g.tan.reserve( nv * 3 );
	g.uv.reserve( nv * 2 );
	g.rgba.reserve( nv * 4 );
	const float twoPi = 6.28318530718f;
	V3 prevN { 0, 0, 0 };
	for ( int k = 0; k < rings; k++ ) {
		const V3 T = unit( k + 1 < rings ? c[size_t( k + 1 )] - c[size_t( k )] : c[size_t( k )] - c[size_t( k - 1 )],
			V3 { 0, 0, 1 } );
		// a frame that turns smoothly along the cable: the previous ring's side vector, made square to T
		V3 N;
		if ( k == 0 ) {
			N = cross( T, V3 { 0, 0, 1 } );
			if ( len( N ) < 1.0e-4f )
				N = cross( T, V3 { 1, 0, 0 } );
			N = unit( N, V3 { 1, 0, 0 } );
		} else {
			N = unit( prevN - T * dot( prevN, T ), prevN );
		}
		prevN = N;
		const V3 S = cross( T, N );
		const float u = float( k ) / float( segments ) * tiles;
		for ( int j = 0; j <= slices; j++ ) {
			const float ang = twoPi * float( j % slices ) / float( slices );
			const V3 r = N * std::cos( ang ) + S * std::sin( ang );
			const V3 p = c[size_t( k )] + r * radius;
			g.pos.insert( g.pos.end(), { p.x, p.y, p.z } );
			g.nrm.insert( g.nrm.end(), { r.x, r.y, r.z } );
			g.tan.insert( g.tan.end(), { T.x, T.y, T.z } );
			g.uv.insert( g.uv.end(), { u, float( j ) / float( slices ) } );
			const quint8 a = quint8( std::lround( w[size_t( k )] * 255.0f ) );
			g.rgba.insert( g.rgba.end(), { rgb[0], rgb[1], rgb[2], a } );
		}
	}
	const quint32 row = quint32( slices + 1 );
	for ( int k = 0; k < segments; k++ ) {
		for ( int j = 0; j < slices; j++ ) {
			const quint32 a = quint32( k ) * row + quint32( j );
			const quint32 b = a + 1, d = a + row, e = d + 1;
			// outward facing: (side step) x (along the cable) points out of the tube
			g.tris.insert( g.tris.end(), { a, b, e, a, e, d } );
		}
	}

	out->tex0 = texPath( look.diffuse );
	out->tex1 = texPath( look.normal );
	out->tex7.clear();
	out->matName.clear();
	out->hasAlpha = false;
	out->nearFacts.vertexColour = true;

	if ( built ) {
		built->centres.clear();
		built->centres.reserve( size_t( rings ) * 3 );
		for ( const V3 & p : c )
			built->centres.insert( built->centres.end(), { p.x, p.y, p.z } );
		built->rings = rings;
		built->slices = slices;
		built->length = curveLen;
		built->tiles = tiles;
		built->wind = wind;
		built->sagExpected = vertical ? 0.0f : 0.7f * L * spec.slack;
		built->sagMeasured = vertical ? 0.0f : wwCableDropAtMiddle( built->centres );
	}
	return true;
}

float wwCableDropAtMiddle( const std::vector<float> & c )
{
	const size_t n = c.size() / 3;
	if ( n < 2 )
		return 0.0f;
	const float ax = c[0], ay = c[1], az = c[2];
	const float bx = c[n * 3 - 3], by = c[n * 3 - 2], bz = c[n * 3 - 1];
	const float dx = bx - ax, dy = by - ay, dd = dx * dx + dy * dy;
	if ( !( dd > 0.0f ) )
		return 0.0f;
	auto share = [&]( size_t i ) { return ( ( c[i * 3] - ax ) * dx + ( c[i * 3 + 1] - ay ) * dy ) / dd; };
	for ( size_t i = 1; i < n; i++ ) {
		const float s0 = share( i - 1 ), s1 = share( i );
		if ( s0 <= 0.5f && s1 >= 0.5f ) {
			const float f = s1 > s0 ? ( 0.5f - s0 ) / ( s1 - s0 ) : 0.0f;
			const float z = c[i * 3 - 1] + ( c[i * 3 + 2] - c[i * 3 - 1] ) * f;
			return ( az + bz ) * 0.5f - z;
		}
	}
	return 0.0f;
}
