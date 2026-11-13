// Lane VKOBJ2: the bindless gate's pixel stage (tests/vkframe/blview.cpp), compiled by blview.sh with -D WW_VK_BINDLESS (and
// WW_VK_BINDLESS_RED_SLOT for the red). The maps come from the shipped include (res/shaders/hlsl/near/ww_bindless_vk.hlsli)
// through the row the twins declare; cell % 8 picks the slot and its sampling op, the cell's uv scale / offset are exact.
struct WwDraw
{
    row_major float4x4 mv;
    float4 nm[3];
    uint2 tex[8];
};
[[vk::binding(12, 0)]] StructuredBuffer<WwDraw> wwDraws : register(t48, space0);	// GL binding 12
static uint wwBlRow;

#include "../../res/shaders/hlsl/near/ww_bindless_vk.hlsli"

float4 main(float4 pos : SV_Position, nointerpolation uint row : TEXCOORD0) : SV_Target
{
    wwBlRow = row;
    const uint cell = row;
    const uint k = cell % 8u;
    const float2 org = float2(float(cell % 16u), float(cell / 16u)) * 24.0;
    const float scl = float(1u << ((cell / 8u) % 4u)) * 0.0625;
    const float off = 0.125 * float(cell % 3u);
    const float2 uv = (pos.xy - org) * scl + off;
    if (k == 0u)
        return BaseMap.Sample(_BaseMap_sampler, uv);
    if (k == 1u)
        return NormalMap.SampleBias(_NormalMap_sampler, uv, 1.25);
    if (k == 2u)
        return GlowMap.SampleLevel(_GlowMap_sampler, uv, 1.5);
    if (k == 3u)
        return BacklightMap.SampleGrad(_BacklightMap_sampler, uv, float2(scl * 1.7, 0.0), float2(0.0, scl * 0.6));
    if (k == 4u)
        return SpecularMap.Sample(_SpecularMap_sampler, uv);
    if (k == 5u)
        return GreyscaleMap.SampleLevel(_GreyscaleMap_sampler, uv, 0.0);
    if (k == 6u)
        return EnvironmentMap.SampleBias(_EnvironmentMap_sampler, uv, -0.75);
    return wwBlMap(7).Sample(wwBlSmp(7), uv);
}
