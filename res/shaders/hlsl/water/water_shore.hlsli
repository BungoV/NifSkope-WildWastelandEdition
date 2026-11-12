// Shared HLSL for res/shaders/features/water/water_shore.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwFoamDetail
#ifdef WW_FN_ALL
static const float kShoreM = 0.0142875;
#endif

#if defined(WW_FN_wwShoreTexelValue) || defined(WW_FN_ALL)
float2 wwShoreTexelValue(uint w, bool inMap)
{
    if (!inMap)
    {
        return float2(255.0f * waterInfoGrid.w, 0.0f);
    }
    if ((w & 65535u) == 0u)
    {
        return 0.0f.xx;
    }
    return float2(float((w >> uint(16)) & 255u) * waterInfoGrid.w, 1.0f);
}
#endif

#if defined(WW_FN_wwShoreField) || defined(WW_FN_ALL)
float2 wwShoreField(float2 worldXY)
{
    if (!waterInfoOn)
    {
        return 0.0f.xx;
    }
    float2 g = ((worldXY - waterInfoGrid.xy) / waterInfoGrid.z.xx) - 0.5f.xx;
    int2 t0 = int2(floor(g));
    float2 f = g - float2(t0);
    if ((waterShoreRed & 1) != 0)
    {
        f = step(0.5f.xx, f);
    }
    int E = int(waterInfoLayout.z);
    int2 n = int2(waterInfoLayout.xy) * E.xx;
    bool i00 = true;
    bool i10 = true;
    bool i01 = true;
    bool i11 = true;
    int2 tile = t0 / E.xx;
    int2 in0 = t0 - (tile * E.xx);
    bool tmp1 = t0.x >= 0;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = t0.y >= 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (tmp2)
    {
        tmp3 = (t0.x + 1) < n.x;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (tmp3)
    {
        tmp4 = (t0.y + 1) < n.y;
    }
    else
    {
        tmp4 = tmp3;
    }
    bool tmp5;
    if (tmp4)
    {
        tmp5 = in0.x < (E - 1);
    }
    else
    {
        tmp5 = tmp4;
    }
    bool tmp6;
    if (tmp5)
    {
        tmp6 = in0.y < (E - 1);
    }
    else
    {
        tmp6 = tmp5;
    }
    uint w00;
    uint w10;
    uint w01;
    uint w11;
    if (tmp6)
    {
        uint2 ix = waterInfoIndex.Load(int3(tile, 0)).xy;
        if (ix.x == 0u)
        {
            w11 = ix.y;
            w01 = ix.y;
            w10 = ix.y;
            w00 = ix.y;
        }
        else
        {
            int slot = int(ix.x) - 1;
            int perRow = int(waterInfoLayout.w);
            int2 a = (int2(slot % perRow, slot / perRow) * E.xx) + in0;
            w00 = waterInfoAtlas.Load(int3(a, 0)).x;
            w10 = waterInfoAtlas.Load(int3(a + int2(1, 0), 0)).x;
            w01 = waterInfoAtlas.Load(int3(a + int2(0, 1), 0)).x;
            w11 = waterInfoAtlas.Load(int3(a + int2(1, 1), 0)).x;
        }
    }
    else
    {
        float2 c = waterInfoGrid.xy + ((float2(t0) + 0.5f.xx) * waterInfoGrid.z);
        float z = waterInfoGrid.z;
        float2 tmp7 = c;
        w00 = wwWaterInfoWord(tmp7);
        float2 tmp8 = c + float2(z, 0.0f);
        w10 = wwWaterInfoWord(tmp8);
        float2 tmp9 = c + float2(0.0f, z);
        w01 = wwWaterInfoWord(tmp9);
        float2 tmp10 = c + z.xx;
        w11 = wwWaterInfoWord(tmp10);
        bool tmp11 = all(bool2(t0.x >= int2(0, 0).x, t0.y >= int2(0, 0).y));
        bool tmp12;
        if (tmp11)
        {
            tmp12 = all(bool2(t0.x < n.x, t0.y < n.y));
        }
        else
        {
            tmp12 = tmp11;
        }
        i00 = tmp12;
        int2 tmp13 = t0 + int2(1, 0);
        bool tmp14 = all(bool2(tmp13.x >= int2(0, 0).x, tmp13.y >= int2(0, 0).y));
        bool tmp15;
        if (tmp14)
        {
            int2 tmp16 = t0 + int2(1, 0);
            tmp15 = all(bool2(tmp16.x < n.x, tmp16.y < n.y));
        }
        else
        {
            tmp15 = tmp14;
        }
        i10 = tmp15;
        int2 tmp17 = t0 + int2(0, 1);
        bool tmp18 = all(bool2(tmp17.x >= int2(0, 0).x, tmp17.y >= int2(0, 0).y));
        bool tmp19;
        if (tmp18)
        {
            int2 tmp20 = t0 + int2(0, 1);
            tmp19 = all(bool2(tmp20.x < n.x, tmp20.y < n.y));
        }
        else
        {
            tmp19 = tmp18;
        }
        i01 = tmp19;
        int2 tmp21 = t0 + int2(1, 1);
        bool tmp22 = all(bool2(tmp21.x >= int2(0, 0).x, tmp21.y >= int2(0, 0).y));
        bool tmp23;
        if (tmp22)
        {
            int2 tmp24 = t0 + int2(1, 1);
            tmp23 = all(bool2(tmp24.x < n.x, tmp24.y < n.y));
        }
        else
        {
            tmp23 = tmp22;
        }
        i11 = tmp23;
    }
    uint tmp25 = w00;
    bool tmp26 = i00;
    float2 v00 = wwShoreTexelValue(tmp25, tmp26);
    uint tmp27 = w10;
    bool tmp28 = i10;
    float2 v10 = wwShoreTexelValue(tmp27, tmp28);
    uint tmp29 = w01;
    bool tmp30 = i01;
    float2 v01 = wwShoreTexelValue(tmp29, tmp30);
    uint tmp31 = w11;
    bool tmp32 = i11;
    float2 v11 = wwShoreTexelValue(tmp31, tmp32);
    float d = lerp(lerp(v00.x, v10.x, f.x), lerp(v01.x, v11.x, f.x), f.y);
    return float2(d, ((v00.y + v10.y) + v01.y) + v11.y);
}
#endif

#if defined(WW_FN_wwShoreSmooth) || defined(WW_FN_ALL)
float wwShoreSmooth(float x)
{
    return (x * x) * (3.0f - (2.0f * x));
}
#endif

#if defined(WW_FN_wwShoreEdge) || defined(WW_FN_ALL)
float wwShoreEdge(float2 field)
{
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float tmp1 = clamp((field.x - waterShoreK0.x) / waterShoreK0.y, 0.0f, 1.0f);
    return 1.0f - wwShoreSmooth(tmp1);
}
#endif

#if defined(WW_FN_wwShoreEdgeSoft) || defined(WW_FN_ALL)
float wwShoreEdgeSoft(float2 field, float along, float viewZ)
{
    float d = along;
    if (field.y > 0.5f)
    {
        d = min(d, max(field.x - waterShoreK0.x, 0.0f) + waterShoreK4.z);
    }
    if (d >= 100000000.0f)
    {
        return 0.0f;
    }
    float w = lerp(waterShoreK4.y, waterShoreK4.x, clamp(abs(viewZ), 0.0f, 1.0f));
    float tmp1 = clamp(d / w, 0.0f, 1.0f);
    return 1.0f - wwShoreSmooth(tmp1);
}
#endif

#if defined(WW_FN_wwShoreHash) || defined(WW_FN_ALL)
uint wwShoreHash(int2 c)
{
    uint h = (uint(c.x) * 2376512323u) ^ (uint(c.y) * 3625334849u);
    h ^= (h >> uint(16));
    h *= 2146121005u;
    h ^= (h >> uint(15));
    h *= 2221713035u;
    h ^= (h >> uint(16));
    return h;
}
#endif

#if defined(WW_FN_wwShoreNoise) || defined(WW_FN_ALL)
float wwShoreNoise(float2 p)
{
    int2 c = int2(floor(p));
    float2 f = p - float2(c);
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 tmp1 = c;
    float a = float(wwShoreHash(tmp1) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 tmp2 = c + int2(1, 0);
    float b = float(wwShoreHash(tmp2) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 tmp3 = c + int2(0, 1);
    float e = float(wwShoreHash(tmp3) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 tmp4 = c + int2(1, 1);
    float g = float(wwShoreHash(tmp4) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    return lerp(lerp(a, b, f.x), lerp(e, g, f.x), f.y);
}
#endif

#if defined(WW_FN_wwShoreFoam) || defined(WW_FN_ALL)
float wwShoreFoam(float2 field, float2 worldXY, float t)
{
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float x = max(field.x - waterShoreK0.x, 0.0f);
    float tmp1 = clamp(x / waterShoreK0.z, 0.0f, 1.0f);
    float env = 1.0f - wwShoreSmooth(tmp1);
    float wave = 0.5f + (0.5f * cos(6.283185482025146484375f * ((x / waterShoreK1.x) + (t / waterShoreK1.y))));
    float cover = env * lerp(waterShoreK1.z, 1.0f, wave);
    float2 tmp2 = worldXY * 0.02083333395421504974365234375f;
    float2 tmp3 = (worldXY * 0.0588235296308994293212890625f) + 0.37000000476837158203125f.xx;
    float n = (0.64999997615814208984375f * wwShoreNoise(tmp2)) + (0.3499999940395355224609375f * wwShoreNoise(tmp3));
    return clamp((n - (1.0f - cover)) / waterShoreK0.w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwShoreWave) || defined(WW_FN_ALL)
float4 wwShoreWave(float2 P)
{
    if (!waterInfoOn)
    {
        return 0.0f.xxxx;
    }
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 tmp1 = c0;
    float2 tmp2 = c0 + float2(tex, 0.0f);
    float2 tmp3 = c0 + float2(0.0f, tex);
    float2 tmp4 = c0 + tex.xx;
    uint4 w = uint4(wwWaterInfoWord(tmp1), wwWaterInfoWord(tmp2), wwWaterInfoWord(tmp3), wwWaterInfoWord(tmp4));
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = w & uint4(65535u, 65535u, 65535u, 65535u);
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
    float4 code = float4(w >> uint4(int4(24, 24, 24, 24))) / 255.0f.xxxx;
    uint4 tmp7 = body.xxxx;
    float depth = dot(bw, ((float4(bool4(id.x == tmp7.x, id.y == tmp7.y, id.z == tmp7.z, id.w == tmp7.w)) * code) * code) * 8192.0f);
    float4 row = waterInfoBodies.Load(int3(int2(int(body % 256u), int(body / 256u)), 0));
    int cls = clamp(int(row.y + 0.5f), 0, 2);
    float len = (sqrt(max(row.w, 1.0f)) * tex) * 0.01428749971091747283935546875f;
    float tmp8;
    if (cls == 1)
    {
        tmp8 = min(len, waterShoreK2.z);
    }
    else
    {
        tmp8 = len;
    }
    float fetch = tmp8;
    float U = waterShoreK2.x;
    float hw = min((0.001599999959580600261688232421875f * U) * sqrt(fetch / 9.81000041961669921875f), ((0.20999999344348907470703125f * U) * U) / 9.81000041961669921875f);
    float tmp9;
    if (cls == 0)
    {
        tmp9 = sqrt((hw * hw) + (waterShoreK2.y * waterShoreK2.y));
    }
    else
    {
        tmp9 = hw;
    }
    float hs = tmp9;
    float tmp10;
    if (cls == 0)
    {
        tmp10 = 1.0f;
    }
    else
    {
        tmp10 = waterShoreK3.x;
    }
    return float4(hs / 0.01428749971091747283935546875f, tmp10, depth, 1.0f);
}
#endif

#if defined(WW_FN_wwFoamHash01) || defined(WW_FN_ALL)
float wwFoamHash01(int2 c)
{
    int2 tmp1 = c;
    return float(wwShoreHash(tmp1) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
}
#endif

#if defined(WW_FN_wwFoamNoise) || defined(WW_FN_ALL)
float wwFoamNoise(float2 p, int k, int2 seed)
{
    float2 fl = floor(p);
    float2 f = p - fl;
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 c = int2(fl) % k.xx;
    int2 c1 = (c + int2(1, 1)) % k.xx;
    int2 tmp1 = c + seed;
    float a = wwFoamHash01(tmp1);
    int2 tmp2 = int2(c1.x, c.y) + seed;
    float b = wwFoamHash01(tmp2);
    int2 tmp3 = int2(c.x, c1.y) + seed;
    float e = wwFoamHash01(tmp3);
    int2 tmp4 = c1 + seed;
    float h = wwFoamHash01(tmp4);
    return lerp(lerp(a, b, f.x), lerp(e, h, f.x), f.y);
}
#endif

#if defined(WW_FN_wwFoamLace) || defined(WW_FN_ALL)
float wwFoamLace(float2 p, int k)
{
    float2 c = floor(p);
    float f1 = 8.0f;
    float f2 = 8.0f;
    float tmp1;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int2 cc = int2(c) + int2(x, y);
            int2 tmp2 = ((cc + k.xx) % k.xx) + int2(331, 977);
            uint h = wwShoreHash(tmp2);
            float2 q = float2(cc) + (float2(float(h & 65535u), float(h >> uint(16))) * 1.525902189314365386962890625e-05f);
            float d = length(p - q);
            if (d < f1)
            {
                tmp1 = f1;
            }
            else
            {
                tmp1 = min(f2, d);
            }
            f2 = tmp1;
            f1 = min(f1, d);
        }
    }
    return f2 - f1;
}
#endif

#if defined(WW_FN_wwShoreFoamWave) || defined(WW_FN_ALL)
float wwShoreFoamWave(float2 field, float2 P, float t, float column, float colFoot, float foot, out float3 dbg)
{
    dbg = 0.0f.xxx;
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float2 tmp1 = P;
    float4 wv = wwShoreWave(tmp1);
    if (wv.w < 0.5f)
    {
        return 0.0f;
    }
    float hs = max(wv.x, 0.001000000047497451305389404296875f);
    bool scene = column < 100000000.0f;
    float tmp2;
    if (scene)
    {
        tmp2 = column;
    }
    else
    {
        tmp2 = wv.z;
    }
    float dep = tmp2;
    float dF = scene ? colFoot : 0.0f;
    float amount = clamp(pow((hs * 0.01428749971091747283935546875f) / waterShoreK2.w, 2.5f) * wv.y, 0.0f, 1.0f);
    dbg = float3(hs / 64.0f, amount, dep / 64.0f);
    float db = hs / waterShoreK3.y;
    float surf = 1.0f - smoothstep(0.60000002384185791015625f * db, (1.2000000476837158203125f * db) + dF, dep);
    float x = max(field.x - waterShoreK0.x, 0.0f);
    float wave = 0.5f + (0.5f * cos(6.283185482025146484375f * ((x / waterShoreK1.x) + (t / waterShoreK1.y))));
    surf *= lerp(waterShoreK1.z, 1.0f, wave);
    float lap = ((1.0f - smoothstep(0.0f, hs + dF, dep)) * (hs / ((hs + dF) + 9.9999997473787516355514526367188e-05f))) * smoothstep(0.0f, waterShoreK3.w, hs * 0.01428749971091747283935546875f);
    if (waterShoreEdgeLaw == 1)
    {
        lap *= smoothstep(0.0f, 0.300000011920928955078125f * (hs + dF), dep);
    }
    float D_1 = max(surf * amount, lap * waterShoreK3.z);
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return 0.0f;
    }
    float2 tmp3 = P;
    float tmp4 = foot;
    float tmp5 = t;
    float3 det = wwFoamDetail(tmp3, tmp4, tmp5);
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return (a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1);
}
#endif
