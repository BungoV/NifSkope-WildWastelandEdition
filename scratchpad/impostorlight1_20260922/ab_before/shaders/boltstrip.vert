#version 410 core

// Textured camera-facing strip for the BSProceduralLightningController preview

out vec4 C;
out vec2 texCoord;

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec4	vertexColor;
layout ( location = 2 ) in vec2	vertexUV;

void main()
{
	C = vertexColor;
	texCoord = vertexUV;

	gl_Position = projectionMatrix * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
