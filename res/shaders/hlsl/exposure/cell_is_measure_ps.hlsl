// HLSL source of features/exposure/cell_is_measure.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ExposureBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane PERF1: the exposure's and the bloom's source from the linear frame already drawn (src/features/exposure/cellhdr.cpp),
 * as the game reads its own HDR target: the pixel's samples averaged (a non-finite one read as 0), alpha 1. The
 * stencil lets it write only where a cell program or a cell effect wrote last (linear light); its mips then hold
 * the mean light over those pixels (rgb / a). */

Texture2DMS<float4> hdrSrc : register(t0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int hdrSamples;
};


static float4 fragColor;

float finiteOr0( float v )
{
	return or( isnan( v ), isinf( v ) ) ? 0.0 : v;
}

void frag_main()
{
	float3 sum = (float3)( 0.0 );
	for ( int i = 0; i < hdrSamples; i++ ) {
		float3 v = hdrSrc.Load( int2( gl_FragCoord.xy ), i ).rgb;
		sum += float3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) );
	}
	fragColor = float4( sum / float( max( hdrSamples, 1 ) ), 1.0 );
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
