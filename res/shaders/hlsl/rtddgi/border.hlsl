// NifSkope entry of the DDGI border copy (lane VKCOMPUTE1, 2026-11-12): the shim + the shared body (DDGIBorder.hlsli).
// Gate: tests/hlslmatch border (src/features/gi/rtddgi/border.comp and this kernel on the same buffers, byte for byte).
#include "DDGIBindingsVk.hlsli"
#include "DDGIBorder.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	DDGI_BorderCS( DDGI_LinearId( tid ) );
}
