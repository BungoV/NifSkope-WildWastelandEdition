// Lane VKFRAME1 P7 (2026-11-12): per-object motion blur pass 1 on Vulkan, cell_objblur_vec.frag's twin (ObjBlurCommon.hlsli).
// T0 the velocity, T1 the inputs mask (.b = an object vector), T2 the depth; c[0..3] = reproj's GL columns,
// c[4] = (jitter ndc xy, 1 / size xy), c[5] = (shutter, max px, -, -), i.x = the app's red. Target RG16F.
// PFX_RED keeps the camera (the whole velocity).
#define PFX_OUT0_FORMAT "rg16f"
#define PFX_OUT0_TYPE float2
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const int3 p = int3( id.xy, 0 );
	bool red = j.i.x != 0;
#ifdef PFX_RED
	red = true;
#endif
	PFX_STORE0( id.xy, F4FX_ObjBlurVector( float2( id.xy ) + 0.5f, PfxT0.Load( p ).xy, PfxT1.Load( p ).b, PfxT2.Load( p ).r, j.c[0],
		j.c[1], j.c[2], j.c[3], j.c[4], j.c[5].x, j.c[5].y, red ) );
}
