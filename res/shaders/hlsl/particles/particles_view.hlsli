// Lane VKOBJ5 (2026-11-14, plan row G4 round 3): particles_cell.prog (particles.vert + particles.geom + particles_cell.frag)
// on the present device, in VKFAR2's precip shape (vkprecip.inc, precip_view_cs / _vs): the present device has no geometry
// stage, so a record pass (pfx_view_cs.hlsl) runs particles.vert and the geom's per-primitive part once a particle and
// writes one fixed record a primitive in GL's order (nothing culled, nothing compacted: GL's geom emits every point and the
// blend needs GL's order), and an expand stage (pfx_view_vs.hlsl) draws each record as the geom's 4-vertex strip
// (SV_InstanceID = the record, SV_VertexID = the corner in GL's EmitVertex order). The pixel stage is particles_cell_ps.hlsl
// as it stands. Both stages here are compiled on VS_SHIFT (b 256 / t 320 / u 448): the record pass's numbers are the
// expand stage's, one mirrored copy of GL's uniforms serves both (vkobjpipe.inc objPfxShade).
#include "ParticlesBindingsVk.hlsli"

// uniforms.glsl as particles_vs.hlsl inlines it (the members the two stages read: projectionMatrix, lightSourcePosition)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};

// particles.vert's and particles.geom's loose uniforms in one block (GL's program holds both: mirrored by name)
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
	float2 particleScale;
	float4 particleUV;	// atlas cell: xy = uv offset, zw = uv scale
	int volPartDump;	// lane VOLPART1, gates only: > 0 = the probe row's width in pixels
};

// one particle as GL's attributes feed particles.vert (an attribute GL does not feed: GL's current value, written by the host)
struct PfxIn
{
	float4 posAngle;	// xyz vertexPosition (location 0), w vertexAngle (location 3)
	float4 color;		// vertexColor (location 1)
	float4 uvSize;		// xy vertexUV (location 2), z particleSize (location 4), w 0
};

/* one primitive: what the geom computes before its first EmitVertex and the pixel stage reads (80 bytes). Not kept: the
 * geom's LightDir, A, D, N (particles_cell.frag reads none of them) */
struct PfxRec
{
	float4 v;		// particles.vert's gl_Position (view space): the geom's gl_in[0].gl_Position
	float4 color;		// vsColor (the geom's C)
	float4 sprite;		// the geom's pSprite: cos, sin of the rotation, sx, the primitive's index
	float4 viewDirSy;	// xyz vsViewDir (the geom's ViewDir), w sy
	float4 uvBase;		// xy particleUV.xy + vsUVOffset, zw 0
};
