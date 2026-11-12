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
        float3 tmp1 = x * 0.5f;
        float3 tmp2 = wwTonemapPhysical(tmp1);
        c = tmp2;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool tmp3;
    if (cellIsLutOn)
    {
        tmp3 = (cellIsRed & 1) == 0;
    }
    else
    {
        tmp3 = cellIsLutOn;
    }
    if (tmp3)
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
        float3 tmp1 = x * 0.5f;
        float3 tmp2 = wwTonemapPhysical(tmp1);
        c = tmp2;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool tmp3;
    if (cellIsLutOn)
    {
        tmp3 = (cellIsRed & 1) == 0;
    }
    else
    {
        tmp3 = cellIsLutOn;
    }
    if (tmp3)
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
        float tmp1;
        if (cellIsFoBloomFogCap > 0.0f)
        {
            tmp1 = min(cellBloomGain, cellIsFoBloomFogCap);
        }
        else
        {
            tmp1 = cellBloomGain;
        }
        float kg = min(cellIsFoBloomEc * tmp1, 1.0f);
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
        float3 tmp2 = x * 0.5f;
        float3 tmp3 = wwTonemapPhysical(tmp2);
        c = tmp3;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool tmp4;
    if (cellIsLutOn)
    {
        tmp4 = (cellIsRed & 1) == 0;
    }
    else
    {
        tmp4 = cellIsLutOn;
    }
    if (tmp4)
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
    float tmp1;
    if (c.z > 0.0f)
    {
        tmp1 = pow(x, c.z);
    }
    else
    {
        tmp1 = 1.0f;
    }
    float xe = tmp1;
    return pow(1.0f - clamp((c.y * xe) + c.x, 0.0f, 1.0f), 2.2000000476837158203125f);
}
#endif

#if defined(WW_FN_cellPhysRaw) || defined(WW_FN_ALL)
float cellPhysRaw(float x, float s)
{
    float x2 = x * x;
    float w = clamp(1.0f - (x2 * x2), 0.0f, 1.0f);
    float den = x2 + (s * s);
    float tmp1;
    if (den > 0.0f)
    {
        tmp1 = (w * w) / den;
    }
    else
    {
        tmp1 = 0.0f;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_cellPhysFalloff) || defined(WW_FN_ALL)
float cellPhysFalloff(float van, float x, float r, float3 c)
{
    float a = clamp(cellLightPhys.y, 0.0f, 1.0f);
    bool tmp1 = !(c.z > 0.0f);
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = !(clamp(c.y + c.x, 0.0f, 1.0f) > clamp(c.x, 0.0f, 1.0f));
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        float tmp4 = 1.0f;
        float3 tmp5 = c;
        tmp3 = cellAttVan(tmp4, tmp5) >= 0.0039215688593685626983642578125f;
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return van;
    }
    float tmp6 = a;
    float3 tmp7 = c;
    float vA = cellAttVan(tmp6, tmp7);
    float b = 1.0f - (a * a);
    float tmp8;
    if (b > 0.0f)
    {
        tmp8 = pow(b, 1.10000002384185791015625f);
    }
    else
    {
        tmp8 = 0.0f;
    }
    if (vA > tmp8)
    {
        return van;
    }
    int red = int(cellLightPhysB.z + 0.5f);
    if (cellLightPhys.x > 1.5f)
    {
        float tmp9;
        if (r > 0.0f)
        {
            tmp9 = clamp((max(max(cellLightPhysB.x, 0.001000000047497451305389404296875f), max((0.100000001490116119384765625f * r) / 69.98999786376953125f, 0.20000000298023223876953125f)) * 69.98999786376953125f) / r, 9.9999997473787516355514526367188e-05f, 0.5f);
        }
        else
        {
            tmp9 = 0.5f;
        }
        float x0 = tmp9;
        float den = 0.89999997615814208984375f - (0.666666686534881591796875f * x0);
        float t = clamp((x - 0.800000011920928955078125f) / 0.20000000298023223876953125f, 0.0f, 1.0f);
        float tmp10;
        if ((red & 2) != 0)
        {
            tmp10 = 1.0f;
        }
        else
        {
            tmp10 = 1.0f - ((t * t) * (3.0f - (2.0f * t)));
        }
        float w = tmp10;
        float m = max(x0, x);
        float tmp11;
        if (den > 0.0f)
        {
            tmp11 = ((cellLightPhys.w / den) * w) / (m * m);
        }
        else
        {
            tmp11 = 0.0f;
        }
        return tmp11;
    }
    float s = cellLightPhys.z;
    float tmp12 = a;
    float tmp13 = s;
    float raw = cellPhysRaw(tmp12, tmp13);
    float tmp14;
    if ((red & 1) != 0)
    {
        tmp14 = 1.0f;
    }
    else
    {
        float tmp15;
        if (raw > 0.0f)
        {
            tmp15 = vA / raw;
        }
        else
        {
            tmp15 = 0.0f;
        }
        tmp14 = tmp15;
    }
    float k = tmp14;
    float tmp16;
    if (k > 0.0f)
    {
        float tmp17 = x;
        float tmp18 = s;
        tmp16 = k * cellPhysRaw(tmp17, tmp18);
    }
    else
    {
        tmp16 = van;
    }
    return tmp16;
}
#endif

