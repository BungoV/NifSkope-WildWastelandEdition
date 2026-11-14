// Full-resolution GGX prefilter for the temporal local cubemap.
//
// This resource is intentionally a TextureCubeArray of roughness levels, not a
// mip chain. Every output level remains 128x128 and every source read is fixed
// at LOD 0. Roughness broadens the angular sample distribution only.

cbuffer PrefilterConstants : register(b0)
{
    // x = roughness, y = destination cube index, z = history weight,
    // w = independent sequence phase
    float4 g_prefilterParams;
};

TextureCubeArray<float4> g_sourceProbes : register(t0);
TextureCubeArray<float4> g_previousCubes : register(t1);
// t2 is not read. The broad mean MeanCS writes is the ambient pass's (t26).
StructuredBuffer<uint> g_selectedProbe : register(t3);
SamplerState g_linearClamp : register(s0);
RWTexture2DArray<float4> g_outputCubes : register(u0);

static const float F4FS_PI = 3.14159265358979323846;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint sampleCount)
{
    return float2(float(i) / float(sampleCount), RadicalInverseVdC(i));
}

uint PrefilterSampleCount(float roughness)
{
    // Raw Fallout probes contain much higher directional contrast than the
    // former camera-filled source. Use enough samples that the first result is
    // already usable while temporal accumulation removes the remaining noise.
    if (roughness < 0.20) return 64u;
    if (roughness < 0.50) return 128u;
    if (roughness < 0.75) return 192u;
    return 256u;
}

float DirectionalAzimuthRotation(float3 n, uint roughnessSlice)
{
    // The old identical tangent-space sequence on every output texel created
    // coherent radial/star patterns. Longitude is continuous modulo one, which
    // is exactly the topology of GGX azimuth, so this rotates neighbouring
    // directions without introducing a cubemap-face seam. Salt each roughness
    // slice to prevent adjacent full-resolution cubes sharing the same pattern.
    float longitude = atan2(n.y, n.x) * (0.5 / F4FS_PI) + 0.5;
    return frac(longitude + n.z * 0.38196601125 +
        float(roughnessSlice) * 0.61803398875);
}

float3 CubemapDirection(uint2 pixel, uint face, uint width, uint height)
{
    float2 st = (float2(pixel) + 0.5) / float2(width, height);
    float2 uv = float2(st.x * 2.0 - 1.0, 1.0 - st.y * 2.0);
    float3 d;
    switch (face)
    {
    case 0: d = float3( 1.0, uv.y, -uv.x); break;
    case 1: d = float3(-1.0, uv.y,  uv.x); break;
    case 2: d = float3( uv.x, 1.0, -uv.y); break;
    case 3: d = float3( uv.x,-1.0,  uv.y); break;
    case 4: d = float3( uv.x, uv.y,  1.0); break;
    default:d = float3(-uv.x, uv.y, -1.0); break;
    }
    return normalize(d);
}

void BuildBasis(float3 n, out float3 tangent, out float3 bitangent)
{
    float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0)
                                : float3(1.0, 0.0, 0.0);
    tangent = normalize(cross(up, n));
    bitangent = cross(n, tangent);
}

float3 ImportanceSampleGGX(float2 xi, float roughness, float3 n)
{
    // Disney/UE convention: GGX alpha is perceptualRoughness squared.
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float phi = 2.0 * F4FS_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) /
        max(1.0 + (alpha2 - 1.0) * xi.y, 1e-6));
    float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
    float3 hLocal = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    float3 tangent, bitangent;
    BuildBasis(n, tangent, bitangent);
    return normalize(tangent * hLocal.x + bitangent * hLocal.y + n * hLocal.z);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint width, height, layers;
    g_outputCubes.GetDimensions(width, height, layers);
    if (tid.x >= width || tid.y >= height || tid.z >= 6u)
        return;

    uint roughnessSlice = min((uint)(g_prefilterParams.y + 0.5), 7u);
    uint outputLayer = roughnessSlice * 6u + tid.z;
    uint probeIndex = min(g_selectedProbe[0], 41u);
    float encodedProbe = float(probeIndex + 1u) * (1.0 / 64.0);
    float roughness = saturate(g_prefilterParams.x);
    float3 n = CubemapDirection(tid.xy, tid.z, width, height);

    if (roughness <= 1e-4)
    {
        // LOD 0 is meant to stay like this. Lower-resolution cubemap mips cause
        // a visible loss of texture quality on rougher surfaces in Fallout 4's
        // already low-resolution probes.
        g_outputCubes[uint3(tid.xy, outputLayer)] =
            float4(max(g_sourceProbes.SampleLevel(
                g_linearClamp, float4(n, probeIndex), 0.0).rgb, 0.0.xxx),
                encodedProbe);
        return;
    }

    float4 accumulated = 0.0;
    float totalWeight = 0.0;
    float3 v = n;
    uint sampleCount = PrefilterSampleCount(roughness);
    // A global Cranley-Patterson shift preserves cubemap continuity while each
    // eight-frame refresh contributes a different deterministic sample set.
    float2 sequenceShift = frac(
        float2(0.754877666, 0.569840296) * g_prefilterParams.w);
    float azimuthRotation = DirectionalAzimuthRotation(n, roughnessSlice);
    [loop]
    for (uint i = 0u; i < sampleCount; ++i)
    {
        float2 xi = Hammersley(i, sampleCount);
        xi.x = frac(xi.x + sequenceShift.x + azimuthRotation);
        xi.y = frac(xi.y + sequenceShift.y);
        float3 h = ImportanceSampleGGX(xi, roughness, n);
        float3 l = normalize(2.0 * dot(v, h) * h - v);
        float noL = saturate(dot(n, l));
        if (noL > 0.0)
        {
			float3 source = max(g_sourceProbes.SampleLevel(
				g_linearClamp, float4(l, probeIndex), 0.0).rgb, 0.0.xxx);
			accumulated.rgb += source * noL;
            totalWeight += noL;
        }
    }
    float4 current = accumulated / max(totalWeight, 1e-4);
	float4 previous = g_previousCubes.SampleLevel(
		g_linearClamp, float4(n, roughnessSlice), 0.0);
	float sameProbe = abs(previous.a - encodedProbe) < (0.25 / 64.0) ? 1.0 : 0.0;
	float historyWeight = saturate(g_prefilterParams.z) * sameProbe;
    g_outputCubes[uint3(tid.xy, outputLayer)] =
		float4(lerp(current.rgb, previous.rgb, historyWeight), encodedProbe);
}
