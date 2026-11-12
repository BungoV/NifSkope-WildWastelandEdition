// HLSL port of features/sky/atmo_apl.geom (geometry shader), written by hand (HLSLBULK1 round 2): the GLSL
// `layout(triangles, invocations = 32)` instancing is [instance(32)] with SV_GSInstanceID, gl_Layer is
// SV_RenderTargetArrayIndex, EmitVertex / EndPrimitive are Append / RestartStrip on a TriangleStream.
// The aerial-perspective volume in ONE layered draw: invocation k writes layer k and hands the pixel stage its slice
// index (atmo_ap.frag's atApSlice). Fusion Shaders (F4FS) feeds the full-screen quad's two triangles and atApDims.
// Not wired: nothing loads this file.
// profile: gs_6_0
#include "SkyBindingsVk.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
    float3 atApDims;	// width, height, slices
};

struct StageInput
{
    float4 gl_PositionIn : SV_Position;
};

struct StageOutput
{
    float4 gl_Position : SV_Position;
    nointerpolation float atApSliceIn : TEXCOORD0;
    uint gl_Layer : SV_RenderTargetArrayIndex;
};

[instance(32)]
[maxvertexcount(3)]
void main(triangle StageInput stage_input[3], uint gl_InvocationID : SV_GSInstanceID, inout TriangleStream<StageOutput> geometry_stream)
{
    if (float(gl_InvocationID) >= atApDims.z)
    {
        return;
    }
    for (int i = 0; i < 3; i++)
    {
        StageOutput o;
        o.gl_Position = stage_input[i].gl_PositionIn;
        o.gl_Layer = gl_InvocationID;
        o.atApSliceIn = float(gl_InvocationID);
        geometry_stream.Append(o);
    }
    geometry_stream.RestartStrip();
}
