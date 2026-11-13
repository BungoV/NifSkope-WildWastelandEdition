// HLSL source of features/near/cellgpucull_hizn.glsl (compute kernel), lane HLSLSRC3, written by hand from the GLSL (the
// translator has no compute stage). GL compiles the .glsl (src/features/near/cellgpucull.cpp); this is its HLSL twin, byte
// for byte on the GPU by tests/hlslmatch/cullmatch.sh. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (cellgpucull.cpp compile() passes no defines)
// profile: cs_6_0
#include "cellgpucull_bind.hlsli"

WW_CULL_BIND( 0 ) RWStructuredBuffer<float> hiz : register( u0 );	// HizBuf
WW_CULL_BIND( 31 ) cbuffer LooseUniforms : register( b31 )
{
	int srcOff;
	int dstOff;
	int2 srcDim;
	int2 dstDim;
};
[numthreads( 8, 8, 1 )]
void main( uint3 gl_GlobalInvocationID : SV_DispatchThreadID )
{
	int2 p = int2( gl_GlobalInvocationID.xy );
	if ( p.x >= dstDim.x || p.y >= dstDim.y )
		return;
	int2 a = p * 2;
	int2 b = min( a + 1, srcDim - 1 );
	float m = max( max( hiz[srcOff + a.y * srcDim.x + a.x], hiz[srcOff + a.y * srcDim.x + b.x] ),
		max( hiz[srcOff + b.y * srcDim.x + a.x], hiz[srcOff + b.y * srcDim.x + b.x] ) );
#ifndef CELLGPUCULL_HIZN_RED	// the gate's red: the level is never written
	hiz[dstOff + p.y * dstDim.x + p.x] = m;
#endif
}
