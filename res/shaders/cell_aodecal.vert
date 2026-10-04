#version 410 core

// lane AODECAL1: model space straight to clip space (the copy's model -> view -> projection, composed on the CPU)

layout ( location = 0 ) in vec3 vertexPosition;

uniform vec4 aoClip[4];		// clip = aoClip[r] . (model, 1), r = x, y, z, w

void main()
{
	vec4 p = vec4( vertexPosition, 1.0 );
	gl_Position = vec4( dot( aoClip[0], p ), dot( aoClip[1], p ), dot( aoClip[2], p ), dot( aoClip[3], p ) );
}
