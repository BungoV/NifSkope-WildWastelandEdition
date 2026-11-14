// profile: cs_6_0
// lane VKFAR3 (2026-11-14, plan row A1 row 2): the cell view's ported AO methods (res/shaders/features/ao/cell_aox.frag, readable
// twin res/shaders/hlsl/ao/cell_aox_ps.hlsl) on Vulkan compute, full size, 8x8 groups. src/features/ao/ao.cpp aoxPasses draws
// stage 1, stage 2 x4, then the method's stages (XeGTAO 6 7 5, GTAO 3 5 5, HBAO 4 5 5); aoxVk lists the same dispatches.
// XeGTAO's math is the readable include itself (cell_aox_xe.hlsli); the rest keeps cell_aox_ps.hlsl's expressions and order.
//   -D AOX_KERNEL=1  R32F: stage 1 (the working depth, T0 gbuf) or stage 2 (mip i.y from T1 = the level above)
//   -D AOX_KERNEL=2  RG32F: stage 6 (XeGTAO term + packed edges): T0 gbuf, T1..T4 depth mips 1..4
//   -D AOX_KERNEL=3  R8: stage 3 GTAO / 4 HBAO (T0 gbuf, T1..T4 mips 1..4), 5 bilateral (T0 gbuf, T1 src), 7 XeGTAO denoise (T1 src)
// Depth mip 0 is read from gbuf.a at the flipped row (stage 1's copy of it, bit for bit), so no stage needs 6 inputs.
// Push: c[0] ndcToView, c[1] aoxParams (radius, falloff, intensity, power), c[2] (slices, steps, denoise radius, depth sigma),
// c[3] (blur beta, full w, full h, -), c[4] (mip 1 w, h, mip 2 w, h), c[5] (mip 3, mip 4); i = (stage, mip level).
// Reds (gate only): AOX_Z_RED the depth filter's plain mean, AOX_XE_RED the falloff halved, AOX_R8_RED the blur beta doubled
// and the bilateral's depth sigma halved.
#ifndef AOX_KERNEL
#define AOX_KERNEL 3
#endif
#if AOX_KERNEL == 1
#define LPX_OUT0_FORMAT "r32f"
#define LPX_OUT0_TYPE float
#elif AOX_KERNEL == 2
#define LPX_OUT0_FORMAT "rg32f"
#define LPX_OUT0_TYPE float2
#else
#define LPX_OUT0_FORMAT "r8"
#define LPX_OUT0_TYPE float
#endif
#include "LightBindingsVk.hlsli"

// cell_aox.frag's uniforms, set from the push block (the names its XeGTAO include reads)
static int2 fullSize;
static int2 mipSize[5];
static int mipLevel;
static float4 ndcToView;
static float4 aoxParams;
static float2 aoxCounts;
static float3 aoxDenoise;

void aoxLoad()
{
	ndcToView = LpxJobPc.c[0];
	aoxParams = LpxJobPc.c[1];
	aoxCounts = LpxJobPc.c[2].xy;
	aoxDenoise = float3( LpxJobPc.c[2].zw, LpxJobPc.c[3].x );
	fullSize = int2( LpxJobPc.c[3].yz );
	mipSize[0] = fullSize;
	mipSize[1] = int2( LpxJobPc.c[4].xy );
	mipSize[2] = int2( LpxJobPc.c[4].zw );
	mipSize[3] = int2( LpxJobPc.c[5].xy );
	mipSize[4] = int2( LpxJobPc.c[5].zw );
	mipLevel = LpxJobPc.i.y;
#ifdef AOX_XE_RED
	aoxParams.y *= 0.5;
#endif
#ifdef AOX_R8_RED
	aoxDenoise.z *= 2.0;
	aoxDenoise.y *= 0.5;
#endif
}

static const float kAoxSky = 1.0e6 * 0.999;	// the far plane test, on the opaque pass's background

float aoxZMip( int m, int2 t )
{
	t = clamp( t, (int2)( 0 ), mipSize[m] - 1 );
	if ( m == 0 )
		return LpxT0.Load( int3( t.x, fullSize.y - 1 - t.y, 0 ) ).a;	// xz0 = gbuf.a, top row first
	if ( m == 1 )
		return LpxT1.Load( int3( t, 0 ) ).r;
	if ( m == 2 )
		return LpxT2.Load( int3( t, 0 ) ).r;
	if ( m == 3 )
		return LpxT3.Load( int3( t, 0 ) ).r;
	return LpxT4.Load( int3( t, 0 ) ).r;
}

float aoxZ( int2 t )
{
	return aoxZMip( 0, t );
}

float2 aoxSrc( int2 t )
{
	return LpxT1.Load( int3( clamp( t, (int2)( 0 ), fullSize - 1 ), 0 ) ).rg;
}

