// hlsl profile: cs_6_0
// Lane VKFAR2 (2026-11-14, the froxel item): the air's integrate on the present device (vkfog.inc fogIntegRun, WW_VKVIEW).
// cell_volair.frag stages 2 and 3 as one compute pass: each thread walks one froxel column front to back and writes the
// running in-scatter L and transmittance T after every layer, the volume lookdev_fog.glsl reads (volTex, wwVolAirAt).
//
//   L = 0, T = 1;  for each layer j:  s = inject[col, base + j];  L += T * s.rgb;  T *= s.a;  integ[col, base + j] = (L, T)
//
// GL's stage 3 does the same sum 7 layers a draw with the running (L, T) carried in an RGBA32F target between draws; one
// thread a column keeps it in registers, so the numbers are the same float32 sum in the same order. The inject volume is
// RGBA32F (the slices' transmittance needs float32); the integrated one RGBA16F (VOLAIR_INTEG_F32: RGBA32F, the f32 red).
// Both are the air's shared 3D targets (cellvolair.cpp), so nothing is copied in or out.

// t0: the inject volume (rgb = the slice's in-scatter, a = the slice's transmittance), read by integer texel
[[vk::binding( 0, 0 )]] Texture3D<float4> volInject;
// u0: the integrated volume, written at the same texels
#ifdef VOLAIR_INTEG_F32
[[vk::binding( 1, 0 )]] [[vk::image_format( "rgba32f" )]] RWTexture3D<float4> volInteg;
#else
[[vk::binding( 1, 0 )]] [[vk::image_format( "rgba16f" )]] RWTexture3D<float4> volInteg;
#endif

struct FogJob
{
	int4 grid;	// columns, rows, the grid's first layer in the packed volume (the far grid sits after the near one), layers
	int4 opt;	// x: the red bits (gates only, 0 in the app): 1 no transmittance, 2 T before L, 4 read one layer late,
				// 8 layer 1's light dropped
};
[[vk::push_constant]] FogJob job;

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	const int2 col = int2( id.xy );
	if ( col.x >= job.grid.x || col.y >= job.grid.y )
		return;
	const int red = job.opt.x;
	const int base = job.grid.z;
	const int read = base + ( ( red & 4 ) ? 1 : 0 );
	float3 L = float3( 0.0, 0.0, 0.0 );
	float T = 1.0;
	for ( int j = 0; j < job.grid.w; j++ ) {
		const float4 s = volInject.Load( int4( col, read + j, 0 ) );
		const float a = ( red & 1 ) ? 1.0 : s.a;
		const float3 c = ( ( red & 8 ) && j == 1 ) ? float3( 0.0, 0.0, 0.0 ) : s.rgb;
		if ( red & 2 ) {
			T *= a;
			L += T * c;
		} else {
			L += T * c;	// what reaches the eye from this slice: its in-scatter through the slices in front
			T *= a;		// then this slice's own extinction
		}
		volInteg[int3( col, base + j )] = float4( L, T );
	}
}
