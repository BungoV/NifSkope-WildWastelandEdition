#version 410 core

/* The cascade caster pass (lane CSM1), vertex stage: the shape's own model-view
 * matrix (as the renderer draws it), then view space -> the cascade's clip space. */

uniform mat4 csmClipFromView;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;

// lane FARDRAW1: an instanced far ring's placement rows (fo4_default.vert); false = every other caster
uniform bool farInst;
layout ( location = 10 ) in vec4 farInstRow0;
layout ( location = 11 ) in vec4 farInstRow1;
layout ( location = 12 ) in vec4 farInstRow2;

void main()
{
	vec4 v = vec4( vertexPosition, 1.0 );
	if ( farInst )
		v.xyz = vec3( dot( farInstRow0, v ), dot( farInstRow1, v ), dot( farInstRow2, v ) );
	gl_Position = csmClipFromView * ( modelViewMatrix * v );
}
