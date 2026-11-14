// HLSL source of features/ao/cell_aox.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane SSAO2: the FO4 Community Shaders' three ambient occlusion methods for the cell view (src/gl/celllights.cpp),
 * at full resolution, in its view space (x right, y up, z forward) with every target top row first. gbuf is lane
 * AO1's opaque pass (bottom row first: GL view normal, linear depth; the background at 1e6).
 *   stage 1  the working depth mip 0 (the linear depth, top row first)
 *   stage 2  working depth mip 1..4: SSAO_DepthMIPFilter over 2x2 of the level above
 *   stage 3  GTAO (GTAOMainCS)        stage 4  HBAO+ (HBAOMainCS)        stage 5  the bilateral denoise (DenoiseCS)
 *   stage 6  XeGTAO main (term + edges)   stage 7  XeGTAO denoise (final apply) + resolve */

cbuffer LooseUniforms : register(b31, space0)
{
	int aoxStage;
	int2 fullSize;
	int2 mipSize[5];	// [0] = fullSize
	int mipLevel;
	float4 ndcToView;		// xy NDCToViewMul, zw NDCToViewAdd (uv in [0, 1], top row first)
	float4 aoxParams;		// effect radius (game units), falloff range, intensity, power
	float2 aoxCounts;		// slices, steps per slice
	float3 aoxDenoise;	// radius, depth sigma, XeGTAO blur beta
};

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);
Texture2D<float4> xz0 : register(t1, space0); SamplerState _xz0_sampler : register(s1, space0);
Texture2D<float4> xz1 : register(t2, space0); SamplerState _xz1_sampler : register(s2, space0);
Texture2D<float4> xz2 : register(t3, space0); SamplerState _xz2_sampler : register(s3, space0);
Texture2D<float4> xz3 : register(t4, space0); SamplerState _xz3_sampler : register(s4, space0);
Texture2D<float4> xz4 : register(t5, space0); SamplerState _xz4_sampler : register(s5, space0);
Texture2D<float4> src : register(t6, space0); SamplerState _src_sampler : register(s6, space0);

static float4 fragColor;

static const float kAoxSky = 1.0e6 * 0.999;	// the far plane test, on the opaque pass's background

float aoxZMip( int m, int2 t )
{
	t = clamp( t, (int2)( 0 ), mipSize[m] - 1 );
	if ( m == 0 )
		return xz0.Load( int3(t, 0) ).r;
	if ( m == 1 )
		return xz1.Load( int3(t, 0) ).r;
	if ( m == 2 )
		return xz2.Load( int3(t, 0) ).r;
	if ( m == 3 )
		return xz3.Load( int3(t, 0) ).r;
	return xz4.Load( int3(t, 0) ).r;
}

float aoxZ( int2 t )
{
	return aoxZMip( 0, t );
}

float2 aoxSrc( int2 t )
{
	return src.Load( int3(clamp( t, (int2)( 0 ), fullSize - 1 ), 0) ).rg;
}

float3 aoxViewPos( float2 uv, float z )
{
	return float3( ( ndcToView.xy * uv + ndcToView.zw ) * z, z );
}

float3 aoxNormal( int2 t )
{
	t = clamp( t, (int2)( 0 ), fullSize - 1 );
	float3 n = gbuf.Load( int3(int2( t.x, fullSize.y - 1 - t.y ), 0) ).xyz;
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

#include "cell_aox_xe.hlsli"

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

void frag_main()
{
	int2 P = int2( gl_FragCoord.xy );
	if ( aoxStage == 1 ) {
		int2 t = min( P, fullSize - 1 );
		fragColor = float4( gbuf.Load( int3(int2( t.x, fullSize.y - 1 - t.y ), 0) ).a, 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 2 ) {
		int2 p = P * 2, s = mipSize[mipLevel - 1] - 1;
		float4 d = float4( src.Load( int3(min( p, s ), 0) ).r, src.Load( int3(min( p + int2( 1, 0 ), s ), 0) ).r,
		               src.Load( int3(min( p + int2( 0, 1 ), s ), 0) ).r, src.Load( int3(min( p + int2( 1, 1 ), s ), 0) ).r );
		// SSAO_DepthMIPFilter: the 2x2 mean weighted toward the far side, cut past the falloff
		float radius = 0.75 * max( aoxParams.x, 1.0e-4 );
		float range = clamp( aoxParams.y, 0.0, 1.0 ) * radius;
		float from = radius * ( 1.0 - clamp( aoxParams.y, 0.0, 1.0 ) );
		float mul = -1.0 / max( range, 1.0e-4 );
		float add = from / max( range, 1.0e-4 ) + 1.0;
		float maxDepth = max( max( d.x, d.y ), max( d.z, d.w ) );
		float4 w = clamp( ( maxDepth - d ) * mul + add, 0.0, 1.0 );
		fragColor = float4( dot( w, d ) / max( w.x + w.y + w.z + w.w, 1.0e-6 ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 3 ) {
		fragColor = float4( aoxUnorm8( aoxGtao( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 4 ) {
		fragColor = float4( aoxUnorm8( aoxHbao( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 5 ) {
		fragColor = float4( aoxUnorm8( aoxBilateral( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 6 ) {
		// the background (no normal): unoccluded (255 / 1.5), open to its sky neighbours so the denoise keeps it whole
		if ( aoxZ( P ) >= kAoxSky )
			fragColor = float4( 170.0, XeGTAO_PackEdges( (float4)( 1.0 ) ), 0.0, 1.0 );
		else
			fragColor = float4( XeGTAO_MainPass( P, aoxCounts.x, aoxCounts.y, aoxStaticNoise( P ), aoxNormal( P ) ), 0.0, 1.0 );
	} else {
		fragColor = float4( aoxUnorm8( XeGTAO_Denoise( P, aoxDenoise.z ) ), 0.0, 0.0, 1.0 );
	}
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