float3 aoxViewPos( float2 uv, float z )
{
	return float3( ( ndcToView.xy * uv + ndcToView.zw ) * z, z );
}

float3 aoxNormal( int2 t )
{
	t = clamp( t, (int2)( 0 ), fullSize - 1 );
	float3 n = LpxT0.Load( int3( t.x, fullSize.y - 1 - t.y, 0 ) ).xyz;
	return normalize( float3( n.x, n.y, -n.z ) );
}

// SSAO_StaticCleanNoise: the 4x4 tile the static denoise kernel reassembles
float2 aoxStaticNoise( int2 p )
{
	uint x = uint( p.x ) & 3u, y = uint( p.y ) & 3u, xy = x ^ y;
	uint index = ( ( ( xy >> 1 ) & 1u ) << 3 ) | ( ( ( y >> 1 ) & 1u ) << 2 ) | ( ( xy & 1u ) << 1 ) | ( y & 1u );
	uint stepIndex = ( ( index & 1u ) << 3 ) | ( ( index & 2u ) << 1 ) | ( ( index & 4u ) >> 1 ) | ( ( index & 8u ) >> 3 );
	return ( float2( float( index ), float( stepIndex ) ) + 0.5 ) * ( 1.0 / 16.0 );
}

uint aoxSelectMip( float len )
{
	return uint( clamp( log2( max( len, 1.0 ) ) - 3.30, 0.0, 4.0 ) );
}

// the R8_UNORM store of the FO4CS targets
float aoxUnorm8( float v )
{
	return floor( clamp( v, 0.0, 1.0 ) * 255.0 + 0.5 ) / 255.0;
}

#define WW_HLSL_SRC 1	// the include's readable half
#include "../ao/cell_aox_xe.hlsli"

#if AOX_KERNEL == 1
float aoxStage1( int2 P )
{
	int2 t = min( P, fullSize - 1 );
	return LpxT0.Load( int3( t.x, fullSize.y - 1 - t.y, 0 ) ).a;
}

float aoxStage2( int2 P )
{
	int2 p = P * 2, s = mipSize[mipLevel - 1] - 1;
	float4 d = float4( LpxT1.Load( int3( min( p, s ), 0 ) ).r, LpxT1.Load( int3( min( p + int2( 1, 0 ), s ), 0 ) ).r,
		LpxT1.Load( int3( min( p + int2( 0, 1 ), s ), 0 ) ).r, LpxT1.Load( int3( min( p + int2( 1, 1 ), s ), 0 ) ).r );
	// SSAO_DepthMIPFilter: the 2x2 mean weighted toward the far side, cut past the falloff
	float radius = 0.75 * max( aoxParams.x, 1.0e-4 );
	float range = clamp( aoxParams.y, 0.0, 1.0 ) * radius;
	float from = radius * ( 1.0 - clamp( aoxParams.y, 0.0, 1.0 ) );
	float mul = -1.0 / max( range, 1.0e-4 );
	float add = from / max( range, 1.0e-4 ) + 1.0;
	float maxDepth = max( max( d.x, d.y ), max( d.z, d.w ) );
	float4 w = clamp( ( maxDepth - d ) * mul + add, 0.0, 1.0 );
#ifdef AOX_Z_RED
	w = (float4)( 1.0 );
#endif
	return dot( w, d ) / max( w.x + w.y + w.z + w.w, 1.0e-6 );
}
#endif

