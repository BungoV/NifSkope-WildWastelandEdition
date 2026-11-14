// Lane VKFAR2 P7 (2026-11-14): the post passes' sources on Vulkan, cellpost.cpp's two blits of the view (the tone-mapped
// RGBA8 samples into cur, RGBA16F; the D24S8 samples into the depth). T0 the view's copy (multisampled).
// Colour: the mean of every sample rounded to half (GL's MS blit into RGBA16F, bloom_back.hlsl's way); i.x = 1 (the red)
// takes sample 0. POST7_DEPTH: the depth of sample i.y into r32f (GL's MS depth blit keeps one sample).
#define PFX_T0_MS
#ifdef POST7_DEPTH
#define PFX_OUT0_FORMAT "r32f"
#define PFX_OUT0_TYPE float
#endif
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const int2 xy = int2( id.xy );
#ifdef POST7_DEPTH
	PFX_STORE0( id.xy, PfxT0.Load( xy, j.i.y ).r );
#else
	uint fw, fh, samples;
	PfxT0.GetDimensions( fw, fh, samples );
	samples = j.i.x == 1 ? 1u : max( samples, 1u );
	float4 c = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	for ( uint s = 0; s < samples; s++ )
		c += PfxT0.Load( xy, s );
	c /= float( samples );
	PFX_STORE0( id.xy, f16tof32( f32tof16( c ) ) );
#endif
}
