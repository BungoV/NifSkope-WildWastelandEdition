#ifndef WWTRANSL_H
#define WWTRANSL_H

// lane MATLOOK1 (TRANSLUC1): the C++ twin of res/shaders/cell_transl.glsl, a v8+ material's thin translucency
// (bTranslucency) as FO4CS's AdvancedLighting port draws it. Pure header (no Qt): tests/translucency pins it to an
// independent reading of that law and pins the shader's text to it. WWTRANSL_RED takes the upstream FO76 term instead.

#include <algorithm>
#include <cmath>

namespace WwTransl {

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 v3( float x, float y, float z ) { return V3{ x, y, z }; }
inline V3 operator*( V3 a, V3 b ) { return v3( a.x * b.x, a.y * b.y, a.z * b.z ); }
inline V3 operator*( V3 a, float s ) { return v3( a.x * s, a.y * s, a.z * s ); }
inline V3 operator-( V3 a, V3 b ) { return v3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline V3 operator-( V3 a ) { return v3( -a.x, -a.y, -a.z ); }
inline float dot( V3 a, V3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 vmax0( V3 a ) { return v3( std::max( a.x, 0.0f ), std::max( a.y, 0.0f ), std::max( a.z, 0.0f ) ); }

// the pixel's record: tint x amount, the quantised turbulence, and its kind (0 none, 1 thin, 2 thick)
struct Rec { V3 col; float turb = 0; int kind = 0; };

inline Rec record( V3 colour, bool mixAlbedo, V3 albedoLin, float lightingMapB, float scale, float turbulence,
				   bool emissiveMasked, bool thick )
{
	Rec r;
	const float amount = lightingMapB * scale;
	if ( emissiveMasked || !( amount > 0.001f ) )
		return r;
	const V3 tint = mixAlbedo ? colour * albedoLin : colour;
	r.col = vmax0( tint * amount );
	r.turb = std::floor( std::clamp( turbulence, 0.0f, 1.0f ) * 127.0f + 0.5f ) / 127.0f;
	r.kind = thick ? 2 : 1;
	return r;
}

inline float lobe( V3 N, V3 L, V3 V, float turbulence )
{
	const float v = std::max( dot( V, -L ), 0.0f );
	const float x = dot( L, -N ) * v * v;
	return std::max( x + ( 0.5f - x ) * turbulence, 0.0f );
}

// one light's excess over the diffuse it already gave (N, L, V unit; L and V point away from the surface)
inline V3 excess( const Rec & r, V3 N, V3 L, V3 V, V3 lightColour, V3 diffuseLit )
{
	if ( r.kind != 1 )
		return V3{};
#ifdef WWTRANSL_RED
	// the upstream FO76 term (res/shaders/f76_default.frag): no view lobe, no max( T, D )
	(void)V;
	(void)diffuseLit;
	return r.col * lightColour * std::max( -dot( N, L ), 0.0f );
#else
	return vmax0( r.col * lightColour * lobe( N, L, V, r.turb ) - diffuseLit );
#endif
}

}	// namespace WwTransl

#endif
