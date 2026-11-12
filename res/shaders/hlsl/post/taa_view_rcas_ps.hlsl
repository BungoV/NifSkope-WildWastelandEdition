// hlsl profile: ps_6_0
// Lane VKSHARE1 (2026-11-12): the Modern temporal AA's RCAS copy-back on the present device (vkpresent.cpp taaRun): taa_rcas.hlsl
// as a fragment pass into the view's RGBA8 multisample colour, every sample the pixel's value (taa2_rcas.frag's draw into the
// frame). T0 = the resolve's picture; binding 7 = the same image (PfxTexel reads its size). Vertex: tone_view_vs.
#define PFX_RASTER 1
#define PFX_OUT0_FORMAT "rgba8"
#include "taa_rcas.hlsl"
#undef main

PfxPsOut main( float4 pos : SV_Position )
{
	return ps( pos );
}
