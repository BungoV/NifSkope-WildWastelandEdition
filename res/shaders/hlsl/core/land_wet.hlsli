// Shared HLSL for res/shaders/land_wet.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwLandWet) || defined(WW_FN_ALL)
float2 wwLandWet(float3 P, float3 N)
{
    if ((!landWetOn) || (!landShoreOn))
    {
        return 0.0f.xx;
    }
    float2 dPart = P.xy;
    float d = wwLandShoreSmooth(dPart);
    if (d >= landWetK.x)
    {
        return 0.0f.xx;
    }
    float nz = clamp(N.z, 0.0500000007450580596923828125f, 1.0f);
    // the height above the water, along the slope
    float h = (d * sqrt(1.0f - (nz * nz))) / nz;
    float wvPick;
    if ((landWetRed & 1) != 0)
    {
        wvPick = 1.0f;
    }
    else
    {
        wvPick = 1.0f - smoothstep(0.0f, landWetK.y, h);
    }
    float wv = wvPick;
    float wet = (1.0f - smoothstep(0.0f, landWetK.x, d)) * wv;
    float filmPick;
    if ((landWetRed & 2) != 0)
    {
        filmPick = 0.0f;
    }
    else
    {
        filmPick = (1.0f - smoothstep(0.0f, landWetK.w, d)) * wv;
    }
    float film = filmPick;
    return float2(wet, film);
}
#endif

#if defined(WW_FN_wwLandWetBase) || defined(WW_FN_ALL)
float wwLandWetBase(float w, float por)
{
    float dPart;
    if ((landWetRed & 4) != 0)
    {
        dPart = 1.0f;
    }
    else
    {
        dPart = lerp(1.0f, landWetK.z, por * w);
    }
    return dPart;
}
#endif

#if defined(WW_FN_wwLandWetRough) || defined(WW_FN_ALL)
float wwLandWetRough(float rough, float2 wet, float por)
{
    float r = rough * lerp(1.0f, lerp(1.0f, landWetK.z, por), 0.5f * wet.x);
    return lerp(r, 0.0500000007450580596923828125f, wet.y);
}
#endif
