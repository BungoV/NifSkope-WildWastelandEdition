// Shared HLSL for res/shaders/features/ao/cell_ao.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellAoFactor) || defined(WW_FN_ALL)
float cellAoFactor()
{
    if (!cellAoOn)
    {
        return 1.0f;
    }
    float2 uv = (gl_FragCoord.xy - cellAoRect.xy) * cellAoRect.zw;
    return cellAo.Sample(_cellAo_sampler, float2(uv.x, 1.0f - uv.y)).x;
}
#endif

#if defined(WW_FN_cellAoShared) || defined(WW_FN_ALL)
float cellAoShared(float baked)
{
    return cellAoFactor() * clamp(baked, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellAoPassOut) || defined(WW_FN_ALL)
float4 cellAoPassOut(float3 normalView, float3 posView)
{
    float z = (-posView.z) * length(cellRow[0].xyz);
    if (cellAoZOnly)
    {
        return z.xxxx;
    }
    return float4(normalize(normalView), z);
}
#endif
