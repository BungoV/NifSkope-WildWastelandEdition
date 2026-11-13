// HLSL source of bonetransform.glsl, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
#pragma once
#include "GlslCompat.hlsli"
cbuffer skinningUniforms : register(b0, space0)
{
	row_major float3x4	boneTransforms[256] : packoffset(c0);		// bone transforms in row-major order
};

static float4	boneWeights[2];

#ifdef BT_POSITION_ONLY
void boneTransform( inout float4 v )
#elif defined( BT_NO_TANGENTS )
void boneTransform( inout float4 v, inout float3 n )
#else
void boneTransform( inout float4 v, inout float3 n, inout float3 t, inout float3 b )
#endif
{
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
	float3	vTmp = (float3)(0.0);
#ifndef BT_POSITION_ONLY
	float3	nTmp = (float3)(0.0);
#endif
#else
	float3x4	mTmp = (float3x4)0;
#endif
	float	wSum = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		float	bw = boneWeights[i >> 2][i & 3];
		if ( !( bw > 0.0 ) )
			break;
		int	bone = (int)(bw) & 0xFF;
		float	w = frac( bw );
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
		float3x4	m = boneTransforms[bone];
		vTmp += mul(m, v) * w;
#ifndef BT_POSITION_ONLY
		nTmp += mul((float3x3)(m), n) * w;
#endif
#else
		mTmp += boneTransforms[bone] * w;
#endif
		wSum += w;
	}
	if ( wSum > 0.0 ) {
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
		v = float4( vTmp / wSum, 1.0 );
#ifndef BT_POSITION_ONLY
		n = nTmp;
#endif
#else
		v = float4( mul(mTmp, v) / wSum, 1.0 );
		float3x3	r = (float3x3)(mTmp);
		n = mul(r, n);
		t = mul(r, t);
		b = mul(r, b);
#endif
	}
}
