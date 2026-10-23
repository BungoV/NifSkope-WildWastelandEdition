#version 410 core

/* The cascade caster pass through the render queues (lane GPUDRIVEN1): lookdev_csmdepth.vert with the shape's
 * model-view matrix read per draw from the queue's matrix buffer (locations 8-11, one per draw via baseInstance). */

uniform mat4 csmClipFromView;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 8 ) in mat4 modelViewMatrix;

void main()
{
	gl_Position = csmClipFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
}
