// Shared HLSL for res/shaders/features/far-ground/cell_farground.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_fgpage_ps.hlsl, cell_fgstamp_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl

#if defined(WW_FN_fgQrot) || defined(WW_FN_ALL)
float3 fgQrot(float4 q, float3 v)
{
    return v + (cross(q.xyz, cross(q.xyz, v) + (v * q.w)) * 2.0f);
}
#endif

#if defined(WW_FN_fgSplat) || defined(WW_FN_ALL)
float3 fgSplat(float2 w, float2 wdx, float2 wdy)
{
    float2 g = (w - fgGridMap.xy) * fgGridMap.z;
    float2 g0 = floor(g);
    float2 f = g - g0;
    float2 uv = w / fgGridMap.w.xx;
    float2 dx = wdx / fgGridMap.w.xx;
    float2 dy = wdy / fgGridMap.w.xx;
    int2 n = int2(fgGridN);
    float3 acc = 0.0f.xxx;
    float3 vc = 0.0f.xxx;
    float tmp1;
    float tmp2;
    float3 tmp3;
    for (int c = 0; c < 4; c++)
    {
        int2 o = int2(c & 1, c >> 1);
        int2 p = clamp(int2(g0) + o, int2(0, 0), n - int2(1, 1));
        if (o.x == 1)
        {
            tmp1 = f.x;
        }
        else
        {
            tmp1 = 1.0f - f.x;
        }
        if (o.y == 1)
        {
            tmp2 = f.y;
        }
        else
        {
            tmp2 = 1.0f - f.y;
        }
        float bw = tmp1 * tmp2;
        uint4 s = fgGrid.Load(int3(p, 0));
        vc += (spvUnpackUnorm4x8(s.w).xyz * bw);
        if (bw <= 0.0f)
        {
            continue;
        }
        float4 wt = spvUnpackUnorm4x8(s.z);
        uint tmp4[4] = { s.x & 65535u, s.x >> uint(16), s.y & 65535u, s.y >> uint(16) };
        uint id[4] = tmp4;
        for (int k = 0; k < 4; k++)
        {
            if (wt[k] <= 0.0f)
            {
                continue;
            }
            if (id[k] == 65535u)
            {
                tmp3 = 0.5f.xxx;
            }
            else
            {
                tmp3 = fgLtex.SampleGrad(_fgLtex_sampler, float3(uv, float(id[k])), dx, dy).xyz;
            }
            float3 t = tmp3;
            acc += (t * (bw * wt[k]));
        }
    }
    return acc * vc;
}
#endif

#if defined(WW_FN_fgDecals) || defined(WW_FN_ALL)
float4 fgDecals(float3 w, float pix)
{
    int2 b = int2(floor((w.xy - fgBinMap.xy) * fgBinMap.z));
    int2 bn = int2(fgBinN.xy);
    bool tmp1 = b.x < 0;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = b.y < 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = b.x >= bn.x;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = b.y >= bn.y;
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return 0.0f.xxxx;
    }
    float4 bin = fgDecal.Load((int(fgBinN.z) + (b.y * bn.x)) + b.x);
    int start = int(bin.x);
    int cnt = int(bin.y);
    float4 acc = 0.0f.xxxx;
    for (int i = 0; i < cnt; i++)
    {
        int k = start + i;
        int r = int(fgDecal.Load(int(fgBinN.w) + (k >> 2))[k & 3]) * 5;
        float4 ps = fgDecal.Load(r);
        float4 q = fgDecal.Load(r + 1);
        float4 b0 = fgDecal.Load(r + 2);
        float4 b1 = fgDecal.Load(r + 3);
        float4 tmp5 = float4(-q.xyz, q.w);
        float3 tmp6 = w - ps.xyz;
        float3 l = fgQrot(tmp5, tmp6) / ps.w.xxx;
        bool tmp7 = l.x < b0.x;
        bool tmp8;
        if (!tmp7)
        {
            tmp8 = l.x > b0.z;
        }
        else
        {
            tmp8 = tmp7;
        }
        bool tmp9;
        if (!tmp8)
        {
            tmp9 = l.y < b0.y;
        }
        else
        {
            tmp9 = tmp8;
        }
        bool tmp10;
        if (!tmp9)
        {
            tmp10 = l.y > b0.w;
        }
        else
        {
            tmp10 = tmp9;
        }
        bool tmp11;
        if (!tmp10)
        {
            tmp11 = l.z < b1.x;
        }
        else
        {
            tmp11 = tmp10;
        }
        bool tmp12;
        if (!tmp11)
        {
            tmp12 = l.z > b1.y;
        }
        else
        {
            tmp12 = tmp11;
        }
        if (tmp12)
        {
            continue;
        }
        float4 at = fgDecal.Load(r + 4);
        float2 uv = float2((l.x - b0.x) / (b0.z - b0.x), (b0.w - l.y) / (b0.w - b0.y));
        float lod = clamp(log2(max(1.0f, pix / (b1.z * ps.w))), 0.0f, fgBinMap.w);
        float4 c = fgAtlas.SampleLevel(_fgAtlas_sampler, lerp(at.xy, at.zw, uv), lod);
        acc = float4(c.xyz * c.w, c.w) + (acc * (1.0f - c.w));
    }
    return acc;
}
#endif

