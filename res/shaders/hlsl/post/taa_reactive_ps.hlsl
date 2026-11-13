// HLSL source of features/post/taa_reactive.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* lane TAAINPUTS1: the mask's R (reactive: the layer's coverage, base alpha x vertex alpha x material alpha) and
 * G (composition: 1 where a see-through surface covers the pixel). outRG picks which the shape writes. */

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float2 uvScale;
	float2 uvOffset;
	int useTex;
	int hasColor;
	float matAlpha;
	float2 outRG;
};


static float4 vColor;
static float2 texCoord;

static float4 mask;

void frag_main()
{
	float a = matAlpha;
	if ( hasColor != 0 )
		a *= vColor.a;
	if ( useTex != 0 )
		a *= BaseMap.Sample( _BaseMap_sampler, texCoord * uvScale + uvOffset ).a;
	a = clamp( a, 0.0, 1.0 );
	if ( a < 1.0 / 255.0 )
		discard;
	mask = float4( outRG.x * a, outRG.y, 0.0, 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 vColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 mask : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vColor = stage_input.vColor;
    texCoord = stage_input.texCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.mask = mask;
    return stage_output;
}
