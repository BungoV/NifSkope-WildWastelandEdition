// Shared HLSL for res/shaders/features/water/water_waves.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kSwellGravity = 686.6;
#endif

#if defined(WW_FN_wwWaterSwell) || defined(WW_FN_ALL)
float4 wwWaterSwell(float2 P, float t, float footprint)
{
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 tmp1 = c0;
    uint w0 = wwWaterInfoWord(tmp1);
    float2 tmp2 = c0 + float2(tex, 0.0f);
    uint w1 = wwWaterInfoWord(tmp2);
    float2 tmp3 = c0 + float2(0.0f, tex);
    uint w2 = wwWaterInfoWord(tmp3);
    float2 tmp4 = c0 + tex.xx;
    uint w3 = wwWaterInfoWord(tmp4);
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = uint4(w0, w1, w2, w3) & uint4(65535u, 65535u, 65535u, 65535u);
    uint body = 0u;
    float best = -1.0f;
    for (int k = 0; k < 4; k++)
    {
        bool tmp5 = id[k] != 0u;
        bool tmp6;
        if (tmp5)
        {
            tmp6 = bw[k] > best;
        }
        else
        {
            tmp6 = tmp5;
        }
        if (tmp6)
        {
            best = bw[k];
            body = id[k];
        }
    }
    if (body == 0u)
    {
        return 0.0f.xxxx;
    }
    float4 steps = float4((uint4(w0, w1, w2, w3) >> uint4(int4(16, 16, 16, 16))) & uint4(255u, 255u, 255u, 255u));
    uint4 tmp7 = body.xxxx;
    float4 same = float4(bool4(id.x == tmp7.x, id.y == tmp7.y, id.z == tmp7.z, id.w == tmp7.w));
    float dist = dot(bw, same * steps) * waterInfoGrid.w;
    float4 row = waterInfoBodies.Load(int3(int2(int(body % 256u), int(body / 256u)), 0));
    int cls = clamp(int(row.y + 0.5f), 0, 2);
    float area = row.w;
    if ((waterWavesRed & 1) != 0)
    {
        cls = 2;
        area = 4096.0f;
    }
    float4 C_1 = waterWavesClass[cls];
    float lam = clamp((sqrt(area) * tex) / C_1.x, C_1.y, C_1.z);
    bool tmp8 = (int(row.z + 0.5f) & 16) != 0;
    bool tmp9;
    if (tmp8)
    {
        tmp9 = (waterWavesRed & 1) == 0;
    }
    else
    {
        tmp9 = tmp8;
    }
    float tmp10;
    if (tmp9)
    {
        tmp10 = 0.0f;
    }
    else
    {
        tmp10 = smoothstep(0.0f, 1.0f, clamp((area - waterWavesShore.z) / (waterWavesShore.w - waterWavesShore.z), 0.0f, 1.0f));
    }
    float wt = tmp10;
    if ((waterWavesRed & 2) == 0)
    {
        wt *= smoothstep(0.0f, 1.0f, clamp((dist - waterWavesShore.x) / (waterWavesShore.y * lam), 0.0f, 1.0f));
    }
    if (wt <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, lam);
    }
    float S = C_1.w * waterWavesK.w;
    float2 grad = 0.0f.xx;
    float tmp11;
    for (int k_1 = 0; k_1 < 4; k_1++)
    {
        float li = lam * float4(1.0f, 0.709999978542327880859375f, 0.5299999713897705078125f, 0.37000000476837158203125f)[k_1];
        float kk = 6.283185482025146484375f / li;
        float om = sqrt(686.5999755859375f * kk);
        float ca = cos(float4(0.0f, 0.4014259874820709228515625f, -0.54105198383331298828125f, 0.82030498981475830078125f)[k_1]);
        float sa = sin(float4(0.0f, 0.4014259874820709228515625f, -0.54105198383331298828125f, 0.82030498981475830078125f)[k_1]);
        float2 d = float2((ca * waterWavesK.x) - (sa * waterWavesK.y), (sa * waterWavesK.x) + (ca * waterWavesK.y));
        if ((waterWavesRed & 4) != 0)
        {
            tmp11 = 1.0f;
        }
        else
        {
            tmp11 = clamp(((li / max(footprint, 0.001000000047497451305389404296875f)) - 2.0f) * 0.5f, 0.0f, 1.0f);
        }
        float aa = tmp11;
        float th = ((kk * dot(d, P)) - (om * t)) + float4(0.0f, 1.7000000476837158203125f, 4.099999904632568359375f, 2.900000095367431640625f)[k_1];
        grad += (d * (((S * float4(0.4000000059604644775390625f, 0.2800000011920928955078125f, 0.20000000298023223876953125f, 0.119999997317790985107421875f)[k_1]) * aa) * cos(th)));
    }
    return float4((-grad) * wt, wt, lam);
}
#endif
