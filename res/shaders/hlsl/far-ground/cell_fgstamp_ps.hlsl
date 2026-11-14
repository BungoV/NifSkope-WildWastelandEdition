// HLSL source of features/far-ground/cell_fgstamp.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarGroundBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane FARDRAW1: the cached far-ground option's stamp (src/features/far-ground/cellfarground.cpp). One page a cell: every decal of
 * the ring over the cell's ground, premultiplied, at fgStampSide texels a cell (the "Far ground pages" preset). */

#define WW_FG_STAMP 1
#define WW_FARGROUND 0
#define WW_LOOSE_TYPES 1
#ifndef WW_FG_STAMP
#define WW_FG_STAMP 1	// as below, before this file
#endif
#include "cell_farground.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#ifndef WW_FG_STAMP
#define WW_FG_STAMP 1	// as below, before this file
#endif
#include "cell_farground.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 fgStampCell;		// the page's cell origin, world
	float fgStampSide;
};
#include "cell_farground.hlsli"

Texture2D<float4> fgHeight : register(t25, space0); SamplerState _fgHeight_sampler : register(s25, space0);		// the .lodl heights, a texel a sample

static float4 fragColor;

void frag_main()
{
	float2 w = fgStampCell + gl_FragCoord.xy / fgStampSide * 4096.0;
	float2 g = ( w - fgGridMap.xy ) * fgGridMap.z;
	float z = fgHeight.Sample( _fgHeight_sampler, ( g + 0.5 ) / fgGridN ).r;
	fragColor = fgDecals( float3( w, z ), 4096.0 / fgStampSide );
}

struct SPIRV_Cross_Input
{
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
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
