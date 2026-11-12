// Shared HLSL for res/shaders/features/weather/rain_wet.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwRainWetW) || defined(WW_FN_ALL)
float wwRainWetW(float gloss, float metal)
{
    float por = clamp(lerp(1.1665999889373779296875f - (1.66600000858306884765625f * gloss), rainWet.y, rainWet.z), 0.0f, 1.0f);
    return (rainWet.x * por) * (1.0f - metal);
}
#endif

#if defined(WW_FN_wwRainWetMetal) || defined(WW_FN_ALL)
float wwRainWetMetal(float3 f0)
{
    return clamp((dot(f0, 0.3300000131130218505859375f.xxx) * 1000.0f) - 500.0f, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwRainWetSmooth) || defined(WW_FN_ALL)
float wwRainWetSmooth(float gloss, float w)
{
    return 1.0f - ((1.0f - (0.4000000059604644775390625f * w)) * (1.0f - gloss));
}
#endif

#if defined(WW_FN_wwRainCoverOpen) || defined(WW_FN_ALL)
float wwRainCoverOpen(int2 t, int2 n, float z)
{
    bool tmp1 = any(bool2(t.x < int2(0, 0).x, t.y < int2(0, 0).y));
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool2(t.x >= n.x, t.y >= n.y));
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    return float(rainCover.Load(int3(t, 0)).x <= z);
}
#endif

#if defined(WW_FN_wwRainWetExposed) || defined(WW_FN_ALL)
float wwRainWetExposed(float3 P, float3 N)
{
    if (rainWet.w < 0.5f)
    {
        return 1.0f;
    }
    float2 q = (((P.xy + (N.xy * (1.5f * rainCoverGrid.w))) - rainCoverGrid.xy) * rainCoverGrid.z) - 0.5f.xx;
    int2 t = int2(floor(q));
    float2 f = q - float2(t);
    int2 n = int2(spvTextureSize(rainCover, uint(0), dummy_parameter));
    float z = (P.z + 8.0f) + (rainCoverGrid.w * min(length(N.xy) / max(abs(N.z), 0.0500000007450580596923828125f), 2.0f));
    int2 tmp1 = t;
    int2 tmp2 = n;
    float tmp3 = z;
    float e00 = wwRainCoverOpen(tmp1, tmp2, tmp3);
    int2 tmp4 = t + int2(1, 0);
    int2 tmp5 = n;
    float tmp6 = z;
    float e10 = wwRainCoverOpen(tmp4, tmp5, tmp6);
    int2 tmp7 = t + int2(0, 1);
    int2 tmp8 = n;
    float tmp9 = z;
    float e01 = wwRainCoverOpen(tmp7, tmp8, tmp9);
    int2 tmp10 = t + int2(1, 1);
    int2 tmp11 = n;
    float tmp12 = z;
    float e11 = wwRainCoverOpen(tmp10, tmp11, tmp12);
    return lerp(lerp(e00, e10, f.x), lerp(e01, e11, f.x), f.y);
}
#endif
