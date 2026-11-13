// HLSL source of features/far-ground/fo4_cellfgp.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarGroundBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): fo4_default.frag, uniforms.glsl, ww_bindless.glsl, cell_grass.glsl, land_shore.glsl, land_wet.glsl, land_swash.glsl
static float4 gl_FragCoord;
static bool gl_FrontFacing;
// lane FARPAGES1: fo4_cell.frag with the far ring 0's cached pages (cell_farground.glsl, option 3). Chosen by
// name in Renderer::setupProgram for ring 0's terrain shapes only while the Far ground row is on cached pages.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_FARGROUND 3
// fo4_default.frag inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
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
// end of uniforms.glsl
#ifdef WW_FOG
#define WW_LOOSE_TYPES 1
#if defined(WW_FOG)
#include "../air/lookdev_fog.hlsli"
#endif
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS) && !defined(WW_CELL_GI_MARCH_COPIES)
#ifndef WW_CELL_GI_MARCH_ONCE
#define WW_CELL_GI_MARCH_ONCE 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_GI_ONEREAD
#define CELL_GI_ONEREAD 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_RESTIR_MISS_PIXEL
#define CELL_RESTIR_MISS_PIXEL 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#include "../reflections/cell_reflprobe.hlsli"
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"
#include "../weather/rain_wet.hlsli"
#endif
#if defined(WW_FARGROUND)
#include "cell_farground.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#if defined(WW_FOG)
#include "../air/lookdev_fog.hlsli"
#endif
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS) && !defined(WW_CELL_GI_MARCH_COPIES)
#ifndef WW_CELL_GI_MARCH_ONCE
#define WW_CELL_GI_MARCH_ONCE 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_GI_ONEREAD
#define CELL_GI_ONEREAD 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_RESTIR_MISS_PIXEL
#define CELL_RESTIR_MISS_PIXEL 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#include "../reflections/cell_reflprobe.hlsli"
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"
#include "../weather/rain_wet.hlsli"
#endif
#if defined(WW_FARGROUND)
#include "cell_farground.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
#if defined(WW_BINDLESS)
	uint wwDrawBase;
#endif
#if defined(WW_BINDLESS) && defined(WW_BL_VERT) && defined(GL_ARB_shader_draw_parameters)
	// lane FARLOOK1101 (FARINSTBL1): an instanced far ring's base instance is a placement row; it reads wwDrawBase alone
	bool wwBlUniformRow;
#endif
	float3 specColor;
	float specStrength;
	float specGlossiness; // "Smoothness" in FO4; 0-1
	float fresnelPower;
	float paletteScale;
	float3 glowColor;
	float glowMult;
	float alpha;
	float2 wwDecalPom;	// lane DECALS1: placed decal parallax (DODT scale x 0.1, step cap); x = 0 off
	bool wwDecalAngle;	// lane DECALANGLE: a placed decal whose vertex alpha is dot(N, D) (src/celldecalangle.h)
	bool wwDecalMapped;	// lane DECALANGLE2: and its vertex rgb is D as a signed byte (not a colour)
	int wwMulDecal;	// MODULES1 M2: 1 a multiply decal (DestColor, Zero), 2 a 2x one (DestColor, SrcColor); 0 none
	int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
	bool landSplat;		// lane TERRBLEND1: vertex alpha = this blended-ground pass's share
	int landSplatDbg;	// lane ALLON2: 0 edge guard, 1 red WW_TERRBLEND_RED=edgepass (no guard), 2 probe WW_TERRBLEND_PROBE=edge
	float alphaThreshold;
	float3 tintColor;
	/* LOD channel preview: 0 off, 1 identity as a hashed colour, 2 raw index bytes,
	 * 3 baked AO, 4 class parameter. Non-zero replaces the whole shading path with
	 * the chosen vertex-colour channel, flat -- no textures, no normals, no light.
	 * A per-vertex payload is otherwise invisible: it is multiplied into an albedo
	 * and then lit, and what reaches the screen says nothing about the number that
	 * is actually stored. */
	int lodChannelView;
	bool lodMaskRaw;		// channel 10: the specular slot raw (a source .lodm's third texture)
	bool lodEmissiveRaw;	// channel 13: the glow slot raw (a source .lodm's emissive texture)
	float3 lodEmissiveColor;	// channel 13: the source's emissive colour, folded into the legacy sheet
	float lodSpecStrength;	// channel 10: the material's specular strength, lighting or not
	bool lodTreeAnim;		// channel 11: this shape carries the tree-animation flag
	bool lodMaskByTree;		// channel 11: the mask is the tree flag (a near tree), not the alpha test
	float2 uvScale;
	float2 uvOffset;
	bool hasEmit;
	bool hasGlowMap;
	bool hasSoftlight;
	bool hasBacklight;
	bool hasRimlight;
	bool hasTintColor;
	bool hasCubeMap;
	bool hasEnvMask;
	bool hasSpecularMap;
	float wwMipBias;	// lane PERFFIX4: the material maps' LOD bias under the Modern TAA's jitter (0 = none)
	bool greyscaleColor;
	// Shader Flags 1 bit 12 (SLSF1_Model_Space_Normals): the bound normal map is an
	// `_msn`, a MODEL-space map, not a tangent-space one.
	bool hasModelSpaceNormals;
#if !(defined(WW_BINDLESS))
	row_major float3x3 normalMatrix;
#endif
	float subsurfaceRolloff;
	float rimPower;
	float backlightPower;
	float envReflection;
	bool doRefraction;
	bool editorBrdf;		// lane VANILLADEF1: the material editor's GGX lobe (Shading > Editor BRDF, WW_EDITOR_BRDF); off = the upstream law
	float refractionStrength;
#if defined(WW_CELLLIGHTS)
	// cell_grass.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
	// The game's grass fade on the cell view's grass shapes (src/gl/cellgrass.h).
	// x = fGrassStartFadeDistance, y = fGrassFadeRange (view units), z = 1 on a grass shape (else the factor is 1.0).
	float3 wwGrassFade;
	float4 landShoreGrid;		// world x, y of texel (0, 0), world units a texel, world units a step
	float4 landShoreLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
	bool landShoreOn;		// built, uploaded, and not WW_CELL_LAND_SHORE_RED=off
	bool landShoreTerrain;		// the drawn shape is land (the cells' LAND or the far LOD terrain)
	// end of land_shore.glsl
	// land_wet.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
	// lane WATERLOOK1 step 13 (WATERWET1): the wet band on the land beside baked water, from the land side of the
	// shore plane (land_shore.glsl, included first). Full wet at the waterline, none past a horizontal reach or a
	// height above the water; the height is the shore distance times the ground's slope (tan of its tilt), so a
	// gentle beach wets wide and a steep bank narrow. Porous ground darkens and gains a little gloss (Lagarde,
	// "Water drop 3b": base x mix(1, 0.2, porosity), gloss toward 1 by half that); a thin film near the line is
	// glossy on any material. Neutral is wet 0: the row off, the shore lookup off, or no water near.
	bool landWetOn;		// the Wet shore row
	int landWetRed;		// 1 noslope (the horizontal reach only), 2 nofilm, 4 nodark
	float4 landWetK;		// horizontal reach (u), vertical reach (u), wet porous base factor (linear), film reach (u)
	// land_swash.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
	/* lane WATERLOOK1 step 14 (WATERSWASH1): the waves' run-up on the land beside baked water (the Wave run-up row).
	 * Needs land_shore.glsl, water_info.glsl and water_shore.glsl first. Each wave's bore reaches the still waterline on
	 * SURF1's phase (water_surf.glsl at the shore distance 0: omega t plus its along-shore noise, its wave groups), runs
	 * up the beach and drains back:
	 *   run-up     the 2% height of Stockdon et al. 2006, R2 = 1.1 ( 0.35 b sqrt( H0 L0 ) + 0.5 sqrt( H0 L0 ( 0.563 b^2 +
	 *              0.004 ) ) ), H0 the shore's Hs (WATERFOAMDETAIL1: wind sea by fetch, plus the swell on a sea), L0 the
	 *              deep wavelength of the period (SURF1's on a sea, the wind sea's on lakes and rivers), b the beach slope;
	 *              a wave's own top scales with the root of its group envelope (R ~ sqrt( H ))
	 *   motion     the tip rises and falls on a parabola in time (ballistic swash, Shen & Meyer 1963), up and back in one
	 *              wave period
	 *   look       a water film under the swash, foam at the tip on the uprush that thins as it drains (fresh water keeps
	 *              less, as the shore foam), the swash zone up to R2 kept saturated
	 * The beach slope is the ground's own (its normal), as in land_wet.glsl. Neutral is all zero. */
	bool landSwashOn;	// the Wave run-up row
	int landSwashRed;	// 1 still (the tip held at its top), 2 nofoam, 4 regular (no groups, no along-shore noise)
	float4 landSwashK;	// sea period (s, SURF1's), waves a group (SURF1's), foam opacity, foam e-fold (m, horizontal)
	float landSwashTime;	// the world clock (s), as the water's
	// lane LANDSPEC1: x 0 off, 1 near ground, 2 far LOD ground; yz the camera cell's centre (scene xy)
	float4 landLod;
	float4 landLodK;	// fLODLandSpecPower, fLODLandSpecScale (0: specular off), blend end, 1 / blend width
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_LANDSPEC_NONE)
	// lane LANDSPEC2: xy the scene origin (world), z 1 = the terrain noise is bound, w 1 = the detail normals are;
	// landLodWin xy the chunk window's corner (world), z its bound chunks (bits, layer = x + 4 y)
	float4 landLodTex;
	float4 landLodWin;	// lane LANDSPEC4: z the window's chunks a side (layer = x + z y), w 1 = red perlayer
	float4 landLodBits[4];	// lane LANDSPEC4: the bound layers, 16 a component; LANDSPEC5: 64 a vec4 (16 x 16)
	// lane LANDSPEC6: past the page (a block wider than 61 cells), the outer window: xy its corner (world), z its chunks a
	// side (0 none), its bound layers 64 a vec4 (32 x 32), each a chunk's mip 1
	// lane LANDSPEC7: up to 45 x 45 (2025 layers, 32 vec4) at mip 2 past 32 chunks a side
	// lane LANDSPEC8: w 2 = packed, 2 x 2 chunks a layer (up to 50 x 50 chunks, 40 vec4 of bits, one a chunk)
	float4 landLodOuter;
	float4 landLodOuterBits[40];
#endif
#if defined(WW_CELLLIGHTS)
	bool wwDecalGbufOn;	// lane DECALANGLE2: cellCtGbuf holds this view's shared opaque pass
	float4 wwDecalGbufRect;	// its origin in the viewport, its size
	float farFadeFrame;
#endif
};
#include "../air/lookdev_fog.hlsli"
#endif
// FOGSUN1: the sun through the air; a variant without the fog compiled in (fo4_default, fo4_csm) has no air between
// the surface and the sun (lane VOLAIR1 10-15: calling wwVolAirSunT there broke both programs; the shader guard found it)
#ifdef WW_FOG
#define WW_AIR_SUN_TD( p ) wwVolAirSunTD( p )
#else
#define WW_AIR_SUN_TD( p ) float2( 1.0, 0.0 )
#endif
#ifdef WW_SUNSHADOW
// lane SUNCELL1: the cascades (fo4_cellcsm, fo4_csm, fo4_fogcsm); the loader takes nothing after an include's quote
#include "../shadows/ww_sunshadow.hlsli"
#endif

