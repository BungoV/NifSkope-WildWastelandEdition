// Shared HLSL for res/shaders/features/clouds/cloud_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_flybake_ps.hlsl, cloud_flybake_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl, cloud_shadowmap_ps.hlsl, cloud_shadowmap_v_ps.hlsl
// Not here (declarations, or no converted entry carried them): cvTypeProfile
#ifdef WW_FN_ALL
static const float kCvShapeKm = 6.0;
static const float kCvDetailKm = 0.6;
static const float kCvSigma = 40.0;
static const float kCvVarWrapKm = 384.0;
static const float kCvCurlPeriodKm = 24.0;
#endif

#if defined(WW_FN_cvCalAt) || defined(WW_FN_ALL)
float cvCalAt(int b, float c)
{
    float t = clamp(c, 0.0f, 1.0f) * 16.0f;
    int i = min(int(t), 15);
    return lerp(cvCal[(b * 17) + i], cvCal[((b * 17) + i) + 1], t - float(i));
}
#endif

#if defined(WW_FN_cvMapAt) || defined(WW_FN_ALL)
float4 cvMapAt(float2 xy)
{
    float2 uv = (xy - cvBands.mapRect.xy) * cvBands.mapRect.z;
    bool tmp1 = cvBands.mapRect.w < 0.5f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return float4(cvBands.coverage.x, cvBands.coverage.y, cvBands.coverage.x * cvBands.sheet, cvBands.coverage.z);
    }
    return cvMap.SampleLevel(_cvMap_sampler, uv, 0.0f);
}
#endif

#if defined(WW_FN_cvSmooth) || defined(WW_FN_ALL)
float cvSmooth(float a, float b, float x)
{
    float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
    return (t * t) * (3.0f - (2.0f * t));
}
#endif

#if defined(WW_FN_cvGradient) || defined(WW_FN_ALL)
float cvGradient(int b, float hf, float sheet)
{
    if (b == 0)
    {
        float tmp1 = 0.0f;
        float tmp2 = 0.070000000298023223876953125f;
        float tmp3 = hf;
        float tmp4 = 0.550000011920928955078125f;
        float tmp5 = 1.0f;
        float tmp6 = hf;
        float cu = cvSmooth(tmp1, tmp2, tmp3) * (1.0f - cvSmooth(tmp4, tmp5, tmp6));
        float tmp7 = 0.0f;
        float tmp8 = 0.039999999105930328369140625f;
        float tmp9 = hf;
        float tmp10 = 0.119999997317790985107421875f;
        float tmp11 = 0.300000011920928955078125f;
        float tmp12 = hf;
        float st = cvSmooth(tmp7, tmp8, tmp9) * (1.0f - cvSmooth(tmp10, tmp11, tmp12));
        return lerp(cu, st, clamp(sheet, 0.0f, 1.0f));
    }
    float tmp13 = 0.0f;
    float tmp14 = 0.1500000059604644775390625f;
    float tmp15 = hf;
    float tmp16 = 0.3499999940395355224609375f;
    float tmp17 = 0.89999997615814208984375f;
    float tmp18 = hf;
    return cvSmooth(tmp13, tmp14, tmp15) * (1.0f - cvSmooth(tmp16, tmp17, tmp18));
}
#endif

