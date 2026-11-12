// Shared HLSL for res/shaders/features/water/water_ssrbend.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSsrBend) || defined(WW_FN_ALL)
float2 wwWaterSsrBend(float3 Rw, float3 Rflat)
{
    float3 tmp1 = float3(1.0f, 0.0f, 0.0f);
    float3 ax = cellWorldDir(tmp1);
    float3 tmp2 = float3(0.0f, 1.0f, 0.0f);
    float3 ay = cellWorldDir(tmp2);
    float3 tmp3 = float3(0.0f, 0.0f, 1.0f);
    float3 az = cellWorldDir(tmp3);
    float3 a = float3(dot(Rw, ax), dot(Rw, ay), dot(Rw, az));
    float3 b = float3(dot(Rflat, ax), dot(Rflat, ay), dot(Rflat, az));
    float za = max(-a.z, 0.0500000007450580596923828125f);
    float zb = max(-b.z, 0.0500000007450580596923828125f);
    float2 d = float2((0.5f * projectionMatrix[0].x) * ((a.x / za) - (b.x / zb)), (0.5f * projectionMatrix[1].y) * ((a.y / za) - (b.y / zb))) * 0.800000011920928955078125f;
    float l = length(d);
    float2 tmp4;
    if (l > 0.07999999821186065673828125f)
    {
        tmp4 = d * (0.07999999821186065673828125f / l);
    }
    else
    {
        tmp4 = d;
    }
    return tmp4;
}
#endif
