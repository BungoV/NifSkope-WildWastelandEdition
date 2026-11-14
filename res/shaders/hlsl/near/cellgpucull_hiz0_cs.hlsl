// HLSL source of features/near/cellgpucull_hiz0.glsl (compute kernel), lane HLSLSRC3, written by hand from the GLSL (the
// translator has no compute stage). GL compiles the .glsl (src/features/near/cellgpucull.cpp); this is its HLSL twin, byte
// for byte on the GPU by tests/hlslmatch/cullmatch.sh. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if MS (cellgpucull.cpp compile(): plain and "#define MS 1")
// profile: cs_6_0
#include "cellgpucull_bind.hlsli"

WW_CULL_BIND( 0 ) RWStructuredBuffer<float> hiz : register( u0 );	// HizBuf, written only
#ifdef MS
WW_CULL_BIND( 1 ) Texture2DMS<float4> depthTex : register( t1 );
#else
WW_CULL_BIND( 1 ) Texture2D<float4> depthTex : register( t1 );
#endif
WW_CULL_BIND( 31 ) cbuffer LooseUniforms : register( b31 )
{
	int samples;
	int stdPad0;	// std140: dim at 8
	int2 dim;
};
[numthreads( 8, 8, 1 )]
void main( uint3 gl_GlobalInvocationID : SV_DispatchThreadID )
{
	int2 p = int2( gl_GlobalInvocationID.xy );
	if ( p.x >= dim.x || p.y >= dim.y )
		return;
#ifdef MS
	float m = 0.0;
	for ( int s = 0; s < samples; s++ )
		m = max( m, depthTex.Load( p, s ).r );
#else
	float m = depthTex.Load( int3( p, 0 ) ).r;
#endif
#ifndef CELLGPUCULL_HIZ0_RED	// the gate's red: the level is never written
	hiz[p.y * dim.x + p.x] = m;
#endif
}
