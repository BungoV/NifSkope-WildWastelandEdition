// Shared HLSL for res/shaders/features/water/water_surf.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwSurfTau, wwSurfLaw, wwWaterSurf
#ifdef WW_FN_ALL
static const float kSurfG = 686.6;
#endif

#if defined(WW_FN_wwSurfK) || defined(WW_FN_ALL)
float wwSurfK(float om, float h)
{
    float k0 = (om * om) / 686.5999755859375f;
    return k0 / pow(tanh(pow(k0 * max(h, 0.00999999977648258209228515625f), 0.75f)), 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwSurfFoamAlpha) || defined(WW_FN_ALL)
float wwSurfFoamAlpha(float D_1, float2 P, float foot, float t)
{
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return 0.0f;
    }
    float2 tmp1 = P;
    float tmp2 = foot;
    float tmp3 = t;
    float3 det = wwFoamDetail(tmp1, tmp2, tmp3);
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return ((a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1)) * waterSurfK.w;
}
#endif
