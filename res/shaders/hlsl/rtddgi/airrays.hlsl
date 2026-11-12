// NifSkope entry of the air rays (lane VKCOMPUTE1, 2026-11-12): both shims + DDGIAirRays.hlsli.
// Gate: tests/hlslmatch airrays (src/features/gi/rtddgi/airrays.comp and this kernel on the same buffers + TLAS, byte for byte).
// hlsl profile: cs_6_5
#define DDGI_USE_TLAS
#include "DDGIBindingsVk.hlsli"
#include "DDGIAirRaysBindingsVk.hlsli"
#include "DDGIAirRays.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	Air_RaysCS( DDGI_LinearId( tid ) );
}
