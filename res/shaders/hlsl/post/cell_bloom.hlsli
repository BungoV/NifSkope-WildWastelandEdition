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
    bool tmp1 = e.x == 2139095040u;
    bool tmp2;
    if (tmp1)
    {
        tmp2 = m.x != 0u;
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        bool tmp4 = e.y == 2139095040u;
        bool tmp5;
        if (tmp4)
        {
            tmp5 = m.y != 0u;
        }
        else
        {
            tmp5 = tmp4;
        }
        tmp3 = tmp5;
    }
    else
    {
        tmp3 = tmp2;
    }
    bool tmp6;
    if (!tmp3)
    {
        bool tmp7 = e.z == 2139095040u;
        bool tmp8;
        if (tmp7)
        {
            tmp8 = m.z != 0u;
        }
        else
        {
            tmp8 = tmp7;
        }
        tmp6 = tmp8;
    }
    else
    {
        tmp6 = tmp3;
    }
    if (tmp6)
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
    float3 tmp1 = m.SampleLevel(_m_sampler, uv, 0.0f).xyz;
    float3 c = bloomSanitize(tmp1) * 4.0f;
    float3 tmp2 = m.SampleLevel(_m_sampler, uv + float2(-o.x, 0.0f), 0.0f).xyz;
    c += (bloomSanitize(tmp2) * 2.0f);
    float3 tmp3 = m.SampleLevel(_m_sampler, uv + float2(o.x, 0.0f), 0.0f).xyz;
    c += (bloomSanitize(tmp3) * 2.0f);
    float3 tmp4 = m.SampleLevel(_m_sampler, uv + float2(0.0f, -o.y), 0.0f).xyz;
    c += (bloomSanitize(tmp4) * 2.0f);
    float3 tmp5 = m.SampleLevel(_m_sampler, uv + float2(0.0f, o.y), 0.0f).xyz;
    c += (bloomSanitize(tmp5) * 2.0f);
    float3 tmp6 = m.SampleLevel(_m_sampler, uv + float2(-o.x, -o.y), 0.0f).xyz;
    c += bloomSanitize(tmp6);
    float3 tmp7 = m.SampleLevel(_m_sampler, uv + float2(o.x, -o.y), 0.0f).xyz;
    c += bloomSanitize(tmp7);
    float3 tmp8 = m.SampleLevel(_m_sampler, uv + float2(-o.x, o.y), 0.0f).xyz;
    c += bloomSanitize(tmp8);
    float3 tmp9 = m.SampleLevel(_m_sampler, uv + float2(o.x, o.y), 0.0f).xyz;
    c += bloomSanitize(tmp9);
    return c * 0.0625f;
}
#endif

#if defined(WW_FN_bloomTentLod) || defined(WW_FN_ALL)
float3 bloomTentLod(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 texel, float radiusPixels, float lod)
{
    float2 o = (texel * exp2(lod)) * max(radiusPixels, 0.0f);
    float3 tmp1 = m.SampleLevel(_m_sampler, uv, lod).xyz;
    float3 c = bloomSanitize(tmp1) * 4.0f;
    float3 tmp2 = m.SampleLevel(_m_sampler, uv + float2(-o.x, 0.0f), lod).xyz;
    c += (bloomSanitize(tmp2) * 2.0f);
    float3 tmp3 = m.SampleLevel(_m_sampler, uv + float2(o.x, 0.0f), lod).xyz;
    c += (bloomSanitize(tmp3) * 2.0f);
    float3 tmp4 = m.SampleLevel(_m_sampler, uv + float2(0.0f, -o.y), lod).xyz;
    c += (bloomSanitize(tmp4) * 2.0f);
    float3 tmp5 = m.SampleLevel(_m_sampler, uv + float2(0.0f, o.y), lod).xyz;
    c += (bloomSanitize(tmp5) * 2.0f);
    float3 tmp6 = m.SampleLevel(_m_sampler, uv + float2(-o.x, -o.y), lod).xyz;
    c += bloomSanitize(tmp6);
    float3 tmp7 = m.SampleLevel(_m_sampler, uv + float2(o.x, -o.y), lod).xyz;
    c += bloomSanitize(tmp7);
    float3 tmp8 = m.SampleLevel(_m_sampler, uv + float2(-o.x, o.y), lod).xyz;
    c += bloomSanitize(tmp8);
    float3 tmp9 = m.SampleLevel(_m_sampler, uv + float2(o.x, o.y), lod).xyz;
    c += bloomSanitize(tmp9);
    return c * 0.0625f;
}
#endif

#if defined(WW_FN_bloomTentDispersedLod) || defined(WW_FN_ALL)
float3 bloomTentDispersedLod(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float radiusPixels, float dispersion, float lod)
{
    float2 texel = 1.0f.xx / float2(max(int2(spvTextureSize(m, uint(0), dummy_parameter)), int2(1, 1)));
    float2 radial = uv - 0.5f.xx;
    float len = length(radial);
    float2 tmp1;
    if (len > 9.9999997473787516355514526367188e-06f)
    {
        tmp1 = ((radial / len.xx) * texel) * max(dispersion, 0.0f);
    }
    else
    {
        tmp1 = 0.0f.xx;
    }
    float2 d = tmp1;
    float2 tmp2 = uv;
    float2 tmp3 = texel;
    float tmp4 = radiusPixels;
    float tmp5 = lod;
    float3 g = bloomTentLod(m, _m_sampler, tmp2, tmp3, tmp4, tmp5);
    if (all(bool2(d.x == 0.0f.xx.x, d.y == 0.0f.xx.y)))
    {
        return g;
    }
    float2 tmp6 = uv + d;
    float2 tmp7 = texel;
    float tmp8 = radiusPixels;
    float tmp9 = lod;
    float2 tmp10 = uv - d;
    float2 tmp11 = texel;
    float tmp12 = radiusPixels;
    float tmp13 = lod;
    return float3(bloomTentLod(m, _m_sampler, tmp6, tmp7, tmp8, tmp9).x, g.y, bloomTentLod(m, _m_sampler, tmp10, tmp11, tmp12, tmp13).z);
}
#endif

#if defined(WW_FN_bloomTentDispersed) || defined(WW_FN_ALL)
float3 bloomTentDispersed(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float radiusPixels, float dispersion)
{
    float2 texel = 1.0f.xx / float2(max(int2(spvTextureSize(m, uint(0), dummy_parameter)), int2(1, 1)));
    float2 radial = uv - 0.5f.xx;
    float len = length(radial);
    float2 tmp1;
    if (len > 9.9999997473787516355514526367188e-06f)
    {
        tmp1 = ((radial / len.xx) * texel) * max(dispersion, 0.0f);
    }
    else
    {
        tmp1 = 0.0f.xx;
    }
    float2 d = tmp1;
    float2 tmp2 = uv;
    float2 tmp3 = texel;
    float tmp4 = radiusPixels;
    float3 g = bloomTent(m, _m_sampler, tmp2, tmp3, tmp4);
    if (all(bool2(d.x == 0.0f.xx.x, d.y == 0.0f.xx.y)))
    {
        return g;
    }
    float2 tmp5 = uv + d;
    float2 tmp6 = texel;
    float tmp7 = radiusPixels;
    float3 r = bloomTent(m, _m_sampler, tmp5, tmp6, tmp7);
    float2 tmp8 = uv - d;
    float2 tmp9 = texel;
    float tmp10 = radiusPixels;
    float3 b = bloomTent(m, _m_sampler, tmp8, tmp9, tmp10);
    return float3(r.x, g.y, b.z);
}
#endif