#ifdef WW_BINDLESS
// lane BINDLESS2: the material's maps as resident handles in the draw's row (slots of src/features/near/cellbindless.h)
// ww_bindless.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
// lane BINDLESS2: the per-draw row (src/features/near/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as resident texture handles. wwDrawIdx is the draw's row in the ring at binding 12.
// lane BINDLESSMDI1: the row is wwDrawBase + the draw's base instance (0 for a plain draw; a multi-draw's command
// carries its own), handed to the fragment stage flat.
struct WwDraw
{
	float4x4 mv;
	float4 nm[3];
	uint2 tex[8];
};
StructuredBuffer<WwDraw> wwDraws : register(t47, space0);
#ifdef WW_BL_VERT
#ifdef GL_ARB_shader_draw_parameters

#define wwDrawIdx ( wwDrawBase + ( wwBlUniformRow ? 0u : uint( gl_BaseInstanceARB ) ) )
#else
#define wwDrawIdx wwDrawBase
#endif
static uint wwBlRow;
#else
static uint wwBlRow;
#define wwDrawIdx wwBlRow
#endif
// end of ww_bindless.glsl
#define BaseMap (sampler2D)( wwDraws[wwDrawIdx].tex[0] )
#define NormalMap (sampler2D)( wwDraws[wwDrawIdx].tex[1] )
#define GlowMap (sampler2D)( wwDraws[wwDrawIdx].tex[2] )
#define BacklightMap (sampler2D)( wwDraws[wwDrawIdx].tex[3] )
#define SpecularMap (sampler2D)( wwDraws[wwDrawIdx].tex[4] )
#define GreyscaleMap (sampler2D)( wwDraws[wwDrawIdx].tex[5] )
#define EnvironmentMap (sampler2D)( wwDraws[wwDrawIdx].tex[6] )
#else
Texture2D<float4> BaseMap : register(t48, space0); SamplerState _BaseMap_sampler : register(s48, space0);
Texture2D<float4> NormalMap : register(t49, space0); SamplerState _NormalMap_sampler : register(s49, space0);
Texture2D<float4> GlowMap : register(t50, space0); SamplerState _GlowMap_sampler : register(s50, space0);
Texture2D<float4> BacklightMap : register(t51, space0); SamplerState _BacklightMap_sampler : register(s51, space0);
Texture2D<float4> SpecularMap : register(t52, space0); SamplerState _SpecularMap_sampler : register(s52, space0);
Texture2D<float4> GreyscaleMap : register(t53, space0); SamplerState _GreyscaleMap_sampler : register(s53, space0);
Texture2D<float4> EnvironmentMap : register(t54, space0); SamplerState _EnvironmentMap_sampler : register(s54, space0);
#endif
TextureCube<float4> CubeMap : register(t55, space0); SamplerState _CubeMap_sampler : register(s55, space0);
// model -> view rotation, the same row-major uniform the vertex stage uses for
// the normal, tangent and bitangent.  Declared here because the model-space
// path has no tangent frame to ride on.
#ifdef WW_BINDLESS
// lane BINDLESS2FIX: the twin's normal matrix is in the draw's row (the uniform is never set for it: a zero matrix
// turned every model-space normal into NaN)
#define normalMatrix float3x3( wwDraws[wwDrawIdx].nm[0].xyz, wwDraws[wwDrawIdx].nm[1].xyz, wwDraws[wwDrawIdx].nm[2].xyz )
#else

#endif



// screen-space refraction preview (SLSF1_Refraction)
Texture2D<float4> RefractionSrc : register(t56, space0); SamplerState _RefractionSrc_sampler : register(s56, space0);

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;
static float rawVertexAlpha;	// the vertex colour's own alpha (channel 11 G only)
static float wwFarSkyVis;	// lane FIX1101A lodch: a far placement vertex's baked sky factor (fo4_default.vert), 1 elsewhere
static float wwFarAo;	// MODULES1 round 10: a far placement vertex's baked AO (fo4_default.vert), 1 elsewhere
#ifdef WW_FAR_SPLAT	// lane FARLOOK1101 (FARSHIMMER2): the far splat's coverage, 1 / (kx * ky) (fo4_default.vert)
static float farSplatA;
#endif

static float3x3 btnMatrix;
static float3x3 reflMatrix;

static float4 fragColor;
#ifdef WW_CELLLIGHTS
// lane SSGI2: the scene pass's second target, the emitted share alone (the bounce's emissive cast; 0 elsewhere)
static float4 cellEmitOut;
#endif

static float3 ViewDir_norm = normalize( ViewDir );
static float3x3 btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0 + FLT_EPSILON != 1.0

float OrenNayar( float3 L, float3 V, float3 N, float roughness, float NdotL )
{
	//float NdotL = dot(N, L);
	float NdotV = dot(N, V);
	float LdotV = dot(L, V);

	float rough2 = roughness * roughness;

	float A = 1.0 - 0.5 * (rough2 / (rough2 + 0.57));
	float B = 0.45 * (rough2 / (rough2 + 0.09));

	float a = min( NdotV, NdotL );
	float b = max( NdotV, NdotL );
	b = (sign(b) == 0.0) ? FLT_EPSILON : sign(b) * max( 0.01, abs(b) ); // For fudging the smoothness of C
	float C = sqrt( (1.0 - a * a) * (1.0 - b * b) ) / b;

	float gamma = LdotV - NdotL * NdotV;
	float L1 = A + B * max( gamma, FLT_EPSILON ) * C;

	return L1 * max( NdotL, FLT_EPSILON );
}

float OrenNayarFull(float NdotL, float NdotV, float LdotV, float roughness)
{
	float	NdotL0 = clamp(NdotL, FLT_EPSILON, 1.0);
	float	angleVN = acos(clamp(abs(NdotV), FLT_EPSILON, 1.0));
	float	angleLN = acos(NdotL0);

	float	alpha = max(angleVN, angleLN);
	float	beta = min(angleVN, angleLN);
	float	gamma = (LdotV - NdotL * NdotV) / sqrt(max((1.0 - NdotL * NdotL) * (1.0 - NdotV * NdotV), 0.000025));

	float roughnessSquared = roughness * roughness;
	float roughnessSquared9 = (roughnessSquared / (roughnessSquared + 0.09));

	// C1, C2, and C3
	float C1 = 1.0 - 0.5 * (roughnessSquared / (roughnessSquared + 0.33));
	float C2 = 0.45 * roughnessSquared9;

	if( gamma >= 0.0 ) {
		C2 *= sin(alpha);
	} else {
		C2 *= (sin(alpha) - pow((2.0 * beta) / M_PI, 3.0));
	}

	float powValue = (4.0 * alpha * beta) / (M_PI * M_PI);
	float C3 = 0.125 * roughnessSquared9 * powValue * powValue;

	// Avoid asymptote at pi/2
	float asym = M_PI / 2.0;
	float lim1 = asym + 0.005;
	float lim2 = asym - 0.005;

	float ab2 = (alpha + beta) / 2.0;

	if ( beta >= asym && beta < lim1 )
		beta = lim1;
	else if ( beta < asym && beta >= lim2 )
		beta = lim2;

	if ( ab2 >= asym && ab2 < lim1 )
		ab2 = lim1;
	else if ( ab2 < asym && ab2 >= lim2 )
		ab2 = lim2;

	// Reflection
	float A = gamma * C2 * tan(beta);
	float B = (1.0 - abs(gamma)) * C3 * tan(ab2);

	float L1 = NdotL0 * (C1 + A + B);

	// Interreflection
	float twoBetaPi = 2.0 * beta / M_PI;
	float L2 = 0.17 * NdotL0 * (roughnessSquared / (roughnessSquared + 0.13)) * (1.0 - gamma * twoBetaPi * twoBetaPi);

	return L1 + L2;
}

// GGX/Trowbridge-Reitz normal distribution. Ported from the PBR Material
// Editor's shader (D_GGX/G1 there) so the two tools agree on the spec/gloss
// response instead of NifSkope using a normalized Phong lobe.
float D_GGX( float NdotH, float alpha )
{
	float a2 = alpha * alpha;
	float d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	return a2 / max( M_PI * d * d, 0.0001 );
}

// Smith geometry term, one direction. k = (rough+1)^2/8 (the editor's split).
float G1( float NdotX, float k )
{
	return NdotX / max( NdotX * ( 1.0 - k ) + k, 0.0001 );
}

// Schlick's Fresnel approximation
float fresnelSchlick( float VdotH, float F0 )
{
	float base = 1.0 - VdotH;
	float exp = pow( base, fresnelPower );
	return clamp( exp + F0 * (1.0 - exp), 0.0, 1.0 );
}

// The Torrance-Sparrow visibility factor, G
float VisibDiv( float NdotL, float NdotV, float VdotH, float NdotH )
{
	float denom = max( VdotH, FLT_EPSILON );
	float numL = min( NdotV, NdotL );
	float numR = 2.0 * NdotH;
	if ( denom >= (numL * numR) ) {
		numL = (numL == NdotV) ? 1.0 : (NdotL / NdotV);
		return (numL * numR) / denom;
	}
	return 1.0 / NdotV;
}

// this is a normalized Phong model used in the Torrance-Sparrow model
float3 TorranceSparrow(float NdotL, float NdotH, float NdotV, float VdotH, float3 color, float power, float F0)
{
	// D: Normalized phong model
	float D = ((power + 2.0) / (2.0 * M_PI)) * pow( NdotH, power );

	// G: Torrance-Sparrow visibility term divided by NdotV
	float G_NdotV = VisibDiv( NdotL, NdotV, VdotH, NdotH );

	// F: Schlick's approximation
	float F = fresnelSchlick( VdotH, F0 );

	// Torrance-Sparrow:
	// (F * G * D) / (4 * NdotL * NdotV)
	// Division by NdotV is done in VisibDiv()
	// and division by NdotL is removed since
	// outgoing radiance is determined by:
	// BRDF * NdotL * L()
	float spec = (F * G_NdotV * D) / 4.0;

	return color * spec * M_PI;
}

float3 tonemap(float3 x)
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	float3 z = x * x * D.a * (A.a * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return sqrt(z / (A.a * 0.93333333));
}


#ifdef WW_CELLLIGHTS
#ifndef WW_CELL_GI_MARCH_COPIES	// red (tests/features/gi/cell_giblack_link.sh): a march copy per call site, over the link limit
#define WW_CELL_GI_MARCH_ONCE 1	// lane GIBLACKLINK1: the march red runs once (cellGiMarchPrime in main)
#endif
#define CELL_GI_ONEREAD 1	// DDGIREADPERF1: one volume read site (cellGiFirst in main, cell_lights.glsl)
#define CELL_RESTIR_MISS_PIXEL 1	// MODULES1 R7 item 2: a surface off the traced depth takes the pixel's shadow ratio (cellShadowF)
#include "../lights/cell_lights.hlsli"
#include "../reflections/cell_reflprobe.hlsli"
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"


float wwGrassFadeK( float3 v )
{
	if ( wwGrassFade.z < 0.5 )
		return 1.0;
	return 1.0 - clamp( ( length( v ) - wwGrassFade.x ) / max( wwGrassFade.y, 1e-3 ), 0.0, 1.0 );
}

