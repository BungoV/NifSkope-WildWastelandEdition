#version 410 core

// lane TAAINPUTS1: a see-through shape or a particle system's sprites at the frame's (jittered) clip position

uniform mat4 clipJit;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec4 vColor;
out vec2 texCoord;

void main()
{
	vColor = vertexColor;
	texCoord = multiTexCoord0;
	gl_Position = clipJit * vec4( vertexPosition, 1.0 );
}
