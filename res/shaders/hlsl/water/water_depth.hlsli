// Shared HLSL for res/shaders/features/water/water_depth.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWwUnitMetres = 0.0142875;
#endif

#if defined(WW_FN_wwWaterBedDepth) || defined(WW_FN_ALL)
float wwWaterBedDepth(float3 P)
{
    if (!waterInfoOn)
    {
        return -1.0f;
    }
    float2 tc = ((P.xy - waterInfoGrid.xy) / waterInfoGrid.z.xx) - 0.5f.xx;
    float2 t0 = floor(tc);
    float2 f = tc - t0;
    if (waterDepthRed == 3)
    {
        t0 = floor(tc + 0.5f.xx);
        f = 0.0f.xx;
    }
    float h = 0.0f;
    bool wet = false;
    float d[4];
    float dPick;
    for (int k = 0; k < 4; k++)
    {
        float2 tk = t0 + float2(float(k & 1), float(k >> 1));
        float2 wPart = waterInfoGrid.xy + ((tk + 0.5f.xx) * waterInfoGrid.z);
        uint w = wwWaterInfoWord(wPart);
        uint id = w & 65535u;
        float code = float(w >> uint(24)) / 255.0f;
        if (id != 0u)
        {
            dPick = (code * code) * 8192.0f;
        }
        else
        {
            dPick = 0.0f;
        }
        d[k] = dPick;
        if ((id != 0u) && (!wet))
        {
            wet = true;
            h = waterInfoBodies.Load(int3(int2(int(id % 256u), int(id / 256u)), 0)).x;
        }
    }
    bool cond2 = !wet;
    bool cond;
    if (!cond2)
    {
        cond = abs(P.z - h) > 128.0f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return -1.0f;
    }
    if (waterDepthRed == 1)
    {
        return 0.0f;
    }
    float b = lerp(lerp(d[0], d[1], f.x), lerp(d[2], d[3], f.x), f.y);
    return max(b + (P.z - h), 0.0f);
}
#endif

#if defined(WW_FN_wwWaterAbsorb) || defined(WW_FN_ALL)
float3 wwWaterAbsorb(float units)
{
    float3 dPick;
    if (waterDepthRed == 2)
    {
        dPick = 1.0f.xxx;
    }
    else
    {
        dPick = exp(float3(-0.3400000035762786865234375f, -0.0564999990165233612060546875f, -0.0092200003564357757568359375f) * (units * 0.01428749971091747283935546875f));
    }
    return dPick;
}
#endif

#if defined(WW_FN_wwWaterBedAt) || defined(WW_FN_ALL)
WwWaterBed wwWaterBedAt(float3 P, float3 V, float sceneVert, float sceneAlong)
{
    float3 dPick = P;
    float bd = wwWaterBedDepth(dPick);
    bool wPart = bd >= 0.0f;
    bool cond2;
    if (wPart)
    {
        bool cond = sceneVert > 100000000.0f;
        bool condPick;
        if (!cond)
        {
            condPick = sceneVert > (bd + 256.0f);
        }
        else
        {
            condPick = cond;
        }
        cond2 = condPick;
    }
    else
    {
        cond2 = wPart;
    }
    WwWaterBed b;
    b.baked = cond2;
    b.known = b.baked || (sceneVert < 100000000.0f);
    b.vert = b.baked ? bd : sceneVert;
    float bPick;
    if (b.baked)
    {
        bPick = bd / max(-V.z, 0.001000000047497451305389404296875f);
    }
    else
    {
        bPick = sceneAlong;
    }
    b.along = bPick;
    b.bd = bd;
    return b;
}
#endif

#if defined(WW_FN_wwWaterBedBakeW) || defined(WW_FN_ALL)
float wwWaterBedBakeW(float bd, float sceneVert, bool soft)
{
    if (bd < 0.0f)
    {
        return 0.0f;
    }
    if (sceneVert > 100000000.0f)
    {
        return 1.0f;
    }
    float dPick;
    if (soft)
    {
        dPick = smoothstep(bd + 64.0f, bd + 768.0f, sceneVert);
    }
    else
    {
        dPick = float(sceneVert > (bd + 256.0f));
    }
    return dPick;
}
#endif

#if defined(WW_FN_wwWaterBedNearW) || defined(WW_FN_ALL)
float wwWaterBedNearW(float bd, float sceneVert, float dist, bool on)
{
    float dPick = bd;
    float wPart = sceneVert;
    bool cond2 = true;
    float far = wwWaterBedBakeW(dPick, wPart, cond2);
    bool wwh0 = ((!on) || (bd < 0.0f));
    bool wwh1 = (sceneVert > 100000000.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return far;
    }
    float hole = smoothstep(bd + 2048.0f, bd + 4096.0f, sceneVert);
    return lerp(hole, far, smoothstep(12288.0f, 16384.0f, dist));
}
#endif

#if defined(WW_FN_wwWaterDepthMix) || defined(WW_FN_ALL)
float3 wwWaterDepthMix(float3 c, float3 fogC, float a, float along)
{
    float3 T = (1.0f - a).xxx;
    if (along < 100000000.0f)
    {
        float dPick = along;
        T *= wwWaterAbsorb(dPick);
    }
    return lerp(fogC, c, T);
}
#endif

#if defined(WW_FN_wwWaterDepthFar) || defined(WW_FN_ALL)
float3 wwWaterDepthFar(float3 far, float3 body, float F, bool known)
{
    float3 dPick;
    if (known)
    {
        dPick = lerp(body, far, F.xxx);
    }
    else
    {
        dPick = far;
    }
    return dPick;
}
#endif
