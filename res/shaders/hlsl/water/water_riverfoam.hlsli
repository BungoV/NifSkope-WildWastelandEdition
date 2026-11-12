// Shared HLSL for res/shaders/features/water/water_riverfoam.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kRfMetre = 0.0142875;
static const float kRfSigma = 0.1586;
static const float kRfCorr = 0.593;
#endif

#if defined(WW_FN_wwRfStreak) || defined(WW_FN_ALL)
float wwRfStreak(float2 s, float2 o)
{
    float2 tmp1 = s + o;
    float2 tmp2 = (s * 2.0299999713897705078125f) + o.yx;
    return ((0.64999997615814208984375f * wwShoreNoise(tmp1)) + (0.3499999940395355224609375f * wwShoreNoise(tmp2))) - 0.5f;
}
#endif

#if defined(WW_FN_wwRfGauss) || defined(WW_FN_ALL)
float wwRfGauss(float x)
{
    float u = x * 6.3051700592041015625f;
    float u2 = u * u;
    return u * (0.919990003108978271484375f + (u2 * (0.023180000483989715576171875f + (u2 * 0.002226999960839748382568359375f))));
}
#endif

#if defined(WW_FN_wwRfRho) || defined(WW_FN_ALL)
float wwRfRho(float2 d)
{
    float e = dot(d, d) / 0.592999994754791259765625f;
    return ((0.42250001430511474609375f * exp(-e)) + (0.122500002384185791015625f * exp((-4.12090015411376953125f) * e))) * 1.83486235141754150390625f;
}
#endif

