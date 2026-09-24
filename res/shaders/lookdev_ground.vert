#version 410 core

/* Lookdev ground plane (lane PBRR2B), vertex stage. The quad arrives in the
 * scene's world space (Z up); texture coordinates come from world XY. */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;

out vec3 worldPos;
out vec3 viewPos;	// lane FOG1: the fog reads the view-space position

void main()
{
	worldPos = vertexPosition;
	vec4 v = modelViewMatrix * vec4( vertexPosition, 1.0 );
	viewPos = v.xyz;
	gl_Position = projectionMatrix * v;
}
