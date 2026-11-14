// HLSL source of features/ao/cell_aozms.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* RESUME 15: stage 6 of the cell view's obscurance (src/features/ao/ao.cpp wwCellAoPass): the depth pre-pass's linear
 * view depth (R32F, multisampled as the frame's depth) to one value a pixel, sample 0 as the depth resolve takes it, so
 * a silhouette pixel holds one surface's depth and never an average of two. */

Texture2DMS<float4> zms : register(t0, space0);

static float4 fragColor;

void frag_main()
{
	fragColor = (float4)( zms.Load( int2( gl_FragCoord.xy ), 0 ).r );
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
