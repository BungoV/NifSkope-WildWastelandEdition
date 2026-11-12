// HLSL port of features/water/ocean_layers.geom (geometry shader), written by hand (HLSLBULK1 round 2): the GLSL
// `layout(triangles, invocations = 6)` instancing is [instance(6)] with SV_GSInstanceID, gl_Layer is
// SV_RenderTargetArrayIndex, EmitVertex / EndPrimitive are Append / RestartStrip on a TriangleStream.
// The full-screen quad into all six layers of a layered target in one draw, one invocation a layer.
// Fusion Shaders (F4FS) feeds the full-screen quad's two triangles. Not wired: nothing loads this file.
// profile: gs_6_0
#include "WaterBindingsVk.hlsli"

struct StageInput
{
    float4 gl_PositionIn : SV_Position;
};

struct StageOutput
{
    float4 gl_Position : SV_Position;
    nointerpolation int oceanLayerG : TEXCOORD0;
    uint gl_Layer : SV_RenderTargetArrayIndex;
};

[instance(6)]
[maxvertexcount(3)]
void main(triangle StageInput stage_input[3], uint gl_InvocationID : SV_GSInstanceID, inout TriangleStream<StageOutput> geometry_stream)
{
    for (int i = 0; i < 3; i++)
    {
        StageOutput o;
        o.gl_Position = stage_input[i].gl_PositionIn;
        o.gl_Layer = gl_InvocationID;
        o.oceanLayerG = int(gl_InvocationID);
        geometry_stream.Append(o);
    }
    geometry_stream.RestartStrip();
}