// the game's grass carries no vertex alpha into its pixel (o6.w is the fade): on a grass shape C.a is the wind's
// weight (fo4_default.vert), so it reads 1; every other shape keeps its own
float wwGrassVertexA( float vertexA )
{
	return wwGrassFade.z < 0.5 ? vertexA : 1.0;
}
// end of cell_grass.glsl
// lane WATERWETBAKE1: the land side of the shore plane; WATERLOOK1 step 13: the Wet shore band on it
// land_shore.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
// lane WATERWETBAKE1: the land side of the .lodl shore plane (src/features/water/cellwaterinfo.h), for the terrain shaders:
// a dry texel's distance to the nearest wet texel in 32 u steps (255 = 8160 u or more, 0 on wet texels).
// One index texel per 16 x 16-texel tile (the uniform tile's byte, or (slot + 1) << 8), and for a non-uniform
// tile one atlas texel. The cell programs' Wet shore row reads it (land_wet.glsl, lane WATERLOOK1 step 13).
Texture2D<uint4> landShoreIndex : register(t57, space0); SamplerState _landShoreIndex_sampler : register(s57, space0);	// R32UI: uniform byte, or (slot + 1) << 8
Texture2D<uint4> landShoreAtlas : register(t58, space0); SamplerState _landShoreAtlas_sampler : register(s58, space0);	// R8UI: the non-uniform tiles' bytes, 64 tiles a row

// The stored steps of texel t (0..255); 255 (far) off the map or with the lookup off
uint wwLandShoreTexel( int2 t )
{
	if ( !landShoreOn )
		return 255u;
	int E = int( landShoreLayout.z );
	int2 tile = t / E;
	if ( or( or( or( t.x < 0, t.y < 0 ), tile.x >= int( landShoreLayout.x ) ), tile.y >= int( landShoreLayout.y ) ) )
		return 255u;
	uint w = landShoreIndex.Load( int3(tile, 0) ).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = int( w >> 8 ) - 1, perRow = int( landShoreLayout.w );
	return landShoreAtlas.Load( int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0) ).r;
}

// The texel's distance to the waterline in world units at a world position (nearest texel); 8160 = far
float wwLandShore( float2 worldXY )
{
	int2 t = int2( floor( ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) ) );
	return float( wwLandShoreTexel( t ) ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}

// The same, bilinear between the four nearest texel centres (a smooth wet edge across texels and tiles)
float wwLandShoreSmooth( float2 worldXY )
{
	float2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	int2 t = int2( floor( p ) );
	float2 f = p - float2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + int2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + int2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + int2( 1, 1 ) ) );
	return lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}

// (wet, film) at world point P with the ground's world normal N (geometric, not the normal map's)
float2 wwLandWet( float3 P, float3 N )
{
	if ( or( !landWetOn, !landShoreOn ) )
		return (float2)( 0.0 );
	float d = wwLandShoreSmooth( P.xy );
	if ( d >= landWetK.x )
		return (float2)( 0.0 );
	float nz = clamp( N.z, 0.05, 1.0 );
	float h = d * sqrt( 1.0 - nz * nz ) / nz;	// the height above the water, along the slope
	float wv = ( landWetRed & 1 ) != 0 ? 1.0 : 1.0 - smoothstep( 0.0, landWetK.y, h );
	float wet = ( 1.0 - smoothstep( 0.0, landWetK.x, d ) ) * wv;
	float film = ( landWetRed & 2 ) != 0 ? 0.0 : ( 1.0 - smoothstep( 0.0, landWetK.w, d ) ) * wv;
	return float2( wet, film );
}

// The linear base factor at wetness w and porosity por (1 = dry or the darkening red)
float wwLandWetBase( float w, float por )
{
	return ( landWetRed & 4 ) != 0 ? 1.0 : lerp( 1.0, landWetK.z, por * w );
}

// Perceptual roughness wet: porous ground halfway toward the base factor's gloss, then the film toward water's
float wwLandWetRough( float rough, float2 wet, float por )
{
	float r = rough * lerp( 1.0, lerp( 1.0, landWetK.z, por ), 0.5 * wet.x );
	return lerp( r, 0.05, wet.y );
}
// end of land_wet.glsl
// lane WATERLOOK1 step 14: the Wave run-up row reads the baked water's bodies and wave heights too
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"


// the land distance (u) and its gradient (per u, pointing inland) from the four texels wwLandShoreSmooth reads
float3 wwLandShoreGrad( float2 worldXY )
{
	float2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	int2 t = int2( floor( p ) );
	float2 f = p - float2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + int2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + int2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + int2( 1, 1 ) ) );
	float q = landShoreOn ? landShoreGrid.w : 32.0;
	float2 g = float2( lerp( b - a, d - c, f.y ), lerp( c - a, d - b, f.x ) ) * q / max( landShoreGrid.z, 1.0 );
	return float3( lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * q, g );
}

// Stockdon et al. 2006: the 2% run-up height (m) for Hs h0 (m), period T (s), beach slope b
float wwSwashR2( float h0, float T, float b )
{
	float hl = h0 * 9.81 * T * T / 6.2831853;
	return 1.1 * ( 0.35 * b * sqrt( hl ) + 0.5 * sqrt( hl * ( 0.563 * b * b + 0.004 ) ) );
}

/* (film, foam, wet, the wave's top in m) at world point P with the ground's world normal N, clock t (s). Call it in
 * uniform flow or not: no derivatives. */
float4 wwLandSwash( float3 P, float3 N, float t )
{
	if ( or( or( !landSwashOn, !landShoreOn ), !waterInfoOn ) )
		return (float4)( 0.0 );
	float3 dg = wwLandShoreGrad( P.xy );
	float d = dg.x;
	float nz = clamp( N.z, 0.05, 1.0 );
	float b = clamp( sqrt( 1.0 - nz * nz ) / nz, 0.005, 1.0 );
	float z = d * b * kShoreM;	// m above the still line
	if ( z > 4.0 )
		return (float4)( 0.0 );	// past any run-up (Hs 6 m, 12 s on a 1:1 bank: under 4 m)
	// the water: down the land distance's gradient, one water texel past the line
	float2 Pw = P.xy - dg.yz / max( length( dg.yz ), 1e-4 ) * ( d + waterInfoGrid.z );
	float4 wv = wwShoreWave( Pw );
	if ( wv.w < 0.5 )
		return (float4)( 0.0 );
	float hs = wv.x * kShoreM;
	bool sea = wwWaterInfo( Pw ).w < 0.5;
	float T = sea ? landSwashK.x : clamp( sqrt( 6.2831853 * hs / ( 0.04 * 9.81 ) ), 1.0, 12.0 );
	float om = 6.2831853 / T;
	float L0 = 9.81 * T * T / 6.2831853 / kShoreM;	// u
	bool regular = ( landSwashRed & 4 ) != 0;
	float phi = om * t + ( regular ? 0.0 : 3.0 * ( wwShoreNoise( Pw / ( 2.0 * L0 ) ) - 0.5 ) );
	float u = frac( phi / 6.2831853 );	// 0 as the bore reaches the line
	float env = 1.0;
	if ( !regular ) {	// SURF1's group envelope at the line, at this wave's arrival
		float psi = om / max( landSwashK.y, 1.0 ) * ( t - u * T ) + 6.2831853 * wwShoreNoise( Pw / ( 3.0 * L0 ) + 0.41 );
		env = 0.35 + 0.65 * ( 0.5 + 0.5 * cos( psi ) );
	}
	float R2 = wwSwashR2( hs, T, b );
	float R = R2 * sqrt( env );
	if ( ( landSwashRed & 1 ) != 0 )
		u = 0.5;
	float zt = R * 4.0 * u * ( 1.0 - u );	// the tip
	float edge = max( 0.1 * b, 1e-3 );	// 0.1 m along the ground
	float film = 1.0 - smoothstep( zt - edge, zt, z );
	float foam = 0.0;
	if ( ( landSwashRed & 2 ) == 0 )
		foam = film * exp( -max( zt - z, 0.0 ) / ( landSwashK.w * b ) ) * lerp( 1.0, 0.2, smoothstep( 0.3, 0.7, u ) )
			* wv.y * landSwashK.z * smoothstep( 0.0, 0.05, R );
	float wet = 1.0 - smoothstep( 0.7 * R2, R2, z );
	return float4( film * smoothstep( 0.0, 0.02, R ), foam, wet * smoothstep( 0.0, 0.02, R2 ), R );
}
// end of land_swash.glsl
#include "../weather/rain_wet.hlsli"

#ifndef WW_LANDSPEC_NONE

Texture2DArray<float4> LandLodDiff : register(t59, space0); SamplerState _LandLodDiff_sampler : register(s59, space0);	// lane LANDSPEC3: the 4 x 4 level-4 LOD diffuse chunks round the camera (clamped)
Texture2DArray<float4> LandLodOuter : register(t60, space0); SamplerState _LandLodOuter_sampler : register(s60, space0);
Texture2D<float4> LandLodNoise : register(t61, space0); SamplerState _LandLodNoise_sampler : register(s61, space0);
Texture2D<float4> LandLodDetail : register(t62, space0); SamplerState _LandLodDetail_sampler : register(s62, space0);
/* lane LANDSPEC3: n moved toward the detail normal d in n's own frame, as the game's land PSs (02858 by v, the LOD
 * land 02861 whole): t1 = n x (n x X), t2 = n x t1, target (t1.d, t2.d, n.d) */
float3 landDetailN( float3 n, float2 w, float2 wdx, float2 wdy, float v )
{
	float2 dd = LandLodDetail.SampleGrad( _LandLodDetail_sampler, w * 0.00035, wdx * 0.00035, wdy * 0.00035 ).rg * 2.0 - 1.0;
	float3 d = float3( dd, sqrt( 1.0 - min( dot( dd, dd ), 1.0 ) ) );
	float3 t1 = normalize( cross( n, cross( n, float3( 1.0, 0.0, 0.0 ) ) ) );
	float3 t2 = normalize( cross( n, t1 ) );
	return normalize( lerp( n, float3( dot( t1, d ), dot( t2, d ), dot( normalize( n ), d ) ), v ) );
}
/* lane LANDSPEC4: a splat layer pass moves toward ONE target (d in the vertex frame, as landDetailN at n = z), so the
 * additive passes sum to the blended normal moved once, not each layer toward its own (red perlayer) */
float3 landDetailSplatN( float3 n, float2 w, float2 wdx, float2 wdy, float v )
{
	if ( landLodWin.w > 0.5 )
		return landDetailN( n, w, wdx, wdy, v );
	float2 dd = LandLodDetail.SampleGrad( _LandLodDetail_sampler, w * 0.00035, wdx * 0.00035, wdy * 0.00035 ).rg * 2.0 - 1.0;
	return normalize( lerp( n, float3( -dd, sqrt( 1.0 - min( dot( dd, dd ), 1.0 ) ) ), v ) );
}
#endif
#endif
#ifdef WW_FARGROUND	// lane FARDRAW1: the far ring 0's live ground
#include "cell_farground.hlsli"
#endif

/* lane DECALANGLE: the game's decal angle rule, per pixel. The vertex alpha is dot(unit N, D) at each piece corner
 * (1 where the face itself passes); over the interpolated normal's length it is dot(normalize(N), D). */
#ifdef WW_CELLLIGHTS

#endif

