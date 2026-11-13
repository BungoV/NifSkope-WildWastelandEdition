// HLSL source of features/sky/atmo_apl.geom (geometry shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[3];
static float4 gl_Position;
static uint gl_Layer;
static int gl_InvocationID;
/* lane PERFFIX3 (item 1): the aerial-perspective volume in ONE layered draw. The full-screen quad's two triangles go to
 * every slice of the 3D target (attached layered): invocation k writes layer k and hands the fragment its slice index,
 * the value the per-slice path set as the uniform atApSlice (atmo_ap.frag), so each froxel runs the same maths.
 * 32 invocations = the most slices any tier draws (Ultra 64x64x32, src/gl/atmosphere.cpp buildAp). */

// GLSL: layout ( triangles , invocations = 32 ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 3 ) out; (main's signature)
cbuffer LooseUniforms : register(b31, space0)
{
	float3 atApDims;	// width, height, slices
};


static float atApSliceIn;


struct SPIRV_Cross_Output
{
    nointerpolation float atApSliceIn : TEXCOORD0;
    float4 gl_Position : SV_Position;
    uint gl_Layer : SV_RenderTargetArrayIndex;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.atApSliceIn = atApSliceIn;
    stage_output.gl_Layer = gl_Layer;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	if ( float( gl_InvocationID ) >= atApDims.z )
		return;
	for ( int i = 0; i < 3; i++ ) {
		gl_Position = gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		atApSliceIn = float( gl_InvocationID );
		EmitVertex( geometry_stream );
	}
	EndPrimitive( geometry_stream );
}

struct SPIRV_Cross_Input
{
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(3)]
[instance(32)]
void main(triangle SPIRV_Cross_Input stage_input[3], inout WW_GS_STREAM geometry_stream, uint gl_InvocationID_in : SV_GSInstanceID)
{
    for (int i = 0; i < 3; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    gl_InvocationID = int(gl_InvocationID_in);
    geom_main(geometry_stream);
}
