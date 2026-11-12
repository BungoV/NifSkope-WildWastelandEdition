// Lane VKFRAME1 (P3 gate): draws the multisample frame for tests/vkframe/srccombmatch.cpp from the job's data, as
// srccomb_gl_ref.py's GEN program does on GL: per sample, the colour at column x * S + s, kept only where that sample's
// stencil value is this draw's (the pipeline's stencil REPLACE writes the value). VS: -E vs -T vs_6_0; PS: -E ps -T ps_6_0.
[[vk::binding( 0, 0 )]] Texture2D<float4> GenData : register( t0 );
[[vk::binding( 1, 0 )]] Texture2D<uint> GenSten : register( t1 );
struct GenPc { int samples; int value; };
[[vk::push_constant]] ConstantBuffer<GenPc> Gen;

float4 vs( uint id : SV_VertexID ) : SV_Position
{
	const float2 p = float2( float( ( id << 1 ) & 2 ), float( id & 2 ) );
	return float4( p * 2.0 - 1.0, 0.0, 1.0 );
}

float4 ps( float4 pos : SV_Position, uint s : SV_SampleIndex ) : SV_Target
{
	const int2 q = int2( int( pos.x ) * Gen.samples + int( s ), int( pos.y ) );
	if ( int( GenSten.Load( int3( q, 0 ) ) ) != Gen.value )
		discard;
	return GenData.Load( int3( q, 0 ) );
}
