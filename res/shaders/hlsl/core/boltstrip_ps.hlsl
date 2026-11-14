// HLSL source of boltstrip.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// Textured camera-facing strip for the BSProceduralLightningController preview

Texture2D<float4> boltTexture : register(t0, space0); SamplerState _boltTexture_sampler : register(s0, space0);

// lane FXPALETTE1: the effect shader's greyscale to palette (fo4_effectshader.frag); both off = the plain strip
Texture2D<float4> GreyscaleMap : register(t1, space0); SamplerState _GreyscaleMap_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool greyscaleColor;
	bool greyscaleAlpha;
	float paletteRow;	// the effect's base colour R, the palette row
};


static float4 C;
static float2 texCoord;

static float4 fragColor;

void frag_main()
{
	float4 tex = boltTexture.Sample( _boltTexture_sampler, texCoord );
	fragColor = tex * C;
	if ( greyscaleColor )
		fragColor.rgb = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( tex.g, paletteRow ), 0.0 ).rgb;
	if ( greyscaleAlpha )
		fragColor.a = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( tex.a, C.a ), 0.0 ).a;
}

struct SPIRV_Cross_Input
{
    float4 C : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    C = stage_input.C;
    texCoord = stage_input.texCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
