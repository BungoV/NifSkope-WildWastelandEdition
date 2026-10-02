#version 410 core

// lane AO1: a full-screen quad (the obscurance's passes, cell_ao.frag)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
