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
    float2 tmp1 = c0;
    float2 tmp2 = c0 + float2(tex, 0.0f);
    float2 tmp3 = c0 + float2(0.0f, tex);
    float2 tmp4 = c0 + tex.xx;
    return lerp(lerp(wwWaterFlow(tmp1), wwWaterFlow(tmp2), f.x.xx), lerp(wwWaterFlow(tmp3), wwWaterFlow(tmp4), f.x.xx), f.y.xx);
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
    float d = (length(v) * abs(waterOceanFlow.x - waterOceanFlow.y)) * 0.01428749971091747283935546875f;
    float r = d / (1.0f / (128.0f * waterOceanInvL[st].z));
    float x = clamp(2.0f * log2(max(r, 1.0f)), 0.0f, 15.0f);
    float c = dot(v, waterOceanWind.xy) / max(length(v), 9.999999717180685365747194737196e-10f);
    int tmp1 = (st * 8) + 4;
    float tmp2 = x;
    int tmp3 = st * 8;
    float tmp4 = x;
    float rho = lerp(wwOcFlowCorrAt(tmp1, tmp2), wwOcFlowCorrAt(tmp3, tmp4), c * c);
    if (r < 1.0f)
    {
        rho = 1.0f - (((1.0f - rho) * r) * r);
    }
    return rsqrt(max(((a * a) + (b * b)) + (((2.0f * rho) * a) * b), 9.9999999747524270787835121154785e-07f));
}
#endif

#if defined(WW_FN_wwWaterOceanFlowed) || defined(WW_FN_ALL)
WwOcean wwWaterOceanFlowed(float2 P, float2 dPx, float2 dPy)
{
    float2 tmp1;
    if ((waterOceanRed & 256) != 0)
    {
        tmp1 = 0.0f.xx;
    }
    else
    {
        float2 tmp2 = P;
        bool tmp3 = (waterOceanRed & 1024) != 0;
        bool tmp4;
        if (!tmp3)
        {
            float2 tmp5 = P;
            tmp4 = abs(wwWaterInfo(tmp5).w - 1.0f) > 0.5f;
        }
        else
        {
            tmp4 = tmp3;
        }
        tmp1 = wwWaterFlowSmooth(tmp2) * (tmp4 ? 1.0f : 69.99124908447265625f);
    }
    float2 v = tmp1;
    if (all(bool2(v.x == 0.0f.xx.x, v.y == 0.0f.xx.y)))
    {
        float2 tmp6 = P;
        float2 tmp7 = dPx;
        float2 tmp8 = dPy;
        return wwWaterOcean(tmp6, tmp7, tmp8);
    }
    float2 tmp9 = P;
    float2 tmp10 = P - (v * waterOceanFlow.x);
    float2 tmp11 = dPx;
    float2 tmp12 = dPy;
    int tmp13;
    WwOcean tmp14 = wwWaterOceanQ(tmp9, tmp10, tmp11, tmp12, tmp13);
    int set = tmp13;
    WwOcean A_1 = tmp14;
    float2 tmp15 = P;
    float2 tmp16 = P - (v * waterOceanFlow.y);
    float2 tmp17 = dPx;
    float2 tmp18 = dPy;
    int tmp19;
    WwOcean tmp20 = wwWaterOceanQ(tmp15, tmp16, tmp17, tmp18, tmp19);
    set = tmp19;
    WwOcean B = tmp20;
    float a = waterOceanFlow.z;
    float b = waterOceanFlow.w;
    float2 tmp21 = v;
    int tmp22 = set;
    float n = wwOcFlowNorm(tmp21, tmp22);
    WwOcean o = A_1;
    o.slope = ((A_1.slope * a) + (B.slope * b)) * n;
    o.var = (((a * a) * A_1.var) + ((b * b) * B.var)) / max((a * a) + (b * b), 9.9999999747524270787835121154785e-07f);
    o.white = (a * A_1.white) + (b * B.white);
    o.cau = (a * A_1.cau) + (b * B.cau);
    return o;
}
#endif
