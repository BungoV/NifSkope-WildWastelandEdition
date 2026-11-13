// Shared HLSL for res/shaders/features/water/water_sss.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kGlowGain = 2.5;
#endif

#if defined(WW_FN_wwPosMean) || defined(WW_FN_ALL)
float wwPosMean(float a, float sd)
{
    if (sd < 9.9999999747524270787835121154785e-07f)
    {
        return max(a, 0.0f);
    }
    // Phi( -|x| ) = 0.5 erfc( |x| / sqrt 2 ) (Abramowitz and Stegun 7.1.26), kept with phi's own exp so the tail stays
    // relative: sd ( phi( x ) + x Phi( x ) )
    float x = a / sd;
    float z = abs(x) * 0.7071068286895751953125f;
    float t = 1.0f / (1.0f + (0.3275910913944244384765625f * z));
    float e = exp(((-0.5f) * x) * x);
    float tail = (0.5f * t) * (0.254829585552215576171875f + (t * ((-0.284496724605560302734375f) + (t * (1.42141377925872802734375f + (t * ((-1.45315206050872802734375f) + (t * 1.0614054203033447265625f))))))));
    float PhiPick;
    if (x < 0.0f)
    {
        PhiPick = tail * e;
    }
    else
    {
        PhiPick = 1.0f - (tail * e);
    }
    float Phi = PhiPick;
    return max(sd * ((0.398942291736602783203125f * e) + (x * Phi)), 0.0f);
}
#endif

#if defined(WW_FN_wwWaterCrestGlow) || defined(WW_FN_ALL)
float3 wwWaterCrestGlow(float2 s, float var, float3 V, float3 L, float3 sun, float3 body)
{
    float h = length(V.xy);
    if (h < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xxx;
    }
    // > 0: the face tilts towards the eye
    float a = dot(s, (-V.xy) / h.xx);
    float PhiPick = a;
    float cPart = sqrt(0.5f * max(var, 0.0f));
    float c = wwPosMean(PhiPick, cPart);
    // / isotropic
    float hg = 0.75f / pow(1.25f - (1.0f * clamp(dot(V, L), -1.0f, 1.0f)), 1.5f);
    // FIX1031B: red dimglow, the gain before
    float gain = ((waterOceanRed & 512) != 0) ? 0.25f : 2.5f;
    return (sun * (((gain * c) * hg) * smoothstep(0.0f, 0.0500000007450580596923828125f, L.z))) * body;
}
#endif
