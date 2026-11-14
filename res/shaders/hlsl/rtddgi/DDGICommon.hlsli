// DDGI volume, shared math (lane VKCOMPUTE1, 2026-11-12). One file for both apps: NifSkope's Vulkan device (SPIR-V) and
// Fusion Shaders (F4FS, DXIL). NO bindings here: each app declares the resources in its own shim (NifSkope:
// DDGIBindingsVk.hlsli) and the kernels' bodies (DDGI<Kernel>.hlsli) read them by the names that shim gives.
// Converted from src/features/gi/rtddgi/ddgi_common.glsl, the same math line for line. Every MIRROR mark names
// src/features/gi/rtddgimath.h's function (the C++ twin the unit test checks; change all three or none).
// SM 6.0 math (no wave ops, no 16-bit types): compiles as cs_6_0 to DXIL and to SPIR-V.
// ddgi_common.glsl's header:
// Lane RTXGI1 (2026-10-29): what the DDGI kernels share -- the bindings, the volume's parameters and the probe math.
// Every function marked MIRROR is src/features/gi/rtddgimath.h's function of that name, line for line (the C++ is the twin the
// unit test checks; change both or neither). Compiled into trace/blend/border/reloc.comp by tools/rtddgi_spirv.sh.
// (HLSL: the bindings live in the shim, DDGIBindingsVk.hlsli; the parameters and the math here.)
#ifndef DDGI_COMMON_HLSLI
#define DDGI_COMMON_HLSLI

#define DDGI_PI 3.14159265358979323846
#define DDGI_FIXED 32
#define DDGI_IRR_N 6
#define DDGI_IRR_S 8
#define DDGI_DIST_N 14
#define DDGI_DIST_S 16
#define DDGI_MISS 1e27
#define DDGI_TMIN 1e-4   // rtddgimath.h kTraceTmin: probebvh's t > 1e-4
#define DDGI_OFF 0
#define DDGI_ASLEEP 1
#define DDGI_VIGILANT 2
#define DDGI_FRESH 3     // M3: rtddgimath.h kFresh, a probe a scroll reset (relocated + classified alone, never read)
// red bits (params.flags.w): the volume's runtime refuters (src/features/gi/rtddgi.h RtDdgiSpec::red)
// (HLSL: params = DDGIParams)
#define DDGI_RED_NOBACK 1u   // backface hits shaded as front faces, distance kept
#define DDGI_RED_NOCHEB 2u   // the bounce sampler without the Chebyshev factor
#define DDGI_RED_NOBIAS 4u   // the bounce sampler without the self-shadow bias
#define DDGI_RED_HWHIT 8u    // GPU only: M1's trace, the hardware's hit on the grown BLAS (no true-triangle test, tmin 0, no tie pass)
#define DDGI_RED_NOTIE 16u   // no tie pass: the nearest face whichever the tree keeps (CPU and GPU)

// the volume's parameters (std430 / structured-buffer layout, 18 float4)
struct DDGIParams {
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
	int4 skipLo;      // RTXCASCADE2: the probes the updates skip, grid indices skipLo.xyz..skipHi.xyz; skipLo.w = 1 on
	int4 skipHi;
};

// the per-pass job (NifSkope: push constants).
// mode 0 trace an update, 1 trace the fixed rays (relocation / classification), 2 blend, 3 border, 4 relocate, 5 classify
// M3: modes 1, 4 and 5 with job.y = 1 take only DDGI_FRESH probes (a scroll's new planes); 0 = every probe (M1's)
struct DDGIJob {
	float4 rot0;   // this update's ray rotation, rows
	float4 rot1;
	float4 rot2;
	uint4 job;     // job: mode, frame, ray first, ray count
};

float DDGI_SignNotZero( float v ) { return v >= 0.0 ? 1.0 : -1.0; }