float wwDecalVertexA( float vertexA )
{
	if ( !wwDecalAngle )
		return vertexA;
#ifdef WW_CELLLIGHTS
	// lane DECALANGLE2: N = the drawn (normal-mapped) normal under this pixel, as the game's g-buffer has it
	if ( wwDecalMapped && wwDecalGbufOn && vertexA < 254.5 / 255.0 ) {
		int2 q = int2( gl_FragCoord.xy - wwDecalGbufRect.xy );
		if ( all( (q >= (int2)( 0 )) ) && all( (q < int2( wwDecalGbufRect.zw )) ) ) {
			float4 g = cellCtGbuf.Load( int3(q, 0) );
			float z = cellAoPassOut( float3( 0.0, 0.0, 1.0 ), -ViewDir ).w;
			if ( abs( g.a - z ) <= 0.5 + 0.005 * z ) {	// the surface under it, not a nearer one
				float3 d = normalize( mul(normalMatrix, ( ( C.rgb * 255.0 - 128.0 ) / 127.0 )) );
				return clamp( ( dot( g.xyz, d ) - 0.3 ) / 0.25, 0.0, 1.0 );
			}
		}
	}
#endif
	return clamp( ( vertexA / max( length( btnMatrix[2] ), 1.0e-4 ) - 0.3 ) / 0.25, 0.0, 1.0 );
}

/* lane DECALS1: the game's decal parallax. Height = the set's TX04 (bound as GlowMap, red); the eye ray in the
 * decal's unit uv axes x tan(angle) x scale, linear search from the top in n steps, then one secant step. */
float2 wwDecalPomUv( float2 uv0, float3 P, float3 N, float3 E )
{
	float3 dpx = ddx( P ), dpy = ddy( P );
	float2 dux = ddx( uv0 ), duy = ddy( uv0 );
	float det = dux.x * duy.y - duy.x * dux.y;
	if ( abs( det ) < 1e-12 )
		return uv0;
	float3 T = normalize( ( dpx * duy.y - dpy * dux.y ) / det );	// +u on the surface
	float3 B = normalize( ( dpy * dux.x - dpx * duy.x ) / det );	// +v
	float ez = max( dot( E, N ), 1e-3 );
	float2 ofs = -float2( dot( E, T ), dot( E, B ) ) / ez * wwDecalPom.x;
	float n = min( 72.0 - 56.0 * clamp( dot( N, E ), 0.0, 1.0 ), wwDecalPom.y );
	float stepH = 1.0 / n;
	float layer = 1.0, prevLayer = 1.0, h = 0.0, prevH = 1.0;
	float2 uv = uv0;
	for ( int i = 0; i < 72; i++ ) {
		if ( float( i ) >= n )
			break;
		uv = uv0 + ofs * ( 1.0 - layer );
		h = GlowMap.SampleGrad( _GlowMap_sampler, uv, dux, duy ).r;
		if ( layer < h )
			break;
		prevLayer = layer;
		prevH = h;
		layer -= stepH;
	}
	float d0 = prevLayer - prevH, d1 = layer - h;
	float t = ( d0 - d1 ) != 0.0 ? prevLayer + ( layer - prevLayer ) * d0 / ( d0 - d1 ) : layer;
	return uv0 + ofs * ( 1.0 - t );
}

#ifdef WW_CELLLIGHTS	// lane PERFLOAD1 P7 round 2: the far row's tree-ladder dither (fo4_default.vert), the cards' noise
static float2 farLadder;
#endif

