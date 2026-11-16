// hlsl profile: vs_6_0
// Lane VKSHADOW8 (2026-11-16): the sun cascades' alpha-tested per-draw casters on the present device (vkpresent.cpp
// csmRun, CsmDraw::atTex), the Vulkan twin of lookdev_csmdepth_at.vert: csm_view_vs.hlsl's products in the same order,
// plus the shape's first UV set (location 5, vertex binding 2, the mesh pool's vec2 stream) passed on as texCoord.
// farInst rows and the far wind (farSwayAmp) are not here: those casters stay on GL (sunshadow.cpp).
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
    [[vk::location(5)]] float2 uv : TEXCOORD5;
};

struct CsmOut
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float2 texCoord : TEXCOORD0;
};

CsmOut main( CsmIn i )
{
    float4x4 mv;
    mv[0] = i.mv0;
    mv[1] = i.mv1;
    mv[2] = i.mv2;
    mv[3] = i.mv3;
    CsmOut o;
    o.texCoord = i.uv;
    float4 p = mul( mul( float4( i.pos, 1.0f ), mv ), P.clip );
    p.z = ( p.z + p.w ) * 0.5f;
    o.pos = p;
    return o;
}
