// HLSL source of boltstrip.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

// Textured camera-facing strip for the BSProceduralLightningController preview

Texture2D<float4> boltTexture; SamplerState _boltTexture_sampler;

// lane FXPALETTE1: the effect shader's greyscale to palette (fo4_effectshader.frag); both off = the plain strip
Texture2D<float4> GreyscaleMap; SamplerState _GreyscaleMap_sampler;
uniform bool greyscaleColor;
uniform bool greyscaleAlpha;
uniform float paletteRow;	// the effect's base colour R, the palette row

static float4 C;
static float2 texCoord;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float4 C : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


void frag_main()
{
	float4 tex = boltTexture.Sample(_boltTexture_sampler, texCoord);
	fragColor = tex * C;
	if ( greyscaleColor )
		fragColor.rgb = GreyscaleMap.SampleLevel(_GreyscaleMap_sampler, float2( tex.g, paletteRow ), 0.0).rgb;
	if ( greyscaleAlpha )
		fragColor.a = GreyscaleMap.SampleLevel(_GreyscaleMap_sampler, float2( tex.a, C.a ), 0.0).a;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texCoord = stage_input.texCoord;
    C = stage_input.C;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
