// Shared HLSL for res/shaders/features/water/water_clear.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwClearDeepR, wwClearK, wwWaterClearBed

#if defined(WW_FN_wwWaterClearSetup) || defined(WW_FN_ALL)
void wwWaterClearSetup(float3 P, float3 V, float3 sun, float3 Ldir, float3 skyLo, float3 skyHi)
{
    float2 cPart = P.xy;
    float c = wwWaterInfo(cPart).w;
    int wwClearInPick;
    if (c < (-0.5f))
    {
        wwClearInPick = 2;
    }
    else
    {
        wwClearInPick = clamp(int(c + 0.5f), 0, 2);
    }
    wwClearIn.cls = wwClearInPick;
    wwClearIn.sunCos = clamp(Ldir.z, 0.0f, 1.0f);
    wwClearIn.sun = sun;
    wwClearIn.sky = (skyLo + skyHi) * 0.5f;
    wwClearIn.viewMu = max(abs(V.z), 0.001000000047497451305389404296875f);
}
#endif

#if defined(WW_FN_wwWaterClear) || defined(WW_FN_ALL)
float3 wwWaterClear(float3 c, float along, float vert)
{
    int cPart = wwClearIn.cls;
    float3 wwClearInPick;
    float3 kPart = wwClearK(cPart, wwClearInPick);
    float3 bb = wwClearInPick;
    float3 k = kPart;
    float s = wwClearIn.sunCos;
    // the sun's cosine under the surface
    float muS = sqrt(1.0f - ((1.0f - (s * s)) / 1.77688896656036376953125f));
    // through the surface (Fresnel ~2% / 6%)
    float3 Es = (wwClearIn.sun * s) * 0.980000019073486328125f;
    float3 Ed = wwClearIn.sky * 0.939999997615814208984375f;
    bool bed = (along < 100000000.0f) && (vert < 100000000.0f);
    float LPick;
    if (bed)
    {
        LPick = along * 0.01428749971091747283935546875f;
    }
    else
    {
        LPick = 1000000.0f;
    }
    float L = LPick;
    float zPick;
    if (bed)
    {
        zPick = vert * 0.01428749971091747283935546875f;
    }
    else
    {
        zPick = 1000000.0f;
    }
    float z = zPick;
    float muVPick;
    if (bed)
    {
        muVPick = clamp(vert / max(along, 0.001000000047497451305389404296875f), 0.001000000047497451305389404296875f, 1.0f);
    }
    else
    {
        muVPick = wwClearIn.viewMu;
    }
    float muV = muVPick;
    float3 Ks = k / muS.xxx;
    float3 Kd = k / 0.800000011920928955078125f.xxx;
    float3 inS = 0.0f.xxx;
    if (waterClearRed != 1)
    {
        float3 sigma = bb * 0.75f;
        float3 gs = k + (Ks * muV);
        float3 gd = k + (Kd * muV);
        inS = sigma * (((Es * (1.0f.xxx - exp((-gs) * L))) / gs) + ((Ed * (1.0f.xxx - exp((-gd) * L))) / gd));
    }
    if (!bed)
    {
        return inS;
    }
    float3 E = Es + Ed;
    float3 TdPick;
    if (waterClearRed == 2)
    {
        TdPick = 1.0f.xxx;
    }
    else
    {
        TdPick = ((Es * exp((-Ks) * z)) + (Ed * exp((-Kd) * z))) / max(E, 9.9999999747524270787835121154785e-07f.xxx);
    }
    float3 Td = TdPick;
    return ((c * Td) * exp((-k) * L)) + inS;
}
#endif
