// Shared HLSL for res/shaders/features/lights/cell_lights.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_hdr_fd_ps.hlsl, cell_hdr_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_reflrelight_ps.hlsl, cell_volair_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): cellRoomSolid, cellGiSeeFrom, cellGiWall, cellGiSeen, cellGiRoomBlend, cellGiRoomSample, cellGiFillAt, cellGiSeenFill, cellGiFillPair, cellGiStrandSearch, cellGiStrandAnswer, cellGiTrilinear, cellGiStrandPick, cellGiSampleFull, cellGiKeyPack, cellGiKeyNormal, cellGiMarchPrime, cellGiVolAt, cellGiVolume, cellAoDecalAt, cellGiFineAdd, cellGiFirst
#ifdef WW_FN_ALL
#ifndef CELL_TPL
#define CELL_TPL 8
#endif
#ifndef CELL_GI_ONE
#define CELL_GI_ONE 1
#endif
#ifndef CELL_GI_HALF1
#define CELL_GI_HALF1 1
#endif
#endif

#if defined(WW_FN_cellImageSpace) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    if (cellIsBloomOn)
    {
        x += cellIsBloom.Sample(_cellIsBloom_sampler, (gl_FragCoord.xy - cellIsBloomRect.xy) * cellIsBloomRect.zw).xyz;
    }
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart2 = x * 0.5f;
        float3 cPart = wwTonemapPhysical(cPart2);
        c = cPart;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond;
    if (cellIsLutOn)
    {
        cond = (cellIsRed & 1) == 0;
    }
    else
    {
        cond = cellIsLutOn;
    }
    if (cond)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellImageSpace_V2) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart2 = x * 0.5f;
        float3 cPart = wwTonemapPhysical(cPart2);
        c = cPart;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond;
    if (cellIsLutOn)
    {
        cond = (cellIsRed & 1) == 0;
    }
    else
    {
        cond = cellIsLutOn;
    }
    if (cond)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellImageSpace_V3) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    if (cellIsBloomOn)
    {
        x += (cellIsBloom.Sample(_cellIsBloom_sampler, (gl_FragCoord.xy - cellIsBloomRect.xy) * cellIsBloomRect.zw).xyz * cellBloomGain);
    }
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    if (cellIsFoBloomOn && (cellIsFoBloomEc > 0.0f))
    {
        float cPart2;
        if (cellIsFoBloomFogCap > 0.0f)
        {
            cPart2 = min(cellBloomGain, cellIsFoBloomFogCap);
        }
        else
        {
            cPart2 = cellBloomGain;
        }
        float kg = min(cellIsFoBloomEc * cPart2, 1.0f);
        x = (x * (1.0f - kg)) + (cellFoBloomHere * (2.0f * (kg / cellIsFoBloomEc)));
    }
    else
    {
        if (cellIsFoBloomOn)
        {
            x += (cellFoBloomHere * (2.0f * cellBloomGain));
        }
    }
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart = x * 0.5f;
        float3 cond = wwTonemapPhysical(cPart);
        c = cond;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond2;
    if (cellIsLutOn)
    {
        cond2 = (cellIsRed & 1) == 0;
    }
    else
    {
        cond2 = cellIsLutOn;
    }
    if (cond2)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellWorldPos) || defined(WW_FN_ALL)
float3 cellWorldPos(float3 posView)
{
    float4 p = float4(posView, 1.0f);
    return float3(dot(cellRow[0], p), dot(cellRow[1], p), dot(cellRow[2], p));
}
#endif

#if defined(WW_FN_cellWorldDir) || defined(WW_FN_ALL)
float3 cellWorldDir(float3 dirView)
{
    return normalize(float3(dot(cellRow[0].xyz, dirView), dot(cellRow[1].xyz, dirView), dot(cellRow[2].xyz, dirView)));
}
#endif

#if defined(WW_FN_cellAttVan) || defined(WW_FN_ALL)
float cellAttVan(float x, float3 c)
{
    float cPart2;
    if (c.z > 0.0f)
    {
        cPart2 = pow(x, c.z);
    }
    else
    {
        cPart2 = 1.0f;
    }
    float xe = cPart2;
    return pow(1.0f - clamp((c.y * xe) + c.x, 0.0f, 1.0f), 2.2000000476837158203125f);
}
#endif

#if defined(WW_FN_cellPhysRaw) || defined(WW_FN_ALL)
float cellPhysRaw(float x, float s)
{
    float x2 = x * x;
    float w = clamp(1.0f - (x2 * x2), 0.0f, 1.0f);
    float den = x2 + (s * s);
    float cPart2;
    if (den > 0.0f)
    {
        cPart2 = (w * w) / den;
    }
    else
    {
        cPart2 = 0.0f;
    }
    return cPart2;
}
#endif

#if defined(WW_FN_cellPhysFalloff) || defined(WW_FN_ALL)
float cellPhysFalloff(float van, float x, float r, float3 c)
{
    float a = clamp(cellLightPhys.y, 0.0f, 1.0f);
    bool cPart2 = !(c.z > 0.0f);
    bool cPart;
    if (!cPart2)
    {
        cPart = !(clamp(c.y + c.x, 0.0f, 1.0f) > clamp(c.x, 0.0f, 1.0f));
    }
    else
    {
        cPart = cPart2;
    }
    bool cond;
    if (!cPart)
    {
        float cond2 = 1.0f;
        float3 condPart = c;
        cond = cellAttVan(cond2, condPart) >= 0.0039215688593685626983642578125f;
    }
    else
    {
        cond = cPart;
    }
    if (cond)
    {
        // an authored curve that is no falloff keeps the game's
        return van;
    }
    float vAPart2 = a;
    float3 vAPart = c;
    float vA = cellAttVan(vAPart2, vAPart);
    float b = 1.0f - (a * a);
    float test;
    if (b > 0.0f)
    {
        test = pow(b, 1.10000002384185791015625f);
    }
    else
    {
        test = 0.0f;
    }
    if (vA > test)
    {
        return van;
    }
    // one loop for both: the program's size (GIDARK1's link limit)
    int red = int(cellLightPhysB.z + 0.5f);
    if (cellLightPhys.x > 1.5f)
    {
        float x0Pick;
        if (r > 0.0f)
        {
            x0Pick = clamp((max(max(cellLightPhysB.x, 0.001000000047497451305389404296875f), max((0.100000001490116119384765625f * r) / 69.98999786376953125f, 0.20000000298023223876953125f)) * 69.98999786376953125f) / r, 9.9999997473787516355514526367188e-05f, 0.5f);
        }
        else
        {
            x0Pick = 0.5f;
        }
        float x0 = x0Pick;
        float den = 0.89999997615814208984375f - (0.666666686534881591796875f * x0);
        float t = clamp((x - 0.800000011920928955078125f) / 0.20000000298023223876953125f, 0.0f, 1.0f);
        float wPick;
        if ((red & 2) != 0)
        {
            wPick = 1.0f;
        }
        else
        {
            wPick = 1.0f - ((t * t) * (3.0f - (2.0f * t)));
        }
        float w = wPick;
        float m = max(x0, x);
        float cellPhysFalloffRet2;
        if (den > 0.0f)
        {
            cellPhysFalloffRet2 = ((cellLightPhys.w / den) * w) / (m * m);
        }
        else
        {
            cellPhysFalloffRet2 = 0.0f;
        }
        return cellPhysFalloffRet2;
    }
    float s = cellLightPhys.z;
    float rawPart2 = a;
    float rawPart = s;
    float raw = cellPhysRaw(rawPart2, rawPart);
    float kPick;
    if ((red & 1) != 0)
    {
        kPick = 1.0f;
    }
    else
    {
        float kPick2;
        if (raw > 0.0f)
        {
            kPick2 = vA / raw;
        }
        else
        {
            kPick2 = 0.0f;
        }
        kPick = kPick2;
    }
    float k = kPick;
    float cellPhysFalloffRet;
    if (k > 0.0f)
    {
        float cellPhysFalloffPart2 = x;
        float cellPhysFalloffPart = s;
        cellPhysFalloffRet = k * cellPhysRaw(cellPhysFalloffPart2, cellPhysFalloffPart);
    }
    else
    {
        cellPhysFalloffRet = van;
    }
    return cellPhysFalloffRet;
}
#endif

