// Shared HLSL for res/shaders/features/post/cell_bloom.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_bloom_comb_ps.hlsl, cell_bloom_down_ps.hlsl, cell_bloom_up_ps.hlsl, cell_hdr_fd_ps.hlsl, cell_hdr_ps.hlsl
// Not here (declarations, or no converted entry carried them): bloomLuma
#ifdef WW_FN_ALL
static const float kBloomMaxFinite = 65504.0;
#endif

#if defined(WW_FN_bloomSanitize) || defined(WW_FN_ALL)
float3 bloomSanitize(float3 c)
{
    uint3 bits = asuint(c);
    uint3 e = bits & uint3(2139095040u, 2139095040u, 2139095040u);
    uint3 m = bits & uint3(8388607u, 8388607u, 8388607u);
    bool cond6 = e.x == 2139095040u;
    bool cond5;
    if (cond6)
    {
        cond5 = m.x != 0u;
    }
    else
    {
        cond5 = cond6;
    }
    bool cond4;
    if (!cond5)
    {
        bool cond3 = e.y == 2139095040u;
        bool condPick2;
        if (cond3)
        {
            condPick2 = m.y != 0u;
        }
        else
        {
            condPick2 = cond3;
        }
        cond4 = condPick2;
    }
    else
    {
        cond4 = cond5;
    }
    bool cond2;
    if (!cond4)
    {
        bool cond = e.z == 2139095040u;
        bool condPick;
        if (cond)
        {
            condPick = m.z != 0u;
        }
        else
        {
            condPick = cond;
        }
        cond2 = condPick;
    }
    else
    {
        cond2 = cond4;
    }
    if (cond2)
    {
        return 0.0f.xxx;
    }
    float3 bounded = sign(c) * min(abs(c), 65504.0f.xxx);
    return clamp(bounded, 0.0f.xxx, 65504.0f.xxx);
}
#endif

#if defined(WW_FN_bloomTent) || defined(WW_FN_ALL)
float3 bloomTent(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 texel, float radiusPixels)
{
    float2 o = texel * max(radiusPixels, 0.0f);
    float3 cond6 = m.SampleLevel(_m_sampler, uv, 0.0f).xyz;
    float3 c = bloomSanitize(cond6) * 4.0f;
    float3 cond5 = m.SampleLevel(_m_sampler, uv + float2(-o.x, 0.0f), 0.0f).xyz;
    c += (bloomSanitize(cond5) * 2.0f);
    float3 cond4 = m.SampleLevel(_m_sampler, uv + float2(o.x, 0.0f), 0.0f).xyz;
    c += (bloomSanitize(cond4) * 2.0f);
    float3 cond3 = m.SampleLevel(_m_sampler, uv + float2(0.0f, -o.y), 0.0f).xyz;
    c += (bloomSanitize(cond3) * 2.0f);
    float3 condPick2 = m.SampleLevel(_m_sampler, uv + float2(0.0f, o.y), 0.0f).xyz;
    c += (bloomSanitize(condPick2) * 2.0f);
    float3 cond2 = m.SampleLevel(_m_sampler, uv + float2(-o.x, -o.y), 0.0f).xyz;
    c += bloomSanitize(cond2);
    float3 cond = m.SampleLevel(_m_sampler, uv + float2(o.x, -o.y), 0.0f).xyz;
    c += bloomSanitize(cond);
    float3 condPick = m.SampleLevel(_m_sampler, uv + float2(-o.x, o.y), 0.0f).xyz;
    c += bloomSanitize(condPick);
    float3 mTexel = m.SampleLevel(_m_sampler, uv + float2(o.x, o.y), 0.0f).xyz;
    c += bloomSanitize(mTexel);
    return c * 0.0625f;
}
#endif

