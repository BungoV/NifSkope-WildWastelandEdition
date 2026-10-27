#version 410 core

/* lane SSAO2: the FO4 Community Shaders' three ambient occlusion methods for the cell view (src/gl/celllights.cpp),
 * at full resolution, in its view space (x right, y up, z forward) with every target top row first. gbuf is lane
 * AO1's opaque pass (bottom row first: GL view normal, linear depth; the background at 1e6).
 *   stage 1  the working depth mip 0 (the linear depth, top row first)
 *   stage 2  working depth mip 1..4: SSAO_DepthMIPFilter over 2x2 of the level above
 *   stage 3  GTAO (GTAOMainCS)        stage 4  HBAO+ (HBAOMainCS)        stage 5  the bilateral denoise (DenoiseCS)
 *   stage 6  XeGTAO main (term + edges)   stage 7  XeGTAO denoise (final apply) + resolve */

uniform int aoxStage;
uniform sampler2D gbuf;
uniform sampler2D xz0;
uniform sampler2D xz1;
uniform sampler2D xz2;
uniform sampler2D xz3;
uniform sampler2D xz4;
uniform sampler2D src;
uniform ivec2 fullSize;
uniform ivec2 mipSize[5];	// [0] = fullSize
uniform int mipLevel;
uniform vec4 ndcToView;		// xy NDCToViewMul, zw NDCToViewAdd (uv in [0, 1], top row first)
uniform vec4 aoxParams;		// effect radius (game units), falloff range, intensity, power
uniform vec2 aoxCounts;		// slices, steps per slice
uniform vec3 aoxDenoise;	// radius, depth sigma, XeGTAO blur beta

out vec4 fragColor;

const float kAoxSky = 1.0e6 * 0.999;	// the far plane test, on the opaque pass's background

float aoxZMip( int m, ivec2 t )
{
	t = clamp( t, ivec2( 0 ), mipSize[m] - 1 );
	if ( m == 0 )
		return texelFetch( xz0, t, 0 ).r;
	if ( m == 1 )
		return texelFetch( xz1, t, 0 ).r;
	if ( m == 2 )
		return texelFetch( xz2, t, 0 ).r;
	if ( m == 3 )
		return texelFetch( xz3, t, 0 ).r;
	return texelFetch( xz4, t, 0 ).r;
}

float aoxZ( ivec2 t )
{
	return aoxZMip( 0, t );
}

vec2 aoxSrc( ivec2 t )
{
	return texelFetch( src, clamp( t, ivec2( 0 ), fullSize - 1 ), 0 ).rg;
}

vec3 aoxViewPos( vec2 uv, float z )
{
	return vec3( ( ndcToView.xy * uv + ndcToView.zw ) * z, z );
}

vec3 aoxNormal( ivec2 t )
{
	t = clamp( t, ivec2( 0 ), fullSize - 1 );
	vec3 n = texelFetch( gbuf, ivec2( t.x, fullSize.y - 1 - t.y ), 0 ).xyz;
	return normalize( vec3( n.x, n.y, -n.z ) );
}

