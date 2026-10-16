#version 410 core

// a full-screen quad over one froxel layer (cell_volfog.frag)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
