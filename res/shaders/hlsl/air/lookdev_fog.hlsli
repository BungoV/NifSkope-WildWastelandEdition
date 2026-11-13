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
    bool cond2 = fogNightHue <= 0.0f;
    bool wwh0 = fogNight.w <= 0.0f;
    bool cond;
    if (!cond2)
    {
        cond = wwh0;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
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
    bool cond2 = ramp > 0.75f;
    bool wwh1 = (fogRed & 1) == 0;
    bool cond;
    if (cond2)
    {
        cond = wwh1;
    }
    else
    {
        cond = cond2;
    }
    float clampTPick;
    if (cond)
    {
        clampTPick = min((((f - 0.75f) * 4.0f) * (1.0f - mx)) + mx, 1.0f);
    }
    else
    {
        clampTPick = mx;
    }
    float clampT = clampTPick;
    bool cond4 = ramp < 0.014999999664723873138427734375f;
    bool wwh2 = (fogRed & 2) == 0;
    bool cond3;
    if (cond4)
    {
        cond3 = wwh2;
    }
    else
    {
        cond3 = cond4;
    }
    float escapePick;
    if (cond3)
    {
        escapePick = f * 66.6666717529296875f;
    }
    else
    {
        escapePick = 1.0f;
    }
    float escape = escapePick;
    float IPick;
    if (f > 0.0f)
    {
        IPick = min(clampT, pow(f, fogK[1].w));
    }
    else
    {
        IPick = 0.0f;
    }
    float I = IPick;
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
    // |fogView.xyz| = view -> world, h is world
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
    float cond2;
    if (u < 0.001000000047497451305389404296875f)
    {
        cond2 = 1.0f - (u * (0.5f - (u / 6.0f)));
    }
    else
    {
        cond2 = (1.0f - exp(-u)) / u;
    }
    float e1 = cond2;
    return below + ((exp((-(a - z0)) * invH) * w) * e1);
}
#endif

#if defined(WW_FN_wwAirEnd) || defined(WW_FN_ALL)
float wwAirEnd(float4 med, float4 ground)
{
    float cond2;
    if (ground.z > 0.0f)
    {
        cond2 = med.w + (16.0f / ground.z);
    }
    else
    {
        cond2 = med.w;
    }
    return cond2;
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
        // G2: the top's density, falling off
        k = exp((-(z - med.w)) * ground.z);
        z = med.w;
    }
    float cond2;
    if (med.z > 0.0f)
    {
        cond2 = min(1.0f, exp((-(z - med.y)) * med.z));
    }
    else
    {
        cond2 = 1.0f;
    }
    float r = cond2;
    bool cond = layer.x <= 0.0f;
    bool wwh3 = layer.w <= 0.0f;
    bool clampTPick;
    if (cond)
    {
        clampTPick = wwh3;
    }
    else
    {
        clampTPick = cond;
    }
    if (clampTPick)
    {
        return k * r;
    }
    if (layer.x > 0.0f)
    {
        float cond4;
        if (layer.z > 0.0f)
        {
            cond4 = min(1.0f, exp((-(z - layer.y)) * layer.z));
        }
        else
        {
            cond4 = 1.0f;
        }
        r = ((1.0f - layer.x) * r) + (layer.x * cond4);
    }
    if (layer.w > 0.0f)
    {
        float cond3;
        if (ground.y > 0.0f)
        {
            cond3 = min(1.0f, exp((-(z - ground.x)) * ground.y));
        }
        else
        {
            cond3 = 1.0f;
        }
        r += (layer.w * cond3);
    }
    return k * r;
}
#endif

