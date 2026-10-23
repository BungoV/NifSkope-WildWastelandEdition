#version 410 core

layout ( points ) in;
layout ( triangle_strip, max_vertices = 4 ) out;

#include "uniforms.glsl"

uniform vec2 particleScale;
// sub-texture cell for atlas sheets: xy = uv offset, zw = uv scale (default 0,0,1,1)
uniform vec4 particleUV;
// lane VOLPART1, gates only: > 0 = the probe row's width in pixels; each particle then draws its own 8 pixels
// (8 x its index .. + 8, one row) at its centre instead of its sprite (src/gl/glparticles.cpp, WW_CELL_VOLPART_DUMP)
uniform int volPartDump;

in vec3 vsLightDir[];
in vec3 vsViewDir[];

in vec4 vsColor[];
in float vsParticleSize[];
in vec2 vsUVOffset[];
in float vsAngle[];

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoords[9];

flat out vec4 A;
out vec4 C;
flat out vec4 D;

out vec3 N;

// lane VOLPART1 (particles_cell.frag reads them; the viewer's particles.frag does not): the sprite's own point
// (view space), its own coordinates (-1..1 before its rotation), and cos, sin of its rotation, its half-size
// (view units), the particle's index
out vec3 pPos;
out vec2 pQ;
flat out vec4 pSprite;

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

	// per-particle sprite rotation about the view axis (NiPSysRotationModifier)
	float	ca = cos( vsAngle[0] );
	float	sa = sin( vsAngle[0] );
	pSprite = vec4( ca, sa, sx, float( gl_PrimitiveIDIn ) );
	// rotated corner offsets: (u,v) in [-1,1] scaled by the sprite half-size
	vec4	c11 = vec4(  ( ca - sa ) * sx, (  sa + ca ) * sy, 0.0, 0.0 );
	vec4	c01 = vec4(  ( -ca - sa ) * sx, ( -sa + ca ) * sy, 0.0, 0.0 );
	vec4	c10 = vec4(  ( ca + sa ) * sx, (  sa - ca ) * sy, 0.0, 0.0 );
	vec4	c00 = vec4(  ( -ca + sa ) * sx, ( -sa - ca ) * sy, 0.0, 0.0 );

	vec2	uv;
	vec2	uvBase = particleUV.xy + vsUVOffset[0];

	if ( volPartDump > 0 ) {
		// the probe: the centre's texel and point on all four corners, the quad over this particle's 8 pixels
		uv = uvBase + vec2( 0.5, 0.5 ) * particleUV.zw;
		float x0 = float( gl_PrimitiveIDIn * 8 ) / float( volPartDump ) * 2.0 - 1.0;
		float x1 = float( gl_PrimitiveIDIn * 8 + 8 ) / float( volPartDump ) * 2.0 - 1.0;
		for ( int k = 0; k < 4; k++ ) {
			for ( int i = 0; i < 9; i++ )
				texCoords[i] = uv;
			pPos = gl_in[0].gl_Position.xyz;
			pQ = vec2( 0.0 );
			gl_Position = vec4( ( k & 1 ) != 0 ? x1 : x0, ( k & 2 ) != 0 ? 1.0 : -1.0, 0.0, 1.0 );
			EmitVertex();
		}
		EndPrimitive();
		return;
	}

	uv = uvBase + vec2( 1.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c11 ).xyz;
	pQ = vec2( 1.0, 1.0 );
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + c11 );
	EmitVertex();
	uv = uvBase + vec2( 0.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c01 ).xyz;
	pQ = vec2( -1.0, 1.0 );
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + c01 );
	EmitVertex();
	uv = uvBase + vec2( 1.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c10 ).xyz;
	pQ = vec2( 1.0, -1.0 );
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + c10 );
	EmitVertex();
	uv = uvBase + vec2( 0.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c00 ).xyz;
	pQ = vec2( -1.0, -1.0 );
	gl_Position = projectionMatrix * ( gl_in[0].gl_Position + c00 );
	EmitVertex();

	EndPrimitive();
}
