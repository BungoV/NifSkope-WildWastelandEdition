// HLSL source of features/post/game_dof_blur.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane GRASSMB1: the depth of field's blur, instruction for instruction from the game's Blur3..Blur15 shaders
 * (Shaders011 entries 03725..03737, ImageSpaceEffectBlur, src/features/post/cellpost.h):
 *   acc = sum over the taps of texture(t0, v1 + o.xy) * o.z
 * o = (offset x, offset y, weight, -): row r of the game's aBlurWeights table (2r + 1 taps, r <= 7), the offsets
 * (k - 7) / the input's size along this pass's axis. v1 = this pass's target uv (the target's size: dstSize). */

Texture2D<float4> t0 : register(t0, space0); SamplerState _t0_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float4 o[15];
	int taps;
	float2 dstSize;
};


static float4 fragColor;

void frag_main()
{
	float2 v1 = gl_FragCoord.xy / dstSize;
	float4 acc = (float4)( 0.0 );
	for ( int i = 0; i < taps; i++ )					// loop / ige / breakc_nz
		acc = t0.Sample( _t0_sampler, v1 + o[i].xy ) * o[i].z + acc;	// add, sample, mad
	fragColor = acc;
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
