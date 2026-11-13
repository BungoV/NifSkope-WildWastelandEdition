// Shared HLSL for res/shaders/land_swash.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwLandShoreGrad) || defined(WW_FN_ALL)
float3 wwLandShoreGrad(float2 worldXY)
{
    float2 p = ((worldXY - landShoreGrid.xy) / max(landShoreGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 aPart = t;
    float a = float(wwLandShoreTexel(aPart));
    int2 bPart = t + int2(1, 0);
    float b = float(wwLandShoreTexel(bPart));
    int2 cPart = t + int2(0, 1);
    float c = float(wwLandShoreTexel(cPart));
    int2 dPart = t + int2(1, 1);
    float d = float(wwLandShoreTexel(dPart));
    float qPick;
    if (landShoreOn)
    {
        qPick = landShoreGrid.w;
    }
    else
    {
        qPick = 32.0f;
    }
    float q = qPick;
    float2 g = (float2(lerp(b - a, d - c, f.y), lerp(c - a, d - b, f.x)) * q) / max(landShoreGrid.z, 1.0f).xx;
    return float3(lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * q, g);
}
#endif

#if defined(WW_FN_wwSwashR2) || defined(WW_FN_ALL)
float wwSwashR2(float h0, float T, float b)
{
    float hl = (((h0 * 9.81000041961669921875f) * T) * T) / 6.283185482025146484375f;
    return 1.10000002384185791015625f * (((0.3499999940395355224609375f * b) * sqrt(hl)) + (0.5f * sqrt(hl * (((0.563000023365020751953125f * b) * b) + 0.0040000001899898052215576171875f))));
}
#endif

#if defined(WW_FN_wwLandSwash) || defined(WW_FN_ALL)
float4 wwLandSwash(float3 P, float3 N, float t)
{
    if (((!landSwashOn) || (!landShoreOn)) || (!waterInfoOn))
    {
        return 0.0f.xxxx;
    }
    float2 aPart = P.xy;
    float3 dg = wwLandShoreGrad(aPart);
    float d = dg.x;
    float nz = clamp(N.z, 0.0500000007450580596923828125f, 1.0f);
    float b = clamp(sqrt(1.0f - (nz * nz)) / nz, 0.004999999888241291046142578125f, 1.0f);
    // m above the still line
    float z = (d * b) * 0.01428749971091747283935546875f;
    if (z > 4.0f)
    {
        return 0.0f.xxxx;
    }
    // the water: down the land distance's gradient, one water texel past the line
    float2 Pw = P.xy - ((dg.yz / max(length(dg.yz), 9.9999997473787516355514526367188e-05f).xx) * (d + waterInfoGrid.z));
    float2 bPart = Pw;
    float4 wv = wwShoreWave(bPart);
    if (wv.w < 0.5f)
    {
        return 0.0f.xxxx;
    }
    float hs = wv.x * 0.01428749971091747283935546875f;
    float2 cPart = Pw;
    bool sea = wwWaterInfo(cPart).w < 0.5f;
    float dPart;
    if (sea)
    {
        dPart = landSwashK.x;
    }
    else
    {
        dPart = clamp(sqrt((6.283185482025146484375f * hs) / 0.3923999965190887451171875f), 1.0f, 12.0f);
    }
    float T = dPart;
    float om = 6.283185482025146484375f / T;
    // u
    float L0 = (((9.81000041961669921875f * T) * T) / 6.283185482025146484375f) / 0.01428749971091747283935546875f;
    bool regular = (landSwashRed & 4) != 0;
    float qPick;
    if (regular)
    {
        qPick = 0.0f;
    }
    else
    {
        float2 qPart = Pw / (2.0f * L0).xx;
        qPick = 3.0f * (wwShoreNoise(qPart) - 0.5f);
    }
    float phi = (om * t) + qPick;
    // 0 as the bore reaches the line
    float u = frac(phi / 6.283185482025146484375f);
    float env = 1.0f;
    if (!regular)
    {
        float2 psiPart = (Pw / (3.0f * L0).xx) + 0.4099999964237213134765625f.xx;
        float psi = ((om / max(landSwashK.y, 1.0f)) * (t - (u * T))) + (6.283185482025146484375f * wwShoreNoise(psiPart));
        env = 0.3499999940395355224609375f + (0.64999997615814208984375f * (0.5f + (0.5f * cos(psi))));
    }
    float R2Part3 = hs;
    float R2Part2 = T;
    float R2Part = b;
    float R2 = wwSwashR2(R2Part3, R2Part2, R2Part);
    float R = R2 * sqrt(env);
    if ((landSwashRed & 1) != 0)
    {
        u = 0.5f;
    }
    // the tip
    float zt = ((R * 4.0f) * u) * (1.0f - u);
    // 0.1 m along the ground
    float edge = max(0.100000001490116119384765625f * b, 0.001000000047497451305389404296875f);
    float film = 1.0f - smoothstep(zt - edge, zt, z);
    float foam = 0.0f;
    if ((landSwashRed & 2) == 0)
    {
        foam = ((((film * exp((-max(zt - z, 0.0f)) / (landSwashK.w * b))) * lerp(1.0f, 0.20000000298023223876953125f, smoothstep(0.300000011920928955078125f, 0.699999988079071044921875f, u))) * wv.y) * landSwashK.z) * smoothstep(0.0f, 0.0500000007450580596923828125f, R);
    }
    float wet = 1.0f - smoothstep(0.699999988079071044921875f * R2, R2, z);
    return float4(film * smoothstep(0.0f, 0.0199999995529651641845703125f, R), foam, wet * smoothstep(0.0f, 0.0199999995529651641845703125f, R2), R);
}
#endif
