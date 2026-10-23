#version 410 core

/* The cell lights' shadow caster pass (lane SHADOW1), vertex stage: the shape's own model-view matrix
 * (as the renderer draws it), view space -> world relative to the light (shRelFromView), then the cube
 * face's projection (shClipFromRel: GL's face axes, 90 degrees). */

uniform mat4 shRelFromView;
uniform mat4 shClipFromRel;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;	// lane GAPFIX1: alpha-tested casters' first UV set

out vec3 shRel;
out vec2 texCoord;

void main()
{
	vec4 rel = shRelFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = shClipFromRel * vec4( rel.xyz, 1.0 );
}
