// NifSkope entry of the DDGI probe trace (lane VKCOMPUTE1, 2026-11-12): the shim + DDGITrace.hlsli.
// Gate: tests/hlslmatch trace (src/features/gi/rtddgi/trace.comp and this kernel on the same buffers + TLAS, byte for byte).
// hlsl profile: cs_6_5
#define DDGI_USE_TLAS
#include "DDGIBindingsVk.hlsli"
#include "DDGITrace.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	DDGI_TraceCS( DDGI_LinearId( tid ) );
}
