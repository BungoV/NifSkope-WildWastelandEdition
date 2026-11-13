// HLSL source of normals.geom (geometry shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[1];
static float4 gl_Position;
// GLSL: layout ( points ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 4 ) out; (main's signature)

#include "uniforms.hlsli"



static float3x3 btnMatrix[1];

static float4 vsColor[1];
static float4 C;

#define WW_LOOSE_TYPES 1
#include "drawline.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "drawline.hlsli"
#undef WW_LOOSE_UNIFORMS
	int btnSelection;				// 0: draw bitangent, 1: draw tangent, 2: draw normal
	float normalLineLength;			// line length in view space
};

struct SPIRV_Cross_Output
{
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.C = C;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

#include "drawline.hlsli"

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	float4	c = float4( vsColor[0].rgb, vsColor[0].a * min( lineWidth, 1.0 ) );

	float4	d = float4( btnMatrix[0][clamp( btnSelection, 0, 2 )] * normalLineLength, 0.0 );
	float4	p0 = mul(( gl_in[0].gl_Position - d * 0.2 ), projectionMatrix);
	float4	p1 = mul(( gl_in[0].gl_Position + d * 0.8 ), projectionMatrix);

	drawLine( geometry_stream, p0, p1, c, c );
}

struct SPIRV_Cross_Input
{
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(4)]
void main(point SPIRV_Cross_Input stage_input[1], inout WW_GS_STREAM geometry_stream)
{
    for (int i = 0; i < 1; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    for (int i = 0; i < 1; i++)
        btnMatrix[i] = stage_input[i].btnMatrix;
    for (int i = 0; i < 1; i++)
        vsColor[i] = stage_input[i].vsColor;
    geom_main(geometry_stream);
}