#if defined(WW_FN_cloudDensity) || defined(WW_FN_ALL)
float cloudDensity(float3 p)
{
    float2 tmp1 = p.xy;
    float4 m = cvMapAt(tmp1);
    float d = 0.0f;
    float tmp2;
    float tmp3;
    for (int b = 0; b < 2; b++)
    {
        float base = cvBands.baseKm[b];
        float top = cvBands.topKm[b];
        float hf = (p.z - base) / max(top - base, 0.001000000047497451305389404296875f);
        bool tmp4 = (hf <= 0.0f) || (hf >= 1.0f);
        bool tmp5;
        if (!tmp4)
        {
            tmp5 = cvBands.density[b] <= 0.0f;
        }
        else
        {
            tmp5 = tmp4;
        }
        if (tmp5)
        {
            continue;
        }
        if (b == 0)
        {
            tmp2 = m.x;
        }
        else
        {
            tmp2 = m.y;
        }
        float cov = tmp2;
        if (b == 0)
        {
            tmp3 = clamp(m.z / max(m.x, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
        }
        else
        {
            tmp3 = 0.0f;
        }
        float sheet = tmp3;
        int tmp6 = b;
        float tmp7 = cov;
        float c = cvCalAt(tmp6, tmp7);
        if (c <= 0.0f)
        {
            continue;
        }
        float3 q = p + cvBands.windKm;
        float s = cvShape.SampleLevel(_cvShape_sampler, q / 6.0f.xxx, log2(max((cvFootKm * 128.0f) / 6.0f, 1.0f))).x;
        float t = 1.0f - c;
        int tmp8 = b;
        float tmp9 = hf;
        float tmp10 = sheet;
        float base_ = clamp(((s * cvGradient(tmp8, tmp9, tmp10)) - t) / max(1.0f - t, 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f) * c;
        if (base_ <= 0.0f)
        {
            continue;
        }
        float det = cvDetail.SampleLevel(_cvDetail_sampler, q / 0.60000002384185791015625f.xxx, log2(max((cvFootKm * 32.0f) / 0.60000002384185791015625f, 1.0f))).x;
        float mod_ = lerp(det, 1.0f - det, clamp(hf * 4.0f, 0.0f, 1.0f)) * 0.20000000298023223876953125f;
        d += (clamp((base_ - mod_) / (1.0f - mod_), 0.0f, 1.0f) * cvBands.density[b]);
    }
    return d;
}
#endif

#if defined(WW_FN_cvVarHash) || defined(WW_FN_ALL)
float cvVarHash(int ix, int iy, int oct)
{
    uint h = ((uint(ix) * 2376512323u) ^ (uint(iy) * 3625334849u)) ^ (uint(oct) * 3407524639u);
    h ^= (h >> 13u);
    h *= 1540483477u;
    h ^= (h >> 15u);
    return float(h >> 8u) * 5.9604644775390625e-08f;
}
#endif

#if defined(WW_FN_cvVarLattice) || defined(WW_FN_ALL)
float cvVarLattice(int ix, int iy, int cells, int oct)
{
    int h = cells / 2;
    float tmp1;
    if (ix >= h)
    {
        int tmp2 = ix - h;
        int tmp3 = iy;
        int tmp4 = oct;
        tmp1 = 1.0f - cvVarHash(tmp2, tmp3, tmp4);
    }
    else
    {
        int tmp5 = ix;
        int tmp6 = iy;
        int tmp7 = oct;
        tmp1 = cvVarHash(tmp5, tmp6, tmp7);
    }
    return tmp1;
}
#endif

#if defined(WW_FN_cvVarValue) || defined(WW_FN_ALL)
float cvVarValue(float2 xy, float cellKm, int cells, int oct)
{
    float2 u = mod(xy, 384.0f.xx) / cellKm.xx;
    int2 i = min(int2(floor(u)), (cells - 1).xx);
    float2 f = u - float2(i);
    int tmp1;
    if ((i.x + 1) == cells)
    {
        tmp1 = 0;
    }
    else
    {
        tmp1 = i.x + 1;
    }
    int tmp2;
    if ((i.y + 1) == cells)
    {
        tmp2 = 0;
    }
    else
    {
        tmp2 = i.y + 1;
    }
    int2 j = int2(tmp1, tmp2);
    float2 w = (f * f) * (3.0f.xx - (f * 2.0f));
    int tmp3 = i.x;
    int tmp4 = i.y;
    int tmp5 = cells;
    int tmp6 = oct;
    float a = cvVarLattice(tmp3, tmp4, tmp5, tmp6);
    int tmp7 = j.x;
    int tmp8 = i.y;
    int tmp9 = cells;
    int tmp10 = oct;
    float b = cvVarLattice(tmp7, tmp8, tmp9, tmp10);
    int tmp11 = i.x;
    int tmp12 = j.y;
    int tmp13 = cells;
    int tmp14 = oct;
    float c = cvVarLattice(tmp11, tmp12, tmp13, tmp14);
    int tmp15 = j.x;
    int tmp16 = j.y;
    int tmp17 = cells;
    int tmp18 = oct;
    float e = cvVarLattice(tmp15, tmp16, tmp17, tmp18);
    return lerp(lerp(a, b, w.x), lerp(c, e, w.x), w.y);
}
#endif

#if defined(WW_FN_cvWeatherNoise) || defined(WW_FN_ALL)
float cvWeatherNoise(float2 xy)
{
    float2 tmp1 = xy;
    float tmp2 = 16.0f;
    int tmp3 = 24;
    int tmp4 = 0;
    float2 tmp5 = xy;
    float tmp6 = 6.0f;
    int tmp7 = 64;
    int tmp8 = 1;
    float v = (0.64999997615814208984375f * cvVarValue(tmp1, tmp2, tmp3, tmp4)) + (0.3499999940395355224609375f * cvVarValue(tmp5, tmp6, tmp7, tmp8));
    return clamp((v - 0.5f) * 4.0f, -1.0f, 1.0f);
}
#endif

#if defined(WW_FN_cvCurlRaw) || defined(WW_FN_ALL)
float3 cvCurlRaw(float3 q)
{
    float3 u = mod(q, 24.0f.xxx) / 24.0f.xxx;
    float c1 = cos(6.283185482025146484375f * frac(((13.0f * u.x) + (5.0f * u.y)) + 0.10999999940395355224609375f));
    float c2 = cos(6.283185482025146484375f * frac((((-6.0f) * u.x) + (11.0f * u.y)) + 0.3300000131130218505859375f));
    float c3 = cos(6.283185482025146484375f * frac(((10.0f * u.x) + (8.0f * u.z)) + 0.63999998569488525390625f));
    float c4 = cos(6.283185482025146484375f * frac((((-7.0f) * u.x) + (12.0f * u.z)) + 0.85000002384185791015625f));
    float ax = (c1 * 0.3589789867401123046875f) + (c2 * 0.877896010875701904296875f);
    float ay = (c1 * (-0.933345973491668701171875f)) + (c2 * 0.4788520038127899169921875f);
    float bx = (c3 * 0.624695003032684326171875f) + (c4 * 0.863779008388519287109375f);
    float bz = (c3 * (-0.780869007110595703125f)) + (c4 * 0.503871023654937744140625f);
    return float3(ax + bx, ay, bz) * 0.5f;
}
#endif

#if defined(WW_FN_cvTypeDepth) || defined(WW_FN_ALL)
float cvTypeDepth(float T)
{
    float u = clamp(T, 0.0f, 1.0f) * 3.0f;
    float dc = cvVarA.w;
    if (u < 1.0f)
    {
        return dc * lerp(0.300000011920928955078125f, 0.60000002384185791015625f, u);
    }
    if (u < 2.0f)
    {
        return dc * lerp(0.60000002384185791015625f, 1.0f, u - 1.0f);
    }
    return lerp(dc, max(cvVarB.x, dc), u - 2.0f);
}
#endif

#if defined(WW_FN_cvCalAtT) || defined(WW_FN_ALL)
float cvCalAtT(float T, float c)
{
    float u = (1.0f - sqrt(1.0f - clamp(T, 0.0f, 1.0f))) * 12.0f;
    int k = min(int(u), 11);
    float t = clamp(c, 0.0f, 1.0f) * 16.0f;
    int i = min(int(t), 15);
    float f = t - float(i);
    float a = lerp(cvCalT[(k * 17) + i], cvCalT[((k * 17) + i) + 1], f);
    float b = lerp(cvCalT[((k * 17) + 17) + i], cvCalT[((k * 17) + 18) + i], f);
    return lerp(a, b, u - float(k));
}
#endif

#if defined(WW_FN_cloudDensity_V2) || defined(WW_FN_ALL)
float cloudDensity(float3 p)
{
    float2 tmp1 = p.xy;
    float4 m = cvMapAt(tmp1);
    float3 q = p + cvBands.windKm;
    float2 tmp2 = q.xy;
    float n = cvWeatherNoise(tmp2);
    float d = 0.0f;
    float s = -1.0f;
    float3 curl = 0.0f.xxx;
    bool curlOk = false;
    float tmp3;
    float hf;
    float c;
    float tmp4;
    float prof;
    for (int b = 0; b < 2; b++)
    {
        if (cvBands.density[b] <= 0.0f)
        {
            continue;
        }
        if (b == 0)
        {
            tmp3 = m.x;
        }
        else
        {
            tmp3 = m.y;
        }
        float cov = tmp3;
        cov += ((cvVarA.z * n) * min(cov, 1.0f - cov));
        if (b == 0)
        {
            float sheet = clamp(m.z / max(m.x, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
            float T = clamp(cvVarA.x + (cvVarA.y * n), 0.0f, 1.0f) * (1.0f - sheet);
            float tmp5 = T;
            hf = (p.z - cvBands.baseKm.x) / max(cvTypeDepth(tmp5), 0.001000000047497451305389404296875f);
            bool wwh0 = (hf <= 0.0f);
            bool wwh1 = (hf >= 1.0f);
            if (wwh0 || wwh1)
            {
                continue;
            }
            float tmp6 = T;
            float tmp7 = cov;
            c = cvCalAtT(tmp6, tmp7);
            float tmp8 = 0.800000011920928955078125f;
            float tmp9 = 1.0f;
            float tmp10 = T;
            float tmp11 = 0.64999997615814208984375f;
            float tmp12 = 0.85000002384185791015625f;
            float tmp13 = hf;
            float e = lerp(1.0f, 0.5f, (cvVarB.w * cvSmooth(tmp8, tmp9, tmp10)) * cvSmooth(tmp11, tmp12, tmp13));
            if (c > 0.0f)
            {
                tmp4 = pow(c, e);
            }
            else
            {
                tmp4 = 0.0f;
            }
            c = tmp4;
            float tmp14 = T;
            float tmp15 = hf;
            prof = cvTypeProfile(tmp14, tmp15);
        }
        else
        {
            hf = (p.z - cvBands.baseKm.y) / max(cvBands.topKm.y - cvBands.baseKm.y, 0.001000000047497451305389404296875f);
            bool wwh2 = (hf <= 0.0f);
            bool wwh3 = (hf >= 1.0f);
            if (wwh2 || wwh3)
            {
                continue;
            }
            int tmp16 = 1;
            float tmp17 = cov;
            c = cvCalAt(tmp16, tmp17);
            int tmp18 = 1;
            float tmp19 = hf;
            float tmp20 = 0.0f;
            prof = cvGradient(tmp18, tmp19, tmp20);
        }
        if (c <= 0.0f)
        {
            continue;
        }
        if (s < 0.0f)
        {
            s = cvShape.SampleLevel(_cvShape_sampler, q / 6.0f.xxx, log2(max((cvFootKm * 128.0f) / 6.0f, 1.0f))).x;
        }
        float t = 1.0f - c;
        float base_ = clamp(((s * prof) - t) / max(1.0f - t, 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f) * c;
        if (base_ <= 0.0f)
        {
            continue;
        }
        if (!curlOk)
        {
            float3 tmp21 = q;
            curl = cvCurlRaw(tmp21);
            curlOk = true;
        }
        float3 qd = q + (curl * (cvVarB.z * (1.0f - (0.5f * hf))));
        float det = cvDetail.SampleLevel(_cvDetail_sampler, qd / 0.60000002384185791015625f.xxx, log2(max((cvFootKm * 32.0f) / 0.60000002384185791015625f, 1.0f))).x;
        float tmp22 = 0.0f;
        float tmp23 = 0.3499999940395355224609375f;
        float tmp24 = hf;
        float er = cvVarB.y * lerp(0.550000011920928955078125f, 1.0f, cvSmooth(tmp22, tmp23, tmp24));
        float mod_ = lerp(det, 1.0f - det, clamp(hf * 4.0f, 0.0f, 1.0f)) * er;
        d += (clamp((base_ - mod_) / (1.0f - mod_), 0.0f, 1.0f) * cvBands.density[b]);
    }
    return d;
}
#endif
