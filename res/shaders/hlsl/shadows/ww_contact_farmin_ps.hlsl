// HLSL source of features/shadows/ww_contact_farmin.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane GAPLOOK1 (C7): the far tier's depth reduction (FO4CS FarDepthMinCS.hlsl). Each coarse texel keeps the NEAREST
// surface of its divisor x divisor block (a min, so a thin trunk still occludes), with that texel's normal.

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);		// RGBA32F: view normal, linear depth (1e6 = none)
cbuffer LooseUniforms : register(b31, space0)
{
	int2 fullSize;
	int divisor;
};


static float4 fragOut;

void frag_main()
{
	int2 base = int2( floor( gl_FragCoord.xy ) ) * divisor;
	float4 best = float4( 0.0, 0.0, 1.0, 1e6 );
	for ( int j = 0; j < 8; j++ ) {
		if ( j >= divisor )
			break;
		for ( int i = 0; i < 8; i++ ) {
			if ( i >= divisor )
				break;
			int2 q = base + int2( i, j );
			if ( any( (q >= fullSize) ) )
				continue;
			float4 t = gbuf.Load( int3(q, 0) );
			if ( t.a < best.a )
				best = t;
		}
	}
	fragOut = best;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragOut : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragOut = fragOut;
    return stage_output;
}
