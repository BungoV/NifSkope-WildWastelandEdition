// HLSL source of land_shore.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 landShoreGrid;		// world x, y of texel (0, 0), world units a texel, world units a step
	float4 landShoreLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
	bool landShoreOn;		// built, uploaded, and not WW_CELL_LAND_SHORE_RED=off
	bool landShoreTerrain;		// the drawn shape is land (the cells' LAND or the far LOD terrain)
#else
// lane WATERWETBAKE1: the land side of the .lodl shore plane (src/features/water/cellwaterinfo.h), for the terrain shaders:
// a dry texel's distance to the nearest wet texel in 32 u steps (255 = 8160 u or more, 0 on wet texels).
// One index texel per 16 x 16-texel tile (the uniform tile's byte, or (slot + 1) << 8), and for a non-uniform
// tile one atlas texel. The cell programs' Wet shore row reads it (land_wet.glsl, lane WATERLOOK1 step 13).
Texture2D<uint4> landShoreIndex : register(t47, space0); SamplerState _landShoreIndex_sampler : register(s47, space0);	// R32UI: uniform byte, or (slot + 1) << 8
Texture2D<uint4> landShoreAtlas : register(t48, space0); SamplerState _landShoreAtlas_sampler : register(s48, space0);	// R8UI: the non-uniform tiles' bytes, 64 tiles a row

// The stored steps of texel t (0..255); 255 (far) off the map or with the lookup off
uint wwLandShoreTexel( int2 t )
{
	if ( !landShoreOn )
		return 255u;
	int E = int( landShoreLayout.z );
	int2 tile = t / E;
	if ( or( or( or( t.x < 0, t.y < 0 ), tile.x >= int( landShoreLayout.x ) ), tile.y >= int( landShoreLayout.y ) ) )
		return 255u;
	uint w = landShoreIndex.Load( int3(tile, 0) ).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = int( w >> 8 ) - 1, perRow = int( landShoreLayout.w );
	return landShoreAtlas.Load( int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0) ).r;
}

// The texel's distance to the waterline in world units at a world position (nearest texel); 8160 = far
float wwLandShore( float2 worldXY )
{
	int2 t = int2( floor( ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) ) );
	return float( wwLandShoreTexel( t ) ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}

// The same, bilinear between the four nearest texel centres (a smooth wet edge across texels and tiles)
float wwLandShoreSmooth( float2 worldXY )
{
	float2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	int2 t = int2( floor( p ) );
	float2 f = p - float2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + int2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + int2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + int2( 1, 1 ) ) );
	return lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/land_shore.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwLandShore

#if defined(WW_FN_wwLandShoreTexel) || defined(WW_FN_ALL)
uint wwLandShoreTexel(int2 t)
{
    if (!landShoreOn)
    {
        return 255u;
    }
    int E = int(landShoreLayout.z);
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
        cond2 = tile.x >= int(landShoreLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(landShoreLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 255u;
    }
    uint w = landShoreIndex.Load(int3(tile, 0)).x;
    if ((w >> uint(8)) == 0u)
    {
        return w & 255u;
    }
    int slot = int(w >> uint(8)) - 1;
    int perRow = int(landShoreLayout.w);
    return landShoreAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwLandShoreSmooth) || defined(WW_FN_ALL)
float wwLandShoreSmooth(float2 worldXY)
{
    float2 p = ((worldXY - landShoreGrid.xy) / max(landShoreGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 cond4 = t;
    float a = float(wwLandShoreTexel(cond4));
    int2 cond3 = t + int2(1, 0);
    float b = float(wwLandShoreTexel(cond3));
    int2 cond2 = t + int2(0, 1);
    float c = float(wwLandShoreTexel(cond2));
    int2 cond = t + int2(1, 1);
    float d = float(wwLandShoreTexel(cond));
    float wwLandShoreSmoothRet;
    if (landShoreOn)
    {
        wwLandShoreSmoothRet = landShoreGrid.w;
    }
    else
    {
        wwLandShoreSmoothRet = 32.0f;
    }
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * wwLandShoreSmoothRet;
}
#endif
#endif	// WW_HLSL_SRC
