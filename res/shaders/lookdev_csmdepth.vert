#version 410 core

/* The cascade caster pass (lane CSM1), vertex stage: the shape's own model-view
 * matrix (as the renderer draws it), then view space -> the cascade's clip space.
 * Lane TERRSHADOW1: the UVs ride along for an alpha-tested caster's cut-out. */

uniform mat4 csmClipFromView;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec2 casterUv;

void main()
{
	casterUv = multiTexCoord0;
	gl_Position = csmClipFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
