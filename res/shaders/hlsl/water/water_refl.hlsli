// Shared HLSL for res/shaders/features/water/water_refl.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWaterPi = 3.14159265;
#endif

#if defined(WW_FN_wwLayerVar) || defined(WW_FN_ALL)
float wwLayerVar(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 tap)
{
    float2 t = (tap * 2.0f) - 1.0f.xx;
    float2 e = m.Sample(_m_sampler, uv).xy;
    return max(e.x - (t.x * t.x), 0.0f) + max(e.y - (t.y * t.y), 0.0f);
}
#endif

#if defined(WW_FN_wwLayerVarAll) || defined(WW_FN_ALL)
float wwLayerVarAll(Texture2D<float4> m, SamplerState _m_sampler)
{
    float2 e = m.SampleLevel(_m_sampler, 0.5f.xx, 20.0f).xy;
    return e.x + e.y;
}
#endif

#if defined(WW_FN_wwWaterAlpha2) || defined(WW_FN_ALL)
float wwWaterAlpha2(float2 uv0, float2 uv1, float2 uv2, float2 tap0, float2 tap1, float2 tap2, float4 Amp, float dfade, float flatten, float specPow, float3 N, bool lean)
{
    float a0 = 2.0f / ((4.0f * max(specPow, 1.0f)) + 2.0f);
    if (!lean)
    {
        return a0;
    }
    float2 vPart6 = uv0;
    float2 vPart5 = tap0;
    float2 vPart4 = uv1;
    float2 vPart3 = tap1;
    float2 vPart2 = uv2;
    float2 vPart = tap2;
    float v = ((Amp.x * Amp.x) * wwLayerVar(waterMoment0, _waterMoment0_sampler, vPart6, vPart5)) + ((dfade * dfade) * (((Amp.y * Amp.y) * wwLayerVar(waterMoment1, _waterMoment1_sampler, vPart4, vPart3)) + ((Amp.z * Amp.z) * wwLayerVar(waterMoment2, _waterMoment2_sampler, vPart2, vPart))));
    float z = 1.0f + ((Amp.y + Amp.z) * dfade);
    float lost = (1.0f - (dfade * dfade)) * (((Amp.y * Amp.y) * wwLayerVarAll(waterMoment1, _waterMoment1_sampler)) + ((Amp.z * Amp.z) * wwLayerVarAll(waterMoment2, _waterMoment2_sampler)));
    float zl = (1.0f + Amp.y) + Amp.z;
    float slopeVar = (((v / (z * z)) + (lost / (zl * zl))) * flatten) * flatten;
    float2 s = N.xy / max(N.z, 0.0500000007450580596923828125f).xx;
    float2 dx = ddx(s);
    float2 dy = ddy(s);
    float foot = min(0.5f * (dot(dx, dx) + dot(dy, dy)), waterGlintK.z);
    return (a0 + slopeVar) + foot;
}
#endif

#if defined(WW_FN_wwWaterFresnel) || defined(WW_FN_ALL)
float wwWaterFresnel(float NoV, float alpha, float F0)
{
    float c = 1.0f - clamp(NoV, 0.0f, 1.0f);
    float c2 = c * c;
    return F0 + ((((max(1.0f - alpha, F0) - F0) * c2) * c2) * c);
}
#endif

#if defined(WW_FN_wwWaterGlare) || defined(WW_FN_ALL)
float3 wwWaterGlare(float3 Ns, float3 V, float3 L, float3 sun, float a2, float F0, bool shoulder)
{
    float3 Vo = -V;
    float NoL = dot(Ns, L);
    float NoV = max(dot(Ns, Vo), 0.001000000047497451305389404296875f);
    if (NoL <= 0.0f)
    {
        return 0.0f.xxx;
    }
    float3 H = normalize(L + Vo);
    float NoH = clamp(dot(Ns, H), 0.0f, 1.0f);
    float LoH = clamp(dot(L, H), 0.0f, 1.0f);
    float d = ((NoH * NoH) * (a2 - 1.0f)) + 1.0f;
    float D_1 = a2 / ((3.1415927410125732421875f * d) * d);
    float vis = 0.5f / ((NoL * sqrt(((NoV * NoV) * (1.0f - a2)) + a2)) + (NoV * sqrt(((NoL * NoL) * (1.0f - a2)) + a2)));
    float c = 1.0f - LoH;
    float F = F0 + ((((((1.0f - F0) * c) * c) * c) * c) * c);
    float g = (((3.1415927410125732421875f * D_1) * vis) * F) * NoL;
    if (shoulder)
    {
        g /= (1.0f + (g / waterGlintK.x));
    }
    return sun * g;
}
#endif

#if defined(WW_FN_wwWaterCloudBand) || defined(WW_FN_ALL)
float wwWaterCloudBand(float3 P, float3 R, float z, float outside, int ch)
{
    float t = (z - P.z) / R.z;
    if (t <= 0.0f)
    {
        return 0.0f;
    }
    float2 uv = (((P.xy + (R.xy * t)) - waterCloudGeo.xy) / (2.0f * waterCloudGeo.z).xx) + 0.5f.xx;
    bool vPart6 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool vPart5;
    if (!vPart6)
    {
        vPart5 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        vPart5 = vPart6;
    }
    if (vPart5)
    {
        return outside;
    }
    if ((waterReflRed & 16) != 0)
    {
        return waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch];
    }
    // lane SKYSEA1: fade to the band's mean cover over the map's outer 15% (the hard square drew a line on the sea)
    float2 e2 = min(uv, 1.0f.xx - uv);
    return lerp(outside, waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch], smoothstep(0.0f, 0.1500000059604644775390625f, min(e2.x, e2.y)));
}
#endif

