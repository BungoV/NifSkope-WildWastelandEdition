#ifndef WWHAIREYE_H
#define WWHAIREYE_H

/* lane MATLOOK3: the C++ twin of res/shaders/ww_hair_eye.glsl (HAIRKK1 hair's Kajiya-Kay lobe, EYECORNEA1 the eye's
 * cornea coat), pinned by tests/haireye. Pure C++ (no Qt). WWHAIREYE_RED swaps in the red constants (unshifted
 * lobes, a rough coat), which the unit test must refuse. */

#include <algorithm>
#include <cmath>

namespace wwhe {

struct V3
{
	double x, y, z;
};
inline V3 operator+( V3 a, V3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-( V3 a, V3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*( V3 a, double s ) { return { a.x * s, a.y * s, a.z * s }; }
inline double dot( V3 a, V3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 norm( V3 a ) { const double l = std::sqrt( dot( a, a ) ); return l > 0.0 ? a * ( 1.0 / l ) : a; }
inline double sat( double v ) { return std::clamp( v, 0.0, 1.0 ); }

#ifdef WWHAIREYE_RED
constexpr double kHairS1 = 0.0, kHairS2 = 0.0;
constexpr double kEyeA = 0.25;
#else
constexpr double kHairS1 = 0.105, kHairS2 = -0.158;	// tan 6 deg (R, -2 alpha), -tan 9 deg (TRT, 3 alpha); alpha 3 deg
constexpr double kEyeA = 0.0025;	// roughness 0.05
#endif
constexpr double kHairF0 = 0.046521;	// IOR 1.55
constexpr double kEyeF0 = 0.025043;	// IOR 1.376

inline double schlick( double f0, double c )
{
	const double f1 = 1.0 - sat( c );
	return f0 + ( 1.0 - f0 ) * f1 * f1 * f1 * f1 * f1;
}

inline double hairD( V3 T, V3 H, double n )
{
	const double th = dot( T, H );
	return sat( th + 1.0 ) * ( n + 2.0 ) * 0.159155 * std::pow( sat( 1.0 - th * th ), 0.5 * n );
}

inline V3 hairStrand( V3 B, V3 N )
{
	V3 t = B - N * dot( N, B );
	const double l = std::sqrt( dot( t, t ) );
	return l > 1e-5 ? t * ( 1.0 / l ) : B;
}

//! per channel (the secondary lobe takes sqrt(albedo)); one channel here, albLin its value
inline double hairSpec( V3 T, V3 N, V3 L, V3 V, double gloss, double albLin )
{
	const double n = std::exp2( gloss * 10.0 + 1.0 );
	const V3 H = norm( V + L );
	const V3 T1 = norm( T + N * kHairS1 ), T2 = norm( T + N * kHairS2 );
	const double F = schlick( kHairF0, dot( H, L ) );
	const double s = 0.25 * F * ( hairD( T1, H, n ) + hairD( T2, H, n * 0.5 ) * std::sqrt( std::max( albLin, 0.0 ) ) );
	return std::min( s, 15.0 ) * 3.141593;
}

inline double eyeFv( V3 Ng, V3 V ) { return schlick( kEyeF0, dot( Ng, V ) ); }

inline double eyeCoat( V3 Ng, V3 L, V3 V )
{
	const double NdotL = dot( Ng, L ), NdotV = dot( Ng, V );
	if ( NdotL <= 0.0 || NdotV <= 0.0 )
		return 0.0;
	const V3 H = norm( V + L );
	const double NdotH = sat( dot( Ng, H ) );
	const double a2 = kEyeA * kEyeA;
	const double d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	const double D = a2 / ( 3.141593 * d * d );
	const double vis = 0.5 / ( NdotL * std::sqrt( NdotV * NdotV * ( 1.0 - a2 ) + a2 ) + NdotV * std::sqrt( NdotL * NdotL * ( 1.0 - a2 ) + a2 ) );
	return std::min( D * vis * schlick( kEyeF0, dot( V, H ) ), 15.0 ) * 3.141593;
}

}	// namespace wwhe

#endif
