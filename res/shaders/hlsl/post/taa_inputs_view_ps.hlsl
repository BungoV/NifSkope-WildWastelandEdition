// HLSL source of features/post/taa_inputs_view.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane TAAINPUTS1: viewMode 1 velocity (hue = direction, brightness = pixels a frame / viewScale, white tint where
 * it is an object vector), 2 the mask (R reactive, G composition, B own motion), 3 velocity left | mask right. */

Texture2D<float4> velTex : register(t0, space0); SamplerState _velTex_sampler : register(s0, space0);
Texture2D<float4> maskTex : register(t1, space0); SamplerState _maskTex_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int viewMode;
	float2 size;
	float viewScale;
};


static float4 fragColor;

float3 hue( float h )
{
	return clamp( abs( frac( h + float3( 0.0, 2.0 / 3.0, 1.0 / 3.0 ) ) * 6.0 - 3.0 ) - 1.0, 0.0, 1.0 );
}

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	int mode = viewMode;
	if ( mode == 3 )
		mode = gl_FragCoord.x < size.x * 0.5 ? 1 : 2;
	float4 m = maskTex.Load( int3(p, 0) );
	if ( mode == 2 ) {
		fragColor = float4( m.rgb, 1.0 );
		return;
	}
	float2 px = velTex.Load( int3(p, 0) ).xy * size;
	float len = length( px );
	float3 c = len > 1e-4 ? hue( atan2( px.y, px.x ) / 6.28318531 + 0.5 ) * clamp( len / viewScale, 0.0, 1.0 ) : (float3)( 0.0 );
	fragColor = float4( lerp( c, (float3)( 1.0 ), m.b * 0.25 ), 1.0 );
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