#if defined(WW_FN_wwWaterCloudSky) || defined(WW_FN_ALL)
float3 wwWaterCloudSky(float3 sky, float3 P, float3 R)
{
    bool graze = (waterReflRed & 32) == 0;
    bool vPart6 = waterCloudGeo.w < 0.5f;
    bool vPart5;
    if (!vPart6)
    {
        vPart5 = (waterReflRed & 8) != 0;
    }
    else
    {
        vPart5 = vPart6;
    }
    bool vPart4;
    if (!vPart5)
    {
        vPart4 = (R.z <= 0.0f) && (!graze);
    }
    else
    {
        vPart4 = vPart5;
    }
    if (vPart4)
    {
        return sky;
    }
    float covMean = 1.0f - (((1.0f - waterCloudCov.x) * (1.0f - waterCloudCov.y)) * (1.0f - waterCloudCov.z));
    float cov = covMean;
    if (R.z > 0.0f)
    {
        float3 vPart3 = P;
        float3 vPart2 = R;
        float vPart = waterCloudZ.x;
        float clPart2 = waterCloudCov.x;
        int clPart = 0;
        float cl = wwWaterCloudBand(vPart3, vPart2, vPart, clPart2, clPart);
        float3 cmPart5 = P;
        float3 cmPart4 = R;
        float cmPart3 = waterCloudZ.y;
        float cmPart2 = waterCloudCov.y;
        int cmPart = 1;
        float cm = wwWaterCloudBand(cmPart5, cmPart4, cmPart3, cmPart2, cmPart);
        float3 chPart5 = P;
        float3 chPart4 = R;
        float chPart3 = waterCloudZ.z;
        float chPart2 = waterCloudCov.z;
        int chPart = 3;
        float ch = wwWaterCloudBand(chPart5, chPart4, chPart3, chPart2, chPart);
        cov = 1.0f - (((1.0f - cl) * (1.0f - cm)) * (1.0f - ch));
    }
    // the far band crowds into the horizon line under R.z 0.08 (flat water: past 12.5 x the eye's height). Lane FARWATER1:
    // there the cover goes to the bands' mean, the deck the drawn sky shows at the horizon; the red (WW_CELL_WATER_REFL_RED=
    // grazing) lets it go to the clear WTHR gradient, the deep blue band on the far water under an overcast
    float g = smoothstep(0.0f, 0.07999999821186065673828125f, R.z);
    float covPick;
    if (graze)
    {
        covPick = lerp(covMean, cov, g);
    }
    else
    {
        covPick = cov * g;
    }
    cov = covPick;
    float l = max(max(dot(waterSky[0].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)), dot(waterSky[1].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))), dot(waterSky[2].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)));
    return lerp(sky, waterCloudTint * (1.5f * l), clamp(cov, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwWaterReflEnv) || defined(WW_FN_ALL)
float3 wwWaterReflEnv(float3 sky, float4 raw, float4 fin, bool haveSsr, float alpha)
{
    if (!haveSsr)
    {
        return sky;
    }
    float vPart6;
    if ((waterReflRed & 4) != 0)
    {
        vPart6 = 0.89999997615814208984375f;
    }
    else
    {
        vPart6 = smoothstep(waterGlintK.w, 8.0f * waterGlintK.w, alpha);
    }
    float w = vPart6;
    // lane FIX1101A: the blur fills the sharp march's holes (dither, refusals): no sky speckle on calm water
    if (waterSsrFlat)
    {
        w = max(w, clamp(fin.w - raw.w, 0.0f, 1.0f));
    }
    float4 s = lerp(raw, fin, w.xxxx);
    return lerp(sky, max(s.xyz, 0.0f.xxx), clamp(s.w, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwWaterReflPhys) || defined(WW_FN_ALL)
bool wwWaterReflPhys()
{
    bool vPart6;
    if (!waterReflOn)
    {
        vPart6 = waterClearOn && (waterClearRed != 4);
    }
    else
    {
        vPart6 = waterReflOn;
    }
    return vPart6;
}
#endif

#if defined(WW_FN_wwWaterReflMix) || defined(WW_FN_ALL)
float3 wwWaterReflMix(float3 body, float3 env, float F, float Fgame, float reflectivity, float3 far, float dfade)
{
    float3 vPart6;
    if ((waterReflRed & 1) != 0)
    {
        vPart6 = lerp(body, env, (Fgame * reflectivity).xxx);
    }
    else
    {
        vPart6 = lerp(body, env, F.xxx);
    }
    float3 c = vPart6;
    float3 vPart5;
    if ((waterReflRed & 2) != 0)
    {
        vPart5 = lerp(far, c, dfade.xxx);
    }
    else
    {
        vPart5 = c;
    }
    return vPart5;
}
#endif
