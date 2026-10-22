#version 410 core

// lane SHADOWCACHE1: one triangle over the viewport, no vertex data
void main()
{
	vec2 p = vec2( ( gl_VertexID << 1 ) & 2, gl_VertexID & 2 );
	gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 );
}
