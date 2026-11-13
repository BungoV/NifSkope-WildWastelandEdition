// Shared HLSL for res/shaders/features/water/water_flowwaves.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterFlowSmooth) || defined(WW_FN_ALL)
float2 wwWaterFlowSmooth(float2 P)
{
    float tex = waterFlowGrid.z;
    float2 g = ((P - waterFlowGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterFlowGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 wwWaterFlowSmoothRet4 = c0;
    float2 wwWaterFlowSmoothRet3 = c0 + float2(tex, 0.0f);
    float2 wwWaterFlowSmoothRet2 = c0 + float2(0.0f, tex);
    float2 wwWaterFlowSmoothRet = c0 + tex.xx;
    return lerp(lerp(wwWaterFlow(wwWaterFlowSmoothRet4), wwWaterFlow(wwWaterFlowSmoothRet3), f.x.xx), lerp(wwWaterFlow(wwWaterFlowSmoothRet2), wwWaterFlow(wwWaterFlowSmoothRet), f.x.xx), f.y.xx);
}
#endif

#if defined(WW_FN_wwOcFlowCorrAt) || defined(WW_FN_ALL)
float wwOcFlowCorrAt(int base, float x)
{
    int i = min(int(x), 14);
    float f = x - float(i);
    return lerp(waterOceanFlowCorr[base + (i / 4)][i % 4], waterOceanFlowCorr[base + ((i + 1) / 4)][(i + 1) % 4], f);
}
#endif

#if defined(WW_FN_wwOcFlowNorm) || defined(WW_FN_ALL)
float wwOcFlowNorm(float2 v, int set)
{
    float a = waterOceanFlow.z;
    float b = waterOceanFlow.w;
    int st = max(set, 0);
    // m
    float d = (length(v) * abs(waterOceanFlow.x - waterOceanFlow.y)) * 0.01428749971091747283935546875f;
    // in finest texels
    float r = d / (1.0f / (128.0f * waterOceanInvL[st].z));
    float x = clamp(2.0f * log2(max(r, 1.0f)), 0.0f, 15.0f);
    float c = dot(v, waterOceanWind.xy) / max(length(v), 9.999999717180685365747194737196e-10f);
    int wwWaterFlowSmoothRet4 = (st * 8) + 4;
    float wwWaterFlowSmoothRet3 = x;
    int wwWaterFlowSmoothRet2 = st * 8;
    float wwWaterFlowSmoothRet = x;
    float rho = lerp(wwOcFlowCorrAt(wwWaterFlowSmoothRet4, wwWaterFlowSmoothRet3), wwOcFlowCorrAt(wwWaterFlowSmoothRet2, wwWaterFlowSmoothRet), c * c);
    if (r < 1.0f)
    {
        // quadratic to 1 at no separation
        rho = 1.0f - (((1.0f - rho) * r) * r);
    }
    return rsqrt(max(((a * a) + (b * b)) + (((2.0f * rho) * a) * b), 9.9999999747524270787835121154785e-07f));
}
#endif

#if defined(WW_FN_wwWaterOceanFlowed) || defined(WW_FN_ALL)
WwOcean wwWaterOceanFlowed(float2 P, float2 dPx, float2 dPy)
{
    float2 wwWaterFlowSmoothRet4;
    if ((waterOceanRed & 256) != 0)
    {
        wwWaterFlowSmoothRet4 = 0.0f.xx;
    }
    else
    {
        float2 wwWaterFlowSmoothRet3 = P;
        bool wwWaterFlowSmoothRet2 = (waterOceanRed & 1024) != 0;
        bool wwWaterFlowSmoothRet;
        if (!wwWaterFlowSmoothRet2)
        {
            float2 wwWaterFlowSmoothPart = P;
            wwWaterFlowSmoothRet = abs(wwWaterInfo(wwWaterFlowSmoothPart).w - 1.0f) > 0.5f;
        }
        else
        {
            wwWaterFlowSmoothRet = wwWaterFlowSmoothRet2;
        }
        wwWaterFlowSmoothRet4 = wwWaterFlowSmooth(wwWaterFlowSmoothRet3) * (wwWaterFlowSmoothRet ? 1.0f : 69.99124908447265625f);
    }
    float2 v = wwWaterFlowSmoothRet4;
    if (all(bool2(v.x == 0.0f.xx.x, v.y == 0.0f.xx.y)))
    {
        float2 wwWaterOceanFlowedRet3 = P;
        float2 wwWaterOceanFlowedRet2 = dPx;
        float2 wwWaterOceanFlowedRet = dPy;
        return wwWaterOcean(wwWaterOceanFlowedRet3, wwWaterOceanFlowedRet2, wwWaterOceanFlowedRet);
    }
    float2 APart5 = P;
    float2 APart4 = P - (v * waterOceanFlow.x);
    float2 APart3 = dPx;
    float2 APart2 = dPy;
    int APick;
    WwOcean APart = wwWaterOceanQ(APart5, APart4, APart3, APart2, APick);
    int set = APick;
    WwOcean A_1 = APart;
    float2 BPart5 = P;
    float2 BPart4 = P - (v * waterOceanFlow.y);
    float2 BPart3 = dPx;
    float2 BPart2 = dPy;
    int BPick;
    WwOcean BPart = wwWaterOceanQ(BPart5, BPart4, BPart3, BPart2, BPick);
    set = BPick;
    WwOcean B = BPart;
    float a = waterOceanFlow.z;
    float b = waterOceanFlow.w;
    float2 nPart2 = v;
    int nPart = set;
    float n = wwOcFlowNorm(nPart2, nPart);
    WwOcean o = A_1;
    o.slope = ((A_1.slope * a) + (B.slope * b)) * n;
    o.var = (((a * a) * A_1.var) + ((b * b) * B.var)) / max((a * a) + (b * b), 9.9999999747524270787835121154785e-07f);
    o.white = (a * A_1.white) + (b * B.white);
    o.cau = (a * A_1.cau) + (b * B.cau);
    return o;
}
#endif
