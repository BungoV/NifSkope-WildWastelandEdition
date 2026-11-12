// Shared HLSL for res/shaders/features/air/lookdev_fog.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_ddgiprobe_ps.hlsl, cell_fardots_ps.hlsl, cell_fogbloom_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volair_ps.hlsl, cell_volsky_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_fog_ps.hlsl, fo4_fogcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, lookdev_ground_csm_ps.hlsl, lookdev_ground_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl, pbrm_default_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwAirCurvedLeg, wwAirCurvedTau, wwVolAirSky
#ifdef WW_FN_ALL
#ifndef WW_LOOKDEV_FOG_GLSL
#define WW_LOOKDEV_FOG_GLSL 1
#endif
static const float kAirEarthU = 6371000.0 * 70.0;
#endif

#if defined(WW_FN_wwAirNightHue) || defined(WW_FN_ALL)
float3 wwAirNightHue(float3 L)
{
    bool tmp1 = fogNightHue <= 0.0f;
    bool wwh0 = fogNight.w <= 0.0f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = wwh0;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return L;
    }
    float yn = dot(fogNight.xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    if (yn <= 9.9999999600419720025001879548654e-13f)
    {
        return L;
    }
    return lerp(L, (fogNight.xyz * dot(max(L, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))) / yn.xxx, clamp(fogNight.w * fogNightHue, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwApOn) || defined(WW_FN_ALL)
bool wwApOn()
{
    return atAp.x > 0.5f;
}
#endif

#if defined(WW_FN_wwAtmoApAt) || defined(WW_FN_ALL)
float4 wwAtmoApAt(float3 posView)
{
    float dKm = length(posView) * atAp.z;
    float zv = max(-posView.z, 9.9999999747524270787835121154785e-07f);
    float2 ndc = float2((atApProj.x * posView.x) + (atApProj.z * posView.z), (atApProj.y * posView.y) + (atApProj.w * posView.z)) / zv.xx;
    float2 uv = clamp((ndc * 0.5f) + 0.5f.xx, 0.0f.xx, 1.0f.xx);
    float n = atAp.w;
    float s = sqrt(clamp(dKm / atAp.y, 0.0f, 1.0f));
    float4 v;
    if (s < (0.5f / n))
    {
        v = atApTex.Sample(_atApTex_sampler, float3(uv, 0.5f / n));
        float f = (s * n) * 2.0f;
        f *= f;
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        v = atApTex.Sample(_atApTex_sampler, float3(uv, s));
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwAtmoApAt_V2) || defined(WW_FN_ALL)
float4 wwAtmoApAt(float3 posView)
{
    float dKm = length(posView) * atAp.z;
    float zv = max(-posView.z, 9.9999999747524270787835121154785e-07f);
    float2 ndc = float2((atApProj.x * posView.x) + (atApProj.z * posView.z), (atApProj.y * posView.y) + (atApProj.w * posView.z)) / zv.xx;
    float2 uv = clamp((ndc * 0.5f) + 0.5f.xx, 0.0f.xx, 1.0f.xx);
    float n = atAp.w;
    float s = sqrt(clamp(dKm / atAp.y, 0.0f, 1.0f));
    float4 v;
    if (s < (0.5f / n))
    {
        v = atApTex.SampleLevel(_atApTex_sampler, float3(uv, 0.5f / n), 0.0f);
        float f = (s * n) * 2.0f;
        f *= f;
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        v = atApTex.SampleLevel(_atApTex_sampler, float3(uv, s), 0.0f);
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwFogEval) || defined(WW_FN_ALL)
float wwFogEval(float d, inout float z, inout float hb, out float3 fogCol)
{
    if ((fogRed & 4) != 0)
    {
        z = 0.0f;
    }
    float ramp = (d * fogK[0].x) - fogK[0].z;
    float f = clamp(ramp, 0.0f, 1.0f);
    float2 pair = clamp((fogK[5].xy * z) - fogK[5].zw, 0.0f.xx, 1.0f.xx);
    hb = lerp(pair.x, pair.y, f);
    float mx = fogK[2].w;
    bool tmp1 = ramp > 0.75f;
    bool wwh1 = (fogRed & 1) == 0;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh1;
    }
    else
    {
        tmp2 = tmp1;
    }
    float tmp3;
    if (tmp2)
    {
        tmp3 = min((((f - 0.75f) * 4.0f) * (1.0f - mx)) + mx, 1.0f);
    }
    else
    {
        tmp3 = mx;
    }
    float clampT = tmp3;
    bool tmp4 = ramp < 0.014999999664723873138427734375f;
    bool wwh2 = (fogRed & 2) == 0;
    bool tmp5;
    if (tmp4)
    {
        tmp5 = wwh2;
    }
    else
    {
        tmp5 = tmp4;
    }
    float tmp6;
    if (tmp5)
    {
        tmp6 = f * 66.6666717529296875f;
    }
    else
    {
        tmp6 = 1.0f;
    }
    float escape = tmp6;
    float tmp7;
    if (f > 0.0f)
    {
        tmp7 = min(clampT, pow(f, fogK[1].w));
    }
    else
    {
        tmp7 = 0.0f;
    }
    float I = tmp7;
    float weight = (hb * fogK[3].w) + (1.0f - hb);
    fogCol = lerp(lerp(fogK[1].xyz, fogK[3].xyz, I.xxx), lerp(fogK[2].xyz, fogK[4].xyz, I.xxx), hb.xxx);
    return (weight * I) * escape;
}
#endif

#if defined(WW_FN_wwAirFlatPos) || defined(WW_FN_ALL)
float3 wwAirFlatPos(float3 posView)
{
    float up2 = max(dot(fogView.xyz, fogView.xyz), 9.9999999600419720025001879548654e-13f);
    float h = dot(fogView.xyz, posView);
    float g2 = max((dot(posView, posView) * up2) - (h * h), 0.0f);
    return posView + (fogView.xyz * ((g2 / 891939968.0f) / up2));
}
#endif

#if defined(WW_FN_wwAirColOne) || defined(WW_FN_ALL)
float wwAirColOne(float lo, float hi, float z0, float invH)
{
    if (hi <= lo)
    {
        return 0.0f;
    }
    if (invH <= 0.0f)
    {
        return hi - lo;
    }
    float below = max(min(hi, z0) - lo, 0.0f);
    float a = max(lo, z0);
    if (hi <= a)
    {
        return below;
    }
    float w = hi - a;
    float u = w * invH;
    float tmp1;
    if (u < 0.001000000047497451305389404296875f)
    {
        tmp1 = 1.0f - (u * (0.5f - (u / 6.0f)));
    }
    else
    {
        tmp1 = (1.0f - exp(-u)) / u;
    }
    float e1 = tmp1;
    return below + ((exp((-(a - z0)) * invH) * w) * e1);
}
#endif

#if defined(WW_FN_wwAirEnd) || defined(WW_FN_ALL)
float wwAirEnd(float4 med, float4 ground)
{
    float tmp1;
    if (ground.z > 0.0f)
    {
        tmp1 = med.w + (16.0f / ground.z);
    }
    else
    {
        tmp1 = med.w;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_wwAirRhoLaw) || defined(WW_FN_ALL)
float wwAirRhoLaw(inout float z, float4 med, float4 layer, float4 ground)
{
    float k = 1.0f;
    if (z > med.w)
    {
        if (ground.z <= 0.0f)
        {
            return 0.0f;
        }
        k = exp((-(z - med.w)) * ground.z);
        z = med.w;
    }
    float tmp1;
    if (med.z > 0.0f)
    {
        tmp1 = min(1.0f, exp((-(z - med.y)) * med.z));
    }
    else
    {
        tmp1 = 1.0f;
    }
    float r = tmp1;
    bool tmp2 = layer.x <= 0.0f;
    bool wwh3 = layer.w <= 0.0f;
    bool tmp3;
    if (tmp2)
    {
        tmp3 = wwh3;
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return k * r;
    }
    if (layer.x > 0.0f)
    {
        float tmp4;
        if (layer.z > 0.0f)
        {
            tmp4 = min(1.0f, exp((-(z - layer.y)) * layer.z));
        }
        else
        {
            tmp4 = 1.0f;
        }
        r = ((1.0f - layer.x) * r) + (layer.x * tmp4);
    }
    if (layer.w > 0.0f)
    {
        float tmp5;
        if (ground.y > 0.0f)
        {
            tmp5 = min(1.0f, exp((-(z - ground.x)) * ground.y));
        }
        else
        {
            tmp5 = 1.0f;
        }
        r += (layer.w * tmp5);
    }
    return k * r;
}
#endif

#if defined(WW_FN_wwAirColLaw) || defined(WW_FN_ALL)
float wwAirColLaw(float lo, inout float hi, float4 med, float4 layer, float4 ground)
{
    bool tmp1 = ground.z > 0.0f;
    bool wwh4 = hi > med.w;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh4;
    }
    else
    {
        tmp2 = tmp1;
    }
    float tmp3;
    if (tmp2)
    {
        float tmp4 = med.w;
        float4 tmp5 = med;
        float4 tmp6 = layer;
        float4 tmp7 = ground;
        float tmp8 = wwAirRhoLaw(tmp4, tmp5, tmp6, tmp7);
        float tmp9 = max(lo, med.w);
        float tmp10 = hi;
        float tmp11 = med.w;
        float tmp12 = ground.z;
        tmp3 = tmp8 * wwAirColOne(tmp9, tmp10, tmp11, tmp12);
    }
    else
    {
        tmp3 = 0.0f;
    }
    float up = tmp3;
    hi = min(hi, med.w);
    float tmp13 = lo;
    float tmp14 = hi;
    float tmp15 = med.y;
    float tmp16 = med.z;
    float c = wwAirColOne(tmp13, tmp14, tmp15, tmp16);
    bool tmp17 = layer.x <= 0.0f;
    bool wwh5 = layer.w <= 0.0f;
    bool tmp18;
    if (tmp17)
    {
        tmp18 = wwh5;
    }
    else
    {
        tmp18 = tmp17;
    }
    if (tmp18)
    {
        return c + up;
    }
    if (layer.x > 0.0f)
    {
        float tmp19 = lo;
        float tmp20 = hi;
        float tmp21 = layer.y;
        float tmp22 = layer.z;
        c = ((1.0f - layer.x) * c) + (layer.x * wwAirColOne(tmp19, tmp20, tmp21, tmp22));
    }
    if (layer.w > 0.0f)
    {
        float tmp23 = lo;
        float tmp24 = hi;
        float tmp25 = ground.x;
        float tmp26 = ground.y;
        c += (layer.w * wwAirColOne(tmp23, tmp24, tmp25, tmp26));
    }
    return c + up;
}
#endif

#if defined(WW_FN_wwAirTerrH) || defined(WW_FN_ALL)
float wwAirTerrH(float3 p)
{
    float2 uv = float2(dot(volAirTerrU.xyz, p) + volAirTerrU.w, dot(volAirTerrV.xyz, p) + volAirTerrV.w);
    float2 uf = float2(dot(volAirTerrFU.xyz, p) + volAirTerrFU.w, dot(volAirTerrFV.xyz, p) + volAirTerrFV.w);
    float hc = volAirTerr.Sample(_volAirTerr_sampler, float3(uv, 0.0f)).x;
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float tmp1;
    if (wf > 0.0f)
    {
        tmp1 = lerp(hc, volAirTerr.Sample(_volAirTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        tmp1 = hc;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_wwAirTerrH_V2) || defined(WW_FN_ALL)
float wwAirTerrH(float3 p)
{
    float2 uv = float2(dot(volAirTerrU.xyz, p) + volAirTerrU.w, dot(volAirTerrV.xyz, p) + volAirTerrV.w);
    float2 uf = float2(dot(volAirTerrFU.xyz, p) + volAirTerrFU.w, dot(volAirTerrFV.xyz, p) + volAirTerrFV.w);
    float hc = volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uv, 0.0f), 0.0f).x;
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float tmp1;
    if (wf > 0.0f)
    {
        tmp1 = lerp(hc, volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        tmp1 = hc;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_wwAirGroundAt) || defined(WW_FN_ALL)
float4 wwAirGroundAt(float3 p)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    float3 tmp1 = p;
    return float4((volAirGround.x + wwAirTerrH(tmp1)) - volAirTerrRef, volAirGround.yzw);
}
#endif

#if defined(WW_FN_wwAirGroundRay) || defined(WW_FN_ALL)
float4 wwAirGroundRay(float3 p0, float3 dirV, float len, float s)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    float k = 1.0f / max(fogDistScale, 9.9999999600419720025001879548654e-13f);
    float2 u0 = float2(dot(volAirTerrU.xyz, p0) + volAirTerrU.w, dot(volAirTerrV.xyz, p0) + volAirTerrV.w);
    float2 du = float2(dot(volAirTerrU.xyz, dirV), dot(volAirTerrV.xyz, dirV)) * k;
    float tmp1;
    if (du.x > 0.0f)
    {
        tmp1 = (1.0f - u0.x) / du.x;
    }
    else
    {
        float tmp2;
        if (du.x < 0.0f)
        {
            tmp2 = (-u0.x) / du.x;
        }
        else
        {
            tmp2 = 1000000015047466219876688855040.0f;
        }
        tmp1 = tmp2;
    }
    float tmp3;
    if (du.y > 0.0f)
    {
        tmp3 = (1.0f - u0.y) / du.y;
    }
    else
    {
        float tmp4;
        if (du.y < 0.0f)
        {
            tmp4 = (-u0.y) / du.y;
        }
        else
        {
            tmp4 = 1000000015047466219876688855040.0f;
        }
        tmp3 = tmp4;
    }
    float2 ex = float2(tmp1, tmp3);
    float L = clamp(min(min(ex.x, ex.y), min(len, 10000000.0f)), 0.0f, 10000000.0f);
    float tmp5;
    if (volAirGround.y > 0.0f)
    {
        tmp5 = 1.0f / volAirGround.y;
    }
    else
    {
        tmp5 = 1000000000.0f;
    }
    float H = tmp5;
    float Lt = min(L, (40.0f * H) / max(abs(s), 9.999999717180685365747194737196e-10f));
    float tmp6;
    if (s < 0.0f)
    {
        tmp6 = L - Lt;
    }
    else
    {
        tmp6 = 0.0f;
    }
    float xs = tmp6;
    float b = (s * Lt) / H;
    float bp = b / 3.0f;
    float E = 1.0f - exp(-bp);
    float m = -1000000015047466219876688855040.0f;
    float t[6];
    float tmp7;
    float a[6];
    for (int q = 0; q < 6; q++)
    {
        float f = (float(q) + 0.5f) / 6.0f;
        if (abs(bp) < 0.001000000047497451305389404296875f)
        {
            tmp7 = f;
        }
        else
        {
            tmp7 = (-log(1.0f - (f * E))) / bp;
        }
        t[q] = tmp7;
        float3 tmp8 = p0 + (dirV * ((xs + (t[q] * Lt)) * k));
        a[q] = (wwAirTerrH(tmp8) / H) - ((b - bp) * t[q]);
        m = max(m, a[q]);
    }
    float lf[2];
    float tmp9;
    float tmp10;
    for (int i = 0; i < 2; i++)
    {
        float c = (i == 0) ? b : bp;
        if (abs(c) < 0.001000000047497451305389404296875f)
        {
            tmp9 = (-0.5f) * c;
        }
        else
        {
            if (c > 0.0f)
            {
                tmp10 = log((1.0f - exp(-c)) / c);
            }
            else
            {
                tmp10 = (-c) + log((1.0f - exp(c)) / (-c));
            }
            tmp9 = tmp10;
        }
        lf[i] = tmp9;
    }
    float sum = 0.0f;
    for (int q_1 = 0; q_1 < 6; q_1++)
    {
        sum += exp(a[q_1] - m);
    }
    float zg = H * (((m + log(sum / 6.0f)) + lf[1]) - lf[0]);
    return float4((volAirGround.x + zg) - volAirTerrRef, volAirGround.yzw);
}
#endif

#if defined(WW_FN_wwAirCol) || defined(WW_FN_ALL)
float wwAirCol(float lo, float hi, float4 ground)
{
    float tmp1 = lo;
    float tmp2 = hi;
    float4 tmp3 = volAirMed;
    float4 tmp4 = volAirLayer;
    float4 tmp5 = ground;
    float tmp6 = wwAirColLaw(tmp1, tmp2, tmp3, tmp4, tmp5);
    return tmp6;
}
#endif

#if defined(WW_FN_wwAirRainP) || defined(WW_FN_ALL)
float wwAirRainP(float h)
{
    if (h >= 0.0f)
    {
        return h + ((volAirRain.y * (1.0f - exp((-h) * volAirRain.z))) / volAirRain.z);
    }
    float hc = max(h, (-1.0f) / volAirRain.w);
    return (1.0f + volAirRain.y) * (hc + (((0.5f * hc) * hc) * volAirRain.w));
}
#endif

#if defined(WW_FN_wwAirRainTau) || defined(WW_FN_ALL)
float wwAirRainTau(float3 pA, float3 pB)
{
    if (volAirRain.x <= 0.0f)
    {
        return 0.0f;
    }
    float3 qA = mul(float4(pA, 1.0f), volAirRainXf).xyz;
    float3 dq = mul(float4(pB, 1.0f), volAirRainXf).xyz - qA;
    float hTop = (-1.0f) / volAirRain.w;
    float t0 = 0.0f;
    float t1 = 1.0f;
    if (abs(dq.z) > 9.999999717180685365747194737196e-10f)
    {
        float tc = (hTop - qA.z) / dq.z;
        if (dq.z > 0.0f)
        {
            t0 = max(t0, tc);
        }
        else
        {
            t1 = min(t1, tc);
        }
    }
    else
    {
        if (qA.z <= hTop)
        {
            return 0.0f;
        }
    }
    if (t1 <= t0)
    {
        return 0.0f;
    }
    int n = int(volAirRainFar.z);
    float w = (t1 - t0) / float(n);
    float dh = dq.z * w;
    bool level = abs(dh) <= 9.9999997473787516355514526367188e-05f;
    float tmp1;
    if (level)
    {
        tmp1 = 0.0f;
    }
    else
    {
        float tmp2 = qA.z + (dq.z * t0);
        tmp1 = wwAirRainP(tmp2);
    }
    float Pa = tmp1;
    float sum = 0.0f;
    float p;
    float tmp3;
    for (int k = 0; k < n; k++)
    {
        float3 qm = qA + (dq * (t0 + ((float(k) + 0.5f) * w)));
        float2 e = abs(qm.xy - 0.5f.xx) * 2.0f;
        float c = lerp(volAirRainMap.SampleLevel(_volAirRainMap_sampler, qm.xy, 0.0f).x, volAirRainFar.x, smoothstep(1.0f - volAirRainFar.y, 1.0f, max(e.x, e.y)));
        if (level)
        {
            if (qm.z >= 0.0f)
            {
                tmp3 = 1.0f + (volAirRain.y * exp((-qm.z) * volAirRain.z));
            }
            else
            {
                tmp3 = (1.0f + volAirRain.y) * max(1.0f + (qm.z * volAirRain.w), 0.0f);
            }
            p = tmp3;
        }
        else
        {
            float tmp4 = qA.z + (dq.z * (t0 + (float(k + 1) * w)));
            float Pb = wwAirRainP(tmp4);
            p = (Pb - Pa) / dh;
            Pa = Pb;
        }
        sum += (c * p);
    }
    return (((volAirRain.x * length(pB - pA)) * fogDistScale) * w) * sum;
}
#endif

#if defined(WW_FN_wwGridAt) || defined(WW_FN_ALL)
float4 wwGridAt(float2 uv, float2 share, float base, float n, float f)
{
    float2 lo = 0.5f.xx / volPackDims.xy;
    float2 st = clamp(uv * share, lo, share - lo);
    float4 v;
    if (f < 1.0f)
    {
        v = volTex.Sample(_volTex_sampler, float3(st, (base + 0.5f) / volPackDims.z));
        v = float4(v.xyz * max(f, 0.0f), 1.0f - ((1.0f - v.w) * max(f, 0.0f)));
    }
    else
    {
        v = volTex.Sample(_volTex_sampler, float3(st, ((base + min(f, n)) - 0.5f) / volPackDims.z));
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwGridAt_V2) || defined(WW_FN_ALL)
float4 wwGridAt(float2 uv, float2 share, float base, float n, float f)
{
    float2 lo = 0.5f.xx / volPackDims.xy;
    float2 st = clamp(uv * share, lo, share - lo);
    float4 v;
    if (f < 1.0f)
    {
        v = volTex.SampleLevel(_volTex_sampler, float3(st, (base + 0.5f) / volPackDims.z), 0.0f);
        v = float4(v.xyz * max(f, 0.0f), 1.0f - ((1.0f - v.w) * max(f, 0.0f)));
    }
    else
    {
        v = volTex.SampleLevel(_volTex_sampler, float3(st, ((base + min(f, n)) - 0.5f) / volPackDims.z), 0.0f);
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwAirTailJ) || defined(WW_FN_ALL)
float3 wwAirTailJ(float4 last, float4 prev)
{
    float dT = prev.w - last.w;
    if ((fogRed & 8) != 0)
    {
        if (dT > 1.0000000116860974230803549289703e-07f)
        {
            return max((last.xyz - prev.xyz) / dT.xxx, 0.0f.xxx);
        }
        bool tmp1 = volAirGround.z > 0.0f;
        bool wwh6 = last.w < 0.99989998340606689453125f;
        bool tmp2;
        if (tmp1)
        {
            tmp2 = wwh6;
        }
        else
        {
            tmp2 = tmp1;
        }
        float3 tmp3;
        if (tmp2)
        {
            tmp3 = max(last.xyz, 0.0f.xxx) / (1.0f - last.w).xxx;
        }
        else
        {
            tmp3 = 0.0f.xxx;
        }
        return tmp3;
    }
    float3 dL = max(last.xyz - prev.xyz, 0.0f.xxx);
    if ((-log(max(last.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f)
    {
        return dL / max(prev.w, 9.9999999747524270787835121154785e-07f).xxx;
    }
    float m = 1.0f - last.w;
    float3 tmp4;
    if (m > 1.0000000116860974230803549289703e-07f)
    {
        tmp4 = max(last.xyz, 0.0f.xxx) / m.xxx;
    }
    else
    {
        tmp4 = 0.0f.xxx;
    }
    float3 Jc = tmp4;
    float3 tmp5;
    if (dT > 1.0000000116860974230803549289703e-07f)
    {
        tmp5 = dL / dT.xxx;
    }
    else
    {
        tmp5 = 0.0f.xxx;
    }
    return lerp(Jc, tmp5, clamp(((dT * 64.0f) / max(last.w, 9.9999999747524270787835121154785e-07f)) - 1.0f, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwAirTailW) || defined(WW_FN_ALL)
float wwAirTailW(float T)
{
    return min(0.0625f / max(-log(max(T, 9.9999999747524270787835121154785e-07f)), 9.999999717180685365747194737196e-10f), 0.5f);
}
#endif

#if defined(WW_FN_wwAirTailF) || defined(WW_FN_ALL)
float wwAirTailF(float T, float lnSpan, float n)
{
    if ((fogRed & 8) != 0)
    {
        return n - 1.0f;
    }
    float tmp1 = T;
    return clamp(n * (1.0f + (log(1.0f - wwAirTailW(tmp1)) / lnSpan)), 1.0f, n - 1.0f);
}
#endif

#if defined(WW_FN_wwAirThinJ) || defined(WW_FN_ALL)
float3 wwAirThinJ(float3 J, float4 v, float z0, float far)
{
    float zc = fogView.w;
    bool tmp1 = (fogRed & 8) == 0;
    bool wwh7 = (-log(max(v.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh7;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool thin = tmp2;
    bool tmp3 = !thin;
    bool tmp4;
    if (tmp3)
    {
        bool tmp5 = (volAirFlags & 32768) == 0;
        bool wwh8 = volAirGround.z <= 0.0f;
        bool tmp6;
        if (!tmp5)
        {
            tmp6 = wwh8;
        }
        else
        {
            tmp6 = tmp5;
        }
        bool wwh9 = min(zc, z0) < volAirMed.w;
        bool tmp7;
        if (!tmp6)
        {
            tmp7 = wwh9;
        }
        else
        {
            tmp7 = tmp6;
        }
        tmp4 = tmp7;
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return J;
    }
    float tmp8;
    if (abs(z0 - zc) < 0.5f)
    {
        tmp8 = zc + 1.0f;
    }
    else
    {
        tmp8 = z0;
    }
    float z1 = tmp8;
    float k = (volAirMed.x * far) / abs(z1 - zc);
    float tmp9 = min(zc, z1);
    float tmp10 = max(zc, z1);
    float4 tmp11 = volAirGround;
    float od = k * wwAirCol(tmp9, tmp10, tmp11);
    if (thin)
    {
        float zh = 0.5f * (zc + z1);
        float tmp12 = min(zh, z1);
        float tmp13 = max(zh, z1);
        float4 tmp14 = volAirGround;
        float b = k * wwAirCol(tmp12, tmp13, tmp14);
        float xa = 1.0f - exp(-od);
        float m = 1.0f - v.w;
        b *= (clamp(xa, m - 0.00048828125f, m + 0.00048828125f) / max(xa, 9.999999717180685365747194737196e-10f));
        float3 tmp15;
        if (b > 1.0000000116860974230803549289703e-07f)
        {
            tmp15 = J / (b * (1.0f - (0.5f * b))).xxx;
        }
        else
        {
            tmp15 = 0.0f.xxx;
        }
        return tmp15;
    }
    float3 tmp16;
    if ((od > 1.0000000116860974230803549289703e-07f) && (od < 0.0199999995529651641845703125f))
    {
        tmp16 = max(v.xyz, 0.0f.xxx) / (od * (1.0f - (0.5f * od))).xxx;
    }
    else
    {
        tmp16 = J;
    }
    return tmp16;
}
#endif

#if defined(WW_FN_wwGridTailJ) || defined(WW_FN_ALL)
float3 wwGridTailJ(float2 uv, float4 a, float4 b, float4 whole)
{
    float nN = volPackDims.w;
    bool red = (fogRed & 8) != 0;
    float tmp1 = whole.w;
    float db = (volFar.x * exp(volFar.y)) * (1.0f - wwAirTailW(tmp1));
    bool wwh10 = db > volFar.x;
    bool tmp2;
    if (!red)
    {
        tmp2 = wwh10;
    }
    else
    {
        tmp2 = red;
    }
    bool inFar = tmp2;
    float tmp3;
    if (red)
    {
        tmp3 = volFar.z - 1.0f;
    }
    else
    {
        float tmp4;
        if (inFar)
        {
            tmp4 = min((log(db / volFar.x) / volFar.y) * volFar.z, volFar.z - 1.0f);
        }
        else
        {
            tmp4 = (log(db / volSlices.x) / volSlices.y) * nN;
        }
        tmp3 = tmp4;
    }
    float f = tmp3;
    float2 tmp5;
    if (inFar)
    {
        tmp5 = volPack.zw;
    }
    else
    {
        tmp5 = volPack.xy;
    }
    float tmp6;
    if (inFar)
    {
        tmp6 = volFar.z;
    }
    else
    {
        tmp6 = nN;
    }
    float2 tmp7 = uv;
    float2 tmp8 = tmp5;
    float tmp9 = inFar ? nN : 0.0f;
    float tmp10 = tmp6;
    float tmp11 = f;
    float4 p = wwGridAt(tmp7, tmp8, tmp9, tmp10, tmp11);
    if (red)
    {
        float4 tmp12 = b;
        float4 tmp13 = p;
        return wwAirTailJ(tmp12, tmp13);
    }
    float4 tmp14;
    if (inFar)
    {
        tmp14 = float4(a.xyz + (p.xyz * a.w), a.w * p.w);
    }
    else
    {
        tmp14 = p;
    }
    float4 tmp15 = whole;
    float4 tmp16 = tmp14;
    return wwAirTailJ(tmp15, tmp16);
}
#endif

#if defined(WW_FN_wwAirRecordColour) || defined(WW_FN_ALL)
float3 wwAirRecordColour(float d, float z, float3 dirView, inout bool ok)
{
    float tmp1 = d;
    float tmp2 = z;
    float tmp3;
    float3 tmp4;
    float tmp5 = wwFogEval(tmp1, tmp2, tmp3, tmp4);
    float hb = tmp3;
    float3 fc = tmp4;
    float tmp6;
    if (fogSun.w > 0.0f)
    {
        tmp6 = pow(max(dot(dirView, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    }
    else
    {
        tmp6 = 0.0f;
    }
    float sunW = tmp6;
    float3 c = max(lerp(fc, fogSunColour.xyz, sunW.xxx), 0.0f.xxx);
    float lc = dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    ok = lc > 9.9999999392252902907785028219223e-09f;
    float3 tmp7;
    if (ok)
    {
        tmp7 = c / lc.xxx;
    }
    else
    {
        tmp7 = 1.0f.xxx;
    }
    return tmp7;
}
#endif

#if defined(WW_FN_wwVolAirAtFar) || defined(WW_FN_ALL)
float4 wwVolAirAtFar(float d, float2 uv, float3 posView)
{
    float nN = volPackDims.w;
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float2 tmp1 = uv;
    float2 tmp2 = volPack.xy;
    float tmp3 = 0.0f;
    float tmp4 = nN;
    float tmp5 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(tmp1, tmp2, tmp3, tmp4, tmp5);
    if (d <= volFar.x)
    {
        return v;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 tmp6 = uv;
    float2 tmp7 = volPack.zw;
    float tmp8 = nN;
    float tmp9 = volFar.z;
    float tmp10 = fF;
    float4 w = wwGridAt(tmp6, tmp7, tmp8, tmp9, tmp10);
    float4 o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool tmp11 = fF <= volFar.z;
    bool wwh11 = (volAirFlags & 2) == 0;
    bool tmp12;
    if (!tmp11)
    {
        tmp12 = wwh11;
    }
    else
    {
        tmp12 = tmp11;
    }
    bool wwh12 = volAirMed.x <= 0.0f;
    bool tmp13;
    if (!tmp12)
    {
        tmp13 = wwh12;
    }
    else
    {
        tmp13 = tmp12;
    }
    if (tmp13)
    {
        return o;
    }
    float far = volFar.x * exp(volFar.y);
    float2 tmp14 = uv;
    float4 tmp15 = v;
    float4 tmp16 = w;
    float4 tmp17 = o;
    float3 J = wwGridTailJ(tmp14, tmp15, tmp16, tmp17);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 tmp18 = J;
    float4 tmp19 = o;
    float tmp20 = z0;
    float tmp21 = far;
    J = wwAirThinJ(tmp18, tmp19, tmp20, tmp21);
    float len = d - far;
    float dz = z1 - z0;
    float3 tmp22 = posView * (far / d);
    float3 tmp23 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float tmp24 = len;
    float tmp25 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(tmp22, tmp23, tmp24, tmp25);
    float tmp26;
    if (abs(dz) < 0.5f)
    {
        float tmp27 = z0;
        float tmp28 = z0 + 1.0f;
        float4 tmp29 = gT;
        tmp26 = (volAirMed.x * len) * wwAirCol(tmp27, tmp28, tmp29);
    }
    else
    {
        float tmp30 = min(z0, z1);
        float tmp31 = max(z0, z1);
        float4 tmp32 = gT;
        tmp26 = ((volAirMed.x * len) * wwAirCol(tmp30, tmp31, tmp32)) / abs(dz);
    }
    float tau = tmp26;
    float3 tmp33 = posView * (far / d);
    float3 tmp34 = posView;
    float Tt = exp(-(tau + wwAirRainTau(tmp33, tmp34)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float tmp35 = far;
        float tmp36 = z0;
        float3 tmp37 = dirV;
        bool tmp38;
        float3 tmp39 = wwAirRecordColour(tmp35, tmp36, tmp37, tmp38);
        bool ok0 = tmp38;
        float3 c0 = tmp39;
        float tmp40 = 0.5f * (far + d);
        float tmp41 = 0.5f * (z0 + z1);
        float3 tmp42 = dirV;
        bool tmp43;
        float3 tmp44 = wwAirRecordColour(tmp40, tmp41, tmp42, tmp43);
        bool ok1 = tmp43;
        float3 c1 = tmp44;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(o.xyz + ((J * o.w) * (1.0f - Tt)), o.w * Tt);
}
#endif

#if defined(WW_FN_wwVolTex) || defined(WW_FN_ALL)
float4 wwVolTex(float3 p, bool xyOnly)
{
    if ((volAirFlags & 32) == 0)
    {
        return volTex.Sample(_volTex_sampler, p);
    }
    float3 n = float3(int3(spvTextureSize(volTex, uint(0), dummy_parameter)));
    float3 c = (p * n) - 0.5f.xxx;
    float3 i = floor(c);
    float3 f = c - i;
    float3 w0 = (((((-f) + 3.0f.xxx) * f) - 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w1 = ((((f * 3.0f) - 6.0f.xxx) * f) * f) + 4.0f.xxx;
    float3 w2 = (((((f * (-3.0f)) + 3.0f.xxx) * f) + 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w3 = (f * f) * f;
    float3 g0 = w0 + w1;
    float3 g1 = w2 + w3;
    float3 h0 = ((i - 0.5f.xxx) + (w1 / g0)) / n;
    float3 h1 = ((i + 1.5f.xxx) + (w3 / g1)) / n;
    g0 /= 6.0f.xxx;
    g1 /= 6.0f.xxx;
    if (xyOnly)
    {
        return (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, p.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, p.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, p.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, p.z)) * g1.x)) * g1.y);
    }
    float4 a = (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, h0.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, h0.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, h0.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, h0.z)) * g1.x)) * g1.y);
    float4 b = (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, h1.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, h1.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, h1.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, h1.z)) * g1.x)) * g1.y);
    return (a * g0.z) + (b * g1.z);
}
#endif

#if defined(WW_FN_wwVolTex_V2) || defined(WW_FN_ALL)
float4 wwVolTex(float3 p, bool xyOnly)
{
    if ((volAirFlags & 32) == 0)
    {
        return volTex.SampleLevel(_volTex_sampler, p, 0.0f);
    }
    float3 n = float3(int3(spvTextureSize(volTex, uint(0), dummy_parameter)));
    float3 c = (p * n) - 0.5f.xxx;
    float3 i = floor(c);
    float3 f = c - i;
    float3 w0 = (((((-f) + 3.0f.xxx) * f) - 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w1 = ((((f * 3.0f) - 6.0f.xxx) * f) * f) + 4.0f.xxx;
    float3 w2 = (((((f * (-3.0f)) + 3.0f.xxx) * f) + 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w3 = (f * f) * f;
    float3 g0 = w0 + w1;
    float3 g1 = w2 + w3;
    float3 h0 = ((i - 0.5f.xxx) + (w1 / g0)) / n;
    float3 h1 = ((i + 1.5f.xxx) + (w3 / g1)) / n;
    g0 /= 6.0f.xxx;
    g1 /= 6.0f.xxx;
    if (xyOnly)
    {
        return (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, p.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, p.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, p.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, p.z), 0.0f) * g1.x)) * g1.y);
    }
    float4 a = (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, h0.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, h0.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, h0.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, h0.z), 0.0f) * g1.x)) * g1.y);
    float4 b = (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, h1.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, h1.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, h1.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, h1.z), 0.0f) * g1.x)) * g1.y);
    return (a * g0.z) + (b * g1.z);
}
#endif

#if defined(WW_FN_wwAirTailNearJ) || defined(WW_FN_ALL)
float3 wwAirTailNearJ(float2 uv, float4 v)
{
    float tmp1 = v.w;
    float tmp2 = volSlices.y;
    float tmp3 = volSlices.z;
    float3 tmp4 = float3(uv, (wwAirTailF(tmp1, tmp2, tmp3) - 0.5f) / volSlices.z);
    bool tmp5 = true;
    float4 tmp6 = v;
    float4 tmp7 = wwVolTex(tmp4, tmp5);
    return wwAirTailJ(tmp6, tmp7);
}
#endif

#if defined(WW_FN_wwVolAirFarHead) || defined(WW_FN_ALL)
bool wwVolAirFarHead(float d, float2 uv, float3 posView, inout float4 o, out float3 J, out float far)
{
    J = 0.0f.xxx;
    far = 0.0f;
    float nN = volPackDims.w;
    if (d <= volSlices.x)
    {
        o = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return false;
    }
    float2 tmp1 = uv;
    float2 tmp2 = volPack.xy;
    float tmp3 = 0.0f;
    float tmp4 = nN;
    float tmp5 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(tmp1, tmp2, tmp3, tmp4, tmp5);
    if (d <= volFar.x)
    {
        o = v;
        return false;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 tmp6 = uv;
    float2 tmp7 = volPack.zw;
    float tmp8 = nN;
    float tmp9 = volFar.z;
    float tmp10 = fF;
    float4 w = wwGridAt(tmp6, tmp7, tmp8, tmp9, tmp10);
    o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool tmp11 = fF <= volFar.z;
    bool wwh13 = (volAirFlags & 2) == 0;
    bool tmp12;
    if (!tmp11)
    {
        tmp12 = wwh13;
    }
    else
    {
        tmp12 = tmp11;
    }
    bool wwh14 = volAirMed.x <= 0.0f;
    bool tmp13;
    if (!tmp12)
    {
        tmp13 = wwh14;
    }
    else
    {
        tmp13 = tmp12;
    }
    if (tmp13)
    {
        return false;
    }
    far = volFar.x * exp(volFar.y);
    float2 tmp14 = uv;
    float4 tmp15 = v;
    float4 tmp16 = w;
    float4 tmp17 = o;
    J = wwGridTailJ(tmp14, tmp15, tmp16, tmp17);
    return true;
}
#endif

#if defined(WW_FN_wwVolAirNearHead) || defined(WW_FN_ALL)
bool wwVolAirNearHead(float d, float2 uv, inout float4 v, out float3 J, out float far)
{
    J = 0.0f.xxx;
    far = 0.0f;
    if (d <= volSlices.x)
    {
        v = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return false;
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    if (f < 1.0f)
    {
        float3 tmp1 = float3(uv, 0.5f / volSlices.z);
        bool tmp2 = true;
        v = wwVolTex(tmp1, tmp2);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 tmp3 = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool tmp4 = f > volSlices.z;
        v = wwVolTex(tmp3, tmp4);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool tmp5 = f <= volSlices.z;
    bool wwh15 = (volAirFlags & 2) == 0;
    bool tmp6;
    if (!tmp5)
    {
        tmp6 = wwh15;
    }
    else
    {
        tmp6 = tmp5;
    }
    bool wwh16 = volAirMed.x <= 0.0f;
    bool tmp7;
    if (!tmp6)
    {
        tmp7 = wwh16;
    }
    else
    {
        tmp7 = tmp6;
    }
    if (tmp7)
    {
        return false;
    }
    far = volSlices.x * exp(volSlices.y);
    float2 tmp8 = uv;
    float4 tmp9 = v;
    J = wwAirTailNearJ(tmp8, tmp9);
    return true;
}
#endif

#if defined(WW_FN_wwVolAirAtUv) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.Sample(_volTile_sampler, uv).x);
    }
    if (volFar.w > 0.5f)
    {
        float tmp1 = d;
        float2 tmp2 = uv;
        float3 tmp3 = posView;
        return wwVolAirAtFar(tmp1, tmp2, tmp3);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 tmp4 = float3(uv, 0.5f / volSlices.z);
        bool tmp5 = true;
        v = wwVolTex(tmp4, tmp5);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 tmp6 = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool tmp7 = f > volSlices.z;
        v = wwVolTex(tmp6, tmp7);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool tmp8 = f <= volSlices.z;
    bool wwh17 = (volAirFlags & 2) == 0;
    bool tmp9;
    if (!tmp8)
    {
        tmp9 = wwh17;
    }
    else
    {
        tmp9 = tmp8;
    }
    bool wwh18 = volAirMed.x <= 0.0f;
    bool tmp10;
    if (!tmp9)
    {
        tmp10 = wwh18;
    }
    else
    {
        tmp10 = tmp9;
    }
    if (tmp10)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 tmp11 = uv;
    float4 tmp12 = v;
    float3 J = wwAirTailNearJ(tmp11, tmp12);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 tmp13 = J;
    float4 tmp14 = v;
    float tmp15 = z0;
    float tmp16 = far;
    J = wwAirThinJ(tmp13, tmp14, tmp15, tmp16);
    float len = d - far;
    float dz = z1 - z0;
    float3 tmp17 = posView * (far / d);
    float3 tmp18 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float tmp19 = len;
    float tmp20 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(tmp17, tmp18, tmp19, tmp20);
    float tmp21;
    if (abs(dz) < 0.5f)
    {
        float tmp22 = z0;
        float tmp23 = z0 + 1.0f;
        float4 tmp24 = gT;
        tmp21 = (volAirMed.x * len) * wwAirCol(tmp22, tmp23, tmp24);
    }
    else
    {
        float tmp25 = min(z0, z1);
        float tmp26 = max(z0, z1);
        float4 tmp27 = gT;
        tmp21 = ((volAirMed.x * len) * wwAirCol(tmp25, tmp26, tmp27)) / abs(dz);
    }
    float tau = tmp21;
    float3 tmp28 = posView * (far / d);
    float3 tmp29 = posView;
    float Tt = exp(-(tau + wwAirRainTau(tmp28, tmp29)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float tmp30 = far;
        float tmp31 = z0;
        float3 tmp32 = dirV;
        bool tmp33;
        float3 tmp34 = wwAirRecordColour(tmp30, tmp31, tmp32, tmp33);
        bool ok0 = tmp33;
        float3 c0 = tmp34;
        float tmp35 = 0.5f * (far + d);
        float tmp36 = 0.5f * (z0 + z1);
        float3 tmp37 = dirV;
        bool tmp38;
        float3 tmp39 = wwAirRecordColour(tmp35, tmp36, tmp37, tmp38);
        bool ok1 = tmp38;
        float3 c1 = tmp39;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAtUv_V2) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.Sample(_volTile_sampler, uv).x);
    }
    if (volFar.w > 0.5f)
    {
        float tmp1 = d;
        float2 tmp2 = uv;
        float3 tmp3 = posView;
        return wwVolAirAtFar(tmp1, tmp2, tmp3);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 tmp4 = float3(uv, 0.5f / volSlices.z);
        bool tmp5 = true;
        v = wwVolTex(tmp4, tmp5);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 tmp6 = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool tmp7 = f > volSlices.z;
        v = wwVolTex(tmp6, tmp7);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool tmp8 = f <= volSlices.z;
    bool wwh19 = (volAirFlags & 2) == 0;
    bool tmp9;
    if (!tmp8)
    {
        tmp9 = wwh19;
    }
    else
    {
        tmp9 = tmp8;
    }
    bool wwh20 = volAirMed.x <= 0.0f;
    bool tmp10;
    if (!tmp9)
    {
        tmp10 = wwh20;
    }
    else
    {
        tmp10 = tmp9;
    }
    if (tmp10)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 tmp11 = uv;
    float4 tmp12 = v;
    float3 J = wwAirTailNearJ(tmp11, tmp12);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 tmp13 = J;
    float4 tmp14 = v;
    float tmp15 = z0;
    float tmp16 = far;
    J = wwAirThinJ(tmp13, tmp14, tmp15, tmp16);
    float len = d - far;
    float dz = z1 - z0;
    float3 tmp17 = posView * (far / d);
    float3 tmp18 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float tmp19 = len;
    float tmp20 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(tmp17, tmp18, tmp19, tmp20);
    float tmp21;
    if (abs(dz) < 0.5f)
    {
        float tmp22 = z0;
        float tmp23 = z0 + 1.0f;
        float4 tmp24 = gT;
        tmp21 = (volAirMed.x * len) * wwAirCol(tmp22, tmp23, tmp24);
    }
    else
    {
        float tmp25 = min(z0, z1);
        float tmp26 = max(z0, z1);
        float4 tmp27 = gT;
        tmp21 = ((volAirMed.x * len) * wwAirCol(tmp25, tmp26, tmp27)) / abs(dz);
    }
    float tau = tmp21;
    float3 tmp28 = posView * (far / d);
    float3 tmp29 = posView;
    float Tt = exp(-(tau + wwAirRainTau(tmp28, tmp29)));
    float3 tmp30 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float tmp31 = far;
    float tmp32 = d;
    float tmp33 = tau;
    J *= wwAirTailShadow(tmp30, tmp31, tmp32, tmp33);
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float tmp34 = far;
        float tmp35 = z0;
        float3 tmp36 = dirV;
        bool tmp37;
        float3 tmp38 = wwAirRecordColour(tmp34, tmp35, tmp36, tmp37);
        bool ok0 = tmp37;
        float3 c0 = tmp38;
        float tmp39 = 0.5f * (far + d);
        float tmp40 = 0.5f * (z0 + z1);
        float3 tmp41 = dirV;
        bool tmp42;
        float3 tmp43 = wwAirRecordColour(tmp39, tmp40, tmp41, tmp42);
        bool ok1 = tmp42;
        float3 c1 = tmp43;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAtUv_V3) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.SampleLevel(_volTile_sampler, uv, 0.0f).x);
    }
    if (volFar.w > 0.5f)
    {
        float tmp1 = d;
        float2 tmp2 = uv;
        float3 tmp3 = posView;
        return wwVolAirAtFar(tmp1, tmp2, tmp3);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 tmp4 = float3(uv, 0.5f / volSlices.z);
        bool tmp5 = true;
        v = wwVolTex(tmp4, tmp5);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 tmp6 = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool tmp7 = f > volSlices.z;
        v = wwVolTex(tmp6, tmp7);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool tmp8 = f <= volSlices.z;
    bool wwh21 = (volAirFlags & 2) == 0;
    bool tmp9;
    if (!tmp8)
    {
        tmp9 = wwh21;
    }
    else
    {
        tmp9 = tmp8;
    }
    bool wwh22 = volAirMed.x <= 0.0f;
    bool tmp10;
    if (!tmp9)
    {
        tmp10 = wwh22;
    }
    else
    {
        tmp10 = tmp9;
    }
    if (tmp10)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 tmp11 = uv;
    float4 tmp12 = v;
    float3 J = wwAirTailNearJ(tmp11, tmp12);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 tmp13 = J;
    float4 tmp14 = v;
    float tmp15 = z0;
    float tmp16 = far;
    J = wwAirThinJ(tmp13, tmp14, tmp15, tmp16);
    float len = d - far;
    float dz = z1 - z0;
    float3 tmp17 = posView * (far / d);
    float3 tmp18 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float tmp19 = len;
    float tmp20 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(tmp17, tmp18, tmp19, tmp20);
    float tmp21;
    if (abs(dz) < 0.5f)
    {
        float tmp22 = z0;
        float tmp23 = z0 + 1.0f;
        float4 tmp24 = gT;
        tmp21 = (volAirMed.x * len) * wwAirCol(tmp22, tmp23, tmp24);
    }
    else
    {
        float tmp25 = min(z0, z1);
        float tmp26 = max(z0, z1);
        float4 tmp27 = gT;
        tmp21 = ((volAirMed.x * len) * wwAirCol(tmp25, tmp26, tmp27)) / abs(dz);
    }
    float tau = tmp21;
    float3 tmp28 = posView * (far / d);
    float3 tmp29 = posView;
    float Tt = exp(-(tau + wwAirRainTau(tmp28, tmp29)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float tmp30 = far;
        float tmp31 = z0;
        float3 tmp32 = dirV;
        bool tmp33;
        float3 tmp34 = wwAirRecordColour(tmp30, tmp31, tmp32, tmp33);
        bool ok0 = tmp33;
        float3 c0 = tmp34;
        float tmp35 = 0.5f * (far + d);
        float tmp36 = 0.5f * (z0 + z1);
        float3 tmp37 = dirV;
        bool tmp38;
        float3 tmp39 = wwAirRecordColour(tmp35, tmp36, tmp37, tmp38);
        bool ok1 = tmp38;
        float3 c1 = tmp39;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAt) || defined(WW_FN_ALL)
float4 wwVolAirAt(inout float3 posView)
{
    float2 uv = (gl_FragCoord.xy - volRect.xy) * volRect.zw;
    if ((volAirFlags & 512) != 0)
    {
        float2 q = gl_FragCoord.xy + (5.588237762451171875f * float((volAirFlags >> 10) & 15)).xx;
        float3 n = frac(frac(float3(dot(q, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(47.0f, 17.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(13.0f, 71.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)))) * 52.98291778564453125f) - 0.5f.xxx;
        uv += (n.xy / (volPackDims.xy * volPack.xy));
        posView *= exp((n.z * volSlices.y) / volPackDims.w);
    }
    float3 tmp1 = posView;
    float2 tmp2 = uv;
    return wwVolAirAtUv(tmp1, tmp2);
}
#endif

#if defined(WW_FN_wwVolAirAt_V2) || defined(WW_FN_ALL)
float4 wwVolAirAt(inout float3 posView)
{
    float2 uv = (pcFragCoord.xy - volRect.xy) * volRect.zw;
    if ((volAirFlags & 512) != 0)
    {
        float2 q = pcFragCoord.xy + (5.588237762451171875f * float((volAirFlags >> 10) & 15)).xx;
        float3 n = frac(frac(float3(dot(q, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(47.0f, 17.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(13.0f, 71.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)))) * 52.98291778564453125f) - 0.5f.xxx;
        uv += (n.xy / (volPackDims.xy * volPack.xy));
        posView *= exp((n.z * volSlices.y) / volPackDims.w);
    }
    float3 tmp1 = posView;
    float2 tmp2 = uv;
    return wwVolAirAtUv(tmp1, tmp2);
}
#endif

#if defined(WW_FN_wwAirDriftAt) || defined(WW_FN_ALL)
float wwAirDriftAt(float3 p)
{
    float f = 1.0f - clamp(((length(p) * fogDistScale) - volAirDrift.y) * volAirDrift.z, 0.0f, 1.0f);
    bool wwh0 = (volAirDrift.x <= 0.0f);
    bool wwh1 = (f <= 0.0f);
    if (wwh0 || wwh1)
    {
        return 1.0f;
    }
    float4 q = float4(p, 1.0f);
    float a = volAirNoise.Sample(_volAirNoise_sampler, float3(dot(volAirDriftUv[0], q), dot(volAirDriftUv[1], q), dot(volAirDriftUv[2], q))).x;
    float b = volAirNoise.Sample(_volAirNoise_sampler, float3(dot(volAirDriftUv[3], q), dot(volAirDriftUv[4], q), dot(volAirDriftUv[5], q)) + float3(0.37000000476837158203125f, 0.61000001430511474609375f, 0.12999999523162841796875f)).x;
    return 1.0f + (((volAirDrift.x * f) * ((a - volAirDrift.w) + ((0.5f * f) * (b - volAirDrift.w)))) * 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwAirDriftAt_V2) || defined(WW_FN_ALL)
float wwAirDriftAt(float3 p)
{
    float f = 1.0f - clamp(((length(p) * fogDistScale) - volAirDrift.y) * volAirDrift.z, 0.0f, 1.0f);
    bool wwh2 = (volAirDrift.x <= 0.0f);
    bool wwh3 = (f <= 0.0f);
    if (wwh2 || wwh3)
    {
        return 1.0f;
    }
    float4 q = float4(p, 1.0f);
    float a = volAirNoise.SampleLevel(_volAirNoise_sampler, float3(dot(volAirDriftUv[0], q), dot(volAirDriftUv[1], q), dot(volAirDriftUv[2], q)), 0.0f).x;
    float b = volAirNoise.SampleLevel(_volAirNoise_sampler, float3(dot(volAirDriftUv[3], q), dot(volAirDriftUv[4], q), dot(volAirDriftUv[5], q)) + float3(0.37000000476837158203125f, 0.61000001430511474609375f, 0.12999999523162841796875f), 0.0f).x;
    return 1.0f + (((volAirDrift.x * f) * ((a - volAirDrift.w) + ((0.5f * f) * (b - volAirDrift.w)))) * 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwAirDriftSunK) || defined(WW_FN_ALL)
float wwAirDriftSunK(float3 p, float lo, float mu, float top)
{
    if (volAirDrift.x <= 0.0f)
    {
        return 1.0f;
    }
    float3 s = fogSun.xyz * (min(max(top - lo, 0.0f) / max(mu, 0.0199999995529651641845703125f), 4096.0f) / max(fogDistScale, 9.9999999600419720025001879548654e-13f));
    int n = ((volAirDriftRed & 1) != 0) ? 1 : 3;
    float f0 = (n == 1) ? 0.5f : 0.16666667163372039794921875f;
    float k = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float3 tmp1 = p + (s * (f0 + (float(i) / 3.0f)));
        k += wwAirDriftAt(tmp1);
    }
    return k / float(n);
}
#endif

#if defined(WW_FN_wwVolAirSunTD) || defined(WW_FN_ALL)
float2 wwVolAirSunTD(float3 posView)
{
    bool tmp1 = (!fogOn) || (!volOn);
    bool wwh23 = volSlices.w < 0.5f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = wwh23;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool wwh24 = fogSun.w <= 0.0f;
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = wwh24;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool wwh25 = volAirMed.x <= 0.0f;
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = wwh25;
    }
    else
    {
        tmp4 = tmp3;
    }
    bool wwh26 = (volAirFlags & 1) == 0;
    bool tmp5;
    if (!tmp4)
    {
        tmp5 = wwh26;
    }
    else
    {
        tmp5 = tmp4;
    }
    if (tmp5)
    {
        return float2(1.0f, 0.0f);
    }
    float lo = dot(fogView.xyz, posView) + fogView.w;
    float4 tmp6 = volAirMed;
    float4 tmp7 = volAirGround;
    float end = wwAirEnd(tmp6, tmp7);
    if (lo >= end)
    {
        return float2(1.0f, 0.0f);
    }
    float mu = max(dot(fogView.xyz, fogSun.xyz) / max(length(fogView.xyz), 9.9999999600419720025001879548654e-13f), 0.0199999995529651641845703125f);
    float3 tmp8 = posView;
    float4 g = wwAirGroundAt(tmp8);
    float tmp9;
    if ((volAirFlags & 128) != 0)
    {
        float tmp10;
        if (volAirLayer.w > 0.0f)
        {
            float tmp11 = lo;
            float tmp12 = volAirMed.w;
            float tmp13 = g.x;
            float tmp14 = g.y;
            tmp10 = volAirLayer.w * wwAirColOne(tmp11, tmp12, tmp13, tmp14);
        }
        else
        {
            tmp10 = 0.0f;
        }
        tmp9 = tmp10;
    }
    else
    {
        float tmp15 = lo;
        float tmp16 = end;
        float4 tmp17 = g;
        tmp9 = wwAirCol(tmp15, tmp16, tmp17);
    }
    float c = tmp9;
    float3 tmp18 = posView;
    float tmp19 = lo;
    float tmp20 = mu;
    float tmp21 = volAirMed.w;
    float tau = (volAirMed.x * c) * wwAirDriftSunK(tmp18, tmp19, tmp20, tmp21);
    float Td = exp((-tau) / mu);
    if (volAirGround.w <= 0.0f)
    {
        return float2(Td, 0.0f);
    }
    float pg = volAirGround.w - 1.0f;
    float gp = pg / (1.0f + pg);
    float tp = (1.0f - (pg * pg)) * tau;
    float g1 = 0.75f * (1.0f - gp);
    float Tt = 1.0f - (((g1 * tp) + (((0.25f * (2.0f - ((3.0f * gp) * mu))) - (g1 * mu)) * (1.0f - exp((-tp) / mu)))) / (1.0f + (g1 * tp)));
    Tt /= (1.0f + (((0.0350000001490116119384765625f * max(pg, 0.0f)) * ((1.0f / mu) - 1.0f)) * (1.0f - Td)));
    return float2(Td, max(Tt - Td, 0.0f) * mu);
}
#endif

#if defined(WW_FN_wwVolAirSunT) || defined(WW_FN_ALL)
float wwVolAirSunT(float3 posView)
{
    float3 tmp1 = posView;
    return wwVolAirSunTD(tmp1).x;
}
#endif

#if defined(WW_FN_wwFogBase) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float tmp1 = d;
    float tmp2 = z;
    float tmp3;
    float3 tmp4;
    float tmp5 = wwFogEval(tmp1, tmp2, tmp3, tmp4);
    float hb = tmp3;
    float3 fogCol = tmp4;
    float alpha = tmp5;
    if (apOn)
    {
        float tmp6 = tAp;
        float tmp7 = 1.0f - alpha;
        bool tmp8 = atApDouble > 0.5f;
        alpha = 1.0f - wwApFogResidualT(tmp6, tmp7, tmp8);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float tmp9;
    if (fogUnit > 0.0f)
    {
        tmp9 = gray / fogUnit;
    }
    else
    {
        tmp9 = gray;
    }
    float grayW = tmp9;
    float3 tmp10;
    if (alpha < fogK[2].w)
    {
        tmp10 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        tmp10 = sunlit;
    }
    float3 resolved = tmp10;
    return lerp(preFog, resolved, alpha.xxx);
}
#endif

#if defined(WW_FN_wwFogBase_V2) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float tmp1 = d;
    float tmp2 = z;
    float tmp3;
    float3 tmp4;
    float tmp5 = wwFogEval(tmp1, tmp2, tmp3, tmp4);
    float hb = tmp3;
    float3 fogCol = tmp4;
    float alpha_1 = tmp5;
    if (apOn)
    {
        float tmp6 = tAp;
        float tmp7 = 1.0f - alpha_1;
        bool tmp8 = atApDouble > 0.5f;
        alpha_1 = 1.0f - wwApFogResidualT(tmp6, tmp7, tmp8);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float tmp9;
    if (fogUnit > 0.0f)
    {
        tmp9 = gray / fogUnit;
    }
    else
    {
        tmp9 = gray;
    }
    float grayW = tmp9;
    float3 tmp10;
    if (alpha_1 < fogK[2].w)
    {
        tmp10 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        tmp10 = sunlit;
    }
    float3 resolved = tmp10;
    return lerp(preFog, resolved, alpha_1.xxx);
}
#endif

#if defined(WW_FN_wwFog) || defined(WW_FN_ALL)
float3 wwFog(inout float3 preFog, float3 posView)
{
    bool apOn = wwApOn();
    float4 tmp1;
    if (apOn)
    {
        float3 tmp2 = posView;
        tmp1 = wwAtmoApAt(tmp2);
    }
    else
    {
        tmp1 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float4 ap = tmp1;
    if (apOn)
    {
        preFog = (preFog * ap.w) + ap.xyz;
    }
    if (!fogOn)
    {
        return preFog;
    }
    bool wwh27 = volSlices.w > 0.5f;
    bool tmp3;
    if (volOn)
    {
        tmp3 = wwh27;
    }
    else
    {
        tmp3 = volOn;
    }
    if (tmp3)
    {
        float3 tmp4 = posView;
        float4 tmp5 = wwVolAirAt(tmp4);
        float4 a = tmp5;
        if (apOn)
        {
            bool dbl = atApDouble > 0.5f;
            float tmp6 = ap.w;
            float tmp7 = a.w;
            bool tmp8 = dbl;
            float tr = wwApFogResidualT(tmp6, tmp7, tmp8);
            float3 tmp9;
            if (dbl)
            {
                tmp9 = a.xyz;
            }
            else
            {
                float3 tmp10 = a.xyz;
                float tmp11 = a.w;
                float tmp12 = tr;
                tmp9 = wwApFogResidualL(tmp10, tmp11, tmp12);
            }
            float3 lr = tmp9;
            if (fogNight.w > 0.0f)
            {
                float3 tmp13 = lr;
                lr = max(wwAirNightHue(tmp13), (fogNight.xyz * (1.0f - (ap.w * tr))) - (ap.xyz * tr));
            }
            return (preFog * tr) + lr;
        }
        if (fogNight.w > 0.0f)
        {
            float3 tmp14 = a.xyz;
            float tmp15 = a.w;
            float3 tmp16 = max(wwAirNightHue(tmp14), fogNight.xyz * (1.0f - tmp15));
            a.x = tmp16.x;
            a.y = tmp16.y;
            a.z = tmp16.z;
        }
        float3 lit = (preFog * a.w) + a.xyz;
        if (volFogBlend == 1)
        {
            float3 tmp17 = lit;
            float3 tmp18 = posView;
            float tmp19 = 1.0f;
            bool tmp20 = false;
            return wwFogBase(tmp17, tmp18, tmp19, tmp20);
        }
        if (volFogBlend == 2)
        {
            float tmp21 = length(posView) * fogDistScale;
            float tmp22 = dot(fogView.xyz, posView) + fogView.w;
            float tmp23;
            float3 tmp24;
            float tmp25 = wwFogEval(tmp21, tmp22, tmp23, tmp24);
            float hb = tmp23;
            float3 fc = tmp24;
            return lerp(lit, fc, (1.0f - a.w).xxx);
        }
        return lit;
    }
    float3 tmp26 = preFog;
    float3 tmp27 = posView;
    float tmp28 = ap.w;
    bool tmp29 = apOn;
    return wwFogBase(tmp26, tmp27, tmp28, tmp29);
}
#endif

#if defined(WW_FN_wwFogFx) || defined(WW_FN_ALL)
float3 wwFogFx(inout float3 lin, float3 posView, bool additive)
{
    bool apOn = wwApOn();
    if ((!fogOn) && (!apOn))
    {
        return lin;
    }
    bool wwh28 = volSlices.w > 0.5f;
    bool tmp1;
    if (volOn)
    {
        tmp1 = wwh28;
    }
    else
    {
        tmp1 = volOn;
    }
    bool wwh29 = (volAirFlags & 16) != 0;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh29;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool volFx = tmp2;
    bool tmp3 = !additive;
    bool tmp4;
    if (tmp3)
    {
        tmp4 = volFx || apOn;
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        float3 tmp5 = lin;
        float3 tmp6 = posView;
        float3 tmp7 = wwFog(tmp5, tmp6);
        return max(tmp7, 0.0f.xxx);
    }
    float tmp8;
    if (apOn)
    {
        float3 tmp9 = posView;
        tmp8 = wwAtmoApAt(tmp9).w;
    }
    else
    {
        tmp8 = 1.0f;
    }
    float tAp = tmp8;
    if (!fogOn)
    {
        return lin * tAp;
    }
    float tmp10 = length(posView) * fogDistScale;
    float tmp11 = dot(fogView.xyz, posView) + fogView.w;
    float tmp12;
    float3 tmp13;
    float tmp14 = wwFogEval(tmp10, tmp11, tmp12, tmp13);
    float hb = tmp12;
    float3 fc = tmp13;
    float f = tmp14;
    if (!volFx)
    {
        if (apOn)
        {
            float tmp15 = tAp;
            float tmp16 = 1.0f - f;
            bool tmp17 = atApDouble > 0.5f;
            return (lin * tAp) * wwApFogResidualT(tmp15, tmp16, tmp17);
        }
        float3 tmp18;
        if (additive)
        {
            tmp18 = lin * (1.0f - f);
        }
        else
        {
            tmp18 = lerp(lin, fc, f.xxx);
        }
        return tmp18;
    }
    float3 tmp19 = posView;
    float4 tmp20 = wwVolAirAt(tmp19);
    float tv = tmp20.w;
    if (apOn)
    {
        float tmp21 = tAp;
        float tmp22 = tv;
        bool tmp23 = atApDouble > 0.5f;
        return (lin * tAp) * wwApFogResidualT(tmp21, tmp22, tmp23);
    }
    lin *= tv;
    float3 tmp24;
    if (volFogBlend == 1)
    {
        tmp24 = lin * (1.0f - f);
    }
    else
    {
        tmp24 = lin;
    }
    return tmp24;
}
#endif

#if defined(WW_FN_wwFogProbe) || defined(WW_FN_ALL)
bool wwFogProbe(float3 posView, out float3 o)
{
    o = 0.0f.xxx;
    bool tmp1 = fogOn && volOn;
    bool wwh30 = (volProbe == 1) || (volProbe == 2);
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh30;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        float3 tmp3 = posView;
        float4 tmp4 = wwVolAirAt(tmp3);
        float4 a = tmp4;
        float3 tmp5;
        if (volProbe == 1)
        {
            tmp5 = a.xyz;
        }
        else
        {
            tmp5 = a.w.xxx;
        }
        o = tmp5;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 tmp6 = posView;
        o = wwVolAirSunT(tmp6).xxx;
        return true;
    }
    bool tmp7 = !fogOn;
    bool wwh31 = fogProbe.z < 0.5f;
    bool tmp8;
    if (!tmp7)
    {
        tmp8 = wwh31;
    }
    else
    {
        tmp8 = tmp7;
    }
    if (tmp8)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float tmp9;
        if (fogProbe.z < 9.5f)
        {
            tmp9 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            tmp9 = (z + 65536.0f) / 131072.0f;
        }
        float x = tmp9;
        float q = floor((clamp(x, 0.0f, 1.0f) * 65535.0f) + 0.5f);
        o = float3(floor(q / 256.0f) / 255.0f, mod(q, 256.0f) / 255.0f, 0.0f);
        return true;
    }
    if (fogProbe.z > 7.5f)
    {
        float d_1 = length(posView) * fogDistScale;
        float z_1 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_1 / 16384.0f, 0.5f + (z_1 / 32768.0f), 0.0f);
        return true;
    }
    if (fogProbe.z > 5.5f)
    {
        float d_2 = length(posView) * fogDistScale;
        float z_2 = dot(fogView.xyz, posView) + fogView.w;
        float tmp10 = d_2;
        float tmp11 = z_2;
        float tmp12;
        float3 tmp13;
        float tmp14 = wwFogEval(tmp10, tmp11, tmp12, tmp13);
        float hb = tmp12;
        float3 fogCol = tmp13;
        float alpha_1 = tmp14;
        float3 tmp15;
        if (fogProbe.z < 6.5f)
        {
            tmp15 = float3(alpha_1, hb, 0.0f);
        }
        else
        {
            tmp15 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = tmp15;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float tmp16 = fogProbe.x;
    float tmp17 = fogProbe.y;
    float tmp18;
    float3 tmp19;
    float tmp20 = wwFogEval(tmp16, tmp17, tmp18, tmp19);
    float hb_1 = tmp18;
    float3 fogCol_1 = tmp19;
    float alpha_2 = tmp20;
    if (fogProbe.z < 1.5f)
    {
        o = alpha_2.xxx;
    }
    else
    {
        if (fogProbe.z < 2.5f)
        {
            o = fogCol_1 * 0.5f;
        }
        else
        {
            o = hb_1.xxx;
        }
    }
    return true;
}
#endif

#if defined(WW_FN_wwFogProbe_V2) || defined(WW_FN_ALL)
bool wwFogProbe(float3 posView, out float3 o)
{
    o = 0.0f.xxx;
    bool tmp1 = fogOn && volOn;
    bool wwh32 = (volProbe == 1) || (volProbe == 2);
    bool tmp2;
    if (tmp1)
    {
        tmp2 = wwh32;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        float3 tmp3 = posView;
        float4 tmp4 = wwVolAirAt(tmp3);
        float4 a = tmp4;
        float3 tmp5;
        if (volProbe == 1)
        {
            tmp5 = a.xyz;
        }
        else
        {
            tmp5 = a.w.xxx;
        }
        o = tmp5;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 tmp6 = posView;
        o = wwVolAirSunT(tmp6).xxx;
        return true;
    }
    bool tmp7 = !fogOn;
    bool wwh33 = fogProbe.z < 0.5f;
    bool tmp8;
    if (!tmp7)
    {
        tmp8 = wwh33;
    }
    else
    {
        tmp8 = tmp7;
    }
    if (tmp8)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float tmp9;
        if (fogProbe.z < 9.5f)
        {
            tmp9 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            tmp9 = (z + 65536.0f) / 131072.0f;
        }
        float x = tmp9;
        float q = floor((clamp(x, 0.0f, 1.0f) * 65535.0f) + 0.5f);
        o = float3(floor(q / 256.0f) / 255.0f, mod(q, 256.0f) / 255.0f, 0.0f);
        return true;
    }
    if (fogProbe.z > 7.5f)
    {
        float d_1 = length(posView) * fogDistScale;
        float z_1 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_1 / 16384.0f, 0.5f + (z_1 / 32768.0f), 0.0f);
        return true;
    }
    if (fogProbe.z > 5.5f)
    {
        float d_2 = length(posView) * fogDistScale;
        float z_2 = dot(fogView.xyz, posView) + fogView.w;
        float tmp10 = d_2;
        float tmp11 = z_2;
        float tmp12;
        float3 tmp13;
        float tmp14 = wwFogEval(tmp10, tmp11, tmp12, tmp13);
        float hb = tmp12;
        float3 fogCol = tmp13;
        float alpha = tmp14;
        float3 tmp15;
        if (fogProbe.z < 6.5f)
        {
            tmp15 = float3(alpha, hb, 0.0f);
        }
        else
        {
            tmp15 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = tmp15;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float tmp16 = fogProbe.x;
    float tmp17 = fogProbe.y;
    float tmp18;
    float3 tmp19;
    float tmp20 = wwFogEval(tmp16, tmp17, tmp18, tmp19);
    float hb_1 = tmp18;
    float3 fogCol_1 = tmp19;
    float alpha_1 = tmp20;
    if (fogProbe.z < 1.5f)
    {
        o = alpha_1.xxx;
    }
    else
    {
        if (fogProbe.z < 2.5f)
        {
            o = fogCol_1 * 0.5f;
        }
        else
        {
            o = hb_1.xxx;
        }
    }
    return true;
}
#endif
