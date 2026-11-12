// Shared HLSL for res/shaders/features/water/water_skyfar.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSkyTexel) || defined(WW_FN_ALL)
uint wwWaterSkyTexel(int2 t)
{
    int E = int(waterSkyLayout.z);
    int2 tile = t / E.xx;
    bool tmp1 = t.x < 0;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = t.y < 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = tile.x >= int(waterSkyLayout.x);
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = tile.y >= int(waterSkyLayout.y);
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return 0u;
    }
    uint w = waterSkyIndex.Load(int3(tile, 0)).x;
    if ((w >> uint(8)) == 0u)
    {
        return w & 255u;
    }
    int slot = int(w >> uint(8)) - 1;
    int perRow = int(waterSkyLayout.w);
    return waterSkyAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwWaterSkyFar) || defined(WW_FN_ALL)
float wwWaterSkyFar(float3 P)
{
    if (!waterSkyOn)
    {
        return 1.0f;
    }
    float2 p = ((P.xy - waterSkyGrid.xy) / max(waterSkyGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 tmp1 = t;
    int2 tmp2 = t + int2(1, 0);
    int2 tmp3 = t + int2(0, 1);
    int2 tmp4 = t + int2(1, 1);
    float4 b = float4(float(wwWaterSkyTexel(tmp1)), float(wwWaterSkyTexel(tmp2)), float(wwWaterSkyTexel(tmp3)), float(wwWaterSkyTexel(tmp4)));
    float4 w = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y) * step(0.5f.xxxx, b);
    float den = dot(w, 1.0f.xxxx);
    float tmp5;
    if (den > 9.9999997473787516355514526367188e-05f)
    {
        tmp5 = dot(w, (b - 1.0f.xxxx) / waterSkyGrid.w.xxxx) / den;
    }
    else
    {
        tmp5 = 1.0f;
    }
    return tmp5;
}
#endif
