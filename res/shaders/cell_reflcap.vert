#version 410 core

/* lane REFLPROBE1: a static shape into one face of a probe's G-buffer cube. Its own model-view matrix (as the
 * renderer draws it), view space -> world relative to the probe (rpRelFromView), then GL's face projection. */

uniform mat4 rpRelFromView;
uniform mat4 rpClipFromRel;
uniform mat4 modelViewMatrix;
uniform vec2 uvScale;
uniform vec2 uvOffset;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;
layout ( location = 2 ) in vec3 normalVector;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec3 rpRel;
out vec3 rpN;
out vec4 rpC;
out vec2 rpUv;

void main()
{
	vec4 rel = rpRelFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
	rpRel = rel.xyz;
	rpN = mat3( rpRelFromView ) * ( mat3( modelViewMatrix ) * normalVector );
	rpC = vertexColor;
	rpUv = multiTexCoord0 * uvScale + uvOffset;
	gl_Position = rpClipFromRel * vec4( rel.xyz, 1.0 );
}
