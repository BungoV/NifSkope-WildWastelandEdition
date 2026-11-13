// Lane VKOBJ2: the bindless gate's vertex stage (tests/vkframe/blview.cpp): one 24 x 24 px cell of a 16 x 8 grid per draw,
// the cell = the row = the draw's BaseInstance (as fo4_defaultbl_vs.hlsl reads it), handed flat to the pixel stage.
struct VsOut
{
    float4 pos : SV_Position;
    nointerpolation uint row : TEXCOORD0;
};

VsOut main(uint vid : SV_VertexID, [[vk::builtin("BaseInstance")]] uint baseInstance : BASEINSTANCE)
{
    const float2 corner[6] = { float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0) };
    const uint cell = baseInstance;
    const float2 px = (float2(float(cell % 16u), float(cell / 16u)) + corner[vid]) * 24.0;
    VsOut o;
    o.pos = float4(px / float2(384.0, 192.0) * 2.0 - 1.0, 0.0, 1.0);
    o.row = cell;
    return o;
}