void frag_main()
{
#ifdef WW_CELLLIGHTS
	// finer side keeps noise < w (x = 1 + w), coarser keeps noise >= 1 - w (x = -(1 + w)): impostor_oct.frag's noise
	if ( farLadder.x != 0.0 ) {
		float2 fp = gl_FragCoord.xy + 5.588238 * farFadeFrame;
		float n = frac( 52.9829189 * frac( dot( fp, float2( 0.06711056, 0.00583715 ) ) ) );
		if ( farLadder.x > 0.0 ? n >= farLadder.x - 1.0 : n < 2.0 + farLadder.x )
			discard;
	}
	cellEmitOut = (float4)( 0.0 );
	float wwGrassK = wwGrassFadeK( ViewDir );	// lane GRASSMB1: exactly 1.0 on every non-grass shape
	if ( wwGrassK <= 0.0 )
		discard;
#endif
	float2 offset = texCoord.xy * uvScale + uvOffset;
	if ( wwDecalPom.x > 0.0 ) {	// lane DECALS1: the uv leaving the decal's rect is clipped, as the game's decal pass does
		offset = wwDecalPomUv( offset, -ViewDir, normalize( btnMatrix[2] ), normalize( ViewDir ) );
		if ( or( any( (offset < (float2)( 0.0 )) ), any( (offset > (float2)( 1.0 )) ) ) )
			discard;
	}
#ifdef WW_SUNSHADOW
	/* lane SUNCELL1: the cascades on the SUN only (CSM1 spec 2.8). Every D.rgb below is the sun: D = sqrt(the
	 * linear sun), so D x sqrt(f) is the sun x f in the linear light the tonemap squares back. The macro takes
	 * every later D; tonemap() above still reads the varying (D.a only, unchanged). */
#ifdef WW_CELLLIGHTS
	/* MODULES1 p18: the obscurance pass (probe 20, early 1) returns at the normal and reads no sun: it skips the
	 * cascades and the air here (cellAoEarly 4 = its red, the sun read as before) */
	bool airAoEarly = cellOn && cellProbe == 20 && cellAoEarly == 1;
	float2 airTD = airAoEarly ? float2( 1.0, 0.0 ) : WW_AIR_SUN_TD( -ViewDir );	// MODULES1 G3: (direct T, the soft share)
	float csmF = airAoEarly ? 1.0 : wwSunShadow( -ViewDir ) * airTD.x;	// FOGSUN1: and the air's transmittance
#else
	float csmF = wwSunShadow( -ViewDir ) * WW_AIR_SUN_TD( -ViewDir ).x;	// FOGSUN1: and the air's transmittance
#endif
	float4 Dcsm = float4( D.rgb * sqrt( csmF ), D.a );
#define D Dcsm
#ifdef WW_CELLLIGHTS
	cellDirVis = csmF;	// the exterior cell sun (cellLit) takes the same cascades
	cellAirDif = airTD.y;	// MODULES1 G3: the sun the air scattered down, no cascades
#endif
#else
	// FOGSUN1: no cascades compiled in; the sun still crosses the air
	float2 airTD = WW_AIR_SUN_TD( -ViewDir );	// MODULES1 G3: (direct T, the soft share)
	float airSunT = airTD.x;
	float4 Dair = float4( D.rgb * sqrt( airSunT ), D.a );
#define D Dair
#ifdef WW_CELLLIGHTS
	cellDirVis = airSunT;
	cellAirDif = airTD.y;
#endif
#endif

	float4 baseMap = BaseMap.SampleBias( _BaseMap_sampler, offset, wwMipBias );
#ifdef WW_FARGROUND
	baseMap.rgb = fgGround( cellWorldPos( -ViewDir ), baseMap.rgb );
#endif
#if defined( WW_CELLLIGHTS ) && !defined( WW_LANDSPEC_NONE )
	/* lane LANDSPEC2 (src/features/far-ground/landlodspec.h): the near ground's land-LOD band, as the game's near land PS: v from the
	 * (gliding) blend centre; the albedo toward the level-4 LOD diffuse x the noise, the normal (below) toward the
	 * detail normals. Gradients first: the samples sit in a branch. */
	float2 landBandW = cellWorldPos( -ViewDir ).xy;
	float landBandV = landLod.x > 0.5 && landLod.x < 1.5
		? 1.0 - clamp( ( landLodK.z - distance( landBandW, landLod.yz ) ) * landLodK.w, 0.0, 1.0 ) : 0.0;
	landBandW += landLodTex.xy;
	float2 landWdx = ddx( landBandW );
	float2 landWdy = ddy( landBandW );
	// lane LANDSPEC3: the far LOD ground (02861) takes the noise and the detail normal whole
	float landDetailV = landLod.x > 1.5 ? 1.0 : landBandV;
	if ( landLodTex.z > 0.5 && landDetailV > 0.0 ) {
		float3 noiseK = LandLodNoise.SampleGrad( _LandLodNoise_sampler, landBandW * 0.00025, landWdx * 0.00025, landWdy * 0.00025 ).rgb
			* 3.777778 - 2.006;
		float2 g = ( landBandW - landLodWin.xy ) * ( 1.0 / 16384.0 );
		int2 ci = int2( floor( g ) );
		int nW = int( landLodWin.z );	// lane LANDSPEC4: the window spans the near block (LANDSPEC5: up to 16 x 16)
		int k = ci.x + nW * ci.y;
		if ( landLod.x > 1.5 ) {
			baseMap.rgb *= noiseK;	// lane LANDSPEC4: the far ring 0's live ground too
		} else if ( ci.x >= 0 && ci.y >= 0 && ci.x < nW && ci.y < nW
			&& ( ( int( landLodBits[k >> 6][( k >> 4 ) & 3] ) >> ( k & 15 ) ) & 1 ) != 0 ) {
			float2 f = g - float2( ci );
			const float2 flipV = float2( 1.0, -1.0 ) / 16384.0;
			float3 lod = LandLodDiff.SampleGrad( _LandLodDiff_sampler, float3( f.x, 1.0 - f.y, float( k ) ), landWdx * flipV, landWdy * flipV ).rgb;
			baseMap.rgb = lerp( baseMap.rgb, lod * noiseK, landBandV );
		} else if ( landLodOuter.z > 0.5 ) {	// lane LANDSPEC6: a cell the page does not hold
			float2 go = ( landBandW - landLodOuter.xy ) * ( 1.0 / 16384.0 );
			int2 co = int2( floor( go ) );
			int nO = int( landLodOuter.z );
			int ko = co.x + nO * co.y;
			if ( co.x >= 0 && co.y >= 0 && co.x < nO && co.y < nO
				&& ( ( int( landLodOuterBits[ko >> 6][( ko >> 4 ) & 3] ) >> ( ko & 15 ) ) & 1 ) != 0 ) {
				float2 f = go - float2( co );
				float2 flipV = float2( 1.0, -1.0 ) / 16384.0;
				float3 uvl = float3( f.x, 1.0 - f.y, float( ko ) );
				if ( landLodOuter.w > 1.5 ) {	// lane LANDSPEC8: the chunk's quarter of its packed layer (odd y on top)
					uvl = float3( ( float2( co.x & 1, 1 - ( co.y & 1 ) ) + uvl.xy ) * 0.5,
						float( ( co.x >> 1 ) + ( ( nO + 1 ) >> 1 ) * ( co.y >> 1 ) ) );
					flipV *= 0.5;
				}
				float3 lod = LandLodOuter.SampleGrad( _LandLodOuter_sampler, uvl, landWdx * flipV, landWdy * flipV ).rgb;
				baseMap.rgb = lerp( baseMap.rgb, lod * noiseK, landBandV );
			}
		}
	}
#endif

	float4 color = float4( baseMap.rgb, 1.0 );
	if ( alphaFlags > 0 ) {
		/* The class parameter LIVES in vertex alpha, so the alpha test has to
		 * run on the real value even when the preview is about to replace the
		 * colour -- otherwise a tree card previews as a solid quad and its
		 * silhouette, the thing you are looking at, is gone. Hence the preview
		 * early-out sits AFTER this and not at the top of main(). */
#ifdef WW_CELLLIGHTS
		float	a = wwGrassVertexA( wwDecalVertexA( C.a ) ) * baseMap.a * alpha;
		a *= wwGrassK;	// lane GRASSMB1: the grass fades out through its own alpha test
#else
		float	a = wwDecalVertexA( C.a ) * baseMap.a * alpha;
#endif
		if ( wwDecalAngle && a < 4.0 / 255.0 )	// lane DECALANGLE: the game drops texture alpha x term under 4/255
			discard;
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	if ( lodChannelView != 0 ) {
		float3 v = (float3)( 0.0 );
		if ( lodChannelView == 1 ) {
			/* The index HASHED to a colour, not the raw bytes: consecutive
			 * indices differ by one part in 255 of red and are
			 * indistinguishable, which is exactly the case worth seeing. */
			float idx = floor( C.r * 255.0 + 0.5 ) + floor( C.g * 255.0 + 0.5 ) * 256.0;
			v = frac( sin( ( idx + 1.0 ) * float3( 12.9898, 78.233, 45.164 ) ) * 43758.5453 );
			v = v * 0.8 + 0.2;
		} else if ( lodChannelView == 2 ) {
			v = float3( C.r, C.g, 0.0 );      // the raw 16-bit index bytes
		} else if ( lodChannelView == 3 ) {
			v = C.bbb;                      // ambient occlusion (objects and terrain)
		} else if ( lodChannelView == 4 ) {
			v = C.aaa;                      // A: tree sway / terrain shore proximity
		} else if ( lodChannelView == 5 ) {
			/* Terrain R is a material CLASS id, not a magnitude -- shown hashed
			 * for the same reason the object index is: neighbouring classes
			 * differ by a few parts in 255 and read as one flat grey. */
			float cls = floor( C.r * 255.0 + 0.5 );
			v = frac( sin( ( cls + 1.0 ) * float3( 12.9898, 78.233, 45.164 ) ) * 43758.5453 );
			v = v * 0.8 + 0.2;
		} else if ( lodChannelView == 6 ) {
			v = C.ggg;                      // terrain flow-accumulation wetness
		} else if ( lodChannelView == 8 ) {
			/* The impostor bake's normal sheet: the GEOMETRIC normal in the
			 * view's space, half-packed, flipped for back faces as the lit
			 * path flips it. After the alpha test above, so a leaf card
			 * writes its normal only where it has a leaf. */
			float3 n = normalize( btnMatrix_norm[2] );
			if ( !gl_FrontFacing )
				n = -n;
			v = n * 0.5 + 0.5;
		} else if ( lodChannelView == 9 ) {
			// the impostor bake's height: window z, linear under the ortho projection
			v = (float3)( gl_FragCoord.z );
		} else if ( lodChannelView == 10 ) {
			/* The impostor bake's material channel (docs/LODGEN_IMPOSTOR_SPEC.md).
			 * A shape retargeted to a source .lodm: its third texture, raw --
			 * roughness/metallic/AO or gloss/specular/AO as the file says.
			 * Otherwise the LEGACY pair from the vanilla material, the way the
			 * lit path composes it: R gloss = smoothness x the specular map's
			 * G, G specular = the map's R (the normal's alpha without a map)
			 * x the specular strength. */
			float4 sm = SpecularMap.SampleBias( _SpecularMap_sampler, offset, wwMipBias );
			if ( lodMaskRaw ) {
				v = sm.rgb;
			} else {
				float gl = clamp( specGlossiness, 0.0, 1.0 ) * ( hasSpecularMap ? sm.g : 1.0 );
				float sp = ( hasSpecularMap ? sm.r : NormalMap.SampleBias( _NormalMap_sampler, offset, wwMipBias ).a )
					* ( lodSpecStrength > 0.0 ? lodSpecStrength : 1.0 );
				v = float3( gl, clamp( sp, 0.0, 1.0 ), 1.0 );	// B: neutral AO, one per-pixel law for mixed shapes
			}
		} else if ( lodChannelView == 12 ) {
			/* The impostor bake's colour sheet: the base colour as the source
			 * has it, times the vertex colour, and NOTHING else - the lit path
			 * below tone-maps before it writes, so an "unlit" render of it is
			 * a curved albedo. The consumer lights the card. */
			v = baseMap.rgb * C.rgb;
		} else if ( lodChannelView == 11 ) {
			/* The impostor bake's subsurface mask source. A LOD tree: the
			 * alpha-tested shapes are the leaf cards, the trunk is opaque. A
			 * near tree: every shape is alpha-tested (the bark card too), and
			 * the branch cards are the ones that carry the tree-animation
			 * flag; the bake picks the rule for the model. */
			float leaf = lodMaskByTree ? ( lodTreeAnim ? 1.0 : 0.0 ) : ( alphaFlags > 0 ? 1.0 : 0.0 );
			/* G: the WIND WEIGHT W (IMPOSTORWIND1, sway A): the raw vertex alpha on a
			 * tree-animation shape -- the only wind input the game's tree vertex
			 * shader reads -- and 0 on any other shape, which the game never moves.
			 * The mask reads only R. */
			v = float3( leaf, lodTreeAnim ? rawVertexAlpha : 0.0, 0.0 );
		} else if ( lodChannelView == 13 ) {
			/* The impostor bake's emissive sheet (docs/LODGEN_IMPOSTOR_SPEC.md).
			 * A shape whose GLOW SLOT a source .lodm retargeted reads that
			 * texture raw - and an EMPTY retarget binds black, which is how a
			 * pbr set that names no emissive says it emits nothing.
			 * Otherwise the VANILLA LOD GLOW RULE, measured off the shipped
			 * atlases: a LOD chunk shape carries Own-Emit with a black emissive
			 * colour and no glow slot, so what lights it is its DIFFUSE'S ALPHA
			 * on the opaque shapes (Diamond City's DXT5 atlas: 216,521 of
			 * 262,144 alpha blocks varying, none fully transparent). An
			 * alpha-tested shape spends its alpha on the cut-out instead and
			 * emits nothing.
			 * The EMISSIVE COLOUR is folded in here, because the engine scales
			 * that alpha by it: a chunk shape own-emits with a black colour and
			 * therefore emits nothing, which is the whole answer to "an opaque
			 * source with alpha 255 throughout would light every wall". The
			 * MULTIPLE is not folded in - it can exceed 1 and this sheet is eight
			 * bits - and rides in the set's .lodm as `emissiveScale`. A retargeted
			 * glow slot stays RAW: a source .lodm's emissive is its own picture. */
			if ( lodEmissiveRaw )
				v = GlowMap.SampleBias( _GlowMap_sampler, offset, wwMipBias ).rgb;
			else
				v = ( alphaFlags == 0 ) ? baseMap.rgb * baseMap.a * lodEmissiveColor : (float3)( 0.0 );
		} else {
			v = C.rrr;                      // raw red: water depth
		}
		fragColor = float4( v, 1.0 );
		return;
	}

	float4 normalMap = NormalMap.SampleBias( _NormalMap_sampler, offset, wwMipBias );
	float4 specMap = SpecularMap.SampleBias( _SpecularMap_sampler, offset, wwMipBias );
	float4 glowMap = GlowMap.SampleBias( _GlowMap_sampler, offset, wwMipBias );

	float3 normal;
	if ( hasModelSpaceNormals ) {
		/* A model-space map carries all three components already, in the
		 * model's own axes, so it needs model -> view and NOTHING else.  The
		 * tangent-space path below would instead read the map's two stored
		 * channels as offsets along the mesh's tangent and bitangent -- and a
		 * terrain LOD tile's tangent frame is arbitrary (btdterrain.cpp builds
		 * T = n x worldUp, B = n x T), so the sheet's "up" lands sideways and
		 * the surface shades in blotches.
		 *
		 * Channel order MEASURED on Bethesda's own shipped sheets
		 * (Data/Textures/Terrain/Commonwealth/Commonwealth.16.*_msn.DDS, six
		 * 512x512 tiles, correlated against the heights of the same cells):
		 *   R = EAST  (+x)   corr 0.364 with -dh/dx, 0.001 with -dh/dy
		 *   B = NORTH (+y)   corr 0.422 with -dh/dy, 0.002 with -dh/dx
		 *   G = UP    (+z)   mean 238.6 of 255
		 * Alpha is a constant 255 on every tile and carries nothing.
		 * sk_msn.frag's `.rbg` swizzle is this same order.
		 *
		 * The blue channel is NOT recomputed here: it is real data. */
		float3 msn = normalMap.rgb * 2.0 - 1.0;
		float3 msnN = float3( msn.r, msn.b, msn.g );
#if defined( WW_CELLLIGHTS ) && !defined( WW_LANDSPEC_NONE )
		if ( landLodTex.w > 0.5 && landDetailV > 0.0 )	// lane LANDSPEC3: the far LOD ground's model-space map
			msnN = landDetailN( msnN, landBandW, landWdx, landWdy, landDetailV );
#endif
		normal = normalize( mul(normalMatrix, msnN) );
	} else {
		normal = normalMap.rgb * 2.0 - 1.0;
		// Calculate missing blue channel
		normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
#if defined( WW_CELLLIGHTS ) && !defined( WW_LANDSPEC_NONE )
		if ( landLodTex.w > 0.5 && landDetailV > 0.0 )	// lane LANDSPEC2: toward the detail normal in its own frame
			normal = landSplat ? landDetailSplatN( normal, landBandW, landWdx, landWdy, landDetailV )
				: landDetailN( normal, landBandW, landWdx, landWdy, landDetailV );
#endif
		normal = normalize( mul(normal, btnMatrix_norm) );
	}
	if ( !gl_FrontFacing )
		normal *= -1.0;
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 && ( or( cellAoEarly == 1, cellAoEarly == 4 ) ) ) {	// lane PERF1: the obscurance pass reads the normal and depth only
		fragColor = cellAoPassOut( normal, -ViewDir );
		return;
	}
#endif

	float3 L = normalize(LightDir);
	float3 V = ViewDir_norm;
	float3 R = reflect(-V, normal);
	float3 H = normalize( L + V );

	float NdotL = dot(normal, L);
	if ( hasModelSpaceNormals ) {
		/* A terrain LOD sheet stores steep banks the coarse LOD mesh does not
		 * have, so a texel can face away from the light on a surface that is
		 * plainly in view, and with the camera light it fell to near-black in
		 * hard-edged patches (bungo 2026-09-18). Squared half-Lambert: 1 at
		 * N.L = 1, 0.25 at the terminator, 0 only when facing straight away. */
		float hl = NdotL * 0.5 + 0.5;
		NdotL = hl * hl;
	}
	float NdotL0 = max( NdotL, FLT_EPSILON );
	float NdotH = max( dot(normal, H), FLT_EPSILON );
	float NdotV = max( dot(normal, V), FLT_EPSILON );
	float VdotH = max( dot(V, H), FLT_EPSILON );
	float NdotNegL = max( dot(normal, -L), FLT_EPSILON );

	float3 reflectedWS = mul(R, reflMatrix);

	float3 albedo = baseMap.rgb * ( wwDecalAngle && wwDecalMapped ? (float3)( 1.0 ) : C.rgb );	// lane DECALANGLE2: rgb is D there
	float3 diffuse = A.rgb + D.rgb * NdotL0;
	if ( greyscaleColor )
		albedo = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( baseMap.g, paletteScale * ( wwDecalAngle && wwDecalMapped ? 1.0 : C.r ) ), 0.0 ).rgb;

	// Emissive
	float3 emissive = (float3)(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;

		if ( hasGlowMap ) {
			emissive *= glowMap.rgb;
		}
	}

	// Specular — GGX + Smith + Schlick, matching the material editor's BRDF:
	//   spec = D_GGX(NoH, a) * G1(NoL,k) * G1(NoV,k) * F / (4 NoL NoV)
	// with a = rough^2 and k = (rough+1)^2/8.
	float g = 1.0;
	float s = 1.0;
	float smoothness = clamp( specGlossiness, 0.0, 1.0 );
	if ( hasSpecularMap ) {
		// G is gloss, R is the specular mask (measured, not assumed). A flat
		// white G correctly falls back to the material's scalar smoothness.
		g = specMap.g;
		s = specMap.r;
		smoothness = g * smoothness;
	}
	// Specular is no longer gated on hasSpecularMap: a material with only the
	// scalar Smoothness/Specular Strength authored used to render completely
	// matte, which is not what the game does with it.
	float specMask = s * specStrength;
