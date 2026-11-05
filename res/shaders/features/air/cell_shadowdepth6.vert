#version 410 core
#extension GL_ARB_shader_viewport_layer_array : require

/* lane MODULES1: the air lamp atlas's layered caster pass (src/features/air/airlampatlas.cpp). One instanced draw a caster:
 * instance i draws cube face shFaces[i] into viewport shFaces[i] (the face's atlas tile), with that face's projection.
 * The same math as cell_shadowdepth.vert, so a tile holds the same depths as the one-draw-a-face pass. */

uniform mat4 shRelFromView;
uniform mat4 shClipFace[6];
uniform int shFaces[6];
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec3 shRel;
out vec2 texCoord;

void main()
{
	int f = shFaces[gl_InstanceID];
	vec4 rel = shRelFromView * ( modelViewMatrix * vec4( vertexPosition, 1.0 ) );
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = shClipFace[f] * vec4( rel.xyz, 1.0 );
	gl_ViewportIndex = f;
}
