// NifSkope entry of ReSTIR DI (lane VKCOMPUTE1, 2026-11-12): both shims + DDGIRestir.hlsli.
// Gate: tests/hlslmatch restir (src/features/gi/rtddgi/restir.comp and this kernel on the same buffers + TLAS, byte for byte).
// hlsl profile: cs_6_5
#define DDGI_USE_TLAS
#include "DDGIBindingsVk.hlsli"
#include "DDGIRestirBindingsVk.hlsli"
#include "DDGIRestir.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	Restir_CS( DDGI_LinearId( tid ) );
}
