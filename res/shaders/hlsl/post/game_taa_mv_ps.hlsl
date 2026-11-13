// HLSL source of features/post/game_taa_mv.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane MOTION1: the temporal AA's motion vectors (src/features/post/gametaa.h). The game draws them from its UNJITTERED
 * current and previous matrices; this viewer's scenes stand still, so the camera's reprojection of the frame's
 * depth is the whole of it: the pixel's unjittered position (its NDC less the jitter) taken to the previous
 * frame's clip space by reproj = previous view-projection x inverse current view-projection (built in double).
 * The vector is previous minus current in texture units: the history is read at uv + mv, as the game reads it. */

Texture2D<float4> depthTex : register(t0, space0); SamplerState _depthTex_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 reproj;
	float2 jitterNdc;
	float2 invSize;
};


static float2 mv;

void frag_main()
{
	float2 uv = gl_FragCoord.xy * invSize;
	float d = depthTex.Load( int3(int2( gl_FragCoord.xy ), 0) ).r;
	float3 ndc = float3( uv * 2.0 - 1.0 - jitterNdc, d * 2.0 - 1.0 );
	float4 p = mul(float4( ndc, 1.0 ), reproj);
	mv = ( p.xy / p.w - ndc.xy ) * 0.5;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float2 mv : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.mv = mv;
    return stage_output;
}