#if defined(WW_FN_bloomTentLod) || defined(WW_FN_ALL)
float3 bloomTentLod(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 texel, float radiusPixels, float lod)
{
    float2 o = (texel * exp2(lod)) * max(radiusPixels, 0.0f);
    float3 cond6 = m.SampleLevel(_m_sampler, uv, lod).xyz;
    float3 c = bloomSanitize(cond6) * 4.0f;
    float3 cond5 = m.SampleLevel(_m_sampler, uv + float2(-o.x, 0.0f), lod).xyz;
    c += (bloomSanitize(cond5) * 2.0f);
    float3 cond4 = m.SampleLevel(_m_sampler, uv + float2(o.x, 0.0f), lod).xyz;
    c += (bloomSanitize(cond4) * 2.0f);
    float3 cond3 = m.SampleLevel(_m_sampler, uv + float2(0.0f, -o.y), lod).xyz;
    c += (bloomSanitize(cond3) * 2.0f);
    float3 condPick2 = m.SampleLevel(_m_sampler, uv + float2(0.0f, o.y), lod).xyz;
    c += (bloomSanitize(condPick2) * 2.0f);
    float3 cond2 = m.SampleLevel(_m_sampler, uv + float2(-o.x, -o.y), lod).xyz;
    c += bloomSanitize(cond2);
    float3 cond = m.SampleLevel(_m_sampler, uv + float2(o.x, -o.y), lod).xyz;
    c += bloomSanitize(cond);
    float3 condPick = m.SampleLevel(_m_sampler, uv + float2(-o.x, o.y), lod).xyz;
    c += bloomSanitize(condPick);
    float3 mTexel = m.SampleLevel(_m_sampler, uv + float2(o.x, o.y), lod).xyz;
    c += bloomSanitize(mTexel);
    return c * 0.0625f;
}
#endif

#if defined(WW_FN_bloomTentDispersedLod) || defined(WW_FN_ALL)
float3 bloomTentDispersedLod(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float radiusPixels, float dispersion, float lod)
{
    float2 texel = 1.0f.xx / float2(max(int2(spvTextureSize(m, uint(0), dummy_parameter)), int2(1, 1)));
    float2 radial = uv - 0.5f.xx;
    float len = length(radial);
    float2 cond6;
    if (len > 9.9999997473787516355514526367188e-06f)
    {
        cond6 = ((radial / len.xx) * texel) * max(dispersion, 0.0f);
    }
    else
    {
        cond6 = 0.0f.xx;
    }
    float2 d = cond6;
    float2 cond5 = uv;
    float2 cond4 = texel;
    float cond3 = radiusPixels;
    float condPick2 = lod;
    float3 g = bloomTentLod(m, _m_sampler, cond5, cond4, cond3, condPick2);
    if (all(bool2(d.x == 0.0f.xx.x, d.y == 0.0f.xx.y)))
    {
        return g;
    }
    float2 cond2 = uv + d;
    float2 cond = texel;
    float condPick = radiusPixels;
    float mTexel = lod;
    float2 bloomTentDispersedLodRet4 = uv - d;
    float2 bloomTentDispersedLodRet3 = texel;
    float bloomTentDispersedLodRet2 = radiusPixels;
    float bloomTentDispersedLodRet = lod;
    return float3(bloomTentLod(m, _m_sampler, cond2, cond, condPick, mTexel).x, g.y, bloomTentLod(m, _m_sampler, bloomTentDispersedLodRet4, bloomTentDispersedLodRet3, bloomTentDispersedLodRet2, bloomTentDispersedLodRet).z);
}
#endif

#if defined(WW_FN_bloomTentDispersed) || defined(WW_FN_ALL)
float3 bloomTentDispersed(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float radiusPixels, float dispersion)
{
    float2 texel = 1.0f.xx / float2(max(int2(spvTextureSize(m, uint(0), dummy_parameter)), int2(1, 1)));
    float2 radial = uv - 0.5f.xx;
    float len = length(radial);
    float2 cond6;
    if (len > 9.9999997473787516355514526367188e-06f)
    {
        cond6 = ((radial / len.xx) * texel) * max(dispersion, 0.0f);
    }
    else
    {
        cond6 = 0.0f.xx;
    }
    float2 d = cond6;
    float2 cond5 = uv;
    float2 cond4 = texel;
    float cond3 = radiusPixels;
    float3 g = bloomTent(m, _m_sampler, cond5, cond4, cond3);
    if (all(bool2(d.x == 0.0f.xx.x, d.y == 0.0f.xx.y)))
    {
        return g;
    }
    float2 condPick2 = uv + d;
    float2 cond2 = texel;
    float cond = radiusPixels;
    float3 r = bloomTent(m, _m_sampler, condPick2, cond2, cond);
    float2 condPick = uv - d;
    float2 mTexel = texel;
    float bloomTentDispersedLodRet4 = radiusPixels;
    float3 b = bloomTent(m, _m_sampler, condPick, mTexel, bloomTentDispersedLodRet4);
    return float3(r.x, g.y, b.z);
}
#endif
