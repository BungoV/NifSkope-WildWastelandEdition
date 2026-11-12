// Bake tracer: what the three GPU tracers share (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/bakert/common.glsl and
// rqx.comp). Shared, no bindings: it reads BakeRTTriOf, BakeRTMaskTris, BakeRTMaps, BakeRTAlpha and the push constants
// BAKERT_PC by name (the shim declares them). Fusion Shaders (F4FS) feeds the same names from its own shim.
#ifndef BAKERT_COMMON_HLSLI
#define BAKERT_COMMON_HLSLI

#include "BakeRTTypes.hlsli"

float BakeRT_Mf( uint i ) { return asfloat( BakeRTMaskTris[i] ); }

// the hit at barycentrics (b1, b2) of soup triangle `tri` lands on a hole. MIRROR probemask.h AlphaMask::hole (float)
bool BakeRT_Hole( int tri, float b1, float b2 )
{
	if ( BAKERT_PC.useMask == 0u || tri < 0 || uint( tri ) >= BAKERT_PC.nTri )
		return false;
	const int m = BakeRTTriOf[tri];
	if ( m < 0 )
		return false;
	const uint b = uint( m ) * 12u;
	const float b0 = 1.0 - b1 - b2;
	float u = b0 * BakeRT_Mf( b + 2u ) + b1 * BakeRT_Mf( b + 4u ) + b2 * BakeRT_Mf( b + 6u );
	float v = b0 * BakeRT_Mf( b + 3u ) + b1 * BakeRT_Mf( b + 5u ) + b2 * BakeRT_Mf( b + 7u );
	const uint4 mp = BakeRTMaps[BakeRTMaskTris[b]];
	u -= floor( u );
	v -= floor( v );
	const int x = clamp( int( u * float( mp.y ) ), 0, int( mp.y ) - 1 );
	const int y = clamp( int( v * float( mp.z ) ), 0, int( mp.z ) - 1 );
	const uint off = mp.x + uint( y ) * mp.y + uint( x );
	const uint a = ( BakeRTAlpha[off >> 2u] >> ( ( off & 3u ) * 8u ) ) & 255u;
	const uint thr = BakeRTMaskTris[b + 1u];
	if ( BakeRTMaskTris[b + 11u] == 0u )   // no corner scale: the integer test
		return a < thr;
	const float s = b0 * BakeRT_Mf( b + 8u ) + b1 * BakeRT_Mf( b + 9u ) + b2 * BakeRT_Mf( b + 10u );
	return float( a ) * s < float( thr );
}

// Doubles: DXC's floor / cross / length take float only (a double argument is narrowed), so under SPIR-V they call the
// GLSL.std.450 ops on doubles directly (Floor 8, Length 66, Cross 68: what glslang emits for rqx.comp). DXIL has no double
// floor or sqrt: there they are emulated (int64 truncation; float sqrt + two Newton steps), not byte-checked.
#ifdef __spirv__
[[vk::ext_instruction( 8, "GLSL.std.450" )]] double BakeRT_SpvFloorD( double x );
[[vk::ext_instruction( 66, "GLSL.std.450" )]] double BakeRT_SpvLengthD( double3 x );
[[vk::ext_instruction( 68, "GLSL.std.450" )]] double3 BakeRT_SpvCrossD( double3 a, double3 b );
// DXC turns double( uint ) into a 64-bit integer widen first (the Int64 capability, which bakert.cpp's device does not
// enable): OpConvertUToF (112) direct, as glslang does
[[vk::ext_instruction( 112 )]] double BakeRT_SpvUToD( uint x );
#endif

double BakeRT_UToD( uint x )
{
#ifdef __spirv__
	return BakeRT_SpvUToD( x );
#else
	return double( x );
#endif
}

double BakeRT_FloorD( double x )
{
#ifdef __spirv__
	return BakeRT_SpvFloorD( x );
#else
	const double t = double( int64_t( x ) );   // |x| < 2^52 here (texel and UV coordinates)
	return t > x ? t - 1.0L : t;
#endif
}

double3 BakeRT_CrossD( double3 a, double3 b )
{
#ifdef __spirv__
	return BakeRT_SpvCrossD( a, b );
#else
	return double3( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
#endif
}

double BakeRT_DotD( double3 a, double3 b )
{
#ifdef __spirv__
	return dot( a, b );
#else
	return a.x * b.x + a.y * b.y + a.z * b.z;
#endif
}

double BakeRT_LengthD( double3 a )
{
#ifdef __spirv__
	return BakeRT_SpvLengthD( a );
#else
	const double s = a.x * a.x + a.y * a.y + a.z * a.z;
	double y = double( sqrt( float( s ) ) );
	if ( !( y > 0.0L ) )
		return 0.0L;
	y = 0.5L * ( y + s / y );
	return 0.5L * ( y + s / y );
#endif
}

#endif
