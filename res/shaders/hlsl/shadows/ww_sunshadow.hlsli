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
    bool tmp1 = ln <= 9.9999996826552253889678874634872e-21f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = dot(lv, lv) <= 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 0.0f.xxx;
    }
    float3 tmp3;
    if (dot(n, posView) > 0.0f)
    {
        tmp3 = (-n) / ln.xxx;
    }
    else
    {
        tmp3 = n / ln.xxx;
    }
    n = tmp3;
    float c = dot(n, normalize(lv));
    return n * ((ussCsmStable.w * sqrt(max(1.0f - (c * c), 0.0f))) / csmParams.x);
}
#endif

#if defined(WW_FN_csmHard) || defined(WW_FN_ALL)
float csmHard(int i, float3 posView, float offsetWorld)
{
    int tmp1 = i;
    float3 tmp2 = posView + (csmNOff * ussCsmTexel[i]);
    float3 q = csmUvd(tmp1, tmp2);
    float tmp3;
    if (ussCsmStable.w != 0.0f)
    {
        tmp3 = ussCsmBias[i];
    }
    else
    {
        tmp3 = offsetWorld;
    }
    float off = tmp3;
    float4 tmp4 = float4(q.xy, float(i), q.z - (off * csmInvRange[i]));
    return csmMap.SampleCmp(_csmMap_sampler, tmp4.xyz, tmp4.w);
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
    float tmp1;
    if ((csmRed & 1) != 0)
    {
        tmp1 = step(0.5f, x);
    }
    else
    {
        tmp1 = smoothstep(0.0f, 1.0f, x);
    }
    t = tmp1;
}
#endif

#if defined(WW_FN_csmBlended) || defined(WW_FN_ALL)
float csmBlended(float3 posView)
{
    float3 tmp1 = posView;
    int tmp2;
    int tmp3;
    float tmp4;
    csmSelect(tmp1, tmp2, tmp3, tmp4);
    int a = tmp2;
    int b = tmp3;
    float t = tmp4;
    float tmp5;
    if (t < 1.0f)
    {
        int tmp6 = a;
        float3 tmp7 = posView;
        float tmp8 = csmSplitOffset.z;
        tmp5 = csmFiltered(tmp6, tmp7, tmp8);
    }
    else
    {
        tmp5 = 1.0f;
    }
    float sA = tmp5;
    float tmp9;
    if (t > 0.0f)
    {
        int tmp10 = b;
        float3 tmp11 = posView;
        float tmp12 = csmSplitOffset.w;
        tmp9 = csmFiltered(tmp10, tmp11, tmp12);
    }
    else
    {
        tmp9 = 1.0f;
    }
    float sB = tmp9;
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
    int tmp1 = i;
    float3 tmp2 = posView + (nOff * ussCsmTexel[i]);
    float3 q = csmUvd(tmp1, tmp2);
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
    float2 tmp3;
    if (ussCsmStable.x > 0.5f)
    {
        tmp3 = mod(floor(q.xy * (csmParams.z * 4.0f)) + float2(ussCsmCellX[i], ussCsmCellY[i]), 4096.0f.xx);
    }
    else
    {
        tmp3 = gl_FragCoord.xy + (float2(32.66500091552734375f, 11.81499958038330078125f) * ussCsmPhase);
    }
    float2 rp = tmp3;
    float2 tmp4 = rp;
    float rot = 6.283185482025146484375f * ussIgn(tmp4);
    float k = 1.0f / csmParams.z;
    float lim = 2000.0f * csmInvRange[i];
    float tmp5;
    if (ussCsmFilter.w > 1.5f)
    {
        tmp5 = lim;
    }
    else
    {
        tmp5 = (4.0f * ussCsmTexel[i]) * csmInvRange[i];
    }
    float cap = tmp5;
    float sum = 0.0f;
    float tmp6;
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
            tmp6 = 1.0f;
        }
        else
        {
            tmp6 = min(1.0f, cap / max(abs(dz), 1.0000000031710768509710513471353e-30f));
        }
        float pull = tmp6;
        float4 tmp7 = float4(q.xy + (o * pull), float(i), zRef + clamp(dz * pull, -lim, lim));
        sum += csmMap.SampleCmp(_csmMap_sampler, tmp7.xyz, tmp7.w);
    }
    return sum / float(n);
}
#endif