// SSAO_StaticCleanNoise: the 4x4 tile the static denoise kernel reassembles
vec2 aoxStaticNoise( ivec2 p )
{
	uint x = uint( p.x ) & 3u, y = uint( p.y ) & 3u, xy = x ^ y;
	uint index = ( ( ( xy >> 1 ) & 1u ) << 3 ) | ( ( ( y >> 1 ) & 1u ) << 2 ) | ( ( xy & 1u ) << 1 ) | ( y & 1u );
	uint stepIndex = ( ( index & 1u ) << 3 ) | ( ( index & 2u ) << 1 ) | ( ( index & 4u ) >> 1 ) | ( ( index & 8u ) >> 3 );
	return ( vec2( float( index ), float( stepIndex ) ) + 0.5 ) * ( 1.0 / 16.0 );
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

#include "cell_aox_xe.glsl"

float aoxGtao( ivec2 P )
{
	ivec2 pmax = fullSize - 1;
	float centerZ = aoxZ( P );
	if ( centerZ >= kAoxSky )
		return 1.0;
	vec2 pix = 1.0 / vec2( fullSize );
	vec3 centerView = aoxViewPos( ( vec2( P ) + 0.5 ) * pix, centerZ );
	vec3 viewVec = normalize( -centerView );
	vec3 normalView = aoxNormal( P );
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
	vec2 noise = aoxStaticNoise( P );
	for ( float slice = 0.0; slice < sliceCount; slice++ ) {
		float phi = ( slice + noise.x ) / sliceCount * 3.14159265359;
		float cosPhi = cos( phi ), sinPhi = sin( phi );
		vec2 omega = vec2( cosPhi, -sinPhi );
		vec3 directionVec = vec3( cosPhi, sinPhi, 0.0 );
		vec3 orthoDirectionVec = directionVec - dot( directionVec, viewVec ) * viewVec;
		vec3 axisVec = normalize( cross( orthoDirectionVec, viewVec ) );
		vec3 projectedNormalVec = normalView - axisVec * dot( normalView, axisVec );
		float projectedNormalVecLength = length( projectedNormalVec );
		float cosNorm = clamp( dot( projectedNormalVec, viewVec ) / max( projectedNormalVecLength, 1.0e-5 ), 0.0, 1.0 );
		float n = sign( dot( orthoDirectionVec, projectedNormalVec ) ) * acos( cosNorm );
		float lowHorizonCos0 = cos( n + 1.57079632679 );
		float lowHorizonCos1 = cos( n - 1.57079632679 );
		float horizonCos0 = lowHorizonCos0, horizonCos1 = lowHorizonCos1;
		for ( float step = 0.0; step < stepCount; step++ ) {
			float s = ( step + noise.y ) / stepCount;
			s = s * s + minS;
			vec2 offsetPixels = s * omega * screenspaceRadius;
			uint tapMip = aoxSelectMip( length( offsetPixels ) );
			ivec2 sp0 = clamp( ivec2( roundEven( vec2( P ) + offsetPixels ) ), ivec2( 0 ), pmax );
			float z0 = aoxZMip( int( tapMip ), sp0 >> int( tapMip ) );
			vec3 delta0 = aoxViewPos( ( vec2( sp0 ) + 0.5 ) * pix, z0 ) - centerView;
			float dist0 = length( delta0 );
			float shc0 = dot( delta0 / max( dist0, 1.0e-5 ), viewVec );
			float w0 = clamp( dist0 * falloffMul + falloffAdd, 0.0, 1.0 );
			horizonCos0 = max( horizonCos0, mix( lowHorizonCos0, shc0, w0 ) );
			ivec2 sp1 = clamp( ivec2( roundEven( vec2( P ) - offsetPixels ) ), ivec2( 0 ), pmax );
			float z1 = aoxZMip( int( tapMip ), sp1 >> int( tapMip ) );
			vec3 delta1 = aoxViewPos( ( vec2( sp1 ) + 0.5 ) * pix, z1 ) - centerView;
			float dist1 = length( delta1 );
			float shc1 = dot( delta1 / max( dist1, 1.0e-5 ), viewVec );
			float w1 = clamp( dist1 * falloffMul + falloffAdd, 0.0, 1.0 );
			horizonCos1 = max( horizonCos1, mix( lowHorizonCos1, shc1, w1 ) );
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

float aoxHbao( ivec2 P )
{
	ivec2 pmax = fullSize - 1;
	float centerZ = aoxZ( P );
	if ( centerZ >= kAoxSky )
		return 1.0;
	vec2 pix = 1.0 / vec2( fullSize );
	vec3 centerView = aoxViewPos( ( vec2( P ) + 0.5 ) * pix, centerZ );
	vec3 normalView = aoxNormal( P );
	float effectRadius = max( aoxParams.x, 0.01 );
	float tangentBiasSin = sin( clamp( aoxParams.y, 0.0, 1.0 ) * ( 3.14159265359 / 12.0 ) );
	float dirCount = aoxCounts.x, stepCount = aoxCounts.y;
	float viewPerWorkPixel = max( ndcToView.x * centerZ * pix.x, 1.0e-5 );
	float screenspaceRadius = clamp( effectRadius / viewPerWorkPixel, 2.0, 96.0 );
	float falloffRadiusView = min( effectRadius, screenspaceRadius * viewPerWorkPixel );
	float smallRadiusFade = clamp( ( 10.0 - screenspaceRadius ) / 100.0, 0.0, 1.0 ) * 0.5;
	// SSAO_InterleavedGradientNoise, frame index 0
	float noise = fract( 52.9829189 * fract( dot( vec2( P ), vec2( 0.06711056, 0.00583715 ) ) ) );
	float occlusion = 0.0;
	for ( float d = 0.0; d < dirCount; d++ ) {
		float phi = ( d + noise ) / dirCount * ( 2.0 * 3.14159265359 );
		vec2 dir = vec2( cos( phi ), -sin( phi ) );
		float horizonExcess = 0.0;
		float stepNoise = fract( noise + d * 0.6180339887 );
		for ( float s = 1.0; s <= stepCount; s++ ) {
			float t = ( s - stepNoise ) / stepCount;
			vec2 offset = dir * ( t * screenspaceRadius );
			ivec2 sp = clamp( ivec2( roundEven( vec2( P ) + offset ) ), ivec2( 0 ), pmax );
			uint m = aoxSelectMip( length( offset ) );
			float sz = aoxZMip( int( m ), sp >> int( m ) );
			vec3 delta = aoxViewPos( ( vec2( sp ) + 0.5 ) * pix, sz ) - centerView;
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
float aoxBilateral( ivec2 P )
{
	int radius = clamp( int( aoxDenoise.x ), 1, 4 );
	float centerDepth = aoxZ( P );
	float invDepthSigma = 1.0 / max( aoxDenoise.y * centerDepth, 1.0e-3 );
	float weightedAO = 0.0, weightSum = 0.0;
	for ( int y = -radius; y <= radius; ++y ) {
		for ( int x = -radius; x <= radius; ++x ) {
			ivec2 q = P + ivec2( x, y );
			float dz = abs( aoxZ( q ) - centerDepth ) * invDepthSigma;
			float spatial = ( abs( x ) == radius ? 0.5 : 1.0 ) * ( abs( y ) == radius ? 0.5 : 1.0 );
			float weight = spatial * exp( -dz * dz );
			weightedAO += aoxSrc( q ).r * weight;
			weightSum += weight;
		}
	}
	return weightedAO / max( weightSum, 1.0e-4 );
}

void main()
{
	ivec2 P = ivec2( gl_FragCoord.xy );
	if ( aoxStage == 1 ) {
		ivec2 t = min( P, fullSize - 1 );
		fragColor = vec4( texelFetch( gbuf, ivec2( t.x, fullSize.y - 1 - t.y ), 0 ).a, 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 2 ) {
		ivec2 p = P * 2, s = mipSize[mipLevel - 1] - 1;
		vec4 d = vec4( texelFetch( src, min( p, s ), 0 ).r, texelFetch( src, min( p + ivec2( 1, 0 ), s ), 0 ).r,
		               texelFetch( src, min( p + ivec2( 0, 1 ), s ), 0 ).r, texelFetch( src, min( p + ivec2( 1, 1 ), s ), 0 ).r );
		// SSAO_DepthMIPFilter: the 2x2 mean weighted toward the far side, cut past the falloff
		float radius = 0.75 * max( aoxParams.x, 1.0e-4 );
		float range = clamp( aoxParams.y, 0.0, 1.0 ) * radius;
		float from = radius * ( 1.0 - clamp( aoxParams.y, 0.0, 1.0 ) );
		float mul = -1.0 / max( range, 1.0e-4 );
		float add = from / max( range, 1.0e-4 ) + 1.0;
		float maxDepth = max( max( d.x, d.y ), max( d.z, d.w ) );
		vec4 w = clamp( ( maxDepth - d ) * mul + add, 0.0, 1.0 );
		fragColor = vec4( dot( w, d ) / max( w.x + w.y + w.z + w.w, 1.0e-6 ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 3 ) {
		fragColor = vec4( aoxUnorm8( aoxGtao( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 4 ) {
		fragColor = vec4( aoxUnorm8( aoxHbao( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 5 ) {
		fragColor = vec4( aoxUnorm8( aoxBilateral( P ) ), 0.0, 0.0, 1.0 );
	} else if ( aoxStage == 6 ) {
		// the background (no normal): unoccluded (255 / 1.5), open to its sky neighbours so the denoise keeps it whole
		if ( aoxZ( P ) >= kAoxSky )
			fragColor = vec4( 170.0, XeGTAO_PackEdges( vec4( 1.0 ) ), 0.0, 1.0 );
		else
			fragColor = vec4( XeGTAO_MainPass( P, aoxCounts.x, aoxCounts.y, aoxStaticNoise( P ), aoxNormal( P ) ), 0.0, 1.0 );
	} else {
		fragColor = vec4( aoxUnorm8( XeGTAO_Denoise( P, aoxDenoise.z ) ), 0.0, 0.0, 1.0 );
	}
}
