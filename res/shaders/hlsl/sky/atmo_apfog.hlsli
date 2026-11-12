// Shared HLSL for res/shaders/features/sky/atmo_apfog.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_ddgiprobe_ps.hlsl, cell_fardots_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volsky_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_fog_ps.hlsl, fo4_fogcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, lookdev_ground_csm_ps.hlsl, lookdev_ground_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl, pbrm_default_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwApFogCombine

#if defined(WW_FN_wwApFogResidualT) || defined(WW_FN_ALL)
float wwApFogResidualT(float tAp, float tFog, bool doubled)
{
    if (doubled)
    {
        return tFog;
    }
    return clamp(tFog / max(tAp, 9.9999999747524270787835121154785e-07f), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwApFogResidualL) || defined(WW_FN_ALL)
float3 wwApFogResidualL(float3 L, float tFull, float tRes)
{
    if (tRes >= 1.0f)
    {
        return 0.0f.xxx;
    }
    return L * clamp((1.0f - tRes) / max(1.0f - tFull, 9.9999999747524270787835121154785e-07f), 0.0f, 1.0f);
}
#endif
