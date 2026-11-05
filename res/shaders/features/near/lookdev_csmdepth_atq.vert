#version 410 core

/* The alpha-tested casters through the render queues (lane GPUDRIVEN1): lookdev_csmdepth_at.vert with the
 * model-view matrix per draw from the queue's matrix buffer (locations 8-11). */

uniform mat4 csmClipFromView;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;
layout ( location = 8 ) in mat4 modelViewMatrix;

out vec2 texCoord;

void main()
{
	texCoord = multiTexCoord0;
	gl_Position = csmClipFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
