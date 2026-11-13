// Shared HLSL for res/shaders/features/reflections/cell_ssr.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellSsrTexel) || defined(WW_FN_ALL)
float4 cellSsrTexel()
{
    float2 uv = (gl_FragCoord.xy - cellSsrRect.xy) * cellSsrRect.zw;
    return cellSsr.Sample(_cellSsr_sampler, float2(uv.x, 1.0f - uv.y));
}
#endif

#if defined(WW_FN_cellSsrMix) || defined(WW_FN_ALL)
float3 cellSsrMix(float3 cubeK, float gloss, float spec, float envScale)
{
    if (cellProbe == 60)
    {
        return 0.0f.xxx;
    }
    if (!cellSsrOn)
    {
        return cubeK;
    }
    float4 s = cellSsrTexel();
    // lane REFLPROBE1: the march's confidence takes the probes' share too
    cellReflAdd *= (1.0f - min(s.w, 1.0f));
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(clamp(gloss, 0.0f, 1.0f) - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    return lerp(cubeK, s.xyz * k, min(s.w, 1.0f).xxx);
}
#endif

#if defined(WW_FN_cellSsrProbeOut) || defined(WW_FN_ALL)
float4 cellSsrProbeOut(float3 sceneLinear)
{
    if (cellProbe == 60)
    {
        return float4(max(sceneLinear, 0.0f.xxx), float(cellSsrMat));
    }
    if (!cellSsrOn)
    {
        return 0.0f.xxxx;
    }
    float4 s = cellSsrTexel();
    float c = clamp(s.w, 0.0f, 1.0f);
    return float4(sqrt(clamp(float3(c, s.y * c, dot(s.xyz, 0.3333333432674407958984375f.xxx) * c), 0.0f.xxx, 1.0f.xxx)), 1.0f);
}
#endif