#if defined(WW_FN_cellRadial) || defined(WW_FN_ALL)
float cellRadial(float d, float r, float3 bse)
{
    float x = clamp(d / max(r, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
    float tmp1;
    if (bse.z > 0.0f)
    {
        tmp1 = pow(x, bse.z);
    }
    else
    {
        tmp1 = 1.0f;
    }
    float xe = tmp1;
    float k = 1.0f - clamp((bse.y * xe) + bse.x, 0.0f, 1.0f);
    if ((cellRed & 1) != 0)
    {
        return k;
    }
    float tmp2;
    if (cellLightPhys.x > 0.5f)
    {
        float tmp3 = pow(k, 2.2000000476837158203125f);
        float tmp4 = x;
        float tmp5 = r;
        float3 tmp6 = bse;
        tmp2 = cellPhysFalloff(tmp3, tmp4, tmp5, tmp6);
    }
    else
    {
        tmp2 = pow(k, 2.2000000476837158203125f);
    }
    return tmp2;
}
#endif

#if defined(WW_FN_cellShadowF) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool tmp1 = !cellShadowOn;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = t4.y < 0.5f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool tmp3 = t4.y > 1.5f;
    bool tmp4;
    if (tmp3)
    {
        tmp4 = t4.y < 2.5f;
    }
    else
    {
        tmp4 = tmp3;
    }
    bool tmp5;
    if (tmp4)
    {
        tmp5 = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        tmp5 = tmp4;
    }
    if (tmp5)
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
    bool3 tmp6 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(tmp6.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, tmp6.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, tmp6.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmp(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float tmp7;
    if (cellShadowFadeOn)
    {
        tmp7 = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        tmp7 = s / 9.0f;
    }
    return tmp7;
}
#endif

#if defined(WW_FN_cellShadowF_V2) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool tmp1 = !cellShadowOn;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = t4.y < 0.5f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool tmp3 = t4.y > 1.5f;
    bool tmp4;
    if (tmp3)
    {
        tmp4 = t4.y < 2.5f;
    }
    else
    {
        tmp4 = tmp3;
    }
    bool tmp5;
    if (tmp4)
    {
        tmp5 = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        tmp5 = tmp4;
    }
    if (tmp5)
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
    bool3 tmp6 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(tmp6.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, tmp6.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, tmp6.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmpLevelZero(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float tmp7;
    if (cellShadowFadeOn)
    {
        tmp7 = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        tmp7 = s / 9.0f;
    }
    return tmp7;
}
#endif

#if defined(WW_FN_cellContactLocal) || defined(WW_FN_ALL)
float cellContactLocal(int i, float3 P)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool tmp1 = !cellCtOn;
    bool tmp2;
    if (!tmp1)
    {
        bool tmp3;
        if (cellShadowOn)
        {
            tmp3 = t4.y > 0.5f;
        }
        else
        {
            tmp3 = cellShadowOn;
        }
        bool tmp4;
        if (tmp3)
        {
            tmp4 = t4.x > (-0.5f);
        }
        else
        {
            tmp4 = tmp3;
        }
        tmp2 = tmp4;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    float3 w = P - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
    float3 l = t0.xyz - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
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
        bool tmp5 = any(bool2(q.x < 0.0f.xx.x, q.y < 0.0f.xx.y));
        bool tmp6;
        if (!tmp5)
        {
            tmp6 = any(bool2(q.x >= cellCtRect.zw.x, q.y >= cellCtRect.zw.y));
        }
        else
        {
            tmp6 = tmp5;
        }
        if (tmp6)
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
    float3 tmp1 = P;
    int tmp2;
    int tmp3 = cellLightsBegin(tmp1, tmp2);
    int nL = tmp2;
    int bL = tmp3;
    for (int k = 0; k < nL; k++)
    {
        int tmp4 = bL;
        int tmp5 = k;
        int i = cellLightAt(tmp4, tmp5);
        float slot = cellLights.Load((i * 8) + 4).x;
        bool wwh0 = (slot < (-0.5f));
        bool wwh1 = (slot > 2.5f);
        if (wwh0 || wwh1)
        {
            continue;
        }
        float4 t0 = cellLights.Load(i * 8);
        float3 Lv = t0.xyz - P;
        float d = length(Lv);
        bool tmp6 = d >= t0.w;
        bool tmp7;
        if (!tmp6)
        {
            tmp7 = dot(N, Lv / max(d, 0.001000000047497451305389404296875f).xxx) < 0.0500000007450580596923828125f;
        }
        else
        {
            tmp7 = tmp6;
        }
        if (tmp7)
        {
            continue;
        }
        int tmp8 = i;
        float3 tmp9 = P;
        float3 tmp10 = N;
        o[int(slot + 0.5f)] = (2.0f + (cellShadowF(tmp8, tmp9, tmp10) * 253.0f)) / 255.0f;
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
    bool tmp1 = any(bool3(c.x < 0.0f.xxx.x, c.y < 0.0f.xxx.y, c.z < 0.0f.xxx.z));
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool3(c.x >= cellRoomsDims.x, c.y >= cellRoomsDims.y, c.z >= cellRoomsDims.z));
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return int2(-1, -1);
    }
    float tmp3 = cellRooms.Load(int4(int3(c), 0)).x;
    return cellRoomUnpack(tmp3);
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
    float3 tmp1 = P;
    float3 tmp2 = N;
    float4 s = cellGiSample(tmp1, tmp2);
    if (cellGiSky && false)
    {
        return max(s.xyz, 0.0f.xxx);
    }
    float3 tmp3;
    if (s.w > 0.00999999977648258209228515625f)
    {
        tmp3 = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        tmp3 = 0.0f.xxx;
    }
    return tmp3;
}
#endif

#if defined(WW_FN_cellGiE) || defined(WW_FN_ALL)
float3 cellGiE(float3 P, float3 N)
{
    float3 tmp1 = P;
    float3 tmp2 = N;
    float3 e = cellGiE0(tmp1, tmp2);
    return e;
}
#endif

#if defined(WW_FN_cellGiSkyK) || defined(WW_FN_ALL)
float cellGiSkyK(float3 P, float3 N)
{
    bool tmp1;
    if (!true)
    {
        tmp1 = !(cellGiSky && false);
    }
    else
    {
        tmp1 = true;
    }
    if (tmp1 || cellInterior)
    {
        return 0.0f;
    }
    float3 tmp2 = P;
    float3 tmp3 = N;
    return clamp(cellGiSample(tmp2, tmp3).w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellSpecSky) || defined(WW_FN_ALL)
float cellSpecSky(float3 P, float3 N, float3 R, float rough)
{
    bool tmp1 = (cellSpecOcc == 0) || true;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = !(cellGiSky && false);
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2 || cellInterior)
    {
        return 1.0f;
    }
    float r = clamp(rough, 0.0f, 1.0f);
    float3 tmp3;
    if (cellSpecOcc == 2)
    {
        tmp3 = N;
    }
    else
    {
        tmp3 = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    }
    float3 D_1 = tmp3;
    float v = 1.0f;
    float tmp4;
    if (cellSpecOcc == 2)
    {
        tmp4 = lerp(1.0f, v, r);
    }
    else
    {
        tmp4 = v;
    }
    return tmp4;
}
#endif

#if defined(WW_FN_cellAmbShare) || defined(WW_FN_ALL)
float cellAmbShare(float3 P, float3 N)
{
    if (!cellInterior)
    {
        float3 tmp1 = P;
        float3 tmp2 = N;
        return cellGiSkyK(tmp1, tmp2);
    }
    bool wwh2 = (cellGiAmb != 1);
    if ((true || (!cellHasDalc)) || wwh2)
    {
        return 0.0f;
    }
    float3 tmp3 = P;
    float3 tmp4 = N;
    return clamp(cellGiSample(tmp3, tmp4).w, 0.0f, 1.0f);
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
        float3 tmp1;
        if ((cellGoboMap & 4) != 0)
        {
            tmp1 = D_1;
        }
        else
        {
            tmp1 = float3(dot(D_1, a0.xyz), dot(D_1, cellLights.Load(r + 1).xyz), dot(D_1, cellLights.Load(r + 2).xyz));
        }
        float3 l = tmp1;
        if ((cellGoboMap & 8) != 0)
        {
            float3 tmp2;
            if ((cellGoboMap & 2) != 0)
            {
                tmp2 = l.yzx;
            }
            else
            {
                tmp2 = l;
            }
            float3 d = normalize(tmp2);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 tmp3;
            if ((cellGoboMap & 2) != 0)
            {
                tmp3 = d_1.yz;
            }
            else
            {
                tmp3 = d_1.zy;
            }
            float2 e = tmp3;
            bool tmp4 = hf == 1;
            bool tmp5;
            if (tmp4)
            {
                tmp5 = (cellGoboMap & 1) == 0;
            }
            else
            {
                tmp5 = tmp4;
            }
            if (tmp5)
            {
                e.x = -e.x;
            }
            float tmp6;
            if (hf == 1)
            {
                tmp6 = 1.0f - d_1.x;
            }
            else
            {
                tmp6 = 1.0f + d_1.x;
            }
            uv = ((e / max(tmp6, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 tmp7;
        if (cellGoboGam)
        {
            tmp7 = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            tmp7 = cellLights.Load(h + 1).xyz;
        }
        return tmp7;
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
    int2 tmp8;
    if (hf < 0)
    {
        tmp8 = sz;
    }
    else
    {
        tmp8 = int2(sz.x, sz.y / 2);
    }
    int2 pz = tmp8;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 tmp9;
    if (cellGoboGam)
    {
        tmp9 = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        tmp9 = lerp(c0, c1, f.y.xxx);
    }
    return tmp9;
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
        float3 tmp1;
        if ((cellGoboMap & 4) != 0)
        {
            tmp1 = D;
        }
        else
        {
            tmp1 = float3(dot(D, a0.xyz), dot(D, cellLights.Load(r + 1).xyz), dot(D, cellLights.Load(r + 2).xyz));
        }
        float3 l = tmp1;
        if ((cellGoboMap & 8) != 0)
        {
            float3 tmp2;
            if ((cellGoboMap & 2) != 0)
            {
                tmp2 = l.yzx;
            }
            else
            {
                tmp2 = l;
            }
            float3 d = normalize(tmp2);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 tmp3;
            if ((cellGoboMap & 2) != 0)
            {
                tmp3 = d_1.yz;
            }
            else
            {
                tmp3 = d_1.zy;
            }
            float2 e = tmp3;
            bool tmp4 = hf == 1;
            bool tmp5;
            if (tmp4)
            {
                tmp5 = (cellGoboMap & 1) == 0;
            }
            else
            {
                tmp5 = tmp4;
            }
            if (tmp5)
            {
                e.x = -e.x;
            }
            float tmp6;
            if (hf == 1)
            {
                tmp6 = 1.0f - d_1.x;
            }
            else
            {
                tmp6 = 1.0f + d_1.x;
            }
            uv = ((e / max(tmp6, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 tmp7;
        if (cellGoboGam)
        {
            tmp7 = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            tmp7 = cellLights.Load(h + 1).xyz;
        }
        return tmp7;
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
    int2 tmp8;
    if (hf < 0)
    {
        tmp8 = sz;
    }
    else
    {
        tmp8 = int2(sz.x, sz.y / 2);
    }
    int2 pz = tmp8;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 tmp9;
    if (cellGoboGam)
    {
        tmp9 = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        tmp9 = lerp(c0, c1, f.y.xxx);
    }
    return tmp9;
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
    float3 tmp1 = abs(k);
    return all(bool3(tmp1.x <= 1.0f.xxx.x, tmp1.y <= 1.0f.xxx.y, tmp1.z <= 1.0f.xxx.z));
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
    bool tmp1 = d >= t0.w;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = dot(N, L) <= 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 0.0f.xxx;
    }
    float4 t1 = cellLights.Load((i * 8) + 1);
    int tmp3 = i;
    float3 tmp4 = P;
    float3 tmp5 = t0.xyz;
    float tmp6 = t1.w;
    if (!cellShapeIn(tmp3, tmp4, tmp5, tmp6))
    {
        return 0.0f.xxx;
    }
    float4 t3 = cellLights.Load((i * 8) + 3);
    float tmp7 = d;
    float tmp8 = t0.w;
    float3 tmp9 = t3.xyz;
    float a = cellRadial(tmp7, tmp8, tmp9);
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
        int tmp10 = i;
        float3 tmp11 = -Lv;
        float3 tmp12 = gAx;
        bool tmp13 = t1.w > (-1.5f);
        g = cellGobo(tmp10, tmp11, tmp12, tmp13);
    }
    if (a > 0.0f)
    {
        int tmp14 = i;
        float3 tmp15 = P;
        float3 tmp16 = N;
        a *= cellShadowF(tmp14, tmp15, tmp16);
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
                int3 tmp1 = k;
                int tmp2 = side;
                uint h = cellFarHash(tmp1, tmp2) & mask;
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
    float3 tmp3;
    if (ws > 0.0f)
    {
        tmp3 = s / ws.xxx;
    }
    else
    {
        tmp3 = 0.0f.xxx;
    }
    return tmp3;
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
    float3 tmp1;
    if (s.w > 0.00999999977648258209228515625f)
    {
        tmp1 = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        tmp1 = 0.0f.xxx;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_cellFarTerms) || defined(WW_FN_ALL)
float cellFarTerms(float3 P, float3 N, out float3 F, out float3 gP)
{
    F = 0.0f.xxx;
    gP = 0.0f.xxx;
    float3 tmp1 = P;
    float w = cellFarW(tmp1);
    if (w <= 0.0f)
    {
        return 0.0f;
    }
    if ((cellFarRed & 1) == 0)
    {
        float3 tmp2 = P;
        float3 tmp3 = N;
        float3 tmp4 = cellFarF(tmp2, tmp3);
        F = tmp4;
    }
    float3 tmp5 = P;
    float3 tmp6 = N;
    gP = cellFarPlacedE(tmp5, tmp6) * 0.31830990314483642578125f;
    return w;
}
#endif

#if defined(WW_FN_cellFarProbeOut) || defined(WW_FN_ALL)
float3 cellFarProbeOut(float3 P, float3 N)
{
    float3 tmp1 = P;
    float w = cellFarW(tmp1);
    if (cellProbe == 93)
    {
        return w.xxx;
    }
    if (cellProbe == 91)
    {
        float3 tmp2;
        if (cellFarOn)
        {
            float3 tmp3 = P;
            float3 tmp4 = N;
            float3 tmp5 = cellFarF(tmp3, tmp4);
            tmp2 = tmp5;
        }
        else
        {
            tmp2 = 0.0f.xxx;
        }
        return sqrt(clamp(tmp2 * 0.25f, 0.0f.xxx, 1.0f.xxx));
    }
    if (cellProbe == 94)
    {
        bool tmp6 = w > 0.0f;
        bool tmp7;
        if (tmp6)
        {
            tmp7 = (cellFarRed & 1) == 0;
        }
        else
        {
            tmp7 = tmp6;
        }
        if (tmp7)
        {
            float3 tmp8 = P;
            float3 tmp9 = N;
            float3 tmp10 = cellFarF(tmp8, tmp9);
        }
        float3 tmp11 = P;
        int tmp12;
        int tmp13 = cellLightsBegin(tmp11, tmp12);
        int nW = tmp12;
        return (float(((w >= 1.0f) ? 0 : nW) + cellFarIters) / 256.0f).xxx;
    }
    float3 E = 0.0f.xxx;
    float3 tmp14 = P;
    int tmp15;
    int tmp16 = cellLightsBegin(tmp14, tmp15);
    int nL = tmp15;
    int bL = tmp16;
    float3 tmp17;
    bool tmp18;
    for (int k = 0; k < nL; k++)
    {
        int tmp19 = bL;
        int tmp20 = k;
        int i = cellLightAt(tmp19, tmp20);
        int tmp21 = i;
        float3 tmp22 = P;
        float3 tmp23 = N;
        float3 tmp24 = cellLightE(tmp21, tmp22, tmp23, tmp17, tmp18);
        float3 L = tmp17;
        bool ns = tmp18;
        float3 c = tmp24;
        E += (c * max(dot(N, L), 0.0f));
    }
    float3 tmp25 = P;
    float3 tmp26 = N;
    E += (cellFarPlacedE(tmp25, tmp26) * 0.31830990314483642578125f);
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
    float tmp1;
    if ((cellSsgiRed & 2) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = lerp(1.0f, clamp(s.w, 0.0f, 1.0f), cellSsgiK.x);
    }
    float keep = tmp1;
    return (eProbe * keep) + (max(s.xyz, 0.0f.xxx) * cellSsgiK.y);
}
#endif

#if defined(WW_FN_cellProbeRaw) || defined(WW_FN_ALL)
float3 cellProbeRaw(float3 P, float3 N)
{
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 tmp1 = P;
        float3 tmp2 = N;
        float3 tmp3 = cellFarProbeOut(tmp1, tmp2);
        return tmp3;
    }
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    if (cellProbe == 5)
    {
        float3 tmp4;
        if (false)
        {
            float3 tmp5 = P;
            float3 tmp6 = N;
            tmp4 = cellGiE(tmp5, tmp6);
        }
        else
        {
            tmp4 = 0.0f.xxx;
        }
        float3 tmp7 = tmp4;
        return clamp(cellSsgiE(tmp7) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 90)
    {
        float3 tmp8 = P;
        float3 tmp9 = N;
        return cellAmbShare(tmp8, tmp9).xxx;
    }
    if (cellProbe == 95)
    {
        float4 tmp10;
        if (false)
        {
            float3 tmp11 = P;
            float3 tmp12 = N;
            tmp10 = cellGiSample(tmp11, tmp12);
        }
        else
        {
            tmp10 = 0.0f.xxxx;
        }
        float4 s = tmp10;
        return float3(float(cellGiPath) * 0.0625f, clamp(s.w, 0.0f, 1.0f), sqrt(clamp(dot(max(s.xyz, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)) * 0.31830990314483642578125f, 0.0f, 1.0f)));
    }
    if (cellProbe == 1)
    {
        float3 E = 0.0f.xxx;
        float3 tmp13 = P;
        int tmp14;
        int tmp15 = cellLightsBegin(tmp13, tmp14);
        int nL = tmp14;
        int bL = tmp15;
        float3 tmp16;
        bool tmp17;
        for (int k = 0; k < nL; k++)
        {
            int tmp18 = bL;
            int tmp19 = k;
            int i = cellLightAt(tmp18, tmp19);
            int tmp20 = i;
            float3 tmp21 = P;
            float3 tmp22 = N;
            float3 tmp23 = cellLightE(tmp20, tmp21, tmp22, tmp16, tmp17);
            float3 L = tmp16;
            bool ns = tmp17;
            float3 c = tmp23;
            E += (c * max(dot(N, L), 0.0f));
        }
        return clamp(E * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 7)
    {
        float3 tmp24 = P;
        float3 tmp25 = N;
        return cellShadowProbe(tmp24, tmp25);
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
    if (wwh3 || true)
    {
        bool3 tmp1 = (cellPass == 6).xxx;
        return float3(tmp1.x ? float3(1.0f, 0.0f, 1.0f).x : float3(0.0f, 1.0f, 1.0f).x, tmp1.y ? float3(1.0f, 0.0f, 1.0f).y : float3(0.0f, 1.0f, 1.0f).y, tmp1.z ? float3(1.0f, 0.0f, 1.0f).z : float3(0.0f, 1.0f, 1.0f).z);
    }
    bool tmp2 = cellPass != 6;
    bool tmp3;
    if (tmp2)
    {
        tmp3 = (cellPassRed & 2) != 0;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool3 tmp4 = tmp3.xxx;
    float3 tmp5 = P;
    float3 tmp6 = float3(tmp4.x ? float3(0.0f, 0.0f, 1.0f).x : N.x, tmp4.y ? float3(0.0f, 0.0f, 1.0f).y : N.y, tmp4.z ? float3(0.0f, 0.0f, 1.0f).z : N.z);
    float4 s = cellGiSample(tmp5, tmp6);
    if (cellPass == 6)
    {
        float3 tmp7;
        if (s.w <= 0.00999999977648258209228515625f)
        {
            tmp7 = float3(1.0f, 0.0f, 1.0f);
        }
        else
        {
            tmp7 = (0.25f + (0.5f * clamp(s.w, 0.0f, 1.0f))).xxx;
        }
        return tmp7;
    }
    if (s.w <= 0.00999999977648258209228515625f)
    {
        return float3(0.0f, 1.0f, 1.0f);
    }
    float3 v = max(s.xyz / s.w.xxx, 0.0f.xxx);
    if (cellPass == 1)
    {
        v *= 0.31830990314483642578125f;
        if ((cellPassRed & 1) != 0)
        {
            float3 tmp8 = P;
            int tmp9;
            int tmp10 = cellLightsBegin(tmp8, tmp9);
            int nL = tmp9;
            int bL = tmp10;
            float3 tmp11;
            bool tmp12;
            for (int k = 0; k < nL; k++)
            {
                int tmp13 = bL;
                int tmp14 = k;
                int i = cellLightAt(tmp13, tmp14);
                int tmp15 = i;
                float3 tmp16 = P;
                float3 tmp17 = N;
                float3 tmp18 = cellLightE(tmp15, tmp16, tmp17, tmp11, tmp12);
                float3 L = tmp11;
                bool ns = tmp12;
                float3 c = tmp18;
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
    float tmp1;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float tmp2;
        if (NdotV == m)
        {
            tmp2 = 1.0f;
        }
        else
        {
            tmp2 = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        tmp1 = ((2.0f * NdotH) * tmp2) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        tmp1 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = tmp1;
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
    float tmp1;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float tmp2;
        if (NdotV == m)
        {
            tmp2 = 1.0f;
        }
        else
        {
            tmp2 = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        tmp1 = ((2.0f * NdotH) * tmp2) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        tmp1 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = tmp1;
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
    float tmp1;
    if (cellFarFade)
    {
        float3 tmp2 = P;
        tmp1 = cellFarW(tmp2);
    }
    else
    {
        tmp1 = 0.0f;
    }
    float fw = tmp1;
    if (fw >= 1.0f)
    {
        return;
    }
    float3 tmp3 = P;
    int tmp4;
    int tmp5 = cellLightsBegin(tmp3, tmp4);
    int nL = tmp4;
    int bL = tmp5;
    float tmp6;
    float tmp7;
    for (int k = 0; k < nL; k++)
    {
        int tmp8 = bL;
        int tmp9 = k;
        int i = cellLightAt(tmp8, tmp9);
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
        int tmp10 = i;
        float3 tmp11 = P;
        float3 tmp12 = t0.xyz;
        float tmp13 = t1.w;
        if (!cellShapeIn(tmp10, tmp11, tmp12, tmp13))
        {
            continue;
        }
        float4 t3 = cellLights.Load((i * 8) + 3);
        float tmp14 = d;
        float tmp15 = t0.w;
        float3 tmp16 = t3.xyz;
        float a = cellRadial(tmp14, tmp15, tmp16);
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
            int tmp17 = i;
            float3 tmp18 = -Lv;
            float3 tmp19 = gAx;
            bool tmp20 = t1.w > (-1.5f);
            g = cellGobo(tmp17, tmp18, tmp19, tmp20);
        }
        if (a > 0.0f)
        {
            int tmp21 = i;
            float3 tmp22 = P;
            float3 tmp23 = N;
            int tmp24 = i;
            float3 tmp25 = P;
            a *= (cellShadowF(tmp21, tmp22, tmp23) * cellContactLocal(tmp24, tmp25));
        }
        float3 E = ((t1.xyz * g) * a) * NdotL;
        diff += E;
        int fl = int(t3.w + 0.5f) & (((cellRed & 64) != 0) ? 1 : 7);
        if ((fl & 6) != 0)
        {
            tmp6 = 0.0f;
        }
        else
        {
            float3 tmp26 = N;
            float3 tmp27 = L;
            float3 tmp28 = Vw;
            float tmp29 = gloss;
            tmp6 = cellRim(tmp26, tmp27, tmp28, tmp29);
        }
        float rim = tmp6;
        if ((fl & 4) != 0)
        {
            tmp7 = 1.0f;
        }
        else
        {
            float3 tmp30 = N;
            float3 tmp31 = L;
            float3 tmp32 = Vw;
            float tmp33 = NdotL;
            float tmp34 = gloss;
            tmp7 = cellOren(tmp30, tmp31, tmp32, tmp33, tmp34);
        }
        diffOn += (E * (tmp7 + rim));
        cellRimSum += (E * rim);
        bool tmp35 = (fl & 1) == 0;
        bool tmp36;
        if (!tmp35)
        {
            tmp36 = (cellRed & 65536) != 0;
        }
        else
        {
            tmp36 = tmp35;
        }
        if (tmp36)
        {
            float3 tmp37 = N;
            float3 tmp38 = L;
            float3 tmp39 = Vw;
            float tmp40 = gloss;
            spec += (E * cellSpecGame(tmp37, tmp38, tmp39, tmp40));
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
        if (cellAmboK[i].w > 0.5f)
        {
            float4 b0 = cellAmboBox[i * 3];
            float4 b1 = cellAmboBox[(i * 3) + 1];
            float4 b2 = cellAmboBox[(i * 3) + 2];
            float3 k = float3(dot(b0.xyz, P) + b0.w, dot(b1.xyz, P) + b1.w, dot(b2.xyz, P) + b2.w);
            float3 tmp1 = abs(k);
            if (all(bool3(tmp1.x < 1.0f.xxx.x, tmp1.y < 1.0f.xxx.y, tmp1.z < 1.0f.xxx.z)))
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
    float3 tmp1 = P;
    return float3(dot(cellDalc[0], n1), dot(cellDalc[1], n1), dot(cellDalc[2], n1)) * cellAmboScale(tmp1);
}
#endif

#if defined(WW_FN_cellAmbient) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 tmp1 = N;
    float3 tmp2 = P;
    return pow(max(cellAmbientSum(tmp1, tmp2), 0.0f.xxx), 2.2000000476837158203125f.xxx);
}
#endif

#if defined(WW_FN_cellAmbient_V2) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 tmp1 = N;
    float3 tmp2 = P;
    return pow(max(cellAmbientSum(tmp1, tmp2), 0.0f.xxx), 2.2000000476837158203125f.xxx) * fxAmbShare;
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
    if (wwh4 || true)
    {
        return amb;
    }
    if (cellGiAmb == 1)
    {
        float3 tmp1 = P;
        float3 tmp2 = N;
        return amb * (1.0f - clamp(cellGiSample(tmp1, tmp2).w, 0.0f, 1.0f));
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
    bool tmp1 = abs(cellCubeMat.z - 1.0f) < 0.5f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = abs(cellCubeMat.z - 3.0f) < 0.5f;
    }
    else
    {
        tmp2 = tmp1;
    }
    return tmp2;
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
    float depth = abs(posView.z) * length(cellRow[0].xyz);
    float lod = (((1.0f - g) * 6.0f) + (depth * 0.00195299996994435787200927734375f)) + log2(float(int2(spvTextureSize(cube, uint(0), dummy_parameter)).x) / 128.0f);
    float3 c = cube.SampleLevel(_cube_sampler, dir, max(lod, 0.0f)).xyz;
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
    float3 tmp1 = normalView;
    float3 n = ((cellWorldDir(tmp1) * 0.5f) + 0.5f.xxx) * 255.0f;
    return (n - floor(n + 0.5f.xxx)) + 0.5f.xxx;
}
#endif

#if defined(WW_FN_cellLit) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 tmp1 = posView;
    float3 P = cellWorldPos(tmp1);
    float3 tmp2 = normalView;
    float3 N = cellWorldDir(tmp2);
    float3 tmp3 = Vview;
    float3 Vw = cellWorldDir(tmp3);
    float gloss = 1.0f - sqrt(alphaR);
    float3 tmp4 = P;
    float3 tmp5 = N;
    float3 tmp6 = Vw;
    float tmp7 = gloss;
    float3 tmp8;
    float3 tmp9;
    float3 tmp10;
    cellSumLights(tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10);
    float3 diff = tmp8;
    float3 diffOn = tmp9;
    float3 spec = tmp10;
    float3 alb = albedo * albedo;
    float3 tmp11;
    if (false)
    {
        float3 tmp12 = P;
        float3 tmp13 = N;
        tmp11 = cellGiE(tmp12, tmp13);
    }
    else
    {
        tmp11 = 0.0f.xxx;
    }
    float3 tmp14 = tmp11;
    float3 gi = cellSsgiE(tmp14) * 0.31830990314483642578125f;
    float3 tmp15 = P;
    float3 tmp16 = N;
    float3 tmp17;
    float3 tmp18;
    float tmp19 = cellFarTerms(tmp15, tmp16, tmp17, tmp18);
    float3 farF = tmp17;
    float3 farGP = tmp18;
    float farW = tmp19;
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
        float3 tmp20 = N;
        float3 tmp21 = Ld;
        float3 tmp22 = Vw;
        float tmp23 = nl;
        float tmp24 = gloss;
        float3 tmp25 = N;
        float3 tmp26 = Ld;
        float3 tmp27 = Vw;
        float tmp28 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(tmp20, tmp21, tmp22, tmp23, tmp24) + cellRim(tmp25, tmp26, tmp27, tmp28)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 tmp29;
        if ((cellRed & 512) != 0)
        {
            tmp29 = Ex;
        }
        else
        {
            tmp29 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * tmp29)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 tmp30 = N;
        float3 tmp31 = P;
        float3 amb = cellAmbient(tmp30, tmp31);
        if (cellGiAmb != 0)
        {
            float3 tmp32 = amb;
            float3 tmp33 = gi;
            float3 tmp34 = P;
            float3 tmp35 = N;
            amb = cellAmbGi(tmp32, tmp33, tmp34, tmp35);
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
        float3 tmp36 = N;
        float3 tmp37 = Ld_1;
        float3 tmp38 = Vw;
        float tmp39 = nl_1;
        float tmp40 = gloss;
        float3 tmp41 = N;
        float3 tmp42 = Ld_1;
        float3 tmp43 = Vw;
        float tmp44 = gloss;
        Ed += ((dir_1 * (cellOren(tmp36, tmp37, tmp38, tmp39, tmp40) + cellRim(tmp41, tmp42, tmp43, tmp44))) + (cellDirColor * cellBackK));
    }
    bool3 tmp45 = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(tmp45.x ? E.x : Ed.x, tmp45.y ? E.y : Ed.y, tmp45.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellLit_V2) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 tmp1 = posView;
    float3 P = cellWorldPos(tmp1);
    float3 tmp2 = normalView;
    float3 N = cellWorldDir(tmp2);
    float3 tmp3 = Vview;
    float3 Vw = cellWorldDir(tmp3);
    float gloss = 1.0f - sqrt(alphaR);
    float3 tmp4 = P;
    float3 tmp5 = N;
    float3 tmp6 = Vw;
    float tmp7 = gloss;
    float3 tmp8;
    float3 tmp9;
    float3 tmp10;
    cellSumLights(tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10);
    float3 diff = tmp8;
    float3 diffOn = tmp9;
    float3 spec = tmp10;
    float3 alb = albedo * albedo;
    float3 tmp11;
    if (false)
    {
        float3 tmp12 = P;
        float3 tmp13 = N;
        tmp11 = cellGiE(tmp12, tmp13);
    }
    else
    {
        tmp11 = 0.0f.xxx;
    }
    float3 tmp14 = tmp11;
    float3 gi = cellSsgiE(tmp14) * 0.31830990314483642578125f;
    float3 tmp15 = P;
    float3 tmp16 = N;
    float3 tmp17;
    float3 tmp18;
    float tmp19 = cellFarTerms(tmp15, tmp16, tmp17, tmp18);
    float3 farF = tmp17;
    float3 farGP = tmp18;
    float farW = tmp19;
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
        float3 tmp20 = N;
        float3 tmp21 = Ld;
        float3 tmp22 = Vw;
        float tmp23 = nl;
        float tmp24 = gloss;
        float3 tmp25 = N;
        float3 tmp26 = Ld;
        float3 tmp27 = Vw;
        float tmp28 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(tmp20, tmp21, tmp22, tmp23, tmp24) + cellRim(tmp25, tmp26, tmp27, tmp28)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 tmp29;
        float tmp30;
        ussTerrainDim(tmp29, tmp30);
        ambDim = tmp29;
        specDim = tmp30;
        float3 tmp31;
        if ((cellRed & 512) != 0)
        {
            tmp31 = Ex;
        }
        else
        {
            tmp31 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * tmp31)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 tmp32 = N;
        float3 tmp33 = P;
        float3 amb = cellAmbient(tmp32, tmp33);
        if (cellGiAmb != 0)
        {
            float3 tmp34 = amb;
            float3 tmp35 = gi;
            float3 tmp36 = P;
            float3 tmp37 = N;
            amb = cellAmbGi(tmp34, tmp35, tmp36, tmp37);
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
        float3 tmp38 = N;
        float3 tmp39 = Ld_1;
        float3 tmp40 = Vw;
        float tmp41 = nl_1;
        float tmp42 = gloss;
        float3 tmp43 = N;
        float3 tmp44 = Ld_1;
        float3 tmp45 = Vw;
        float tmp46 = gloss;
        Ed += ((dir_1 * (cellOren(tmp38, tmp39, tmp40, tmp41, tmp42) + cellRim(tmp43, tmp44, tmp45, tmp46))) + (cellDirColor * cellBackK));
    }
    bool3 tmp47 = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(tmp47.x ? E.x : Ed.x, tmp47.y ? E.y : Ed.y, tmp47.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellProbeOut) || defined(WW_FN_ALL)
float3 cellProbeOut(float3 normalView, float3 posView, float alphaR, float kSmith)
{
    float3 tmp1 = posView;
    float3 P = cellWorldPos(tmp1);
    float3 tmp2 = normalView;
    float3 N = cellWorldDir(tmp2);
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    if (cellProbe == 5)
    {
        float3 tmp3;
        if (false)
        {
            float3 tmp4 = P;
            float3 tmp5 = N;
            tmp3 = cellGiE(tmp4, tmp5);
        }
        else
        {
            tmp3 = 0.0f.xxx;
        }
        float3 tmp6 = tmp3;
        return clamp(cellSsgiE(tmp6) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 90)
    {
        float3 tmp7 = P;
        float3 tmp8 = N;
        return cellAmbShare(tmp7, tmp8).xxx;
    }
    if (cellProbe == 95)
    {
        float3 tmp9 = P;
        float3 tmp10 = N;
        float3 tmp11 = cellProbeRaw(tmp9, tmp10);
        return tmp11;
    }
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 tmp12 = P;
        float3 tmp13 = N;
        float3 tmp14 = cellFarProbeOut(tmp12, tmp13);
        return tmp14;
    }
    bool wwh5 = ((((cellProbe == 1) || (cellProbe == 8)) || (cellProbe == 10)) || (cellProbe == 30));
    bool wwh6 = (cellProbe == 31);
    if (wwh5 || wwh6)
    {
        float3 tmp15;
        if (cellProbe == 1)
        {
            tmp15 = N;
        }
        else
        {
            float3 tmp16 = -posView;
            tmp15 = cellWorldDir(tmp16);
        }
        float3 tmp17 = P;
        float3 tmp18 = N;
        float3 tmp19 = tmp15;
        float tmp20 = 1.0f - sqrt(alphaR);
        float3 tmp21;
        float3 tmp22;
        float3 tmp23;
        cellSumLights(tmp17, tmp18, tmp19, tmp20, tmp21, tmp22, tmp23);
        float3 diff = tmp21;
        float3 diffOn = tmp22;
        float3 spec = tmp23;
        if (cellProbe == 10)
        {
            return clamp(cellRimSum * 4.0f, 0.0f.xxx, 1.0f.xxx);
        }
        if (cellProbe == 30)
        {
            return clamp(spec * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        if (cellProbe == 31)
        {
            return clamp((spec * cellSpecTint) * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        bool3 tmp24 = (cellProbe == 8).xxx;
        return clamp(float3(tmp24.x ? diffOn.x : diff.x, tmp24.y ? diffOn.y : diff.y, tmp24.z ? diffOn.z : diff.z) * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 9)
    {
        return float3(1.0f - sqrt(alphaR), 0.0f, 0.0f);
    }
    if (cellProbe == 11)
    {
        float3 tmp25;
        if (cellHasDalc)
        {
            float3 tmp26 = N;
            float3 tmp27 = P;
            tmp25 = clamp(cellAmbientSum(tmp26, tmp27) * 8.0f, 0.0f.xxx, 1.0f.xxx);
        }
        else
        {
            tmp25 = 0.0f.xxx;
        }
        return tmp25;
    }
    if (cellProbe == 7)
    {
        float3 tmp28 = P;
        float3 tmp29 = N;
        return cellShadowProbe(tmp28, tmp29);
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