// MIRROR rtddgimath.h octEncode
float2 DDGI_OctEncode( float3 d )
{
	const float l1 = abs( d.x ) + abs( d.y ) + abs( d.z );
	float2 uv = d.xy / l1;
	if ( d.z < 0.0 ) {
		const float ou = uv.x;
		uv.x = ( 1.0 - abs( uv.y ) ) * DDGI_SignNotZero( ou );
		uv.y = ( 1.0 - abs( ou ) ) * DDGI_SignNotZero( uv.y );
	}
	return uv;
}
// MIRROR rtddgimath.h octDecode
float3 DDGI_OctDecode( float2 uv )
{
	float3 d = float3( uv, 1.0 - abs( uv.x ) - abs( uv.y ) );
	if ( d.z < 0.0 ) {
		const float ox = d.x;
		d.x = ( 1.0 - abs( d.y ) ) * DDGI_SignNotZero( ox );
		d.y = ( 1.0 - abs( ox ) ) * DDGI_SignNotZero( d.y );
	}
	return normalize( d );
}
// MIRROR rtddgimath.h texelDir
float3 DDGI_TexelDir( int tx, int ty, int n )
{
	return DDGI_OctDecode( float2( ( float( tx ) + 0.5 ) / float( n ) * 2.0 - 1.0, ( float( ty ) + 0.5 ) / float( n ) * 2.0 - 1.0 ) );
}
// MIRROR rtddgimath.h borderSource
int2 DDGI_BorderSource( int x, int y, int S )
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
int2 DDGI_BorderXY( int k, int S )
{
	if ( k < S ) return int2( k, 0 );
	k -= S;
	if ( k < S ) return int2( k, S - 1 );
	k -= S;
	if ( k < S - 2 ) return int2( 0, 1 + k );
	k -= S - 2;
	return int2( S - 1, 1 + k );
}
// MIRROR rtddgimath.h sampleTile (the bilinear corner and weights; one reader per buffer below)
// (HLSL: the readers are in DDGIResources.hlsli)
void DDGI_TileTaps( float3 d, int S, out int2 x0y0, out float2 a )
{
	const float2 uv = DDGI_OctEncode( d );
	const int n = S - 2;
	const float2 f = float2( 1.0, 1.0 ) + ( uv * 0.5 + float2( 0.5, 0.5 ) ) * float( n ) - float2( 0.5, 0.5 );
	x0y0 = clamp( int2( floor( f ) ), int2( 0, 0 ), int2( S - 2, S - 2 ) );
	a = clamp( f - float2( x0y0 ), float2( 0.0, 0.0 ), float2( 1.0, 1.0 ) );
}
// MIRROR rtddgimath.h sphericalFibonacci
float3 DDGI_Fibonacci( int i, int n )
{
	const float kPhiMinus1 = 0.61803398874989484820;
	const float phi = 2.0 * DDGI_PI * frac( float( i ) * kPhiMinus1 );
	const float cosT = 1.0 - ( 2.0 * float( i ) + 1.0 ) / float( n );
	const float sinT = sqrt( clamp( 1.0 - cosT * cosT, 0.0, 1.0 ) );
	return float3( cos( phi ) * sinT, sin( phi ) * sinT, cosT );
}
// MIRROR rtddgimath.h rayDir (the rotation's rows in pc.rot0..2)
// (HLSL: rot0..2 = DDGIJob's rows)
float3 DDGI_RayDir( int i, int n, float3 rot0, float3 rot1, float3 rot2 )
{
	if ( i < DDGI_FIXED )
		return DDGI_Fibonacci( i, DDGI_FIXED );
	const float3 f = DDGI_Fibonacci( i - DDGI_FIXED, n - DDGI_FIXED );
	return normalize( float3( dot( rot0, f ), dot( rot1, f ), dot( rot2, f ) ) );
}
// MIRROR rtddgimath.h tieWindow
float DDGI_TieWindow( float3 o, float t ) { return 1e-4 + 2e-6 * ( abs( o.x ) + abs( o.y ) + abs( o.z ) + t ); }
// MIRROR rtddgimath.h tieWins
bool DDGI_TieWins( bool backA, int triA, bool backB, int triB )
{
	if ( backA != backB )
		return !backA;
	return triA < triB;
}
// MIRROR rtddgimath.h gammaEncode / gammaDecode
float DDGI_GammaEncode( float x ) { return pow( max( x, 0.0 ), 1.0 / 5.0 ); }
float DDGI_GammaDecode( float e ) { return pow( max( e, 0.0 ), 5.0 ); }
// MIRROR rtddgimath.h texelHysteresis
float DDGI_TexelHysteresis( float h, float3 oldEnc, float3 newEnc )
{
	const float3 dv = abs( newEnc - oldEnc );
	const float ch = max( dv.x, max( dv.y, dv.z ) );
	if ( ch > 0.25 )
		h = max( 0.0, h - 0.15 );
	if ( ch > 0.8 )
		h = 0.0;
	return h;
}
// MIRROR rtddgimath.h ageHysteresis
float DDGI_AgeHysteresis( uint a, float h )
{
	return a == 0u ? 0.0 : min( h, float( a ) / float( a + 1u ) );
}
// MIRROR rtddgimath.h biasVector
float3 DDGI_BiasVector( float3 n, float3 o, float D, float B ) { return ( n * 0.2 + o * 0.8 ) * ( 0.75 * D * B ); }
// MIRROR rtddgimath.h backfaceWeight
float DDGI_BackfaceWeight( float3 dirToProbe, float3 n )
{
	const float c = ( dot( dirToProbe, n ) + 1.0 ) * 0.5;
	return c * c + 0.2;
}
// MIRROR rtddgimath.h chebyshevWeight
float DDGI_Chebyshev( float r, float mean, float mean2 )
{
	if ( r <= mean )
		return 1.0;
	const float var = abs( mean * mean - mean2 );
	const float d = r - mean;
	float c = var / max( var + d * d, 1e-12 );
	c = c * c * c;
	return max( 0.05, max( c, 0.0 ) );
}
// MIRROR rtddgimath.h crushWeight
float DDGI_Crush( float w ) { return w < 0.2 ? w * w * w / ( 0.2 * 0.2 ) : w; }
// MIRROR rtddgimath.h lightRadial
float DDGI_Radial( float d, float r, float bias, float scale, float expo )
{
	const float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
	const float xe = expo > 0.0 ? pow( x, expo ) : 1.0;
	const float k = 1.0 - clamp( scale * xe + bias, 0.0, 1.0 );
	return pow( k, 2.2 );
}
// MIRROR rtddgimath.h spotCone
float DDGI_Spot( float dl, float cosOuter, float cone )
{
	const float base = clamp( 1.0 - ( 1.0 - dl ) / max( 1.0 - cosOuter, 1e-4 ), 0.0, 1.0 );
	return min( pow( base, max( cone, 1e-3 ) ), 1.0 );
}
// MIRROR rtddgimath.h skyRadiance
float3 DDGI_Sky( DDGIParams p, float3 d )
{
	const float3 rx = d.x >= 0.0 ? p.sky[0].rgb : p.sky[1].rgb;
	const float3 ry = d.y >= 0.0 ? p.sky[2].rgb : p.sky[3].rgb;
	const float3 rz = d.z >= 0.0 ? p.sky[4].rgb : p.sky[5].rgb;
	return d.x * d.x * rx + d.y * d.y * ry + d.z * d.z * rz;
}
// lane SKYBELOW1 (bungo 10-30): below the horizon is ground, never sky. P.sky[1..3].w = the ground's radiance (an
// exterior's albedo x the sky irradiance from above / pi; 0 in an interior), P.sky[4].w = 1 the rule, 0 the red
// (WW_SKYBELOW_RED: today's main, the sky in every direction and every miss sky visibility)
// MIRROR rtddgimath.h belowHorizon
bool DDGI_BelowHorizon( float3 d ) { return d.z < 0.0; }
// MIRROR rtddgimath.h skyVisRay
bool DDGI_SkyVisRay( DDGIParams p, float3 d, float dist ) { return dist >= DDGI_MISS * 0.5 && !( p.sky[4].w > 0.5 && DDGI_BelowHorizon( d ) ); }
// MIRROR rtddgimath.h missRadiance
float3 DDGI_Miss( DDGIParams p, float3 d )
{
	if ( p.sky[4].w > 0.5 && DDGI_BelowHorizon( d ) )
		return float3( p.sky[1].w, p.sky[2].w, p.sky[3].w );
	return DDGI_Sky( p, d );
}
// probe p's grid index, position (grid + offset) and state
// (HLSL: the grid index here; position and state read the buffers, DDGIResources.hlsli)
int3 DDGI_ProbeCoord( DDGIParams p, uint i ) { return int3( int( i ) % p.dims.x, ( int( i ) / p.dims.x ) % p.dims.y, int( i ) / ( p.dims.x * p.dims.y ) ); }
// RTXCASCADE2: a coarse cascade's probe a finer cascade covers whole (the range: src/features/gi/rtddgimath.h cascadeCovered, set per
// step by the cell view; src/features/gi/rtddgi/rtddgi_vk.inc ddgiSkipped is this test on the CPU for the active count). The CPU
// twin never sets it (rtddgiRun is one volume): off, every probe as before
// (HLSL: skipLo.w = 0: off)
bool DDGI_Skipped( DDGIParams p, uint i )
{
	if ( p.skipLo.w == 0 )
		return false;
	const int3 g = int3( int( i % uint( p.dims.x ) ), int( ( i / uint( p.dims.x ) ) % uint( p.dims.y ) ),
		int( i / uint( p.dims.x * p.dims.y ) ) );
	return all( g >= p.skipLo.xyz ) && all( g <= p.skipHi.xyz );
}

#endif