#if defined(WW_FN_cellRadial) || defined(WW_FN_ALL)
float cellRadial(float d, float r, float3 bse)
{
    float x = clamp(d / max(r, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
    float cPart2;
    if (bse.z > 0.0f)
    {
        cPart2 = pow(x, bse.z);
    }
    else
    {
        cPart2 = 1.0f;
    }
    float xe = cPart2;
    float k = 1.0f - clamp((bse.y * xe) + bse.x, 0.0f, 1.0f);
    if ((cellRed & 1) != 0)
    {
        return k;
    }
    float cPart;
    if (cellLightPhys.x > 0.5f)
    {
        float cond = pow(k, 2.2000000476837158203125f);
        float cond2 = x;
        float condPart = r;
        float3 vAPart2 = bse;
        cPart = cellPhysFalloff(cond, cond2, condPart, vAPart2);
    }
    else
    {
        cPart = pow(k, 2.2000000476837158203125f);
    }
    return cPart;
}
#endif

#if defined(WW_FN_cellShadowF) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellShadowOn;
    bool cPart;
    if (!cPart2)
    {
        cPart = t4.y < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool cond = t4.y > 1.5f;
    bool cond2;
    if (cond)
    {
        cond2 = t4.y < 2.5f;
    }
    else
    {
        cond2 = cond;
    }
    bool condPart;
    if (cond2)
    {
        condPart = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        condPart = cond2;
    }
    if (condPart)
    {
        return 0.0f;
    }
    if (t4.x < (-0.5f))
    {
        return 1.0f;
    }
    float3 r = P - t0.xyz;
    float3 rn = r + (N * ((1.5f * length(r)) * cellShadowTexel));
    float d = length(rn);
    float ref = (d - 1.0f) / max(t0.w, 0.001000000047497451305389404296875f);
    float3 a = rn / max(d, 0.001000000047497451305389404296875f).xxx;
    bool3 vAPart2 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(vAPart2.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, vAPart2.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, vAPart2.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmp(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float vAPart;
    if (cellShadowFadeOn)
    {
        vAPart = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        vAPart = s / 9.0f;
    }
    return vAPart;
}
#endif

#if defined(WW_FN_cellShadowF_V2) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellShadowOn;
    bool cPart;
    if (!cPart2)
    {
        cPart = t4.y < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool cond = t4.y > 1.5f;
    bool cond2;
    if (cond)
    {
        cond2 = t4.y < 2.5f;
    }
    else
    {
        cond2 = cond;
    }
    bool condPart;
    if (cond2)
    {
        condPart = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        condPart = cond2;
    }
    if (condPart)
    {
        return 0.0f;
    }
    if (t4.x < (-0.5f))
    {
        return 1.0f;
    }
    float3 r = P - t0.xyz;
    float3 rn = r + (N * ((1.5f * length(r)) * cellShadowTexel));
    float d = length(rn);
    float ref = (d - 1.0f) / max(t0.w, 0.001000000047497451305389404296875f);
    float3 a = rn / max(d, 0.001000000047497451305389404296875f).xxx;
    bool3 vAPart2 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(vAPart2.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, vAPart2.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, vAPart2.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmpLevelZero(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float vAPart;
    if (cellShadowFadeOn)
    {
        vAPart = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        vAPart = s / 9.0f;
    }
    return vAPart;
}
#endif

#if defined(WW_FN_cellContactLocal) || defined(WW_FN_ALL)
float cellContactLocal(int i, float3 P)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellCtOn;
    bool cPart;
    if (!cPart2)
    {
        bool cond;
        if (cellShadowOn)
        {
            cond = t4.y > 0.5f;
        }
        else
        {
            cond = cellShadowOn;
        }
        bool cond2;
        if (cond)
        {
            cond2 = t4.x > (-0.5f);
        }
        else
        {
            cond2 = cond;
        }
        cPart = cond2;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    // world to view: the inverse of cellRow's rotation (orthonormal) and translation
    float3 w = P - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
    float3 l = t0.xyz - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
    // columns = rows of view-to-world: R = its transpose
    float3x3 R = float3x3(float3(cellRow[0].xyz), float3(cellRow[1].xyz), float3(cellRow[2].xyz));
    float3 pv = mul(w, R);
    float3 lv = mul(l, R);
    float3 toL = lv - pv;
    float dist = length(toL);
    if (dist < 0.001000000047497451305389404296875f)
    {
        return 1.0f;
    }
    float3 dir = (toL / dist.xxx) * ((cellCtRed == 1) ? (-1.0f) : 1.0f);
    float z = max(-pv.z, 8.0f);
    float bias = 0.800000011920928955078125f + (z * 0.01200000010430812835693359375f);
    float thickMin = 0.60000002384185791015625f + (z * 0.008000000379979610443115234375f);
    float thickMax = max(20.0f, z * 0.100000001490116119384765625f);
    float reach = max(min(0.1500000059604644775390625f * t0.w, 12.0f), 4.0f);
    float march = min(dist - bias, reach);
    if (march <= 0.0f)
    {
        return 1.0f;
    }
    float3 S0 = pv + (dir * bias);
    float3 S1 = S0 + (dir * march);
    float2 a = ((cellCtProj.xy * S0.xy) + (cellCtProj.zw * S0.z)) / max(-S0.z, 9.9999997473787516355514526367188e-05f).xx;
    float2 b = ((cellCtProj.xy * S1.xy) + (cellCtProj.zw * S1.z)) / max(-S1.z, 9.9999997473787516355514526367188e-05f).xx;
    float px = length(((b - a) * 0.5f) * cellCtRect.zw);
    int steps = clamp(int(px), 2, 48);
    float jit = frac(52.98291778564453125f * frac(dot(gl_FragCoord.xy, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f))));
    for (int k = 1; k <= 48; k++)
    {
        if (k > steps)
        {
            break;
        }
        float t = (float(k) - jit) / float(steps);
        float3 S = S0 + (dir * (march * t));
        if (S.z >= (-1.0f))
        {
            break;
        }
        float2 ndc = ((cellCtProj.xy * S.xy) + (cellCtProj.zw * S.z)) / (-S.z).xx;
        float2 q = ((ndc * 0.5f) + 0.5f.xx) * cellCtRect.zw;
        bool condPart = any(bool2(q.x < 0.0f.xx.x, q.y < 0.0f.xx.y));
        bool vAPart2;
        if (!condPart)
        {
            vAPart2 = any(bool2(q.x >= cellCtRect.zw.x, q.y >= cellCtRect.zw.y));
        }
        else
        {
            vAPart2 = condPart;
        }
        if (vAPart2)
        {
            break;
        }
        float delta = (-S.z) - cellCtGbuf.Load(int3(int2(q), 0)).w;
        if ((delta > thickMin) && (delta < thickMax))
        {
            return 1.0f - (clamp(1.0f - ((march * t) / reach), 0.0f, 1.0f) * 0.5f);
        }
    }
    return 1.0f;
}
#endif

#if defined(WW_FN_cellShadowProbe) || defined(WW_FN_ALL)
float3 cellShadowProbe(float3 P, float3 N)
{
    float3 o = 0.0f.xxx;
    float3 cPart2 = P;
    int cPart;
    int cond = cellLightsBegin(cPart2, cPart);
    int nL = cPart;
    int bL = cond;
    for (int k = 0; k < nL; k++)
    {
        int cond2 = bL;
        int condPart = k;
        int i = cellLightAt(cond2, condPart);
        float slot = cellLights.Load((i * 8) + 4).x;
        bool wwh0 = (slot < (-0.5f));
        bool wwh1 = (slot > 2.5f);
        if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
        {
            continue;
        }
        float4 t0 = cellLights.Load(i * 8);
        float3 Lv = t0.xyz - P;
        float d = length(Lv);
        bool vAPart2 = d >= t0.w;
        bool vAPart;
        if (!vAPart2)
        {
            vAPart = dot(N, Lv / max(d, 0.001000000047497451305389404296875f).xxx) < 0.0500000007450580596923828125f;
        }
        else
        {
            vAPart = vAPart2;
        }
        if (vAPart)
        {
            continue;
        }
        int test = i;
        float3 x0Pick = P;
        float3 wPick = N;
        o[int(slot + 0.5f)] = (2.0f + (cellShadowF(test, x0Pick, wPick) * 253.0f)) / 255.0f;
    }
    return o;
}
#endif

#if defined(WW_FN_cellRoomUnpack) || defined(WW_FN_ALL)
int2 cellRoomUnpack(float v)
{
    int k = int(v + 0.5f);
    return int2((k / 4096) - 1, (k - ((k / 4096) * 4096)) - 1);
}
#endif

#if defined(WW_FN_cellRoomAt) || defined(WW_FN_ALL)
int2 cellRoomAt(float3 q)
{
    float3 c = floor((q - cellRoomsOrigin) / cellRoomsCell.xxx);
    bool cPart2 = any(bool3(c.x < 0.0f.xxx.x, c.y < 0.0f.xxx.y, c.z < 0.0f.xxx.z));
    bool cPart;
    if (!cPart2)
    {
        cPart = any(bool3(c.x >= cellRoomsDims.x, c.y >= cellRoomsDims.y, c.z >= cellRoomsDims.z));
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return int2(-1, -1);
    }
    float cond = cellRooms.Load(int4(int3(c), 0)).x;
    return cellRoomUnpack(cond);
}
#endif

#if defined(WW_FN_cellGiEdgeFade) || defined(WW_FN_ALL)
float cellGiEdgeFade(float2 g)
{
    if (cellGiPad <= 0.0f)
    {
        return 1.0f;
    }
    float2 e = min(g, cellGiDims.xy - g);
    return clamp(min(e.x, e.y) / cellGiPad, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellGiSample) || defined(WW_FN_ALL)
float4 cellGiSample(float3 P, float3 N)
{
    return 0.0f.xxxx;
}
#endif

#if defined(WW_FN_cellGiE0) || defined(WW_FN_ALL)
float3 cellGiE0(float3 P, float3 N)
{
    float3 cPart2 = P;
    float3 cPart = N;
    float4 s = cellGiSample(cPart2, cPart);
    // with the sky in the grid the sample keeps its valid share (E x share), the other
    // ( 1 - share ) being the weather's own ambient the program leaves in (cellGiSkyK)
    if (cellGiSky && false)
    {
        return max(s.xyz, 0.0f.xxx);
    }
    float3 cond;
    if (s.w > 0.00999999977648258209228515625f)
    {
        cond = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        cond = 0.0f.xxx;
    }
    return cond;
}
#endif

#if defined(WW_FN_cellGiE) || defined(WW_FN_ALL)
float3 cellGiE(float3 P, float3 N)
{
    float3 cPart2 = P;
    float3 cPart = N;
    float3 e = cellGiE0(cPart2, cPart);
    // lane PRTOFF1: the AO decals are the PRT relight's
    return e;
}
#endif

#if defined(WW_FN_cellGiSkyK) || defined(WW_FN_ALL)
float cellGiSkyK(float3 P, float3 N)
{
    bool cPart2;
    if (!true)
    {
        cPart2 = !(cellGiSky && false);
    }
    else
    {
        cPart2 = true;
    }
    if (or(cPart2, cellInterior))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 0.0f;
    }
    float3 cPart = P;
    float3 cond = N;
    return clamp(cellGiSample(cPart, cond).w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellSpecSky) || defined(WW_FN_ALL)
float cellSpecSky(float3 P, float3 N, float3 R, float rough)
{
    bool cPart2 = (cellSpecOcc == 0) || true;
    bool cPart;
    if (!cPart2)
    {
        cPart = !(cellGiSky && false);
    }
    else
    {
        cPart = cPart2;
    }
    if (or(cPart, cellInterior))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float r = clamp(rough, 0.0f, 1.0f);
    float3 cond;
    if (cellSpecOcc == 2)
    {
        cond = N;
    }
    else
    {
        cond = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    }
    float3 D_1 = cond;
    float v = 1.0f;
    float cond2;
    if (cellSpecOcc == 2)
    {
        cond2 = lerp(1.0f, v, r);
    }
    else
    {
        cond2 = v;
    }
    return cond2;
}
#endif

#if defined(WW_FN_cellAmbShare) || defined(WW_FN_ALL)
float cellAmbShare(float3 P, float3 N)
{
    if (!cellInterior)
    {
        float3 cPart2 = P;
        float3 cPart = N;
        return cellGiSkyK(cPart2, cPart);
    }
    bool wwh2 = (cellGiAmb != 1);
    if ((true || (!cellHasDalc)) || wwh2)
    {
        return 0.0f;
    }
    float3 cond = P;
    float3 cond2 = N;
    return clamp(cellGiSample(cond, cond2).w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellGobo) || defined(WW_FN_ALL)
float3 cellGobo(int i, float3 D_1, float3 axis, bool spot)
{
    if (!cellGoboOn)
    {
        return 1.0f.xxx;
    }
    int hf = -1;
    int h;
    float2 uv;
    if (spot)
    {
        float4 t5 = cellLights.Load((i * 8) + 5);
        if (t5.w < 0.5f)
        {
            return 1.0f.xxx;
        }
        float4 t6 = cellLights.Load((i * 8) + 6);
        h = int(t5.w + 0.5f) - 1;
        uv = (float2(dot(D_1, t6.xyz), dot(D_1, t5.xyz)) / ((2.0f * max(dot(D_1, axis), 0.001000000047497451305389404296875f)) * max(t6.w, 9.9999997473787516355514526367188e-05f)).xx) + 0.5f.xx;
    }
    else
    {
        if (cellGoboTbl <= 0)
        {
            return 1.0f.xxx;
        }
        int r = int(cellLights.Load(cellGoboTbl + (i / 4))[i % 4] + 0.5f) - 1;
        if (r < 0)
        {
            return 1.0f.xxx;
        }
        float4 a0 = cellLights.Load(r);
        h = int(a0.w + 0.5f) - 1;
        float3 cPart2;
        if ((cellGoboMap & 4) != 0)
        {
            cPart2 = D_1;
        }
        else
        {
            cPart2 = float3(dot(D_1, a0.xyz), dot(D_1, cellLights.Load(r + 1).xyz), dot(D_1, cellLights.Load(r + 2).xyz));
        }
        float3 l = cPart2;
        if ((cellGoboMap & 8) != 0)
        {
            float3 cPart;
            if ((cellGoboMap & 2) != 0)
            {
                cPart = l.yzx;
            }
            else
            {
                cPart = l;
            }
            float3 d = normalize(cPart);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 cond;
            if ((cellGoboMap & 2) != 0)
            {
                cond = d_1.yz;
            }
            else
            {
                cond = d_1.zy;
            }
            float2 e = cond;
            bool cond2 = hf == 1;
            bool condPart;
            if (cond2)
            {
                condPart = (cellGoboMap & 1) == 0;
            }
            else
            {
                condPart = cond2;
            }
            if (condPart)
            {
                e.x = -e.x;
            }
            float vAPart2;
            if (hf == 1)
            {
                vAPart2 = 1.0f - d_1.x;
            }
            else
            {
                vAPart2 = 1.0f + d_1.x;
            }
            uv = ((e / max(vAPart2, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 vAPart;
        if (cellGoboGam)
        {
            vAPart = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            vAPart = cellLights.Load(h + 1).xyz;
        }
        return vAPart;
    }
    if ((cellGoboRot & 4) != 0)
    {
        uv = uv.yx;
    }
    if ((cellGoboRot & 1) != 0)
    {
        uv.x = 1.0f - uv.x;
    }
    if ((cellGoboRot & 2) != 0)
    {
        uv.y = 1.0f - uv.y;
    }
    int2 sz = int2(cellLights.Load(h).xy + 0.5f.xx);
    int2 test;
    if (hf < 0)
    {
        test = sz;
    }
    else
    {
        test = int2(sz.x, sz.y / 2);
    }
    int2 pz = test;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 x0Pick;
    if (cellGoboGam)
    {
        x0Pick = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        x0Pick = lerp(c0, c1, f.y.xxx);
    }
    return x0Pick;
}
#endif

#if defined(WW_FN_cellGobo_V2) || defined(WW_FN_ALL)
float3 cellGobo(int i, float3 D, float3 axis, bool spot)
{
    if (!cellGoboOn)
    {
        return 1.0f.xxx;
    }
    int hf = -1;
    int h;
    float2 uv;
    if (spot)
    {
        float4 t5 = cellLights.Load((i * 8) + 5);
        if (t5.w < 0.5f)
        {
            return 1.0f.xxx;
        }
        float4 t6 = cellLights.Load((i * 8) + 6);
        h = int(t5.w + 0.5f) - 1;
        uv = (float2(dot(D, t6.xyz), dot(D, t5.xyz)) / ((2.0f * max(dot(D, axis), 0.001000000047497451305389404296875f)) * max(t6.w, 9.9999997473787516355514526367188e-05f)).xx) + 0.5f.xx;
    }
    else
    {
        if (cellGoboTbl <= 0)
        {
            return 1.0f.xxx;
        }
        int r = int(cellLights.Load(cellGoboTbl + (i / 4))[i % 4] + 0.5f) - 1;
        if (r < 0)
        {
            return 1.0f.xxx;
        }
        float4 a0 = cellLights.Load(r);
        h = int(a0.w + 0.5f) - 1;
        float3 cPart2;
        if ((cellGoboMap & 4) != 0)
        {
            cPart2 = D;
        }
        else
        {
            cPart2 = float3(dot(D, a0.xyz), dot(D, cellLights.Load(r + 1).xyz), dot(D, cellLights.Load(r + 2).xyz));
        }
        float3 l = cPart2;
        if ((cellGoboMap & 8) != 0)
        {
            float3 cPart;
            if ((cellGoboMap & 2) != 0)
            {
                cPart = l.yzx;
            }
            else
            {
                cPart = l;
            }
            float3 d = normalize(cPart);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 cond;
            if ((cellGoboMap & 2) != 0)
            {
                cond = d_1.yz;
            }
            else
            {
                cond = d_1.zy;
            }
            float2 e = cond;
            bool cond2 = hf == 1;
            bool condPart;
            if (cond2)
            {
                condPart = (cellGoboMap & 1) == 0;
            }
            else
            {
                condPart = cond2;
            }
            if (condPart)
            {
                e.x = -e.x;
            }
            float vAPart2;
            if (hf == 1)
            {
                vAPart2 = 1.0f - d_1.x;
            }
            else
            {
                vAPart2 = 1.0f + d_1.x;
            }
            uv = ((e / max(vAPart2, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 vAPart;
        if (cellGoboGam)
        {
            vAPart = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            vAPart = cellLights.Load(h + 1).xyz;
        }
        return vAPart;
    }
    if ((cellGoboRot & 4) != 0)
    {
        uv = uv.yx;
    }
    if ((cellGoboRot & 1) != 0)
    {
        uv.x = 1.0f - uv.x;
    }
    if ((cellGoboRot & 2) != 0)
    {
        uv.y = 1.0f - uv.y;
    }
    int2 sz = int2(cellLights.Load(h).xy + 0.5f.xx);
    int2 test;
    if (hf < 0)
    {
        test = sz;
    }
    else
    {
        test = int2(sz.x, sz.y / 2);
    }
    int2 pz = test;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 x0Pick;
    if (cellGoboGam)
    {
        x0Pick = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        x0Pick = lerp(c0, c1, f.y.xxx);
    }
    return x0Pick;
}
#endif

#if defined(WW_FN_cellShapeIn) || defined(WW_FN_ALL)
bool cellShapeIn(int i, float3 P, float3 Lpos, float shape)
{
    if (shape > (-2.5f))
    {
        return true;
    }
    if (shape > (-3.5f))
    {
        return dot(P - Lpos, cellLights.Load((i * 8) + 2).xyz) >= 0.0f;
    }
    float4 b0 = cellLights.Load((i * 8) + 5);
    float4 b1 = cellLights.Load((i * 8) + 6);
    float4 b2 = cellLights.Load((i * 8) + 7);
    float3 k = float3(dot(b0.xyz, P) + b0.w, dot(b1.xyz, P) + b1.w, dot(b2.xyz, P) + b2.w);
    float3 cPart2 = abs(k);
    return all(bool3(cPart2.x <= 1.0f.xxx.x, cPart2.y <= 1.0f.xxx.y, cPart2.z <= 1.0f.xxx.z));
}
#endif

#if defined(WW_FN_cellLightE) || defined(WW_FN_ALL)
float3 cellLightE(int i, float3 P, float3 N, inout float3 L, out bool noSpec)
{
    float4 t0 = cellLights.Load(i * 8);
    float3 Lv = t0.xyz - P;
    float d = length(Lv);
    L = Lv / max(d, 0.001000000047497451305389404296875f).xxx;
    noSpec = true;
    bool cPart2 = d >= t0.w;
    bool cPart;
    if (!cPart2)
    {
        cPart = dot(N, L) <= 0.0f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 0.0f.xxx;
    }
    float4 t1 = cellLights.Load((i * 8) + 1);
    int cond = i;
    float3 cond2 = P;
    float3 condPart = t0.xyz;
    float vAPart2 = t1.w;
    if (!cellShapeIn(cond, cond2, condPart, vAPart2))
    {
        return 0.0f.xxx;
    }
    float4 t3 = cellLights.Load((i * 8) + 3);
    float vAPart = d;
    float test = t0.w;
    float3 x0Pick = t3.xyz;
    float a = cellRadial(vAPart, test, x0Pick);
    // lane LIGHTS5 (GOBO1)
    float3 g = 1.0f.xxx;
    float3 gAx = 0.0f.xxx;
    if (t1.w > (-1.5f))
    {
        float4 t2 = cellLights.Load((i * 8) + 2);
        float base = clamp(1.0f - ((1.0f - dot(-L, t2.xyz)) / max(1.0f - t1.w, 9.9999997473787516355514526367188e-05f)), 0.0f, 1.0f);
        a *= min(pow(base, max(t2.w, 0.001000000047497451305389404296875f)), 1.0f);
        gAx = t2.xyz;
    }
    if (a > 0.0f)
    {
        int wPick = i;
        float3 cellPhysFalloffRet2 = -Lv;
        float3 rawPart2 = gAx;
        bool rawPart = t1.w > (-1.5f);
        // one call for every shape
        g = cellGobo(wPick, cellPhysFalloffRet2, rawPart2, rawPart);
    }
    if (a > 0.0f)
    {
        int kPick = i;
        float3 kPick2 = P;
        float3 cellPhysFalloffRet = N;
        a *= cellShadowF(kPick, kPick2, cellPhysFalloffRet);
    }
    noSpec = (int(t3.w + 0.5f) & 1) != 0;
    return (t1.xyz * g) * a;
}
#endif

#if defined(WW_FN_cellAreaSpec) || defined(WW_FN_ALL)
void cellAreaSpec(int i, float3 P, float3 N, float3 V, out float3 Ls, inout float rS)
{
    float4 t0 = cellLights.Load(i * 8);
    float3 Lv = t0.xyz - P;
    float d = length(Lv);
    float R = clamp(min(cellLightPhysB.y, t0.w * cellLightPhys.z), 2.0f, 32.0f);
    float3 r = reflect(-V, N);
    float3 c = (r * dot(Lv, r)) - Lv;
    float3 p = Lv + (c * clamp(R / max(length(c), 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f));
    Ls = normalize(p);
    if ((int(cellLightPhysB.z + 0.5f) & 4) == 0)
    {
        rS = sqrt(clamp(min(max(rS * rS, 9.9999997473787516355514526367188e-05f) + (R / max(2.0f * d, 9.9999997473787516355514526367188e-05f)), 0.99989998340606689453125f), 0.0f, 1.0f));
    }
}
#endif

#if defined(WW_FN_cellFarW) || defined(WW_FN_ALL)
float cellFarW(float3 P)
{
    if (!cellFarOn)
    {
        return 0.0f;
    }
    float d = length(P - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w));
    if ((cellFarRed & 2) != 0)
    {
        return step(0.5f * (cellFarBand.x + cellFarBand.y), d);
    }
    return smoothstep(cellFarBand.x, cellFarBand.y, d);
}
#endif

#if defined(WW_FN_cellFarHash) || defined(WW_FN_ALL)
uint cellFarHash(int3 k, int side)
{
    return (((uint(k.x) * 73856093u) ^ (uint(k.y) * 19349663u)) ^ (uint(k.z) * 83492791u)) ^ (uint(side) * 2654435761u);
}
#endif

#if defined(WW_FN_cellFarF) || defined(WW_FN_ALL)
float3 cellFarF(float3 P, float3 N)
{
    float3 a = abs(N);
    int ax = 0;
    if (a.y > a.x)
    {
        ax = 1;
    }
    if (a.z > a[ax])
    {
        ax = 2;
    }
    int side = (2 * ax) + ((N[ax] >= 0.0f) ? 0 : 1);
    float3 nudge = 0.0f.xxx;
    nudge[ax] = ((side & 1) != 0) ? (-0.5f) : 0.5f;
    int3 k0 = int3(floor((P - nudge) / cellFarCell.xxx));
    if ((cellFarRed & 4) != 0)
    {
        side ^= 1;
    }
    uint mask = (1u << uint(cellFarBits)) - 1u;
    float R2 = (2.25f * cellFarCell) * cellFarCell;
    float3 s = 0.0f.xxx;
    float ws = 0.0f;
    for (int dz = -1; dz <= 1; dz++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                int3 k = k0 + int3(dx, dy, dz);
                int3 cPart2 = k;
                int cPart = side;
                uint h = cellFarHash(cPart2, cPart) & mask;
                float3 key = float3(float(k.x), float(k.y), float((k.z * 8) + side));
                for (int j = 0; (j < cellFarProbe) && (j < 64); j++)
                {
                    cellFarIters++;
                    float4 t = cellFarSlots.Load(int(h));
                    if (t.w == 0.0f)
                    {
                        break;
                    }
                    if (all(bool3(t.xyz.x == key.x, t.xyz.y == key.y, t.xyz.z == key.z)))
                    {
                        int r = int(t.w) - 1;
                        float3 d = P - cellFarRecs.Load(r * 2).xyz;
                        float q = 1.0f - (dot(d, d) / R2);
                        if (q > 0.0f)
                        {
                            q *= q;
                            s += (cellFarRecs.Load((r * 2) + 1).xyz * q);
                            ws += q;
                        }
                    }
                    h = (h + 1u) & mask;
                }
            }
        }
    }
    float3 cond;
    if (ws > 0.0f)
    {
        cond = s / ws.xxx;
    }
    else
    {
        cond = 0.0f.xxx;
    }
    return cond;
}
#endif

#if defined(WW_FN_cellFarPlacedE) || defined(WW_FN_ALL)
float3 cellFarPlacedE(float3 P, float3 N)
{
    if (!cellFarPlacedOn)
    {
        return 0.0f.xxx;
    }
    float3 g = ((P + (N * (0.5f * cellFarPlacedVoxel))) - cellFarPlacedOrigin) / cellFarPlacedVoxel.xxx;
    float2 xy = g.xy / cellFarPlacedDims.xy;
    float z = clamp(g.z, 0.5f, cellFarPlacedDims.z - 0.5f);
    float depth = 6.0f * cellFarPlacedDims.z;
    float3 n2 = N * N;
    float4 s = ((cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.x >= 0.0f) ? 0.0f : 1.0f) * cellFarPlacedDims.z)) / depth)) * n2.x) + (cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.y >= 0.0f) ? 2.0f : 3.0f) * cellFarPlacedDims.z)) / depth)) * n2.y)) + (cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.z >= 0.0f) ? 4.0f : 5.0f) * cellFarPlacedDims.z)) / depth)) * n2.z);
    if (cellGiSky && false)
    {
        return max(s.xyz, 0.0f.xxx);
    }
    float3 cPart2;
    if (s.w > 0.00999999977648258209228515625f)
    {
        cPart2 = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        cPart2 = 0.0f.xxx;
    }
    return cPart2;
}
#endif

