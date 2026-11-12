// NifSkope entry of the DDGI probe blend (lane VKCOMPUTE1, 2026-11-12): the shim + DDGIBlend.hlsli.
// Gate: tests/hlslmatch blend (src/features/gi/rtddgi/blend.comp and this kernel on the same buffers, byte for byte).
#include "DDGIBindingsVk.hlsli"
#include "DDGIBlend.hlsli"

[numthreads( 256, 1, 1 )]
void main( uint3 gid : SV_GroupID, uint3 lid : SV_GroupThreadID )
{
	DDGI_BlendCS( DDGI_GroupId( gid ), lid.x );
}
