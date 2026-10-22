#version 410 core

/* The cell lights' shadow caster pass, alpha-tested shapes (lane LIGHTBOOTH1): cell_shadowdepth.vert plus the
 * shape's first UV set, in the renderer's location 7. */

uniform mat4 shRelFromView;
uniform mat4 shClipFromRel;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec3 shRel;
out vec2 texCoord;

void main()
{
	vec4 rel = shRelFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = shClipFromRel * vec4( rel.xyz, 1.0 );
}