#if defined(WW_FN_cellFarTerms) || defined(WW_FN_ALL)
float cellFarTerms(float3 P, float3 N, out float3 F, out float3 gP)
{
    F = 0.0f.xxx;
    gP = 0.0f.xxx;
    float3 cPart2 = P;
    float w = cellFarW(cPart2);
    if (w <= 0.0f)
    {
        return 0.0f;
    }
    if ((cellFarRed & 1) == 0)
    {
        float3 cPart = P;
        float3 cond = N;
        float3 cond2 = cellFarF(cPart, cond);
        F = cond2;
    }
    float3 condPart = P;
    float3 vAPart2 = N;
    gP = cellFarPlacedE(condPart, vAPart2) * 0.31830990314483642578125f;
    return w;
}
#endif

#if defined(WW_FN_cellFarProbeOut) || defined(WW_FN_ALL)
float3 cellFarProbeOut(float3 P, float3 N)
{
    float3 cPart2 = P;
    float w = cellFarW(cPart2);
    if (cellProbe == 93)
    {
        return w.xxx;
    }
    if (cellProbe == 91)
    {
        float3 cPart;
        if (cellFarOn)
        {
            float3 cond = P;
            float3 cond2 = N;
            float3 condPart = cellFarF(cond, cond2);
            cPart = condPart;
        }
        else
        {
            cPart = 0.0f.xxx;
        }
        return sqrt(clamp(cPart * 0.25f, 0.0f.xxx, 1.0f.xxx));
    }
    if (cellProbe == 94)
    {
        bool vAPart2 = w > 0.0f;
        bool vAPart;
        if (vAPart2)
        {
            vAPart = (cellFarRed & 1) == 0;
        }
        else
        {
            vAPart = vAPart2;
        }
        if (vAPart)
        {
            float3 test = P;
            float3 x0Pick = N;
            float3 wPick = cellFarF(test, x0Pick);
        }
        float3 cellPhysFalloffRet2 = P;
        int rawPart2;
        int rawPart = cellLightsBegin(cellPhysFalloffRet2, rawPart2);
        int nW = rawPart2;
        return (float(((w >= 1.0f) ? 0 : nW) + cellFarIters) / 256.0f).xxx;
    }
    float3 E = 0.0f.xxx;
    float3 kPick = P;
    int kPick2;
    int cellPhysFalloffRet = cellLightsBegin(kPick, kPick2);
    int nL = kPick2;
    int bL = cellPhysFalloffRet;
    float3 cellPhysFalloffPart2;
    bool cellPhysFalloffPart;
    for (int k = 0; k < nL; k++)
    {
        int iPart2 = bL;
        int iPart = k;
        int i = cellLightAt(iPart2, iPart);
        int cellLightEPart3 = i;
        float3 cellLightEPart2 = P;
        float3 cellLightEPart = N;
        float3 cellLightERet = cellLightE(cellLightEPart3, cellLightEPart2, cellLightEPart, cellPhysFalloffPart2, cellPhysFalloffPart);
        float3 L = cellPhysFalloffPart2;
        bool ns = cellPhysFalloffPart;
        float3 c = cellLightERet;
        E += (c * max(dot(N, L), 0.0f));
    }
    float3 EPart2 = P;
    float3 EPart = N;
    E += (cellFarPlacedE(EPart2, EPart) * 0.31830990314483642578125f);
    return sqrt(clamp(E * 0.25f, 0.0f.xxx, 1.0f.xxx));
}
#endif

