// Shared HLSL for res/shaders/features/water/water_ripple.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kRipDecay = 3.0;
static const float kRipGain = 0.7;
#endif

#if defined(WW_FN_wwContactRipple) || defined(WW_FN_ALL)
float3 wwContactRipple(float d, float2 dir, float foot, float s0)
{
    float3 o = 0.0f.xxx;
    for (int i = 0; i < 2; i++)
    {
        float k = waterOceanRip[i];
        float lam = 6.283185482025146484375f / k;
        float a = (0.699999988079071044921875f * s0) * exp((-d) / (3.0f * lam));
        float keep = 1.0f - smoothstep(0.25f, 0.5f, foot / lam);
        float3 tmp1 = o;
        float2 tmp2 = tmp1.xy + (dir * ((keep * a) * cos((k * d) - waterOceanRip[2 + i])));
        o.x = tmp2.x;
        o.y = tmp2.y;
        o.z += ((((1.0f - (keep * keep)) * 0.5f) * a) * a);
    }
    return o;
}
#endif

#if defined(WW_FN_wwContactDist) || defined(WW_FN_ALL)
float wwContactDist(float vert, float3 P, float3 S, float3 eye, out float2 dir, out float foot, inout float trust)
{
    float3 sx = ddx(S);
    float3 sy = ddy(S);
    float3 Vs = normalize(S - eye);
    float ang = max(length(ddx(Vs)), length(ddy(Vs)));
    foot = max(length(ddx(P.xy)), length(ddy(P.xy)));
    trust = 1.0f - smoothstep(4.0f, 8.0f, max(length(sx), length(sy)) / max(length(S - eye) * ang, 9.9999999747524270787835121154785e-07f));
    float3 n = cross(sx, sy);
    float nl = length(n);
    n /= max(nl, 9.9999999600419720025001879548654e-13f).xxx;
    float e = dot(P - S, n);
    float h = length(n.xy);
    trust *= smoothstep(0.1500000059604644775390625f, 0.300000011920928955078125f, abs(dot(n, Vs)));
    dir = (n.xy * sign(e)) / max(h, 9.9999999747524270787835121154785e-07f).xx;
    float tmp1;
    if (((vert > 100000000.0f) || (nl < 9.9999999600419720025001879548654e-13f)) || (h < 0.001000000047497451305389404296875f))
    {
        tmp1 = 1000000000.0f;
    }
    else
    {
        tmp1 = abs(e) / h;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_wwContactRippleAt) || defined(WW_FN_ALL)
float3 wwContactRippleAt(float vert, float3 P, float3 S, float3 eye, float s0)
{
    float tmp1 = vert;
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
    float lamMax = 6.283185482025146484375f / min(waterOceanRip.x, waterOceanRip.y);
    bool tmp9 = (s0 <= 0.0f) || (trust <= 0.0f);
    bool tmp10;
    if (!tmp9)
    {
        tmp10 = d > (12.0f * lamMax);
    }
    else
    {
        tmp10 = tmp9;
    }
    if (tmp10)
    {
        return 0.0f.xxx;
    }
    float tmp11 = d;
    float2 tmp12 = dir;
    float tmp13 = foot;
    float tmp14 = s0;
    float3 o = wwContactRipple(tmp11, tmp12, tmp13, tmp14);
    return float3(o.xy * trust, o.z);
}
#endif
