// Lane VKFRAME1 round 8: a multisampled input for postfxmatch.cpp. T0 holds the samples side by side (W x S wide); drawn
// per sample, sample s of pixel (x, y) takes column x * S + s, an exact copy. i.x = S. Entry ps (the vertex stage is
// pfx_vs.spv); built by tests/vkframe/post2.sh.
#include "../../res/shaders/hlsl/post/PostFxBindingsVk.hlsli"

float4 ps( float4 pos : SV_Position, uint s : SV_SampleIndex ) : SV_Target0
{
	return PfxT0.Load( int3( int( pos.x ) * PfxJobPc.i.x + int( s ), int( pos.y ), 0 ) );
}
