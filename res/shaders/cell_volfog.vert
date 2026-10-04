#version 410 core

// lane VOLFOG1: a full-screen quad over one froxel layer (src/gl/cellvolfog.cpp, cell_volfog.frag)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
