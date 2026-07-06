#version 410 core

layout ( points ) in;
layout ( triangle_strip, max_vertices = 4 ) out;

#include "uniforms.glsl"

uniform vec2 particleScale;
// sub-texture cell for atlas sheets: xy = uv offset, zw = uv scale (default 0,0,1,1)
uniform vec4 particleUV;

in vec3 vsLightDir[];
in vec3 vsViewDir[];

in vec4 vsColor[];
in float vsParticleSize[];
in vec2 vsUVOffset[];

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoords[9];

flat out vec4 A;
out vec4 C;
flat out vec4 D;

out vec3 N;

void main()
{
	LightDir = vsLightDir[0];
	ViewDir = vsViewDir[0];

	A = vec4( vec3(1.0), toneMapScale );
	C = vsColor[0];
	D = vec4( vec3(0.0), sqrt(brightnessScale) );

	N = vec3( 0.0, 0.0, 1.0 );

	float	sx = vsParticleSize[0] * particleScale.x;
	float	sy = vsParticleSize[0] * particleScale.y;

	vec2	uv;
	vec2	uvBase = particleUV.xy + vsUVOffset[0];

	uv = uvBase + vec2( 1.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + vec4( sx, sy, 0.0, 0.0 ) );
	EmitVertex();
	uv = uvBase + vec2( 0.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + vec4( -sx, sy, 0.0, 0.0 ) );
	EmitVertex();
	uv = uvBase + vec2( 1.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + vec4( sx, -sy, 0.0, 0.0 ) );
	EmitVertex();
	uv = uvBase + vec2( 0.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + vec4( -sx, -sy, 0.0, 0.0 ) );
	EmitVertex();

	EndPrimitive();
}
