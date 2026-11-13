// HLSL source of normals.geom, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: gs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_GS 1
#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
static float4 gl_PositionIn[1];
static float4 gl_Position;


#include "uniforms.src.hlsli"

uniform int btnSelection;				// 0: draw bitangent, 1: draw tangent, 2: draw normal
uniform float normalLineLength;			// line length in view space

static float3x3 btnMatrix[1];

static float4 vsColor[1];
static float4 C;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};
struct SPIRV_Cross_Output
{
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

// GLSL EmitVertex(): the current outputs become one vertex of the strip
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.C = C;
    stage_output.gl_Position = gl_Position;
    geometry_stream.Append(stage_output);
}


#include "drawline.src.hlsli"

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	float4	c = float4( vsColor[0].rgb, vsColor[0].a * min( lineWidth, 1.0 ) );

	float4	d = float4( btnMatrix[0][clamp( btnSelection, 0, 2 )] * normalLineLength, 0.0 );
	float4	p0 = mul(( gl_PositionIn[0] - d * 0.2 ), projectionMatrix);
	float4	p1 = mul(( gl_PositionIn[0] + d * 0.8 ), projectionMatrix);

	drawLine( p0, p1, c, c , geometry_stream);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
[maxvertexcount(4)]
void main(point SPIRV_Cross_Input stage_input[1], inout TriangleStream<SPIRV_Cross_Output> geometry_stream)
{
    for (int i = 0; i < 1; i++)
    {
        gl_PositionIn[i] = stage_input[i].gl_PositionIn;
    }
    for (int i = 0; i < 1; i++)
    {
        vsColor[i] = stage_input[i].vsColor;
    }
    for (int i = 0; i < 1; i++)
    {
        btnMatrix[i] = stage_input[i].btnMatrix;
    }
    geom_main(geometry_stream);
}
