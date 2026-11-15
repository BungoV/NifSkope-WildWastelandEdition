// profile: cs_6_0
// lane VKFAR3 (2026-11-15, plan row A1 step b): the screen GI's volume read (ao/cell_gihalf_ps.hlsl, the ray-traced
// volume's branch) on Vulkan compute, one thread per work texel, 8x8 groups. The fragment file is included as it is:
// its frag_main runs with gl_FragCoord at the texel's centre.
// Inputs on DXC's default numbering (a binding = the register's number; -fvk-bind-register would need every declared
// resource): t46 giGbuf -> binding 46 (the g-buffer, RGBA32F), t44 cellDdgiAtlas -> binding 44 (the DDGI atlas, a texel
// buffer), b31 LooseUniforms -> binding 31 (GL's uniforms, kT_gihalf). LightBindingsVk.hlsli's out0 / out1 stay at 7 / 8.
// out0 giOut, out1 giKey (both RGBA32F, the work size). Compiled with WW_CELL_FOGLEAK, WW_CELL_RTDDGI,
// WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO; never WW_CELL_PRT.
#define LPX_OUT1_FORMAT "rgba32f"
#define LPX_OUT1_TYPE float4
#include "LightBindingsVk.hlsli"

#define main gihalf_ps_main
#include "../ao/cell_gihalf_ps.hlsl"
#undef main

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	gl_FragCoord = float4( float2( id.xy ) + 0.5f, 0.5f, 1.0f );
	frag_main();
	LpxOut0[id.xy] = giOut;
	LpxOut1[id.xy] = giKey;
}
