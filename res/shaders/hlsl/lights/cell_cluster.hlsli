// Shared HLSL for res/shaders/features/lights/cell_cluster.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellClusterOf) || defined(WW_FN_ALL)
int cellClusterOf(float3 P)
{
    int3 d = int3(cellClusterDims.xyz + 0.5f.xxx);
    int2 t = clamp(int2(floor((gl_FragCoord.xy - cellClusterOrigin) / cellClusterDims.w.xx)), int2(0, 0), d.xy - int2(1, 1));
    float s = floor((log(max(dot(cellClusterDepth.xyz, P) + cellClusterDepth.w, 9.9999999747524270787835121154785e-07f)) - cellClusterLog.x) * cellClusterLog.y);
    return (((clamp(int(s), 0, d.z - 1) * d.y) + t.y) * d.x) + t.x;
}
#endif

#if defined(WW_FN_cellLightsBegin) || defined(WW_FN_ALL)
int cellLightsBegin(float3 P, out int n)
{
    n = cellLightCount;
    if (!cellClusterOn)
    {
        return -1;
    }
    float3 cPart = P;
    int c = cellClusterOf(cPart);
    int k = cellCluster.Load((2 * c) + 1).x;
    if (k < 0)
    {
        return -1;
    }
    n = k;
    return cellClusterBase + cellCluster.Load(2 * c).x;
}
#endif

#if defined(WW_FN_cellLightAt) || defined(WW_FN_ALL)
int cellLightAt(int base, int k)
{
    int cPart;
    if (base < 0)
    {
        cPart = k;
    }
    else
    {
        cPart = cellCluster.Load(base + k).x;
    }
    return cPart;
}
#endif
