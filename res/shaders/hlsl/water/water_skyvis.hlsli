// Shared HLSL for res/shaders/features/water/water_skyvis.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSkyVisDir) || defined(WW_FN_ALL)
float wwWaterSkyVisDir(float3 P, float3 N, float3 D_1, float farV)
{
    bool cond3 = !waterSkyVisOn;
    bool condPick;
    if (!cond3)
    {
        condPick = (waterSkyVisRed & 1) != 0;
    }
    else
    {
        condPick = cond3;
    }
    bool cond2 = condPick || true;
    bool cond;
    if (!cond2)
    {
        cond = !(cellGiSky && false);
    }
    else
    {
        cond = cond2;
    }
    bool wwh0 = (cellSpecSkyBase < 1.0f);
    if ((cond || cellInterior) || wwh0)
    {
        return farV;
    }
    // texels: voxel i's centre is i + 0.5
    float3 t = ((P + (N * (0.5f * cellGiVoxel))) - cellGiOrigin) / cellGiVoxel.xxx;
    // the slabs stack on z: never filter across one
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
    float2 wwWaterSkyVisDirRet = t.xy;
    return lerp(farV, clamp(a.x / a.y, 0.0f, 1.0f), cellGiEdgeFade(wwWaterSkyVisDirRet));
}
#endif

#if defined(WW_FN_wwWaterSkyVis) || defined(WW_FN_ALL)
float wwWaterSkyVis(float3 P, float3 N, float3 R, float rough, float farV)
{
    float r = clamp(rough, 0.0f, 1.0f);
    float3 cond3 = P;
    float3 condPick = N;
    float3 cond2 = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    float cond = farV;
    return wwWaterSkyVisDir(cond3, condPick, cond2, cond);
}
#endif

#if defined(WW_FN_wwWaterSkyVisSun) || defined(WW_FN_ALL)
float wwWaterSkyVisSun(float3 P, float3 N, float3 L, float farV)
{
    float cond3;
    if ((waterSkyVisRed & 2) != 0)
    {
        cond3 = farV;
    }
    else
    {
        float3 condPick = P;
        float3 cond2 = N;
        float3 cond = L;
        float wwWaterSkyVisDirRet = farV;
        cond3 = wwWaterSkyVisDir(condPick, cond2, cond, wwWaterSkyVisDirRet);
    }
    return cond3;
}
#endif
