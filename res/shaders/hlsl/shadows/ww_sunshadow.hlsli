// Shared HLSL for res/shaders/features/shadows/ww_sunshadow.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_reflrelight_ps.hlsl, cell_volair_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_csm_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_fogcsm_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl, lookdev_ground_csm_ps.hlsl, particles_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl
// Not here (declarations, or no converted entry carried them): csmFiltered, ussCloudDvInfo, wwSunShadowProbe

#if defined(WW_FN_csmUvd) || defined(WW_FN_ALL)
float3 csmUvd(int i, float3 posView)
{
    return mul(float4(posView, 1.0f), csmMat[i]).xyz;
}
#endif

#if defined(WW_FN_csmNormalOff) || defined(WW_FN_ALL)
float3 csmNormalOff(float3 posView, float3 dPx, float3 dPy)
{
    float3 n = cross(dPx, dPy);
    float ln = length(n);
    float3 lv = float3(csmMat[0][0].z, csmMat[0][1].z, csmMat[0][2].z);
    bool cond2 = ln <= 9.9999996826552253889678874634872e-21f;
    bool cond;
    if (!cond2)
    {
        cond = dot(lv, lv) <= 0.0f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0.0f.xxx;
    }
    float3 nPick;
    if (dot(n, posView) > 0.0f)
    {
        nPick = (-n) / ln.xxx;
    }
    else
    {
        nPick = n / ln.xxx;
    }
    n = nPick;
    float c = dot(n, normalize(lv));
    return n * ((ussCsmStable.w * sqrt(max(1.0f - (c * c), 0.0f))) / csmParams.x);
}
#endif

#if defined(WW_FN_csmHard) || defined(WW_FN_ALL)
float csmHard(int i, float3 posView, float offsetWorld)
{
    int cond2 = i;
    float3 cond = posView + (csmNOff * ussCsmTexel[i]);
    // lane SHADOWSHIMMER2: the Off tier's stable bias (csmNOff 0 and CSM1's offset with the red)
    float3 q = csmUvd(cond2, cond);
    float nPick;
    if (ussCsmStable.w != 0.0f)
    {
        nPick = ussCsmBias[i];
    }
    else
    {
        nPick = offsetWorld;
    }
    float off = nPick;
    float4 csmHardRet = float4(q.xy, float(i), q.z - (off * csmInvRange[i]));
    return csmMap.SampleCmp(_csmMap_sampler, csmHardRet.xyz, csmHardRet.w);
}
#endif

#if defined(WW_FN_csmSelect) || defined(WW_FN_ALL)
void csmSelect(float3 posView, out int a, out int b, out float t)
{
    float dv = (-posView.z) * csmParams.x;
    float bound;
    if (dv < csmSplitOffset.y)
    {
        a = 0;
        b = 1;
        bound = csmSplitOffset.x;
    }
    else
    {
        a = 1;
        b = 2;
        bound = csmSplitOffset.y;
    }
    float x = clamp((dv - bound) / csmParams.w, 0.0f, 1.0f);
    float cond2;
    if ((csmRed & 1) != 0)
    {
        cond2 = step(0.5f, x);
    }
    else
    {
        cond2 = smoothstep(0.0f, 1.0f, x);
    }
    t = cond2;
}
#endif

#if defined(WW_FN_csmBlended) || defined(WW_FN_ALL)
float csmBlended(float3 posView)
{
    float3 cond2 = posView;
    int cond;
    int nPick;
    float csmHardRet;
    csmSelect(cond2, cond, nPick, csmHardRet);
    int a = cond;
    int b = nPick;
    float t = csmHardRet;
    float sAPick;
    if (t < 1.0f)
    {
        int sAPart3 = a;
        float3 sAPart2 = posView;
        float sAPart = csmSplitOffset.z;
        sAPick = csmFiltered(sAPart3, sAPart2, sAPart);
    }
    else
    {
        sAPick = 1.0f;
    }
    float sA = sAPick;
    float sBPick;
    if (t > 0.0f)
    {
        int sBPart3 = b;
        float3 sBPart2 = posView;
        float sBPart = csmSplitOffset.w;
        sBPick = csmFiltered(sBPart3, sBPart2, sBPart);
    }
    else
    {
        sBPick = 1.0f;
    }
    float sB = sBPick;
    return lerp(sA, sB, t);
}
#endif

#if defined(WW_FN_ussIgn) || defined(WW_FN_ALL)
float ussIgn(float2 p)
{
    return frac(52.98291778564453125f * frac(dot(p, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f))));
}
#endif

#if defined(WW_FN_ussVogel) || defined(WW_FN_ALL)
float ussVogel(int i, float3 posView, float offsetWorld, float3 dPx, float3 dPy, float3 nOff)
{
    int cond2 = i;
    float3 cond = posView + (nOff * ussCsmTexel[i]);
    float3 q = csmUvd(cond2, cond);
    float zRef = q.z - (offsetWorld * csmInvRange[i]);
    float2 rpdb = 0.0f.xx;
    if (ussCsmFilter.w > 0.5f)
    {
        float3 qx = mul(dPx, float3x3(csmMat[i][0].xyz, csmMat[i][1].xyz, csmMat[i][2].xyz));
        float3 qy = mul(dPy, float3x3(csmMat[i][0].xyz, csmMat[i][1].xyz, csmMat[i][2].xyz));
        float det = (qx.x * qy.y) - (qx.y * qy.x);
        if (abs(det) > 1.0000000195414813782625560981111e-24f)
        {
            rpdb = float2((qx.z * qy.y) - (qx.y * qy.z), (qx.x * qy.z) - (qy.x * qx.z)) / det.xx;
        }
    }
    float R = ussCsmFilter.y * lerp(0.800000011920928955078125f, 1.2000000476837158203125f, clamp(float(i) * 0.5f, 0.0f, 1.0f));
    int n = max(int(ussCsmFilter.z), 1);
    if (ussCsmWorld.x > 0.5f)
    {
        R = clamp(ussCsmWorld[i + 1] / max(ussCsmTexel[i], 9.9999999747524270787835121154785e-07f), 0.5f, 16.0f);
        n = int(clamp(floor(((32.0f * (R / 3.0f)) * (R / 3.0f)) + 0.5f), 8.0f, 48.0f));
    }
    float2 nPick;
    if (ussCsmStable.x > 0.5f)
    {
        nPick = mod(floor(q.xy * (csmParams.z * 4.0f)) + float2(ussCsmCellX[i], ussCsmCellY[i]), 4096.0f.xx);
    }
    else
    {
        nPick = gl_FragCoord.xy + (float2(32.66500091552734375f, 11.81499958038330078125f) * ussCsmPhase);
    }
    float2 rp = nPick;
    float2 csmHardRet = rp;
    float rot = 6.283185482025146484375f * ussIgn(csmHardRet);
    float k = 1.0f / csmParams.z;
    float lim = 2000.0f * csmInvRange[i];
    float sAPick;
    if (ussCsmFilter.w > 1.5f)
    {
        sAPick = lim;
    }
    else
    {
        sAPick = (4.0f * ussCsmTexel[i]) * csmInvRange[i];
    }
    float cap = sAPick;
    float sum = 0.0f;
    float sAPart3;
    for (int t = 0; t < 48; t++)
    {
        if (t >= n)
        {
            break;
        }
        float r = sqrt((float(t) + 0.5f) / float(n)) * R;
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * (r * k);
        float dz = dot(rpdb, o);
        if (ussCsmFilter.w > 1.5f)
        {
            sAPart3 = 1.0f;
        }
        else
        {
            sAPart3 = min(1.0f, cap / max(abs(dz), 1.0000000031710768509710513471353e-30f));
        }
        float pull = sAPart3;
        float4 sAPart2 = float4(q.xy + (o * pull), float(i), zRef + clamp(dz * pull, -lim, lim));
        sum += csmMap.SampleCmp(_csmMap_sampler, sAPart2.xyz, sAPart2.w);
    }
    return sum / float(n);
}
#endif