#if defined(WW_FN_wwWaterRiverFoam) || defined(WW_FN_ALL)
float4 wwWaterRiverFoam(float2 P, float t, float foot)
{
    if ((!waterRiverFoamOn) || (!waterFlowOn))
    {
        return 0.0f.xxxx;
    }
    float2 tmp1 = P;
    if (abs(wwWaterInfo(tmp1).w - 1.0f) > 0.5f)
    {
        return 0.0f.xxxx;
    }
    float tex = waterFlowGrid.z;
    float2 g = ((P - waterFlowGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterFlowGrid.xy + ((t0 + 0.5f.xx) * tex);
    bool raw = (waterRiverFoamRed & 16) != 0;
    float uq = raw ? 1.0f : 69.99124908447265625f;
    float2 tmp2 = c0;
    float2 tmp3 = c0 + float2(tex, 0.0f);
    float2 tmp4 = c0 + float2(0.0f, tex);
    float2 tmp5 = c0 + tex.xx;
    float2 tmp6[4] = { wwWaterFlow(tmp2), wwWaterFlow(tmp3), wwWaterFlow(tmp4), wwWaterFlow(tmp5) };
    float2 vc[4] = tmp6;
    for (int i = 0; i < 4; i++)
    {
        vc[i] *= uq;
    }
    float2 v = lerp(lerp(vc[0], vc[1], f.x.xx), lerp(vc[2], vc[3], f.x.xx), f.y.xx);
    float spd = length(v);
    if (spd < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xxxx;
    }
    float div = (lerp(vc[1].x - vc[0].x, vc[3].x - vc[2].x, f.y) + lerp(vc[2].y - vc[0].y, vc[3].y - vc[1].y, f.x)) / tex;
    float T = waterRiverFoamK.x;
    float tmp7;
    if ((waterRiverFoamRed & 1) != 0)
    {
        tmp7 = 0.0f;
    }
    else
    {
        tmp7 = clamp((-div) * T, -9.0f, waterRiverFoamK2.w);
    }
    float CT = tmp7;
    float sm = spd / uq;
    float src = (waterRiverFoamK.y * sm) / (sm + waterRiverFoamK.z);
    if (!raw)
    {
        float2 tmp8 = P;
        float dHere = wwWaterInfo(tmp8).y * 0.01428749971091747283935546875f;
        float2 tmp9 = P + ((v / spd.xx) * tex);
        float dDown = wwWaterInfo(tmp9).y * 0.01428749971091747283935546875f;
        float shallow = 1.0f - smoothstep(0.300000011920928955078125f, 1.5f, dHere);
        float shoal = clamp((dHere - dDown) / 0.5f, 0.0f, 1.0f);
        src *= ((1.0f + (2.0f * shallow)) + (2.0f * shoal));
    }
    float D_1 = clamp(src / (1.0f - CT), 0.0f, 1.0f);
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return float4(0.0f, 0.0f, CT, spd);
    }
    float W = waterRiverFoamK2.x;
    float tmp10;
    if ((waterRiverFoamRed & 8) != 0)
    {
        tmp10 = 1.0f;
    }
    else
    {
        tmp10 = 1.0f - smoothstep(0.3499999940395355224609375f, 0.699999988079071044921875f, foot / W);
    }
    float vis = tmp10;
    float z = log((1.0f - D_1) / D_1) / 1.70200002193450927734375f;
    float a = 0.0f;
    if (vis > 0.0f)
    {
        float tmp11;
        if ((waterRiverFoamRed & 4) != 0)
        {
            tmp11 = W;
        }
        else
        {
            tmp11 = W * waterRiverFoamK2.y;
        }
        float L = tmp11;
        float ph = frac(t / T);
        float2 wp = float2(1.0f - abs((2.0f * ph) - 1.0f), abs((2.0f * ph) - 1.0f));
        float2 tmp12;
        if ((waterRiverFoamRed & 2) != 0)
        {
            tmp12 = 0.0f.xx;
        }
        else
        {
            tmp12 = float2(ph, frac(ph + 0.5f)) * T;
        }
        float2 tp = tmp12;
        float2 wc = 1.0f.xx - f;
        float4 wq = float4(wc.x * wc.y, f.x * wc.y, wc.x * f.y, f.x * f.y);
        float n = 0.0f;
        float var = 0.0f;
        float2 tmp13;
        float2 s[4];
        float tmp14;
        for (int k = 0; k < 2; k++)
        {
            bool2 tmp15 = (k == 0).xx;
            float2 o = float2(tmp15.x ? float2(0.37000000476837158203125f, 0.709999978542327880859375f).x : float2(311.3699951171875f, 97.70999908447265625f).x, tmp15.y ? float2(0.37000000476837158203125f, 0.709999978542327880859375f).y : float2(311.3699951171875f, 97.70999908447265625f).y);
            for (int i_1 = 0; i_1 < 4; i_1++)
            {
                float sp = length(vc[i_1]);
                if (sp > 9.9999997473787516355514526367188e-05f)
                {
                    tmp13 = vc[i_1] / sp.xx;
                }
                else
                {
                    tmp13 = float2(1.0f, 0.0f);
                }
                float2 dir = tmp13;
                float2 Pc = c0 + (float2(float(i_1 & 1), float(i_1 >> 1)) * tex);
                float2 blk = floor(Pc * 0.00048828125f);
                float2 r = (P - (blk * 2048.0f)) - (vc[i_1] * tp[k]);
                int2 tmp16 = int2(blk);
                uint hb = wwShoreHash(tmp16);
                s[i_1] = float2(dot(r, dir) / L, dot(r, float2(-dir.y, dir.x)) / W) + (float2(float(hb & 1023u), float((hb >> uint(10)) & 1023u)) * 2.0f);
                float2 tmp17 = s[i_1];
                float2 tmp18 = o;
                float tmp19 = wwRfStreak(tmp17, tmp18);
                n += ((wp[k] * wq[i_1]) * wwRfGauss(tmp19));
            }
            for (int i_2 = 0; i_2 < 4; i_2++)
            {
                for (int j = 0; j < 4; j++)
                {
                    if (i_2 == j)
                    {
                        tmp14 = 1.0f;
                    }
                    else
                    {
                        float2 tmp20 = s[i_2] - s[j];
                        tmp14 = wwRfRho(tmp20);
                    }
                    var += ((((wp[k] * wp[k]) * wq[i_2]) * wq[j]) * tmp14);
                }
            }
        }
        n /= sqrt(max(var, 9.9999999392252902907785028219223e-09f));
        float sf = waterRiverFoamK2.z;
        a = smoothstep(z - sf, z + sf, n);
    }
    a = lerp(D_1, a, vis);
    if (((waterRiverFoamRed & 32) == 0) && (a > 0.00200000009499490261077880859375f))
    {
        float T2 = waterRiverFoamK.x;
        float ph2 = frac(t / T2);
        float w0 = 1.0f - abs((2.0f * ph2) - 1.0f);
        float2 tmp21 = P - (v * (ph2 * T2));
        float tmp22 = foot;
        float tmp23 = t;
        float3 d0 = wwFoamDetail(tmp21, tmp22, tmp23);
        float2 tmp24 = P - (v * (frac(ph2 + 0.5f) * T2));
        float tmp25 = foot;
        float tmp26 = t;
        float3 d1 = wwFoamDetail(tmp24, tmp25, tmp26);
        float3 det = lerp(d1, d0, w0.xxx);
        float dens = 0.60000002384185791015625f * a;
        float s_1 = lerp(0.5f, waterShoreK0.w, det.z);
        a = (smoothstep((1.0f - dens) - s_1, (1.0f - dens) + s_1, det.x) * lerp(det.y, 1.0f, dens * dens)) * min(1.0f, 1.5f * dens);
    }
    a *= waterRiverFoamK.w;
    return float4(D_1, a, CT, spd);
}
#endif

#if defined(WW_FN_wwFoamWet) || defined(WW_FN_ALL)
float wwFoamWet(float edgeV, float column)
{
    float tmp1;
    if ((waterRiverFoamRed & 32) != 0)
    {
        tmp1 = 1.0f;
    }
    else
    {
        tmp1 = (1.0f - clamp(edgeV, 0.0f, 1.0f)) * smoothstep(4.0f, 16.0f, column);
    }
    return tmp1;
}
#endif
