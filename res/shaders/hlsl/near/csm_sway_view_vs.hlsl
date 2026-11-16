// hlsl profile: vs_6_0
// Lane VKSHADOW10 (2026-11-16): the sun cascades' swaying per-draw casters on the present device (vkpresent.cpp csmRun,
// CsmDraw::sway), the Vulkan twin of lookdev_csmdepth.vert / shadows/lookdev_csmdepth_vs.hlsl with lodTreeAnim 1 and
// farInst false: csm_view_vs.hlsl's products, plus the far wind op for op in GL's order (NV GL's dot / mat3 * vec3 are
// fma chains from x: x * x', then fma( y, y', . ), then fma( z, z', . )). The wind weight (vertexColor.a) is location 5
// (vertex binding 3, the mesh pool's float stream). Push block: clip, then viewMatrix's GL columns with the block's
// centre in .w, then ( block w, farSwayAmp, farSwayTime, 0 ).
// profile: vs_6_0
struct CsmSwayPush
{
    row_major float4x4 clip;
    float4 c0;  // viewMatrix column 0, block.x
    float4 c1;  // viewMatrix column 1, block.y
    float4 c2;  // viewMatrix column 2, block.z
    float4 bw;  // block.w, farSwayAmp, farSwayTime, 0
};
[[vk::push_constant]] CsmSwayPush P;

struct CsmIn
{
    [[vk::location(0)]] float3 pos : TEXCOORD0;
    [[vk::location(1)]] float4 mv0 : TEXCOORD1;
    [[vk::location(2)]] float4 mv1 : TEXCOORD2;
    [[vk::location(3)]] float4 mv2 : TEXCOORD3;
    [[vk::location(4)]] float4 mv3 : TEXCOORD4;
    [[vk::location(5)]] float wind : TEXCOORD5;
};

float dot3( float3 a, float3 b )
{
    return mad( a.z, b.z, mad( a.y, b.y, a.x * b.x ) );
}

float4 main( CsmIn i ) : SV_Position
{
    float4x4 mv;
    mv[0] = i.mv0;
    mv[1] = i.mv1;
    mv[2] = i.mv2;
    mv[3] = i.mv3;
    float4 v = mul( float4( i.pos, 1.0f ), mv );
    // transpose( viewMatrix ) * ( v.xyz - farSwayBlock.xyz ): the world-axis offset from the block's centre
    const float3 d = v.xyz - float3( P.c0.w, P.c1.w, P.c2.w );
    const float rx = dot3( P.c0.xyz, d ), ry = dot3( P.c1.xyz, d );
    const float amp = P.bw.y, t = P.bw.z;
    if ( amp > 0.0f && max( abs( rx ), abs( ry ) ) > P.bw.x ) {
        // world-horizontal offset, phase by the vertex's own place so a forest does not move as one
        const float ph = mad( i.pos.y, 0.0017f, i.pos.x * 0.0021f );
        const float3 w = float3( sin( mad( t, 1.3f, ph ) ), cos( mad( t, 0.9f, ph * 1.7f ) ) * 0.6f, 0.0f );
        const float3 u = w * ( amp * i.wind );
        float3 o = P.c0.xyz * u.x;
        o = mad( P.c1.xyz, u.y, o );
        o = mad( P.c2.xyz, u.z, o );
        v.xyz += o;
    }
    float4 p = mul( v, P.clip );
    p.z = ( p.z + p.w ) * 0.5f;
    return p;
}
