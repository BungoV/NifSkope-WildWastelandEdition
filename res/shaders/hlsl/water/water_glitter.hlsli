// Shared HLSL for res/shaders/features/water/water_glitter.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWwGlitFacet = 1.4;
static const float kWwGlitSunR = 0.00466;
static const float kWwGlitLamMin = 4.0;
#endif

#if defined(WW_FN_wwGlitSunA2) || defined(WW_FN_ALL)
float wwGlitSunA2()
{
    return ((waterGlintRed & 16) != 0) ? 0.0f : 2.7144499199494021013379096984863e-06f;
}
#endif

#if defined(WW_FN_wwGlitHash) || defined(WW_FN_ALL)
uint4 wwGlitHash(inout uint4 v)
{
    v = (v * uint4(1664525u, 1664525u, 1664525u, 1664525u)) + uint4(1013904223u, 1013904223u, 1013904223u, 1013904223u);
    v.x += (v.y * v.w);
    v.y += (v.z * v.x);
    v.z += (v.x * v.y);
    v.w += (v.y * v.z);
    v ^= (v >> uint4(16u, 16u, 16u, 16u));
    v.x += (v.y * v.w);
    v.y += (v.z * v.x);
    v.z += (v.x * v.y);
    v.w += (v.y * v.z);
    return v;
}
#endif

#if defined(WW_FN_wwGlitCount) || defined(WW_FN_ALL)
float wwGlitCount(float lambda, float2 u)
{
    if (lambda >= 32.0f)
    {
        float z = sqrt((-2.0f) * log(max(u.x, 1.0000000116860974230803549289703e-07f))) * cos(6.283185482025146484375f * u.y);
        return max(1.0f + (z * rsqrt(lambda)), 0.0f);
    }
    float pk = exp(-lambda);
    float cdf = pk;
    float k = 0.0f;
    int i = 0;
    for (;;)
    {
        bool tmp1 = i < 80;
        bool tmp2;
        if (tmp1)
        {
            tmp2 = u.x > cdf;
        }
        else
        {
            tmp2 = tmp1;
        }
        if (tmp2)
        {
            k += 1.0f;
            pk *= (lambda / k);
            cdf += pk;
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
    return k / max(lambda, 9.9999999600419720025001879548654e-13f);
}
#endif

#if defined(WW_FN_wwGlitLevel) || defined(WW_FN_ALL)
float wwGlitLevel(float2 xy, float lev, float p, uint2 hb)
{
    float side = 1.39999997615814208984375f * exp2(lev);
    float2 c = floor(xy / side.xx);
    int2 ic = int2(c);
    uint4 tmp1 = uint4(uint2(ic), (uint(lev) * 40503u) ^ hb.x, hb.y);
    uint4 tmp2 = wwGlitHash(tmp1);
    uint4 h = tmp2;
    float2 u = float2(h.xy >> uint2(8u, 8u)) / 16777216.0f.xx;
    float n = exp2(2.0f * lev);
    float lam = n * p;
    if ((waterGlintRed & 128) == 0)
    {
        lam = max(lam, 4.0f);
    }
    float tmp3 = lam;
    float2 tmp4 = u;
    return wwGlitCount(tmp3, tmp4);
}
#endif

#if defined(WW_FN_wwWaterGlitter) || defined(WW_FN_ALL)
float wwWaterGlitter(inout float2 xy, float foot, float2 h, float a2, float cosH)
{
    if ((waterGlintRed & 8) != 0)
    {
        return 1.0f;
    }
    float t = 1.0f + (dot(h, h) / a2);
    float pdf = 1.0f / (((3.1415927410125732421875f * a2) * t) * t);
    float r = 0.0023300000466406345367431640625f;
    float cone = ((3.1415927410125732421875f * r) * r) / max((cosH * cosH) * cosH, 0.001000000047497451305389404296875f);
    float p = min(pdf * cone, 1.0f);
    uint2 hb = uint2(int2(floor(h / (2.0f * r).xx)));
    float lev = max(0.5f * log2(max(foot, 9.9999999747524270787835121154785e-07f) / 1.96000003814697265625f), 0.0f);
    if ((waterGlintRed & 32) != 0)
    {
        xy = floor(gl_FragCoord.xy) * 1.39999997615814208984375f;
        lev = 0.0f;
    }
    float l0 = floor(lev);
    float f = lev - l0;
    if ((waterGlintRed & 64) != 0)
    {
        f = 0.0f;
    }
    float2 tmp1 = xy;
    float tmp2 = l0;
    float tmp3 = p;
    uint2 tmp4 = hb;
    float g0 = wwGlitLevel(tmp1, tmp2, tmp3, tmp4);
    float tmp5;
    if (f > 0.0f)
    {
        float2 tmp6 = xy;
        float tmp7 = l0 + 1.0f;
        float tmp8 = p;
        uint2 tmp9 = hb;
        tmp5 = lerp(g0, wwGlitLevel(tmp6, tmp7, tmp8, tmp9), f);
    }
    else
    {
        tmp5 = g0;
    }
    return tmp5;
}
#endif

#if defined(WW_FN_wwWaterGlareGlit) || defined(WW_FN_ALL)
float wwWaterGlareGlit(float3 Ns, float3 V, float3 Ldir, float reflA2, float2 xy, float foot)
{
    float glA2 = reflA2 + wwGlitSunA2();
    float3 glH = normalize(Ldir - V);
    float3 tmp1 = Ns;
    float3 tmp2 = V;
    float3 tmp3 = Ldir;
    float3 tmp4 = 1.0f.xxx;
    float tmp5 = glA2;
    float tmp6 = waterGlintK.y;
    bool tmp7 = false;
    float g = wwWaterGlare(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7).x;
    float2 tmp8 = xy;
    float tmp9 = foot;
    float2 tmp10 = (glH.xy / max(glH.z, 0.001000000047497451305389404296875f).xx) - (Ns.xy / max(Ns.z, 0.001000000047497451305389404296875f).xx);
    float tmp11 = glA2;
    float tmp12 = dot(glH, Ns);
    float tmp13 = wwWaterGlitter(tmp8, tmp9, tmp10, tmp11, tmp12);
    float k = tmp13;
    bool shoulder = (waterGlintRed & 4) == 0;
    if ((waterGlintRed & 128) != 0)
    {
        g *= k;
        float tmp14;
        if (shoulder)
        {
            tmp14 = g / (1.0f + (g / waterGlintK.x));
        }
        else
        {
            tmp14 = g;
        }
        return tmp14;
    }
    if (shoulder)
    {
        g /= (1.0f + (g / waterGlintK.x));
    }
    return g * k;
}
#endif