#if defined(WW_FN_ussVogel_V2) || defined(WW_FN_ALL)
float ussVogel(int i, float3 posView, float offsetWorld, float3 dPx, float3 dPy, float3 nOff)
{
    int cond2 = i;
    float3 cond = posView + (nOff * ussCsmTexel[i]);
    float3 q = csmUvd(cond2, cond);
    float zRef = q.z - (offsetWorld * csmInvRange[i]);
    float2 rpdb = 0.0f.xx;
    if (ussCsmFilter.w > 0.5f)
    {
        float3 qx = mul(dPx, float3x3(csmMat[i][0].xyz, csmMat[i][1].xyz, csmMat[i][2].xyz));
        float3 qy = mul(dPy, float3x3(csmMat[i][0].xyz, csmMat[i][1].xyz, csmMat[i][2].xyz));
        float det = (qx.x * qy.y) - (qx.y * qy.x);
        if (abs(det) > 1.0000000195414813782625560981111e-24f)
        {
            rpdb = float2((qx.z * qy.y) - (qx.y * qy.z), (qx.x * qy.z) - (qy.x * qx.z)) / det.xx;
        }
    }
    float R = ussCsmFilter.y * lerp(0.800000011920928955078125f, 1.2000000476837158203125f, clamp(float(i) * 0.5f, 0.0f, 1.0f));
    int n = max(int(ussCsmFilter.z), 1);
    if (ussCsmWorld.x > 0.5f)
    {
        R = clamp(ussCsmWorld[i + 1] / max(ussCsmTexel[i], 9.9999999747524270787835121154785e-07f), 0.5f, 16.0f);
        n = int(clamp(floor(((32.0f * (R / 3.0f)) * (R / 3.0f)) + 0.5f), 8.0f, 48.0f));
    }
    float2 nPick;
    if (ussCsmStable.x > 0.5f)
    {
        nPick = mod(floor(q.xy * (csmParams.z * 4.0f)) + float2(ussCsmCellX[i], ussCsmCellY[i]), 4096.0f.xx);
    }
    else
    {
        nPick = pcFragCoord.xy + (float2(32.66500091552734375f, 11.81499958038330078125f) * ussCsmPhase);
    }
    float2 rp = nPick;
    float2 csmHardRet = rp;
    float rot = 6.283185482025146484375f * ussIgn(csmHardRet);
    float k = 1.0f / csmParams.z;
    float lim = 2000.0f * csmInvRange[i];
    float sAPick;
    if (ussCsmFilter.w > 1.5f)
    {
        sAPick = lim;
    }
    else
    {
        sAPick = (4.0f * ussCsmTexel[i]) * csmInvRange[i];
    }
    float cap = sAPick;
    float sum = 0.0f;
    float sAPart3;
    for (int t = 0; t < 48; t++)
    {
        if (t >= n)
        {
            break;
        }
        float r = sqrt((float(t) + 0.5f) / float(n)) * R;
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * (r * k);
        float dz = dot(rpdb, o);
        if (ussCsmFilter.w > 1.5f)
        {
            sAPart3 = 1.0f;
        }
        else
        {
            sAPart3 = min(1.0f, cap / max(abs(dz), 1.0000000031710768509710513471353e-30f));
        }
        float pull = sAPart3;
        float4 sAPart2 = float4(q.xy + (o * pull), float(i), zRef + clamp(dz * pull, -lim, lim));
        sum += csmMap.SampleCmpLevelZero(_csmMap_sampler, sAPart2.xyz, sAPart2.w);
    }
    return sum / float(n);
}
#endif

#if defined(WW_FN_ussCascades) || defined(WW_FN_ALL)
float ussCascades(float3 posView, float3 dPx, float3 dPy, out float w)
{
    w = 0.0f;
    if (ussCsmOn == 0)
    {
        return 1.0f;
    }
    float dv = (-posView.z) * csmParams.x;
    if (dv > (csmParams.y + csmParams.w))
    {
        return 1.0f;
    }
    float3 cond2 = posView;
    int cond;
    int nPick;
    float csmHardRet;
    csmSelect(cond2, cond, nPick, csmHardRet);
    int a = cond;
    int b = nPick;
    float t = csmHardRet;
    float sA = 1.0f;
    float sB = 1.0f;
    if (ussCsmFilter.x > 0.5f)
    {
        // lane SHADOWSHIMMER1: the normal offset, ussCsmStable.y texels of the cascade x sin(the light's angle to the
        // surface), along the geometric normal (the position's derivatives) turned toward the eye; once for both cascades
        float3 nOff = 0.0f.xxx;
        if (ussCsmStable.y > 0.0f)
        {
            float3 n = cross(dPx, dPy);
            float ln = length(n);
            float3 lv = float3(csmMat[a][0].z, csmMat[a][1].z, csmMat[a][2].z);
            bool sAPick = ln > 9.9999996826552253889678874634872e-21f;
            bool sAPart3;
            if (sAPick)
            {
                sAPart3 = dot(lv, lv) > 0.0f;
            }
            else
            {
                sAPart3 = sAPick;
            }
            if (sAPart3)
            {
                float3 sAPart2;
                if (dot(n, posView) > 0.0f)
                {
                    sAPart2 = (-n) / ln.xxx;
                }
                else
                {
                    sAPart2 = n / ln.xxx;
                }
                n = sAPart2;
                float c = dot(n, normalize(lv));
                nOff = n * ((ussCsmStable.y * sqrt(max(1.0f - (c * c), 0.0f))) / csmParams.x);
            }
        }
        if (t < 1.0f)
        {
            int sAPart = a;
            float3 sBPick = posView;
            float sBPart3 = ussCsmBias[a];
            float3 sBPart2 = dPx;
            float3 sBPart = dPy;
            float3 sAPart7 = nOff;
            sA = ussVogel(sAPart, sBPick, sBPart3, sBPart2, sBPart, sAPart7);
        }
        if (t > 0.0f)
        {
            int sBPart12 = b;
            float3 sBPart11 = posView;
            float sBPart10 = ussCsmBias[b];
            float3 sBPart9 = dPx;
            float3 sBPart8 = dPy;
            float3 sBPart7 = nOff;
            sB = ussVogel(sBPart12, sBPart11, sBPart10, sBPart9, sBPart8, sBPart7);
        }
    }
    else
    {
        if (t < 1.0f)
        {
            int sAPart6 = a;
            float3 sAPart5 = posView;
            float sAPart4 = csmSplitOffset.z;
            sA = csmFiltered(sAPart6, sAPart5, sAPart4);
        }
        if (t > 0.0f)
        {
            int sBPart6 = b;
            float3 sBPart5 = posView;
            float sBPart4 = csmSplitOffset.w;
            sB = csmFiltered(sBPart6, sBPart5, sBPart4);
        }
    }
    float s = clamp(((dot(posView, posView) * csmParams.x) * csmParams.x) / (csmParams.y * csmParams.y), 0.0f, 1.0f);
    float s2 = s * s;
    float wPick;
    if ((csmRed & 2) != 0)
    {
        wPick = 1.0f;
    }
    else
    {
        wPick = 1.0f - (s2 * s2);
    }
    w = wPick;
    return lerp(sA, sB, t);
}
#endif

