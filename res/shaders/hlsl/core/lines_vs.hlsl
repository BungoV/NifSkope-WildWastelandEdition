// HLSL source of lines.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static float4 vsColor;
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
	float4 highlightColor;
};


static float3 vertexPosition;
static float4 vertexColor;

void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );

	gl_Position = mul(v, modelViewMatrix);

	vsColor = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vsColor = vsColor;
    return stage_output;
}