#if defined(WW_FN_ussVogel_V2) || defined(WW_FN_ALL)
float ussVogel(int i, float3 posView, float offsetWorld, float3 dPx, float3 dPy, float3 nOff)
{
    int tmp1 = i;
    float3 tmp2 = posView + (nOff * ussCsmTexel[i]);
    float3 q = csmUvd(tmp1, tmp2);
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
    float2 tmp3;
    if (ussCsmStable.x > 0.5f)
    {
        tmp3 = mod(floor(q.xy * (csmParams.z * 4.0f)) + float2(ussCsmCellX[i], ussCsmCellY[i]), 4096.0f.xx);
    }
    else
    {
        tmp3 = pcFragCoord.xy + (float2(32.66500091552734375f, 11.81499958038330078125f) * ussCsmPhase);
    }
    float2 rp = tmp3;
    float2 tmp4 = rp;
    float rot = 6.283185482025146484375f * ussIgn(tmp4);
    float k = 1.0f / csmParams.z;
    float lim = 2000.0f * csmInvRange[i];
    float tmp5;
    if (ussCsmFilter.w > 1.5f)
    {
        tmp5 = lim;
    }
    else
    {
        tmp5 = (4.0f * ussCsmTexel[i]) * csmInvRange[i];
    }
    float cap = tmp5;
    float sum = 0.0f;
    float tmp6;
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
            tmp6 = 1.0f;
        }
        else
        {
            tmp6 = min(1.0f, cap / max(abs(dz), 1.0000000031710768509710513471353e-30f));
        }
        float pull = tmp6;
        float4 tmp7 = float4(q.xy + (o * pull), float(i), zRef + clamp(dz * pull, -lim, lim));
        sum += csmMap.SampleCmpLevelZero(_csmMap_sampler, tmp7.xyz, tmp7.w);
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
    float3 tmp1 = posView;
    int tmp2;
    int tmp3;
    float tmp4;
    csmSelect(tmp1, tmp2, tmp3, tmp4);
    int a = tmp2;
    int b = tmp3;
    float t = tmp4;
    float sA = 1.0f;
    float sB = 1.0f;
    if (ussCsmFilter.x > 0.5f)
    {
        float3 nOff = 0.0f.xxx;
        if (ussCsmStable.y > 0.0f)
        {
            float3 n = cross(dPx, dPy);
            float ln = length(n);
            float3 lv = float3(csmMat[a][0].z, csmMat[a][1].z, csmMat[a][2].z);
            bool tmp5 = ln > 9.9999996826552253889678874634872e-21f;
            bool tmp6;
            if (tmp5)
            {
                tmp6 = dot(lv, lv) > 0.0f;
            }
            else
            {
                tmp6 = tmp5;
            }
            if (tmp6)
            {
                float3 tmp7;
                if (dot(n, posView) > 0.0f)
                {
                    tmp7 = (-n) / ln.xxx;
                }
                else
                {
                    tmp7 = n / ln.xxx;
                }
                n = tmp7;
                float c = dot(n, normalize(lv));
                nOff = n * ((ussCsmStable.y * sqrt(max(1.0f - (c * c), 0.0f))) / csmParams.x);
            }
        }
        if (t < 1.0f)
        {
            int tmp8 = a;
            float3 tmp9 = posView;
            float tmp10 = ussCsmBias[a];
            float3 tmp11 = dPx;
            float3 tmp12 = dPy;
            float3 tmp13 = nOff;
            sA = ussVogel(tmp8, tmp9, tmp10, tmp11, tmp12, tmp13);
        }
        if (t > 0.0f)
        {
            int tmp14 = b;
            float3 tmp15 = posView;
            float tmp16 = ussCsmBias[b];
            float3 tmp17 = dPx;
            float3 tmp18 = dPy;
            float3 tmp19 = nOff;
            sB = ussVogel(tmp14, tmp15, tmp16, tmp17, tmp18, tmp19);
        }
    }
    else
    {
        if (t < 1.0f)
        {
            int tmp20 = a;
            float3 tmp21 = posView;
            float tmp22 = csmSplitOffset.z;
            sA = csmFiltered(tmp20, tmp21, tmp22);
        }
        if (t > 0.0f)
        {
            int tmp23 = b;
            float3 tmp24 = posView;
            float tmp25 = csmSplitOffset.w;
            sB = csmFiltered(tmp23, tmp24, tmp25);
        }
    }
    float s = clamp(((dot(posView, posView) * csmParams.x) * csmParams.x) / (csmParams.y * csmParams.y), 0.0f, 1.0f);
    float s2 = s * s;
    float tmp26;
    if ((csmRed & 2) != 0)
    {
        tmp26 = 1.0f;
    }
    else
    {
        tmp26 = 1.0f - (s2 * s2);
    }
    w = tmp26;
    return lerp(sA, sB, t);
}
#endif

