#version 410 core

/* Lookdev ground plane (lane PBRR2B), vertex stage. The quad arrives in the
 * scene's world space (Z up); texture coordinates come from world XY. */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;

out vec3 worldPos;

void main()
{
	worldPos = vertexPosition;
	gl_Position = projectionMatrix * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
