#ifndef LODCHSKY_H
#define LODCHSKY_H

/* lane FIX1101A lodch (11-01, bungo: "is sky visibility from lod bakes taken into consideration?"): the far objects'
 * baked per-vertex sky byte (.lodi v7 0x110, docs/LODGEN_NATIVE_LODO_LODI.md 4.10) turned into the factor the cell
 * view's ambient takes, so the far field (the bake) and the near field (the ray-traced GI volume) agree at the last
 * cascade edge.
 *
 * The byte is the bake's +Z-cosine weighted share of 56 open rays (7 rings x 8 azimuths, cast from p + 2n, NOT about
 * the normal). Two facts make the raw byte the wrong factor:
 *   1. a surface's own plane blocks every ray behind it, so an OPEN wall reads 0.5 and an open roof 1.0. The share of
 *      the rays on the outer side (skyRef) is the open value; f = s / skyRef is the occlusion (1 = open);
 *   2. the GI weights the sky by cos about the NORMAL, the bake by cos about +Z: a low horizon in front of a wall
 *      blocks little +Z weight and much normal weight. So f is read back as the uniform horizon b the ray set would
 *      see it under (the "uniform facade": horizonOf, the set's own response at its band edges), and the factor is
 *      the GI's share under that horizon: 1 - (b + sin b cos b) / (pi / 2) for a wall, cos^2 b for a roof, linear
 *      in nz between, and 1 where the surface faces down (skyRef -> 0: the bake has no evidence about its sky).
 * Gate: tests/features/far-lod/lodchsky (an independent integral of the GI's sky share against this law at the cascade edge). */

#include <algorithm>
#include <cmath>
#include <vector>

