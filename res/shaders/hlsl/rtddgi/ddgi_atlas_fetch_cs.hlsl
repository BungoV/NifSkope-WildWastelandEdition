// Lane VKFAR3 (row A1, the DDGI atlas on the present device): every texel of the probe atlas fetched through the
// present device's texel view (Buffer<float4> = cell_ddgi.glsl's samplerBuffer) into a storage buffer the host reads.
// vkddgi.inc ddgiAtlasRead runs it for the gate (tests/vkframe/ddgiview.sh): the view, not only the memory, is proven.
// Built by src/features/vkframe/vkframe_spirv.sh (fxs: t at 64, u at 192), so t0 is binding 64, u1 binding 193.

Buffer<float4> atlas : register( t0 );
RWStructuredBuffer<float4> texels : register( u1 );

struct FetchPush
{
	uint first;  // the first texel fetched
	uint count;  // texels fetched (texels[0 .. count))
};
[[vk::push_constant]] FetchPush push;

[numthreads( 64, 1, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	if ( id.x < push.count )
		texels[id.x] = atlas.Load( int( push.first + id.x ) );
}
