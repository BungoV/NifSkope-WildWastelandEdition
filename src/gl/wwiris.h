#pragma once
/* lane SKINSSS2 (EYEIRIS1): the iris under the cornea (row "Eye iris refraction", WW_CELL_IRIS), the C++ twin of
 * wwIrisShift in res/shaders/ww_hair_eye.glsl. The eye atlas is painted from the front, so the eye's mesh carries the
 * iris on its surface. The real iris lies on a flat plane under the cornea: a cornea sphere of 7.8 mm over a limbus of
 * 5.85 mm (A = 4/3, the iris plane 2.64 mm under the apex), refractive index 1.376. Per fragment: rebuild the eye axis
 * from the sphere model (the normal leans out by asin( rho / A ) along the radial direction), refract the view at the
 * normal, step down to the iris plane (depth = the cornea's height over it there) and map the step back to uv across
 * the axis. Outside the limbus the step is 0. WWIRIS_RED (the shader's WW_IRIS_RED): no step, the old picture.
 * tests/eyeiris traces the eye itself and checks this against it. */
#include <cmath>
#include <cstring>

namespace WwIris
{
struct V3
{
	float x, y, z;
};
inline V3 operator+( V3 a, V3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-( V3 a, V3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*( V3 a, float s ) { return { a.x * s, a.y * s, a.z * s }; }
inline float dot( V3 a, V3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }

const float kA = 1.333333f;	// cornea radius / limbus radius (7.8 / 5.85 mm)
const float kH1 = 0.881917f;	// sqrt( A^2 - 1 ): the cornea's height is sqrt( A^2 - rho^2 ) - kH1 iris radii
const float kEta = 1.0f / 1.376f;

// GLSL refract( I, N, eta )
inline V3 refract( V3 I, V3 N, float eta )
{
	const float	c = dot( N, I );
	const float	k = 1.0f - eta * eta * ( 1.0f - c * c );
	if ( k < 0.0f )
		return { 0.0f, 0.0f, 0.0f };
	return I * eta - N * ( eta * c + std::sqrt( k ) );
}

/* The uv step. uv: the fragment's uv; iris: the atlas's iris (centre u, v, radius); P: position, V: unit view (toward
 * the eye), N: unit normal; dPx/dPy, dUx/dUy: the screen derivatives of P and uv (GLSL dFdx / dFdy). */
inline void shift( const float uv[2], const float iris[3], V3 P, V3 V, V3 N, V3 dPx, V3 dPy, const float dUx[2],
	const float dUy[2], float out[2] )
{
	(void) P;
	out[0] = out[1] = 0.0f;
#ifndef WWIRIS_RED
	const float	rx = ( uv[0] - iris[0] ) / iris[2], ry = ( uv[1] - iris[1] ) / iris[2];
	const float	rho2 = rx * rx + ry * ry;
	const float	j = dUx[0] * dUy[1] - dUx[1] * dUy[0];
	if ( rho2 >= 1.0f || std::fabs( j ) < 1.0e-20f )
		return;
	// dP per iris radius along u and v
	const V3	Tu = ( dPx * dUy[1] - dPy * dUx[1] ) * ( iris[2] / j );
	const V3	Tv = ( dPy * dUx[0] - dPx * dUy[0] ) * ( iris[2] / j );
	const float	g11 = dot( Tu, Tu ), g12 = dot( Tu, Tv ), g22 = dot( Tv, Tv );
	const float	s = std::sqrt( std::sqrt( std::fmax( g11 * g22 - g12 * g12, 0.0f ) ) );
	const float	h = std::sqrt( kA * kA - rho2 ) - kH1;
	V3	rad = Tu * rx + Tv * ry;
	rad = rad - N * dot( rad, N );
	const float	rl = std::sqrt( dot( rad, rad ) );
	const float	sa = std::sqrt( rho2 ) / kA;
	const V3	ax = ( rl > 1.0e-6f * s ) ? N * std::sqrt( 1.0f - sa * sa ) - rad * ( sa / rl ) : N;
	const V3	t = refract( V * -1.0f, N, kEta );
	const V3	d = t * ( h * s / std::fmax( -dot( t, ax ), 0.2f ) );
	const V3	pu = Tu - ax * dot( Tu, ax ), pv = Tv - ax * dot( Tv, ax ), pd = d - ax * dot( d, ax );
	const float	q11 = dot( pu, pu ), q12 = dot( pu, pv ), q22 = dot( pv, pv );
	const float	det = q11 * q22 - q12 * q12;
	if ( det <= 0.0f )
		return;
	const float	b1 = dot( pu, pd ), b2 = dot( pv, pd );
	out[0] = ( q22 * b1 - q12 * b2 ) / det * iris[2];
	out[1] = ( q11 * b2 - q12 * b1 ) / det * iris[2];
#else
	(void) uv; (void) iris; (void) V; (void) N; (void) dPx; (void) dPy; (void) dUx; (void) dUy;
#endif
}

/* The atlas's iris, measured on the vanilla eye textures (notes/skinsss2/tools/iris_profile.py: EyeBlue / EyeBrown /
 * EyeHazel / EyeLightGrey limbus at u 87..170 and v 151..234 of 256): centre (0.502, 0.752), radius 0.162. Every
 * textures/actors/character/eyes/ texture (human and ghoul eyes) shares that atlas; any other (the supermutants' own
 * layout) gets radius 0 = no step. */
inline void layout( const char * diffuse, float out[3] )
{
	out[0] = out[1] = out[2] = 0.0f;
	if ( !diffuse )
		return;
	char	low[512];
	size_t	n = 0;
	for ( ; diffuse[n] && n + 1 < sizeof( low ); n++ ) {
		char	c = diffuse[n] == '\\' ? '/' : diffuse[n];
		low[n] = ( c >= 'A' && c <= 'Z' ) ? char( c - 'A' + 'a' ) : c;
	}
	low[n] = 0;
	if ( !std::strstr( low, "actors/character/eyes/" ) )
		return;
	out[0] = 0.502f;
	out[1] = 0.752f;
	out[2] = 0.162f;
}
}
