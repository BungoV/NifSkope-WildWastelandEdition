// Lane VKFAR2 P9 (2026-11-14): lens_flare.hlsl's vertex entry (vs) under the entry name the generator compiles (main), for
// the sprites drawn in the tone pass (src/features/vkframe/vkflare.inc). The shader is lens_flare.hlsl; this file only renames.
// profile: vs_6_0
#define vs LensFlareVs
#define main LensFlarePs
#include "lens_flare.hlsl"
#undef main
#undef vs

FlareV main( uint v : SV_VertexID )
{
	return LensFlareVs( v );
}