#if defined(WW_FN_wwAirColLaw) || defined(WW_FN_ALL)
float wwAirColLaw(float lo, inout float hi, float4 med, float4 layer, float4 ground)
{
    bool cond2 = ground.z > 0.0f;
    bool wwh4 = hi > med.w;
    bool cond;
    if (cond2)
    {
        cond = wwh4;
    }
    else
    {
        cond = cond2;
    }
    float clampTPick;
    if (cond)
    {
        float cond4 = med.w;
        float4 cond3 = med;
        float4 escapePick = layer;
        float4 IPick = ground;
        float wwAirRhoLawRet = wwAirRhoLaw(cond4, cond3, escapePick, IPick);
        float clampTPart4 = max(lo, med.w);
        float clampTPart3 = hi;
        float clampTPart2 = med.w;
        float clampTPart = ground.z;
        clampTPick = wwAirRhoLawRet * wwAirColOne(clampTPart4, clampTPart3, clampTPart2, clampTPart);
    }
    else
    {
        clampTPick = 0.0f;
    }
    float up = clampTPick;
    hi = min(hi, med.w);
    float cPart12 = lo;
    float cPart11 = hi;
    float cPart10 = med.y;
    float cPart9 = med.z;
    float c = wwAirColOne(cPart12, cPart11, cPart10, cPart9);
    bool cond6 = layer.x <= 0.0f;
    bool wwh5 = layer.w <= 0.0f;
    bool cond5;
    if (cond6)
    {
        cond5 = wwh5;
    }
    else
    {
        cond5 = cond6;
    }
    if (cond5)
    {
        return c + up;
    }
    if (layer.x > 0.0f)
    {
        float cPart8 = lo;
        float cPart7 = hi;
        float cPart6 = layer.y;
        float cPart5 = layer.z;
        c = ((1.0f - layer.x) * c) + (layer.x * wwAirColOne(cPart8, cPart7, cPart6, cPart5));
    }
    if (layer.w > 0.0f)
    {
        float cPart4 = lo;
        float cPart3 = hi;
        float cPart2 = ground.x;
        float cPart = ground.y;
        c += (layer.w * wwAirColOne(cPart4, cPart3, cPart2, cPart));
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
    float cond2;
    if (wf > 0.0f)
    {
        cond2 = lerp(hc, volAirTerr.Sample(_volAirTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        cond2 = hc;
    }
    return cond2;
}
#endif

#if defined(WW_FN_wwAirTerrH_V2) || defined(WW_FN_ALL)
float wwAirTerrH(float3 p)
{
    float2 uv = float2(dot(volAirTerrU.xyz, p) + volAirTerrU.w, dot(volAirTerrV.xyz, p) + volAirTerrV.w);
    float2 uf = float2(dot(volAirTerrFU.xyz, p) + volAirTerrFU.w, dot(volAirTerrFV.xyz, p) + volAirTerrFV.w);
    float hc = volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uv, 0.0f), 0.0f).x;
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float cond2;
    if (wf > 0.0f)
    {
        cond2 = lerp(hc, volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        cond2 = hc;
    }
    return cond2;
}
#endif

#if defined(WW_FN_wwAirGroundAt) || defined(WW_FN_ALL)
float4 wwAirGroundAt(float3 p)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    float3 cond2 = p;
    return float4((volAirGround.x + wwAirTerrH(cond2)) - volAirTerrRef, volAirGround.yzw);
}
#endif

#if defined(WW_FN_wwAirGroundRay) || defined(WW_FN_ALL)
float4 wwAirGroundRay(float3 p0, float3 dirV, float len, float s)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    // view units per fog unit
    float k = 1.0f / max(fogDistScale, 9.9999999600419720025001879548654e-13f);
    float2 u0 = float2(dot(volAirTerrU.xyz, p0) + volAirTerrU.w, dot(volAirTerrV.xyz, p0) + volAirTerrV.w);
    float2 du = float2(dot(volAirTerrU.xyz, dirV), dot(volAirTerrV.xyz, dirV)) * k;
    float cond2;
    if (du.x > 0.0f)
    {
        cond2 = (1.0f - u0.x) / du.x;
    }
    else
    {
        float cond;
        if (du.x < 0.0f)
        {
            cond = (-u0.x) / du.x;
        }
        else
        {
            cond = 1000000015047466219876688855040.0f;
        }
        cond2 = cond;
    }
    float clampTPick;
    if (du.y > 0.0f)
    {
        clampTPick = (1.0f - u0.y) / du.y;
    }
    else
    {
        float cond4;
        if (du.y < 0.0f)
        {
            cond4 = (-u0.y) / du.y;
        }
        else
        {
            cond4 = 1000000015047466219876688855040.0f;
        }
        clampTPick = cond4;
    }
    float2 ex = float2(cond2, clampTPick);
    float L = clamp(min(min(ex.x, ex.y), min(len, 10000000.0f)), 0.0f, 10000000.0f);
    float cond3;
    if (volAirGround.y > 0.0f)
    {
        cond3 = 1.0f / volAirGround.y;
    }
    else
    {
        cond3 = 1000000000.0f;
    }
    float H = cond3;
    // the weight's live span
    float Lt = min(L, (40.0f * H) / max(abs(s), 9.999999717180685365747194737196e-10f));
    float escapePick;
    if (s < 0.0f)
    {
        escapePick = L - Lt;
    }
    else
    {
        escapePick = 0.0f;
    }
    float xs = escapePick;
    float b = (s * Lt) / H;
    float bp = b / 3.0f;
    float E = 1.0f - exp(-bp);
    float m = -1000000015047466219876688855040.0f;
    float t[6];
    float IPick;
    float a[6];
    for (int q = 0; q < 6; q++)
    {
        float f = (float(q) + 0.5f) / 6.0f;
        if (abs(bp) < 0.001000000047497451305389404296875f)
        {
            IPick = f;
        }
        else
        {
            IPick = (-log(1.0f - (f * E))) / bp;
        }
        t[q] = IPick;
        float3 wwAirRhoLawRet = p0 + (dirV * ((xs + (t[q] * Lt)) * k));
        a[q] = (wwAirTerrH(wwAirRhoLawRet) / H) - ((b - bp) * t[q]);
        m = max(m, a[q]);
    }
    float lf[2];
    float clampTPart4;
    float clampTPart3;
    for (int i = 0; i < 2; i++)
    {
        float c = (i == 0) ? b : bp;
        if (abs(c) < 0.001000000047497451305389404296875f)
        {
            clampTPart4 = (-0.5f) * c;
        }
        else
        {
            if (c > 0.0f)
            {
                clampTPart3 = log((1.0f - exp(-c)) / c);
            }
            else
            {
                clampTPart3 = (-c) + log((1.0f - exp(c)) / (-c));
            }
            clampTPart4 = clampTPart3;
        }
        lf[i] = clampTPart4;
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
    float cond2 = lo;
    float cond = hi;
    float4 clampTPick = volAirMed;
    float4 cond4 = volAirLayer;
    float4 cond3 = ground;
    float escapePick = wwAirColLaw(cond2, cond, clampTPick, cond4, cond3);
    return escapePick;
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
    // the fade's top: no fog above it
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
    // every sub-segment's height range (km)
    float dh = dq.z * w;
    bool level = abs(dh) <= 9.9999997473787516355514526367188e-05f;
    float cond2;
    if (level)
    {
        cond2 = 0.0f;
    }
    else
    {
        float cond = qA.z + (dq.z * t0);
        cond2 = wwAirRainP(cond);
    }
    // the profile's integral at the sub-segment's start
    float Pa = cond2;
    float sum = 0.0f;
    float p;
    float clampTPick;
    for (int k = 0; k < n; k++)
    {
        float3 qm = qA + (dq * (t0 + ((float(k) + 0.5f) * w)));
        float2 e = abs(qm.xy - 0.5f.xx) * 2.0f;
        float c = lerp(volAirRainMap.SampleLevel(_volAirRainMap_sampler, qm.xy, 0.0f).x, volAirRainFar.x, smoothstep(1.0f - volAirRainFar.y, 1.0f, max(e.x, e.y)));
        if (level)
        {
            if (qm.z >= 0.0f)
            {
                clampTPick = 1.0f + (volAirRain.y * exp((-qm.z) * volAirRain.z));
            }
            else
            {
                clampTPick = (1.0f + volAirRain.y) * max(1.0f + (qm.z * volAirRain.w), 0.0f);
            }
            p = clampTPick;
        }
        else
        {
            float cond4 = qA.z + (dq.z * (t0 + (float(k + 1) * w)));
            float Pb = wwAirRainP(cond4);
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
        bool cond2 = volAirGround.z > 0.0f;
        bool wwh6 = last.w < 0.99989998340606689453125f;
        bool cond;
        if (cond2)
        {
            cond = wwh6;
        }
        else
        {
            cond = cond2;
        }
        float3 clampTPick;
        if (cond)
        {
            clampTPick = max(last.xyz, 0.0f.xxx) / (1.0f - last.w).xxx;
        }
        else
        {
            clampTPick = 0.0f.xxx;
        }
        return clampTPick;
    }
    float3 dL = max(last.xyz - prev.xyz, 0.0f.xxx);
    if ((-log(max(last.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f)
    {
        return dL / max(prev.w, 9.9999999747524270787835121154785e-07f).xxx;
    }
    float m = 1.0f - last.w;
    float3 cond4;
    if (m > 1.0000000116860974230803549289703e-07f)
    {
        cond4 = max(last.xyz, 0.0f.xxx) / m.xxx;
    }
    else
    {
        cond4 = 0.0f.xxx;
    }
    float3 Jc = cond4;
    float3 cond3;
    if (dT > 1.0000000116860974230803549289703e-07f)
    {
        cond3 = dL / dT.xxx;
    }
    else
    {
        cond3 = 0.0f.xxx;
    }
    return lerp(Jc, cond3, clamp(((dT * 64.0f) / max(last.w, 9.9999999747524270787835121154785e-07f)) - 1.0f, 0.0f, 1.0f).xxx);
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
    float cond2 = T;
    return clamp(n * (1.0f + (log(1.0f - wwAirTailW(cond2)) / lnSpan)), 1.0f, n - 1.0f);
}
#endif

#if defined(WW_FN_wwAirThinJ) || defined(WW_FN_ALL)
float3 wwAirThinJ(float3 J, float4 v, float z0, float far)
{
    float zc = fogView.w;
    bool cond2 = (fogRed & 8) == 0;
    bool wwh7 = (-log(max(v.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f;
    bool cond;
    if (cond2)
    {
        cond = wwh7;
    }
    else
    {
        cond = cond2;
    }
    bool thin = cond;
    bool clampTPick = !thin;
    bool cond4;
    if (clampTPick)
    {
        bool cond3 = (volAirFlags & 32768) == 0;
        bool wwh8 = volAirGround.z <= 0.0f;
        bool escapePick;
        if (!cond3)
        {
            escapePick = wwh8;
        }
        else
        {
            escapePick = cond3;
        }
        bool wwh9 = min(zc, z0) < volAirMed.w;
        bool IPick;
        if (!escapePick)
        {
            IPick = wwh9;
        }
        else
        {
            IPick = escapePick;
        }
        cond4 = IPick;
    }
    else
    {
        cond4 = clampTPick;
    }
    if (cond4)
    {
        return J;
    }
    float wwAirRhoLawRet;
    if (abs(z0 - zc) < 0.5f)
    {
        wwAirRhoLawRet = zc + 1.0f;
    }
    else
    {
        wwAirRhoLawRet = z0;
    }
    // the law's depth of the whole reach (a level ray: the density at the camera's height), one call for both rules
    float z1 = wwAirRhoLawRet;
    float k = (volAirMed.x * far) / abs(z1 - zc);
    float clampTPart4 = min(zc, z1);
    float clampTPart3 = max(zc, z1);
    float4 clampTPart2 = volAirGround;
    float od = k * wwAirCol(clampTPart4, clampTPart3, clampTPart2);
    if (thin)
    {
        // MODULES1 R7-4: a thin column (the same test as wwAirTailJ's; J = its far half's dL over the T at the half's
        // start). od, b = the law's depth of the whole reach and of its far half; the law is scaled so its whole-reach
        // 1 - T lies within one 16F step of the stored one (zones, rain: the stored depth rules where they differ by
        // more)
        float zh = 0.5f * (zc + z1);
        float clampTPart = min(zh, z1);
        float cPart12 = max(zh, z1);
        float4 cPart11 = volAirGround;
        float b = k * wwAirCol(clampTPart, cPart12, cPart11);
        float xa = 1.0f - exp(-od);
        float m = 1.0f - v.w;
        b *= (clamp(xa, m - 0.00048828125f, m + 0.00048828125f) / max(xa, 9.999999717180685365747194737196e-10f));
        float3 cPart10;
        if (b > 1.0000000116860974230803549289703e-07f)
        {
            cPart10 = J / (b * (1.0f - (0.5f * b))).xxx;
        }
        else
        {
            cPart10 = 0.0f.xxx;
        }
        return cPart10;
    }
    float3 cPart9;
    if ((od > 1.0000000116860974230803549289703e-07f) && (od < 0.0199999995529651641845703125f))
    {
        cPart9 = max(v.xyz, 0.0f.xxx) / (od * (1.0f - (0.5f * od))).xxx;
    }
    else
    {
        cPart9 = J;
    }
    return cPart9;
}
#endif

#if defined(WW_FN_wwGridTailJ) || defined(WW_FN_ALL)
float3 wwGridTailJ(float2 uv, float4 a, float4 b, float4 whole)
{
    float nN = volPackDims.w;
    bool red = (fogRed & 8) != 0;
    float cond2 = whole.w;
    float db = (volFar.x * exp(volFar.y)) * (1.0f - wwAirTailW(cond2));
    bool wwh10 = db > volFar.x;
    bool cond;
    if (!red)
    {
        cond = wwh10;
    }
    else
    {
        cond = red;
    }
    bool inFar = cond;
    float clampTPick;
    if (red)
    {
        clampTPick = volFar.z - 1.0f;
    }
    else
    {
        float cond4;
        if (inFar)
        {
            cond4 = min((log(db / volFar.x) / volFar.y) * volFar.z, volFar.z - 1.0f);
        }
        else
        {
            cond4 = (log(db / volSlices.x) / volSlices.y) * nN;
        }
        clampTPick = cond4;
    }
    float f = clampTPick;
    float2 cond3;
    if (inFar)
    {
        cond3 = volPack.zw;
    }
    else
    {
        cond3 = volPack.xy;
    }
    float escapePick;
    if (inFar)
    {
        escapePick = volFar.z;
    }
    else
    {
        escapePick = nN;
    }
    float2 IPick = uv;
    float2 wwAirRhoLawRet = cond3;
    float clampTPart4 = inFar ? nN : 0.0f;
    float clampTPart3 = escapePick;
    float clampTPart2 = f;
    float4 p = wwGridAt(IPick, wwAirRhoLawRet, clampTPart4, clampTPart3, clampTPart2);
    if (red)
    {
        float4 clampTPart = b;
        float4 cPart12 = p;
        return wwAirTailJ(clampTPart, cPart12);
    }
    float4 cPart11;
    if (inFar)
    {
        cPart11 = float4(a.xyz + (p.xyz * a.w), a.w * p.w);
    }
    else
    {
        cPart11 = p;
    }
    float4 cPart10 = whole;
    float4 cPart9 = cPart11;
    return wwAirTailJ(cPart10, cPart9);
}
#endif

#if defined(WW_FN_wwAirRecordColour) || defined(WW_FN_ALL)
float3 wwAirRecordColour(float d, float z, float3 dirView, inout bool ok)
{
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fc = cond4;
    float escapePick;
    if (fogSun.w > 0.0f)
    {
        escapePick = pow(max(dot(dirView, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    }
    else
    {
        escapePick = 0.0f;
    }
    float sunW = escapePick;
    float3 c = max(lerp(fc, fogSunColour.xyz, sunW.xxx), 0.0f.xxx);
    float lc = dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    ok = lc > 9.9999999392252902907785028219223e-09f;
    float3 IPick;
    if (ok)
    {
        IPick = c / lc.xxx;
    }
    else
    {
        IPick = 1.0f.xxx;
    }
    return IPick;
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
    float2 cond2 = uv;
    float2 cond = volPack.xy;
    float clampTPick = 0.0f;
    float cond4 = nN;
    float cond3 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(cond2, cond, clampTPick, cond4, cond3);
    if (d <= volFar.x)
    {
        return v;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 escapePick = uv;
    float2 IPick = volPack.zw;
    float wwAirRhoLawRet = nN;
    float clampTPart4 = volFar.z;
    float clampTPart3 = fF;
    float4 w = wwGridAt(escapePick, IPick, wwAirRhoLawRet, clampTPart4, clampTPart3);
    float4 o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool clampTPart2 = fF <= volFar.z;
    bool wwh11 = (volAirFlags & 2) == 0;
    bool clampTPart;
    if (!clampTPart2)
    {
        clampTPart = wwh11;
    }
    else
    {
        clampTPart = clampTPart2;
    }
    bool wwh12 = volAirMed.x <= 0.0f;
    bool cPart12;
    if (!clampTPart)
    {
        cPart12 = wwh12;
    }
    else
    {
        cPart12 = clampTPart;
    }
    if (cPart12)
    {
        return o;
    }
    // the closed-form tail past the far grid (wwVolAirAt's, from the far grid's last slice)
    float far = volFar.x * exp(volFar.y);
    float2 cPart11 = uv;
    float4 cPart10 = v;
    float4 cPart9 = w;
    float4 cond6 = o;
    float3 J = wwGridTailJ(cPart11, cPart10, cPart9, cond6);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cond5 = J;
    float4 cPart8 = o;
    float cPart7 = z0;
    float cPart6 = far;
    // MODULES1 10c
    J = wwAirThinJ(cond5, cPart8, cPart7, cPart6);
    float len = d - far;
    float dz = z1 - z0;
    float3 cPart5 = posView * (far / d);
    float3 cPart4 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart3 = len;
    float cPart2 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    // FOGTERRAIN3: along the tail
    float4 gT = wwAirGroundRay(cPart5, cPart4, cPart3, cPart2);
    float cPart;
    if (abs(dz) < 0.5f)
    {
        float cPart18 = z0;
        float cPart17 = z0 + 1.0f;
        float4 cPart16 = gT;
        cPart = (volAirMed.x * len) * wwAirCol(cPart18, cPart17, cPart16);
    }
    else
    {
        float cPart15 = min(z0, z1);
        float cPart14 = max(z0, z1);
        float4 cPart13 = gT;
        cPart = ((volAirMed.x * len) * wwAirCol(cPart15, cPart14, cPart13)) / abs(dz);
    }
    float tau = cPart;
    float3 TtPart2 = posView * (far / d);
    float3 TtPart = posView;
    // lane RAINFOG2: + the rain fog
    float Tt = exp(-(tau + wwAirRainTau(TtPart2, TtPart)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float wwAirRecordColourPart6 = far;
        float wwAirRecordColourPart5 = z0;
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok0 = wwAirRecordColourPick2;
        float3 c0 = wwAirRecordColourRet2;
        float wwAirRecordColourPart3 = 0.5f * (far + d);
        float wwAirRecordColourPart2 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart = dirV;
        bool wwAirRecordColourPick;
        float3 wwAirRecordColourRet = wwAirRecordColour(wwAirRecordColourPart3, wwAirRecordColourPart2, wwAirRecordColourPart, wwAirRecordColourPick);
        bool ok1 = wwAirRecordColourPick;
        float3 c1 = wwAirRecordColourRet;
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
    float cond2 = v.w;
    float cond = volSlices.y;
    float clampTPick = volSlices.z;
    float3 cond4 = float3(uv, (wwAirTailF(cond2, cond, clampTPick) - 0.5f) / volSlices.z);
    bool cond3 = true;
    float4 escapePick = v;
    float4 IPick = wwVolTex(cond4, cond3);
    return wwAirTailJ(escapePick, IPick);
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
    float2 cond2 = uv;
    float2 cond = volPack.xy;
    float clampTPick = 0.0f;
    float cond4 = nN;
    float cond3 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(cond2, cond, clampTPick, cond4, cond3);
    if (d <= volFar.x)
    {
        o = v;
        return false;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 escapePick = uv;
    float2 IPick = volPack.zw;
    float wwAirRhoLawRet = nN;
    float clampTPart4 = volFar.z;
    float clampTPart3 = fF;
    float4 w = wwGridAt(escapePick, IPick, wwAirRhoLawRet, clampTPart4, clampTPart3);
    o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool clampTPart2 = fF <= volFar.z;
    bool wwh13 = (volAirFlags & 2) == 0;
    bool clampTPart;
    if (!clampTPart2)
    {
        clampTPart = wwh13;
    }
    else
    {
        clampTPart = clampTPart2;
    }
    bool wwh14 = volAirMed.x <= 0.0f;
    bool cPart12;
    if (!clampTPart)
    {
        cPart12 = wwh14;
    }
    else
    {
        cPart12 = clampTPart;
    }
    if (cPart12)
    {
        return false;
    }
    far = volFar.x * exp(volFar.y);
    float2 cPart11 = uv;
    float4 cPart10 = v;
    float4 cPart9 = w;
    float4 cond6 = o;
    J = wwGridTailJ(cPart11, cPart10, cPart9, cond6);
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
        float3 cond2 = float3(uv, 0.5f / volSlices.z);
        bool cond = true;
        v = wwVolTex(cond2, cond);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 clampTPick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool cond4 = f > volSlices.z;
        v = wwVolTex(clampTPick, cond4);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool cond3 = f <= volSlices.z;
    bool wwh15 = (volAirFlags & 2) == 0;
    bool escapePick;
    if (!cond3)
    {
        escapePick = wwh15;
    }
    else
    {
        escapePick = cond3;
    }
    bool wwh16 = volAirMed.x <= 0.0f;
    bool IPick;
    if (!escapePick)
    {
        IPick = wwh16;
    }
    else
    {
        IPick = escapePick;
    }
    if (IPick)
    {
        return false;
    }
    far = volSlices.x * exp(volSlices.y);
    float2 wwAirRhoLawRet = uv;
    float4 clampTPart4 = v;
    J = wwAirTailNearJ(wwAirRhoLawRet, clampTPart4);
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
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh17 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh17;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh18 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh18;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float cPart15 = far;
        float cPart14 = z0;
        float3 cPart13 = dirV;
        bool TtPart2;
        float3 TtPart = wwAirRecordColour(cPart15, cPart14, cPart13, TtPart2);
        bool ok0 = TtPart2;
        float3 c0 = TtPart;
        float wwAirRecordColourPart6 = 0.5f * (far + d);
        float wwAirRecordColourPart5 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok1 = wwAirRecordColourPick2;
        float3 c1 = wwAirRecordColourRet2;
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
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh19 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh19;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh20 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh20;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    float3 cPart15 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart14 = far;
    float cPart13 = d;
    float TtPart2 = tau;
    J *= wwAirTailShadow(cPart15, cPart14, cPart13, TtPart2);
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float TtPart = far;
        float wwAirRecordColourPart6 = z0;
        float3 wwAirRecordColourPart5 = dirV;
        bool wwAirRecordColourPart4;
        float3 wwAirRecordColourPick2 = wwAirRecordColour(TtPart, wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4);
        bool ok0 = wwAirRecordColourPart4;
        float3 c0 = wwAirRecordColourPick2;
        float wwAirRecordColourRet2 = 0.5f * (far + d);
        float wwAirRecordColourPart3 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart2 = dirV;
        bool wwAirRecordColourPart;
        float3 wwAirRecordColourPick = wwAirRecordColour(wwAirRecordColourRet2, wwAirRecordColourPart3, wwAirRecordColourPart2, wwAirRecordColourPart);
        bool ok1 = wwAirRecordColourPart;
        float3 c1 = wwAirRecordColourPick;
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
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh21 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh21;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh22 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh22;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float cPart15 = far;
        float cPart14 = z0;
        float3 cPart13 = dirV;
        bool TtPart2;
        float3 TtPart = wwAirRecordColour(cPart15, cPart14, cPart13, TtPart2);
        bool ok0 = TtPart2;
        float3 c0 = TtPart;
        float wwAirRecordColourPart6 = 0.5f * (far + d);
        float wwAirRecordColourPart5 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok1 = wwAirRecordColourPick2;
        float3 c1 = wwAirRecordColourRet2;
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
    float3 cond2 = posView;
    float2 cond = uv;
    return wwVolAirAtUv(cond2, cond);
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
    float3 cond2 = posView;
    float2 cond = uv;
    return wwVolAirAtUv(cond2, cond);
}
#endif

#if defined(WW_FN_wwAirDriftAt) || defined(WW_FN_ALL)
float wwAirDriftAt(float3 p)
{
    float f = 1.0f - clamp(((length(p) * fogDistScale) - volAirDrift.y) * volAirDrift.z, 0.0f, 1.0f);
    bool wwh0 = (volAirDrift.x <= 0.0f);
    bool wwh1 = (f <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
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
    if (or(wwh2, wwh3))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
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
    // a loop on a uniform count: one copy of the noise reads (three unrolled copies broke the decal program's limit)
    int n = ((volAirDriftRed & 1) != 0) ? 1 : 3;
    float f0 = (n == 1) ? 0.5f : 0.16666667163372039794921875f;
    float k = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float3 cond2 = p + (s * (f0 + (float(i) / 3.0f)));
        k += wwAirDriftAt(cond2);
    }
    return k / float(n);
}
#endif

#if defined(WW_FN_wwVolAirSunTD) || defined(WW_FN_ALL)
float2 wwVolAirSunTD(float3 posView)
{
    bool cond2 = (!fogOn) || (!volOn);
    bool wwh23 = volSlices.w < 0.5f;
    bool cond;
    if (!cond2)
    {
        cond = wwh23;
    }
    else
    {
        cond = cond2;
    }
    bool wwh24 = fogSun.w <= 0.0f;
    bool clampTPick;
    if (!cond)
    {
        clampTPick = wwh24;
    }
    else
    {
        clampTPick = cond;
    }
    bool wwh25 = volAirMed.x <= 0.0f;
    bool cond4;
    if (!clampTPick)
    {
        cond4 = wwh25;
    }
    else
    {
        cond4 = clampTPick;
    }
    bool wwh26 = (volAirFlags & 1) == 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = wwh26;
    }
    else
    {
        cond3 = cond4;
    }
    if (cond3)
    {
        return float2(1.0f, 0.0f);
    }
    float lo = dot(fogView.xyz, posView) + fogView.w;
    float4 escapePick = volAirMed;
    float4 IPick = volAirGround;
    // G2: the climb runs through the top's falloff
    // G2
    float end = wwAirEnd(escapePick, IPick);
    if (lo >= end)
    {
        return float2(1.0f, 0.0f);
    }
    float mu = max(dot(fogView.xyz, fogSun.xyz) / max(length(fogView.xyz), 9.9999999600419720025001879548654e-13f), 0.0199999995529651641845703125f);
    float3 wwAirRhoLawRet = posView;
    float4 g = wwAirGroundAt(wwAirRhoLawRet);
    float clampTPart4;
    if ((volAirFlags & 128) != 0)
    {
        float clampTPart3;
        if (volAirLayer.w > 0.0f)
        {
            float clampTPart2 = lo;
            float clampTPart = volAirMed.w;
            float cPart12 = g.x;
            float cPart11 = g.y;
            clampTPart3 = volAirLayer.w * wwAirColOne(clampTPart2, clampTPart, cPart12, cPart11);
        }
        else
        {
            clampTPart3 = 0.0f;
        }
        clampTPart4 = clampTPart3;
    }
    else
    {
        float cPart10 = lo;
        float cPart9 = end;
        float4 cond6 = g;
        clampTPart4 = wwAirCol(cPart10, cPart9, cond6);
    }
    // lane FIX1101A (atmo, the fog-sun fault): flag 128 (shipped) = only the derived ground layer dims the sun. The far and
    // near bands ARE the records' fog, and the weather's Sunlight colour is already the sun that weather gives the ground;
    // their column to the scene's top (its tallest shape) took Foggy noon at c3 to Ts 0.026 (no shadows, no shafts).
    float c = clampTPart4;
    float3 cond5 = posView;
    float cPart8 = lo;
    float cPart7 = mu;
    float cPart6 = volAirMed.w;
    float tau = (volAirMed.x * c) * wwAirDriftSunK(cond5, cPart8, cPart7, cPart6);
    float Td = exp((-tau) / mu);
    // MODULES1 G3 (bungo 11-09 items 6, 10e: the lit fog far brighter than the sunlit land beside it): what the column
    // takes out of the direct beam is mostly scattered forward and reaches the surface as soft light. The conservative
    // delta-Eddington total transmission (Meador & Weaver 1980 eq. 24) with a low-sun divisor fitted to a Monte Carlo
    // slab (notes/modules1/tools/g3_sundif_mc.py: 80 cases, worst 0.030), minus the direct beam; y = that share x mu,
    // the irradiance on a level surface per unit of the sun. volAirGround.w = 1 + the air's phase g (0: the red
    // WW_CELL_AIR_SUNDIF_RED=1, the direct beam alone)
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
    float3 cond2 = posView;
    return wwVolAirSunTD(cond2).x;
}
#endif

#if defined(WW_FN_wwFogBase) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fogCol = cond4;
    float alpha = cond3;
    if (apOn)
    {
        float escapePick = tAp;
        float IPick = 1.0f - alpha;
        bool wwAirRhoLawRet = atApDouble > 0.5f;
        alpha = 1.0f - wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float clampTPart4;
    if (fogUnit > 0.0f)
    {
        clampTPart4 = gray / fogUnit;
    }
    else
    {
        clampTPart4 = gray;
    }
    float grayW = clampTPart4;
    float3 clampTPart3;
    if (alpha < fogK[2].w)
    {
        clampTPart3 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        clampTPart3 = sunlit;
    }
    float3 resolved = clampTPart3;
    return lerp(preFog, resolved, alpha.xxx);
}
#endif

#if defined(WW_FN_wwFogBase_V2) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fogCol = cond4;
    float alpha_1 = cond3;
    if (apOn)
    {
        float escapePick = tAp;
        float IPick = 1.0f - alpha_1;
        bool wwAirRhoLawRet = atApDouble > 0.5f;
        alpha_1 = 1.0f - wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float clampTPart4;
    if (fogUnit > 0.0f)
    {
        clampTPart4 = gray / fogUnit;
    }
    else
    {
        clampTPart4 = gray;
    }
    float grayW = clampTPart4;
    float3 clampTPart3;
    if (alpha_1 < fogK[2].w)
    {
        clampTPart3 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        clampTPart3 = sunlit;
    }
    float3 resolved = clampTPart3;
    return lerp(preFog, resolved, alpha_1.xxx);
}
#endif

#if defined(WW_FN_wwFog) || defined(WW_FN_ALL)
float3 wwFog(inout float3 preFog, float3 posView)
{
    bool apOn = wwApOn();
    float4 cond2;
    if (apOn)
    {
        float3 cond = posView;
        cond2 = wwAtmoApAt(cond);
    }
    else
    {
        cond2 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float4 ap = cond2;
    if (apOn)
    {
        preFog = (preFog * ap.w) + ap.xyz;
    }
    if (!fogOn)
    {
        return preFog;
    }
    bool wwh27 = volSlices.w > 0.5f;
    bool clampTPick;
    if (volOn)
    {
        clampTPick = wwh27;
    }
    else
    {
        clampTPick = volOn;
    }
    if (clampTPick)
    {
        float3 cond4 = posView;
        float4 cond3 = wwVolAirAt(cond4);
        float4 a = cond3;
        if (apOn)
        {
            bool dbl = atApDouble > 0.5f;
            float escapePick = ap.w;
            float IPick = a.w;
            bool wwAirRhoLawRet = dbl;
            float tr = wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
            float3 clampTPart4;
            if (dbl)
            {
                clampTPart4 = a.xyz;
            }
            else
            {
                float3 clampTPart3 = a.xyz;
                float clampTPart2 = a.w;
                float clampTPart = tr;
                clampTPart4 = wwApFogResidualL(clampTPart3, clampTPart2, clampTPart);
            }
            float3 lr = clampTPart4;
            // R7 night sky d: the two airs' light together (ap.rgb x tr + lr), their T ap.a x tr
            if (fogNight.w > 0.0f)
            {
                float3 cPart12 = lr;
                lr = max(wwAirNightHue(cPart12), (fogNight.xyz * (1.0f - (ap.w * tr))) - (ap.xyz * tr));
            }
            return (preFog * tr) + lr;
        }
        if (fogNight.w > 0.0f)
        {
            float3 cPart11 = a.xyz;
            float cPart10 = a.w;
            float3 cPart9 = max(wwAirNightHue(cPart11), fogNight.xyz * (1.0f - cPart10));
            a.x = cPart9.x;
            a.y = cPart9.y;
            a.z = cPart9.z;
        }
        float3 lit = (preFog * a.w) + a.xyz;
        if (volFogBlend == 1)
        {
            float3 cond6 = lit;
            float3 cond5 = posView;
            float cPart8 = 1.0f;
            bool cPart7 = false;
            return wwFogBase(cond6, cond5, cPart8, cPart7);
        }
        // red blendnear (gates only): weighted by the air's 1 - T, which has no start distance
        if (volFogBlend == 2)
        {
            float cPart6 = length(posView) * fogDistScale;
            float cPart5 = dot(fogView.xyz, posView) + fogView.w;
            float cPart4;
            float3 cPart3;
            float cPart2 = wwFogEval(cPart6, cPart5, cPart4, cPart3);
            float hb = cPart4;
            float3 fc = cPart3;
            return lerp(lit, fc, (1.0f - a.w).xxx);
        }
        return lit;
    }
    float3 cPart = preFog;
    float3 cPart18 = posView;
    float cPart17 = ap.w;
    bool cPart16 = apOn;
    return wwFogBase(cPart, cPart18, cPart17, cPart16);
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
    bool cond2;
    if (volOn)
    {
        cond2 = wwh28;
    }
    else
    {
        cond2 = volOn;
    }
    bool wwh29 = (volAirFlags & 16) != 0;
    bool cond;
    if (cond2)
    {
        cond = wwh29;
    }
    else
    {
        cond = cond2;
    }
    bool volFx = cond;
    bool clampTPick = !additive;
    bool cond4;
    if (clampTPick)
    {
        cond4 = volFx || apOn;
    }
    else
    {
        cond4 = clampTPick;
    }
    if (cond4)
    {
        float3 cond3 = lin;
        float3 escapePick = posView;
        float3 IPick = wwFog(cond3, escapePick);
        return max(IPick, 0.0f.xxx);
    }
    float wwAirRhoLawRet;
    if (apOn)
    {
        float3 clampTPart4 = posView;
        wwAirRhoLawRet = wwAtmoApAt(clampTPart4).w;
    }
    else
    {
        wwAirRhoLawRet = 1.0f;
    }
    float tAp = wwAirRhoLawRet;
    if (!fogOn)
    {
        return lin * tAp;
    }
    float clampTPart3 = length(posView) * fogDistScale;
    float clampTPart2 = dot(fogView.xyz, posView) + fogView.w;
    float clampTPart;
    float3 cPart12;
    float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
    float hb = clampTPart;
    float3 fc = cPart12;
    float f = cPart11;
    if (!volFx)
    {
        if (apOn)
        {
            float cPart10 = tAp;
            float cPart9 = 1.0f - f;
            bool cond6 = atApDouble > 0.5f;
            return (lin * tAp) * wwApFogResidualT(cPart10, cPart9, cond6);
        }
        float3 cond5;
        if (additive)
        {
            cond5 = lin * (1.0f - f);
        }
        else
        {
            cond5 = lerp(lin, fc, f.xxx);
        }
        return cond5;
    }
    float3 cPart8 = posView;
    float4 cPart7 = wwVolAirAt(cPart8);
    float tv = cPart7.w;
    if (apOn)
    {
        float cPart6 = tAp;
        float cPart5 = tv;
        bool cPart4 = atApDouble > 0.5f;
        return (lin * tAp) * wwApFogResidualT(cPart6, cPart5, cPart4);
    }
    lin *= tv;
    float3 cPart3;
    if (volFogBlend == 1)
    {
        cPart3 = lin * (1.0f - f);
    }
    else
    {
        cPart3 = lin;
    }
    return cPart3;
}
#endif

#if defined(WW_FN_wwFogProbe) || defined(WW_FN_ALL)
bool wwFogProbe(float3 posView, out float3 o)
{
    o = 0.0f.xxx;
    bool cond2 = fogOn && volOn;
    bool wwh30 = (volProbe == 1) || (volProbe == 2);
    bool cond;
    if (cond2)
    {
        cond = wwh30;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float3 clampTPick = posView;
        float4 cond4 = wwVolAirAt(clampTPick);
        float4 a = cond4;
        float3 cond3;
        if (volProbe == 1)
        {
            cond3 = a.xyz;
        }
        else
        {
            cond3 = a.w.xxx;
        }
        o = cond3;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 escapePick = posView;
        o = wwVolAirSunT(escapePick).xxx;
        return true;
    }
    bool IPick = !fogOn;
    bool wwh31 = fogProbe.z < 0.5f;
    bool wwAirRhoLawRet;
    if (!IPick)
    {
        wwAirRhoLawRet = wwh31;
    }
    else
    {
        wwAirRhoLawRet = IPick;
    }
    if (wwAirRhoLawRet)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart4;
        if (fogProbe.z < 9.5f)
        {
            clampTPart4 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            clampTPart4 = (z + 65536.0f) / 131072.0f;
        }
        float x = clampTPart4;
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
        float clampTPart3 = d_2;
        float clampTPart2 = z_2;
        float clampTPart;
        float3 cPart12;
        float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
        float hb = clampTPart;
        float3 fogCol = cPart12;
        float alpha_1 = cPart11;
        float3 cPart10;
        if (fogProbe.z < 6.5f)
        {
            cPart10 = float3(alpha_1, hb, 0.0f);
        }
        else
        {
            cPart10 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = cPart10;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float cPart9 = fogProbe.x;
    float cond6 = fogProbe.y;
    float cond5;
    float3 cPart8;
    float cPart7 = wwFogEval(cPart9, cond6, cond5, cPart8);
    float hb_1 = cond5;
    float3 fogCol_1 = cPart8;
    float alpha_2 = cPart7;
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
    bool cond2 = fogOn && volOn;
    bool wwh32 = (volProbe == 1) || (volProbe == 2);
    bool cond;
    if (cond2)
    {
        cond = wwh32;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float3 clampTPick = posView;
        float4 cond4 = wwVolAirAt(clampTPick);
        float4 a = cond4;
        float3 cond3;
        if (volProbe == 1)
        {
            cond3 = a.xyz;
        }
        else
        {
            cond3 = a.w.xxx;
        }
        o = cond3;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 escapePick = posView;
        o = wwVolAirSunT(escapePick).xxx;
        return true;
    }
    bool IPick = !fogOn;
    bool wwh33 = fogProbe.z < 0.5f;
    bool wwAirRhoLawRet;
    if (!IPick)
    {
        wwAirRhoLawRet = wwh33;
    }
    else
    {
        wwAirRhoLawRet = IPick;
    }
    if (wwAirRhoLawRet)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart4;
        if (fogProbe.z < 9.5f)
        {
            clampTPart4 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            clampTPart4 = (z + 65536.0f) / 131072.0f;
        }
        float x = clampTPart4;
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
        float clampTPart3 = d_2;
        float clampTPart2 = z_2;
        float clampTPart;
        float3 cPart12;
        float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
        float hb = clampTPart;
        float3 fogCol = cPart12;
        float alpha = cPart11;
        float3 cPart10;
        if (fogProbe.z < 6.5f)
        {
            cPart10 = float3(alpha, hb, 0.0f);
        }
        else
        {
            cPart10 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = cPart10;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float cPart9 = fogProbe.x;
    float cond6 = fogProbe.y;
    float cond5;
    float3 cPart8;
    float cPart7 = wwFogEval(cPart9, cond6, cond5, cPart8);
    float hb_1 = cond5;
    float3 fogCol_1 = cPart8;
    float alpha_1 = cPart7;
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