#if defined(WW_FN_cellSsgiE) || defined(WW_FN_ALL)
float3 cellSsgiE(float3 eProbe)
{
    if (!cellSsgiOn)
    {
        return eProbe;
    }
    float4 s = cellSsgi.Sample(_cellSsgi_sampler, (gl_FragCoord.xy - cellSsgiRect.xy) * cellSsgiRect.zw);
    float cPart2;
    if ((cellSsgiRed & 2) != 0)
    {
        cPart2 = 1.0f;
    }
    else
    {
        cPart2 = lerp(1.0f, clamp(s.w, 0.0f, 1.0f), cellSsgiK.x);
    }
    float keep = cPart2;
    return (eProbe * keep) + (max(s.xyz, 0.0f.xxx) * cellSsgiK.y);
}
#endif

#if defined(WW_FN_cellProbeRaw) || defined(WW_FN_ALL)
float3 cellProbeRaw(float3 P, float3 N)
{
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 cPart2 = P;
        float3 cPart = N;
        float3 cond = cellFarProbeOut(cPart2, cPart);
        return cond;
    }
    // the lit effects' probes: a surface writes black
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    // lane SSGI2: E_probe x the open share + E_screen when SSGI runs (the double-count gate)
    if (cellProbe == 5)
    {
        float3 cond2;
        if (false)
        {
            float3 condPart = P;
            float3 vAPart2 = N;
            cond2 = cellGiE(condPart, vAPart2);
        }
        else
        {
            cond2 = 0.0f.xxx;
        }
        float3 vAPart = cond2;
        return clamp(cellSsgiE(vAPart) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    // the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
    if (cellProbe == 90)
    {
        float3 test = P;
        float3 x0Pick = N;
        return cellAmbShare(test, x0Pick).xxx;
    }
    // r = the branch cellGiSample took / 16, g = its .a, b = its E luminance / pi (sqrt)
    if (cellProbe == 95)
    {
        float4 wPick;
        if (false)
        {
            float3 cellPhysFalloffRet2 = P;
            float3 rawPart2 = N;
            wPick = cellGiSample(cellPhysFalloffRet2, rawPart2);
        }
        else
        {
            wPick = 0.0f.xxxx;
        }
        float4 s = wPick;
        return float3(float(cellGiPath) * 0.0625f, clamp(s.w, 0.0f, 1.0f), sqrt(clamp(dot(max(s.xyz, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)) * 0.31830990314483642578125f, 0.0f, 1.0f)));
    }
    if (cellProbe == 1)
    {
        float3 E = 0.0f.xxx;
        float3 rawPart = P;
        int kPick;
        int kPick2 = cellLightsBegin(rawPart, kPick);
        int nL = kPick;
        int bL = kPick2;
        float3 cellPhysFalloffRet;
        bool cellPhysFalloffPart2;
        for (int k = 0; k < nL; k++)
        {
            int cellPhysFalloffPart = bL;
            int iPart2 = k;
            int i = cellLightAt(cellPhysFalloffPart, iPart2);
            int iPart = i;
            float3 cellLightEPart3 = P;
            float3 cellLightEPart2 = N;
            float3 cellLightEPart = cellLightE(iPart, cellLightEPart3, cellLightEPart2, cellPhysFalloffRet, cellPhysFalloffPart2);
            float3 L = cellPhysFalloffRet;
            bool ns = cellPhysFalloffPart2;
            float3 c = cellLightEPart;
            E += (c * max(dot(N, L), 0.0f));
        }
        return clamp(E * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 7)
    {
        float3 cellLightERet = P;
        float3 EPart2 = N;
        return cellShadowProbe(cellLightERet, EPart2);
    }
    float3 q = clamp(floor((P - cellCenter) + 32768.0f.xxx), 0.0f.xxx, 65535.0f.xxx);
    if (cellProbe == 2)
    {
        return floor(q / 256.0f.xxx) / 255.0f.xxx;
    }
    if (cellProbe == 3)
    {
        return mod(q, 256.0f.xxx) / 255.0f.xxx;
    }
    return (N * 0.5f) + 0.5f.xxx;
}
#endif

#if defined(WW_FN_cellPassOut) || defined(WW_FN_ALL)
float3 cellPassOut(float3 P, float3 N)
{
    bool wwh3 = ((cellPass >= 3) && (cellPass != 6));
    if (or(wwh3, true))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        bool3 cPart2 = (cellPass == 6).xxx;
        return float3(cPart2.x ? float3(1.0f, 0.0f, 1.0f).x : float3(0.0f, 1.0f, 1.0f).x, cPart2.y ? float3(1.0f, 0.0f, 1.0f).y : float3(0.0f, 1.0f, 1.0f).y, cPart2.z ? float3(1.0f, 0.0f, 1.0f).z : float3(0.0f, 1.0f, 1.0f).z);
    }
    bool cPart = cellPass != 6;
    bool cond;
    if (cPart)
    {
        cond = (cellPassRed & 2) != 0;
    }
    else
    {
        cond = cPart;
    }
    bool3 cond2 = cond.xxx;
    float3 condPart = P;
    float3 vAPart2 = float3(cond2.x ? float3(0.0f, 0.0f, 1.0f).x : N.x, cond2.y ? float3(0.0f, 0.0f, 1.0f).y : N.y, cond2.z ? float3(0.0f, 0.0f, 1.0f).z : N.z);
    float4 s = cellGiSample(condPart, vAPart2);
    if (cellPass == 6)
    {
        float3 vAPart;
        if (s.w <= 0.00999999977648258209228515625f)
        {
            vAPart = float3(1.0f, 0.0f, 1.0f);
        }
        else
        {
            vAPart = (0.25f + (0.5f * clamp(s.w, 0.0f, 1.0f))).xxx;
        }
        return vAPart;
    }
    if (s.w <= 0.00999999977648258209228515625f)
    {
        return float3(0.0f, 1.0f, 1.0f);
    }
    float3 v = max(s.xyz / s.w.xxx, 0.0f.xxx);
    if (cellPass == 1)
    {
        v *= 0.31830990314483642578125f;
        // debug "direct": the cell's lights leak into the pass
        if ((cellPassRed & 1) != 0)
        {
            float3 test = P;
            int x0Pick;
            int wPick = cellLightsBegin(test, x0Pick);
            int nL = x0Pick;
            int bL = wPick;
            float3 cellPhysFalloffRet2;
            bool rawPart2;
            for (int k = 0; k < nL; k++)
            {
                int rawPart = bL;
                int kPick = k;
                int i = cellLightAt(rawPart, kPick);
                int kPick2 = i;
                float3 cellPhysFalloffRet = P;
                float3 cellPhysFalloffPart2 = N;
                float3 cellPhysFalloffPart = cellLightE(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffRet2, rawPart2);
                float3 L = cellPhysFalloffRet2;
                bool ns = rawPart2;
                float3 c = cellPhysFalloffPart;
                v += (c * max(dot(N, L), 0.0f));
            }
        }
    }
    return pow(clamp(v, 0.0f.xxx, 1.0f.xxx), 0.4545454680919647216796875f.xxx);
}
#endif

#if defined(WW_FN_cellSpecGame) || defined(WW_FN_ALL)
float cellSpecGame(float3 N, float3 L, float3 V, float gloss)
{
    float n = exp2((gloss * 10.0f) + 1.0f);
    float3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
    float NdotV = clamp(dot(N, V), 0.0f, 1.0f);
    float VdotH = clamp(dot(V, H), 0.0f, 1.0f);
    float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
    float D_1 = (pow(NdotH, n) * (n + 2.0f)) * 0.15915499627590179443359375f;
    float m = min(NdotL, NdotV);
    float cPart2;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float cPart;
        if (NdotV == m)
        {
            cPart = 1.0f;
        }
        else
        {
            cPart = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        cPart2 = ((2.0f * NdotH) * cPart) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        cPart2 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = cPart2;
    float f1 = 1.0f - VdotH;
    float f4 = ((f1 * f1) * f1) * f1;
    float F = min(((1.0f - (f1 * f4)) * 0.20000000298023223876953125f) + (f4 * f1), 1.0f);
    return min(((D_1 * G) * F) * 0.25f, 15.0f) * 3.14159297943115234375f;
}
#endif

#if defined(WW_FN_cellSpecGame_V2) || defined(WW_FN_ALL)
float cellSpecGame(float3 N, float3 L, float3 V, float gloss)
{
    float n = exp2((gloss * 10.0f) + 1.0f);
    float3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
    float NdotV = clamp(dot(N, V), 0.0f, 1.0f);
    float VdotH = clamp(dot(V, H), 0.0f, 1.0f);
    float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
    float D = (pow(NdotH, n) * (n + 2.0f)) * 0.15915499627590179443359375f;
    float m = min(NdotL, NdotV);
    float cPart2;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float cPart;
        if (NdotV == m)
        {
            cPart = 1.0f;
        }
        else
        {
            cPart = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        cPart2 = ((2.0f * NdotH) * cPart) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        cPart2 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = cPart2;
    float f1 = 1.0f - VdotH;
    float f4 = ((f1 * f1) * f1) * f1;
    float F = min(((1.0f - (f1 * f4)) * 0.20000000298023223876953125f) + (f4 * f1), 1.0f);
    return min(((D * G) * F) * 0.25f, 15.0f) * 3.14159297943115234375f;
}
#endif

#if defined(WW_FN_cellOren) || defined(WW_FN_ALL)
float cellOren(float3 N, float3 L, float3 V, float NdotL, float gloss)
{
    if ((cellRed & 8) != 0)
    {
        return 1.0f;
    }
    float s2 = (1.0f - gloss) * (1.0f - gloss);
    float A_1 = 1.0f - ((0.5f * s2) / (s2 + 0.569999992847442626953125f));
    float B = (0.449999988079071044921875f * s2) / (s2 + 0.0900000035762786865234375f);
    float NdotV = dot(N, V);
    float3 tV = V - (N * NdotV);
    float3 tL = L - (N * NdotL);
    float cosPhi = dot(tV, tL);
    if ((cellRed & 16) != 0)
    {
        cosPhi /= max(length(tV) * length(tL), 9.9999999747524270787835121154785e-07f);
    }
    float geom = sqrt(clamp((1.0f - (NdotL * NdotL)) * (1.0f - (NdotV * NdotV)), 0.0f, 1.0f)) / max(max(NdotL, NdotV), 9.9999997473787516355514526367188e-05f);
    return ((max(cosPhi, 0.0f) * B) * geom) + A_1;
}
#endif

#if defined(WW_FN_cellOren_V2) || defined(WW_FN_ALL)
float cellOren(float3 N, float3 L, float3 V, float NdotL, float gloss)
{
    if ((cellRed & 8) != 0)
    {
        return 1.0f;
    }
    float s2 = (1.0f - gloss) * (1.0f - gloss);
    float A = 1.0f - ((0.5f * s2) / (s2 + 0.569999992847442626953125f));
    float B = (0.449999988079071044921875f * s2) / (s2 + 0.0900000035762786865234375f);
    float NdotV = dot(N, V);
    float3 tV = V - (N * NdotV);
    float3 tL = L - (N * NdotL);
    float cosPhi = dot(tV, tL);
    if ((cellRed & 16) != 0)
    {
        cosPhi /= max(length(tV) * length(tL), 9.9999999747524270787835121154785e-07f);
    }
    float geom = sqrt(clamp((1.0f - (NdotL * NdotL)) * (1.0f - (NdotV * NdotV)), 0.0f, 1.0f)) / max(max(NdotL, NdotV), 9.9999997473787516355514526367188e-05f);
    return ((max(cosPhi, 0.0f) * B) * geom) + A;
}
#endif

#if defined(WW_FN_cellRim) || defined(WW_FN_ALL)
float cellRim(float3 N, float3 L, float3 V, float gloss)
{
    if ((cellRed & 40) != 0)
    {
        return 0.0f;
    }
    float edge = pow(clamp(1.0f - dot(N, V), 0.0f, 1.0f), 0.00999999977648258209228515625f);
    return (clamp(dot(V, -L), 0.0f, 1.0f) * edge) * (1.0f - gloss);
}
#endif

#if defined(WW_FN_cellSumLights) || defined(WW_FN_ALL)
void cellSumLights(float3 P, float3 N, float3 Vw, float gloss, inout float3 diff, inout float3 diffOn, inout float3 spec)
{
    diff = 0.0f.xxx;
    diffOn = 0.0f.xxx;
    cellRimSum = 0.0f.xxx;
    spec = 0.0f.xxx;
    float cPart2;
    if (cellFarFade)
    {
        float3 cPart = P;
        cPart2 = cellFarW(cPart);
    }
    else
    {
        cPart2 = 0.0f;
    }
    // the real lights fade out over the far band
    float fw = cPart2;
    if (fw >= 1.0f)
    {
        return;
    }
    float3 cond = P;
    int cond2;
    int condPart = cellLightsBegin(cond, cond2);
    int nL = cond2;
    int bL = condPart;
    float vAPart2;
    float vAPart;
    for (int k = 0; k < nL; k++)
    {
        int test = bL;
        int x0Pick = k;
        int i = cellLightAt(test, x0Pick);
        float4 t0 = cellLights.Load(i * 8);
        float3 Lv = t0.xyz - P;
        float d = length(Lv);
        if (d >= t0.w)
        {
            continue;
        }
        float3 L = Lv / max(d, 0.001000000047497451305389404296875f).xxx;
        float NdotL = dot(N, L);
        if (NdotL <= 0.0f)
        {
            continue;
        }
        float4 t1 = cellLights.Load((i * 8) + 1);
        int wPick = i;
        float3 cellPhysFalloffRet2 = P;
        float3 rawPart2 = t0.xyz;
        float rawPart = t1.w;
        if (!cellShapeIn(wPick, cellPhysFalloffRet2, rawPart2, rawPart))
        {
            continue;
        }
        float4 t3 = cellLights.Load((i * 8) + 3);
        float kPick = d;
        float kPick2 = t0.w;
        float3 cellPhysFalloffRet = t3.xyz;
        float a = cellRadial(kPick, kPick2, cellPhysFalloffRet);
        // lane LIGHTS5 (GOBO1)
        float3 g = 1.0f.xxx;
        float3 gAx = 0.0f.xxx;
        if (t1.w > (-1.5f))
        {
            // PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
            float4 t2 = cellLights.Load((i * 8) + 2);
            float base = clamp(1.0f - ((1.0f - dot(-L, t2.xyz)) / max(1.0f - t1.w, 9.9999997473787516355514526367188e-05f)), 0.0f, 1.0f);
            a *= min(pow(base, max(t2.w, 0.001000000047497451305389404296875f)), 1.0f);
            gAx = t2.xyz;
        }
        if (a > 0.0f)
        {
            int cellPhysFalloffPart2 = i;
            float3 cellPhysFalloffPart = -Lv;
            float3 iPart2 = gAx;
            bool iPart = t1.w > (-1.5f);
            // GOBO2
            g = cellGobo(cellPhysFalloffPart2, cellPhysFalloffPart, iPart2, iPart);
        }
        if (a > 0.0f)
        {
            int cellLightEPart3 = i;
            float3 cellLightEPart2 = P;
            float3 cellLightEPart = N;
            int cellLightERet = i;
            float3 EPart2 = P;
            // lane SHADOW1; GAPLOOK1 (C8)
            a *= (cellShadowF(cellLightEPart3, cellLightEPart2, cellLightEPart) * cellContactLocal(cellLightERet, EPart2));
        }
        float3 E = ((t1.xyz * g) * a) * NdotL;
        diff += E;
        // No Rim Lighting (2) drops the back-light; Ignore Roughness (4) also takes Lambert's diffuse
        // WW_CELL_LIT_RED=rimflags
        int fl = int(t3.w + 0.5f) & (((cellRed & 64) != 0) ? 1 : 7);
        if ((fl & 6) != 0)
        {
            vAPart2 = 0.0f;
        }
        else
        {
            float3 EPart = N;
            float3 vAPart10 = L;
            float3 vAPart9 = Vw;
            float vAPart8 = gloss;
            vAPart2 = cellRim(EPart, vAPart10, vAPart9, vAPart8);
        }
        float rim = vAPart2;
        if ((fl & 4) != 0)
        {
            vAPart = 1.0f;
        }
        else
        {
            float3 vAPart7 = N;
            float3 vAPart6 = L;
            float3 vAPart5 = Vw;
            float vAPart4 = NdotL;
            float vAPart3 = gloss;
            vAPart = cellOren(vAPart7, vAPart6, vAPart5, vAPart4, vAPart3);
        }
        diffOn += (E * (vAPart + rim));
        cellRimSum += (E * rim);
        bool cond4 = (fl & 1) == 0;
        bool cond3;
        if (!cond4)
        {
            cond3 = (cellRed & 65536) != 0;
        }
        else
        {
            cond3 = cond4;
        }
        if (cond3)
        {
            float3 specPart4 = N;
            float3 specPart3 = L;
            float3 specPart2 = Vw;
            float specPart = gloss;
            spec += (E * cellSpecGame(specPart4, specPart3, specPart2, specPart));
        }
    }
    if (fw > 0.0f)
    {
        diff *= (1.0f - fw);
        diffOn *= (1.0f - fw);
        cellRimSum *= (1.0f - fw);
        spec *= (1.0f - fw);
    }
}
#endif

#if defined(WW_FN_cellAmboScale) || defined(WW_FN_ALL)
float3 cellAmboScale(float3 P)
{
    for (int i = 0; i < cellAmboCount; i++)
    {
        // linked to a box, it fills the box (not the sphere)
        if (cellAmboK[i].w > 0.5f)
        {
            float4 b0 = cellAmboBox[i * 3];
            float4 b1 = cellAmboBox[(i * 3) + 1];
            float4 b2 = cellAmboBox[(i * 3) + 2];
            float3 k = float3(dot(b0.xyz, P) + b0.w, dot(b1.xyz, P) + b1.w, dot(b2.xyz, P) + b2.w);
            float3 cPart2 = abs(k);
            if (all(bool3(cPart2.x < 1.0f.xxx.x, cPart2.y < 1.0f.xxx.y, cPart2.z < 1.0f.xxx.z)))
            {
                return cellAmboK[i].xyz;
            }
            continue;
        }
        float3 d = P - cellAmbo[i].xyz;
        if (dot(d, d) < (cellAmbo[i].w * cellAmbo[i].w))
        {
            return cellAmboK[i].xyz;
        }
    }
    return 1.0f.xxx;
}
#endif

#if defined(WW_FN_cellAmbientSum) || defined(WW_FN_ALL)
float3 cellAmbientSum(float3 N, float3 P)
{
    float4 n1 = float4(N, 1.0f);
    float3 cPart2 = P;
    return float3(dot(cellDalc[0], n1), dot(cellDalc[1], n1), dot(cellDalc[2], n1)) * cellAmboScale(cPart2);
}
#endif

#if defined(WW_FN_cellAmbient) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 cPart2 = N;
    float3 cPart = P;
    return pow(max(cellAmbientSum(cPart2, cPart), 0.0f.xxx), 2.2000000476837158203125f.xxx);
}
#endif

#if defined(WW_FN_cellAmbient_V2) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 cPart2 = N;
    float3 cPart = P;
    return pow(max(cellAmbientSum(cPart2, cPart), 0.0f.xxx), 2.2000000476837158203125f.xxx) * fxAmbShare;
}
#endif

#if defined(WW_FN_cellAmbGi) || defined(WW_FN_ALL)
float3 cellAmbGi(float3 amb, float3 gi, float3 P, float3 N)
{
    if (cellGiAmb == 2)
    {
        return 0.0f.xxx;
    }
    bool wwh4 = (cellGiAmb == 0);
    if (or(wwh4, true))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return amb;
    }
    if (cellGiAmb == 1)
    {
        float3 cPart2 = P;
        float3 cPart = N;
        return amb * (1.0f - clamp(cellGiSample(cPart2, cPart).w, 0.0f, 1.0f));
    }
    if (cellGiAmb == 3)
    {
        return max(amb - gi, 0.0f.xxx);
    }
    return gi;
}
#endif

