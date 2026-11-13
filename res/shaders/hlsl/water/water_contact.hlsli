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
    float amountPick;
    if ((waterContactRed & 2) != 0)
    {
        amountPick = 1.0f;
    }
    else
    {
        amountPick = clamp(pow(hm / waterShoreK2.w, 2.5f) * pers, 0.0f, 1.0f);
    }
    float amount = amountPick;
    // the period of a wind sea of this height at its usual steepness (L = Hs / s, deep water: T = sqrt( 2 pi L / g ))
    float T = clamp(sqrt((6.283185482025146484375f * hm) / (waterContactK.z * 9.81000041961669921875f)), 1.0f, 12.0f);
    // the foam band's e-fold width (u)
    float W = clamp(waterContactK.x * hs, 20.99737548828125f, 419.947509765625f);
    // the foam drifts out a band width in its life (u/s)
    float v = W / (waterContactK.y * T);
    float2 phPart = cxy / 140.0f.xx;
    // born at the contact when a wave strikes (bright), thinning until the next; the strike time drifts along the object
    float ph = ((t - (d / v)) / T) + (2.0f * wwShoreNoise(phPart));
    float pulse = exp((-4.0f) * frac(ph));
    // wave groups of 7
    float env = 0.3499999940395355224609375f + (0.64999997615814208984375f * (0.5f + (0.5f * cos((6.283185482025146484375f * ph) / 7.0f))));
    if ((waterContactRed & 4) != 0)
    {
        pulse = 0.24539999663829803466796875f;
        env = 0.675000011920928955078125f;
    }
    // rings closer than a few pixels show their mean, a band thinner than a pixel is fainter, not wider
    pulse = lerp(pulse, 0.24539999663829803466796875f, smoothstep(0.25f, 0.5f, foot / (v * T)));
    float ring = (((amount * exp((-d) / W)) * pulse) * env) * (W / (W + foot));
    // ripples run up the object about H: a thin translucent lap line (WATERFOAMDETAIL1's, from K3.w metres)
    float lap = (((1.0f - smoothstep(0.0f, hs + foot, d)) * (hs / ((hs + foot) + 9.9999997473787516355514526367188e-05f))) * smoothstep(0.0f, waterShoreK3.w, hm)) * waterShoreK3.z;
    return clamp(max(ring, lap), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwWaterContact) || defined(WW_FN_ALL)
float4 wwWaterContact(float vert, float3 P, float3 S, float3 eye, float t)
{
    float amountPick = ((waterContactRed & 1) != 0) ? 1000000000.0f : vert;
    float3 phPart = P;
    float3 dPart3 = S;
    float3 dPart2 = eye;
    float2 dPick3;
    float dPick2;
    float dPick;
    float dPart = wwContactDist(amountPick, phPart, dPart3, dPart2, dPick3, dPick2, dPick);
    float2 dir = dPick3;
    float foot = dPick2;
    float trust = dPick;
    float d = dPart;
    bool wwh0 = ((!waterContactOn) || (trust <= 0.0f));
    bool wwh1 = (d > 100000000.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return float4(0.0f, d, trust, foot);
    }
    float2 wvPart = P.xy;
    float4 wv = wwShoreWave(wvPart);
    // lane FIX1031B (LATE ITEM 8): a river's current breaking on the object (piers, boats, rocks) makes foam by itself; the
    // wind waves of a 40 m fetch give none (amount 0.001 at Cutler Bend). Amount ( v / 1.2 m/s )^2 (0.17 at 0.5 m/s, 0.43
    // at 0.79), a steady band 1 m wide (e-fold) on the object's sides, broken by the shared foam texture. Red
    // WW_CELL_WATER_CONTACT_RED=noflow (16).
    float flowD = 0.0f;
    bool cond2 = ((waterContactRed & 16) == 0) && waterFlowOn;
    bool cond;
    if (cond2)
    {
        float2 condPart = P.xy;
        cond = abs(wwWaterInfo(condPart).w - 1.0f) < 0.5f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float2 smPart = P.xy;
        // m/s (the bake's flow)
        float sm = length(wwWaterFlow(smPart));
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
        float2 fieldPart = P.xy;
        // a bank: the baked land lies about as far as the contact (within the bake's error); an object in open water
        // stands well inside the field's distance
        float2 field = wwShoreField(fieldPart);
        if (field.y > 0.5f)
        {
            cut = smoothstep(1.5f, 2.5f, max(field.x - waterShoreK0.x, 0.0f) / ((d + waterShoreK4.z) + 1.0f));
        }
    }
    float wavePick;
    if (wv.w < 0.5f)
    {
        wavePick = 0.0f;
    }
    else
    {
        float wavePart6 = d;
        float wavePart5 = hs;
        float wavePart4 = wv.y;
        float wavePart3 = t;
        float2 wavePart2 = P.xy - (dir * d);
        float wavePart = foot;
        wavePick = wwContactLaw(wavePart6, wavePart5, wavePart4, wavePart3, wavePart2, wavePart);
    }
    float wave = wavePick;
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
    float2 amountPick = P;
    float phPart = foot;
    float dPart3 = t;
    float3 det = wwFoamDetail(amountPick, phPart, dPart3);
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return ((a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1)) * waterContactK.w;
}
#endif
