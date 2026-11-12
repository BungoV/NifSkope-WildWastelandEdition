// Shared HLSL for res/shaders/features/water/water_under.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_underwater_ms_ps.hlsl, cell_underwater_ps.hlsl

#if defined(WW_FN_wwUnderwater) || defined(WW_FN_ALL)
void wwUnderwater(float3 eye, float3 Pn, float3 Pw, bool farPlane, float4 uwFog_1, float3 fogCol, float3 tint, int red, float3 fwd, out float3 add, out float3 mul)
{
    add = 0.0f.xxx;
    mul = 1.0f.xxx;
    float H = uwFog_1.w;
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
    float far = max(uwFog_1.z, 0.001000000047497451305389404296875f);
    float span = max(uwFog_1.z - uwFog_1.y, 0.001000000047497451305389404296875f);
    float tmp1;
    if (dot(fwd, fwd) > 0.0f)
    {
        tmp1 = L * max(dot(dir, fwd), 0.0f);
    }
    else
    {
        tmp1 = L;
    }
    float Lf = tmp1;
    float a = clamp(uwFog_1.x * clamp((Lf - uwFog_1.y) / span, 0.0f, 1.0f), 0.0f, 1.0f);
    float dv = clamp(H - (eye.z + (dir.z * L)), 0.0f, far);
    float dc = clamp(H - eye.z, 0.0f, far);
    float3 Tv = 1.0f.xxx - ((1.0f.xxx - tint) * (clamp(uwFog_1.x, 0.0f, 1.0f) * (dv / far)));
    float3 Tc = 1.0f.xxx - ((1.0f.xxx - tint) * (clamp(uwFog_1.x, 0.0f, 1.0f) * (dc / far)));
    if ((red & 1) != 0)
    {
        Tv = 1.0f.xxx;
        Tc = 1.0f.xxx;
    }
    if ((red & 2) != 0)
    {
        a = 0.0f;
    }
    add = (fogCol * Tc) * a;
    mul = Tv * (1.0f - a);
}
#endif
