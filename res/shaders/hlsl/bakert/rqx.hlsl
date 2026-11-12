// NifSkope entry of the bake tracer's rqx kernel (lane VKCOMPUTE1, 2026-11-12): the shim + BakeRTRayQueryX.hlsli.
// Gate: tests/hlslmatch rqx (src/features/gi/bakert/rqx.comp and this kernel on the same buffers, byte for byte).
// hlsl profile: cs_6_5
#define BAKERT_RQX
#include "BakeRTBindingsVk.hlsli"
#include "BakeRTRayQueryX.hlsli"

[numthreads( 128, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	BakeRT_RayQueryXCS( tid.x );   // bakert.cpp dispatches x only (at most 2^20 rays a submit)
}
