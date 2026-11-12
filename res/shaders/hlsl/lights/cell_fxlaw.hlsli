// Shared HLSL for res/shaders/features/lights/cell_fxlaw.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, particles_cell_ps.hlsl

#if defined(WW_FN_fxLocalOn) || defined(WW_FN_ALL)
bool fxLocalOn()
{
    return vpSun.w > 0.5f;
}
#endif

#if defined(WW_FN_fxLocalSky) || defined(WW_FN_ALL)
void fxLocalSky(float3 P, bool additive)
{
    fxAmbShare = 1.0f;
    fxLocalGi = 0.0f.xxx;
    fxLawRecv = 0.0f;
    if (((!fxLocalOn()) || cellInterior) || additive)
    {
        return;
    }
    if ((fxLocalRed & 2) == 0)
    {
        fxLawRecv = 1.0f;
    }
    if ((fxLocalRed & 1) != 0)
    {
        return;
    }
    float3 tmp1 = P;
    float3 tmp2 = float3(0.0f, 0.0f, 1.0f);
    fxAmbShare = 1.0f - cellGiSkyK(tmp1, tmp2);
    if (false)
    {
        float3 tmp3 = P;
        float3 tmp4 = float3(0.0f, 0.0f, 1.0f);
        fxLocalGi = cellGiE(tmp3, tmp4) * 0.31830990314483642578125f;
    }
}
#endif

#if defined(WW_FN_fxLawAdd) || defined(WW_FN_ALL)
float3 fxLawAdd(float3 G, float influence)
{
    return G * (max(influence, fxLawRecv) * (1.0f - fxLaw.x));
}
#endif

#if defined(WW_FN_fxLawUnit) || defined(WW_FN_ALL)
float fxLawUnit(bool additive)
{
    float uk = additive ? cellFxEmitK : cellFxUnlitK;
    return ((fxLaw.y > 0.5f) && (uk > 0.0f)) ? uk : 1.0f;
}
#endif

#if defined(WW_FN_fxLawMix) || defined(WW_FN_ALL)
float3 fxLawMix(float3 E, inout float influence, bool additive)
{
    influence = max(influence, fxLawRecv);
    bool tmp1 = additive;
    return lerp(fxLawUnit(tmp1).xxx, E, (influence * (1.0f - fxLaw.x)).xxx);
}
#endif

#if defined(WW_FN_fxLawMix_V2) || defined(WW_FN_ALL)
float3 fxLawMix(float3 E, float influence, bool additive)
{
    bool tmp1 = additive;
    return lerp(fxLawUnit(tmp1).xxx, E, (influence * (1.0f - fxLaw.x)).xxx);
}
#endif
