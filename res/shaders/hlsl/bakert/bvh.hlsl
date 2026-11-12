// NifSkope entry of the bake tracer's bvh kernel (lane VKCOMPUTE1, 2026-11-12): the shim + BakeRTBvh.hlsli.
// Gate: tests/hlslmatch bvh (src/features/gi/bakert/bvh.comp and this kernel on the same buffers, byte for byte).
// hlsl profile: cs_6_0
#define BAKERT_BVH
#include "BakeRTBindingsVk.hlsli"
#include "BakeRTBvh.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	BakeRT_BvhCS( tid.x );   // bakert.cpp dispatches x only (at most 2^20 rays a submit)
}
