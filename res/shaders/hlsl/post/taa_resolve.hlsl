// Lane VKFRAME1 P5 (2026-11-12): the Modern temporal AA resolve on Vulkan (taa2.frag's twin, one thread a pixel). Its three
// outputs go to RGBA16F storage images: the next history, the picture, the lock. TAA_RESOLVE_RED drops the luma weights
// (the gate's red must FAIL).
#include "TaaBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint w, h;
	TaaCur.GetDimensions( w, h );
	if ( any( id.xy >= uint2( w, h ) ) )
		return;
	const F4FX_TaaParams P = TaaParams( TaaJobPc );
#ifdef TAA_RESOLVE_RED
	const bool red = true;
#else
	const bool red = false;
#endif
	float4 o0, o1, o2;
	F4FX_TaaResolve( int2( id.xy ), int2( w, h ), P, TaaCur, TaaHist, TaaMv, TaaDepth, TaaLock, TaaVel, TaaMask, TaaLinearClamp, red,
		o0, o1, o2 );
	// raw stores: GL's RGBA16F attachment writes truncate here as the storage store does (an explicit round-to-nearest
	// put half of all halves 1 ulp off GL)
	TaaOutHist[id.xy] = o0;
	TaaOutPic[id.xy] = o1;
	TaaOutLock[id.xy] = o2;
}
