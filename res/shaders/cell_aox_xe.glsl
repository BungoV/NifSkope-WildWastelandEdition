///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2021, Intel Corporation
//
// SPDX-License-Identifier: MIT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// XeGTAO is based on GTAO/GTSO "Jimenez et al. / Practical Real-Time Strategies for Accurate Indirect Occlusion",
// https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
//
// Implementation:  Filip Strugar (filip.strugar@intel.com), Steve Mccalla <stephen.mccalla@intel.com>
// Details:         https://github.com/GameTechDev/XeGTAO
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// lane SSAO2: a GLSL port of XeGTAO_MainPass and XeGTAO_Denoise (finalApply) as the FO4 Community Shaders port
// vendors them (fp32, non-default constants, no bent normals); included by cell_aox.frag, which supplies
// aoxZ / aoxZMip / aoxViewPos / aoxNormal and the uniforms.

#define XE_GTAO_PI      3.1415926535897932384626433832795
#define XE_GTAO_PI_HALF 1.5707963267948966192313216916398
#define XE_GTAO_OCCLUSION_TERM_SCALE 1.5

vec4 XeGTAO_CalculateEdges( float centerZ, float leftZ, float rightZ, float topZ, float bottomZ )
{
	vec4 edgesLRTB = vec4( leftZ, rightZ, topZ, bottomZ ) - centerZ;
	float slopeLR = ( edgesLRTB.y - edgesLRTB.x ) * 0.5;
	float slopeTB = ( edgesLRTB.w - edgesLRTB.z ) * 0.5;
	vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4( slopeLR, -slopeLR, slopeTB, -slopeTB );
	edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
	return clamp( 1.25 - edgesLRTB / ( centerZ * 0.011 ), 0.0, 1.0 );
}

float XeGTAO_PackEdges( vec4 edgesLRTB )
{
	edgesLRTB = roundEven( clamp( edgesLRTB, 0.0, 1.0 ) * 2.9 );
	return dot( edgesLRTB, vec4( 64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0 ) );
}

vec4 XeGTAO_UnpackEdges( float packedValue )
{
	uint packedVal = uint( packedValue * 255.5 );
	vec4 edgesLRTB;
	edgesLRTB.x = float( ( packedVal >> 6 ) & 3u ) / 3.0;
	edgesLRTB.y = float( ( packedVal >> 4 ) & 3u ) / 3.0;
	edgesLRTB.z = float( ( packedVal >> 2 ) & 3u ) / 3.0;
	edgesLRTB.w = float( ( packedVal >> 0 ) & 3u ) / 3.0;
	return clamp( edgesLRTB, 0.0, 1.0 );
}

float XeGTAO_FastSqrt( float x )
{
	return intBitsToFloat( 0x1fbd1df5 + ( floatBitsToInt( x ) >> 1 ) );
}

float XeGTAO_FastACos( float inX )
{
	const float PI = 3.141593;
	const float HALF_PI = 1.570796;
	float x = abs( inX );
	float res = -0.156583 * x + HALF_PI;
	res *= XeGTAO_FastSqrt( 1.0 - x );
	return ( inX >= 0.0 ) ? res : PI - res;
}

// the point-clamp SampleLevel of the 5-mip working depth: the nearest level, the texel under the uv
float XeGTAO_SampleDepth( vec2 uv, float mipLevel )
{
	int m = clamp( int( floor( mipLevel + 0.5 ) ), 0, 4 );
	ivec2 t = ivec2( floor( uv * vec2( mipSize[m] ) ) );
	return aoxZMip( m, t );
}

