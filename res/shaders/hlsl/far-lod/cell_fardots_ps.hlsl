// HLSL source of features/far-lod/cell_fardots.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
static float2 gl_PointCoord;	// HLSL has no point sprites: the stage input wwPointCoord
// a bulb's Gaussian, added into the linear frame (cell_fardots.vert), dimmed by the
// air's transmittance to the bulb, as an additive effect is (wwFogFx; red fxfog: the game's fog alone)

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
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_UNIFORMS
};
#include "../air/lookdev_fog.hlsli"

static float3 dotValue;
static float dotSigma;
static float3 dotView;
static float4 fragColor;

void frag_main()
{
	float2 r = ( gl_PointCoord - 0.5 ) * ( 2.0 * ceil( 3.0 * dotSigma ) + 1.0 );
	float g = exp( -dot( r, r ) / ( 2.0 * dotSigma * dotSigma ) );
	if ( g < 0.011 )	// past 3 sigma: the sprite's corners stay the sky's
		discard;
	fragColor = float4( wwFogFx( dotValue * g, dotView, true ), 0.0 );
}

struct SPIRV_Cross_Input
{
    float2 wwPointCoord : TEXCOORD0;
    float3 dotValue : TEXCOORD1;
    float dotSigma : TEXCOORD2;
    float3 dotView : TEXCOORD3;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    gl_PointCoord = stage_input.wwPointCoord;
    dotValue = stage_input.dotValue;
    dotSigma = stage_input.dotSigma;
    dotView = stage_input.dotView;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
