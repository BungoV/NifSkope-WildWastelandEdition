// hlsl profile: vs_6_0
// Lane VKFRAME1 round 11 (2026-11-12): the full-screen triangle of the tone pass on the present device's view
// (vkpresent.cpp). z = 1 like cell_hdr.vert, so the view's depth test (GREATER / LEQUAL) treats it as GL's quad.
float4 main( uint id : SV_VertexID ) : SV_Position
{
	const float2 q = float2( float( ( id << 1 ) & 2 ), float( id & 2 ) );
	return float4( q * 2.0f - 1.0f, 1.0f, 1.0f );
}
