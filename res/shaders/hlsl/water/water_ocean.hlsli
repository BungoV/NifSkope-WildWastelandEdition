// Shared HLSL for res/shaders/features/water/water_ocean.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwOcWhite, wwWaterOceanQ, wwOcCascadeD, wwWaterOceanDisp
#ifdef WW_FN_ALL
static const float kOcMetre = 0.0142875;
static const float kOcGrid = 2.3255814;
static const float kOcPatchK = 1.602095;
#endif

#if defined(WW_FN_wwOcMix) || defined(WW_FN_ALL)
uint wwOcMix(inout uint x)
{
    x ^= (x >> uint(16));
    x *= 2146121005u;
    x ^= (x >> uint(15));
    x *= 2221713035u;
    x ^= (x >> uint(16));
    return x;
}
#endif

#if defined(WW_FN_wwOcHash01) || defined(WW_FN_ALL)
float wwOcHash01(int2 c, uint seed)
{
    uint2 u = uint2(c & int2(1023, 1023));
    uint tmp1 = ((u.x * 2376512323u) ^ (u.y * 3625334849u)) ^ seed;
    uint tmp2 = wwOcMix(tmp1);
    return float(tmp2) * 2.3283064365386962890625e-10f;
}
#endif

#if defined(WW_FN_wwOcValue) || defined(WW_FN_ALL)
float wwOcValue(float2 p, uint seed)
{
    float2 i = floor(p);
    float2 f = p - i;
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 c = int2(i);
    int2 tmp1 = c;
    uint tmp2 = seed;
    float a = wwOcHash01(tmp1, tmp2);
    int2 tmp3 = c + int2(1, 0);
    uint tmp4 = seed;
    float b = wwOcHash01(tmp3, tmp4);
    int2 tmp5 = c + int2(0, 1);
    uint tmp6 = seed;
    float d = wwOcHash01(tmp5, tmp6);
    int2 tmp7 = c + int2(1, 1);
    uint tmp8 = seed;
    float e = wwOcHash01(tmp7, tmp8);
    return lerp(lerp(a, b, f.x), lerp(d, e, f.x), f.y);
}
#endif

#if defined(WW_FN_wwOcPatch) || defined(WW_FN_ALL)
float wwOcPatch(float2 P, int set)
{
    float2 tmp1;
    if (set == 0)
    {
        tmp1 = waterOceanPatch.xy;
    }
    else
    {
        tmp1 = waterOceanPatch.zw;
    }
    float2 p = (P + tmp1) / waterOceanSetB[set].z.xx;
    float2 tmp2 = p;
    uint tmp3 = 625341585u;
    float2 tmp4 = (p * 2.0f) + float2(17.0f, 31.0f);
    uint tmp5 = 2654435769u;
    float n = (0.64999997615814208984375f * wwOcValue(tmp2, tmp3)) + (0.3499999940395355224609375f * wwOcValue(tmp4, tmp5));
    return 1.60209500789642333984375f * smoothstep(0.300000011920928955078125f, 0.699999988079071044921875f, n);
}
#endif

#if defined(WW_FN_wwOcOffset) || defined(WW_FN_ALL)
float2 wwOcOffset(int2 v, uint seed)
{
    uint tmp1 = ((uint(v.x) * 2376512323u) ^ (uint(v.y) * 3625334849u)) ^ seed;
    uint tmp2 = wwOcMix(tmp1);
    uint h = tmp2;
    uint tmp3 = h ^ 1759714724u;
    uint tmp4 = wwOcMix(tmp3);
    uint h2 = tmp4;
    return float2(float(h), float(h2)) * 2.3283064365386962890625e-10f;
}
#endif

#if defined(WW_FN_wwOcCascade) || defined(WW_FN_ALL)
float4 wwOcCascade(float2 uv, float2 gx, float2 gy, float layer, uint seed, out float dxy, out float lost)
{
    if ((waterOceanRed & 2) != 0)
    {
        float4 B = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv, layer), gx, gy);
        float4 C_1 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv, layer), gx, gy);
        dxy = C_1.w;
        lost = max(C_1.x - (B.x * B.x), 0.0f) + max(C_1.y - (B.y * B.y), 0.0f);
        return B;
    }
    float2 s = float2(uv.x, ((-0.57735025882720947265625f) * uv.x) + (1.1547005176544189453125f * uv.y)) * 2.3255813121795654296875f;
    int2 b = int2(floor(s));
    float2 f = s - float2(b);
    float z = (1.0f - f.x) - f.y;
    float3 w;
    int2 v0;
    int2 v1;
    int2 v2;
    if (z > 0.0f)
    {
        w = float3(z, f.y, f.x);
        v0 = b;
        v1 = b + int2(0, 1);
        v2 = b + int2(1, 0);
    }
    else
    {
        w = float3(-z, 1.0f - f.y, 1.0f - f.x);
        v0 = b + int2(1, 1);
        v1 = b + int2(1, 0);
        v2 = b + int2(0, 1);
    }
    int2 tmp1 = v0;
    uint tmp2 = seed;
    float4 B0 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(tmp1, tmp2), layer), gx, gy);
    int2 tmp3 = v1;
    uint tmp4 = seed;
    float4 B1 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(tmp3, tmp4), layer), gx, gy);
    int2 tmp5 = v2;
    uint tmp6 = seed;
    float4 B2 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(tmp5, tmp6), layer), gx, gy);
    int2 tmp7 = v0;
    uint tmp8 = seed;
    float4 C0 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(tmp7, tmp8), layer), gx, gy);
    int2 tmp9 = v1;
    uint tmp10 = seed;
    float4 C1 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(tmp9, tmp10), layer), gx, gy);
    int2 tmp11 = v2;
    uint tmp12 = seed;
    float4 C2 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(tmp11, tmp12), layer), gx, gy);
    float n2 = dot(w, w);
    float rn = rsqrt(max(n2, 9.9999999747524270787835121154785e-07f));
    dxy = (((w.x * C0.w) + (w.y * C1.w)) + (w.z * C2.w)) * rn;
    float3 l = float3(max(C0.x - (B0.x * B0.x), 0.0f) + max(C0.y - (B0.y * B0.y), 0.0f), max(C1.x - (B1.x * B1.x), 0.0f) + max(C1.y - (B1.y * B1.y), 0.0f), max(C2.x - (B2.x * B2.x), 0.0f) + max(C2.y - (B2.y * B2.y), 0.0f));
    lost = dot(w * w, l) / max(n2, 9.9999999747524270787835121154785e-07f);
    return (((B0 * w.x) + (B1 * w.y)) + (B2 * w.z)) * rn;
}
#endif

