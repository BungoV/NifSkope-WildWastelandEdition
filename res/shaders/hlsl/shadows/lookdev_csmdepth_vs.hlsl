// HLSL source of features/shadows/lookdev_csmdepth.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
/* The cascade caster pass (lane CSM1), vertex stage: the shape's own model-view
 * matrix (as the renderer draws it), then view space -> the cascade's clip space. */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 csmClipFromView;
	row_major float4x4 modelViewMatrix;
	// lane FARDRAW1: an instanced far ring's placement rows (fo4_default.vert); false = every other caster
	bool farInst;
	// end of uniforms.glsl
	float farSwayAmp;
	float farSwayTime;
	float4 farSwayBlock;
	bool lodTreeAnim;
};


static float3 vertexPosition;
static float4 farInstRow0;
static float4 farInstRow1;
static float4 farInstRow2;

/* lane SHADOWSHIMMER2 (FARSWAYSHADOW): the far field's wind, fo4_default.vert's block op for op with the same uniforms
 * (gl/sunshadow.cpp sets them from the one per-paint sample cellfarlod.cpp's colour uniforms take; farSwayAmp 0 = none,
 * the red WW_SUN_SHADOW_STABLE_RED=sway). viewMatrix is the colour pass's own (globalUniforms). */
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
static float4 vertexColor;

void vert_main()
{
	float4 v = float4( vertexPosition, 1.0 );
	float2 farXY = vertexPosition.xy;
	if ( farInst ) {
		v.xyz = float3( dot( farInstRow0, v ), dot( farInstRow1, v ), dot( farInstRow2, v ) );
		farXY = v.xy;
	}
	v = mul(v, modelViewMatrix);
	float3 farRel = mul(( v.xyz - farSwayBlock.xyz ), transpose( viewMatrix ));	// world-axis offset from the block's centre
	if ( farSwayAmp > 0.0 && lodTreeAnim && max( abs( farRel.x ), abs( farRel.y ) ) > farSwayBlock.w ) {
		// world-horizontal offset, phase by the vertex's own place so a forest does not move as one
		float ph = dot( farXY, float2( 0.0021, 0.0017 ) );
		float3 w = float3( sin( farSwayTime * 1.3 + ph ), cos( farSwayTime * 0.9 + ph * 1.7 ) * 0.6, 0.0 );
		v.xyz += mul(( w * ( farSwayAmp * vertexColor.a ) ), viewMatrix);
	}
	gl_Position = mul(v, csmClipFromView);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float4 farInstRow0 : TEXCOORD10;
    float4 farInstRow1 : TEXCOORD11;
    float4 farInstRow2 : TEXCOORD12;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    farInstRow0 = stage_input.farInstRow0;
    farInstRow1 = stage_input.farInstRow1;
    farInstRow2 = stage_input.farInstRow2;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
