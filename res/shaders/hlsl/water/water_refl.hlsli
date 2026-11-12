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
    float2 tmp1 = uv0;
    float2 tmp2 = tap0;
    float2 tmp3 = uv1;
    float2 tmp4 = tap1;
    float2 tmp5 = uv2;
    float2 tmp6 = tap2;
    float v = ((Amp.x * Amp.x) * wwLayerVar(waterMoment0, _waterMoment0_sampler, tmp1, tmp2)) + ((dfade * dfade) * (((Amp.y * Amp.y) * wwLayerVar(waterMoment1, _waterMoment1_sampler, tmp3, tmp4)) + ((Amp.z * Amp.z) * wwLayerVar(waterMoment2, _waterMoment2_sampler, tmp5, tmp6))));
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
    bool tmp1 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return outside;
    }
    if ((waterReflRed & 16) != 0)
    {
        return waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch];
    }
    float2 e2 = min(uv, 1.0f.xx - uv);
    return lerp(outside, waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch], smoothstep(0.0f, 0.1500000059604644775390625f, min(e2.x, e2.y)));
}
#endif

#if defined(WW_FN_wwWaterCloudSky) || defined(WW_FN_ALL)
float3 wwWaterCloudSky(float3 sky, float3 P, float3 R)
{
    bool graze = (waterReflRed & 32) == 0;
    bool tmp1 = waterCloudGeo.w < 0.5f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = (waterReflRed & 8) != 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = (R.z <= 0.0f) && (!graze);
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return sky;
    }
    float covMean = 1.0f - (((1.0f - waterCloudCov.x) * (1.0f - waterCloudCov.y)) * (1.0f - waterCloudCov.z));
    float cov = covMean;
    if (R.z > 0.0f)
    {
        float3 tmp4 = P;
        float3 tmp5 = R;
        float tmp6 = waterCloudZ.x;
        float tmp7 = waterCloudCov.x;
        int tmp8 = 0;
        float cl = wwWaterCloudBand(tmp4, tmp5, tmp6, tmp7, tmp8);
        float3 tmp9 = P;
        float3 tmp10 = R;
        float tmp11 = waterCloudZ.y;
        float tmp12 = waterCloudCov.y;
        int tmp13 = 1;
        float cm = wwWaterCloudBand(tmp9, tmp10, tmp11, tmp12, tmp13);
        float3 tmp14 = P;
        float3 tmp15 = R;
        float tmp16 = waterCloudZ.z;
        float tmp17 = waterCloudCov.z;
        int tmp18 = 3;
        float ch = wwWaterCloudBand(tmp14, tmp15, tmp16, tmp17, tmp18);
        cov = 1.0f - (((1.0f - cl) * (1.0f - cm)) * (1.0f - ch));
    }
    float g = smoothstep(0.0f, 0.07999999821186065673828125f, R.z);
    float tmp19;
    if (graze)
    {
        tmp19 = lerp(covMean, cov, g);
    }
    else
    {
        tmp19 = cov * g;
    }
    cov = tmp19;
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
    float tmp1;
    if ((waterReflRed & 4) != 0)
    {
        tmp1 = 0.89999997615814208984375f;
    }
    else
    {
        tmp1 = smoothstep(waterGlintK.w, 8.0f * waterGlintK.w, alpha);
    }
    float w = tmp1;
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
    bool tmp1;
    if (!waterReflOn)
    {
        tmp1 = waterClearOn && (waterClearRed != 4);
    }
    else
    {
        tmp1 = waterReflOn;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_wwWaterReflMix) || defined(WW_FN_ALL)
float3 wwWaterReflMix(float3 body, float3 env, float F, float Fgame, float reflectivity, float3 far, float dfade)
{
    float3 tmp1;
    if ((waterReflRed & 1) != 0)
    {
        tmp1 = lerp(body, env, (Fgame * reflectivity).xxx);
    }
    else
    {
        tmp1 = lerp(body, env, F.xxx);
    }
    float3 c = tmp1;
    float3 tmp2;
    if ((waterReflRed & 2) != 0)
    {
        tmp2 = lerp(far, c, dfade.xxx);
    }
    else
    {
        tmp2 = c;
    }
    return tmp2;
}
#endif
