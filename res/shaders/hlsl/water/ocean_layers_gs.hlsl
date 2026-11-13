// HLSL source of features/water/ocean_layers.geom (geometry shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[3];
static float4 gl_Position;
static uint gl_Layer;
static int gl_InvocationID;
// lane WATERLOOK1 (src/gl/cellocean.cpp): the full-screen quad into all six layers of a layered target in one draw
// (instanced geometry shader, one invocation a layer): one render target change a mip level instead of six.

// GLSL: layout ( triangles , invocations = 6 ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 3 ) out; (main's signature)

static int oceanLayerG;


struct SPIRV_Cross_Output
{
    nointerpolation int oceanLayerG : TEXCOORD0;
    float4 gl_Position : SV_Position;
    uint gl_Layer : SV_RenderTargetArrayIndex;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.oceanLayerG = oceanLayerG;
    stage_output.gl_Layer = gl_Layer;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	for ( int i = 0; i < 3; i++ ) {
		gl_Position = gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		oceanLayerG = gl_InvocationID;
		EmitVertex( geometry_stream );
	}
	EndPrimitive( geometry_stream );
}

struct SPIRV_Cross_Input
{
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(3)]
[instance(6)]
void main(triangle SPIRV_Cross_Input stage_input[3], inout WW_GS_STREAM geometry_stream, uint gl_InvocationID_in : SV_GSInstanceID)
{
    for (int i = 0; i < 3; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    gl_InvocationID = int(gl_InvocationID_in);
    geom_main(geometry_stream);
}
