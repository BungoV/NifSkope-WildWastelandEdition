#version 410 core

/* The cascade caster pass, alpha-tested shapes (lane VOLAIR1): lookdev_csmdepth.vert plus the
 * shape's first UV set, in the renderer's location 7. */

uniform mat4 csmClipFromView;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec2 texCoord;

void main()
{
	texCoord = multiTexCoord0;
	gl_Position = csmClipFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
