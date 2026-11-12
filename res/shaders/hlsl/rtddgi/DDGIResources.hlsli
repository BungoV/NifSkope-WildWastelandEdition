// DDGI volume, shared reads of the volume's buffers (lane VKCOMPUTE1, 2026-11-12). No bindings: the app's shim declares
// DDGIRays, DDGIIrradiance, DDGIDistance, DDGIProbes, DDGIAge, DDGITriAttr, DDGIRemap, DDGIVerts, DDGITriOf, DDGIMaskTris,
// DDGIMaps, DDGIAlpha, DDGILights, DDGI_P (the DDGIParams) and DDGI_JOB (the DDGIJob) before including this.
// Converted from src/features/gi/rtddgi/ddgi_common.glsl.
#ifndef DDGI_RESOURCES_HLSLI
#define DDGI_RESOURCES_HLSLI

#include "DDGICommon.hlsli"

float3 DDGI_ProbePos( uint p ) { return DDGI_P.origin.xyz + float3( DDGI_ProbeCoord( DDGI_P, p ) ) * DDGI_P.stepv.xyz + DDGIProbes[p].xyz; }
int DDGI_ProbeState( uint p ) { return int( DDGIProbes[p].w + 0.5 ); }

// MIRROR rtddgimath.h rayDir with this job's rotation
float3 DDGI_JobRayDir( int i, int n ) { return DDGI_RayDir( i, n, DDGI_JOB.rot0.xyz, DDGI_JOB.rot1.xyz, DDGI_JOB.rot2.xyz ); }

float3 DDGI_SampleIrr( uint p, float3 d )
{
	int2 c;
	float2 a;
	DDGI_TileTaps( d, DDGI_IRR_S, c, a );
	const uint b = p * uint( DDGI_IRR_S * DDGI_IRR_S );
	const float3 t00 = DDGIIrradiance[b + uint( c.y * DDGI_IRR_S + c.x )].rgb, t10 = DDGIIrradiance[b + uint( c.y * DDGI_IRR_S + c.x + 1 )].rgb;
	const float3 t01 = DDGIIrradiance[b + uint( ( c.y + 1 ) * DDGI_IRR_S + c.x )].rgb, t11 = DDGIIrradiance[b + uint( ( c.y + 1 ) * DDGI_IRR_S + c.x + 1 )].rgb;
	return lerp( lerp( t00, t10, a.x ), lerp( t01, t11, a.x ), a.y );
}
float2 DDGI_SampleDist( uint p, float3 d )
{
	int2 c;
	float2 a;
	DDGI_TileTaps( d, DDGI_DIST_S, c, a );
	const uint b = p * uint( DDGI_DIST_S * DDGI_DIST_S );
	const float2 t00 = DDGIDistance[b + uint( c.y * DDGI_DIST_S + c.x )], t10 = DDGIDistance[b + uint( c.y * DDGI_DIST_S + c.x + 1 )];
	const float2 t01 = DDGIDistance[b + uint( ( c.y + 1 ) * DDGI_DIST_S + c.x )], t11 = DDGIDistance[b + uint( ( c.y + 1 ) * DDGI_DIST_S + c.x + 1 )];
	return lerp( lerp( t00, t10, a.x ), lerp( t01, t11, a.x ), a.y );
}

// the alpha-test hole at barycentrics (b1, b2) of soup triangle tri (src/features/gi/bakert/common.glsl's hole)
bool DDGI_Hole( int tri, float b1, float b2 )
{
	bool wwh0 = DDGI_P.flags.z == 0;
	bool wwh1 = tri < 0;
	bool wwh2 = tri >= DDGI_P.counts.z;
	if (wwh0 || wwh1 || wwh2)
		return false;
	const int m = DDGITriOf[tri];
	if ( m < 0 )
		return false;
	const uint b = uint( m ) * 12u;
	const float b0 = 1.0 - b1 - b2;
	float u = b0 * asfloat( DDGIMaskTris[b + 2u] ) + b1 * asfloat( DDGIMaskTris[b + 4u] ) + b2 * asfloat( DDGIMaskTris[b + 6u] );
	float v = b0 * asfloat( DDGIMaskTris[b + 3u] ) + b1 * asfloat( DDGIMaskTris[b + 5u] ) + b2 * asfloat( DDGIMaskTris[b + 7u] );
	const uint4 mp = DDGIMaps[DDGIMaskTris[b]];
	u -= floor( u );
	v -= floor( v );
	const int x = clamp( int( u * float( mp.y ) ), 0, int( mp.y ) - 1 );
	const int y = clamp( int( v * float( mp.z ) ), 0, int( mp.z ) - 1 );
	const uint off = mp.x + uint( y ) * mp.y + uint( x );
	const uint a = ( DDGIAlpha[off >> 2u] >> ( ( off & 3u ) * 8u ) ) & 255u;
	const uint thr = DDGIMaskTris[b + 1u];
	if ( DDGIMaskTris[b + 11u] == 0u )
		return a < thr;
	const float s = b0 * asfloat( DDGIMaskTris[b + 8u] ) + b1 * asfloat( DDGIMaskTris[b + 9u] ) + b2 * asfloat( DDGIMaskTris[b + 10u] );
	return float( a ) * s < float( thr );
}

#endif
