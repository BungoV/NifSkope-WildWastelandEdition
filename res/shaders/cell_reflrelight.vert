#version 410 core

// lane REFLPROBE1: a full-screen quad over one probe's atlas tile (the viewport is the tile)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
