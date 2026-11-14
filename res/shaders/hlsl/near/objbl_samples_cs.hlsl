// Lane VKOBJ4 (2026-11-13): read every sample of the shared cell frame back, so a gate can compare GL's frame with
// Vulkan's sample by sample (WwVkPresent::objBlFrameSamples, vkobjpipe.inc).
//
// The frame is the app's: a multisampled RGBA16F colour target and a D24S8 (or D32FS8) depth-stencil target, both
// shared between GL and the present device. One thread per pixel walks the pixel's samples and writes one uint4 per
// sample, in the order (y * w + x) * samples + s:
//   x: the colour's red and green as half-float bits (red low, green high)
//   y: the colour's blue and alpha as half-float bits (blue low, alpha high)
//   z: the depth as float bits (a D24 reads back as k / 16777215; the gate rounds it to k)
//   w: the stencil
// A half read as float and turned back into a half is exact, so the colour comes back bit for bit.

// the frame's colour, its depth aspect, its stencil aspect
[[vk::binding( 0, 0 )]] Texture2DMS<float4> FrameColor;
[[vk::binding( 1, 0 )]] Texture2DMS<float> FrameDepth;
[[vk::binding( 2, 0 )]] Texture2DMS<uint> FrameStencil;

// the samples, written as described above
[[vk::binding( 3, 0 )]] RWStructuredBuffer<uint4> Samples;

struct ReadJob
{
	uint width;
	uint height;
	uint sampleCount;
	uint red;  // 1: the gate's red, every pixel's sample 1 reads sample 0 instead (a readback that drops a sample)
};
[[vk::push_constant]] ReadJob Job;

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	if ( id.x >= Job.width || id.y >= Job.height )
		return;
	const int2 pixel = int2( id.xy );
	const uint first = ( id.y * Job.width + id.x ) * Job.sampleCount;
	for ( uint s = 0; s < Job.sampleCount; s++ ) {
		const uint from = ( Job.red == 1 && s == 1 ) ? 0 : s;
		const float4 colour = FrameColor.Load( pixel, from );
		const float depth = FrameDepth.Load( pixel, from );
		const uint stencil = FrameStencil.Load( pixel, from ).r;
		uint4 o;
		o.x = f32tof16( colour.r ) | ( f32tof16( colour.g ) << 16 );
		o.y = f32tof16( colour.b ) | ( f32tof16( colour.a ) << 16 );
		o.z = asuint( depth );
		o.w = stencil;
		Samples[first + s] = o;
	}
}
