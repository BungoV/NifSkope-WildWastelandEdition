// HLSL source of features/post/cell_post_mscopy.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane POSTFX3: the linear post's picture back into a multisampled linear frame, every sample keeping its own value
 * where the blur left the pixel alone (src/features/post/cellpost.cpp 4b). Dual-source blend ONE, SRC1_COLOR (alpha ZERO, ONE):
 * sample s -> s * w + (B - w * R), R = the frame's resolve (the post's input), B = the picture, w = min(1, B / R) per
 * channel. Mean of the samples = B; none negative (B < R: s * B / R; else s + (B - R)); B == R: s exactly. */

Texture2D<float4> srcTex : register(t0, space0); SamplerState _srcTex_sampler : register(s0, space0);	// B, the picture
Texture2D<float4> refTex : register(t1, space0); SamplerState _refTex_sampler : register(s1, space0);	// R, the frame as the post read it (its samples resolved)

static float4 addOut;
static float4 scaleOut;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	float3 b = srcTex.Load( int3(p, 0) ).rgb;
	float3 r = refTex.Load( int3(p, 0) ).rgb;
	// B >= R takes w = 1 by a compare, not B / R: a GPU's divide by itself is not always exactly 1
	float3 w = float3( or( b.r >= r.r, r.r <= 0.0 ) ? 1.0 : b.r / r.r, or( b.g >= r.g, r.g <= 0.0 ) ? 1.0 : b.g / r.g,
		or( b.b >= r.b, r.b <= 0.0 ) ? 1.0 : b.b / r.b );
	addOut = float4( b - w * r, 0.0 );
	scaleOut = float4( w, 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 addOut : SV_Target0;
    float4 scaleOut : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.addOut = addOut;
    stage_output.scaleOut = scaleOut;
    return stage_output;
}
