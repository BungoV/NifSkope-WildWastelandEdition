// Shared HLSL for res/shaders/features/sky/atmo_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: atmo_ap_ps.hlsl, atmo_apl_ps.hlsl, atmo_ms_ps.hlsl, atmo_sky_ps.hlsl, atmo_skyview_ps.hlsl, atmo_transmittance_ps.hlsl, cell_volsky_ps.hlsl, cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl, lookdev_clouds_ps.hlsl, sky_moon_ps.hlsl
// Not here (declarations, or no converted entry carried them): atTransRmu, atMsRowToH, atSkyUvToDir
#ifdef WW_FN_ALL
static const float kPi = 3.14159265;
static const float kRb = 6360.0;
static const float kRt = 6460.0;
static const float kRayH = 8.0;
static const float kMieH = 1.2;
#endif

#if defined(WW_FN_atMedium) || defined(WW_FN_ALL)
void atMedium(inout float h, inout float3 rayS, out float3 mieS, out float3 ext)
{
    h = max(h, 0.0f);
    float dr = exp((-h) / 8.0f);
    float dm = exp((-h) / 1.2000000476837158203125f);
    float dz = max(0.0f, 1.0f - (abs(h - 25.0f) / 15.0f));
    float dh = atHaze.x * exp((-max(h - atHaze.z, 0.0f)) / max(atHaze.y, 0.0199999995529651641845703125f));
    rayS = atRay * dr;
    mieS = (atMieS * dm).xxx + ((atHazeSpec * dh) * atHazeAlbedo);
    ext = ((rayS + (atMieE * dm).xxx) + (atHazeSpec * dh)) + (atOzone * dz);
}
#endif

#if defined(WW_FN_atDistTop) || defined(WW_FN_ALL)
float atDistTop(float r, float mu)
{
    return max(0.0f, ((-r) * mu) + sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f)));
}
#endif

#if defined(WW_FN_atDistGround) || defined(WW_FN_ALL)
float atDistGround(float r, float mu)
{
    return max(0.0f, ((-r) * mu) - sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 40449600.0f)));
}
#endif

#if defined(WW_FN_atHitsGround) || defined(WW_FN_ALL)
bool atHitsGround(float r, float mu)
{
    bool cond = mu < 0.0f;
    bool atHitsGroundRet;
    if (cond)
    {
        atHitsGroundRet = (((r * r) * ((mu * mu) - 1.0f)) + 40449600.0f) >= 0.0f;
    }
    else
    {
        atHitsGroundRet = cond;
    }
    return atHitsGroundRet;
}
#endif

#if defined(WW_FN_atUnitToSub) || defined(WW_FN_ALL)
float atUnitToSub(float u, float n)
{
    return (0.5f / n) + (u * (1.0f - (1.0f / n)));
}
#endif

#if defined(WW_FN_atSubToUnit) || defined(WW_FN_ALL)
float atSubToUnit(float u, float n)
{
    return (u - (0.5f / n)) / (1.0f - (1.0f / n));
}
#endif

#if defined(WW_FN_atTransUv) || defined(WW_FN_ALL)
float2 atTransUv(float r, float mu)
{
    float H = 1132.25439453125f;
    float rho = sqrt(max(0.0f, (r * r) - 40449600.0f));
    float cond = r;
    float atHitsGroundRet = mu;
    float d = atDistTop(cond, atHitsGroundRet);
    float dmin = 6460.0f - r;
    float dmax = rho + H;
    float atTransUvRet4 = (d - dmin) / max(dmax - dmin, 9.9999999747524270787835121154785e-07f);
    float atTransUvRet3 = 256.0f;
    float atTransUvRet2 = rho / H;
    float atTransUvRet = 64.0f;
    return float2(atUnitToSub(atTransUvRet4, atTransUvRet3), atUnitToSub(atTransUvRet2, atTransUvRet));
}
#endif

#if defined(WW_FN_atTrans) || defined(WW_FN_ALL)
float3 atTrans(Texture2D<float4> T, SamplerState _T_sampler, float r, float mu)
{
    float cond = r;
    float atHitsGroundRet = mu;
    if (atHitsGround(cond, atHitsGroundRet))
    {
        return 0.0f.xxx;
    }
    float atTransUvRet4 = r;
    float atTransUvRet3 = mu;
    return T.Sample(_T_sampler, atTransUv(atTransUvRet4, atTransUvRet3)).xyz;
}
#endif

#if defined(WW_FN_atMsUv) || defined(WW_FN_ALL)
float2 atMsUv(float r, float mus, float2 n)
{
    float hu = clamp((r - 6360.0f) / 100.0f, 0.0f, 1.0f);
    float cond = clamp((mus * 0.5f) + 0.5f, 0.0f, 1.0f);
    float atHitsGroundRet = n.x;
    float atTransUvRet4;
    if (atMsOld > 0.5f)
    {
        atTransUvRet4 = hu;
    }
    else
    {
        atTransUvRet4 = sqrt(hu);
    }
    float atTransUvRet3 = atTransUvRet4;
    float atTransUvRet2 = n.y;
    return float2(atUnitToSub(cond, atHitsGroundRet), atUnitToSub(atTransUvRet3, atTransUvRet2));
}
#endif

