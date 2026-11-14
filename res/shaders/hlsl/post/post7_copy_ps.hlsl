// Lane VKFAR2 P7 (2026-11-14): the post passes' picture back into the view, game_taa_copy.frag's twin as a fragment pass in
// the tone pass (every sample of a pixel gets the texel; the RGBA8 target rounds it as GL's does). T0 the picture.
#include "PostFxBindingsVk.hlsli"

float4 main( float4 pos : SV_Position ) : SV_Target0
{
	return PfxT0.Load( int3( int2( pos.xy ), 0 ) );
}
