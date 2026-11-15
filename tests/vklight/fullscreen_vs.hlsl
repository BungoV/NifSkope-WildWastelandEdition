// VKFAR3 ruling 2: the GL reference's full-screen triangle (light_gl_ref.py VERT) as HLSL, so a SPIR-V fragment links with it
float4 main( uint id : SV_VertexID ) : SV_Position
{
	float2 p = float2( float( ( id << 1 ) & 2 ), float( id & 2 ) );
	return float4( p * 2.0 - 1.0, 0.0, 1.0 );
}