#if defined( WW_CELLLIGHTS ) && !defined( WW_LANDSPEC_NONE )
	/* lane LANDSPEC1 (src/features/far-ground/landlodspec.h): the game's LOD land gloss/spec on the far ground, and on the near ground
	 * lerped in from 1.7 to 2.35 cells off the camera cell's centre (its land-LOD blend) */
	float landLodV = landLod.x > 1.5 ? 1.0 : landBandV;	// lane LANDSPEC2: the band's v, worked out with the albedo
	smoothness = lerp( smoothness, landLodK.x, landLodV );
	specMask = lerp( specMask, landLodK.y, landLodV );
#elif defined( WW_CELLLIGHTS )
	const float landLodV = 0.0;
#endif
#ifdef WW_CELLLIGHTS
	if ( cellOn && ( or( landWetOn, landSwashOn ) ) ) {	// lane WATERLOOK1 steps 13-14: the Wet shore and Wave run-up rows
		float3 Pl = cellWorldPos( -ViewDir ), Nl = cellWorldDir( btnMatrix_norm[2] );
		float2 wet = wwLandWet( Pl, Nl );
		float4 sw = wwLandSwash( Pl, Nl, landSwashTime );
		wet = max( wet, sw.zx );
		float por = landShoreTerrain ? 1.0 : 0.0;
		albedo *= pow( wwLandWetBase( wet.x, por ), 1.0 / 2.2 );	// the albedo is texture gamma here
		smoothness = 1.0 - wwLandWetRough( 1.0 - smoothness, wet, por );
		specMask = lerp( specMask, max( specMask, 0.5 ), wet.y );
		albedo = lerp( albedo, (float3)( 0.9 ), sw.y );	// the run-up's foam (texture gamma)
	}
	if ( cellOn && rainWet.x > 0.0 ) {	// lane [reference game]WEATHER1: rain wetness, [reference game]'s law; F0 is 0.04 here, no metal
		float rw = wwRainWetW( smoothness, 0.0 ) * wwRainWetExposed( cellWorldPos( -ViewDir ), cellWorldDir( btnMatrix_norm[2] ) );	// [reference game]WEATHER2: dry under cover
		albedo *= pow( 1.0 - 0.8 * rw, 1.0 / 2.2 );	// the albedo is texture gamma here
		smoothness = wwRainWetSmooth( smoothness, rw );
	}
#endif

	// FO4 encodes NO metallicity, so F0 is the dielectric constant and _s.R
	// stays what it is authored as — a specular MASK, not a metalness channel.
	// The old hardcoded F0 of 0.2 was ~5x too reflective for a dielectric; the
	// editor's default is 0.04.
	float rough = clamp( 1.0 - smoothness, 0.02, 1.0 );
	float alphaR = rough * rough;
	float kSmith = ( rough + 1.0 ) * ( rough + 1.0 ) * 0.125;
	// fresnelSchlick honours the material's authored Fresnel Power rather than
	// hardwiring ^5 — that field exists in FO4 and the editor has no equivalent.
	float F = fresnelSchlick( VdotH, 0.04 );
	float3 spec = (float3)( D_GGX( NdotH, alphaR )
	                  * G1( NdotL0, kSmith ) * G1( NdotV, kSmith )
	                  * F / max( 4.0 * NdotL0 * NdotV, 0.001 ) )
	            * specMask * NdotL0 * D.rgb * specColor;
	float ambF0 = 0.04;
	float ambMask = specMask;
	if ( !editorBrdf ) {
		/* lane VANILLADEF1: the upstream law (and the game's light specular, cell_lights.glsl): normalized Phong,
		 * n = 2^(gloss x 10 + 1), F0 0.2, no specular without a specular map, no (1 - F) on the diffuse */
		F = 0.0;
		ambF0 = 0.2;
		spec = (float3)( 0.0 );
		if ( hasSpecularMap )
			spec = TorranceSparrow( NdotL0, NdotH, NdotV, VdotH, (float3)( specMask ), exp2( smoothness * 10.0 + 1.0 ), 0.2 )
			       * NdotL0 * D.rgb * specColor;
		else
			ambMask = 1.0;
	}
	float3 specSun = spec;	// lane SUNCELL1: the sun's specular before the cube (the exterior cell sun keeps it)

	// Environment
	float specSky = 1.0;	// lane SPECOCC1: outdoors the cube reflection keeps the sky visibility along R (cell_lights.glsl)
#ifdef WW_CELLLIGHTS
#ifdef CELL_GI_ONE
	/* DDGIREADPERF1: the one volume read, at the P and N every later site asks for (cellSpecSky, cellGiSkyK, cellLit's
	 * cellGiE, the probe and pass views; the PROBEVIEW1 pass red's up normal included); they then take its answer */
	if ( cellOn )
		cellGiFirst( cellWorldPos( -ViewDir ), cellWorldDir( normal ),
		             cellPass > 0 && cellPass != 6 && cellProbe == 0 && ( cellPassRed & 2 ) != 0 ? float3( 0.0, 0.0, 1.0 ) : cellWorldDir( normal ),
		             cellWorldDir( R ), rough );
	cellGiOneRead = true;
#endif
	if ( cellOn )
		specSky = cellSpecSky( cellWorldPos( -ViewDir ), cellWorldDir( normal ), cellWorldDir( R ), rough );
#endif
	float4 cube = CubeMap.SampleLevel( _CubeMap_sampler, reflectedWS, 8.0 - smoothness * 8.0 );
	float4 env = EnvironmentMap.Sample( _EnvironmentMap_sampler, offset );
	float3 envSpec = (float3)( 0.0 );	// lane PRTPGI: the reflection before the viewport light, for the cell path
	if ( hasCubeMap ) {
		cube.rgb *= envReflection * specStrength;
		if ( hasEnvMask ) {
			cube.rgb *= env.r;
		} else {
			cube.rgb *= s;
		}

		envSpec = cube.rgb;
		spec += cube.rgb * diffuse * specSky;
	}
#ifdef WW_CELLLIGHTS
	// lane CUBE1: the reflection the cell path adds before its light, linear: the game's term, or the old one
	// (squared, x Lambert, the default cube too) under WW_CELL_LIT_RED=cubeold
	float3 cubeK = envSpec * envSpec;
	if ( cellOn && ( cellRed & 512 ) == 0 )
		cubeK = hasCubeMap ? cellCubeGame( CubeMap, _CubeMap_sampler, reflectedWS, cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ),
		                                   cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), envReflection, -ViewDir )
		                   : (float3)( 0.0 );
	// lane REFLPROBE1: the placed probes take the material cube's place where they cover (cell_reflprobe.glsl)
	if ( cellOn && hasCubeMap && ( cellRed & 512 ) == 0 )
		cubeK *= cellReflProbe( GreyscaleMap, _GreyscaleMap_sampler, -ViewDir, R, cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ),
		                        cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), envReflection );
	// lane SSR1: the screen-space reflection over the cube term, by the march's confidence (cell_ssr.glsl)
	if ( cellOn )
		cubeK = cellSsrMix( cubeK, lerp( cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ), landLodK.x, landLodV ),
		                    lerp( cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), landLodK.y, landLodV ), envReflection );
	cubeK *= specSky;	// lane SPECOCC1: after the SSR mix, as FO4CS scales its reflection after the SSR blend
#endif

	float3 backlight = (float3)(0.0);
	if ( backlightPower > 0.0 ) {
		backlight = albedo * NdotNegL * clamp( backlightPower, 0.0, 1.0 );

#ifdef WW_CELLLIGHTS
		/* MODULES1 R6-1: under the cell's sun the back light is the sun through the leaf, in the sun's own unit and
		 * in linear light (cellLit adds albedo x sun x cellBackK x the sun's visibility). The old law added it to the
		 * ROOT of the lit colour in the viewport light's 0..1 unit: (root + e)^2 took a cross term and no exposure,
		 * a back-lit far crown (no cascades there) came out as bright as a white wall facing the sun.
		 * Red WW_CELL_LIT_RED=backglow. A far placement's baked sky visibility stands in for the crown's own shade. */
		if ( cellOn && cellHasDir && ( cellRed & 2048 ) == 0 ) {
			float bk = NdotNegL * clamp( backlightPower, 0.0, 1.0 );
			cellBackK = bk * bk * max( wwFarSkyVis, 0.0 );
		} else
#endif
		emissive += backlight * D.rgb;
	}

	float3 rim = (float3)(0.0);
	if ( hasRimlight ) {
		rim = (float3)(pow((1.0 - NdotV), rimPower));
		rim *= smoothstep( -0.2, 1.0, dot(-L, V) );

		//emissive += rim * D.rgb * specMask;
	}

	// Diffuse
	float diff = OrenNayarFull( NdotL, dot(normal, V), dot(L, V), 1.0 - smoothness );
	diffuse = (float3)(diff);

	float3 soft = (float3)(0.0);
	float wrap = NdotL;
	if ( or( hasSoftlight, subsurfaceRolloff > 0.0 ) ) {
		wrap = (wrap + subsurfaceRolloff) / (1.0 + subsurfaceRolloff);
		soft = albedo * max( 0.0, wrap ) * smoothstep( 1.0, 0.0, sqrt(diff) );

		diffuse += soft;
	}

	if ( hasTintColor ) {
		albedo *= tintColor;
	}

	// Diffuse. (1 - F) is the editor's energy conservation: light reflected by
	// the specular lobe is no longer also counted as diffuse, so glancing angles
	// stop reading brighter than the material can physically be. The lobe shape
	// stays Oren-Nayar rather than the editor's Lambert — it is roughness-aware
	// and is what the rest of the FO4 path is tuned against.
	color.rgb = diffuse * albedo * D.rgb * ( 1.0 - F );
	// Ambient
	color.rgb += A.rgb * albedo;
	// Specular
	color.rgb += spec;
	color.rgb += A.rgb * ambMask * fresnelSchlick( VdotH, ambF0 ) * (1.0 - NdotV) * D.rgb;
	// Emissive
	color.rgb += emissive * glowScaleSRGB;
