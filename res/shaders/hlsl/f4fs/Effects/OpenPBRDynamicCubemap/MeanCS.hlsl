// Stable broad radiance estimate for the selected Fallout t8 probe.
// Six face-center samples are deliberately fixed at source LOD 0.
//
// It also writes the source signature OpenPBRDynamicCubemapRuntime reads back,
// without waiting, to tell whether the prefilter has anything left to do
// (src/Effects/OpenPBRPrefilterRefresh.h): the selected probe and the view's
// size, then the raw bits of the selected probe at the six face centres and
// eight off-axis directions at LOD 0 (what PrefilterCS reads) and at the six
// faces of the coarsest mip (each face's average).

TextureCubeArray<float4> g_sourceProbes : register(t0);
StructuredBuffer<uint> g_selectedProbe : register(t1);
SamplerState g_linearClamp : register(s0);
// Point, clamped, every mip: g_linearClamp stops at LOD 0.
SamplerState g_coarsestMip : register(s1);
RWTexture2D<float4> g_outputMean : register(u0);
RWStructuredBuffer<uint4> g_signature : register(u1);

// Mirror of kSignatureEntries in src/Effects/OpenPBRPrefilterRefresh.h:
// [0] probe and view, [1..6] face centres, [7..12] coarsest mip, [13..20] off-axis.
static const uint kSignatureEntries = 21u;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    static const float3 directions[6] = {
        float3( 1, 0, 0), float3(-1, 0, 0),
        float3( 0, 1, 0), float3( 0,-1, 0),
        float3( 0, 0, 1), float3( 0, 0,-1)
    };
    // One per octant, away from face centres, edges and corners.
    static const float3 offAxis[8] = {
        float3( 0.8, 0.5, 0.3), float3(-0.3, 0.8, 0.5),
        float3( 0.5,-0.3, 0.8), float3(-0.8,-0.5, 0.3),
        float3( 0.3, 0.8,-0.5), float3(-0.5, 0.3,-0.8),
        float3( 0.8,-0.3,-0.5), float3(-0.3,-0.5,-0.8)
    };
    uint selectedProbe = g_selectedProbe[0];
    uint probeIndex = min(selectedProbe, 41u);
    float encodedProbe = float(probeIndex + 1u) * (1.0 / 64.0);
    float3 mean = 0.0;
    [unroll]
    for (uint face = 0u; face < 6u; ++face)
        mean += max(g_sourceProbes.SampleLevel(
            g_linearClamp, float4(directions[face], probeIndex), 0.0).rgb,
            0.0.xxx);
    // Alpha tags the selected probe so the ambient pass can reject slices
    // that still belong to the previous location during an eight-frame refresh.
    g_outputMean[uint2(0, 0)] =
        float4(mean * (1.0 / 6.0), encodedProbe);

    uint width, height, cubes, levels;
    g_sourceProbes.GetDimensions(0u, width, height, cubes, levels);
    float coarsestLod = float(max(levels, 1u) - 1u);
    g_signature[0] = uint4(selectedProbe, width, cubes, levels);
    [unroll]
    for (uint side = 0u; side < 6u; ++side)
    {
        float4 location = float4(directions[side], probeIndex);
        g_signature[1u + side] = asuint(g_sourceProbes.SampleLevel(
            g_linearClamp, location, 0.0));
        g_signature[7u + side] = asuint(g_sourceProbes.SampleLevel(
            g_coarsestMip, location, coarsestLod));
    }
    [unroll]
    for (uint tap = 0u; tap < 8u; ++tap)
        g_signature[13u + tap] = asuint(g_sourceProbes.SampleLevel(
            g_linearClamp, float4(offAxis[tap], probeIndex), 0.0));
}