#if defined(WW_FN_ussTerrAtlasUv) || defined(WW_FN_ALL)
float2 ussTerrAtlasUv(inout float2 uv, int tier)
{
    bool tmp1 = ussTerrSize.y <= 0.0f;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = ussTerrExtra.z <= 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return uv;
    }
    float tmp3;
    if (tier == 0)
    {
        tmp3 = ussTerrSize.x;
    }
    else
    {
        tmp3 = ussTerrSize.y;
    }
    float n = tmp3;
    uv = clamp(uv, (0.5f / n).xx, (1.0f - (0.5f / n)).xx);
    float2 tmp4;
    if (tier == 0)
    {
        tmp4 = float2(uv.x, uv.y * ussTerrAtlas.x);
    }
    else
    {
        tmp4 = float2(uv.x * ussTerrAtlas.y, ussTerrAtlas.z + (uv.y * ussTerrAtlas.w));
    }
    return tmp4;
}
#endif

#if defined(WW_FN_ussTerrVis) || defined(WW_FN_ALL)
float ussTerrVis(float2 rg, float z)
{
    float lo = ussTerrRange.x + (rg.x * ussTerrRange.y);
    float hi = ussTerrRange.x + (rg.y * ussTerrRange.y);
    float tmp1;
    if (rg.x <= 0.0f)
    {
        tmp1 = 1.0f;
    }
    else
    {
        float tmp2;
        if ((lo - hi) > 0.001000000047497451305389404296875f)
        {
            tmp2 = clamp((z - hi) / (lo - hi), 0.0f, 1.0f);
        }
        else
        {
            tmp2 = float(z >= lo);
        }
        tmp1 = tmp2;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_ussTerrTier) || defined(WW_FN_ALL)
float ussTerrTier(float2 uv, int tier, float z, float texel, bool pen, float2 wxy)
{
    float2 tmp1 = uv;
    int tmp2 = tier;
    float2 tmp3 = ussTerrAtlasUv(tmp1, tmp2);
    float2 rg = ussTerrMap.Sample(_ussTerrMap_sampler, tmp3).xy;
    float2 tmp4 = rg;
    float tmp5 = z;
    float c = ussTerrVis(tmp4, tmp5);
    bool tmp6 = !pen;
    bool tmp7;
    if (!tmp6)
    {
        tmp7 = ussTerrPen.x < 0.5f;
    }
    else
    {
        tmp7 = tmp6;
    }
    bool tmp8;
    if (!tmp7)
    {
        tmp8 = rg.x <= 0.0f;
    }
    else
    {
        tmp8 = tmp7;
    }
    if (tmp8)
    {
        return c;
    }
    float dist = (max(rg.x - rg.y, 0.0f) * ussTerrRange.y) / max(ussTerrPen.w - ussTerrPen.z, 9.9999999747524270787835121154785e-07f);
    float r = min((dist * 0.0046251001767814159393310546875f) / max(texel, 9.9999999747524270787835121154785e-07f), 16.0f);
    if (r <= 0.5f)
    {
        return c;
    }
    float tmp9;
    if (tier == 0)
    {
        tmp9 = ussTerrSize.x;
    }
    else
    {
        tmp9 = ussTerrSize.y;
    }
    float n = tmp9;
    if (n <= 0.0f)
    {
        n = 1.0f / max(ussTerrXform.x * texel, 9.9999999600419720025001879548654e-13f);
    }
    float2 tmp10;
    if (ussCsmStable.z > 0.5f)
    {
        tmp10 = gl_FragCoord.xy;
    }
    else
    {
        tmp10 = mod(floor(wxy * (4.0f / max(texel, 9.9999999747524270787835121154785e-07f))), 4096.0f.xx);
    }
    float2 tmp11 = tmp10;
    float rot = 6.283185482025146484375f * ussIgn(tmp11);
    float sum = c;
    for (int t = 0; t < 8; t++)
    {
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * ((sqrt((float(t) + 0.5f) / 8.0f) * r) / n);
        float2 tmp12 = clamp(uv + o, 0.0f.xx, 1.0f.xx);
        int tmp13 = tier;
        float2 tmp14 = ussTerrAtlasUv(tmp12, tmp13);
        float2 tmp15 = ussTerrMap.Sample(_ussTerrMap_sampler, tmp14).xy;
        float tmp16 = z;
        sum += ussTerrVis(tmp15, tmp16);
    }
    return sum / 9.0f;
}
#endif

#if defined(WW_FN_ussTerrTier_V2) || defined(WW_FN_ALL)
float ussTerrTier(float2 uv, int tier, float z, float texel, bool pen, float2 wxy)
{
    float2 tmp1 = uv;
    int tmp2 = tier;
    float2 tmp3 = ussTerrAtlasUv(tmp1, tmp2);
    float2 rg = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp3, 0.0f).xy;
    float2 tmp4 = rg;
    float tmp5 = z;
    float c = ussTerrVis(tmp4, tmp5);
    bool tmp6 = !pen;
    bool tmp7;
    if (!tmp6)
    {
        tmp7 = ussTerrPen.x < 0.5f;
    }
    else
    {
        tmp7 = tmp6;
    }
    bool tmp8;
    if (!tmp7)
    {
        tmp8 = rg.x <= 0.0f;
    }
    else
    {
        tmp8 = tmp7;
    }
    if (tmp8)
    {
        return c;
    }
    float dist = (max(rg.x - rg.y, 0.0f) * ussTerrRange.y) / max(ussTerrPen.w - ussTerrPen.z, 9.9999999747524270787835121154785e-07f);
    float r = min((dist * 0.0046251001767814159393310546875f) / max(texel, 9.9999999747524270787835121154785e-07f), 16.0f);
    if (r <= 0.5f)
    {
        return c;
    }
    float tmp9;
    if (tier == 0)
    {
        tmp9 = ussTerrSize.x;
    }
    else
    {
        tmp9 = ussTerrSize.y;
    }
    float n = tmp9;
    if (n <= 0.0f)
    {
        n = 1.0f / max(ussTerrXform.x * texel, 9.9999999600419720025001879548654e-13f);
    }
    float2 tmp10;
    if (ussCsmStable.z > 0.5f)
    {
        tmp10 = pcFragCoord.xy;
    }
    else
    {
        tmp10 = mod(floor(wxy * (4.0f / max(texel, 9.9999999747524270787835121154785e-07f))), 4096.0f.xx);
    }
    float2 tmp11 = tmp10;
    float rot = 6.283185482025146484375f * ussIgn(tmp11);
    float sum = c;
    for (int t = 0; t < 8; t++)
    {
        float th = (float(t) * 2.3999631404876708984375f) + rot;
        float2 o = float2(cos(th), sin(th)) * ((sqrt((float(t) + 0.5f) / 8.0f) * r) / n);
        float2 tmp12 = clamp(uv + o, 0.0f.xx, 1.0f.xx);
        int tmp13 = tier;
        float2 tmp14 = ussTerrAtlasUv(tmp12, tmp13);
        float2 tmp15 = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp14, 0.0f).xy;
        float tmp16 = z;
        sum += ussTerrVis(tmp15, tmp16);
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
    float2 tmp1 = uv;
    float tmp2 = ussTerrExtra.x;
    float2 tmp3 = ussTerrExtraUv(tmp1, tmp2);
    float c = ussTerrMap.Sample(_ussTerrMap_sampler, tmp3).x;
    float2 tmp4 = uv + float2(d, 0.0f);
    float tmp5 = ussTerrExtra.x;
    float2 tmp6 = ussTerrExtraUv(tmp4, tmp5);
    float hE = ussTerrMap.Sample(_ussTerrMap_sampler, tmp6).x;
    float2 tmp7 = uv - float2(d, 0.0f);
    float tmp8 = ussTerrExtra.x;
    float2 tmp9 = ussTerrExtraUv(tmp7, tmp8);
    float hW = ussTerrMap.Sample(_ussTerrMap_sampler, tmp9).x;
    float2 tmp10 = uv + float2(0.0f, d);
    float tmp11 = ussTerrExtra.x;
    float2 tmp12 = ussTerrExtraUv(tmp10, tmp11);
    float hN = ussTerrMap.Sample(_ussTerrMap_sampler, tmp12).x;
    float2 tmp13 = uv - float2(0.0f, d);
    float tmp14 = ussTerrExtra.x;
    float2 tmp15 = ussTerrExtraUv(tmp13, tmp14);
    float hS = ussTerrMap.Sample(_ussTerrMap_sampler, tmp15).x;
    bool tmp16 = c <= 0.0f;
    bool tmp17;
    if (!tmp16)
    {
        tmp17 = min(min(hE, hW), min(hN, hS)) <= 0.0f;
    }
    else
    {
        tmp17 = tmp16;
    }
    if (tmp17)
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
    float2 tmp1 = uv;
    float tmp2 = ussTerrExtra.x;
    float2 tmp3 = ussTerrExtraUv(tmp1, tmp2);
    float c = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp3, 0.0f).x;
    float2 tmp4 = uv + float2(d, 0.0f);
    float tmp5 = ussTerrExtra.x;
    float2 tmp6 = ussTerrExtraUv(tmp4, tmp5);
    float hE = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp6, 0.0f).x;
    float2 tmp7 = uv - float2(d, 0.0f);
    float tmp8 = ussTerrExtra.x;
    float2 tmp9 = ussTerrExtraUv(tmp7, tmp8);
    float hW = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp9, 0.0f).x;
    float2 tmp10 = uv + float2(0.0f, d);
    float tmp11 = ussTerrExtra.x;
    float2 tmp12 = ussTerrExtraUv(tmp10, tmp11);
    float hN = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp12, 0.0f).x;
    float2 tmp13 = uv - float2(0.0f, d);
    float tmp14 = ussTerrExtra.x;
    float2 tmp15 = ussTerrExtraUv(tmp13, tmp14);
    float hS = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp15, 0.0f).x;
    bool tmp16 = c <= 0.0f;
    bool tmp17;
    if (!tmp16)
    {
        tmp17 = min(min(hE, hW), min(hN, hS)) <= 0.0f;
    }
    else
    {
        tmp17 = tmp16;
    }
    if (tmp17)
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
    float2 tmp1 = uv;
    int tmp2 = 0;
    float2 tmp3 = ussTerrAtlasUv(tmp1, tmp2);
    float2 rg = ussTerrMap.Sample(_ussTerrMap_sampler, tmp3).xy;
    float2 tmp4 = uv;
    float tmp5 = ussTerrExtra.x;
    float2 tmp6 = ussTerrExtraUv(tmp4, tmp5);
    float cast = ussTerrMap.Sample(_ussTerrMap_sampler, tmp6).y;
    if ((rg.x <= 0.0f) || (cast <= 0.0f))
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
    float tmp7;
    if (lo > 0.0f)
    {
        tmp7 = fade * smoothstep(0.0f, 1.0f, ((hc - wz) - lo) / lo);
    }
    else
    {
        tmp7 = fade;
    }
    return tmp7;
}
#endif

