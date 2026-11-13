// Shared HLSL for res/shaders/features/water/water_underlight.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_underwater_ms_ps.hlsl, cell_underwater_ps.hlsl

#if defined(WW_FN_wwUlInt) || defined(WW_FN_ALL)
float3 wwUlInt(float3 g, float L)
{
    float3 gl = g * L;
    float3 wwUlIntRet3 = (1.0f.xxx - exp(-gl)) / g;
    float3 wwUlIntRet2 = (1.0f.xxx - (gl * 0.5f)) * L;
    float3 wwUlIntPart = abs(gl);
    bool3 wwUlIntRet = bool3(wwUlIntPart.x < 9.9999997473787516355514526367188e-05f.xxx.x, wwUlIntPart.y < 9.9999997473787516355514526367188e-05f.xxx.y, wwUlIntPart.z < 9.9999997473787516355514526367188e-05f.xxx.z);
    return float3(wwUlIntRet.x ? wwUlIntRet2.x : wwUlIntRet3.x, wwUlIntRet.y ? wwUlIntRet2.y : wwUlIntRet3.y, wwUlIntRet.z ? wwUlIntRet2.z : wwUlIntRet3.z);
}
#endif

#if defined(WW_FN_wwUnderMeniscus) || defined(WW_FN_ALL)
float wwUnderMeniscus(float dz, float w)
{
    bool wwUlIntRet3 = uwLight == 0;
    bool wwUlIntRet2;
    if (!wwUlIntRet3)
    {
        wwUlIntRet2 = (uwLightRed & 2) != 0;
    }
    else
    {
        wwUlIntRet2 = wwUlIntRet3;
    }
    if (wwUlIntRet2)
    {
        return 0.0f;
    }
    return uwMenK.y * (1.0f - smoothstep(0.0f, max(w, 9.9999999747524270787835121154785e-07f), abs(dz)));
}
#endif

#if defined(WW_FN_wwUnderLight) || defined(WW_FN_ALL)
void wwUnderLight(float3 eye, float3 Pn, float3 Pw, bool farPlane, float H, out float3 add, out float3 mul)
{
    add = 0.0f.xxx;
    mul = 1.0f.xxx;
    if (Pn.z >= H)
    {
        return;
    }
    float3 d = Pw - eye;
    float L = length(d);
    float3 dir = d / max(L, 9.9999999747524270787835121154785e-07f).xxx;
    if (farPlane)
    {
        L = 1000000000.0f;
    }
    if (dir.z > 9.9999999747524270787835121154785e-07f)
    {
        L = min(L, (H - eye.z) / dir.z);
    }
    int wwUlIntRet3 = uwClass;
    float3 wwUlIntRet2;
    float3 wwUlIntPart = wwClearK(wwUlIntRet3, wwUlIntRet2);
    float3 bb = wwUlIntRet2;
    float3 k = wwUlIntPart;
    float s = clamp(uwSun.w, 0.0f, 1.0f);
    // the sun's cosine under the surface
    float muS = sqrt(1.0f - ((1.0f - (s * s)) / 1.77688896656036376953125f));
    // through the surface (Fresnel ~2% / 6%), as the clear colour
    float3 Es = (uwSun.xyz * s) * 0.980000019073486328125f;
    float3 Ed = uwSky * 0.939999997615814208984375f;
    float3 Ks = k / muS.xxx;
    float3 Kd = k / 0.800000011920928955078125f.xxx;
    float Lm = min(L, 10000000.0f) * 0.01428749971091747283935546875f;
    // the eye's depth (m)
    float zc = max(H - eye.z, 0.0f) * 0.01428749971091747283935546875f;
    // the seen point's depth (m)
    float zv = max(H - (eye.z + (dir.z * min(L, 10000000.0f))), 0.0f) * 0.01428749971091747283935546875f;
    float3 sigma = bb * 0.75f;
    float3 wwUlIntRet = k - (Ks * dir.z);
    float addPart3 = Lm;
    float3 addPart2 = k - (Kd * dir.z);
    float addPart = Lm;
    add = sigma * (((Es * exp((-Ks) * zc)) * wwUlInt(wwUlIntRet, addPart3)) + ((Ed * exp((-Kd) * zc)) * wwUlInt(addPart2, addPart)));
    // the scene's light on its way down (a small diffuse floor keeps lamp-lit points at night from going black)
    float3 Td = ((Es * exp((-Ks) * zv)) + ((Ed + 9.9999997473787516355514526367188e-05f.xxx) * exp((-Kd) * zv))) / ((Es + Ed) + 9.9999997473787516355514526367188e-05f.xxx);
    mul = Td * exp((-k) * Lm);
}
#endif
