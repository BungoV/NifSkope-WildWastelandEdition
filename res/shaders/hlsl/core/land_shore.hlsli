// Shared HLSL for res/shaders/land_shore.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwLandShore

#if defined(WW_FN_wwLandShoreTexel) || defined(WW_FN_ALL)
uint wwLandShoreTexel(int2 t)
{
    if (!landShoreOn)
    {
        return 255u;
    }
    int E = int(landShoreLayout.z);
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
        tmp3 = tile.x >= int(landShoreLayout.x);
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = tile.y >= int(landShoreLayout.y);
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return 255u;
    }
    uint w = landShoreIndex.Load(int3(tile, 0)).x;
    if ((w >> uint(8)) == 0u)
    {
        return w & 255u;
    }
    int slot = int(w >> uint(8)) - 1;
    int perRow = int(landShoreLayout.w);
    return landShoreAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwLandShoreSmooth) || defined(WW_FN_ALL)
float wwLandShoreSmooth(float2 worldXY)
{
    float2 p = ((worldXY - landShoreGrid.xy) / max(landShoreGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 tmp1 = t;
    float a = float(wwLandShoreTexel(tmp1));
    int2 tmp2 = t + int2(1, 0);
    float b = float(wwLandShoreTexel(tmp2));
    int2 tmp3 = t + int2(0, 1);
    float c = float(wwLandShoreTexel(tmp3));
    int2 tmp4 = t + int2(1, 1);
    float d = float(wwLandShoreTexel(tmp4));
    float tmp5;
    if (landShoreOn)
    {
        tmp5 = landShoreGrid.w;
    }
    else
    {
        tmp5 = 32.0f;
    }
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * tmp5;
}
#endif