#if defined(WW_FN_ussTerrAtlasUv) || defined(WW_FN_ALL)
float2 ussTerrAtlasUv(inout float2 uv, int tier)
{
    bool cond2 = ussTerrSize.y <= 0.0f;
    bool cond;
    if (cond2)
    {
        cond = ussTerrExtra.z <= 0.0f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        // one tier: the map as TERRSHADOW1 made it
        return uv;
    }
    float nPick;
    if (tier == 0)
    {
        nPick = ussTerrSize.x;
    }
    else
    {
        nPick = ussTerrSize.y;
    }
    float n = nPick;
    // no bleed across the tiers' seam
    uv = clamp(uv, (0.5f / n).xx, (1.0f - (0.5f / n)).xx);
    float2 csmHardRet;
    if (tier == 0)
    {
        csmHardRet = float2(uv.x, uv.y * ussTerrAtlas.x);
    }
    else
    {
        csmHardRet = float2(uv.x * ussTerrAtlas.y, ussTerrAtlas.z + (uv.y * ussTerrAtlas.w));
    }
    return csmHardRet;
}
#endif

#if defined(WW_FN_ussTerrVis) || defined(WW_FN_ALL)
float ussTerrVis(float2 rg, float z)
{
    float lo = ussTerrRange.x + (rg.x * ussTerrRange.y);
    float hi = ussTerrRange.x + (rg.y * ussTerrRange.y);
    float cond2;
    if (rg.x <= 0.0f)
    {
        cond2 = 1.0f;
    }
    else
    {
        float cond;
        if ((lo - hi) > 0.001000000047497451305389404296875f)
        {
            cond = clamp((z - hi) / (lo - hi), 0.0f, 1.0f);
        }
        else
        {
            cond = float(z >= lo);
        }
        cond2 = cond;
    }
    return cond2;
}
#endif

#if defined(WW_FN_ussTerrTier) || defined(WW_FN_ALL)
float ussTerrTier(float2 uv, int tier, float z, float texel, bool pen, float2 wxy)
{
    float2 cond2 = uv;
    int cond = tier;
    float2 nPick = ussTerrAtlasUv(cond2, cond);
    float2 rg = ussTerrMap.Sample(_ussTerrMap_sampler, nPick).xy;
    float2 csmHardRet = rg;
    float sAPick = z;
    float c = ussTerrVis(csmHardRet, sAPick);
    bool sAPart3 = !pen;
    bool sAPart2;
    if (!sAPart3)
    {
        sAPart2 = ussTerrPen.x < 0.5f;
    }
    else
    {
        sAPart2 = sAPart3;
    }
    bool sAPart;
    if (!sAPart2)
    {
        sAPart = rg.x <= 0.0f;
    }
    else
    {
        sAPart = sAPart2;
    }
    if (sAPart)
    {
        return c;
    }
    float dist = (max(rg.x - rg.y, 0.0f) * ussTerrRange.y) / max(ussTerrPen.w - ussTerrPen.z, 9.9999999747524270787835121154785e-07f);
    float r = min((dist * 0.0046251001767814159393310546875f) / max(texel, 9.9999999747524270787835121154785e-07f), 16.0f);
    if (r <= 0.5f)
    {
        return c;
    }
    float sBPick;
    if (tier == 0)
    {
        sBPick = ussTerrSize.x;
    }
    else
    {
        sBPick = ussTerrSize.y;
    }
    float n = sBPick;
    if (n <= 0.0f)
    {
        n = 1.0f / max(ussTerrXform.x * texel, 9.9999999600419720025001879548654e-13f);
    }
    float2 sBPart3;
    if (ussCsmStable.z > 0.5f)
    {
        sBPart3 = gl_FragCoord.xy;
    }
    else
    {
        sBPart3 = mod(floor(wxy * (4.0f / max(texel, 9.9999999747524270787835121154785e-07f))), 4096.0f.xx);
    }
    float2 sBPart2 = sBPart3;
    float rot = 6.283185482025146484375f * ussIgn(sBPart2);
    float sum = c;
    for (int t = 0; t < 8; t++)
    {
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * ((sqrt((float(t) + 0.5f) / 8.0f) * r) / n);
        float2 sBPart = clamp(uv + o, 0.0f.xx, 1.0f.xx);
        int sAPart7 = tier;
        float2 sBPart12 = ussTerrAtlasUv(sBPart, sAPart7);
        float2 sBPart11 = ussTerrMap.Sample(_ussTerrMap_sampler, sBPart12).xy;
        float sBPart10 = z;
        sum += ussTerrVis(sBPart11, sBPart10);
    }
    return sum / 9.0f;
}
#endif