// rg: the working term (an integer 0..255, the visibility / 1.5) and the packed edges
vec2 XeGTAO_MainPass( ivec2 pixCoord, float sliceCount, float stepsPerSlice, vec2 localNoise, vec3 viewspaceNormal )
{
	vec2 viewportPixelSize = 1.0 / vec2( fullSize );
	vec2 normalizedScreenPos = ( vec2( pixCoord ) + 0.5 ) * viewportPixelSize;
	float viewspaceZ = aoxZ( pixCoord );
	float pixLZ = aoxZ( pixCoord + ivec2( -1, 0 ) );
	float pixTZ = aoxZ( pixCoord + ivec2( 0, -1 ) );
	float pixRZ = aoxZ( pixCoord + ivec2( 1, 0 ) );
	float pixBZ = aoxZ( pixCoord + ivec2( 0, 1 ) );
	vec4 edgesLRTB = XeGTAO_CalculateEdges( viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ );
	float edges = XeGTAO_PackEdges( edgesLRTB );

	viewspaceZ *= 0.99999;	// XE_GTAO_FP32_DEPTHS
	vec3 pixCenterPos = aoxViewPos( normalizedScreenPos, viewspaceZ );
	vec3 viewVec = normalize( -pixCenterPos );

	float effectRadius = aoxParams.x;	// RadiusMultiplier 1: the radius is already in view units
	float sampleDistributionPower = 2.0;
	float falloffRange = aoxParams.y * effectRadius;
	float falloffFrom = effectRadius * ( 1.0 - aoxParams.y );
	float falloffMul = -1.0 / falloffRange;
	float falloffAdd = falloffFrom / falloffRange + 1.0;

	float visibility = 0.0;
	float noiseSlice = localNoise.x;
	float noiseSample = localNoise.y;
	const float pixelTooCloseThreshold = 1.3;
	vec2 pixelDirRBViewspaceSizeAtCenterZ = vec2( viewspaceZ ) * ( ndcToView.xy * viewportPixelSize );
	float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;
	visibility += clamp( ( 10.0 - screenspaceRadius ) / 100.0, 0.0, 1.0 ) * 0.5;
	float minS = pixelTooCloseThreshold / screenspaceRadius;
	for ( float slice = 0.0; slice < sliceCount; slice++ ) {
		float sliceK = ( slice + noiseSlice ) / sliceCount;
		float phi = sliceK * XE_GTAO_PI;
		float cosPhi = cos( phi );
		float sinPhi = sin( phi );
		vec2 omega = vec2( cosPhi, -sinPhi ) * screenspaceRadius;
		vec3 directionVec = vec3( cosPhi, sinPhi, 0.0 );
		vec3 orthoDirectionVec = directionVec - ( dot( directionVec, viewVec ) * viewVec );
		vec3 axisVec = normalize( cross( orthoDirectionVec, viewVec ) );
		vec3 projectedNormalVec = viewspaceNormal - axisVec * dot( viewspaceNormal, axisVec );
		float signNorm = sign( dot( orthoDirectionVec, projectedNormalVec ) );
		float projectedNormalVecLength = length( projectedNormalVec );
		float cosNorm = clamp( dot( projectedNormalVec, viewVec ) / projectedNormalVecLength, 0.0, 1.0 );
		float n = signNorm * XeGTAO_FastACos( cosNorm );
		float lowHorizonCos0 = cos( n + XE_GTAO_PI_HALF );
		float lowHorizonCos1 = cos( n - XE_GTAO_PI_HALF );
		float horizonCos0 = lowHorizonCos0;
		float horizonCos1 = lowHorizonCos1;
		for ( float step = 0.0; step < stepsPerSlice; step++ ) {
			float stepBaseNoise = ( slice + step * stepsPerSlice ) * 0.6180339887498948482;
			float stepNoise = fract( noiseSample + stepBaseNoise );
			float s = ( step + stepNoise ) / stepsPerSlice;
			s = pow( s, sampleDistributionPower );
			s += minS;
			vec2 sampleOffset = s * omega;
			float sampleOffsetLength = length( sampleOffset );
			float mipLevel = clamp( log2( sampleOffsetLength ) - 3.30, 0.0, 5.0 );
			sampleOffset = roundEven( sampleOffset ) * viewportPixelSize;

			vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
			float SZ0 = XeGTAO_SampleDepth( sampleScreenPos0, mipLevel );
			vec3 samplePos0 = aoxViewPos( sampleScreenPos0, SZ0 );
			vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
			float SZ1 = XeGTAO_SampleDepth( sampleScreenPos1, mipLevel );
			vec3 samplePos1 = aoxViewPos( sampleScreenPos1, SZ1 );

			vec3 sampleDelta0 = samplePos0 - pixCenterPos;
			vec3 sampleDelta1 = samplePos1 - pixCenterPos;
			float sampleDist0 = length( sampleDelta0 );
			float sampleDist1 = length( sampleDelta1 );
			vec3 sampleHorizonVec0 = sampleDelta0 / sampleDist0;
			vec3 sampleHorizonVec1 = sampleDelta1 / sampleDist1;
			// thin occluder compensation 0: the falloff base is the plain distance
			float weight0 = clamp( sampleDist0 * falloffMul + falloffAdd, 0.0, 1.0 );
			float weight1 = clamp( sampleDist1 * falloffMul + falloffAdd, 0.0, 1.0 );
			float shc0 = dot( sampleHorizonVec0, viewVec );
			float shc1 = dot( sampleHorizonVec1, viewVec );
			shc0 = mix( lowHorizonCos0, shc0, weight0 );
			shc1 = mix( lowHorizonCos1, shc1, weight1 );
			horizonCos0 = max( horizonCos0, shc0 );
			horizonCos1 = max( horizonCos1, shc1 );
		}
		projectedNormalVecLength = mix( projectedNormalVecLength, 1.0, 0.05 );
		float h0 = -XeGTAO_FastACos( horizonCos1 );
		float h1 = XeGTAO_FastACos( horizonCos0 );
		float iarc0 = ( cosNorm + 2.0 * h0 * sin( n ) - cos( 2.0 * h0 - n ) ) / 4.0;
		float iarc1 = ( cosNorm + 2.0 * h1 * sin( n ) - cos( 2.0 * h1 - n ) ) / 4.0;
		visibility += projectedNormalVecLength * ( iarc0 + iarc1 );
	}
	visibility /= sliceCount;
	visibility = pow( visibility, aoxParams.w );	// FinalValuePower
	visibility = max( 0.03, visibility );
	visibility = clamp( visibility / XE_GTAO_OCCLUSION_TERM_SCALE, 0.0, 1.0 );
	return vec2( float( uint( visibility * 255.0 + 0.5 ) ), edges );
}

