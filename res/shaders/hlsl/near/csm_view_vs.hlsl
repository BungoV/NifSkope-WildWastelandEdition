// hlsl profile: vs_6_0
// Lane VKSHADOW1 (2026-11-13): the sun cascades' queued casters on the present device (vkpresent.cpp csmRun), the
// Vulkan twin of lookdev_csmdepth_q.vert / lookdev_csmdepth_q_vs.hlsl with the same products in the same order.
// Position at location 0 (binding 0), the caster's model-view matrix GL column-major at 1-4 (binding 1, per instance),
// csmClipFromView GL column-major in the push block. GL's clip z in [-w, w] is written as (z + w) / 2 in [0, w]: with
// the ortho cascades w = 1, so the window depth equals GL's z * 0.5 + 0.5 bit for bit (the halving is exact).
// profile: vs_6_0
struct CsmPush
{
    row_major float4x4 clip;
};
[[vk::push_constant]] CsmPush P;

struct CsmIn
{
    [[vk::location(0)]] float3 pos : TEXCOORD0;
    [[vk::location(1)]] float4 mv0 : TEXCOORD1;
    [[vk::location(2)]] float4 mv1 : TEXCOORD2;
    [[vk::location(3)]] float4 mv2 : TEXCOORD3;
    [[vk::location(4)]] float4 mv3 : TEXCOORD4;
};

float4 main( CsmIn i ) : SV_Position
{
    float4x4 mv;
    mv[0] = i.mv0;
    mv[1] = i.mv1;
    mv[2] = i.mv2;
    mv[3] = i.mv3;
    float4 p = mul( mul( float4( i.pos, 1.0f ), mv ), P.clip );
#ifndef CSM_RED_NOHALF
    p.z = ( p.z + p.w ) * 0.5f;
#endif
    return p;
}
