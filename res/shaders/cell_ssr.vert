#version 410 core

// lane SSR1: a full-screen quad (the reflections' passes, cell_ssr.frag)

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
