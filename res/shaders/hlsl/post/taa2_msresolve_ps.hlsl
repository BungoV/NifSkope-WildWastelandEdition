// HLSL source of features/post/taa2_msresolve.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane POSTFX3: the linear frame's multisamples into one value for the Modern temporal AA before the tone map
 * (src/features/post/gametaa.h, WW_TAA2_LINEAR). Each sample is weighted as the display will show it (Karis 2014: c / (1 + max c)
 * at about display level), the mean turned back to light. A plain average (the red, a blit) lets one sky sample
 * take an edge pixel over, so the tone-mapped edge steps where the toned resolve's does not. */

Texture2DMS<float4> msTex : register(t0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int msSamples;
	float msExpose;	// the linear frame's scale to about display level (last frame's exposure)
};


static float4 fragColor;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	float3 sum = (float3)( 0.0 );
	float a = 0.0;
	for ( int i = 0; i < msSamples; i++ ) {
		float4 c = msTex.Load( p, i );
		float3 t = max( c.rgb, (float3)( 0.0 ) ) * msExpose;
		sum += t / ( 1.0 + max( t.r, max( t.g, t.b ) ) );
		a += c.a;
	}
	float inv = 1.0 / float( max( msSamples, 1 ) );
	float3 t = clamp( sum * inv, 0.0, 0.999 );
	fragColor = float4( t / ( ( 1.0 - max( t.r, max( t.g, t.b ) ) ) * max( msExpose, 1e-6 ) ), a * inv );
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