#if defined(WW_FN_ussTerrSelfWeight_V2) || defined(WW_FN_ALL)
float ussTerrSelfWeight(float2 uv, float wz, float4 h)
{
    float2 tmp1 = uv;
    int tmp2 = 0;
    float2 tmp3 = ussTerrAtlasUv(tmp1, tmp2);
    float2 rg = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp3, 0.0f).xy;
    float2 tmp4 = uv;
    float tmp5 = ussTerrExtra.x;
    float2 tmp6 = ussTerrExtraUv(tmp4, tmp5);
    float cast = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp6, 0.0f).y;
    if ((rg.x <= 0.0f) || (cast <= 0.0f))
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
    float tmp7;
    if (lo > 0.0f)
    {
        tmp7 = fade * smoothstep(0.0f, 1.0f, ((hc - wz) - lo) / lo);
    }
    else
    {
        tmp7 = fade;
    }
    return tmp7;
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
    bool tmp1 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    inMap = 1.0f;
    float4 h = 0.0f.xxxx;
    bool tmp3;
    if (pen)
    {
        bool tmp4 = ussTerrBias.x > 1.5f;
        bool tmp5;
        if (!tmp4)
        {
            tmp5 = ussTerrExtra.y > 0.0f;
        }
        else
        {
            tmp5 = tmp4;
        }
        tmp3 = tmp5;
    }
    else
    {
        tmp3 = pen;
    }
    if (tmp3)
    {
        float2 tmp6 = uv;
        h = ussTerrHeights(tmp6);
    }
    bool tmp7;
    if (pen)
    {
        tmp7 = ussTerrBias.x > 1.5f;
    }
    else
    {
        tmp7 = pen;
    }
    if (tmp7)
    {
        float tmp8;
        if (ussTerrBias.x > 2.5f)
        {
            tmp8 = 0.0f;
        }
        else
        {
            tmp8 = h.z;
        }
        grad = tmp8;
    }
    float tmp9;
    if (ussTerrBias.x > 0.5f)
    {
        tmp9 = (12.0f * (1.0f + clamp(grad, 0.0f, 4.0f))) / ussTerrBias.y;
    }
    else
    {
        tmp9 = ussTerrRange.z;
    }
    float z = wp.z + tmp9;
    float2 tmp10 = uv;
    int tmp11 = 0;
    float tmp12 = z;
    float tmp13 = ussTerrPen.y;
    bool tmp14 = pen;
    float2 tmp15 = wp.xy;
    float vis = ussTerrTier(tmp10, tmp11, tmp12, tmp13, tmp14, tmp15);
    if (ussTerrSize.y > 0.0f)
    {
        float2 un = float2((wp.x * ussTerrNearXform.x) + ussTerrNearXform.y, (wp.y * ussTerrNearXform.z) + ussTerrNearXform.w);
        float2 en = (min(un, 1.0f.xx - un) * ussTerrSize.y) / 128.0f.xx;
        float w = smoothstep(0.0f, 1.0f, clamp(min(en.x, en.y), 0.0f, 1.0f));
        if (w > 0.0f)
        {
            float2 tmp16 = un;
            int tmp17 = 1;
            float tmp18 = z;
            float tmp19 = ussTerrBias.w;
            bool tmp20 = pen;
            float2 tmp21 = wp.xy;
            vis = lerp(vis, ussTerrTier(tmp16, tmp17, tmp18, tmp19, tmp20, tmp21), w);
        }
    }
    if (ussTerrExtra.y > 0.0f)
    {
        bool tmp22 = h.w > 0.0f;
        bool tmp23;
        if (tmp22)
        {
            tmp23 = ussTerrSelf.x > 0.5f;
        }
        else
        {
            tmp23 = tmp22;
        }
        if (tmp23)
        {
            float2 tmp24 = uv;
            float tmp25 = ussTerrExtra.y;
            float2 tmp26 = ussTerrExtraUv(tmp24, tmp25);
            float2 tmp27 = ussTerrMap.Sample(_ussTerrMap_sampler, tmp26).xy;
            float tmp28 = z;
            float2 tmp29 = uv;
            float tmp30 = wp.z;
            float4 tmp31 = h;
            vis = min(ussTerrVis(tmp27, tmp28), lerp(1.0f, vis, ussTerrSelfWeight(tmp29, tmp30, tmp31)));
        }
        else
        {
            if (h.w > 0.0f)
            {
                float tmp32;
                if (ussTerrExtra.w < 0.0f)
                {
                    tmp32 = 1.0f;
                }
                else
                {
                    tmp32 = smoothstep(0.75f * ussTerrExtra.w, 1.25f * ussTerrExtra.w, (wp.z - h.x) - min(h.y, 4.0f * ussTerrPen.y));
                }
                float m = tmp32;
                if (m > 0.0f)
                {
                    float2 tmp33 = uv;
                    float tmp34 = ussTerrExtra.y;
                    float2 tmp35 = ussTerrExtraUv(tmp33, tmp34);
                    float2 tmp36 = ussTerrMap.Sample(_ussTerrMap_sampler, tmp35).xy;
                    float tmp37 = z;
                    vis = lerp(vis, ussTerrVis(tmp36, tmp37), m);
                }
            }
        }
    }
    float2 e = min(uv, 1.0f.xx - uv) / max(ussTerrRange.w, 9.9999999747524270787835121154785e-07f).xx;
    float tmp38;
    if (ussTerrRange.w < 0.0f)
    {
        tmp38 = 1.0f - smoothstep(0.800000011920928955078125f, 1.0f, length(wp.xy - ussWorldFromView[3].xy) / (-ussTerrRange.w));
    }
    else
    {
        tmp38 = clamp(min(e.x, e.y), 0.0f, 1.0f);
    }
    float edgeW = tmp38;
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
    bool tmp1 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
    {
        return 1.0f;
    }
    inMap = 1.0f;
    float4 h = 0.0f.xxxx;
    bool tmp3;
    if (pen)
    {
        bool tmp4 = ussTerrBias.x > 1.5f;
        bool tmp5;
        if (!tmp4)
        {
            tmp5 = ussTerrExtra.y > 0.0f;
        }
        else
        {
            tmp5 = tmp4;
        }
        tmp3 = tmp5;
    }
    else
    {
        tmp3 = pen;
    }
    if (tmp3)
    {
        float2 tmp6 = uv;
        h = ussTerrHeights(tmp6);
    }
    bool tmp7;
    if (pen)
    {
        tmp7 = ussTerrBias.x > 1.5f;
    }
    else
    {
        tmp7 = pen;
    }
    if (tmp7)
    {
        float tmp8;
        if (ussTerrBias.x > 2.5f)
        {
            tmp8 = 0.0f;
        }
        else
        {
            tmp8 = h.z;
        }
        grad = tmp8;
    }
    float tmp9;
    if (ussTerrBias.x > 0.5f)
    {
        tmp9 = (12.0f * (1.0f + clamp(grad, 0.0f, 4.0f))) / ussTerrBias.y;
    }
    else
    {
        tmp9 = ussTerrRange.z;
    }
    float z = wp.z + tmp9;
    float2 tmp10 = uv;
    int tmp11 = 0;
    float tmp12 = z;
    float tmp13 = ussTerrPen.y;
    bool tmp14 = pen;
    float2 tmp15 = wp.xy;
    float vis = ussTerrTier(tmp10, tmp11, tmp12, tmp13, tmp14, tmp15);
    if (ussTerrSize.y > 0.0f)
    {
        float2 un = float2((wp.x * ussTerrNearXform.x) + ussTerrNearXform.y, (wp.y * ussTerrNearXform.z) + ussTerrNearXform.w);
        float2 en = (min(un, 1.0f.xx - un) * ussTerrSize.y) / 128.0f.xx;
        float w = smoothstep(0.0f, 1.0f, clamp(min(en.x, en.y), 0.0f, 1.0f));
        if (w > 0.0f)
        {
            float2 tmp16 = un;
            int tmp17 = 1;
            float tmp18 = z;
            float tmp19 = ussTerrBias.w;
            bool tmp20 = pen;
            float2 tmp21 = wp.xy;
            vis = lerp(vis, ussTerrTier(tmp16, tmp17, tmp18, tmp19, tmp20, tmp21), w);
        }
    }
    if (ussTerrExtra.y > 0.0f)
    {
        bool tmp22 = h.w > 0.0f;
        bool tmp23;
        if (tmp22)
        {
            tmp23 = ussTerrSelf.x > 0.5f;
        }
        else
        {
            tmp23 = tmp22;
        }
        if (tmp23)
        {
            float2 tmp24 = uv;
            float tmp25 = ussTerrExtra.y;
            float2 tmp26 = ussTerrExtraUv(tmp24, tmp25);
            float2 tmp27 = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp26, 0.0f).xy;
            float tmp28 = z;
            float2 tmp29 = uv;
            float tmp30 = wp.z;
            float4 tmp31 = h;
            vis = min(ussTerrVis(tmp27, tmp28), lerp(1.0f, vis, ussTerrSelfWeight(tmp29, tmp30, tmp31)));
        }
        else
        {
            if (h.w > 0.0f)
            {
                float tmp32;
                if (ussTerrExtra.w < 0.0f)
                {
                    tmp32 = 1.0f;
                }
                else
                {
                    tmp32 = smoothstep(0.75f * ussTerrExtra.w, 1.25f * ussTerrExtra.w, (wp.z - h.x) - min(h.y, 4.0f * ussTerrPen.y));
                }
                float m = tmp32;
                if (m > 0.0f)
                {
                    float2 tmp33 = uv;
                    float tmp34 = ussTerrExtra.y;
                    float2 tmp35 = ussTerrExtraUv(tmp33, tmp34);
                    float2 tmp36 = ussTerrMap.SampleLevel(_ussTerrMap_sampler, tmp35, 0.0f).xy;
                    float tmp37 = z;
                    vis = lerp(vis, ussTerrVis(tmp36, tmp37), m);
                }
            }
        }
    }
    float2 e = min(uv, 1.0f.xx - uv) / max(ussTerrRange.w, 9.9999999747524270787835121154785e-07f).xx;
    float tmp38;
    if (ussTerrRange.w < 0.0f)
    {
        tmp38 = 1.0f - smoothstep(0.800000011920928955078125f, 1.0f, length(wp.xy - ussWorldFromView[3].xy) / (-ussTerrRange.w));
    }
    else
    {
        tmp38 = clamp(min(e.x, e.y), 0.0f, 1.0f);
    }
    float edgeW = tmp38;
    vis = lerp(1.0f, vis, edgeW);
    return lerp(1.0f, vis, ussTerrBias.z);
}
#endif

