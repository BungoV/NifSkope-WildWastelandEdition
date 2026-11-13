// HLSL source of lookdev_bill.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

/* Lookdev sky billboard (lane PBRWX1): rgb = texture (sRGB-decoded here: the
 * sky DDS carry no DX10 header, so GL samples them raw) x tint (linear),
 * a = texture alpha x alpha. Blended in display space (no HDR yet, ruling). */

#include "uniforms.src.hlsli"

Texture2D<float4> Tex; SamplerState _Tex_sampler;
uniform float3 tint;
uniform float alpha;
uniform float sceneExposure;
uniform int viewTransform;

static float2 uv;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float2 uv : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


#include "lookdev_output.src.hlsli"

void frag_main()
{
	float4 t = Tex.Sample(_Tex_sampler, uv);
	fragColor = float4( studioOutput( srgbToLinear( t.rgb ) * tint ), t.a * alpha );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
