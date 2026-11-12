// Shared HLSL for res/shaders/features/water/water_skyvis.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSkyVisDir) || defined(WW_FN_ALL)
float wwWaterSkyVisDir(float3 P, float3 N, float3 D_1, float farV)
{
    bool tmp1 = !waterSkyVisOn;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = (waterSkyVisRed & 1) != 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3 = tmp2 || true;
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = !(cellGiSky && false);
    }
    else
    {
        tmp4 = tmp3;
    }
    if ((tmp4 || cellInterior) || (cellSpecSkyBase < 1.0f))
    {
        return farV;
    }
    float3 t = ((P + (N * (0.5f * cellGiVoxel))) - cellGiOrigin) / cellGiVoxel.xxx;
    float z = clamp(t.z, 0.5f, cellGiDims.z - 0.5f);
    float3 sl = float3((D_1.x >= 0.0f) ? 0.0f : 1.0f, (D_1.y >= 0.0f) ? 2.0f : 3.0f, (D_1.z >= 0.0f) ? 4.0f : 5.0f);
    float3 w = D_1 * D_1;
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float2 a = 0.0f.xx;
    for (int k = 0; k < 3; k++)
    {
        float4 s = cellGi.Sample(_cellGi_sampler, float3(t.xy / cellGiDims.xy, (((cellSpecSkyBase + sl[k]) * cellGiDims.z) + z) / depth));
        a += (s.xw * w[k]);
    }
    if (a.y < 0.00999999977648258209228515625f)
    {
        return farV;
    }
    float2 tmp5 = t.xy;
    return lerp(farV, clamp(a.x / a.y, 0.0f, 1.0f), cellGiEdgeFade(tmp5));
}
#endif

#if defined(WW_FN_wwWaterSkyVis) || defined(WW_FN_ALL)
float wwWaterSkyVis(float3 P, float3 N, float3 R, float rough, float farV)
{
    float r = clamp(rough, 0.0f, 1.0f);
    float3 tmp1 = P;
    float3 tmp2 = N;
    float3 tmp3 = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    float tmp4 = farV;
    return wwWaterSkyVisDir(tmp1, tmp2, tmp3, tmp4);
}
#endif

#if defined(WW_FN_wwWaterSkyVisSun) || defined(WW_FN_ALL)
float wwWaterSkyVisSun(float3 P, float3 N, float3 L, float farV)
{
    float tmp1;
    if ((waterSkyVisRed & 2) != 0)
    {
        tmp1 = farV;
    }
    else
    {
        float3 tmp2 = P;
        float3 tmp3 = N;
        float3 tmp4 = L;
        float tmp5 = farV;
        tmp1 = wwWaterSkyVisDir(tmp2, tmp3, tmp4, tmp5);
    }
    return tmp1;
}
#endif