// one pixel of XeGTAO_Denoise (finalApply): the edge-aware 3x3 over the working term (src.r) and edges (src.g)
float XeGTAO_Denoise( ivec2 pixCoord, float blurBeta )
{
	const float diagWeight = 0.85 * 0.5;
	vec2 cC = aoxSrc( pixCoord );
	vec2 cL = aoxSrc( pixCoord + ivec2( -1, 0 ) );
	vec2 cR = aoxSrc( pixCoord + ivec2( 1, 0 ) );
	vec2 cT = aoxSrc( pixCoord + ivec2( 0, -1 ) );
	vec2 cB = aoxSrc( pixCoord + ivec2( 0, 1 ) );
	vec4 edgesL_LRTB = XeGTAO_UnpackEdges( cL.g );
	vec4 edgesT_LRTB = XeGTAO_UnpackEdges( cT.g );
	vec4 edgesR_LRTB = XeGTAO_UnpackEdges( cR.g );
	vec4 edgesB_LRTB = XeGTAO_UnpackEdges( cB.g );
	vec4 edgesC_LRTB = XeGTAO_UnpackEdges( cC.g );
	edgesC_LRTB *= vec4( edgesL_LRTB.y, edgesR_LRTB.x, edgesT_LRTB.w, edgesB_LRTB.z );
	const float leak_threshold = 2.5;
	const float leak_strength = 0.5;
	float edginess = ( clamp( 4.0 - leak_threshold - dot( edgesC_LRTB, vec4( 1.0 ) ), 0.0, 1.0 ) / ( 4.0 - leak_threshold ) ) * leak_strength;
	edgesC_LRTB = clamp( edgesC_LRTB + edginess, 0.0, 1.0 );
	float weightTL = diagWeight * ( edgesC_LRTB.x * edgesL_LRTB.z + edgesC_LRTB.z * edgesT_LRTB.x );
	float weightTR = diagWeight * ( edgesC_LRTB.z * edgesT_LRTB.y + edgesC_LRTB.y * edgesR_LRTB.z );
	float weightBL = diagWeight * ( edgesC_LRTB.w * edgesB_LRTB.x + edgesC_LRTB.x * edgesL_LRTB.w );
	float weightBR = diagWeight * ( edgesC_LRTB.y * edgesR_LRTB.w + edgesC_LRTB.w * edgesB_LRTB.y );
	float sumWeight = blurBeta;
	float sum = cC.r / 255.0 * sumWeight;
	sum += cL.r / 255.0 * edgesC_LRTB.x;	sumWeight += edgesC_LRTB.x;
	sum += cR.r / 255.0 * edgesC_LRTB.y;	sumWeight += edgesC_LRTB.y;
	sum += cT.r / 255.0 * edgesC_LRTB.z;	sumWeight += edgesC_LRTB.z;
	sum += cB.r / 255.0 * edgesC_LRTB.w;	sumWeight += edgesC_LRTB.w;
	sum += aoxSrc( pixCoord + ivec2( -1, -1 ) ).r / 255.0 * weightTL;	sumWeight += weightTL;
	sum += aoxSrc( pixCoord + ivec2( 1, -1 ) ).r / 255.0 * weightTR;	sumWeight += weightTR;
	sum += aoxSrc( pixCoord + ivec2( -1, 1 ) ).r / 255.0 * weightBL;	sumWeight += weightBL;
	sum += aoxSrc( pixCoord + ivec2( 1, 1 ) ).r / 255.0 * weightBR;	sumWeight += weightBR;
	float outputValue = sum / sumWeight * XE_GTAO_OCCLUSION_TERM_SCALE;
	// the R32_UINT store, then the resolve's saturate( packed / 255 )
	return clamp( float( uint( outputValue * 255.0 + 0.5 ) ) / 255.0, 0.0, 1.0 );
}
