// Shared HLSL for res/shaders/lookdev_output.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: atmo_sky_ps.hlsl, cloud_composite_ps.hlsl, lookdev_bill_ps.hlsl, lookdev_clouds_ps.hlsl, lookdev_dome_ps.hlsl, lookdev_ground_cell_ps.hlsl, lookdev_ground_csm_ps.hlsl, lookdev_ground_ps.hlsl, lookdev_sky_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl, pbrm_default_ps.hlsl, sky_moon_ps.hlsl

#if defined(WW_FN_srgbToLinear) || defined(WW_FN_ALL)
float3 srgbToLinear(float3 c)
{
    return lerp(c / 12.9200000762939453125f.xxx, pow((c + 0.054999999701976776123046875f.xxx) / 1.05499994754791259765625f.xxx, 2.400000095367431640625f.xxx), step(0.040449999272823333740234375f.xxx, c));
}
#endif

#if defined(WW_FN_linearToSrgb) || defined(WW_FN_ALL)
float3 linearToSrgb(inout float3 c)
{
    c = clamp(c, 0.0f.xxx, 1.0f.xxx);
    return lerp(c * 12.9200000762939453125f, (pow(c, 0.4166666567325592041015625f.xxx) * 1.05499994754791259765625f) - 0.054999999701976776123046875f.xxx, step(0.003130800090730190277099609375f.xxx, c));
}
#endif

#if defined(WW_FN_agxContrast) || defined(WW_FN_ALL)
float3 agxContrast(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return (((((((x4 * 15.5f) * x2) - ((x4 * 40.1399993896484375f) * x)) + (x4 * 31.95999908447265625f)) - ((x2 * 6.868000030517578125f) * x)) + (x2 * 0.4298000037670135498046875f)) + (x * 0.119099996984004974365234375f)) - 0.002319999970495700836181640625f.xxx;
}
#endif

#if defined(WW_FN_agxDisplay) || defined(WW_FN_ALL)
float3 agxDisplay(inout float3 c)
{
    c = mul(max(c, 1.0000000133514319600180897396058e-10f.xxx), float3x3(float3(0.842479050159454345703125f, 0.0423282422125339508056640625f, 0.0423756539821624755859375f), float3(0.0784336030483245849609375f, 0.87846863269805908203125f, 0.0784336030483245849609375f), float3(0.079223744571208953857421875f, 0.07916612923145294189453125f, 0.8791429996490478515625f)));
    c = clamp(log2(c), (-12.47393035888671875f).xxx, 4.026069164276123046875f.xxx);
    c = (c - (-12.47393035888671875f).xxx) / 16.4999980926513671875f.xxx;
    float3 tmp1 = c;
    c = agxContrast(tmp1);
    return clamp(mul(c, float3x3(float3(1.19687902927398681640625f, -0.0528968535363674163818359375f, -0.0529716350138187408447265625f), float3(-0.0980208814144134521484375f, 1.1519031524658203125f, -0.098043449223041534423828125f), float3(-0.099029742181301116943359375f, -0.098961174488067626953125f, 1.1510736942291259765625f))), 0.0f.xxx, 1.0f.xxx);
}
#endif

#if defined(WW_FN_pbrNeutral) || defined(WW_FN_ALL)
float3 pbrNeutral(inout float3 color)
{
    float x = min(color.x, min(color.y, color.z));
    float tmp1;
    if (x < 0.07999999821186065673828125f)
    {
        tmp1 = x - ((6.25f * x) * x);
    }
    else
    {
        tmp1 = 0.039999999105930328369140625f;
    }
    float offset = tmp1;
    color -= offset.xxx;
    float peak = max(color.x, max(color.y, color.z));
    if (peak < 0.7599999904632568359375f)
    {
        return color;
    }
    float newPeak = 1.0f - (0.057599999010562896728515625f / ((peak + 0.23999999463558197021484375f) - 0.7599999904632568359375f));
    color *= (newPeak / peak);
    float g = 1.0f - (1.0f / ((0.1500000059604644775390625f * (peak - newPeak)) + 1.0f));
    return lerp(color, newPeak.xxx, g.xxx);
}
#endif

#if defined(WW_FN_studioOutput) || defined(WW_FN_ALL)
float3 studioOutput(inout float3 c)
{
    c = max(c, 0.0f.xxx) * sceneExposure;
    if (viewTransform == 3)
    {
        return c;
    }
    if (viewTransform == 1)
    {
        float3 tmp1 = c;
        float3 tmp2 = agxDisplay(tmp1);
        return tmp2;
    }
    if (viewTransform == 2)
    {
        float3 tmp3 = c;
        float3 tmp4 = pbrNeutral(tmp3);
        float3 tmp5 = tmp4;
        float3 tmp6 = linearToSrgb(tmp5);
        return tmp6;
    }
    float3 tmp7 = c;
    float3 tmp8 = linearToSrgb(tmp7);
    return tmp8;
}
#endif

#if defined(WW_FN_studioOutput_V2) || defined(WW_FN_ALL)
float3 studioOutput(inout float3 c)
{
    c = max(c, 0.0f.xxx) * sceneExposure;
    if (viewTransform == 1)
    {
        float3 tmp1 = c;
        float3 tmp2 = agxDisplay(tmp1);
        return tmp2;
    }
    if (viewTransform == 2)
    {
        float3 tmp3 = c;
        float3 tmp4 = pbrNeutral(tmp3);
        float3 tmp5 = tmp4;
        float3 tmp6 = linearToSrgb(tmp5);
        return tmp6;
    }
    float3 tmp7 = c;
    float3 tmp8 = linearToSrgb(tmp7);
    return tmp8;
}
#endif
