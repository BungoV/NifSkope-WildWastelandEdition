// Shared HLSL for res/shaders/features/air/cell_volpart.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, particles_cell_ps.hlsl
// Not here (declarations, or no converted entry carried them): pcDropPi, pcDropCube, vpLight
#ifdef WW_FN_ALL
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1
#endif
static const float PC_DROP_G = 0.7668;
#endif

#if defined(WW_FN_vpAmbKeep) || defined(WW_FN_ALL)
float vpAmbKeep(float3 posView)
{
    if (volEnvGrid.w < 0.5f)
    {
        return 1.0f;
    }
    float d = length(posView);
    float4 c = mul(float4(posView, 1.0f), projectionMatrix);
    bool tmp1 = d <= volSlices.x;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = c.w <= 0.0f;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        float2 tmp4 = abs(c.xy);
        float2 tmp5 = c.w.xx;
        tmp3 = any(bool2(tmp4.x > tmp5.x, tmp4.y > tmp5.y));
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return 1.0f;
    }
    int3 n = int3(volEnvGrid.xyz + 0.5f.xxx);
    float f = (log(d / volSlices.x) / volSlices.y) * float(n.z);
    if (f >= float(n.z))
    {
        return 1.0f;
    }
    int2 xc = clamp(int2((((c.xy / c.w.xx) * 0.5f) + 0.5f.xx) * float2(n.xy)), int2(0, 0), n.xy - int2(1, 1));
    int xk = clamp(int(f), 0, n.z - 1);
    float4 h1 = volEnvHist.Load(int4(int3(xc, (5 * n.z) + xk), 0));
    if (h1.z <= 0.99989998340606689453125f)
    {
        return 1.0f;
    }
    float tmp6 = volEnvHist.Load(int4(int3(xc, (4 * n.z) + xk), 0)).w;
    float tmp7 = h1.y;
    return airAmbKeep(tmp6, tmp7);
}
#endif

#if defined(WW_FN_vpLocal) || defined(WW_FN_ALL)
bool vpLocal()
{
    return fxLocalOn() && (!cellInterior);
}
#endif

