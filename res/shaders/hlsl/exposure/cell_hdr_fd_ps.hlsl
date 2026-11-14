// HLSL source of features/exposure/cell_hdr_fd.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ExposureBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
static float gl_FragDepth;
static int gl_SampleID;
/* lane PERF2 red (WW_CELL_HDR_EARLY_RED=1): cell_hdr.frag as it was, its depth written per sample (no early tests).
 * lane HDR1: the cell view's one tone map (src/features/exposure/cellhdr.h). The main draw went into a linear float frame;
 * this full-screen pass draws it back into the view, sample for sample:
 *   hdrMode 0  the pixels a cell program or a cell effect wrote last: the cell's imagespace (the bloom added
 *              once, the exposure, the curve, the grade, the LUT) on the summed linear light, as the game runs
 *              its imagespace once on its HDR target
 *   hdrMode 1  the pixels any other program wrote last: the value as it was written
 *   hdrMode 2  (lane SKYHOLE1) the pixels of mode 0 no opaque draw reached (depth at the far plane): a cell
 *              effect over the sky, composited over the backdrop by its coverage instead of replacing it
 *   hdrMode 3  (lane BLOOM1b, the FO4CS bloom only) every pixel mode 0 does not take: the glow the bloom spreads
 *              there, added in display space (blend ONE, ONE) as the tone-mapped difference it makes
 * The stencil (copied back before this pass) picks the pixels for each mode. */

#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_CELL_PBR 1
#define WW_CELL_HDR 1
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
#define WW_LOOSE_TYPES 1
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#ifndef WW_CELL_HDR
#define WW_CELL_HDR 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#ifndef WW_CELL_HDR
#define WW_CELL_HDR 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	int hdrMode;
};
#include "../lights/cell_lights.hlsli"
#include "../post/cell_bloom.hlsli"

Texture2DMS<float4> hdrSrc : register(t46, space0);

static float4 fragColor;

void frag_main()
{
	float4 x = hdrSrc.Load( int2( gl_FragCoord.xy ), gl_SampleID );
	// lane SKYHOLE1: the passes sort the pixels by depth too (the far plane = no opaque draw reached them)
	gl_FragDepth = 1.0;
	/* lane BLOOM1b: the FO4CS bloom at this pixel, exposed light (cell_bloom.glsl): the tent of the chain's top, red
	 * and blue pushed by the glare's dispersion, times the bloom weight. cellImageSpace adds it before its curve. */
	float3 foBloom = (float3)( 0.0 );
	if ( cellIsFoBloomOn )
		foBloom = bloomTentDispersed( cellIsFoBloom, _cellIsFoBloom_sampler, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z ) * cellIsFoBloomParams.y;
	if ( hdrMode == 3 ) {
		cellFoBloomHere = foBloom;
		float3 lit = cellImageSpace( (float3)( 0.0 ) );
		cellFoBloomHere = (float3)( 0.0 );
		fragColor = float4( max( lit - cellImageSpace( (float3)( 0.0 ) ), (float3)( 0.0 ) ), 0.0 );
		return;
	}
	// mode 2 (the effects over the sky) takes the glow from mode 3, as every pixel mode 0 does not take
	cellFoBloomHere = hdrMode == 0 ? foBloom : (float3)( 0.0 );
	if ( hdrMode == 2 ) {
		/* lane SKYHOLE1: a cell effect over the backdrop (the sky). The game draws its sky into the HDR target and
		 * blends the effect over it; here the sky is in the view, not the linear frame, so the effect went over 0.
		 * Its value un-premultiplied by its coverage (the frame's alpha: the effects blend alpha as ONE, dst),
		 * tone-mapped, then laid over the backdrop by that coverage (blend ONE, ONE_MINUS_SRC_ALPHA). */
		float A = clamp( x.a, 0.0, 1.0 );
		float3 v = max( x.rgb, (float3)( 0.0 ) );
		if ( or( !cellOn, !cellIsOn ) ) {
			fragColor = float4( v, A );
			return;
		}
		if ( A > 1.0 / 1024.0 )
			fragColor = float4( cellImageSpace( sqrt( v / A ) ) * A, A );
		else	// no coverage (an additive glow): its light added to the sky
			fragColor = float4( max( cellImageSpace( sqrt( v ) ) - cellImageSpace( (float3)( 0.0 ) ), (float3)( 0.0 ) ), 0.0 );
		return;
	}
	if ( or( or( hdrMode != 0, !cellOn ), !cellIsOn ) ) {
		fragColor = x;
		return;
	}
	fragColor = float4( cellImageSpace( sqrt( max( x.rgb, (float3)( 0.0 ) ) ) ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
    uint gl_SampleID : SV_SampleIndex;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
    float gl_FragDepth : SV_Depth;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    gl_SampleID = int(stage_input.gl_SampleID);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    stage_output.fragColor = fragColor;
    return stage_output;
}