#if defined(WW_FN_fgGround) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 wdx = ddx(w.xy);
    float2 wdy = ddy(w.xy);
    float2 tmp1 = w.xy;
    float2 tmp2 = wdx;
    float2 tmp3 = wdy;
    float3 col = fgSplat(tmp1, tmp2, tmp3);
    if (fgDecalsOn == 0)
    {
        return col;
    }
    float2 rel = (w.xy - fgGridMap.xy) / 4096.0f.xx;
    float2 cell = floor(rel);
    int2 n = int2(fgGridN);
    uint pg = fgGrid.Load(int3(clamp(int2(floor((w.xy - fgGridMap.xy) * fgGridMap.z)), int2(0, 0), n - int2(1, 1)), 0)).w >> uint(24);
    float4 d = 0.0f.xxxx;
    if (pg != 255u)
    {
        d = fgPages.SampleGrad(_fgPages_sampler, float3(rel - cell, float(pg)), wdx / 4096.0f.xx, wdy / 4096.0f.xx);
    }
    return (col * (1.0f - d.w)) + d.xyz;
}
#endif

#if defined(WW_FN_fgGround_V2) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 wdx = ddx(w.xy);
    float2 wdy = ddy(w.xy);
    float2 tmp1 = w.xy;
    float2 tmp2 = wdx;
    float2 tmp3 = wdy;
    float3 col = fgSplat(tmp1, tmp2, tmp3);
    if (fgDecalsOn == 0)
    {
        return col;
    }
    float pix = max(length(ddx(w)), length(ddy(w)));
    float3 tmp4 = w;
    float tmp5 = pix;
    float4 d = fgDecals(tmp4, tmp5);
    return (col * (1.0f - d.w)) + d.xyz;
}
#endif

#if defined(WW_FN_fgGround_V3) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 rel = (w.xy - fgGridMap.xy) / 4096.0f.xx;
    float2 rdx = ddx(rel);
    float2 rdy = ddy(rel);
    int2 cn = int2(spvTextureSize(fgCells, uint(0), dummy_parameter));
    int2 ci = int2(floor(rel));
    bool tmp1 = ci.x < 0;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = ci.y < 0;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = ci.x >= cn.x;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp4;
    if (!tmp3)
    {
        tmp4 = ci.y >= cn.y;
    }
    else
    {
        tmp4 = tmp3;
    }
    if (tmp4)
    {
        return sheet;
    }
    uint4 c = fgCells.Load(int3(ci, 0));
    bool tmp5 = c.x == 0u;
    bool tmp6;
    if (!tmp5)
    {
        tmp6 = c.z == 0u;
    }
    else
    {
        tmp6 = tmp5;
    }
    if (tmp6)
    {
        return sheet;
    }
    float3 uvl = float3(rel - float2(ci), float(c.y));
    float3 tmp7;
    if (c.x == 1u)
    {
        tmp7 = fgPagesN.SampleGrad(_fgPagesN_sampler, uvl, rdx, rdy).xyz;
    }
    else
    {
        tmp7 = fgPagesF.SampleGrad(_fgPagesF_sampler, uvl, rdx, rdy).xyz;
    }
    float3 page = tmp7;
    return lerp(sheet, page, (float(c.z) / 255.0f).xxx);
}
#endif