#ifdef WW_CELLLIGHTS
#if defined( WW_CELL_GI_MARCH ) && defined( WW_CELL_GI_MARCH_ONCE )
	// lane GIBLACKLINK1: the march red's one GI read, at the (P, N) every later caller passes (cellPassOut's nonormal red: up)
	if ( cellOn && cellGiOn )
		cellGiMarchPrime( cellWorldPos( -ViewDir ), cellPass > 0 && cellPass != 6 && cellProbe == 0 && ( cellPassRed & 2 ) != 0
		                                               ? float3( 0.0, 0.0, 1.0 ) : cellWorldDir( normal ) );
#endif
	// lane SKY1: outdoors the bounce grid's sky stands in for the weather's unshadowed ambient (0 elsewhere)
	/* lane SUNCELL1: an exterior with the weather's sun (cellHasDir) hands cellLit the ambient alone; the sun's
	 * diffuse and the cube are the game's terms there, its specular the viewport's (cellExtSpec, linear) */
	if ( cellOn && !cellInterior && cellHasDir ) {
		// lane BOUNCE1: the sun's specular x the imagespace Sunlight Scale, as its diffuse (cellDirColor); sqrt space here
		float3 sx = specSun * sqrt( max( cellSunScale, 0.0 ) ) + A.rgb * specMask * fresnelSchlick( VdotH, 0.04 ) * ( 1.0 - NdotV ) * D.rgb;
		cellExtSpec = sx * sx;
		/* lane SUNCELL1: the ambient is the weather's directional ambient (DALC, the hour's keys) through the
		 * interior's law (cellAmbient), not the flat NAM0 Ambient the viewport carries */
		float3 Ax = A.rgb;
		if ( cellExtDalc )
			Ax = sqrt( cellAmbient( cellWorldDir( normal ), cellWorldPos( -ViewDir ) ) );
		cellExtAmb = Ax * Ax;
		color.rgb = Ax * albedo;
		/* lane GISHADE1: the grid's sky replaces the share k of THIS ambient (the weather's DALC) in linear light, as
		 * the PBR program does: the ambient keeps (1 - k) of itself, so its colour here (sqrt of linear) takes
		 * sqrt(1 - k), and the cube's ambient (cellExtAmb) keeps (1 - k) too. The old law (red WW_CELL_GISHADE_RED=sqrt)
		 * took k x the flat NAM0 A off in sqrt space: (Ax - k A)^2 stayed lit beside the grid's sky (k = 1, open sky
		 * facing up: +23% blue on CommonwealthClear's day), (1 - k)^2 where the grid fades out */
		if ( cellGiSky && ( cellRed & 131072 ) == 0 ) {
			float k = cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) );
			color.rgb *= sqrt( 1.0 - k );
			cellExtAmb *= 1.0 - k;
		}
		/* lane FIX1101A lodch: the share of the weather's ambient the GI did not replace takes the far placement's
		 * baked sky visibility (src/features/far-lod/lodchsky.h: the byte read as the GI's share, so near (GI) and far (bake) agree at
		 * the last cascade edge). Exactly 1 on every other shape: nothing changes there */
		if ( wwFarSkyVis < 0.9999 ) {
			color.rgb *= sqrt( max( wwFarSkyVis, 0.0 ) );
			cellExtAmb *= max( wwFarSkyVis, 0.0 );
		}
	}
	if ( cellOn && cellGiSky && ( cellRed & 131072 ) == 0 && !( !cellInterior && cellHasDir ) ) {
		// lane GISHADE1: without the weather's sun the colour carries the flat A x albedo among its other terms: keep
		// sqrt(1 - k) of that ambient (linear (1 - k)), the rest as drawn
		float k = cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) );
		color.rgb -= ( 1.0 - sqrt( 1.0 - k ) ) * A.rgb * albedo;
	} else if ( cellOn && cellGiSky && ( cellRed & 131072 ) != 0 )
		color.rgb -= cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) ) * A.rgb * albedo;
	float3 cellPreLit = color.rgb;	// lane SSGI2: an exterior without the sun adds the glow before cellLit's root
	// lane PRTP3: the cell's own lights (src/gl/celllights.h)
	// lane GAPFIX1: the game's deferred lights read only the spec scale (pool1 findings 5); blended draws are forward and keep the colour
	cellSpecTint = ( or( ( alphaFlags & 8 ) != 0, ( cellRed & 262144 ) != 0 ) ) ? specColor : (float3)( 1.0 );	// WW_CELL_SPEC_RED=tint
	if ( cellOn )
		color.rgb = cellLit( color.rgb, albedo, normal, -ViewDir, V, specMask, cellSpecTint, alphaR, kSmith,
		                     emissive * glowScaleSRGB, cubeK );
	float3 cellSsrScene = color.rgb * color.rgb;	// lane SSR1: what the reflections' march samples (probe 60)
	if ( cellOn && cellProbe == 60 ) {
		// lane SSGI2: the glow's share of that (cellLit adds it after the root; c^2 - (c - e)^2), coverage in a
		float3 e = max( emissive * glowScaleSRGB, (float3)( 0.0 ) );
		float3 c = ( !cellInterior && !cellHasDir ) ? cellPreLit : color.rgb;
		float3 c0 = max( c - e, (float3)( 0.0 ) );
		float3 em = clamp( c * c - c0 * c0, (float3)( 0.0 ), cellSsrScene );
		cellEmitOut = float4( em, dot( em, float3( 0.2126, 0.7152, 0.0722 ) ) > 1.0e-6 ? 1.0 : 0.0 );
	}
	// lane AO1: the ambient obscurance on the whole lit colour (linear, so its root here), before the fog
	if ( cellOn )
		color.rgb *= sqrt( cellAoShared( wwFarAo ) );	// MODULES1 round 10: near (screen) and far (baked) through one rule
#endif

#ifdef WW_DV
	float3 wwDvLit = color.rgb * color.rgb;	// lane DEBUGVIEW1: the lit colour before the fog
#endif
	// lane FOG1 (Lookdev weather fog): this colour is sqrt of linear light (tonemap squares it),
	// so the fog runs on its square, before the exposure and the curve
#ifdef WW_FOG
	if ( or( fogOn, wwApOn() ) )	// lane SKYA: the sky's aerial perspective with the fog off too
		color.rgb = sqrt( max( wwFog( color.rgb * color.rgb, -ViewDir ), (float3)( 0.0 ) ) );
#endif
#ifdef WW_CELLLIGHTS
	// lane IMGS1: the cell's imagespace in place of the viewer's curve; probe 6 writes the linear colour raw
	float3 cellHdr = color.rgb * color.rgb;
	if ( cellOn && cellIsOn )
		color.rgb = cellIsLinear ? cellHdr : cellImageSpace( color.rgb );	// lane HDR1: linear into the HDR frame
	else
#endif
	color.rgb = tonemap( color.rgb );

	if ( doRefraction ) {
		// sample the already-rendered scene behind the shape, displaced by the
		// (normal-mapped) surface normal - glass / heat-haze preview. The base
		// texture of a refraction mesh is a distortion/noise map (not a diffuse
		// colour), so it must NOT tint the result: show the clean refracted
		// background. (Per-texel alpha blending garbled the render because the
		// base texture is not opacity, so it is deliberately not applied here.)
		float2	suv = ( gl_FragCoord.xy - float2( viewportDimensions.xy ) ) / float2( viewportDimensions.zw );

		/* Every route that scales the effect, multiplied together:
		 *
		 *  - normal.xy is the eye-space shading normal, so the NORMAL MAP is
		 *    what decides where each texel of the background is fetched from;
		 *    a flat-normal region samples straight through and shows the scene
		 *    unchanged.
		 *  - refractionStrength is the property's Refraction Strength, static
		 *    or driven per frame by a BSLightingShaderPropertyFloatController
		 *    on Controlled Variable 0.
		 *  - C.a is the effective per-vertex alpha (1.0 when the mesh has no
		 *    vertex colours, because vertexColorOverride replaces it), so an
		 *    authored alpha-0 region distorts nothing and alpha-1 distorts
		 *    fully.
		 */
		float	rStrength = clamp( refractionStrength, 0.0, 1.0 ) * clamp( C.a, 0.0, 1.0 );

		/* Maximum displacement is a fraction of the viewport HEIGHT.
		 *
		 * Reading Refraction Strength as a fraction of the whole viewport threw
		 * the source hundreds of pixels into unrelated empty background and drew
		 * a giant dark silhouette; capping it at 8 screen pixels fixed that and
		 * then left nothing to see, because 8 px of a mostly flat backdrop is
		 * invisible and 8 px means something different on every monitor. A
		 * fraction of the height keeps the distortion LOCAL, and keeps it
		 * looking the SAME at every resolution — which is what
		 * resolution-independent has to mean for a screen-space effect.
		 *
		 * The x term converts that height fraction into u, so the displacement
		 * is isotropic instead of being stretched by the aspect ratio.
		 */
		const float refractionMaxScreenFraction = 0.05;
		float2	roffs = normal.xy * rStrength * refractionMaxScreenFraction
			* float2( float( viewportDimensions.w ) / float( max( viewportDimensions.z, 1 ) ), 1.0 );
		float3	bg = RefractionSrc.Sample( _RefractionSrc_sampler, clamp( suv + roffs, (float2)( 0.001 ), (float2)( 0.999 ) ) ).rgb;
		color.rgb = bg;
		color.a = 1.0;
	}

#ifdef WW_CELLLIGHTS
	/* MODULES1 M2 (bungo 11-10, decal_tall_wall / decal_unfogged_wall: the whole grime rectangle showed, dark in the
	 * sun, light in the fog). A multiply decal's material (GrimeSubtle01.BGSM: blend DestColor x Zero) multiplies the
	 * ALBEDO under it in the game's g-buffer, before any light. Drawn lit and fogged here, its output multiplied the lit
	 * frame by the decal's own light: the rectangle took the alley's darkness twice, and the fog's colour once. Now it
	 * writes the albedo factor alone, its texture alpha as the coverage (GrimeSubtle01_d: rgb mean 31/255, alpha mean
	 * 12/255: the dark rgb counts only where the alpha says grime), gamma-2 albedo -> linear: squared, pushed to neutral
	 * by the fog's transmittance at the pixel (the fog's own affine split: wwFog(1) - wwFog(0)), so the decal fades
	 * with the wall under it. */
	if ( cellOn && wwMulDecal != 0 ) {
		float m = wwMulDecal == 2 ? 2.0 : 1.0;	// the blend doubles a 2x decal: neutral grey is 0.5
		float cov = clamp( wwDecalVertexA( C.a ) * baseMap.a * alpha, 0.0, 1.0 );
		float3 k = clamp( lerp( (float3)( 1.0 / m ), albedo, cov ) * m, (float3)( 0.0 ), (float3)( m ) );
		float3 F = k * k;
#ifdef WW_FOG
		if ( or( fogOn, wwApOn() ) ) {
			float3 f0 = wwFog( (float3)( 0.0 ), -ViewDir );
			float3 T = clamp( wwFog( (float3)( 1.0 ), -ViewDir ) - f0, (float3)( 0.0 ), (float3)( 1.0 ) );
			F = lerp( (float3)( 1.0 ), F, T );
		}
#endif
		color.rgb = ( cellIsOn && cellIsLinear ? F : sqrt( F ) ) / m;
	}
#endif
	fragColor = color;
