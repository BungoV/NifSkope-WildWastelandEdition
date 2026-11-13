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
        bool cond2 = i < 80;
        bool cond;
        if (cond2)
        {
            cond = u.x > cdf;
        }
        else
        {
            cond = cond2;
        }
        if (cond)
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
    // u, a cell's side
    float side = 1.39999997615814208984375f * exp2(lev);
    float2 c = floor(xy / side.xx);
    int2 ic = int2(c);
    uint4 cond2 = uint4(uint2(ic), (uint(lev) * 40503u) ^ hb.x, hb.y);
    uint4 cond = wwGlitHash(cond2);
    uint4 h = cond;
    float2 u = float2(h.xy >> uint2(8u, 8u)) / 16777216.0f.xx;
    // facets in the cell
    float n = exp2(2.0f * lev);
    float lam = n * p;
    // FIX1031B: no count the display cannot show (red sparse: the bare count)
    if ((waterGlintRed & 128) == 0)
    {
        lam = max(lam, 4.0f);
    }
    float wwGlitLevelRet2 = lam;
    float2 wwGlitLevelRet = u;
    return wwGlitCount(wwGlitLevelRet2, wwGlitLevelRet);
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
    // GGX slope density
    float pdf = 1.0f / (((3.1415927410125732421875f * a2) * t) * t);
    // the half-vector's cone
    float r = 0.0023300000466406345367431640625f;
    float cone = ((3.1415927410125732421875f * r) * r) / max((cosH * cosH) * cosH, 0.001000000047497451305389404296875f);
    float p = min(pdf * cone, 1.0f);
    uint2 hb = uint2(int2(floor(h / (2.0f * r).xx)));
    float lev = max(0.5f * log2(max(foot, 9.9999999747524270787835121154785e-07f) / 1.96000003814697265625f), 0.0f);
    // red pixel: the screen's pixels are the cells
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
    float2 cond2 = xy;
    float cond = l0;
    float wwGlitLevelRet2 = p;
    uint2 wwGlitLevelRet = hb;
    float g0 = wwGlitLevel(cond2, cond, wwGlitLevelRet2, wwGlitLevelRet);
    float wwWaterGlitterRet;
    if (f > 0.0f)
    {
        float2 wwWaterGlitterPart4 = xy;
        float wwWaterGlitterPart3 = l0 + 1.0f;
        float wwWaterGlitterPart2 = p;
        uint2 wwWaterGlitterPart = hb;
        wwWaterGlitterRet = lerp(g0, wwGlitLevel(wwWaterGlitterPart4, wwWaterGlitterPart3, wwWaterGlitterPart2, wwWaterGlitterPart), f);
    }
    else
    {
        wwWaterGlitterRet = g0;
    }
    return wwWaterGlitterRet;
}
#endif

#if defined(WW_FN_wwWaterGlareGlit) || defined(WW_FN_ALL)
float wwWaterGlareGlit(float3 Ns, float3 V, float3 Ldir, float reflA2, float2 xy, float foot)
{
    float glA2 = reflA2 + wwGlitSunA2();
    float3 glH = normalize(Ldir - V);
    float3 cond2 = Ns;
    float3 cond = V;
    float3 wwGlitLevelRet2 = Ldir;
    float3 wwGlitLevelRet = 1.0f.xxx;
    float wwWaterGlitterRet = glA2;
    float wwWaterGlitterPart4 = waterGlintK.y;
    bool wwWaterGlitterPart3 = false;
    float g = wwWaterGlare(cond2, cond, wwGlitLevelRet2, wwGlitLevelRet, wwWaterGlitterRet, wwWaterGlitterPart4, wwWaterGlitterPart3).x;
    float2 wwWaterGlitterPart2 = xy;
    float wwWaterGlitterPart = foot;
    float2 wwWaterGlitterPart7 = (glH.xy / max(glH.z, 0.001000000047497451305389404296875f).xx) - (Ns.xy / max(Ns.z, 0.001000000047497451305389404296875f).xx);
    float wwWaterGlitterPart6 = glA2;
    float wwWaterGlitterPart5 = dot(glH, Ns);
    float wwWaterGlitterRet2 = wwWaterGlitter(wwWaterGlitterPart2, wwWaterGlitterPart, wwWaterGlitterPart7, wwWaterGlitterPart6, wwWaterGlitterPart5);
    float k = wwWaterGlitterRet2;
    bool shoulder = (waterGlintRed & 4) == 0;
    if ((waterGlintRed & 128) != 0)
    {
        g *= k;
        float wwWaterGlareGlitRet;
        if (shoulder)
        {
            wwWaterGlareGlitRet = g / (1.0f + (g / waterGlintK.x));
        }
        else
        {
            wwWaterGlareGlitRet = g;
        }
        return wwWaterGlareGlitRet;
    }
    if (shoulder)
    {
        g /= (1.0f + (g / waterGlintK.x));
    }
    return g * k;
}
#endif
