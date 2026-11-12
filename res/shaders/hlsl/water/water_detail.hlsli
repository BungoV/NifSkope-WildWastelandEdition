// Shared HLSL for res/shaders/features/water/water_detail.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterDetailUv) || defined(WW_FN_ALL)
float2 wwWaterDetailUv(float2 wxy, float2 cs, float k, float tile, float2 scroll)
{
    float2 r = float2((cs.x * wxy.x) - (cs.y * wxy.y), (cs.y * wxy.x) + (cs.x * wxy.y));
    return (r * (k / tile)) + (scroll * k);
}
#endif

#if defined(WW_FN_wwWaterOceanDetail) || defined(WW_FN_ALL)
float2 wwWaterOceanDetail(float2 wxy, float tile, float2 scroll, float var, float w, out float keep)
{
    keep = 0.0f;
    if (((((waterOceanRed & 4096) != 0) || (w <= 0.0f)) || (var <= 0.0f)) || (tile <= 0.0f))
    {
        return 0.0f.xx;
    }
    float _all = wwLayerVarAll(waterMoment2, _waterMoment2_sampler);
    if (_all < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xx;
    }
    float2 tmp1 = wxy;
    float2 tmp2 = float2(0.7986400127410888671875f, 0.601809978485107421875f);
    float tmp3 = 4.099999904632568359375f;
    float tmp4 = tile;
    float2 tmp5 = scroll;
    float2 uA = wwWaterDetailUv(tmp1, tmp2, tmp3, tmp4, tmp5);
    float2 tmp6 = wxy;
    float2 tmp7 = float2(0.32556998729705810546875f, -0.945519983768463134765625f);
    float tmp8 = 6.69999980926513671875f;
    float tmp9 = tile;
    float2 tmp10 = scroll;
    float2 uB = wwWaterDetailUv(tmp6, tmp7, tmp8, tmp9, tmp10);
    float2 tA = waterNoise2.Sample(_waterNoise2_sampler, uA).xy;
    float2 tB = waterNoise2.Sample(_waterNoise2_sampler, uB).xy;
    float2 tmp11 = uA;
    float2 tmp12 = tA;
    float2 tmp13 = uB;
    float2 tmp14 = tB;
    float lost = wwLayerVar(waterMoment2, _waterMoment2_sampler, tmp11, tmp12) + wwLayerVar(waterMoment2, _waterMoment2_sampler, tmp13, tmp14);
    keep = (clamp(1.0f - (lost / (2.0f * _all)), 0.0f, 1.0f) * w) * w;
    float2 sA = (tA * 2.0f) - 1.0f.xx;
    float2 sB = (tB * 2.0f) - 1.0f.xx;
    float2 s = float2((0.7986400127410888671875f * sA.x) + (0.601809978485107421875f * sA.y), ((-0.601809978485107421875f) * sA.x) + (0.7986400127410888671875f * sA.y)) + float2((0.32556998729705810546875f * sB.x) + ((-0.945519983768463134765625f) * sB.y), (0.945519983768463134765625f * sB.x) + (0.32556998729705810546875f * sB.y));
    return s * ((rsqrt(2.0f * _all) * sqrt(var)) * w);
}
#endif
