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
    float3 tmp1 = (1.0f.xxx - exp(-gl)) / g;
    float3 tmp2 = (1.0f.xxx - (gl * 0.5f)) * L;
    float3 tmp3 = abs(gl);
    bool3 tmp4 = bool3(tmp3.x < 9.9999997473787516355514526367188e-05f.xxx.x, tmp3.y < 9.9999997473787516355514526367188e-05f.xxx.y, tmp3.z < 9.9999997473787516355514526367188e-05f.xxx.z);
    return float3(tmp4.x ? tmp2.x : tmp1.x, tmp4.y ? tmp2.y : tmp1.y, tmp4.z ? tmp2.z : tmp1.z);
}
#endif

#if defined(WW_FN_wwUnderMeniscus) || defined(WW_FN_ALL)
float wwUnderMeniscus(float dz, float w)
{
    bool tmp1 = uwLight == 0;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = (uwLightRed & 2) != 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
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
    int tmp1 = uwClass;
    float3 tmp2;
    float3 tmp3 = wwClearK(tmp1, tmp2);
    float3 bb = tmp2;
    float3 k = tmp3;
    float s = clamp(uwSun.w, 0.0f, 1.0f);
    float muS = sqrt(1.0f - ((1.0f - (s * s)) / 1.77688896656036376953125f));
    float3 Es = (uwSun.xyz * s) * 0.980000019073486328125f;
    float3 Ed = uwSky * 0.939999997615814208984375f;
    float3 Ks = k / muS.xxx;
    float3 Kd = k / 0.800000011920928955078125f.xxx;
    float Lm = min(L, 10000000.0f) * 0.01428749971091747283935546875f;
    float zc = max(H - eye.z, 0.0f) * 0.01428749971091747283935546875f;
    float zv = max(H - (eye.z + (dir.z * min(L, 10000000.0f))), 0.0f) * 0.01428749971091747283935546875f;
    float3 sigma = bb * 0.75f;
    float3 tmp4 = k - (Ks * dir.z);
    float tmp5 = Lm;
    float3 tmp6 = k - (Kd * dir.z);
    float tmp7 = Lm;
    add = sigma * (((Es * exp((-Ks) * zc)) * wwUlInt(tmp4, tmp5)) + ((Ed * exp((-Kd) * zc)) * wwUlInt(tmp6, tmp7)));
    float3 Td = ((Es * exp((-Ks) * zv)) + ((Ed + 9.9999997473787516355514526367188e-05f.xxx) * exp((-Kd) * zv))) / ((Es + Ed) + 9.9999997473787516355514526367188e-05f.xxx);
    mul = Td * exp((-k) * Lm);
}
#endif
