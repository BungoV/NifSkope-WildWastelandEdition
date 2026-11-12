// Lane VKLIGHT1 round 2: the full-screen triangle for tests/vklight/fragmatch.cpp (the GL twin: frag_gate.py VERT).
// profile: vs_6_0
float4 main( uint id : SV_VertexID ) : SV_Position
{
	float2 p = float2( float( ( id << 1 ) & 2 ), float( id & 2 ) );
	return float4( p * 2.0 - 1.0, 0.0, 1.0 );
}