#if defined(WW_FN_ussSlope) || defined(WW_FN_ALL)
float ussSlope(float3 dPx, float3 dPy)
{
    float3 n = cross(dPx, dPy);
    float l = length(n);
    bool tmp1 = l < 9.9999996826552253889678874634872e-21f;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = ussTerrBias.x < 0.5f;
    }
    else
    {
        tmp2 = tmp1;
    }
    if (tmp2)
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
    float3 tmp1 = posView;
    float3 tmp2 = dPx;
    float3 tmp3 = dPy;
    float tmp4;
    float tmp5 = ussCascades(tmp1, tmp2, tmp3, tmp4);
    float w = tmp4;
    float casc = tmp5;
    float3 tmp6 = dPx;
    float3 tmp7 = dPy;
    float3 tmp8 = posView;
    float tmp9 = ussSlope(tmp6, tmp7);
    bool tmp10 = true;
    float tmp11;
    float tmp12 = ussTerrain(tmp8, tmp11, tmp9, tmp10);
    float inMap = tmp11;
    float terr = tmp12;
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
    float3 tmp1 = posView;
    float3 tmp2 = dPx;
    float3 tmp3 = dPy;
    float tmp4;
    float tmp5 = ussCascades(tmp1, tmp2, tmp3, tmp4);
    float w = tmp4;
    float casc = tmp5;
    float3 tmp6 = dPx;
    float3 tmp7 = dPy;
    float3 tmp8 = posView;
    float tmp9 = ussSlope(tmp6, tmp7);
    bool tmp10 = true;
    float tmp11;
    float tmp12 = ussTerrain(tmp8, tmp11, tmp9, tmp10);
    float inMap = tmp11;
    float terr = tmp12;
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
        float3 tmp1 = posView;
        float3 tmp2 = ddx(posView);
        float3 tmp3 = ddy(posView);
        csmNOff = csmNormalOff(tmp1, tmp2, tmp3);
    }
    if (ussOn != 0)
    {
        float3 tmp4 = posView;
        float tmp5 = ussCompose(tmp4);
        return tmp5;
    }
    float dv = (-posView.z) * csmParams.x;
    if (dv > (csmParams.y + csmParams.w))
    {
        return 1.0f;
    }
    float3 tmp6 = posView;
    float sh = csmBlended(tmp6);
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
        float3 tmp1 = posView;
        float3 tmp2 = pcDPx;
        float3 tmp3 = pcDPy;
        csmNOff = csmNormalOff(tmp1, tmp2, tmp3);
    }
    if (ussOn != 0)
    {
        float3 tmp4 = posView;
        float tmp5 = ussCompose(tmp4);
        return tmp5;
    }
    float dv = (-posView.z) * csmParams.x;
    if (dv > (csmParams.y + csmParams.w))
    {
        return 1.0f;
    }
    float3 tmp6 = posView;
    float sh = csmBlended(tmp6);
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
    float3 tmp1 = posView;
    float tmp2 = wwSunShadowGeo(tmp1);
    float3 tmp3 = posView;
    return tmp2 * ussCloudT(tmp3);
}
#endif

