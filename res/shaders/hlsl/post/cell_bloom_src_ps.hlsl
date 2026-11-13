// HLSL source of features/post/cell_bloom_src.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
static int gl_SampleID;
/* The bloom's source: the linear frame's cell-lit samples times the imagespace exposure (FO4CS order: exposure,
 * bloom, tone map). Multisampled beside the frame's depth / stencil; a blit resolves it for the chain. */

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
	float3 bloomImpulse;	// test probe: x, y, value (value > 0: only the 2x2 block holding x, y, at that value;
};
#include "../lights/cell_lights.hlsli"

Texture2DMS<float4> hdrSrc : register(t46, space0);
							// a lone pixel would be dropped by the Karis average)

static float4 fragColor;

void frag_main()
{
	if ( bloomImpulse.z > 0.0 ) {
		fragColor = float4( all(int2( gl_FragCoord.xy ) / 2 == int2( bloomImpulse.xy ) / 2) ? (float3)( bloomImpulse.z ) : (float3)( 0.0 ), 1.0 );
		return;
	}
	if ( or( !cellOn, !cellIsOn ) ) {
		fragColor = (float4)( 0.0 );
		return;
	}
	float4 x = hdrSrc.Load( int2( gl_FragCoord.xy ), gl_SampleID );
	fragColor = float4( max( x.rgb, (float3)( 0.0 ) ) * ( ( cellIsRed & 2 ) != 0 ? 1.0 : cellIsExposure ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
    uint gl_SampleID : SV_SampleIndex;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    gl_SampleID = int(stage_input.gl_SampleID);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
