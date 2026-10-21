#version 410 core

// lane VOLAIR1: a full-screen quad over one froxel layer (src/gl/cellvolair.cpp, cell_volair.frag)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
