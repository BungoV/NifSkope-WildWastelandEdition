// Lane VKSHADOW8 (2026-11-16): the sun cascades' alpha test on the present device, the Vulkan twin of
// lookdev_csmdepth_at.frag: the same test the colour pass runs (BaseMap.a * alpha against alphaThreshold), the fragment
// kept where it passes. The map is the texture cache's BC twin with GL's own sampler state (vkpresent.cpp csmAtTex); the
// push block follows the vertex stage's 64-byte clip matrix.
// hlsl profile: ps_6_0
struct AtPush
{
    [[vk::offset(64)]] float2 uvScale;
    float2 uvOffset;
    float alpha;
    float alphaThreshold;
};
[[vk::push_constant]] AtPush P;
[[vk::binding(0, 0)]] Texture2D<float4> BaseMap;
[[vk::binding(1, 0)]] SamplerState BaseMapSampler;

void main( float4 fc : SV_Position, [[vk::location(0)]] float2 texCoord : TEXCOORD0 )
{
    if ( BaseMap.Sample( BaseMapSampler, texCoord * P.uvScale + P.uvOffset ).a * P.alpha < P.alphaThreshold )
        discard;
}
