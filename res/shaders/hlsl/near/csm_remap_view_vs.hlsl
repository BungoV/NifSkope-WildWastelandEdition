// Lane VKSHADOW6 (2026-11-16): the sun cascade cache's Scroll remap on the present device (vkpresent.cpp csmRun, a
// CsmDraw with remap set), the Vulkan twin of lookdev_csmremap.vert: one triangle over the viewport, no vertex data.
// hlsl profile: vs_6_0
float4 main( uint id : SV_VertexID ) : SV_Position
{
    const float2 p = float2( ( id << 1 ) & 2, id & 2 );
    return float4( p * 2.0f - 1.0f, 0.0f, 1.0f );
}
