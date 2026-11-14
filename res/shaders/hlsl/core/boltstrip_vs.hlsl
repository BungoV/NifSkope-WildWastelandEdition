// HLSL source of boltstrip.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
// Textured camera-facing strip for the BSProceduralLightningController preview

static float4 C;
static float2 texCoord;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
};


static float3 vertexPosition;
static float4 vertexColor;
static float2 vertexUV;

void vert_main()
{
	C = vertexColor;
	texCoord = vertexUV;

	gl_Position = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), projectionMatrix);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 vertexUV : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 C : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vertexUV = stage_input.vertexUV;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.C = C;
    stage_output.texCoord = texCoord;
    return stage_output;
}
