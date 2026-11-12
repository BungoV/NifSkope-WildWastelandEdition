// Lane VKCOMPUTE1 (2026-11-12): the DDGI kernels' shared part in HLSL, converted one kernel at a time from
// src/features/gi/rtddgi/ddgi_common.glsl: the same bindings, the same Params layout and the same math. Every MIRROR mark
// still names src/features/gi/rtddgimath.h's function (the C++ twin the unit test checks; change all three or none).
// It holds only what the converted kernels use so far; the rest arrives with each kernel (hlsl_plan.md).
// SM 6.0 through DXC both ways: -spirv for NifSkope's Vulkan device, DXIL (-T cs_6_0) so FO4CS's D3D12 path can take it.
#ifndef DDGI_COMMON_HLSLI
#define DDGI_COMMON_HLSLI

#ifdef __spirv__
#define DDGI_BIND( b ) [[vk::binding( b, 0 )]]
#else
#define DDGI_BIND( b )
#endif

#define DDGI_IRR_S 8
#define DDGI_DIST_S 16
#define DDGI_OFF 0
#define DDGI_ASLEEP 1
#define DDGI_VIGILANT 2
#define DDGI_FRESH 3

struct Params {
	int4 dims;        // probes x y z, total
	float4 origin;    // the grid's first probe (the soup's local frame), w = distClamp
	float4 stepv;     // probe spacing per axis, w = the smallest
	float4 sunTo;     // toward the sun, w = 1 on
	float4 sun;       // its colour, w = the trace tmax
	float4 dirTo;     // an interior's directional light, w = 1 on
	float4 dirColor;  // its colour, w = fixtureClear
	float4 sky[6];    // the sky radiance cube (+X -X +Y -Y +Z -Z), sky[0].w = 1 on
	int4 counts;      // rays per probe, lights, soup triangles, opaque triangles (the BLAS's geometry 0)
	int4 flags;       // oneBounce, skipAsleep, useMask, red bits
	float4 blendp;    // base hysteresis, distance sharpness, self-shadow bias B, 0
	int4 skipLo;      // the probes the updates skip, grid indices skipLo.xyz..skipHi.xyz; skipLo.w = 1 on
	int4 skipHi;
};

DDGI_BIND( 1 ) RWStructuredBuffer<float4> irr : register( u1 );         // per probe: 8x8 encoded rgb, a = sky visibility
DDGI_BIND( 2 ) RWStructuredBuffer<float2> dist : register( u2 );        // per probe: 16x16 mean, mean^2
DDGI_BIND( 3 ) RWStructuredBuffer<float4> probes : register( u3 );      // offset xyz, state
DDGI_BIND( 4 ) RWStructuredBuffer<uint> age : register( u4 );           // per probe: updates blended
DDGI_BIND( 14 ) StructuredBuffer<Params> ParamBuf : register( t14 );
#define P ParamBuf[0]

// HLSL has no gl_NumWorkGroups (DXIL has no such value): rtddgi_vk.inc ddgiDispatch splits a dispatch into x = min(groups,
// 65535) and y = the rest, so whenever y > 0 the x count is exactly 65535 and the linear id is this, for 64-wide groups
uint ddgiLinearId( uint3 gid ) { return gid.x + gid.y * ( 65535u * 64u ); }

int ddgiProbeState( uint p ) { return int( probes[p].w + 0.5 ); }

// MIRROR rtddgimath.h borderSource
int2 ddgiBorderSource( int x, int y, int S )
{
	const int n = S - 2;
	int2 s = int2( x, y );
	const bool l = x == 0, r = x == S - 1, t = y == 0, b = y == S - 1;
	if ( ( l || r ) && ( t || b ) )
		return int2( l ? n : 1, t ? n : 1 );
	if ( t ) s = int2( S - 1 - x, 1 );
	if ( b ) s = int2( S - 1 - x, n );
	if ( l ) s = int2( 1, S - 1 - y );
	if ( r ) s = int2( n, S - 1 - y );
	return s;
}

// MIRROR rtddgimath.h borderXY
int2 ddgiBorderXY( int k, int S )
{
	if ( k < S ) return int2( k, 0 );
	k -= S;
	if ( k < S ) return int2( k, S - 1 );
	k -= S;
	if ( k < S - 2 ) return int2( 0, 1 + k );
	k -= S - 2;
	return int2( S - 1, 1 + k );
}

// a coarse cascade's probe a finer cascade covers whole (rtddgimath.h cascadeCovered; skipLo.w = 0: off)
bool ddgiSkipped( uint p )
{
	if ( P.skipLo.w == 0 )
		return false;
	const int3 g = int3( int( p % uint( P.dims.x ) ), int( ( p / uint( P.dims.x ) ) % uint( P.dims.y ) ),
		int( p / uint( P.dims.x * P.dims.y ) ) );
	return all( g >= P.skipLo.xyz ) && all( g <= P.skipHi.xyz );
}

#endif
