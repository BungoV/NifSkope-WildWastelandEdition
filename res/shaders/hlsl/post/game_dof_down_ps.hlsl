// HLSL source of features/post/game_dof_down.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane GRASSMB1: the depth of field's downsample, instruction for instruction from the game's shader (Shaders011
 * entry 03706, ImageSpaceEffectDepthOfField's first pass, src/features/post/cellpost.h). Four taps of the full-size frame
 * into the quarter-size target: uv = (v1 + tap.xy * c7.xy) * c4.xy, a tap whose colour is Inf or NaN counts as 0,
 * weighted by tap.z; alpha = c7.z.
 * c4 = (1, 1, 2/W, 2/H), c7 = (1/W, 1/H, 0, 0), taps = (+-1, +-1, 0.25). */

Texture2D<float4> t0 : register(t0, space0); SamplerState _t0_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float4 c4;
	float4 c7;
	float4 taps[4];
	float2 dstSize;
};


static float4 fragColor;

void frag_main()
{
	float2 v1 = gl_FragCoord.xy / dstSize;					// the ceil(W/4) x ceil(H/4) target's uv
	float3 acc = (float3)( 0.0 );
	for ( int i = 0; i < 4; i++ ) {						// loop / ige / breakc_nz
		float2 uv = ( taps[i].xy * c7.xy + v1 ) * c4.xy;		// mad, mul
		float3 c = t0.Sample( _t0_sampler, uv ).rgb;
		bool3 bad = (asuint( c ) & 0x7f800000u == (uint3)( 0x7f800000u ));	// and, ieq
		c = select(bad, (float3)( 0.0 ), c);					// movc
		acc = c * taps[i].z + acc;							// mad
	}
	fragColor = float4( acc, c7.z );
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
