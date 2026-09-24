#version 410 core

/* Lookdev background (lane PBRR2B), vertex stage: one full-screen quad in NDC.
 * The fragment stage turns each pixel into its view ray through the SAME
 * projection the model is drawn with, so the background and the model's
 * reflections agree on the field of view. */

#include "uniforms.glsl"

layout ( location = 0 ) in vec3 vertexPosition;	// -1..1 NDC

out vec2 ndc;

void main()
{
	ndc = vertexPosition.xy;
	gl_Position = vec4( vertexPosition.xy, 1.0, 1.0 );
}
