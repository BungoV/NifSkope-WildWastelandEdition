#version 410 core

/* Lookdev weather sky dome (lane PBRWX1), vertex stage. The dome follows the
 * camera (the engine's sky root does): view ROTATION only, no translation.
 * Depth is off for every sky pass, so z is parked mid-range. */

#include "uniforms.glsl"

layout ( location = 0 ) in vec3 vertexPosition;	// sky-root space, Z up
layout ( location = 1 ) in vec4 vertexColor;	// R = Horizon, G = Sky-Lower, B = Sky-Upper weights

out vec4 vcol;

void main()
{
	vcol = vertexColor;
	vec4 q = projectionMatrix * vec4( viewMatrix * vertexPosition, 1.0 );
	gl_Position = vec4( q.xy, 0.0, q.w );
}
