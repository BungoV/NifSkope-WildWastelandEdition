// Shared HLSL for res/shaders/features/post/game_doflens.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: game_doflens_prep_ps.hlsl, game_doflens_ps.hlsl, game_doflensmblur_ps.hlsl
// Not here (declarations, or no converted entry carried them): lensTexel

#if defined(WW_FN_lensSat) || defined(WW_FN_ALL)
float lensSat(float v)
{
    float lensSatRet;
    if (v >= 0.0f)
    {
        lensSatRet = min(v, 1.0f);
    }
    else
    {
        lensSatRet = 0.0f;
    }
    return lensSatRet;
}
#endif

#if defined(WW_FN_lensDist) || defined(WW_FN_ALL)
float lensDist(float d)
{
    float x = 1.0f - d;
    float lensSatRet;
    if (9.9999997473787516355514526367188e-06f < x)
    {
        lensSatRet = lz.z / ((x * lz.y) + lz.x);
    }
    else
    {
        lensSatRet = -1.0f;
    }
    return lensSatRet;
}
#endif

#if defined(WW_FN_lensFocus) || defined(WW_FN_ALL)
float lensFocus(Texture2D<float4> depthTex, SamplerState _depthTex_sampler)
{
    float f = max(lensP.z, 0.001000000047497451305389404296875f);
    float s1 = lensP.x;
    if (!(s1 > 0.0f))
    {
        int2 c = int2(spvTextureSize(depthTex, uint(0), dummy_parameter)) / int2(2, 2);
        float lensSatRet = depthTex.Load(int3(c, 0)).x;
        float z = lensDist(lensSatRet);
        s1 = (z > 0.0f) ? z : 1000000.0f;
    }
    return max(s1, f * 1.0099999904632568359375f);
}
#endif

#if defined(WW_FN_lensCoc) || defined(WW_FN_ALL)
float lensCoc(float z, float s1)
{
    float f = max(lensP.z, 0.001000000047497451305389404296875f);
    float c;
    if (z < 0.0f)
    {
        float lensSatRet;
        if (lz.w != 0.0f)
        {
            lensSatRet = 0.0f;
        }
        else
        {
            lensSatRet = (lensP.y * f) / (s1 - f);
        }
        // the sky: sharp, or the lens's limit at infinity
        c = lensSatRet;
    }
    else
    {
        c = ((lensP.y * f) * (z - s1)) / (z * (s1 - f));
    }
    if (lensQ.z == 0.0f)
    {
        c = min(c, 0.0f);
    }
    if (lensQ.y == 0.0f)
    {
        c = max(c, 0.0f);
    }
    return clamp(c, -lensQ.x, lensQ.x);
}
#endif