#if defined(WW_FN_wwAirTailShadow) || defined(WW_FN_ALL)
float wwAirTailShadow(float3 dirView, float t0, float t1, float tau)
{
    bool tmp1 = ussOn != 0;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = ussTail.x > 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool terr = tmp2 && (ussTerrOn != 0);
    bool tmp3 = ussCloudOn != 0;
    bool tmp4;
    if (tmp3)
    {
        tmp4 = ussCloudFar.z > 0.0f;
    }
    else
    {
        tmp4 = tmp3;
    }
    bool cloud = tmp4;
    if (((!terr) && (!cloud)) || (t1 <= t0))
    {
        return 1.0f;
    }
    float tmp5;
    if (terr)
    {
        tmp5 = ussTail.y;
    }
    else
    {
        tmp5 = ussCloudFar.w;
    }
    float a = clamp(tmp5, 0.0f, 1.0f);
    float3 p0 = dirView * (t0 / fogDistScale);
    float tmp6;
    float inMap;
    if (terr)
    {
        float3 tmp7 = p0;
        float tmp8 = 0.0f;
        bool tmp9 = false;
        float tmp10;
        float tmp11 = ussTerrain(tmp7, tmp10, tmp8, tmp9);
        inMap = tmp10;
        tmp6 = tmp11;
    }
    else
    {
        tmp6 = 1.0f;
    }
    float tmp12;
    if (cloud)
    {
        float3 tmp13 = p0;
        tmp12 = ussCloudT(tmp13);
    }
    else
    {
        tmp12 = 1.0f;
    }
    float v0 = tmp6 * tmp12;
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
        float2 tmp14;
        if (dw.z >= 0.0f)
        {
            tmp14 = dw.xy;
        }
        else
        {
            tmp14 = (1.0f.xx - abs(dw.yx)) * float2((dw.x >= 0.0f) ? 1.0f : (-1.0f), (dw.y >= 0.0f) ? 1.0f : (-1.0f));
        }
        float2 oct = tmp14;
        key = mod(floor(oct * 2048.0f), 4096.0f.xx);
    }
    else
    {
        float3 w1 = mul(float4(dirView * (t1 / fogDistScale), 1.0f), ussWorldFromView).xyz;
        float cell = exp2(max(ceil(log2(max(t1, 1.0f) / 2048.0f)), 0.0f));
        float3 c = floor(w1 / cell.xxx);
        key = mod(c.xy + (float2(37.0f, 61.0f) * c.z), 4096.0f.xx);
    }
    float2 tmp15;
    if (ussCsmStable.z > 0.5f)
    {
        tmp15 = gl_FragCoord.xy;
    }
    else
    {
        tmp15 = key;
    }
    float2 tmp16 = tmp15;
    float jit = ussIgn(tmp16);
    float acc = 0.0f;
    float tmp17;
    float tmp18;
    float tmp19;
    float tmp20;
    for (int i = 0; i < 8; i++)
    {
        float q = (float(i) + jit) / 8.0f;
        if (tau > 9.9999997473787516355514526367188e-05f)
        {
            tmp17 = (-log(max(1.0f - (q * e), 9.9999999747524270787835121154785e-07f))) / tau;
        }
        else
        {
            tmp17 = q;
        }
        float u = tmp17;
        float3 p = dirView * (lerp(t0, t1, clamp(u, 0.0f, 1.0f)) / fogDistScale);
        if (terr)
        {
            float3 tmp21 = p;
            float tmp22 = 0.0f;
            bool tmp23 = false;
            float tmp24 = ussTerrain(tmp21, tmp19, tmp22, tmp23);
            inMap = tmp19;
            tmp18 = tmp24;
        }
        else
        {
            tmp18 = 1.0f;
        }
        if (cloud)
        {
            float3 tmp25 = p;
            tmp20 = ussCloudT(tmp25);
        }
        else
        {
            tmp20 = 1.0f;
        }
        acc += (tmp18 * tmp20);
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