#if defined(WW_FN_vpSunColour) || defined(WW_FN_ALL)
float3 vpSunColour()
{
    float3 tmp1;
    if (vpLocal())
    {
        tmp1 = vpSun.xyz;
    }
    else
    {
        tmp1 = cellDirColor;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpHasSun) || defined(WW_FN_ALL)
bool vpHasSun()
{
    bool tmp1;
    if (vpLocal())
    {
        tmp1 = any(bool3(vpSun.xyz.x > 0.0f.xxx.x, vpSun.xyz.y > 0.0f.xxx.y, vpSun.xyz.z > 0.0f.xxx.z));
    }
    else
    {
        tmp1 = cellHasDir;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpVeilK) || defined(WW_FN_ALL)
float vpVeilK(float3 Jenv, float t, float z)
{
    bool tmp1 = (!volVeil) || cellInterior;
    bool tmp2;
    if (!tmp1)
    {
        bool tmp3;
        if (cellOn)
        {
            tmp3 = vpHasSun();
        }
        else
        {
            tmp3 = cellOn;
        }
        tmp2 = !tmp3;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp4;
    if (!tmp2)
    {
        tmp4 = (volTerms & 1) == 0;
    }
    else
    {
        tmp4 = tmp2;
    }
    if (tmp4)
    {
        return 1.0f;
    }
    float tmp5 = t;
    float tmp6 = z;
    float tmp7;
    float3 tmp8;
    float tmp9 = wwFogEval(tmp5, tmp6, tmp7, tmp8);
    float hb = tmp7;
    float3 fc = tmp8;
    float rec = dot(max(fc, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float open = (0.25f * dot(vpSunColour(), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))) + dot(Jenv, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float tmp10;
    if ((open > rec) && (open > 9.9999999392252902907785028219223e-09f))
    {
        tmp10 = rec / open;
    }
    else
    {
        tmp10 = 1.0f;
    }
    return tmp10;
}
#endif

#if defined(WW_FN_vpPhase) || defined(WW_FN_ALL)
float3 vpPhase(float c)
{
    float3 tmp1;
    if ((volPartRed & 256) != 0)
    {
        tmp1 = volGi.xyz * 0.25f;
    }
    else
    {
        float tmp2 = c;
        tmp1 = volPhasePi(tmp2);
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpPhaseN) || defined(WW_FN_ALL)
float3 vpPhaseN(float c)
{
    float tmp1 = c;
    return vpPhase(tmp1) / max(volGi.xyz * 0.25f, 9.9999999747524270787835121154785e-07f.xxx);
}
#endif

#if defined(WW_FN_vpEdgeFade) || defined(WW_FN_ALL)
float vpEdgeFade(float nv)
{
    float tmp1;
    if ((volPartRed & 128) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = smoothstep(0.0500000007450580596923828125f, 0.3499999940395355224609375f, nv);
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpNearFade) || defined(WW_FN_ALL)
float vpNearFade(float depth)
{
    float tmp1;
    if ((volPartRed & 512) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = smoothstep(16.0f, 128.0f, depth);
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpPuffAlpha) || defined(WW_FN_ALL)
float vpPuffAlpha(float a0, float2 q)
{
    if (((volPartRed & 2) != 0) || (vpTiled != 0))
    {
        return a0;
    }
    float l = sqrt(max(1.0f - dot(q, q), 0.0f));
    return 1.0f - pow(1.0f - clamp(a0, 0.0f, 0.999000012874603271484375f), 1.5f * l);
}
#endif

#if defined(WW_FN_vpTint) || defined(WW_FN_ALL)
float3 vpTint(float3 e)
{
    float3 tmp1;
    if ((volPartRed & 16) != 0)
    {
        tmp1 = dot(e, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)).xxx;
    }
    else
    {
        tmp1 = e;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_vpWrap) || defined(WW_FN_ALL)
float vpWrap(float nl, float w)
{
    return max(nl + w, 0.0f) / (1.0f + w);
}
#endif

#if defined(WW_FN_vpCubeAt) || defined(WW_FN_ALL)
float3 vpCubeAt(float3 C_1[6], float3 N)
{
    float3 n2 = N * N;
    float3 tmp1;
    if (N.x >= 0.0f)
    {
        tmp1 = C_1[0];
    }
    else
    {
        tmp1 = C_1[1];
    }
    float3 tmp2;
    if (N.y >= 0.0f)
    {
        tmp2 = C_1[2];
    }
    else
    {
        tmp2 = C_1[3];
    }
    float3 tmp3;
    if (N.z >= 0.0f)
    {
        tmp3 = C_1[4];
    }
    else
    {
        tmp3 = C_1[5];
    }
    return ((tmp1 * n2.x) + (tmp2 * n2.y)) + (tmp3 * n2.z);
}
#endif

#if defined(WW_FN_vpCubeAt_V2) || defined(WW_FN_ALL)
float3 vpCubeAt(float3 C[6], float3 N)
{
    float3 n2 = N * N;
    float3 tmp1;
    if (N.x >= 0.0f)
    {
        tmp1 = C[0];
    }
    else
    {
        tmp1 = C[1];
    }
    float3 tmp2;
    if (N.y >= 0.0f)
    {
        tmp2 = C[2];
    }
    else
    {
        tmp2 = C[3];
    }
    float3 tmp3;
    if (N.z >= 0.0f)
    {
        tmp3 = C[4];
    }
    else
    {
        tmp3 = C[5];
    }
    return ((tmp1 * n2.x) + (tmp2 * n2.y)) + (tmp3 * n2.z);
}
#endif

#if defined(WW_FN_vpLightsBegin) || defined(WW_FN_ALL)
int vpLightsBegin(float3 P, float3 posView, out int n)
{
    n = cellLightCount;
    bool tmp1 = !cellClusterOn;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = (volPartRed & 1024) != 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = volRect.z <= 0.0f;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = volRect.w <= 0.0f;
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return -1;
    }
    float4 c = mul(float4(posView, 1.0f), projectionMatrix);
    bool tmp5 = c.w <= 0.0f;
    bool tmp6;
    if (!tmp5)
    {
        float3 tmp7 = abs(c.xyz);
        float3 tmp8 = c.w.xxx;
        tmp6 = any(bool3(tmp7.x > tmp8.x, tmp7.y > tmp8.y, tmp7.z > tmp8.z));
    }
    else
    {
        tmp6 = tmp5;
    }
    if (tmp6)
    {
        return -1;
    }
    float2 px = volRect.xy + ((((c.xy / c.w.xx) * 0.5f) + 0.5f.xx) / volRect.zw);
    int3 d = int3(cellClusterDims.xyz + 0.5f.xxx);
    int2 t = clamp(int2(floor((px - cellClusterOrigin) / cellClusterDims.w.xx)), int2(0, 0), d.xy - int2(1, 1));
    float s = floor((log(max(dot(cellClusterDepth.xyz, P) + cellClusterDepth.w, 9.9999999747524270787835121154785e-07f)) - cellClusterLog.x) * cellClusterLog.y);
    int cl = (((clamp(int(s), 0, d.z - 1) * d.y) + t.y) * d.x) + t.x;
    int k = cellCluster.Load((2 * cl) + 1).x;
    if (k < 0)
    {
        return -1;
    }
    n = k;
    return cellClusterBase + cellCluster.Load(2 * cl).x;
}
#endif

#if defined(WW_FN_vpLit) || defined(WW_FN_ALL)
float3 vpLit(float3 posView, float3 Nview, float a)
{
    bool tmp1 = (fxLitMode & 8) != 0;
    bool tmp2;
    if (!tmp1)
    {
        bool tmp3 = (!cellInterior) && (!cellHasDir);
        bool tmp4;
        if (tmp3)
        {
            tmp4 = !vpLocal();
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
        return 1.0f.xxx;
    }
    float tmp5;
    if ((volPartRed & 2) != 0)
    {
        tmp5 = 1.0f;
    }
    else
    {
        tmp5 = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = tmp5;
    float3 tmp6 = posView;
    float3 tmp7 = posView;
    float3 tmp8 = Nview;
    float3 tmp9 = posView;
    float3 tmp10 = cellWorldPos(tmp6);
    float3 tmp11 = cellWorldDir(tmp7);
    float tmp12 = dot(fogView.xyz, posView) + fogView.w;
    float3 tmp13 = cellWorldDir(tmp8);
    float tmp14 = m;
    float3 tmp15;
    float3 tmp16;
    vpLight(tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16);
    float3 thin = tmp15;
    float3 thick = tmp16;
    thin += (vpLampMs * vpAirCardA);
    float3 tmp17 = (thin * m) + (thick * (1.0f - m));
    float tmp18 = lightingInfluence;
    bool tmp19 = false;
    float3 tmp20 = fxLawMix(tmp17, tmp18, tmp19);
    return tmp20;
}
#endif

#if defined(WW_FN_vpLit_V2) || defined(WW_FN_ALL)
float3 vpLit(float3 posView, float3 Nview, float a)
{
    bool tmp1 = (fxLitMode & 8) != 0;
    bool tmp2;
    if (!tmp1)
    {
        bool tmp3 = (!cellInterior) && (!cellHasDir);
        bool tmp4;
        if (tmp3)
        {
            tmp4 = !vpLocal();
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
        return 1.0f.xxx;
    }
    float tmp5;
    if ((volPartRed & 2) != 0)
    {
        tmp5 = 1.0f;
    }
    else
    {
        tmp5 = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = tmp5;
    float3 tmp6 = posView;
    float3 tmp7 = posView;
    float3 tmp8 = Nview;
    float3 tmp9 = posView;
    float3 tmp10 = cellWorldPos(tmp6);
    float3 tmp11 = cellWorldDir(tmp7);
    float tmp12 = dot(fogView.xyz, posView) + fogView.w;
    float3 tmp13 = cellWorldDir(tmp8);
    float tmp14 = m;
    float3 tmp15;
    float3 tmp16;
    vpLight(tmp9, tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16);
    float3 thin = tmp15;
    float3 thick = tmp16;
    float3 tmp17 = (thin * m) + (thick * (1.0f - m));
    float tmp18 = lightingInfluence;
    bool tmp19 = false;
    return fxLawMix(tmp17, tmp18, tmp19);
}
#endif

#if defined(WW_FN_vpOneLight) || defined(WW_FN_ALL)
float3 vpOneLight(float3 dW, float3 N, float3 to, float a)
{
    float tmp1;
    if ((volPartRed & 2) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = tmp1;
    float c = dot(to, dW);
    float tmp2 = c;
    float tmp3 = dot(N, to);
    float tmp4 = m;
    float tmp5 = c;
    return (vpPhase(tmp2) * m) + (vpPhaseN(tmp5) * ((1.0f - m) * vpWrap(tmp3, tmp4)));
}
#endif

#if defined(WW_FN_vpProbe) || defined(WW_FN_ALL)
float4 vpProbe(int slot, float3 posView, float a)
{
    float3 tmp1 = posView;
    float3 P = cellWorldPos(tmp1);
    float3 tmp2 = posView;
    float3 dW = cellWorldDir(tmp2);
    float z = dot(fogView.xyz, posView) + fogView.w;
    float3 tmp3 = float3(0.0f, 0.0f, 1.0f);
    float3 Zw = cellWorldDir(tmp3);
    float3 thin;
    float3 thick;
    if (slot <= 1)
    {
        float3 tmp4 = posView;
        float3 tmp5 = P;
        float3 tmp6 = dW;
        float tmp7 = z;
        float3 tmp8 = Zw;
        float tmp9 = 0.0f;
        float3 tmp10;
        float3 tmp11;
        vpLight(tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11);
        thin = tmp10;
        thick = tmp11;
        bool3 tmp12 = (slot == 0).xxx;
        return float4(float3(tmp12.x ? thin.x : thick.x, tmp12.y ? thin.y : thick.y, tmp12.z ? thin.z : thick.z), 1.0f);
    }
    if (slot == 2)
    {
        float3 tmp13 = posView;
        float3 tmp14 = P;
        float3 tmp15 = dW;
        float tmp16 = z;
        float3 tmp17 = -Zw;
        float tmp18 = 0.0f;
        float3 tmp19;
        float3 tmp20;
        vpLight(tmp13, tmp14, tmp15, tmp16, tmp17, tmp18, tmp19, tmp20);
        thin = tmp19;
        thick = tmp20;
        return float4(thick, 1.0f);
    }
    if (slot == 3)
    {
        return float4(P, z);
    }
    if (slot == 4)
    {
        return float4(dW, length(posView) * fogDistScale);
    }
    if (slot == 5)
    {
        float4 c = mul(float4(posView, 1.0f), projectionMatrix);
        bool tmp21;
        if (fogOn)
        {
            tmp21 = (volPartRed & 32) == 0;
        }
        else
        {
            tmp21 = fogOn;
        }
        bool tmp22 = tmp21 && volOn;
        bool tmp23;
        if (tmp22)
        {
            tmp23 = volSlices.w > 0.5f;
        }
        else
        {
            tmp23 = tmp22;
        }
        float4 tmp24;
        if (tmp23)
        {
            float3 tmp25 = posView;
            float2 tmp26 = ((c.xy / c.w.xx) * 0.5f) + 0.5f.xx;
            tmp24 = wwVolAirAtUv(tmp25, tmp26);
        }
        else
        {
            tmp24 = float4(0.0f, 0.0f, 0.0f, -1.0f);
        }
        return tmp24;
    }
    if (slot == 6)
    {
        float3 tmp27 = posView;
        float3 tmp28 = float3(0.0f, 0.0f, 1.0f);
        float tmp29 = a;
        float3 tmp30 = vpLit(tmp27, tmp28, tmp29);
        return float4(tmp30, a);
    }
    return float4(posView, a);
}
#endif