#ifdef WW_CELLLIGHTS
	/* lane FO4CELLGI1: the probe and pass views (cellProbeOut, cellPassOut) compile only under the loader's WW_CELL_VIEWS,
	 * added while a probe or a Pass is asked (wwCellViewsShaderDefine; red WW_CELL_VIEWS_RED=1 always), in the programs with
	 * the GI volume read: compiled in, never run, they set fo4_cell's light-loop occupancy there (300 lights offline: 50 ms
	 * with them, 40 without). Without the volume the program keeps them (its machine code as before) */
#if defined( WW_CELL_VIEWS ) || !defined( WW_CELL_RTDDGI )
	if ( cellOn && cellProbe > 0 )
		fragColor = cellProbe == 6 ? float4( cellHdr * cellProbeGain, color.a ) : float4( cellProbeOut( normal, -ViewDir, alphaR, kSmith ), 1.0 );
#else
	if ( cellOn && cellProbe > 0 )	// 6 the measuring pass; 20, 60 and 61 (the AO and SSR passes) write over it below
		fragColor = cellProbe == 6 ? float4( cellHdr * cellProbeGain, color.a ) : float4( cellWorldDir( normal ) * 0.5 + 0.5, 1.0 );
#endif
	if ( cellOn && cellProbe >= 50 && cellProbe <= 53 )	// lane CUBE1
		fragColor = float4( cellProbe == 50 ? clamp( cubeK * 0.25, 0.0, 1.0 ) : cellCubeProbe( offset, normal ), 1.0 );
	if ( cellOn && cellProbe == 66 )	// lane REFLPROBE1: the boxes' weight, the camera probe's share, the added light
		fragColor = float4( cellReflW, cellReflFall, clamp( dot( cellReflAdd, (float3)( 1.0 / 3.0 ) ), 0.0, 1.0 ), 1.0 );
	if ( cellOn && cellProbe == 80 )	// lane GICAL1 (80; 81-89 kept for it): the albedo as drawn, texture gamma space
		fragColor = float4( clamp( albedo, 0.0, 1.0 ), 1.0 );
	if ( cellOn && cellProbe == 81 )	// lane GICAL1: the same, blends and decals composited over it with their own alpha
		fragColor = float4( clamp( albedo, 0.0, 1.0 ), color.a );
#if defined( WW_CELL_VIEWS ) || !defined( WW_CELL_RTDDGI )
	if ( cellOn && cellPass > 0 && cellProbe == 0 )	// lane PROBEVIEW1: the PRTP band's Pass
		fragColor = float4( cellPassOut( cellWorldPos( -ViewDir ), cellWorldDir( normal ) ), 1.0 );
#endif
#endif
#ifdef WW_FOG
	float3 fogProbeOut;
	if ( wwFogProbe( -ViewDir, fogProbeOut ) )
		fragColor = float4( fogProbeOut, 1.0 );
#endif
#ifdef WW_SUNSHADOW
	float3 csmProbeOut;	// lane SUNCELL1: the CSM1 probe views (WW_CSM_PROBE) on the cell-lit legacy draw
	if ( wwSunShadowProbe( -ViewDir, fragColor.rgb, csmProbeOut ) )
		fragColor = float4( csmProbeOut, 1.0 );
#endif
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 ) {
		fragColor = cellAoPassOut( normal, -ViewDir );	// lane AO1
		if ( cellAoEarly == 2 )
			fragColor.w *= 1.001;	// lane PERF1 red: full shading runs (the old cost), and the gate must see the mark
	}
	if ( cellOn && ( or( cellProbe == 60, cellProbe == 61 ) ) )
		fragColor = cellSsrProbeOut( cellSsrScene );	// lane SSR1
#endif
#ifdef WW_DV
	if ( cellOn && wwDebugView != 0 ) {	// lane DEBUGVIEW1: one pass or term in place of the pixel (debug_view.glsl)
		float dvSh = 1.0, dvCl = 1.0;
#ifdef WW_SUNSHADOW
		dvSh = wwSunShadowGeo( -ViewDir );
		float3 dvCi = ussCloudDvInfo( -ViewDir );	// SEAMCLOUD1: T, the height share, the far share
		dvCl = dvCi.x;
		wwDvCloud = dvCi.yz;
#endif
		float3 dvT = (float3)( 1.0 ), dvIn = (float3)( 0.0 );
#ifdef WW_FOG
		if ( or( fogOn, wwApOn() ) ) {
			dvIn = max( wwFog( (float3)( 0.0 ), -ViewDir ), (float3)( 0.0 ) );
			dvT = clamp( wwFog( (float3)( 1.0 ), -ViewDir ) - dvIn, 0.0, 1.0 );
		}
#endif
		float3 dvE = emissive * glowScaleSRGB;
		fragColor = float4( wwDvSurface( albedo * albedo, cellWorldDir( normal ), smoothness, specMask, wwFarAo, cellAoFactor(),
			dvSh, dvCl, dvE * dvE, dvT, dvIn, wwDvLit, cellHdr, length( ViewDir ) * length( cellRow[0].xyz ), wwDvOwner ), fragColor.a );
	}
#endif
	/* lane TERRBLEND1: a blended-ground pass (src/cellsplat.h). The vertex alpha
	 * is this pass's SHARE of the engine's weighted sum: an opaque base is scaled
	 * by it, a layer is ADDED at it (SRC_ALPHA, ONE). Never the texture's alpha, and
	 * the RAW vertex alpha: vertexColorOverride may force C.a to 1. */
	/* water2fix: never over the data passes -- the obscurance pass (20: normal + view depth) and the
	 * reflections' scene pass (60/61): the ground wrote depth 1 there and the water SSR march died on it */
	bool splatData = false;
#ifdef WW_CELLLIGHTS
	splatData = cellOn && ( or( or( cellProbe == 20, cellProbe == 60 ), cellProbe == 61 ) );
#endif
	if ( landSplat && !splatData ) {
		/* lane ALLON2 (bungo's thin dark lines along the ground's edges, 21.png): with MSAA a pixel on the
		 * OUTER edge of a layer's quads is shaded once at its centre, which lies outside every triangle of
		 * that layer, so every input is extrapolated past the edge -- the share below zero, and the lit
		 * colour can go non-finite. The blend then adds NaN x 0 = NaN and the resolve drops the whole
		 * pixel to black: one dark render pixel along every place a layer's quads stop. A share at or
		 * below zero adds nothing, so it is written as exactly nothing; a non-finite colour likewise.
		 * Red WW_TERRBLEND_RED=edgepass: the old output. Probe WW_TERRBLEND_PROBE=edge: such pixels green. */
		float	w = rawVertexAlpha;
		bool	bad = or( or( or( any( isnan( fragColor.rgb ) ), any( isinf( fragColor.rgb ) ) ), isnan( w ) ), isinf( w ) );
		if ( landSplatDbg == 2 ) {
			fragColor = bad ? float4( 0.0, 1.0, 0.0, 1.0 ) : ( ( alphaFlags & 8 ) != 0 ? (float4)( 0.0 ) : float4( 0.0, 0.0, 0.0, 1.0 ) );
		} else if ( landSplatDbg == 0 && ( or( bad, ( ( alphaFlags & 8 ) != 0 && !( w > 0.0 ) ) ) ) ) {
			fragColor = ( alphaFlags & 8 ) != 0 ? (float4)( 0.0 ) : float4( 0.0, 0.0, 0.0, 1.0 );
		} else if ( ( alphaFlags & 8 ) != 0 )
			fragColor.a = clamp( rawVertexAlpha, 0.0, 1.0 );
		else
			fragColor = float4( fragColor.rgb * clamp( rawVertexAlpha, 0.0, 1.0 ), 1.0 );
	}
#ifdef WW_CELLLIGHTS
	/* lane PERFLOAD1 P8: the LOD preview's tint (lodpreview.cpp's colours): the row's level code y (2 rep0 .. 5 rep3), its
	 * luminance kept; a row mid ladder fade (x != 0) the band's magenta. LODLIVE1: code 1 = a full mesh, white */
	if ( farLadder.y > 0.5 ) {
		float3 lt = farLadder.x != 0.0 ? float3( 1.0, 0.2, 1.0 ) : farLadder.y < 1.5 ? (float3)( 0.92 ) : farLadder.y < 2.5 ? float3( 0.2, 0.85, 0.25 )
			: farLadder.y < 3.5 ? float3( 0.95, 0.9, 0.15 ) : farLadder.y < 4.5 ? float3( 1.0, 0.55, 0.1 ) : float3( 0.95, 0.15, 0.15 );
		fragColor.rgb = lt * ( 0.25 + 0.75 * clamp( dot( fragColor.rgb, float3( 0.2126, 0.7152, 0.0722 ) ), 0.0, 1.0 ) );
	}
#endif
#ifdef WW_FAR_SPLAT	// lane FARLOOK1101: the splat pass blends at the row's true coverage (glBlendFuncSeparate keeps dst alpha)
	fragColor.a = farSplatA;
	cellEmitOut.a *= farSplatA;
#endif
}
// end of fo4_default.frag

// the GLSL's macros named after a varying end with its main()
#undef D

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(12) float rawVertexAlpha : TEXCOORD12;
    WW_LOC(15) float wwFarSkyVis : TEXCOORD15;
    WW_LOC(14) float wwFarAo : TEXCOORD14;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
#if defined(WW_CELLLIGHTS)
    WW_LOC(13) nointerpolation float2 farLadder : TEXCOORD13;
#endif
    float4 gl_FragCoord : SV_Position;
    bool gl_FrontFacing : SV_IsFrontFace;
#if defined(WW_BINDLESS) && !(defined(WW_BL_VERT))
    nointerpolation uint wwBlRow : TEXCOORD0;
#endif
#if defined(WW_FAR_SPLAT)
    nointerpolation float farSplatA : TEXCOORD10;
#endif
};

struct SPIRV_Cross_Output
{
#if defined(WW_BINDLESS) && defined(WW_BL_VERT)
    uint wwBlRow : SV_Target0;
#endif
    float4 fragColor : SV_Target0;
#if defined(WW_CELLLIGHTS)
    float4 cellEmitOut : SV_Target1;
#endif
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
#if defined(WW_BINDLESS) && !(defined(WW_BL_VERT))
    wwBlRow = stage_input.wwBlRow;
#endif
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    A = stage_input.A;
    C = stage_input.C;
    D = stage_input.D;
    rawVertexAlpha = stage_input.rawVertexAlpha;
    wwFarSkyVis = stage_input.wwFarSkyVis;
    wwFarAo = stage_input.wwFarAo;
#if defined(WW_FAR_SPLAT)
    farSplatA = stage_input.farSplatA;
#endif
    btnMatrix = stage_input.btnMatrix;
    reflMatrix = stage_input.reflMatrix;
#if defined(WW_CELLLIGHTS)
    farLadder = stage_input.farLadder;
#endif
    gl_FrontFacing = stage_input.gl_FrontFacing;
    frag_main();
    SPIRV_Cross_Output stage_output;
#if defined(WW_BINDLESS) && defined(WW_BL_VERT)
    stage_output.wwBlRow = wwBlRow;
#endif
    stage_output.fragColor = fragColor;
#if defined(WW_CELLLIGHTS)
    stage_output.cellEmitOut = cellEmitOut;
#endif
    return stage_output;
}
