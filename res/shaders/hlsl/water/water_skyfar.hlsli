// HLSL source of features/water/water_skyfar.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 waterSkyGrid;		// world x, y of texel (0, 0), world units a texel, 254 (byte scale)
	float4 waterSkyLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
	bool waterSkyOn;		// a sky plane uploaded and not the red
#else
// lane WATERSKYVISFAR1 (src/features/water/cellwaterinfo.h): the far water's sky visibility through the terrain, from the .lodl
// sky plane (version 4): one byte a 128 u body texel, 0 dry, else 1 + round(vis * 254). Only fo4_waterlook.frag
// compiles this (WW_WATER_SKYFAR). 1 with no sky plane, the red (WW_CELL_WATER_SKYFAR_RED=one) or no wet texel near.
Texture2D<uint4> waterSkyIndex : register(t42, space0); SamplerState _waterSkyIndex_sampler : register(s42, space0);	// R32UI: uniform byte, or (slot + 1) << 8
Texture2D<uint4> waterSkyAtlas : register(t43, space0); SamplerState _waterSkyAtlas_sampler : register(s43, space0);	// R8UI: the non-uniform tiles' bytes, 64 tiles a row

// The stored byte of texel t; 0 (dry) off the map
uint wwWaterSkyTexel( int2 t )
{
	int E = int( waterSkyLayout.z );
	int2 tile = t / E;
	if ( or( or( or( t.x < 0, t.y < 0 ), tile.x >= int( waterSkyLayout.x ) ), tile.y >= int( waterSkyLayout.y ) ) )
		return 0u;
	uint w = waterSkyIndex.Load( int3(tile, 0) ).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = int( w >> 8 ) - 1, perRow = int( waterSkyLayout.w );
	return waterSkyAtlas.Load( int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0) ).r;
}

// Bilinear over the four nearest texel centres, wet texels only (a dry neighbour carries no evidence)
float wwWaterSkyFar( float3 P )
{
	if ( !waterSkyOn )
		return 1.0;
	float2 p = ( P.xy - waterSkyGrid.xy ) / max( waterSkyGrid.z, 1.0 ) - 0.5;
	int2 t = int2( floor( p ) );
	float2 f = p - float2( t );
	float4 b = float4( float( wwWaterSkyTexel( t ) ), float( wwWaterSkyTexel( t + int2( 1, 0 ) ) ),
		float( wwWaterSkyTexel( t + int2( 0, 1 ) ) ), float( wwWaterSkyTexel( t + int2( 1, 1 ) ) ) );
	float4 w = float4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y )
		* step( 0.5, b );
	float den = dot( w, (float4)( 1.0 ) );
	return den > 1e-4 ? dot( w, ( b - 1.0 ) / waterSkyGrid.w ) / den : 1.0;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_skyfar.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSkyTexel) || defined(WW_FN_ALL)
uint wwWaterSkyTexel(int2 t)
{
    int E = int(waterSkyLayout.z);
    int2 tile = t / E.xx;
    bool cond4 = t.x < 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = t.y < 0;
    }
    else
    {
        cond3 = cond4;
    }
    bool cond2;
    if (!cond3)
    {
        cond2 = tile.x >= int(waterSkyLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterSkyLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0u;
    }
    uint w = waterSkyIndex.Load(int3(tile, 0)).x;
    if ((w >> uint(8)) == 0u)
    {
        return w & 255u;
    }
    int slot = int(w >> uint(8)) - 1;
    int perRow = int(waterSkyLayout.w);
    return waterSkyAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwWaterSkyFar) || defined(WW_FN_ALL)
float wwWaterSkyFar(float3 P)
{
    if (!waterSkyOn)
    {
        return 1.0f;
    }
    float2 p = ((P.xy - waterSkyGrid.xy) / max(waterSkyGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 cond4 = t;
    int2 cond3 = t + int2(1, 0);
    int2 cond2 = t + int2(0, 1);
    int2 cond = t + int2(1, 1);
    float4 b = float4(float(wwWaterSkyTexel(cond4)), float(wwWaterSkyTexel(cond3)), float(wwWaterSkyTexel(cond2)), float(wwWaterSkyTexel(cond)));
    float4 w = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y) * step(0.5f.xxxx, b);
    float den = dot(w, 1.0f.xxxx);
    float wwWaterSkyFarRet;
    if (den > 9.9999997473787516355514526367188e-05f)
    {
        wwWaterSkyFarRet = dot(w, (b - 1.0f.xxxx) / waterSkyGrid.w.xxxx) / den;
    }
    else
    {
        wwWaterSkyFarRet = 1.0f;
    }
    return wwWaterSkyFarRet;
}
#endif
#endif	// WW_HLSL_SRC