#if AOX_KERNEL == 3
float aoxGtao( int2 P )
{
	int2 pmax = fullSize - 1;
	float centerZ = aoxZ( P );
	if ( centerZ >= kAoxSky )
		return 1.0;
	float2 pix = 1.0 / float2( fullSize );
	float3 centerView = aoxViewPos( ( float2( P ) + 0.5 ) * pix, centerZ );
	float3 viewVec = normalize( -centerView );
	float3 normalView = aoxNormal( P );
	float effectRadius = max( aoxParams.x, 0.01 );
	float sliceCount = aoxCounts.x, stepCount = aoxCounts.y;
	float viewPerWorkPixel = max( ndcToView.x * centerZ * pix.x, 1.0e-5 );
	float screenspaceRadius = clamp( effectRadius / viewPerWorkPixel, 2.0, 96.0 );
	float minS = 1.3 / screenspaceRadius;
	float falloffRadiusView = min( effectRadius, screenspaceRadius * viewPerWorkPixel );
	float falloffRange = clamp( aoxParams.y, 0.0, 1.0 ) * falloffRadiusView;
	float falloffFrom = falloffRadiusView * ( 1.0 - clamp( aoxParams.y, 0.0, 1.0 ) );
	float falloffMul = -1.0 / max( falloffRange, 1.0e-4 );
	float falloffAdd = falloffFrom / max( falloffRange, 1.0e-4 ) + 1.0;
	float visibility = clamp( ( 10.0 - screenspaceRadius ) / 100.0, 0.0, 1.0 ) * 0.5;
	float2 noise = aoxStaticNoise( P );
	for ( float slice = 0.0; slice < sliceCount; slice++ ) {
		float phi = ( slice + noise.x ) / sliceCount * 3.14159265359;
		float cosPhi = cos( phi ), sinPhi = sin( phi );
		float2 omega = float2( cosPhi, -sinPhi );
		float3 directionVec = float3( cosPhi, sinPhi, 0.0 );
		float3 orthoDirectionVec = directionVec - dot( directionVec, viewVec ) * viewVec;
		float3 axisVec = normalize( cross( orthoDirectionVec, viewVec ) );
		float3 projectedNormalVec = normalView - axisVec * dot( normalView, axisVec );
		float projectedNormalVecLength = length( projectedNormalVec );
		float cosNorm = clamp( dot( projectedNormalVec, viewVec ) / max( projectedNormalVecLength, 1.0e-5 ), 0.0, 1.0 );
		float n = sign( dot( orthoDirectionVec, projectedNormalVec ) ) * acos( cosNorm );
		float lowHorizonCos0 = cos( n + 1.57079632679 );
		float lowHorizonCos1 = cos( n - 1.57079632679 );
		float horizonCos0 = lowHorizonCos0, horizonCos1 = lowHorizonCos1;
		for ( float step = 0.0; step < stepCount; step++ ) {
			float s = ( step + noise.y ) / stepCount;
			s = s * s + minS;
			float2 offsetPixels = s * omega * screenspaceRadius;
			uint tapMip = aoxSelectMip( length( offsetPixels ) );
			int2 sp0 = clamp( int2( round( float2( P ) + offsetPixels ) ), (int2)( 0 ), pmax );
			float z0 = aoxZMip( int( tapMip ), sp0 >> int( tapMip ) );
			float3 delta0 = aoxViewPos( ( float2( sp0 ) + 0.5 ) * pix, z0 ) - centerView;
			float dist0 = length( delta0 );
			float shc0 = dot( delta0 / max( dist0, 1.0e-5 ), viewVec );
			float w0 = clamp( dist0 * falloffMul + falloffAdd, 0.0, 1.0 );
			horizonCos0 = max( horizonCos0, lerp( lowHorizonCos0, shc0, w0 ) );
			int2 sp1 = clamp( int2( round( float2( P ) - offsetPixels ) ), (int2)( 0 ), pmax );
			float z1 = aoxZMip( int( tapMip ), sp1 >> int( tapMip ) );
			float3 delta1 = aoxViewPos( ( float2( sp1 ) + 0.5 ) * pix, z1 ) - centerView;
			float dist1 = length( delta1 );
			float shc1 = dot( delta1 / max( dist1, 1.0e-5 ), viewVec );
			float w1 = clamp( dist1 * falloffMul + falloffAdd, 0.0, 1.0 );
			horizonCos1 = max( horizonCos1, lerp( lowHorizonCos1, shc1, w1 ) );
		}
		float h0 = -acos( clamp( horizonCos1, -1.0, 1.0 ) );
		float h1 = acos( clamp( horizonCos0, -1.0, 1.0 ) );
		float sinN = sin( n );
		float iarc0 = ( cosNorm + 2.0 * h0 * sinN - cos( 2.0 * h0 - n ) ) * 0.25;
		float iarc1 = ( cosNorm + 2.0 * h1 * sinN - cos( 2.0 * h1 - n ) ) * 0.25;
		visibility += projectedNormalVecLength * ( iarc0 + iarc1 );
	}
	visibility = clamp( visibility / sliceCount, 0.0, 1.0 );
	float occlusion = clamp( ( 1.0 - visibility ) * max( aoxParams.z, 0.0 ), 0.0, 1.0 );
	return pow( clamp( 1.0 - occlusion, 0.0, 1.0 ), max( aoxParams.w, 0.0001 ) );
}

