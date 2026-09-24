#version 410 core

/* The cascade caster pass (lane CSM1), vertex stage: the shape's own model-view
 * matrix (as the renderer draws it), then view space -> the cascade's clip space. */

uniform mat4 csmClipFromView;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = csmClipFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