#if defined(WW_FN_ussTerrTier_V2) || defined(WW_FN_ALL)
float ussTerrTier(float2 uv, int tier, float z, float texel, bool pen, float2 wxy)
{
    float2 cond2 = uv;
    int cond = tier;
    float2 nPick = ussTerrAtlasUv(cond2, cond);
    float2 rg = ussTerrMap.SampleLevel(_ussTerrMap_sampler, nPick, 0.0f).xy;
    float2 csmHardRet = rg;
    float sAPick = z;
    float c = ussTerrVis(csmHardRet, sAPick);
    bool sAPart3 = !pen;
    bool sAPart2;
    if (!sAPart3)
    {
        sAPart2 = ussTerrPen.x < 0.5f;
    }
    else
    {
        sAPart2 = sAPart3;
    }
    bool sAPart;
    if (!sAPart2)
    {
        sAPart = rg.x <= 0.0f;
    }
    else
    {
        sAPart = sAPart2;
    }
    if (sAPart)
    {
        return c;
    }
    float dist = (max(rg.x - rg.y, 0.0f) * ussTerrRange.y) / max(ussTerrPen.w - ussTerrPen.z, 9.9999999747524270787835121154785e-07f);
    float r = min((dist * 0.0046251001767814159393310546875f) / max(texel, 9.9999999747524270787835121154785e-07f), 16.0f);
    if (r <= 0.5f)
    {
        return c;
    }
    float sBPick;
    if (tier == 0)
    {
        sBPick = ussTerrSize.x;
    }
    else
    {
        sBPick = ussTerrSize.y;
    }
    float n = sBPick;
    if (n <= 0.0f)
    {
        n = 1.0f / max(ussTerrXform.x * texel, 9.9999999600419720025001879548654e-13f);
    }
    float2 sBPart3;
    if (ussCsmStable.z > 0.5f)
    {
        sBPart3 = pcFragCoord.xy;
    }
    else
    {
        sBPart3 = mod(floor(wxy * (4.0f / max(texel, 9.9999999747524270787835121154785e-07f))), 4096.0f.xx);
    }
    float2 sBPart2 = sBPart3;
    float rot = 6.283185482025146484375f * ussIgn(sBPart2);
    float sum = c;
    for (int t = 0; t < 8; t++)
    {
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * ((sqrt((float(t) + 0.5f) / 8.0f) * r) / n);
        float2 sBPart = clamp(uv + o, 0.0f.xx, 1.0f.xx);
        int sAPart7 = tier;
        float2 sBPart12 = ussTerrAtlasUv(sBPart, sAPart7);
        float2 sBPart11 = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sBPart12, 0.0f).xy;
        float sBPart10 = z;
        sum += ussTerrVis(sBPart11, sBPart10);
    }
    return sum / 9.0f;
}
#endif

#if defined(WW_FN_ussTerrExtraUv) || defined(WW_FN_ALL)
float2 ussTerrExtraUv(inout float2 uv, float off)
{
    uv = clamp(uv, (0.5f / ussTerrSize.x).xx, (1.0f - (0.5f / ussTerrSize.x)).xx);
    return float2(uv.x, off + (uv.y * ussTerrExtra.z));
}
#endif

#if defined(WW_FN_ussTerrHeights) || defined(WW_FN_ALL)
float4 ussTerrHeights(float2 uv)
{
    float d = 1.0f / ussTerrSize.x;
    float2 cond2 = uv;
    float cond = ussTerrExtra.x;
    float2 nPick = ussTerrExtraUv(cond2, cond);
    float c = ussTerrMap.Sample(_ussTerrMap_sampler, nPick).x;
    float2 csmHardRet = uv + float2(d, 0.0f);
    float sAPick = ussTerrExtra.x;
    float2 sAPart3 = ussTerrExtraUv(csmHardRet, sAPick);
    float hE = ussTerrMap.Sample(_ussTerrMap_sampler, sAPart3).x;
    float2 sAPart2 = uv - float2(d, 0.0f);
    float sAPart = ussTerrExtra.x;
    float2 sBPick = ussTerrExtraUv(sAPart2, sAPart);
    float hW = ussTerrMap.Sample(_ussTerrMap_sampler, sBPick).x;
    float2 sBPart3 = uv + float2(0.0f, d);
    float sBPart2 = ussTerrExtra.x;
    float2 sBPart = ussTerrExtraUv(sBPart3, sBPart2);
    float hN = ussTerrMap.Sample(_ussTerrMap_sampler, sBPart).x;
    float2 sAPart7 = uv - float2(0.0f, d);
    float sBPart12 = ussTerrExtra.x;
    float2 sBPart11 = ussTerrExtraUv(sAPart7, sBPart12);
    float hS = ussTerrMap.Sample(_ussTerrMap_sampler, sBPart11).x;
    bool sBPart10 = c <= 0.0f;
    bool sBPart9;
    if (!sBPart10)
    {
        sBPart9 = min(min(hE, hW), min(hN, hS)) <= 0.0f;
    }
    else
    {
        sBPart9 = sBPart10;
    }
    if (sBPart9)
    {
        return float4(ussTerrRange.x + (c * ussTerrRange.y), 0.0f, 0.0f, float(c > 0.0f));
    }
    float s = ussTerrRange.y;
    float relief = max(max(abs(hE - c), abs(hW - c)), max(abs(hN - c), abs(hS - c))) * s;
    float g = (max(abs(hE - hW), abs(hS - hN)) * s) / max(2.0f * ussTerrPen.y, 9.9999999747524270787835121154785e-07f);
    return float4(ussTerrRange.x + (c * s), relief, g, 1.0f);
}
#endif

#if defined(WW_FN_ussTerrHeights_V2) || defined(WW_FN_ALL)
float4 ussTerrHeights(float2 uv)
{
    float d = 1.0f / ussTerrSize.x;
    float2 cond2 = uv;
    float cond = ussTerrExtra.x;
    float2 nPick = ussTerrExtraUv(cond2, cond);
    float c = ussTerrMap.SampleLevel(_ussTerrMap_sampler, nPick, 0.0f).x;
    float2 csmHardRet = uv + float2(d, 0.0f);
    float sAPick = ussTerrExtra.x;
    float2 sAPart3 = ussTerrExtraUv(csmHardRet, sAPick);
    float hE = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sAPart3, 0.0f).x;
    float2 sAPart2 = uv - float2(d, 0.0f);
    float sAPart = ussTerrExtra.x;
    float2 sBPick = ussTerrExtraUv(sAPart2, sAPart);
    float hW = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sBPick, 0.0f).x;
    float2 sBPart3 = uv + float2(0.0f, d);
    float sBPart2 = ussTerrExtra.x;
    float2 sBPart = ussTerrExtraUv(sBPart3, sBPart2);
    float hN = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sBPart, 0.0f).x;
    float2 sAPart7 = uv - float2(0.0f, d);
    float sBPart12 = ussTerrExtra.x;
    float2 sBPart11 = ussTerrExtraUv(sAPart7, sBPart12);
    float hS = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sBPart11, 0.0f).x;
    bool sBPart10 = c <= 0.0f;
    bool sBPart9;
    if (!sBPart10)
    {
        sBPart9 = min(min(hE, hW), min(hN, hS)) <= 0.0f;
    }
    else
    {
        sBPart9 = sBPart10;
    }
    if (sBPart9)
    {
        return float4(ussTerrRange.x + (c * ussTerrRange.y), 0.0f, 0.0f, float(c > 0.0f));
    }
    float s = ussTerrRange.y;
    float relief = max(max(abs(hE - c), abs(hW - c)), max(abs(hN - c), abs(hS - c))) * s;
    float g = (max(abs(hE - hW), abs(hS - hN)) * s) / max(2.0f * ussTerrPen.y, 9.9999999747524270787835121154785e-07f);
    return float4(ussTerrRange.x + (c * s), relief, g, 1.0f);
}
#endif

