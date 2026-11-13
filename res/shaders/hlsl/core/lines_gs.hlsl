// HLSL source of lines.geom (geometry shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[2];
static float4 gl_Position;
// GLSL: layout ( lines ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 4 ) out; (main's signature)

#include "uniforms.hlsli"

static float4 vsColor[2];
static float4 C;

#define WW_LOOSE_TYPES 1
#include "drawline.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "drawline.hlsli"
#undef WW_LOOSE_UNIFORMS
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
	// per-endpoint colours so selection gradients interpolate along the edge
	float	a = min( lineWidth, 1.0 );
	float4	c0 = float4( vsColor[0].rgb, vsColor[0].a * a );
	float4	c1 = float4( vsColor[1].rgb, vsColor[1].a * a );

	float4	p0 = mul(gl_in[0].gl_Position, projectionMatrix);
	float4	p1 = mul(gl_in[1].gl_Position, projectionMatrix);

	drawLine( geometry_stream, p0, p1, c0, c1 );
}

struct SPIRV_Cross_Input
{
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(4)]
void main(line SPIRV_Cross_Input stage_input[2], inout WW_GS_STREAM geometry_stream)
{
    for (int i = 0; i < 2; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    for (int i = 0; i < 2; i++)
        vsColor[i] = stage_input[i].vsColor;
    geom_main(geometry_stream);
}
