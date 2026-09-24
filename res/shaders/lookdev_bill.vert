#version 410 core

/* Lookdev sky billboard (lane PBRWX1), vertex stage: a camera-facing quad at a
 * world DIRECTION, at unit distance, half-size = tan(half-angle). */

#include "uniforms.glsl"

layout ( location = 0 ) in vec3 vertexPosition;	// corners -1..1

uniform vec3 centerWorld;	// unit, world (Z up)
uniform float halfSize;

out vec2 uv;

void main()
{
	vec3 c = viewMatrix * centerWorld;
	vec3 p = c + vec3( vertexPosition.xy * halfSize, 0.0 );
	uv = vec2( vertexPosition.x * 0.5 + 0.5, 0.5 - vertexPosition.y * 0.5 );
	vec4 q = projectionMatrix * vec4( p, 1.0 );
	gl_Position = vec4( q.xy, 0.0, q.w );
}
