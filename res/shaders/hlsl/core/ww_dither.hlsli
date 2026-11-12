// Shared HLSL for res/shaders/ww_dither.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_hdr_ps.hlsl, game_taa_ps.hlsl, taa2_ps.hlsl

#if defined(WW_FN_wwDitherHash) || defined(WW_FN_ALL)
float wwDitherHash(uint2 p, uint s)
{
    uint h = ((p.x * 1664525u) + (p.y * 1013904223u)) + (s * 2654435769u);
    h ^= (h >> uint(16));
    h *= 2246822519u;
    h ^= (h >> uint(13));
    h *= 3266489917u;
    h ^= (h >> uint(16));
    return float(h >> uint(8)) * 5.9604644775390625e-08f;
}
#endif

#if defined(WW_FN_wwDither8) || defined(WW_FN_ALL)
float3 wwDither8(float3 c, float2 fragCoord)
{
    if (wwDitherOn == 0)
    {
        return c;
    }
    uint2 p = uint2(fragCoord);
    uint s = uint(wwDitherSeed);
    uint2 tmp1 = p;
    uint tmp2 = 2u * s;
    uint2 tmp3 = p;
    uint tmp4 = (2u * s) + 1u;
    float n = (wwDitherHash(tmp1, tmp2) + wwDitherHash(tmp3, tmp4)) - 1.0f;
    float3 w = clamp(min(c, 1.0f.xxx - c) * 255.0f, 0.0f.xxx, 1.0f.xxx);
    return c + ((w * n) * 0.0039215688593685626983642578125f);
}
#endif
