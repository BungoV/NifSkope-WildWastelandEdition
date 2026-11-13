// HLSL source of features/clouds/cloudmap.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* lane CLOUD1: a layer's coverage into the map, a = tex.a x edge fade x the layer's JNAM alpha (the drawn 2D layer's
 * alpha, lookdev_clouds.frag). The blend unions it into the band's channel: dst = 1 - (1 - src)(1 - dst).
 * show: the map as colour (R Low, G Mid, B High). */

Texture2D<float4> Tex : register(t0, space0); SamplerState _Tex_sampler : register(s0, space0);
Texture2D<float4> Tex2 : register(t1, space0); SamplerState _Tex2_sampler : register(s1, space0);	// lane WEATHERBLEND3: during a change the new weather's layer, cross-faded by texBlend
cbuffer LooseUniforms : register(b31, space0)
{
	float texBlend;	// 0 = Tex alone
	float alpha;
	bool cmShow;
};


static float vfade;
static float2 uv;

static float4 fragColor;

void frag_main()
{
	float4 t = Tex.Sample( _Tex_sampler, uv );
	if ( texBlend > 0.0 )
		t = lerp( t, Tex2.Sample( _Tex2_sampler, uv ), texBlend );
	if ( cmShow ) {
		fragColor = float4( t.r, t.g, t.a, 1.0 );
		return;
	}
	fragColor = (float4)( clamp( t.a * vfade * alpha, 0.0, 1.0 ) );
}

struct SPIRV_Cross_Input
{
    float vfade : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vfade = stage_input.vfade;
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
