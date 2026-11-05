#version 410 core

// lane WATERLOOK1 (src/gl/cellocean.cpp): the full-screen quad into all six layers of a layered target in one draw
// (instanced geometry shader, one invocation a layer): one render target change a mip level instead of six.

layout ( triangles, invocations = 6 ) in;
layout ( triangle_strip, max_vertices = 3 ) out;

flat out int oceanLayerG;

void main()
{
	for ( int i = 0; i < 3; i++ ) {
		gl_Position = gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		oceanLayerG = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}
