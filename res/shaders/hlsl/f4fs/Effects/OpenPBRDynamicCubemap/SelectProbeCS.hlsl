// Select the dominant Fallout t8 probe used by visible OpenPBR pixels.
// One 64-thread group scans a deterministic 64x32 screen grid and builds a
// 42-bin histogram. If no code-84 pixel is visible, the previous selection is
// retained so the persistent prefilter does not jump to an arbitrary probe.

cbuffer SelectProbeConstants : register(b0)
{
    uint4 g_selectParams; // x=width, y=height, z=material code, w=probe count
};

Texture2D<float4> g_probeMaterial : register(t0); // engine ambient t2; .y probe index
Texture2D<float4> g_shadingData : register(t1);   // engine ambient t3; .w material code
RWStructuredBuffer<uint> g_selectedProbe : register(u0);

groupshared uint g_probeCounts[64];

[numthreads(64, 1, 1)]
void main(uint3 groupThread : SV_GroupThreadID)
{
    uint lane = groupThread.x;
    g_probeCounts[lane] = 0u;
    GroupMemoryBarrierWithGroupSync();

    const uint gridWidth = 64u;
    const uint gridHeight = 32u;
    const uint sampleCount = gridWidth * gridHeight;
    [loop]
    for (uint sampleIndex = lane; sampleIndex < sampleCount; sampleIndex += 64u)
    {
        uint gridX = sampleIndex % gridWidth;
        uint gridY = sampleIndex / gridWidth;
        uint2 pixel = uint2(
            min(((gridX * 2u + 1u) * g_selectParams.x) / (gridWidth * 2u),
                max(g_selectParams.x, 1u) - 1u),
            min(((gridY * 2u + 1u) * g_selectParams.y) / (gridHeight * 2u),
                max(g_selectParams.y, 1u) - 1u));
        float4 shading = g_shadingData.Load(int3(pixel, 0));
        if (abs(shading.w * 255.0 - float(g_selectParams.z)) < 0.25)
        {
            float4 probeMaterial = g_probeMaterial.Load(int3(pixel, 0));
            int probeIndex = (int)floor(probeMaterial.y * 255.0 - 1.0);
            if (probeIndex >= 0 && probeIndex < (int)min(g_selectParams.w, 64u))
                InterlockedAdd(g_probeCounts[probeIndex], 1u);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    if (lane == 0u)
    {
        uint bestProbe = g_selectedProbe[0];
        uint bestCount = 0u;
        [loop]
        for (uint probe = 0u; probe < min(g_selectParams.w, 64u); ++probe)
        {
            if (g_probeCounts[probe] > bestCount)
            {
                bestCount = g_probeCounts[probe];
                bestProbe = probe;
            }
        }
        if (bestCount > 0u)
            g_selectedProbe[0] = bestProbe;
    }
}