#if defined(WW_FN_ussTerrSelfWeight) || defined(WW_FN_ALL)
float ussTerrSelfWeight(float2 uv, float wz, float4 h)
{
    float2 cond2 = uv;
    int cond = 0;
    float2 nPick = ussTerrAtlasUv(cond2, cond);
    float2 rg = ussTerrMap.Sample(_ussTerrMap_sampler, nPick).xy;
    float2 csmHardRet = uv;
    float sAPick = ussTerrExtra.x;
    float2 sAPart3 = ussTerrExtraUv(csmHardRet, sAPick);
    float cast = ussTerrMap.Sample(_ussTerrMap_sampler, sAPart3).y;
    bool wwh0 = (rg.x <= 0.0f);
    bool wwh1 = (cast <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float s = ussTerrRange.x + (rg.x * ussTerrRange.y);
    float hc = ussTerrRange.x + (cast * ussTerrRange.y);
    float tanE = max(ussTerrPen.z, 9.9999997473787516355514526367188e-05f);
    float cosE = rsqrt(1.0f + (tanE * tanE));
    float delta = max(hc - s, 0.0f) / (tanE * cosE);
    float grad = (ussTerrPen.y * cosE) * (1.0f + (h.z * tanE));
    float margin = (4.0f * ussTerrSelf.w) + min(1.5f * grad, ussTerrSelf.z);
    float fade = clamp((delta - margin) / max(grad, 8.0f * ussTerrSelf.w), 0.0f, 1.0f);
    float lo = ussTerrSelf.y;
    float sAPart2;
    if (lo > 0.0f)
    {
        sAPart2 = fade * smoothstep(0.0f, 1.0f, ((hc - wz) - lo) / lo);
    }
    else
    {
        sAPart2 = fade;
    }
    return sAPart2;
}
#endif

#if defined(WW_FN_ussTerrSelfWeight_V2) || defined(WW_FN_ALL)
float ussTerrSelfWeight(float2 uv, float wz, float4 h)
{
    float2 cond2 = uv;
    int cond = 0;
    float2 nPick = ussTerrAtlasUv(cond2, cond);
    float2 rg = ussTerrMap.SampleLevel(_ussTerrMap_sampler, nPick, 0.0f).xy;
    float2 csmHardRet = uv;
    float sAPick = ussTerrExtra.x;
    float2 sAPart3 = ussTerrExtraUv(csmHardRet, sAPick);
    float cast = ussTerrMap.SampleLevel(_ussTerrMap_sampler, sAPart3, 0.0f).y;
    bool wwh2 = (rg.x <= 0.0f);
    bool wwh3 = (cast <= 0.0f);
    if (or(wwh2, wwh3))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float s = ussTerrRange.x + (rg.x * ussTerrRange.y);
    float hc = ussTerrRange.x + (cast * ussTerrRange.y);
    float tanE = max(ussTerrPen.z, 9.9999997473787516355514526367188e-05f);
    float cosE = rsqrt(1.0f + (tanE * tanE));
    float delta = max(hc - s, 0.0f) / (tanE * cosE);
    float grad = (ussTerrPen.y * cosE) * (1.0f + (h.z * tanE));
    float margin = (4.0f * ussTerrSelf.w) + min(1.5f * grad, ussTerrSelf.z);
    float fade = clamp((delta - margin) / max(grad, 8.0f * ussTerrSelf.w), 0.0f, 1.0f);
    float lo = ussTerrSelf.y;
    float sAPart2;
    if (lo > 0.0f)
    {
        sAPart2 = fade * smoothstep(0.0f, 1.0f, ((hc - wz) - lo) / lo);
    }
    else
    {
        sAPart2 = fade;
    }
    return sAPart2;
}
#endif

#if defined(WW_FN_ussTerrain) || defined(WW_FN_ALL)
float ussTerrain(float3 posView, out float inMap, inout float grad, bool pen)
{
    inMap = 0.0f;
    if (ussTerrOn == 0)
    {
        return 1.0f;
    }
    float3 wp = mul(float4(posView, 1.0f), ussWorldFromView).xyz;
    float2 uv = float2((wp.x * ussTerrXform.x) + ussTerrXform.y, (wp.y * ussTerrXform.z) + ussTerrXform.w);
    bool cond2 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool cond;
    if (!cond2)
    {
        cond = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 1.0f;
    }
    inMap = 1.0f;
    float4 h = 0.0f.xxxx;
    bool nPick;
    if (pen)
    {
        bool csmHardRet = ussTerrBias.x > 1.5f;
        bool sAPick;
        if (!csmHardRet)
        {
            sAPick = ussTerrExtra.y > 0.0f;
        }
        else
        {
            sAPick = csmHardRet;
        }
        nPick = sAPick;
    }
    else
    {
        nPick = pen;
    }
    if (nPick)
    {
        float2 sAPart3 = uv;
        h = ussTerrHeights(sAPart3);
    }
    bool sAPart2;
    if (pen)
    {
        sAPart2 = ussTerrBias.x > 1.5f;
    }
    else
    {
        sAPart2 = pen;
    }
    if (sAPart2)
    {
        float sAPart;
        if (ussTerrBias.x > 2.5f)
        {
            sAPart = 0.0f;
        }
        else
        {
            sAPart = h.z;
        }
        grad = sAPart;
    }
    float sBPick;
    if (ussTerrBias.x > 0.5f)
    {
        sBPick = (12.0f * (1.0f + clamp(grad, 0.0f, 4.0f))) / ussTerrBias.y;
    }
    else
    {
        sBPick = ussTerrRange.z;
    }
    float z = wp.z + sBPick;
    float2 sBPart3 = uv;
    int sBPart2 = 0;
    float sBPart = z;
    float sAPart7 = ussTerrPen.y;
    bool sBPart12 = pen;
    float2 sBPart11 = wp.xy;
    float vis = ussTerrTier(sBPart3, sBPart2, sBPart, sAPart7, sBPart12, sBPart11);
    if (ussTerrSize.y > 0.0f)
    {
        float2 un = float2((wp.x * ussTerrNearXform.x) + ussTerrNearXform.y, (wp.y * ussTerrNearXform.z) + ussTerrNearXform.w);
        float2 en = (min(un, 1.0f.xx - un) * ussTerrSize.y) / 128.0f.xx;
        float w = smoothstep(0.0f, 1.0f, clamp(min(en.x, en.y), 0.0f, 1.0f));
        if (w > 0.0f)
        {
            float2 sBPart10 = un;
            int sBPart9 = 1;
            float sBPart8 = z;
            float sBPart7 = ussTerrBias.w;
            bool sAPart6 = pen;
            float2 sAPart5 = wp.xy;
            vis = lerp(vis, ussTerrTier(sBPart10, sBPart9, sBPart8, sBPart7, sAPart6, sAPart5), w);
        }
    }
    if (ussTerrExtra.y > 0.0f)
    {
        bool sAPart4 = h.w > 0.0f;
        bool sBPart6;
        if (sAPart4)
        {
            sBPart6 = ussTerrSelf.x > 0.5f;
        }
        else
        {
            sBPart6 = sAPart4;
        }
        if (sBPart6)
        {
            float2 sBPart5 = uv;
            float sBPart4 = ussTerrExtra.y;
            float2 wPick = ussTerrExtraUv(sBPart5, sBPart4);
            float2 ussTerrMapTexel2 = ussTerrMap.Sample(_ussTerrMap_sampler, wPick).xy;
            float visPart5 = z;
            float2 visPart4 = uv;
            float visPart3 = wp.z;
            float4 visPart2 = h;
            vis = min(ussTerrVis(ussTerrMapTexel2, visPart5), lerp(1.0f, vis, ussTerrSelfWeight(visPart4, visPart3, visPart2)));
        }
        else
        {
            if (h.w > 0.0f)
            {
                float mPick;
                if (ussTerrExtra.w < 0.0f)
                {
                    mPick = 1.0f;
                }
                else
                {
                    mPick = smoothstep(0.75f * ussTerrExtra.w, 1.25f * ussTerrExtra.w, (wp.z - h.x) - min(h.y, 4.0f * ussTerrPen.y));
                }
                float m = mPick;
                if (m > 0.0f)
                {
                    float2 ussTerrExtraUvPart2 = uv;
                    float ussTerrExtraUvPart = ussTerrExtra.y;
                    float2 ussTerrExtraUvRet = ussTerrExtraUv(ussTerrExtraUvPart2, ussTerrExtraUvPart);
                    float2 ussTerrMapTexel = ussTerrMap.Sample(_ussTerrMap_sampler, ussTerrExtraUvRet).xy;
                    float visPart = z;
                    vis = lerp(vis, ussTerrVis(ussTerrMapTexel, visPart), m);
                }
            }
        }
    }
    float2 e = min(uv, 1.0f.xx - uv) / max(ussTerrRange.w, 9.9999999747524270787835121154785e-07f).xx;
    float edgeWPick;
    if (ussTerrRange.w < 0.0f)
    {
        edgeWPick = 1.0f - smoothstep(0.800000011920928955078125f, 1.0f, length(wp.xy - ussWorldFromView[3].xy) / (-ussTerrRange.w));
    }
    else
    {
        edgeWPick = clamp(min(e.x, e.y), 0.0f, 1.0f);
    }
    float edgeW = edgeWPick;
    vis = lerp(1.0f, vis, edgeW);
    return lerp(1.0f, vis, ussTerrBias.z);
}
#endif

#if defined(WW_FN_ussTerrain_V2) || defined(WW_FN_ALL)
float ussTerrain(float3 posView, out float inMap, inout float grad, bool pen)
{
    inMap = 0.0f;
    if (ussTerrOn == 0)
    {
        return 1.0f;
    }
    float3 wp = mul(float4(posView, 1.0f), ussWorldFromView).xyz;
    float2 uv = float2((wp.x * ussTerrXform.x) + ussTerrXform.y, (wp.y * ussTerrXform.z) + ussTerrXform.w);
    bool cond2 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool cond;
    if (!cond2)
    {
        cond = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 1.0f;
    }
    inMap = 1.0f;
    float4 h = 0.0f.xxxx;
    bool nPick;
    if (pen)
    {
        bool csmHardRet = ussTerrBias.x > 1.5f;
        bool sAPick;
        if (!csmHardRet)
        {
            sAPick = ussTerrExtra.y > 0.0f;
        }
        else
        {
            sAPick = csmHardRet;
        }
        nPick = sAPick;
    }
    else
    {
        nPick = pen;
    }
    if (nPick)
    {
        float2 sAPart3 = uv;
        h = ussTerrHeights(sAPart3);
    }
    bool sAPart2;
    if (pen)
    {
        sAPart2 = ussTerrBias.x > 1.5f;
    }
    else
    {
        sAPart2 = pen;
    }
    if (sAPart2)
    {
        float sAPart;
        if (ussTerrBias.x > 2.5f)
        {
            sAPart = 0.0f;
        }
        else
        {
            sAPart = h.z;
        }
        grad = sAPart;
    }
    float sBPick;
    if (ussTerrBias.x > 0.5f)
    {
        sBPick = (12.0f * (1.0f + clamp(grad, 0.0f, 4.0f))) / ussTerrBias.y;
    }
    else
    {
        sBPick = ussTerrRange.z;
    }
    float z = wp.z + sBPick;
    float2 sBPart3 = uv;
    int sBPart2 = 0;
    float sBPart = z;
    float sAPart7 = ussTerrPen.y;
    bool sBPart12 = pen;
    float2 sBPart11 = wp.xy;
    float vis = ussTerrTier(sBPart3, sBPart2, sBPart, sAPart7, sBPart12, sBPart11);
    if (ussTerrSize.y > 0.0f)
    {
        float2 un = float2((wp.x * ussTerrNearXform.x) + ussTerrNearXform.y, (wp.y * ussTerrNearXform.z) + ussTerrNearXform.w);
        float2 en = (min(un, 1.0f.xx - un) * ussTerrSize.y) / 128.0f.xx;
        float w = smoothstep(0.0f, 1.0f, clamp(min(en.x, en.y), 0.0f, 1.0f));
        if (w > 0.0f)
        {
            float2 sBPart10 = un;
            int sBPart9 = 1;
            float sBPart8 = z;
            float sBPart7 = ussTerrBias.w;
            bool sAPart6 = pen;
            float2 sAPart5 = wp.xy;
            vis = lerp(vis, ussTerrTier(sBPart10, sBPart9, sBPart8, sBPart7, sAPart6, sAPart5), w);
        }
    }
    if (ussTerrExtra.y > 0.0f)
    {
        bool sAPart4 = h.w > 0.0f;
        bool sBPart6;
        if (sAPart4)
        {
            sBPart6 = ussTerrSelf.x > 0.5f;
        }
        else
        {
            sBPart6 = sAPart4;
        }
        if (sBPart6)
        {
            float2 sBPart5 = uv;
            float sBPart4 = ussTerrExtra.y;
            float2 wPick = ussTerrExtraUv(sBPart5, sBPart4);
            float2 ussTerrMapTexel2 = ussTerrMap.SampleLevel(_ussTerrMap_sampler, wPick, 0.0f).xy;
            float visPart5 = z;
            float2 visPart4 = uv;
            float visPart3 = wp.z;
            float4 visPart2 = h;
            vis = min(ussTerrVis(ussTerrMapTexel2, visPart5), lerp(1.0f, vis, ussTerrSelfWeight(visPart4, visPart3, visPart2)));
        }
        else
        {
            if (h.w > 0.0f)
            {
                float mPick;
                if (ussTerrExtra.w < 0.0f)
                {
                    mPick = 1.0f;
                }
                else
                {
                    mPick = smoothstep(0.75f * ussTerrExtra.w, 1.25f * ussTerrExtra.w, (wp.z - h.x) - min(h.y, 4.0f * ussTerrPen.y));
                }
                float m = mPick;
                if (m > 0.0f)
                {
                    float2 ussTerrExtraUvPart2 = uv;
                    float ussTerrExtraUvPart = ussTerrExtra.y;
                    float2 ussTerrExtraUvRet = ussTerrExtraUv(ussTerrExtraUvPart2, ussTerrExtraUvPart);
                    float2 ussTerrMapTexel = ussTerrMap.SampleLevel(_ussTerrMap_sampler, ussTerrExtraUvRet, 0.0f).xy;
                    float visPart = z;
                    vis = lerp(vis, ussTerrVis(ussTerrMapTexel, visPart), m);
                }
            }
        }
    }
    float2 e = min(uv, 1.0f.xx - uv) / max(ussTerrRange.w, 9.9999999747524270787835121154785e-07f).xx;
    float edgeWPick;
    if (ussTerrRange.w < 0.0f)
    {
        edgeWPick = 1.0f - smoothstep(0.800000011920928955078125f, 1.0f, length(wp.xy - ussWorldFromView[3].xy) / (-ussTerrRange.w));
    }
    else
    {
        edgeWPick = clamp(min(e.x, e.y), 0.0f, 1.0f);
    }
    float edgeW = edgeWPick;
    vis = lerp(1.0f, vis, edgeW);
    return lerp(1.0f, vis, ussTerrBias.z);
}
#endif

#if defined(WW_FN_ussSlope) || defined(WW_FN_ALL)
float ussSlope(float3 dPx, float3 dPy)
{
    float3 n = cross(dPx, dPy);
    float l = length(n);
    bool cond2 = l < 9.9999996826552253889678874634872e-21f;
    bool cond;
    if (!cond2)
    {
        cond = ussTerrBias.x < 0.5f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0.0f;
    }
    float nz = abs(mul(float4(n / l.xxx, 0.0f), ussWorldFromView).z) / max(length(ussWorldFromView[0].xyz), 9.9999996826552253889678874634872e-21f);
    return sqrt(max(1.0f - (nz * nz), 0.0f)) / max(nz, 0.20000000298023223876953125f);
}
#endif

#if defined(WW_FN_ussContact) || defined(WW_FN_ALL)
float ussContact()
{
    if (ussContactOn == 0)
    {
        return 1.0f;
    }
    return ussContactMap.Sample(_ussContactMap_sampler, gl_FragCoord.xy * ussContactInvSize).x;
}
#endif

#if defined(WW_FN_ussContact_V2) || defined(WW_FN_ALL)
float ussContact()
{
    if (ussContactOn == 0)
    {
        return 1.0f;
    }
    return ussContactMap.SampleLevel(_ussContactMap_sampler, pcFragCoord.xy * ussContactInvSize, 0.0f).x;
}
#endif

#if defined(WW_FN_ussCompose) || defined(WW_FN_ALL)
float ussCompose(float3 posView)
{
    float3 dPx = ddx(posView);
    float3 dPy = ddy(posView);
    float3 cond2 = posView;
    float3 cond = dPx;
    float3 nPick = dPy;
    float csmHardRet;
    float sAPick = ussCascades(cond2, cond, nPick, csmHardRet);
    float w = csmHardRet;
    float casc = sAPick;
    float3 sAPart3 = dPx;
    float3 sAPart2 = dPy;
    float3 sAPart = posView;
    float sBPick = ussSlope(sAPart3, sAPart2);
    bool sBPart3 = true;
    float sBPart2;
    float sBPart = ussTerrain(sAPart, sBPart2, sBPick, sBPart3);
    float inMap = sBPart2;
    float terr = sBPart;
    float con = ussContact();
    if (((ussRed & 1) != 0) && (w > 0.0f))
    {
        terr = 1.0f;
    }
    ussLastTerr = terr;
    float c = lerp(1.0f, casc, w);
    if ((ussRed & 2) != 0)
    {
        return (c * terr) * con;
    }
    return min(min(c, terr), con);
}
#endif

#if defined(WW_FN_ussCompose_V2) || defined(WW_FN_ALL)
float ussCompose(float3 posView)
{
    float3 dPx = pcDPx;
    float3 dPy = pcDPy;
    float3 cond2 = posView;
    float3 cond = dPx;
    float3 nPick = dPy;
    float csmHardRet;
    float sAPick = ussCascades(cond2, cond, nPick, csmHardRet);
    float w = csmHardRet;
    float casc = sAPick;
    float3 sAPart3 = dPx;
    float3 sAPart2 = dPy;
    float3 sAPart = posView;
    float sBPick = ussSlope(sAPart3, sAPart2);
    bool sBPart3 = true;
    float sBPart2;
    float sBPart = ussTerrain(sAPart, sBPart2, sBPick, sBPart3);
    float inMap = sBPart2;
    float terr = sBPart;
    float con = ussContact();
    if (((ussRed & 1) != 0) && (w > 0.0f))
    {
        terr = 1.0f;
    }
    ussLastTerr = terr;
    float c = lerp(1.0f, casc, w);
    if ((ussRed & 2) != 0)
    {
        return (c * terr) * con;
    }
    return min(min(c, terr), con);
}
#endif

#if defined(WW_FN_ussCloudT) || defined(WW_FN_ALL)
float ussCloudT(float3 posView)
{
    if (ussCloudOn == 0)
    {
        return 1.0f;
    }
    float3 q = mul(float4(posView, 1.0f), ussCloudFromView).xyz;
    float3 m = ussCloudMap.SampleLevel(_ussCloudMap_sampler, q.xy, 0.0f).xyz;
    float t = exp(-min(m.z, m.y * max(m.x - q.z, 0.0f)));
    float2 e = abs(q.xy - 0.5f.xx) * 2.0f;
    return lerp(t, ussCloudFar.x, smoothstep(1.0f - ussCloudFar.y, 1.0f, max(e.x, e.y)));
}
#endif

#if defined(WW_FN_wwSunShadowGeo) || defined(WW_FN_ALL)
float wwSunShadowGeo(float3 posView)
{
    if ((csmRed & 8) != 0)
    {
        return 0.5f;
    }
    if (csmForce >= 0.0f)
    {
        return csmForce;	// WW_CSM_FORCE: >= 0 replaces the factor (the factor-site wiring test); -1 off
    }
    if (ussCsmStable.w > 0.0f)
    {
        float3 cond2 = posView;
        float3 cond = ddx(posView);
        float3 nPick = ddy(posView);
        csmNOff = csmNormalOff(cond2, cond, nPick);
    }
    if (ussOn != 0)
    {
        float3 csmHardRet = posView;
        float sAPick = ussCompose(csmHardRet);
        return sAPick;
    }
    float dv = (-posView.z) * csmParams.x;
    if (dv > (csmParams.y + csmParams.w))
    {
        return 1.0f;
    }
    float3 sAPart3 = posView;
    float sh = csmBlended(sAPart3);
    if ((csmRed & 2) == 0)
    {
        float s = clamp(((dot(posView, posView) * csmParams.x) * csmParams.x) / (csmParams.y * csmParams.y), 0.0f, 1.0f);
        float s2 = s * s;
        sh = 1.0f - ((1.0f - sh) * (1.0f - (s2 * s2)));
    }
    return sh;
}
#endif

#if defined(WW_FN_wwSunShadowGeo_V2) || defined(WW_FN_ALL)
float wwSunShadowGeo(float3 posView)
{
    if ((csmRed & 8) != 0)
    {
        return 0.5f;
    }
    if (csmForce >= 0.0f)
    {
        return csmForce;	// WW_CSM_FORCE: >= 0 replaces the factor (the factor-site wiring test); -1 off
    }
    if (ussCsmStable.w > 0.0f)
    {
        float3 cond2 = posView;
        float3 cond = pcDPx;
        float3 nPick = pcDPy;
        csmNOff = csmNormalOff(cond2, cond, nPick);
    }
    if (ussOn != 0)
    {
        float3 csmHardRet = posView;
        float sAPick = ussCompose(csmHardRet);
        return sAPick;
    }
    float dv = (-posView.z) * csmParams.x;
    if (dv > (csmParams.y + csmParams.w))
    {
        return 1.0f;
    }
    float3 sAPart3 = posView;
    float sh = csmBlended(sAPart3);
    if ((csmRed & 2) == 0)
    {
        float s = clamp(((dot(posView, posView) * csmParams.x) * csmParams.x) / (csmParams.y * csmParams.y), 0.0f, 1.0f);
        float s2 = s * s;
        sh = 1.0f - ((1.0f - sh) * (1.0f - (s2 * s2)));
    }
    return sh;
}
#endif

#if defined(WW_FN_wwSunShadow) || defined(WW_FN_ALL)
float wwSunShadow(float3 posView)
{
    float3 cond2 = posView;
    float cond = wwSunShadowGeo(cond2);
    float3 nPick = posView;
    return cond * ussCloudT(nPick);
}
#endif

#if defined(WW_FN_wwAirTailShadow) || defined(WW_FN_ALL)
float wwAirTailShadow(float3 dirView, float t0, float t1, float tau)
{
    bool cond2 = ussOn != 0;
    bool cond;
    if (cond2)
    {
        cond = ussTail.x > 0.0f;
    }
    else
    {
        cond = cond2;
    }
    bool terr = cond && (ussTerrOn != 0);
    bool nPick = ussCloudOn != 0;
    bool csmHardRet;
    if (nPick)
    {
        csmHardRet = ussCloudFar.z > 0.0f;
    }
    else
    {
        csmHardRet = nPick;
    }
    bool cloud = csmHardRet;
    bool wwh4 = (t1 <= t0);
    if (((!terr) && (!cloud)) || wwh4)
    {
        return 1.0f;
    }
    float sAPick;
    if (terr)
    {
        sAPick = ussTail.y;
    }
    else
    {
        sAPick = ussCloudFar.w;
    }
    float a = clamp(sAPick, 0.0f, 1.0f);
    float3 p0 = dirView * (t0 / fogDistScale);
    float sAPart3;
    float inMap;
    if (terr)
    {
        float3 sAPart2 = p0;
        float sAPart = 0.0f;
        bool sBPick = false;
        float sBPart3;
        float sBPart2 = ussTerrain(sAPart2, sBPart3, sAPart, sBPick);
        inMap = sBPart3;
        sAPart3 = sBPart2;
    }
    else
    {
        sAPart3 = 1.0f;
    }
    float sBPart;
    if (cloud)
    {
        float3 sAPart7 = p0;
        sBPart = ussCloudT(sAPart7);
    }
    else
    {
        sBPart = 1.0f;
    }
    float v0 = sAPart3 * sBPart;
    float e = 1.0f - exp(-max(tau, 0.0f));
    float2 key;
    if (ussCsmStable.z < (-0.5f))
    {
        float3 dw = mul(dirView, float3x3(ussWorldFromView[0].xyz, ussWorldFromView[1].xyz, ussWorldFromView[2].xyz));
        if (dot(dw, dw) <= 9.9999996826552253889678874634872e-21f)
        {
            dw = dirView;
        }
        dw /= ((abs(dw.x) + abs(dw.y)) + abs(dw.z)).xxx;
        float2 sBPart12;
        if (dw.z >= 0.0f)
        {
            sBPart12 = dw.xy;
        }
        else
        {
            sBPart12 = (1.0f.xx - abs(dw.yx)) * float2((dw.x >= 0.0f) ? 1.0f : (-1.0f), (dw.y >= 0.0f) ? 1.0f : (-1.0f));
        }
        float2 oct = sBPart12;
        key = mod(floor(oct * 2048.0f), 4096.0f.xx);
    }
    else
    {
        float3 w1 = mul(float4(dirView * (t1 / fogDistScale), 1.0f), ussWorldFromView).xyz;
        float cell = exp2(max(ceil(log2(max(t1, 1.0f) / 2048.0f)), 0.0f));
        float3 c = floor(w1 / cell.xxx);
        key = mod(c.xy + (float2(37.0f, 61.0f) * c.z), 4096.0f.xx);
    }
    float2 sBPart11;
    if (ussCsmStable.z > 0.5f)
    {
        sBPart11 = gl_FragCoord.xy;
    }
    else
    {
        sBPart11 = key;
    }
    float2 sBPart10 = sBPart11;
    float jit = ussIgn(sBPart10);
    float acc = 0.0f;
    float sBPart9;
    float sBPart8;
    float sBPart7;
    float sAPart6;
    for (int i = 0; i < 8; i++)
    {
        float q = (float(i) + jit) / 8.0f;
        if (tau > 9.9999997473787516355514526367188e-05f)
        {
            sBPart9 = (-log(max(1.0f - (q * e), 9.9999999747524270787835121154785e-07f))) / tau;
        }
        else
        {
            sBPart9 = q;
        }
        float u = sBPart9;
        float3 p = dirView * (lerp(t0, t1, clamp(u, 0.0f, 1.0f)) / fogDistScale);
        if (terr)
        {
            float3 sAPart5 = p;
            float sAPart4 = 0.0f;
            bool sBPart6 = false;
            float sBPart5 = ussTerrain(sAPart5, sBPart7, sAPart4, sBPart6);
            inMap = sBPart7;
            sBPart8 = sBPart5;
        }
        else
        {
            sBPart8 = 1.0f;
        }
        if (cloud)
        {
            float3 sBPart4 = p;
            sAPart6 = ussCloudT(sBPart4);
        }
        else
        {
            sAPart6 = 1.0f;
        }
        acc += (sBPart8 * sAPart6);
    }
    return (a + (((1.0f - a) * acc) / 8.0f)) / max(a + ((1.0f - a) * v0), 0.25f);
}
#endif

#if defined(WW_FN_ussTerrainDim) || defined(WW_FN_ALL)
void ussTerrainDim(out float3 amb, out float spec)
{
    amb = 1.0f.xxx;
    spec = 1.0f;
    if (ussOn == 0)
    {
        return;
    }
    float fl = 1.0f - ussDim.x;
    float m = max(ussLastTerr, fl);
    if ((ussDim.x > 0.0f) && (m < 1.0f))
    {
        float shift = min(0.3499999940395355224609375f * clamp((1.0f - m) / max(1.0f - fl, 9.9999999747524270787835121154785e-07f), 0.0f, 1.0f), (1.0f - m) / (m * 0.541400015354156494140625f));
        amb = max(lerp(1.0f.xxx, float3(0.777000010013580322265625f, 1.011600017547607421875f, 1.54139995574951171875f), shift.xxx) * m, (0.777000010013580322265625f * fl).xxx);
    }
    if (ussDim.y > 0.0f)
    {
        spec = max(ussLastTerr, 1.0f - ussDim.y);
    }
}
#endif
