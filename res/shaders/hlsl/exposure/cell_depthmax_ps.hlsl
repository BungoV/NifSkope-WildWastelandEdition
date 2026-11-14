// HLSL source of features/exposure/cell_depthmax.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ExposureBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane PERF4: the linear frame's farthest depth per pixel (the max over its samples), for the far cards' depth reject
// (impostor_oct.frag, WW_CARDINST). Drawn by wwCellHdrDepthMax (src/features/exposure/cellhdr.cpp) into an R32F target.

Texture2DMS<float4> depthMs : register(t0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int depthSamples;
};


static float4 fragColor;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	float m = 0.0;
	for ( int i = 0; i < depthSamples; i++ )
		m = max( m, depthMs.Load( p, i ).r );
	fragColor = float4( m, 0.0, 0.0, 1.0 );
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