float aoxHbao( int2 P )
{
	int2 pmax = fullSize - 1;
	float centerZ = aoxZ( P );
	if ( centerZ >= kAoxSky )
		return 1.0;
	float2 pix = 1.0 / float2( fullSize );
	float3 centerView = aoxViewPos( ( float2( P ) + 0.5 ) * pix, centerZ );
	float3 normalView = aoxNormal( P );
	float effectRadius = max( aoxParams.x, 0.01 );
	float tangentBiasSin = sin( clamp( aoxParams.y, 0.0, 1.0 ) * ( 3.14159265359 / 12.0 ) );
	float dirCount = aoxCounts.x, stepCount = aoxCounts.y;
	float viewPerWorkPixel = max( ndcToView.x * centerZ * pix.x, 1.0e-5 );
	float screenspaceRadius = clamp( effectRadius / viewPerWorkPixel, 2.0, 96.0 );
	float falloffRadiusView = min( effectRadius, screenspaceRadius * viewPerWorkPixel );
	float smallRadiusFade = clamp( ( 10.0 - screenspaceRadius ) / 100.0, 0.0, 1.0 ) * 0.5;
	// SSAO_InterleavedGradientNoise, frame index 0
	float noise = frac( 52.9829189 * frac( dot( float2( P ), float2( 0.06711056, 0.00583715 ) ) ) );
	float occlusion = 0.0;
	for ( float d = 0.0; d < dirCount; d++ ) {
		float phi = ( d + noise ) / dirCount * ( 2.0 * 3.14159265359 );
		float2 dir = float2( cos( phi ), -sin( phi ) );
		float horizonExcess = 0.0;
		float stepNoise = frac( noise + d * 0.6180339887 );
		for ( float s = 1.0; s <= stepCount; s++ ) {
			float t = ( s - stepNoise ) / stepCount;
			float2 offset = dir * ( t * screenspaceRadius );
			int2 sp = clamp( int2( round( float2( P ) + offset ) ), (int2)( 0 ), pmax );
			uint m = aoxSelectMip( length( offset ) );
			float sz = aoxZMip( int( m ), sp >> int( m ) );
			float3 delta = aoxViewPos( ( float2( sp ) + 0.5 ) * pix, sz ) - centerView;
			float dist = length( delta );
			float sinElev = dot( delta / max( dist, 1.0e-5 ), normalView );
			float atten = clamp( 1.0 - ( dist * dist ) / max( falloffRadiusView * falloffRadiusView, 1.0e-4 ), 0.0, 1.0 );
			horizonExcess = max( horizonExcess, max( sinElev - tangentBiasSin, 0.0 ) * atten );
		}
		occlusion += horizonExcess;
	}
	occlusion = clamp( ( occlusion - smallRadiusFade ) / dirCount * max( aoxParams.z, 0.0 ) * 2.0, 0.0, 1.0 );
	return pow( clamp( 1.0 - occlusion, 0.0, 1.0 ), max( aoxParams.w, 0.0001 ) );
}

// DenoiseCS, static sampling: a (2r+1)^2 box with half-weight rims, cut by the relative depth
float aoxBilateral( int2 P )
{
	int radius = clamp( int( aoxDenoise.x ), 1, 4 );
	float centerDepth = aoxZ( P );
	float invDepthSigma = 1.0 / max( aoxDenoise.y * centerDepth, 1.0e-3 );
	float weightedAO = 0.0, weightSum = 0.0;
	for ( int y = -radius; y <= radius; ++y ) {
		for ( int x = -radius; x <= radius; ++x ) {
			int2 q = P + int2( x, y );
			float dz = abs( aoxZ( q ) - centerDepth ) * invDepthSigma;
			float spatial = ( abs( x ) == radius ? 0.5 : 1.0 ) * ( abs( y ) == radius ? 0.5 : 1.0 );
			float weight = spatial * exp( -dz * dz );
			weightedAO += aoxSrc( q ).r * weight;
			weightSum += weight;
		}
	}
	return weightedAO / max( weightSum, 1.0e-4 );
}
#endif

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	aoxLoad();
	const int2 P = int2( id.xy );
	const int stage = LpxJobPc.i.x;
#if AOX_KERNEL == 1
	if ( stage == 1 )
		LpxOut0[id.xy] = aoxStage1( P );
	else
		LpxOut0[id.xy] = aoxStage2( P );
#elif AOX_KERNEL == 2
	// the background (no normal): unoccluded (255 / 1.5), open to its sky neighbours so the denoise keeps it whole
	if ( aoxZ( P ) >= kAoxSky )
		LpxOut0[id.xy] = float2( 170.0, XeGTAO_PackEdges( (float4)( 1.0 ) ) );
	else
		LpxOut0[id.xy] = XeGTAO_MainPass( P, aoxCounts.x, aoxCounts.y, aoxStaticNoise( P ), aoxNormal( P ) );
#else
	float v;
	if ( stage == 3 )
		v = aoxGtao( P );
	else if ( stage == 4 )
		v = aoxHbao( P );
	else if ( stage == 5 )
		v = aoxBilateral( P );
	else
		v = XeGTAO_Denoise( P, aoxDenoise.z );
	LpxOut0[id.xy] = aoxUnorm8( v );
#endif
}
