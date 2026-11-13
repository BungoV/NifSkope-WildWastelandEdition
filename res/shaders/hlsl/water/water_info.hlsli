// Shared HLSL for res/shaders/features/water/water_info.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwSeamFade) || defined(WW_FN_ALL)
float wwSeamFade(float2 worldXY)
{
    float2 a = worldXY - waterSeamBox.xy;
    float2 b = waterSeamBox.zw - worldXY;
    return clamp(min(min(a.x, a.y), min(b.x, b.y)) / max(waterSeam.x, 1.0f), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwWaterInfoRaw) || defined(WW_FN_ALL)
uint wwWaterInfoRaw(float2 worldXY)
{
    if (!waterInfoOn)
    {
        return 16711680u;
    }
    int2 t = int2(floor((worldXY - waterInfoGrid.xy) / waterInfoGrid.z.xx));
    int E = int(waterInfoLayout.z);
    int2 tile = t / E.xx;
    bool cond4 = t.x < 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = t.y < 0;
    }
    else
    {
        cond3 = cond4;
    }
    bool cond2;
    if (!cond3)
    {
        cond2 = tile.x >= int(waterInfoLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterInfoLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 16711680u;
    }
    uint2 ix = waterInfoIndex.Load(int3(tile, 0)).xy;
    if (ix.x == 0u)
    {
        return ix.y;
    }
    int slot = int(ix.x) - 1;
    int perRow = int(waterInfoLayout.w);
    return waterInfoAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwWaterInfoWord) || defined(WW_FN_ALL)
uint wwWaterInfoWord(float2 worldXY)
{
    float2 cond4 = worldXY;
    uint w = wwWaterInfoRaw(cond4);
    bool wwh0 = (waterSeam.x <= 0.0f);
    if (wwh0 || (!waterInfoOn))
    {
        return w;
    }
    uint id = w & 65535u;
    uint sea = uint(waterSeam.y);
    if (id == 0u)
    {
        uint cond3;
        if (waterSeam.z > 0.0f)
        {
            cond3 = sea | 4294901760u;
        }
        else
        {
            cond3 = w;
        }
        return cond3;
    }
    if (id != sea)
    {
        return w;
    }
    float2 cond2 = worldXY;
    float f = wwSeamFade(cond2);
    uint sh = uint(lerp(255.0f, float((w >> uint(16)) & 255u), f) + 0.5f);
    uint dc = uint(lerp(255.0f, float(w >> uint(24)), f) + 0.5f);
    return (id | (sh << uint(16))) | (dc << uint(24));
}
#endif

#if defined(WW_FN_wwFarWaterDry) || defined(WW_FN_ALL)
bool wwFarWaterDry(float2 worldXY)
{
    bool cond4;
    if (waterInfoOn)
    {
        float2 cond3 = worldXY;
        cond4 = (wwWaterInfoRaw(cond3) & 65535u) == 0u;
    }
    else
    {
        cond4 = waterInfoOn;
    }
    return cond4;
}
#endif

#if defined(WW_FN_wwWaterFlow) || defined(WW_FN_ALL)
float2 wwWaterFlow(float2 worldXY)
{
    if (!waterFlowOn)
    {
        return 0.0f.xx;
    }
    int2 t = int2(floor((worldXY - waterFlowGrid.xy) / waterFlowGrid.z.xx));
    int E = int(waterFlowLayout.z);
    int2 tile = t / E.xx;
    bool cond4 = t.x < 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = t.y < 0;
    }
    else
    {
        cond3 = cond4;
    }
    bool cond2;
    if (!cond3)
    {
        cond2 = tile.x >= int(waterFlowLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterFlowLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0.0f.xx;
    }
    uint2 ix = waterFlowIndex.Load(int3(tile, 0)).xy;
    uint w = ix.y;
    if (ix.x != 0u)
    {
        int slot = int(ix.x) - 1;
        int perRow = int(waterFlowLayout.w);
        w = waterFlowAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
    }
    float2 v = float2(float(int(w << 16u) >> 16), float(int(w) >> 16)) / waterFlowGrid.w.xx;
    float2 wwWaterFlowRet;
    if (waterSeam.x > 0.0f)
    {
        float2 wwWaterFlowPart = worldXY;
        wwWaterFlowRet = v * wwSeamFade(wwWaterFlowPart);
    }
    else
    {
        wwWaterFlowRet = v;
    }
    return wwWaterFlowRet;
}
#endif

#if defined(WW_FN_wwWaterInfo) || defined(WW_FN_ALL)
float4 wwWaterInfo(float2 worldXY)
{
    float2 cond4 = worldXY;
    uint w = wwWaterInfoWord(cond4);
    uint id = w & 65535u;
    float code = float(w >> uint(24)) / 255.0f;
    float cond3;
    if (id != 0u)
    {
        cond3 = waterInfoBodies.Load(int3(int2(int(id % 256u), int(id / 256u)), 0)).y;
    }
    else
    {
        cond3 = -1.0f;
    }
    float cls = cond3;
    float cond2;
    if (waterInfoOn)
    {
        cond2 = waterInfoGrid.w;
    }
    else
    {
        cond2 = 32.0f;
    }
    float q = cond2;
    return float4(float((w >> uint(16)) & 255u) * q, (code * code) * 8192.0f, float(id), cls);
}
#endif