#if defined(WW_FN_cellCubeGameOn) || defined(WW_FN_ALL)
bool cellCubeGameOn()
{
    bool cPart2 = abs(cellCubeMat.z - 1.0f) < 0.5f;
    bool cPart;
    if (!cPart2)
    {
        cPart = abs(cellCubeMat.z - 3.0f) < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    return cPart;
}
#endif

#if defined(WW_FN_cellCubeGame) || defined(WW_FN_ALL)
float3 cellCubeGame(TextureCube<float4> cube, SamplerState _cube_sampler, float3 dir, float gloss, float spec, float envScale, float3 posView)
{
    if (!cellCubeGameOn())
    {
        return 0.0f.xxx;
    }
    float g = clamp(gloss, 0.0f, 1.0f);
    // view units -> game units
    float depth = abs(posView.z) * length(cellRow[0].xyz);
    float lod = (((1.0f - g) * 6.0f) + (depth * 0.00195299996994435787200927734375f)) + log2(float(int2(spvTextureSize(cube, uint(0), dummy_parameter)).x) / 128.0f);
    float3 c = cube.SampleLevel(_cube_sampler, dir, max(lod, 0.0f)).xyz;
    // an untagged cube: the game's sampler decodes sRGB before the filter, this after
    if (cellCubeMat.z < 2.0f)
    {
        c = lerp(c / 12.9200000762939453125f.xxx, pow((c + 0.054999999701976776123046875f.xxx) / 1.05499994754791259765625f.xxx, 2.400000095367431640625f.xxx), step(0.040449999272823333740234375f.xxx, c));
    }
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(g - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    return c * k;
}
#endif

#if defined(WW_FN_cellCubeProbe) || defined(WW_FN_ALL)
float3 cellCubeProbe(float2 uv, float3 normalView)
{
    float2 f = clamp(floor((frac(uv) * 65535.0f) + 0.5f.xx), 0.0f.xx, 65535.0f.xx);
    float tag = cellCubeMat.w / 255.0f;
    if (cellProbe == 51)
    {
        return float3(floor(f.x / 256.0f) / 255.0f, mod(f.x, 256.0f) / 255.0f, tag);
    }
    if (cellProbe == 52)
    {
        return float3(floor(f.y / 256.0f) / 255.0f, mod(f.y, 256.0f) / 255.0f, tag);
    }
    float3 cPart2 = normalView;
    float3 n = ((cellWorldDir(cPart2) * 0.5f) + 0.5f.xxx) * 255.0f;
    return (n - floor(n + 0.5f.xxx)) + 0.5f.xxx;
}
#endif

#if defined(WW_FN_cellLit) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    float3 cond = Vview;
    float3 Vw = cellWorldDir(cond);
    float gloss = 1.0f - sqrt(alphaR);
    float3 cond2 = P;
    float3 condPart = N;
    float3 vAPart2 = Vw;
    float vAPart = gloss;
    float3 test;
    float3 x0Pick;
    float3 wPick;
    cellSumLights(cond2, condPart, vAPart2, vAPart, test, x0Pick, wPick);
    float3 diff = test;
    float3 diffOn = x0Pick;
    float3 spec = wPick;
    float3 alb = albedo * albedo;
    float3 cellPhysFalloffRet2;
    if (false)
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        cellPhysFalloffRet2 = cellGiE(rawPart2, rawPart);
    }
    else
    {
        cellPhysFalloffRet2 = 0.0f.xxx;
    }
    float3 kPick = cellPhysFalloffRet2;
    float3 gi = cellSsgiE(kPick) * 0.31830990314483642578125f;
    float3 kPick2 = P;
    float3 cellPhysFalloffRet = N;
    float3 cellPhysFalloffPart2;
    float3 cellPhysFalloffPart;
    float iPart2 = cellFarTerms(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffPart);
    float3 farF = cellPhysFalloffPart2;
    float3 farGP = cellPhysFalloffPart;
    float farW = iPart2;
    if (farW > 0.0f)
    {
        gi = max(gi - (farGP * farW), 0.0f.xxx);
        diff += (farF * farW);
        diffOn += (farF * farW);
    }
    float3 add = (alb * (diffOn + gi)) + ((spec * specMask) * specCol);
    if ((!cellInterior) && (!cellHasDir))
    {
        return sqrt(((color * color) + add) + cellReflAdd);
    }
    if (!cellInterior)
    {
        float3 Ld = normalize(cellDirTo);
        float nl = dot(N, Ld);
        float3 dir = (cellDirColor * max(nl, 0.0f)) * cellDirVis;
        float3 airDif = cellDirColor * (cellAirDif * (0.5f + (0.5f * N.z)));
        float3 Ex = (((diff + gi) + dir) + airDif) + cellExtAmb;
        float3 iPart = N;
        float3 cellLightEPart3 = Ld;
        float3 cellLightEPart2 = Vw;
        float cellLightEPart = nl;
        float cellLightERet = gloss;
        float3 EPart2 = N;
        float3 EPart = Ld;
        float3 vAPart10 = Vw;
        float vAPart9 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(iPart, cellLightEPart3, cellLightEPart2, cellLightEPart, cellLightERet) + cellRim(EPart2, EPart, vAPart10, vAPart9)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 vAPart8;
        if ((cellRed & 512) != 0)
        {
            vAPart8 = Ex;
        }
        else
        {
            vAPart8 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * vAPart8)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 vAPart7 = N;
        float3 vAPart6 = P;
        float3 amb = cellAmbient(vAPart7, vAPart6);
        if (cellGiAmb != 0)
        {
            float3 vAPart5 = amb;
            float3 vAPart4 = gi;
            float3 vAPart3 = P;
            float3 cond4 = N;
            amb = cellAmbGi(vAPart5, vAPart4, vAPart3, cond4);
        }
        E += amb;
        Ed += amb;
    }
    if (cellHasDir)
    {
        float3 Ld_1 = normalize(cellDirTo);
        float nl_1 = dot(N, Ld_1);
        float3 dir_1 = cellDirColor * max(nl_1, 0.0f);
        E += dir_1;
        float3 cond3 = N;
        float3 specPart4 = Ld_1;
        float3 specPart3 = Vw;
        float specPart2 = nl_1;
        float specPart = gloss;
        float3 EdPart4 = N;
        float3 EdPart3 = Ld_1;
        float3 EdPart2 = Vw;
        float EdPart = gloss;
        Ed += ((dir_1 * (cellOren(cond3, specPart4, specPart3, specPart2, specPart) + cellRim(EdPart4, EdPart3, EdPart2, EdPart))) + (cellDirColor * cellBackK));
    }
    bool3 linPart = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(linPart.x ? E.x : Ed.x, linPart.y ? E.y : Ed.y, linPart.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellLit_V2) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    float3 cond = Vview;
    float3 Vw = cellWorldDir(cond);
    float gloss = 1.0f - sqrt(alphaR);
    float3 cond2 = P;
    float3 condPart = N;
    float3 vAPart2 = Vw;
    float vAPart = gloss;
    float3 test;
    float3 x0Pick;
    float3 wPick;
    cellSumLights(cond2, condPart, vAPart2, vAPart, test, x0Pick, wPick);
    float3 diff = test;
    float3 diffOn = x0Pick;
    float3 spec = wPick;
    float3 alb = albedo * albedo;
    float3 cellPhysFalloffRet2;
    if (false)
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        cellPhysFalloffRet2 = cellGiE(rawPart2, rawPart);
    }
    else
    {
        cellPhysFalloffRet2 = 0.0f.xxx;
    }
    float3 kPick = cellPhysFalloffRet2;
    float3 gi = cellSsgiE(kPick) * 0.31830990314483642578125f;
    float3 kPick2 = P;
    float3 cellPhysFalloffRet = N;
    float3 cellPhysFalloffPart2;
    float3 cellPhysFalloffPart;
    float iPart2 = cellFarTerms(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffPart);
    float3 farF = cellPhysFalloffPart2;
    float3 farGP = cellPhysFalloffPart;
    float farW = iPart2;
    if (farW > 0.0f)
    {
        gi = max(gi - (farGP * farW), 0.0f.xxx);
        diff += (farF * farW);
        diffOn += (farF * farW);
    }
    float3 add = (alb * (diffOn + gi)) + ((spec * specMask) * specCol);
    if ((!cellInterior) && (!cellHasDir))
    {
        return sqrt(((color * color) + add) + cellReflAdd);
    }
    if (!cellInterior)
    {
        float3 Ld = normalize(cellDirTo);
        float nl = dot(N, Ld);
        float3 dir = (cellDirColor * max(nl, 0.0f)) * cellDirVis;
        float3 airDif = cellDirColor * (cellAirDif * (0.5f + (0.5f * N.z)));
        float3 Ex = (((diff + gi) + dir) + airDif) + cellExtAmb;
        float3 iPart = N;
        float3 cellLightEPart3 = Ld;
        float3 cellLightEPart2 = Vw;
        float cellLightEPart = nl;
        float cellLightERet = gloss;
        float3 EPart2 = N;
        float3 EPart = Ld;
        float3 vAPart10 = Vw;
        float vAPart9 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(iPart, cellLightEPart3, cellLightEPart2, cellLightEPart, cellLightERet) + cellRim(EPart2, EPart, vAPart10, vAPart9)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 vAPart8;
        float vAPart7;
        ussTerrainDim(vAPart8, vAPart7);
        ambDim = vAPart8;
        specDim = vAPart7;
        float3 vAPart6;
        if ((cellRed & 512) != 0)
        {
            vAPart6 = Ex;
        }
        else
        {
            vAPart6 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * vAPart6)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 vAPart5 = N;
        float3 vAPart4 = P;
        float3 amb = cellAmbient(vAPart5, vAPart4);
        if (cellGiAmb != 0)
        {
            float3 vAPart3 = amb;
            float3 cond4 = gi;
            float3 cond3 = P;
            float3 specPart4 = N;
            amb = cellAmbGi(vAPart3, cond4, cond3, specPart4);
        }
        E += amb;
        Ed += amb;
    }
    if (cellHasDir)
    {
        float3 Ld_1 = normalize(cellDirTo);
        float nl_1 = dot(N, Ld_1);
        float3 dir_1 = cellDirColor * max(nl_1, 0.0f);
        E += dir_1;
        float3 specPart3 = N;
        float3 specPart2 = Ld_1;
        float3 specPart = Vw;
        float EdPart4 = nl_1;
        float EdPart3 = gloss;
        float3 EdPart2 = N;
        float3 EdPart = Ld_1;
        float3 linPart = Vw;
        float EdPart5 = gloss;
        Ed += ((dir_1 * (cellOren(specPart3, specPart2, specPart, EdPart4, EdPart3) + cellRim(EdPart2, EdPart, linPart, EdPart5))) + (cellDirColor * cellBackK));
    }
    bool3 linPart2 = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(linPart2.x ? E.x : Ed.x, linPart2.y ? E.y : Ed.y, linPart2.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellProbeOut) || defined(WW_FN_ALL)
