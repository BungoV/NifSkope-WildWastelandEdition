// Shared HLSL for res/shaders/features/water/water_contact.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwContactLaw) || defined(WW_FN_ALL)
float wwContactLaw(float d, float hs, float pers, float t, float2 cxy, float foot)
{
    float hm = hs * 0.01428749971091747283935546875f;
    float tmp1;
    if ((waterContactRed & 2) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = clamp(pow(hm / waterShoreK2.w, 2.5f) * pers, 0.0f, 1.0f);
    }
    float amount = tmp1;
    float T = clamp(sqrt((6.283185482025146484375f * hm) / (waterContactK.z * 9.81000041961669921875f)), 1.0f, 12.0f);
    float W = clamp(waterContactK.x * hs, 20.99737548828125f, 419.947509765625f);
    float v = W / (waterContactK.y * T);
    float2 tmp2 = cxy / 140.0f.xx;
    float ph = ((t - (d / v)) / T) + (2.0f * wwShoreNoise(tmp2));
    float pulse = exp((-4.0f) * frac(ph));
    float env = 0.3499999940395355224609375f + (0.64999997615814208984375f * (0.5f + (0.5f * cos((6.283185482025146484375f * ph) / 7.0f))));
    if ((waterContactRed & 4) != 0)
    {
        pulse = 0.24539999663829803466796875f;
        env = 0.675000011920928955078125f;
    }
    pulse = lerp(pulse, 0.24539999663829803466796875f, smoothstep(0.25f, 0.5f, foot / (v * T)));
    float ring = (((amount * exp((-d) / W)) * pulse) * env) * (W / (W + foot));
    float lap = (((1.0f - smoothstep(0.0f, hs + foot, d)) * (hs / ((hs + foot) + 9.9999997473787516355514526367188e-05f))) * smoothstep(0.0f, waterShoreK3.w, hm)) * waterShoreK3.z;
    return clamp(max(ring, lap), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwWaterContact) || defined(WW_FN_ALL)
float4 wwWaterContact(float vert, float3 P, float3 S, float3 eye, float t)
{
    float tmp1 = ((waterContactRed & 1) != 0) ? 1000000000.0f : vert;
    float3 tmp2 = P;
    float3 tmp3 = S;
    float3 tmp4 = eye;
    float2 tmp5;
    float tmp6;
    float tmp7;
    float tmp8 = wwContactDist(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7);
    float2 dir = tmp5;
    float foot = tmp6;
    float trust = tmp7;
    float d = tmp8;
    bool wwh0 = ((!waterContactOn) || (trust <= 0.0f));
    bool wwh1 = (d > 100000000.0f);
    if (wwh0 || wwh1)
    {
        return float4(0.0f, d, trust, foot);
    }
    float2 tmp9 = P.xy;
    float4 wv = wwShoreWave(tmp9);
    float flowD = 0.0f;
    bool tmp10 = ((waterContactRed & 16) == 0) && waterFlowOn;
    bool tmp11;
    if (tmp10)
    {
        float2 tmp12 = P.xy;
        tmp11 = abs(wwWaterInfo(tmp12).w - 1.0f) < 0.5f;
    }
    else
    {
        tmp11 = tmp10;
    }
    if (tmp11)
    {
        float2 tmp13 = P.xy;
        float sm = length(wwWaterFlow(tmp13));
        float Wf = 69.99124908447265625f;
        flowD = (clamp((sm * sm) / 1.440000057220458984375f, 0.0f, 1.0f) * exp((-d) / Wf)) * (Wf / (Wf + foot));
    }
    if ((wv.w < 0.5f) && (flowD <= 0.0f))
    {
        return float4(0.0f, d, trust, foot);
    }
    float hs = max(wv.x, 0.001000000047497451305389404296875f);
    float cut = 1.0f;
    if ((waterContactRed & 8) == 0)
    {
        float2 tmp14 = P.xy;
        float2 field = wwShoreField(tmp14);
        if (field.y > 0.5f)
        {
            cut = smoothstep(1.5f, 2.5f, max(field.x - waterShoreK0.x, 0.0f) / ((d + waterShoreK4.z) + 1.0f));
        }
    }
    float tmp15;
    if (wv.w < 0.5f)
    {
        tmp15 = 0.0f;
    }
    else
    {
        float tmp16 = d;
        float tmp17 = hs;
        float tmp18 = wv.y;
        float tmp19 = t;
        float2 tmp20 = P.xy - (dir * d);
        float tmp21 = foot;
        tmp15 = wwContactLaw(tmp16, tmp17, tmp18, tmp19, tmp20, tmp21);
    }
    float wave = tmp15;
    float D_1 = (trust * cut) * max(wave, flowD);
    return float4(D_1, d, trust, foot);
}
#endif

#if defined(WW_FN_wwContactFoamAlpha) || defined(WW_FN_ALL)
float wwContactFoamAlpha(float D_1, float2 P, float foot, float t)
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
    return ((a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1)) * waterContactK.w;
}
#endif