namespace LodchSky
{

//! the bake's ray set, as the doc's 4.10 lists it: ring k at its band's irradiance median, weight = band share / 8
struct Rays
{
	float d[56][3];
	float w[56];
};

inline const Rays & rays()
{
	static const Rays r = [] {
		Rays o {};
		const float bands[8] = { 0.0f, 10.0f, 20.0f, 30.0f, 45.0f, 60.0f, 75.0f, 90.0f };
		const float kDeg = 3.14159265358979f / 180.0f;
		int i = 0;
		for ( int k = 0; k < 7; k++ ) {
			const float a = bands[k] * kDeg, b = bands[k + 1] * kDeg;
			const float sa = std::sin( a ), sb = std::sin( b );
			const float e = std::asin( std::sqrt( ( sa * sa + sb * sb ) * 0.5f ) );
			for ( int j = 0; j < 8; j++, i++ ) {
				const float ph = ( float( j ) + ( ( k & 1 ) ? 0.5f : 0.0f ) ) * 3.14159265358979f * 0.25f;
				o.d[i][0] = std::cos( e ) * std::cos( ph );
				o.d[i][1] = std::cos( e ) * std::sin( ph );
				o.d[i][2] = std::sin( e );
				o.w[i] = ( sb * sb - sa * sa ) / 8.0f;
			}
		}
		return o;
	}();
	return r;
}

//! the byte an OPEN surface with this world normal z reads: the weight of the rays on its outer side (azimuth 0;
//! a ray in the surface's plane counts as outside)
inline float skyRef( float nz )
{
	const Rays & r = rays();
	const float c = std::clamp( nz, -1.0f, 1.0f ), s = std::sqrt( std::max( 0.0f, 1.0f - c * c ) );
	float w = 0.0f;
	for ( int i = 0; i < 56; i++ )
		if ( r.d[i][0] * s + r.d[i][2] * c > -1.0e-4f )
			w += r.w[i];
	return w;
}

/*! the uniform horizon (radians) under which THIS ray set reads the share f (= s / skyRef) on a surface with normal z
 *  nz: the set's own response at the 8 band edges (exact there: a ring sits inside its band), linear between them */
inline float horizonOf( float f, float nz )
{
	const Rays & r = rays();
	const float c = std::clamp( nz, -1.0f, 1.0f ), sn = std::sqrt( std::max( 0.0f, 1.0f - c * c ) );
	const float edges[8] = { 0.0f, 10.0f, 20.0f, 30.0f, 45.0f, 60.0f, 75.0f, 90.0f };
	const float kDeg = 3.14159265358979f / 180.0f;
	float F[8] = {}, tot = 0.0f;
	for ( int i = 0; i < 56; i++ ) {
		if ( r.d[i][0] * sn + r.d[i][2] * c <= -1.0e-4f )
			continue;
		tot += r.w[i];
		const float el = std::asin( std::clamp( r.d[i][2], -1.0f, 1.0f ) ) / kDeg;
		for ( int e = 0; e < 8; e++ )
			if ( el > edges[e] )
				F[e] += r.w[i];
	}
	if ( tot <= 0.0f || f >= 1.0f )
		return 0.0f;
	for ( int e = 0; e < 8; e++ )
		F[e] /= tot;
	for ( int e = 0; e < 7; e++ ) {
		if ( f >= F[e + 1] ) {
			const float d = F[e] - F[e + 1];
			const float t = d > 1.0e-6f ? std::clamp( ( F[e] - f ) / d, 0.0f, 1.0f ) : 0.0f;
			return ( edges[e] + t * ( edges[e + 1] - edges[e] ) ) * kDeg;
		}
	}
	return 90.0f * kDeg;
}

//! the GI-equivalent sky factor of a baked sky value s (0..1) on a surface whose world normal has this z
inline float giFactor( float s, float nz )
{
	const float sr = skyRef( nz );
	if ( sr < 1.0e-3f )
		return 1.0f;
	const float f = std::clamp( s / sr, 0.0f, 1.0f );
	const float b = horizonOf( f, nz );	// the uniform horizon the byte says
	const float wall = 1.0f - ( b + std::sin( b ) * std::cos( b ) ) / 1.57079632679490f;
	const float roof = std::cos( b ) * std::cos( b );
	const float g = wall + ( roof - wall ) * std::clamp( nz, 0.0f, 1.0f );
	const float t = std::clamp( sr / 0.1f, 0.0f, 1.0f );	// facing down: no evidence about the sky
	return 1.0f + ( g - 1.0f ) * t;
}

//! the factor table the far builder reads: [byte][nz bin], nz = -1 + 2 bin / 128
inline const float * giTable()
{
	static const std::vector<float> t = [] {
		std::vector<float> o( size_t( 256 ) * 129 );
		for ( int s = 0; s < 256; s++ )
			for ( int n = 0; n <= 128; n++ )
				o[size_t( s ) * 129 + size_t( n )] = giFactor( float( s ) / 255.0f, -1.0f + 2.0f * float( n ) / 128.0f );
		return o;
	}();
	return t.data();
}

inline float giFactorByte( int s, float nz )
{
	const int n = std::clamp( int( std::lround( ( nz + 1.0f ) * 64.0f ) ), 0, 128 );
	return giTable()[size_t( std::clamp( s, 0, 255 ) ) * 129 + size_t( n )];
}

/*! lane FIX1101A lodch r2 (LATE ITEM 25): the .lodi v12 ground-contact ramp (255 at the terrain surface, 0 by 256 u
 *  above it; docs/LODGEN_NATIVE_LODO_LODI.md 4.16) drawn as contact darkening, folded into the AO byte the far rows
 *  already carry (no extra byte): min(AO, 1 - kContactK x ramp). The bake's 8 AO rays never aim lower than ~8 deg over
 *  the tangent plane, so a wall foot in the ground reads open (4.8 "Known"); the min never darkens twice where the AO
 *  already holds the contact. The darkest a foot gets from it is 1 - kContactK (0.65). Red WW_LODL_AO_RED=noground. */
constexpr float kContactK = 0.35f;

inline float contactAo( float ao, int ramp )
{
#if defined( LODCH_GROUND_RED ) && LODCH_GROUND_RED == 1
	( void ) ramp;
	return ao;	// today: the ramp is not drawn
#elif defined( LODCH_GROUND_RED ) && LODCH_GROUND_RED == 2
	return ao * ( 1.0f - kContactK * float( std::clamp( ramp, 0, 255 ) ) / 255.0f );	// the product: darkens twice
#else
	return std::min( ao, 1.0f - kContactK * float( std::clamp( ramp, 0, 255 ) ) / 255.0f );
#endif
}

}

#endif