float3 cellProbeOut(float3 normalView, float3 posView, float alphaR, float kSmith)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    // the lit effects' probes: a surface writes black
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    // lane SSGI2: as cellProbeRaw's
    if (cellProbe == 5)
    {
        float3 cond;
        if (false)
        {
            float3 cond2 = P;
            float3 condPart = N;
            cond = cellGiE(cond2, condPart);
        }
        else
        {
            cond = 0.0f.xxx;
        }
        float3 vAPart2 = cond;
        return clamp(cellSsgiE(vAPart2) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    // the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
    if (cellProbe == 90)
    {
        float3 vAPart = P;
        float3 test = N;
        return cellAmbShare(vAPart, test).xxx;
    }
    // the branch cellGiSample took, its .a and its E (cellProbeRaw's)
    if (cellProbe == 95)
    {
        float3 x0Pick = P;
        float3 wPick = N;
        float3 cellPhysFalloffRet2 = cellProbeRaw(x0Pick, wPick);
        return cellPhysFalloffRet2;
    }
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        float3 kPick = cellFarProbeOut(rawPart2, rawPart);
        return kPick;
    }
    bool wwh5 = ((((cellProbe == 1) || (cellProbe == 8)) || (cellProbe == 10)) || (cellProbe == 30));
    bool wwh6 = (cellProbe == 31);
    if (or(wwh5, wwh6))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        float3 kPick2;
        if (cellProbe == 1)
        {
            kPick2 = N;
        }
        else
        {
            float3 cellPhysFalloffRet = -posView;
            kPick2 = cellWorldDir(cellPhysFalloffRet);
        }
        float3 cellPhysFalloffPart2 = P;
        float3 cellPhysFalloffPart = N;
        float3 iPart2 = kPick2;
        float iPart = 1.0f - sqrt(alphaR);
        float3 cellLightEPart3;
        float3 cellLightEPart2;
        float3 cellLightEPart;
        cellSumLights(cellPhysFalloffPart2, cellPhysFalloffPart, iPart2, iPart, cellLightEPart3, cellLightEPart2, cellLightEPart);
        float3 diff = cellLightEPart3;
        float3 diffOn = cellLightEPart2;
        float3 spec = cellLightEPart;
        if (cellProbe == 10)
        {
            return clamp(cellRimSum * 4.0f, 0.0f.xxx, 1.0f.xxx);
        }
        // the placed lights' specular / 4, before the mask
        if (cellProbe == 30)
        {
            return clamp(spec * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        // lane GAPFIX1: the same after the colour cellLit puts on it (deferred = white)
        if (cellProbe == 31)
        {
            return clamp((spec * cellSpecTint) * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        bool3 cellLightERet = (cellProbe == 8).xxx;
        return clamp(float3(cellLightERet.x ? diffOn.x : diff.x, cellLightERet.y ? diffOn.y : diff.y, cellLightERet.z ? diffOn.z : diff.z) * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 9)
    {
        // the gloss the diffuse used
        return float3(1.0f - sqrt(alphaR), 0.0f, 0.0f);
    }
    // the interior ambient's affine sum before its 2.2, x 8 (it is dim)
    if (cellProbe == 11)
    {
        float3 EPart2;
        if (cellHasDalc)
        {
            float3 EPart = N;
            float3 vAPart10 = P;
            EPart2 = clamp(cellAmbientSum(EPart, vAPart10) * 8.0f, 0.0f.xxx, 1.0f.xxx);
        }
        else
        {
            EPart2 = 0.0f.xxx;
        }
        return EPart2;
    }
    if (cellProbe == 7)
    {
        float3 vAPart9 = P;
        float3 vAPart8 = N;
        return cellShadowProbe(vAPart9, vAPart8);
    }
    // the position, 16 bits an axis over the 65536-unit box around the center
    float3 q = clamp(floor((P - cellCenter) + 32768.0f.xxx), 0.0f.xxx, 65535.0f.xxx);
    if (cellProbe == 2)
    {
        return floor(q / 256.0f.xxx) / 255.0f.xxx;
    }
    if (cellProbe == 3)
    {
        return mod(q, 256.0f.xxx) / 255.0f.xxx;
    }
    return (N * 0.5f) + 0.5f.xxx;
}
#endif
