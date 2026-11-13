// HLSL source of features/weather/rain_wet.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// src/features/weather/cellprecip.cpp wwCellRainWetUniforms: x the wet amount (0 = off, the old path), y the material's custom
	// porosity, z its weight (BGSM bCustomPorosity), w 1 = test the cover map (lane [reference game]WEATHER2). (No name "smooth": it
	// is a GLSL keyword.)
	float4 rainWet;
	float4 rainCoverGrid;
#else
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
// lane WETWEATHER1: rain wetness on every material, [the reference game's] law (src/features/weather/rainwetlaw.h). Set by

// lane WETWEATHER2: the cover map (src/features/weather/preciproof.h WwRoofMap, R32F: the highest z over a texel, -1e30 open),
// rainCoverGrid = ( world x, y of texel (0, 0)'s corner, 1 / texel side, texel side )
Texture2D<float4> rainCover : register(t46, space0); SamplerState _rainCover_sampler : register(s46, space0);

// the wetness of a pixel of this smoothness; metal 1 never wets
float wwRainWetW( float gloss, float metal )
{
	float por = clamp( lerp( 1.1666 - 1.666 * gloss, rainWet.y, rainWet.z ), 0.0, 1.0 );
	return rainWet.x * por * ( 1.0 - metal );
}

// 1 where the reflectance averages above about 0.5
float wwRainWetMetal( float3 f0 )
{
	return clamp( dot( f0, (float3)( 0.33 ) ) * 1000.0 - 500.0, 0.0, 1.0 );
}

float wwRainWetSmooth( float gloss, float w )
{
	return 1.0 - ( 1.0 - 0.4 * w ) * ( 1.0 - gloss );
}

// rainwetlaw.h coverTexelOpen: off the map or low enough = open
float wwRainCoverOpen( int2 t, int2 n, float z )
{
	if ( or( any( (t < (int2)( 0 )) ), any( (t >= n) ) ) )
		return 1.0;
	return rainCover.Load( int3(t, 0) ).r <= z ? 1.0 : 0.0;
}

// rainwetlaw.h exposed: 1 open sky, 0 under a roof or a bridge (P world, N the unit surface normal, world)
float wwRainWetExposed( float3 P, float3 N )
{
#ifndef RAIN_COVER_RED
	if ( rainWet.w < 0.5 )
		return 1.0;
	float2 q = ( P.xy + N.xy * ( 1.5 * rainCoverGrid.w ) - rainCoverGrid.xy ) * rainCoverGrid.z - 0.5;
	int2 t = int2( floor( q ) );
	float2 f = q - float2( t );
	int2 n = textureSize( rainCover, 0 );
	float z = P.z + 8.0 + rainCoverGrid.w * min( length( N.xy ) / max( abs( N.z ), 0.05 ), 2.0 );
	float e00 = wwRainCoverOpen( t, n, z ), e10 = wwRainCoverOpen( t + int2( 1, 0 ), n, z );
	float e01 = wwRainCoverOpen( t + int2( 0, 1 ), n, z ), e11 = wwRainCoverOpen( t + int2( 1, 1 ), n, z );
	return lerp( lerp( e00, e10, f.x ), lerp( e01, e11, f.x ), f.y );
#else
	return 1.0;	// tests/features/weather/raincover red: no cover test (WETWEATHER1)
#endif
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/weather/rain_wet.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwRainWetW) || defined(WW_FN_ALL)
float wwRainWetW(float gloss, float metal)
{
    float por = clamp(lerp(1.1665999889373779296875f - (1.66600000858306884765625f * gloss), rainWet.y, rainWet.z), 0.0f, 1.0f);
    return (rainWet.x * por) * (1.0f - metal);
}
#endif

#if defined(WW_FN_wwRainWetMetal) || defined(WW_FN_ALL)
float wwRainWetMetal(float3 f0)
{
    return clamp((dot(f0, 0.3300000131130218505859375f.xxx) * 1000.0f) - 500.0f, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwRainWetSmooth) || defined(WW_FN_ALL)
float wwRainWetSmooth(float gloss, float w)
{
    return 1.0f - ((1.0f - (0.4000000059604644775390625f * w)) * (1.0f - gloss));
}
#endif

#if defined(WW_FN_wwRainCoverOpen) || defined(WW_FN_ALL)
float wwRainCoverOpen(int2 t, int2 n, float z)
{
    bool cond2 = any(bool2(t.x < int2(0, 0).x, t.y < int2(0, 0).y));
    bool cond;
    if (!cond2)
    {
        cond = any(bool2(t.x >= n.x, t.y >= n.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 1.0f;
    }
    return float(rainCover.Load(int3(t, 0)).x <= z);
}
#endif

#if defined(WW_FN_wwRainWetExposed) || defined(WW_FN_ALL)
float wwRainWetExposed(float3 P, float3 N)
{
    if (rainWet.w < 0.5f)
    {
        return 1.0f;
    }
    float2 q = (((P.xy + (N.xy * (1.5f * rainCoverGrid.w))) - rainCoverGrid.xy) * rainCoverGrid.z) - 0.5f.xx;
    int2 t = int2(floor(q));
    float2 f = q - float2(t);
    int2 n = int2(spvTextureSize(rainCover, uint(0), dummy_parameter));
    float z = (P.z + 8.0f) + (rainCoverGrid.w * min(length(N.xy) / max(abs(N.z), 0.0500000007450580596923828125f), 2.0f));
    int2 cond2 = t;
    int2 cond = n;
    float e00Part = z;
    float e00 = wwRainCoverOpen(cond2, cond, e00Part);
    int2 e10Part3 = t + int2(1, 0);
    int2 e10Part2 = n;
    float e10Part = z;
    float e10 = wwRainCoverOpen(e10Part3, e10Part2, e10Part);
    int2 e01Part3 = t + int2(0, 1);
    int2 e01Part2 = n;
    float e01Part = z;
    float e01 = wwRainCoverOpen(e01Part3, e01Part2, e01Part);
    int2 e11Part3 = t + int2(1, 1);
    int2 e11Part2 = n;
    float e11Part = z;
    float e11 = wwRainCoverOpen(e11Part3, e11Part2, e11Part);
    return lerp(lerp(e00, e10, f.x), lerp(e01, e11, f.x), f.y);
}
#endif
#endif	// WW_HLSL_SRC
