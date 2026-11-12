// Shared HLSL for res/shaders/features/clouds/ww_droplet_phase.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl

#if defined(WW_FN_wwHg) || defined(WW_FN_ALL)
float wwHg(float g, float c)
{
    return (1.0f - (g * g)) / (12.56637096405029296875f * pow(max(9.9999997473787516355514526367188e-05f, (1.0f + (g * g)) - ((2.0f * g) * c)), 1.5f));
}
#endif

#if defined(WW_FN_wwDropletPhase) || defined(WW_FN_ALL)
float wwDropletPhase(float4 prm, float c, float k)
{
    float gh = prm.x * k;
    float gd = prm.y * k;
    float tmp1 = gd;
    float tmp2 = c;
    float dr = (wwHg(tmp1, tmp2) * (1.0f + ((prm.z * c) * c))) / (1.0f + ((prm.z * (1.0f + ((2.0f * gd) * gd))) / 3.0f));
    float tmp3 = gh;
    float tmp4 = c;
    return ((1.0f - prm.w) * wwHg(tmp3, tmp4)) + (prm.w * dr);
}
#endif

#if defined(WW_FN_wwIcePhase) || defined(WW_FN_ALL)
float wwIcePhase(float c, float k)
{
    float a = acos(clamp(c, -1.0f, 1.0f)) - 0.383971989154815673828125f;
    float ring = exp((-(a * a)) / 0.00030461768619716167449951171875f) / 0.072812400758266448974609375f;
    float tmp1 = 0.75f * k;
    float tmp2 = c;
    float tmp3 = 0.980000019073486328125f * k;
    float tmp4 = c;
    return ((0.87000000476837158203125f * wwHg(tmp1, tmp2)) + (0.100000001490116119384765625f * wwHg(tmp3, tmp4))) + (0.02999999932944774627685546875f * ((k * ring) + ((1.0f - k) / 12.56637096405029296875f)));
}
#endif
