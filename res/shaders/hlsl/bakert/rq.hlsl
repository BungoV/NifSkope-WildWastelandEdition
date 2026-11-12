// NifSkope entry of the bake tracer's rq kernel (lane VKCOMPUTE1, 2026-11-12): the shim + BakeRTRayQuery.hlsli.
// Gate: tests/hlslmatch rq (src/features/gi/bakert/rq.comp and this kernel on the same buffers, byte for byte).
// hlsl profile: cs_6_5
#define BAKERT_RQ
#include "BakeRTBindingsVk.hlsli"
#include "BakeRTRayQuery.hlsli"

[numthreads( 128, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	BakeRT_RayQueryCS( tid.x );   // bakert.cpp dispatches x only (at most 2^20 rays a submit)
}
