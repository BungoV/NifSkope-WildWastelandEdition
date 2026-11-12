// Shared HLSL for res/shaders/ww_treewind.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_default_vs.hlsl, fo4_defaultfs_vs.hlsl

#if defined(WW_FN_wwTreeWindApply) || defined(WW_FN_ALL)
float3 wwTreeWindApply(float3 p, float3 nrm, float bitLen, float4x4 mv)
{
    float tmp1;
    if (bitLen > 1.5f)
    {
        tmp1 = clamp(bitLen - 2.0f, 0.0f, 1.0f);
    }
    else
    {
        tmp1 = 0.0f;
    }
    float a = tmp1;
    float3 n = nrm * rsqrt(max(dot(nrm, nrm), 1.0000000031710768509710513471353e-30f));
    float s = dot(n, wwTreeWind5.xyz);
    float base = (3.0f * s) + wwTreeWind3.w;
    float F = wwTreeWind4.z * 20.0f;
    float dist = length(mul(float4(wwTreeWind3.xyz, 1.0f), mv).xyz) / length(mv[0].xyz);
    float A_1 = (wwTreeWind4.y * clamp((4600.0f - dist) * 0.001000000047497451305389404296875f, 0.0f, 1.0f)) * 0.0350000001490116119384765625f;
    float halfR = (wwTreeWind2.y - wwTreeWind2.x) * 0.5f;
    float sp = wwTreeWind2.x + (halfR * sin((wwTreeWind.w * ((F * wwTreeWind2.z) * 5.0f)) + base));
    float3 off = (p + ((n * (sp * A_1)) * a)) - wwTreeWind3.xyz;
    float h = ((off.x + off.y) + off.z) / wwTreeWind4.x;
    float c = sin(((wwTreeWind.w * wwTreeWind2.z) * F) + ((h * (-1.5f)) + wwTreeWind3.w));
    float g = wwTreeWind2.x + (halfR * (((0.300000011920928955078125f * sin(3.1415927410125732421875f * c)) + sin(6.283185482025146484375f * c)) + 2.0f));
    float b = ((h * g) / 3.0f) * a;
    float3 q = off + float3(cos(wwTreeWind.x) * b, sin(wwTreeWind.x) * b, 0.0f);
    float ql = length(q);
    float3 tmp2;
    if (ql > 0.0f)
    {
        tmp2 = q * (length(off) / ql);
    }
    else
    {
        tmp2 = q;
    }
    return tmp2 + wwTreeWind3.xyz;
}
#endif