#if defined(WW_FN_wwOcHw) || defined(WW_FN_ALL)
float wwOcHw(float U, float F)
{
    return min((0.001599999959580600261688232421875f * U) * sqrt(F / 9.81000041961669921875f), ((0.20999999344348907470703125f * U) * U) / 9.81000041961669921875f);
}
#endif

#if defined(WW_FN_wwOcBody) || defined(WW_FN_ALL)
int wwOcBody(float2 P, inout float ws, out float a, out float3 gc, inout float m, inout float pw)
{
    ws = 0.0f;
    a = 0.0f;
    gc = 1.0f.xxx;
    m = 1.0f;
    pw = 0.0f;
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 tmp1 = c0;
    float2 tmp2 = c0 + float2(tex, 0.0f);
    float2 tmp3 = c0 + float2(0.0f, tex);
    float2 tmp4 = c0 + tex.xx;
    uint4 wd = uint4(wwWaterInfoWord(tmp1), wwWaterInfoWord(tmp2), wwWaterInfoWord(tmp3), wwWaterInfoWord(tmp4));
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = wd & uint4(65535u, 65535u, 65535u, 65535u);
    float U = waterOceanWind.z;
    float4 cset = (-1.0f).xxxx;
    float4 cgain = 0.0f.xxxx;
    float best = -1.0f;
    int set = -1;
    for (int k = 0; k < 4; k++)
    {
        if (id[k] == 0u)
        {
            continue;
        }
        float4 row = waterInfoBodies.Load(int3(int2(int(id[k] % 256u), int(id[k] / 256u)), 0));
        int cls = clamp(int(row.y + 0.5f), 0, 2);
        if ((int(row.z + 0.5f) & 16) != 0)
        {
            continue;
        }
        int s = (cls == 0) ? 0 : 1;
        float F = (sqrt(max(row.w, 1.0f)) * tex) * 0.01428749971091747283935546875f;
        if (cls == 1)
        {
            F = min(F, 40.0f);
        }
        float4 S = waterOceanSet[s];
        float tmp5 = U;
        float tmp6 = F;
        float tmp7 = U;
        float tmp8 = waterOceanSetB[s].x;
        float hw = (S.y * wwOcHw(tmp5, tmp6)) / max(wwOcHw(tmp7, tmp8), 9.9999999747524270787835121154785e-07f);
        cset[k] = float(s);
        cgain[k] = clamp(sqrt((hw * hw) + (S.z * S.z)) / max(S.x, 9.9999999747524270787835121154785e-07f), 0.1500000059604644775390625f, 3.0f);
        if (bw[k] > best)
        {
            best = bw[k];
            set = s;
        }
    }
    if (set < 0)
    {
        return -1;
    }
    float4 tmp9 = float(set).xxxx;
    float4 same = float4(bool4(cset.x == tmp9.x, cset.y == tmp9.y, cset.z == tmp9.z, cset.w == tmp9.w));
    ws = dot(bw, same);
    a = dot(bw * same, cgain) / max(ws, 9.9999999747524270787835121154785e-07f);
    float tmp10;
    if ((waterOceanRed & 4) != 0)
    {
        tmp10 = 1.0f;
    }
    else
    {
        float2 tmp11 = P;
        int tmp12 = set;
        tmp10 = wwOcPatch(tmp11, tmp12);
    }
    m = tmp10;
    float tmp13;
    if ((waterOceanRed & 4) != 0)
    {
        tmp13 = 0.0f;
    }
    else
    {
        tmp13 = 1.0f - (0.699999988079071044921875f * clamp((U - 3.0f) / 7.0f, 0.0f, 1.0f));
    }
    pw = tmp13;
    gc = float3(1.0f, lerp(1.0f, m, 0.5f * pw), lerp(1.0f, m, pw));
    return set;
}
#endif

#if defined(WW_FN_wwWaterOcean) || defined(WW_FN_ALL)
WwOcean wwWaterOcean(float2 P, float2 dPx, float2 dPy)
{
    float2 tmp1 = P;
    float2 tmp2 = P;
    float2 tmp3 = dPx;
    float2 tmp4 = dPy;
    int tmp5;
    WwOcean tmp6 = wwWaterOceanQ(tmp1, tmp2, tmp3, tmp4, tmp5);
    int set = tmp5;
    return tmp6;
}
#endif
