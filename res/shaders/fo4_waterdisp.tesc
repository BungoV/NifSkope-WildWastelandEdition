#version 410 core

// lane WATERLOOK1 step 2 (src/gl/cellocean.h): the water's triangles are split near the camera so the wave heights can
// move its vertices (fo4_waterdisp.tese). An edge's level is a function of the edge alone, symmetric in its two ends
// (a + b, a - b up to sign), so the two triangles that share an edge split it alike: no cracks. Past the fade (no
// displacement) every level is 1, the triangle as fo4_default.vert drew it.

layout ( vertices = 3 ) out;

in vec3 LightDir[];
in vec3 ViewDir[];
in vec2 texCoord[];
in mat3 btnMatrix[];
flat in vec4 A[];
in vec4 C[];
flat in vec4 D[];
in float rawVertexAlpha[];
flat in mat3 reflMatrix[];

out vec3 tcLightDir[];
out vec3 tcViewDir[];
out vec2 tcTexCoord[];
out mat3 tcBtn[];
flat out vec4 tcA[];
out vec4 tcC[];
flat out vec4 tcD[];
out float tcRawAlpha[];
flat out mat3 tcRefl[];

#include "uniforms.glsl"

uniform bool waterOceanOn;
uniform vec4 waterOceanDisp;	// full out to (u), none past (u), the target edge (px), the most a GPU splits an edge
uniform int waterOceanRed;	// lane PERFFIX3: 8192 densetess = every edge up to waterOceanDisp.w splits (before)

// the level of the edge a - b (view space)
float wwDispLevel( vec3 a, vec3 b )
{
	vec3 mid = 0.5 * ( a + b );
	float L = length( a - b );
	float dm = length( mid );
	if ( dm - 0.5 * L >= waterOceanDisp.y )
		return 1.0;
	float pxu = projectionMatrix[1][1] * 0.5 * float( viewportDimensions.w ) / max( dm, 1.0 );	// pixels a unit there
	float lv = max( L * pxu / waterOceanDisp.z, L * ( 1.0 / 256.0 ) );	// and never coarser than 256 u where it moves
	/* lane PERFFIX3 (PERFRES1101 B water OPT 2): no vertex step finer than the heights' own filter (fo4_waterdisp.tese:
	 * step >= 4096 / waterOceanDisp.w = 64 u): the pre-split 1024 u quads (src/cellview.cpp) split to 16 u and sampled
	 * a field already smoothed over 64 u four times over, ~1 M triangles near the camera (+1.4 ms at 1080p 4x MSAA,
	 * tests/watertess). An edge's own length bounds it, so two triangles sharing the edge still split it alike. */
	float cap = ( waterOceanRed & 8192 ) != 0 ? waterOceanDisp.w
		: clamp( L * waterOceanDisp.w * ( 1.0 / 4096.0 ), 1.0, waterOceanDisp.w );
	return clamp( lv, 1.0, cap );
}

void main()
{
	tcLightDir[gl_InvocationID] = LightDir[gl_InvocationID];
	tcViewDir[gl_InvocationID] = ViewDir[gl_InvocationID];
	tcTexCoord[gl_InvocationID] = texCoord[gl_InvocationID];
	tcBtn[gl_InvocationID] = btnMatrix[gl_InvocationID];
	tcA[gl_InvocationID] = A[gl_InvocationID];
	tcC[gl_InvocationID] = C[gl_InvocationID];
	tcD[gl_InvocationID] = D[gl_InvocationID];
	tcRawAlpha[gl_InvocationID] = rawVertexAlpha[gl_InvocationID];
	tcRefl[gl_InvocationID] = reflMatrix[gl_InvocationID];
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	if ( gl_InvocationID == 0 ) {
		// outer i is the edge facing vertex i; ViewDir = -position (an orthographic view has none: never split)
		bool on = waterOceanOn && projectionMatrix[3][3] != 1.0;
		vec3 p0 = -ViewDir[0], p1 = -ViewDir[1], p2 = -ViewDir[2];
		float e0 = on ? wwDispLevel( p1, p2 ) : 1.0;
		float e1 = on ? wwDispLevel( p2, p0 ) : 1.0;
		float e2 = on ? wwDispLevel( p0, p1 ) : 1.0;
		gl_TessLevelOuter[0] = e0;
		gl_TessLevelOuter[1] = e1;
		gl_TessLevelOuter[2] = e2;
		gl_TessLevelInner[0] = max( e0, max( e1, e2 ) );
	}
}
