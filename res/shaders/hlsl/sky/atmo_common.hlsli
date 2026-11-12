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
    bool tmp1 = mu < 0.0f;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = (((r * r) * ((mu * mu) - 1.0f)) + 40449600.0f) >= 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    return tmp2;
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
    float tmp1 = r;
    float tmp2 = mu;
    float d = atDistTop(tmp1, tmp2);
    float dmin = 6460.0f - r;
    float dmax = rho + H;
    float tmp3 = (d - dmin) / max(dmax - dmin, 9.9999999747524270787835121154785e-07f);
    float tmp4 = 256.0f;
    float tmp5 = rho / H;
    float tmp6 = 64.0f;
    return float2(atUnitToSub(tmp3, tmp4), atUnitToSub(tmp5, tmp6));
}
#endif

#if defined(WW_FN_atTrans) || defined(WW_FN_ALL)
float3 atTrans(Texture2D<float4> T, SamplerState _T_sampler, float r, float mu)
{
    float tmp1 = r;
    float tmp2 = mu;
    if (atHitsGround(tmp1, tmp2))
    {
        return 0.0f.xxx;
    }
    float tmp3 = r;
    float tmp4 = mu;
    return T.Sample(_T_sampler, atTransUv(tmp3, tmp4)).xyz;
}
#endif

#if defined(WW_FN_atMsUv) || defined(WW_FN_ALL)
float2 atMsUv(float r, float mus, float2 n)
{
    float hu = clamp((r - 6360.0f) / 100.0f, 0.0f, 1.0f);
    float tmp1 = clamp((mus * 0.5f) + 0.5f, 0.0f, 1.0f);
    float tmp2 = n.x;
    float tmp3;
    if (atMsOld > 0.5f)
    {
        tmp3 = hu;
    }
    else
    {
        tmp3 = sqrt(hu);
    }
    float tmp4 = tmp3;
    float tmp5 = n.y;
    return float2(atUnitToSub(tmp1, tmp2), atUnitToSub(tmp4, tmp5));
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
    float tmp1 = uv.x;
    float tmp2 = n.x;
    float tmp3 = uv.y;
    float tmp4 = n.y;
    return float2(atUnitToSub(tmp1, tmp2), atUnitToSub(tmp3, tmp4));
}
#endif

#if defined(WW_FN_atSkyMarch) || defined(WW_FN_ALL)
float3 atSkyMarch(Texture2D<float4> T, SamplerState _T_sampler, Texture2D<float4> M, SamplerState _M_sampler, float2 msSize, float msOn, float ground, float r, float3 v, float3 s)
{
    float tmp1 = r;
    float tmp2 = v.z;
    bool gnd = atHitsGround(tmp1, tmp2);
    float tmp3;
    if (gnd)
    {
        float tmp4 = r;
        float tmp5 = v.z;
        tmp3 = atDistGround(tmp4, tmp5);
    }
    else
    {
        float tmp6 = r;
        float tmp7 = v.z;
        tmp3 = atDistTop(tmp6, tmp7);
    }
    float d = tmp3;
    float cvs = dot(v, s);
    float tmp8 = cvs;
    float pr = atPhaseR(tmp8);
    float tmp9 = atG;
    float tmp10 = cvs;
    float pm = atPhaseHg(tmp9, tmp10);
    float3 thr = 1.0f.xxx;
    float3 L = 0.0f.xxx;
    float t0 = 0.0f;
    float3 tmp11;
    float3 tmp12;
    float3 tmp13;
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
        float tmp14 = rp - 6360.0f;
        atMedium(tmp14, tmp11, tmp12, tmp13);
        float3 rs = tmp11;
        float3 ms = tmp12;
        float3 e = tmp13;
        float tmp15 = rp;
        float tmp16 = mus;
        float3 Ts = atTrans(T, _T_sampler, tmp15, tmp16);
        float tmp17 = rp;
        float tmp18 = mus;
        float2 tmp19 = msSize;
        float3 S = (Ts * ((rs * pr) + (ms * pm))) + ((M.Sample(_M_sampler, atMsUv(tmp17, tmp18, tmp19)).xyz * msOn) * (rs + ms));
        float3 Tst = exp((-e) * dt);
        L += (((thr * S) * (1.0f.xxx - Tst)) / max(e, 9.999999717180685365747194737196e-10f.xxx));
        thr *= Tst;
    }
    if (gnd && (ground > 0.5f))
    {
        float c = dot(normalize(float3(v.xy * d, r + (v.z * d))), s);
        float tmp20 = 6360.0f;
        float tmp21 = c;
        L += ((((thr * atTrans(T, _T_sampler, tmp20, tmp21)) * max(c, 0.0f)) * 0.300000011920928955078125f.xxx) / 3.1415927410125732421875f.xxx);
    }
    return L;
}
#endif