#if defined(WW_FN_atPhaseR) || defined(WW_FN_ALL)
float atPhaseR(float c)
{
    return 0.0596831031143665313720703125f * (1.0f + (c * c));
}
#endif

#if defined(WW_FN_atPhaseHg) || defined(WW_FN_ALL)
float atPhaseHg(float g, float c)
{
    return (1.0f - (g * g)) / (12.56637096405029296875f * pow(max(9.9999997473787516355514526367188e-05f, (1.0f + (g * g)) - ((2.0f * g) * c)), 1.5f));
}
#endif

#if defined(WW_FN_atSkyDirToUv) || defined(WW_FN_ALL)
float2 atSkyDirToUv(float r, float cosZ, float cosAz, float2 n)
{
    float vh = sqrt(max(0.0f, (r * r) - 40449600.0f));
    float beta = acos(clamp(vh / r, -1.0f, 1.0f));
    float zh = 3.1415927410125732421875f - beta;
    float za = acos(clamp(cosZ, -1.0f, 1.0f));
    float2 uv;
    if (za < zh)
    {
        float c = 1.0f - (za / zh);
        uv.y = (1.0f - sqrt(max(0.0f, c))) * 0.5f;
    }
    else
    {
        float c_1 = (za - zh) / max(beta, 9.9999997473787516355514526367188e-06f);
        uv.y = (sqrt(clamp(c_1, 0.0f, 1.0f)) * 0.5f) + 0.5f;
    }
    uv.x = sqrt(clamp(((-cosAz) * 0.5f) + 0.5f, 0.0f, 1.0f));
    float cond = uv.x;
    float atHitsGroundRet = n.x;
    float atTransUvRet4 = uv.y;
    float atTransUvRet3 = n.y;
    return float2(atUnitToSub(cond, atHitsGroundRet), atUnitToSub(atTransUvRet4, atTransUvRet3));
}
#endif

#if defined(WW_FN_atSkyMarch) || defined(WW_FN_ALL)
float3 atSkyMarch(Texture2D<float4> T, SamplerState _T_sampler, Texture2D<float4> M, SamplerState _M_sampler, float2 msSize, float msOn, float ground, float r, float3 v, float3 s)
{
    float cond = r;
    float atHitsGroundRet = v.z;
    bool gnd = atHitsGround(cond, atHitsGroundRet);
    float atTransUvRet4;
    if (gnd)
    {
        float atTransUvRet3 = r;
        float atTransUvRet2 = v.z;
        atTransUvRet4 = atDistGround(atTransUvRet3, atTransUvRet2);
    }
    else
    {
        float atTransUvRet = r;
        float atTransUvPart = v.z;
        atTransUvRet4 = atDistTop(atTransUvRet, atTransUvPart);
    }
    float d = atTransUvRet4;
    float cvs = dot(v, s);
    float prPart = cvs;
    float pr = atPhaseR(prPart);
    float pmPart2 = atG;
    float pmPart = cvs;
    float pm = atPhaseHg(pmPart2, pmPart);
    float3 thr = 1.0f.xxx;
    float3 L = 0.0f.xxx;
    float t0 = 0.0f;
    float3 part4;
    float3 part3;
    float3 part2;
    for (int i = 0; i < 30; i++)
    {
        float a = float(i + 1) / 30.0f;
        float t1 = (d * a) * a;
        float dt = t1 - t0;
        float t = t0 + (dt * 0.5f);
        t0 = t1;
        float3 p = float3(v.xy * t, r + (v.z * t));
        float rp = length(p);
        float3 up = p / rp.xxx;
        float mus = dot(up, s);
        float part = rp - 6360.0f;
        atMedium(part, part4, part3, part2);
        float3 rs = part4;
        float3 ms = part3;
        float3 e = part2;
        float TsPart2 = rp;
        float TsPart = mus;
        float3 Ts = atTrans(T, _T_sampler, TsPart2, TsPart);
        float SPart3 = rp;
        float SPart2 = mus;
        float2 SPart = msSize;
        float3 S = (Ts * ((rs * pr) + (ms * pm))) + ((M.Sample(_M_sampler, atMsUv(SPart3, SPart2, SPart)).xyz * msOn) * (rs + ms));
        float3 Tst = exp((-e) * dt);
        L += (((thr * S) * (1.0f.xxx - Tst)) / max(e, 9.999999717180685365747194737196e-10f.xxx));
        thr *= Tst;
    }
    if (gnd && (ground > 0.5f))
    {
        float c = dot(normalize(float3(v.xy * d, r + (v.z * d))), s);
        float LPart2 = 6360.0f;
        float LPart = c;
        L += ((((thr * atTrans(T, _T_sampler, LPart2, LPart)) * max(c, 0.0f)) * 0.300000011920928955078125f.xxx) / 3.1415927410125732421875f.xxx);
    }
    return L;
}
#endif
