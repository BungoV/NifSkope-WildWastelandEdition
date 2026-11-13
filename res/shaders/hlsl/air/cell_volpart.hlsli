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
    bool cond3 = d <= volSlices.x;
    bool cond2;
    if (!cond3)
    {
        cond2 = c.w <= 0.0f;
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        float2 condPart2 = abs(c.xy);
        float2 condPart = c.w.xx;
        cond = any(bool2(condPart2.x > condPart.x, condPart2.y > condPart.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
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
    float volEnvHistTexel = volEnvHist.Load(int4(int3(xc, (4 * n.z) + xk), 0)).w;
    float vpAmbKeepRet = h1.y;
    return airAmbKeep(volEnvHistTexel, vpAmbKeepRet);
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
    float3 cond3;
    if (vpLocal())
    {
        cond3 = vpSun.xyz;
    }
    else
    {
        cond3 = cellDirColor;
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpHasSun) || defined(WW_FN_ALL)
bool vpHasSun()
{
    bool cond3;
    if (vpLocal())
    {
        cond3 = any(bool3(vpSun.xyz.x > 0.0f.xxx.x, vpSun.xyz.y > 0.0f.xxx.y, vpSun.xyz.z > 0.0f.xxx.z));
    }
    else
    {
        cond3 = cellHasDir;
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpVeilK) || defined(WW_FN_ALL)
float vpVeilK(float3 Jenv, float t, float z)
{
    bool cond3 = (!volVeil) || cellInterior;
    bool cond2;
    if (!cond3)
    {
        bool cond;
        if (cellOn)
        {
            cond = vpHasSun();
        }
        else
        {
            cond = cellOn;
        }
        cond2 = !cond;
    }
    else
    {
        cond2 = cond3;
    }
    bool condPart2;
    if (!cond2)
    {
        condPart2 = (volTerms & 1) == 0;
    }
    else
    {
        condPart2 = cond2;
    }
    if (condPart2)
    {
        return 1.0f;
    }
    float condPart = t;
    float volEnvHistTexel = z;
    float vpAmbKeepRet;
    float3 partPick;
    float part = wwFogEval(condPart, volEnvHistTexel, vpAmbKeepRet, partPick);
    float hb = vpAmbKeepRet;
    float3 fc = partPick;
    float rec = dot(max(fc, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float open = (0.25f * dot(vpSunColour(), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))) + dot(Jenv, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float vpVeilKRet;
    if ((open > rec) && (open > 9.9999999392252902907785028219223e-09f))
    {
        vpVeilKRet = rec / open;
    }
    else
    {
        vpVeilKRet = 1.0f;
    }
    return vpVeilKRet;
}
#endif

#if defined(WW_FN_vpPhase) || defined(WW_FN_ALL)
float3 vpPhase(float c)
{
    float3 cond3;
    if ((volPartRed & 256) != 0)
    {
        cond3 = volGi.xyz * 0.25f;
    }
    else
    {
        float cond2 = c;
        cond3 = volPhasePi(cond2);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpPhaseN) || defined(WW_FN_ALL)
float3 vpPhaseN(float c)
{
    float cond3 = c;
    return vpPhase(cond3) / max(volGi.xyz * 0.25f, 9.9999999747524270787835121154785e-07f.xxx);
}
#endif

#if defined(WW_FN_vpEdgeFade) || defined(WW_FN_ALL)
float vpEdgeFade(float nv)
{
    float cond3;
    if ((volPartRed & 128) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = smoothstep(0.0500000007450580596923828125f, 0.3499999940395355224609375f, nv);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpNearFade) || defined(WW_FN_ALL)
float vpNearFade(float depth)
{
    float cond3;
    if ((volPartRed & 512) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = smoothstep(16.0f, 128.0f, depth);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpPuffAlpha) || defined(WW_FN_ALL)
float vpPuffAlpha(float a0, float2 q)
{
    bool wwh0 = ((volPartRed & 2) != 0);
    bool wwh1 = (vpTiled != 0);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
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
    float3 cond3;
    if ((volPartRed & 16) != 0)
    {
        cond3 = dot(e, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)).xxx;
    }
    else
    {
        cond3 = e;
    }
    return cond3;
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
    float3 cond3;
    if (N.x >= 0.0f)
    {
        cond3 = C_1[0];
    }
    else
    {
        cond3 = C_1[1];
    }
    float3 cond2;
    if (N.y >= 0.0f)
    {
        cond2 = C_1[2];
    }
    else
    {
        cond2 = C_1[3];
    }
    float3 cond;
    if (N.z >= 0.0f)
    {
        cond = C_1[4];
    }
    else
    {
        cond = C_1[5];
    }
    return ((cond3 * n2.x) + (cond2 * n2.y)) + (cond * n2.z);
}
#endif

#if defined(WW_FN_vpCubeAt_V2) || defined(WW_FN_ALL)
float3 vpCubeAt(float3 C[6], float3 N)
{
    float3 n2 = N * N;
    float3 cond3;
    if (N.x >= 0.0f)
    {
        cond3 = C[0];
    }
    else
    {
        cond3 = C[1];
    }
    float3 cond2;
    if (N.y >= 0.0f)
    {
        cond2 = C[2];
    }
    else
    {
        cond2 = C[3];
    }
    float3 cond;
    if (N.z >= 0.0f)
    {
        cond = C[4];
    }
    else
    {
        cond = C[5];
    }
    return ((cond3 * n2.x) + (cond2 * n2.y)) + (cond * n2.z);
}
#endif

#if defined(WW_FN_vpLightsBegin) || defined(WW_FN_ALL)
int vpLightsBegin(float3 P, float3 posView, out int n)
{
    n = cellLightCount;
    bool cond3 = !cellClusterOn;
    bool cond2;
    if (!cond3)
    {
        cond2 = (volPartRed & 1024) != 0;
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = volRect.z <= 0.0f;
    }
    else
    {
        cond = cond2;
    }
    bool condPart2;
    if (!cond)
    {
        condPart2 = volRect.w <= 0.0f;
    }
    else
    {
        condPart2 = cond;
    }
    if (condPart2)
    {
        return -1;
    }
    float4 c = mul(float4(posView, 1.0f), projectionMatrix);
    bool condPart = c.w <= 0.0f;
    bool volEnvHistTexel;
    if (!condPart)
    {
        float3 vpAmbKeepRet = abs(c.xyz);
        float3 partPick = c.w.xxx;
        volEnvHistTexel = any(bool3(vpAmbKeepRet.x > partPick.x, vpAmbKeepRet.y > partPick.y, vpAmbKeepRet.z > partPick.z));
    }
    else
    {
        volEnvHistTexel = condPart;
    }
    if (volEnvHistTexel)
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
    bool cond3 = (fxLitMode & 8) != 0;
    bool cond2;
    if (!cond3)
    {
        bool cond = (!cellInterior) && (!cellHasDir);
        bool condPart2;
        if (cond)
        {
            condPart2 = !vpLocal();
        }
        else
        {
            condPart2 = cond;
        }
        cond2 = condPart2;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        return 1.0f.xxx;
    }
    float condPart;
    if ((volPartRed & 2) != 0)
    {
        condPart = 1.0f;
    }
    else
    {
        condPart = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = condPart;
    float3 volEnvHistTexel = posView;
    float3 vpAmbKeepRet = posView;
    float3 partPick = Nview;
    float3 part = posView;
    float3 vpVeilKRet = cellWorldPos(volEnvHistTexel);
    float3 part7 = cellWorldDir(vpAmbKeepRet);
    float part6 = dot(fogView.xyz, posView) + fogView.w;
    float3 part5 = cellWorldDir(partPick);
    float part4 = m;
    float3 part3;
    float3 part2;
    vpLight(part, vpVeilKRet, part7, part6, part5, part4, part3, part2);
    float3 thin = part3;
    float3 thick = part2;
    thin += (vpLampMs * vpAirCardA);
    float3 vpLitPart3 = (thin * m) + (thick * (1.0f - m));
    float vpLitPart2 = lightingInfluence;
    bool vpLitPart = false;
    float3 vpLitRet = fxLawMix(vpLitPart3, vpLitPart2, vpLitPart);
    return vpLitRet;
}
#endif

#if defined(WW_FN_vpLit_V2) || defined(WW_FN_ALL)
float3 vpLit(float3 posView, float3 Nview, float a)
{
    bool cond3 = (fxLitMode & 8) != 0;
    bool cond2;
    if (!cond3)
    {
        bool cond = (!cellInterior) && (!cellHasDir);
        bool condPart2;
        if (cond)
        {
            condPart2 = !vpLocal();
        }
        else
        {
            condPart2 = cond;
        }
        cond2 = condPart2;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        return 1.0f.xxx;
    }
    float condPart;
    if ((volPartRed & 2) != 0)
    {
        condPart = 1.0f;
    }
    else
    {
        condPart = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = condPart;
    float3 volEnvHistTexel = posView;
    float3 vpAmbKeepRet = posView;
    float3 partPick = Nview;
    float3 part = posView;
    float3 vpVeilKRet = cellWorldPos(volEnvHistTexel);
    float3 part7 = cellWorldDir(vpAmbKeepRet);
    float part6 = dot(fogView.xyz, posView) + fogView.w;
    float3 part5 = cellWorldDir(partPick);
    float part4 = m;
    float3 part3;
    float3 part2;
    vpLight(part, vpVeilKRet, part7, part6, part5, part4, part3, part2);
    float3 thin = part3;
    float3 thick = part2;
    float3 vpLitPart3 = (thin * m) + (thick * (1.0f - m));
    float vpLitPart2 = lightingInfluence;
    bool vpLitPart = false;
    return fxLawMix(vpLitPart3, vpLitPart2, vpLitPart);
}
#endif

#if defined(WW_FN_vpOneLight) || defined(WW_FN_ALL)
float3 vpOneLight(float3 dW, float3 N, float3 to, float a)
{
    float cond3;
    if ((volPartRed & 2) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = cond3;
    float c = dot(to, dW);
    float cond2 = c;
    float cond = dot(N, to);
    float condPart2 = m;
    float condPart = c;
    return (vpPhase(cond2) * m) + (vpPhaseN(condPart) * ((1.0f - m) * vpWrap(cond, condPart2)));
}
#endif

#if defined(WW_FN_vpProbe) || defined(WW_FN_ALL)
float4 vpProbe(int slot, float3 posView, float a)
{
    float3 cond3 = posView;
    float3 P = cellWorldPos(cond3);
    float3 cond2 = posView;
    float3 dW = cellWorldDir(cond2);
    // lane SHADOWPAR1: FO4CS's bias 12 (1 + slope) / sin(elevation); else TERRSHADOW1's 0.05 T + 4
    float z = dot(fogView.xyz, posView) + fogView.w;
    float3 cond = float3(0.0f, 0.0f, 1.0f);
    float3 Zw = cellWorldDir(cond);
    float3 thin;
    float3 thick;
    if (slot <= 1)
    {
        float3 condPart2 = posView;
        float3 condPart = P;
        float3 volEnvHistTexel = dW;
        float vpAmbKeepRet = z;
        float3 partPick = Zw;
        float part = 0.0f;
        float3 vpVeilKRet;
        float3 part7;
        vpLight(condPart2, condPart, volEnvHistTexel, vpAmbKeepRet, partPick, part, vpVeilKRet, part7);
        thin = vpVeilKRet;
        thick = part7;
        bool3 part6 = (slot == 0).xxx;
        return float4(float3(part6.x ? thin.x : thick.x, part6.y ? thin.y : thick.y, part6.z ? thin.z : thick.z), 1.0f);
    }
    if (slot == 2)
    {
        float3 part5 = posView;
        float3 part4 = P;
        float3 part3 = dW;
        float part2 = z;
        float3 vpLitPart3 = -Zw;
        float vpLitPart2 = 0.0f;
        float3 vpLitPart;
        float3 vpLitRet;
        vpLight(part5, part4, part3, part2, vpLitPart3, vpLitPart2, vpLitPart, vpLitRet);
        thin = vpLitPart;
        thick = vpLitRet;
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
        bool condPick;
        if (fogOn)
        {
            condPick = (volPartRed & 32) == 0;
        }
        else
        {
            condPick = fogOn;
        }
        bool cond5 = condPick && volOn;
        bool cond4;
        if (cond5)
        {
            cond4 = volSlices.w > 0.5f;
        }
        else
        {
            cond4 = cond5;
        }
        float4 vpProbeRet;
        if (cond4)
        {
            float3 vpProbePart2 = posView;
            float2 vpProbePart = ((c.xy / c.w.xx) * 0.5f) + 0.5f.xx;
            vpProbeRet = wwVolAirAtUv(vpProbePart2, vpProbePart);
        }
        else
        {
            vpProbeRet = float4(0.0f, 0.0f, 0.0f, -1.0f);
        }
        return vpProbeRet;
    }
    if (slot == 6)
    {
        float3 vpLitPart6 = posView;
        float3 vpLitPart5 = float3(0.0f, 0.0f, 1.0f);
        float vpLitPart4 = a;
        float3 vpLitRet2 = vpLit(vpLitPart6, vpLitPart5, vpLitPart4);
        return float4(vpLitRet2, a);
    }
    return float4(posView, a);
}
#endif
