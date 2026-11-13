// HLSL source of features/water/water_info.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 waterInfoGrid;		// world x, y of texel (0, 0), world units a texel, world units a shore step
	float4 waterInfoLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
	bool waterInfoOn;		// built, uploaded, and not WW_CELL_WATER_INFO_RED=off
	// lane FARSEA1 (src/features/water/endlesssea.h): the endless sea. x = the seam band (u; 0 = no endless sea, every word
	// as baked), y = the sea body's id, z = 1 on the endless sheet's own draw; the bake's box (x0, y0, x1, y1)
	float4 waterSeam;
	float4 waterSeamBox;
	float4 waterFlowGrid;		// world x, y of texel (0, 0), world units a texel, counts a world unit a second
	float4 waterFlowLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
	bool waterFlowOn;		// built, uploaded, the file has a flow plane, and not WW_CELL_WATER_INFO_RED=off
#else
// lane WATERDATA1: the baked .lodl water planes (src/features/water/cellwaterinfo.h), one lookup a water pixel:
// an index texel per 16 x 16-texel tile, and for a non-uniform tile one atlas texel; the class from the body table.
Texture2D<uint4> waterInfoIndex : register(t35, space0); SamplerState _waterInfoIndex_sampler : register(s35, space0);	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
Texture2D<uint4> waterInfoAtlas : register(t36, space0); SamplerState _waterInfoAtlas_sampler : register(s36, space0);	// R32UI: the non-uniform tiles' words, 64 tiles a row
Texture2D<float4> waterInfoBodies : register(t37, space0); SamplerState _waterInfoBodies_sampler : register(s37, space0);	// RGBA32F, 256 bodies a row: height, class, flags, area (body 0 = none)

// lane FARSEA1: 1 a band inside the bake's box, 0 at its edge (the open sea)
float wwSeamFade( float2 worldXY )
{
	float2 a = worldXY - waterSeamBox.xy, b = waterSeamBox.zw - worldXY;
	return clamp( min( min( a.x, a.y ), min( b.x, b.y ) ) / max( waterSeam.x, 1.0 ), 0.0, 1.0 );
}

// The packed word as baked: body id | shore steps << 16 | depth code << 24; the neutral word off the map
uint wwWaterInfoRaw( float2 worldXY )
{
	const uint neutral = 255u << 16;
	if ( !waterInfoOn )
		return neutral;
	int2 t = int2( floor( ( worldXY - waterInfoGrid.xy ) / waterInfoGrid.z ) );
	int E = int( waterInfoLayout.z );
	int2 tile = t / E;
	if ( or( or( or( t.x < 0, t.y < 0 ), tile.x >= int( waterInfoLayout.x ) ), tile.y >= int( waterInfoLayout.y ) ) )
		return neutral;
	uint2 ix = waterInfoIndex.Load( int3(tile, 0) ).rg;
	if ( ix.r == 0u )
		return ix.g;
	int slot = int( ix.r ) - 1, perRow = int( waterInfoLayout.w );
	return waterInfoAtlas.Load( int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0) ).r;
}

/* The word every reader takes. With the endless sea (lane FARSEA1): the sea's words fade to the open sea over the
 * seam band at the box edge (shore steps and depth code to 255), and on the endless sheet a dry or off-map word is
 * the open sea itself (the sea body, no shore, deep). Without it, the word as baked. */
uint wwWaterInfoWord( float2 worldXY )
{
	uint w = wwWaterInfoRaw( worldXY );
	if ( or( waterSeam.x <= 0.0, !waterInfoOn ) )
		return w;
	uint id = w & 0xFFFFu, sea = uint( waterSeam.y );
	if ( id == 0u )
		return waterSeam.z > 0.0 ? ( sea | 0xFFFF0000u ) : w;
	if ( id != sea )
		return w;
	float f = wwSeamFade( worldXY );
	uint sh = uint( lerp( 255.0, float( ( w >> 16 ) & 0xFFu ), f ) + 0.5 );
	uint dc = uint( lerp( 255.0, float( w >> 24 ), f ) + 0.5 );
	return id | ( sh << 16 ) | ( dc << 24 );
}

// lane SHORE2: a far water fragment on a sample the .lodl calls dry (body 0); never while the lookup is not built
bool wwFarWaterDry( float2 worldXY )
{
	return waterInfoOn && ( wwWaterInfoRaw( worldXY ) & 0xFFFFu ) == 0u;
}

// lane WATERFLOWBAKE1: the baked flow plane as a velocity, the same sparse layout at the flow plane's rate
Texture2D<uint4> waterFlowIndex : register(t38, space0); SamplerState _waterFlowIndex_sampler : register(s38, space0);	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
Texture2D<uint4> waterFlowAtlas : register(t39, space0); SamplerState _waterFlowAtlas_sampler : register(s39, space0);	// R32UI: the non-uniform tiles' words, 64 tiles a row

// The water's velocity at a world position, world units a second (x east, y north); (0, 0) still, dry, off the map or off
float2 wwWaterFlow( float2 worldXY )
{
	if ( !waterFlowOn )
		return (float2)( 0.0 );
	int2 t = int2( floor( ( worldXY - waterFlowGrid.xy ) / waterFlowGrid.z ) );
	int E = int( waterFlowLayout.z );
	int2 tile = t / E;
	if ( or( or( or( t.x < 0, t.y < 0 ), tile.x >= int( waterFlowLayout.x ) ), tile.y >= int( waterFlowLayout.y ) ) )
		return (float2)( 0.0 );
	uint2 ix = waterFlowIndex.Load( int3(tile, 0) ).rg;
	uint w = ix.g;
	if ( ix.r != 0u ) {
		int slot = int( ix.r ) - 1, perRow = int( waterFlowLayout.w );
		w = waterFlowAtlas.Load( int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0) ).r;
	}
	float2 v = float2( float( int( w << 16u ) >> 16 ), float( int( w ) >> 16 ) ) / waterFlowGrid.w;
	return waterSeam.x > 0.0 ? v * wwSeamFade( worldXY ) : v;	// lane FARSEA1: still at the bake's edge
}

// (shore distance in world units, depth in world units, body id, class 0 sea 1 river 2 lake); neutral = (8160, 0, 0, -1)
float4 wwWaterInfo( float2 worldXY )
{
	uint w = wwWaterInfoWord( worldXY );
	uint id = w & 0xFFFFu;
	float code = float( w >> 24 ) / 255.0;
	float cls = id != 0u ? waterInfoBodies.Load( int3(int2( int( id % 256u ), int( id / 256u ) ), 0) ).g : -1.0;
	float q = waterInfoOn ? waterInfoGrid.w : 32.0;
	return float4( float( ( w >> 16 ) & 0xFFu ) * q, code * code * 8192.0, float( id ), cls );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_info.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwSeamFade) || defined(WW_FN_ALL)
float wwSeamFade(float2 worldXY)
{
    float2 a = worldXY - waterSeamBox.xy;
    float2 b = waterSeamBox.zw - worldXY;
    return clamp(min(min(a.x, a.y), min(b.x, b.y)) / max(waterSeam.x, 1.0f), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwWaterInfoRaw) || defined(WW_FN_ALL)
uint wwWaterInfoRaw(float2 worldXY)
{
    if (!waterInfoOn)
    {
        return 16711680u;
    }
    int2 t = int2(floor((worldXY - waterInfoGrid.xy) / waterInfoGrid.z.xx));
    int E = int(waterInfoLayout.z);
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
        cond2 = tile.x >= int(waterInfoLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterInfoLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 16711680u;
    }
    uint2 ix = waterInfoIndex.Load(int3(tile, 0)).xy;
    if (ix.x == 0u)
    {
        return ix.y;
    }
    int slot = int(ix.x) - 1;
    int perRow = int(waterInfoLayout.w);
    return waterInfoAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}
#endif

#if defined(WW_FN_wwWaterInfoWord) || defined(WW_FN_ALL)
uint wwWaterInfoWord(float2 worldXY)
{
    float2 cond4 = worldXY;
    uint w = wwWaterInfoRaw(cond4);
    bool wwh0 = (waterSeam.x <= 0.0f);
    if (wwh0 || (!waterInfoOn))
    {
        return w;
    }
    uint id = w & 65535u;
    uint sea = uint(waterSeam.y);
    if (id == 0u)
    {
        uint cond3;
        if (waterSeam.z > 0.0f)
        {
            cond3 = sea | 4294901760u;
        }
        else
        {
            cond3 = w;
        }
        return cond3;
    }
    if (id != sea)
    {
        return w;
    }
    float2 cond2 = worldXY;
    float f = wwSeamFade(cond2);
    uint sh = uint(lerp(255.0f, float((w >> uint(16)) & 255u), f) + 0.5f);
    uint dc = uint(lerp(255.0f, float(w >> uint(24)), f) + 0.5f);
    return (id | (sh << uint(16))) | (dc << uint(24));
}
#endif

#if defined(WW_FN_wwFarWaterDry) || defined(WW_FN_ALL)
bool wwFarWaterDry(float2 worldXY)
{
    bool cond4;
    if (waterInfoOn)
    {
        float2 cond3 = worldXY;
        cond4 = (wwWaterInfoRaw(cond3) & 65535u) == 0u;
    }
    else
    {
        cond4 = waterInfoOn;
    }
    return cond4;
}
#endif

#if defined(WW_FN_wwWaterFlow) || defined(WW_FN_ALL)
float2 wwWaterFlow(float2 worldXY)
{
    if (!waterFlowOn)
    {
        return 0.0f.xx;
    }
    int2 t = int2(floor((worldXY - waterFlowGrid.xy) / waterFlowGrid.z.xx));
    int E = int(waterFlowLayout.z);
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
        cond2 = tile.x >= int(waterFlowLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterFlowLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0.0f.xx;
    }
    uint2 ix = waterFlowIndex.Load(int3(tile, 0)).xy;
    uint w = ix.y;
    if (ix.x != 0u)
    {
        int slot = int(ix.x) - 1;
        int perRow = int(waterFlowLayout.w);
        w = waterFlowAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
    }
    float2 v = float2(float(int(w << 16u) >> 16), float(int(w) >> 16)) / waterFlowGrid.w.xx;
    float2 wwWaterFlowRet;
    if (waterSeam.x > 0.0f)
    {
        float2 wwWaterFlowPart = worldXY;
        wwWaterFlowRet = v * wwSeamFade(wwWaterFlowPart);
    }
    else
    {
        wwWaterFlowRet = v;
    }
    return wwWaterFlowRet;
}
#endif

#if defined(WW_FN_wwWaterInfo) || defined(WW_FN_ALL)
float4 wwWaterInfo(float2 worldXY)
{
    float2 cond4 = worldXY;
    uint w = wwWaterInfoWord(cond4);
    uint id = w & 65535u;
    float code = float(w >> uint(24)) / 255.0f;
    float cond3;
    if (id != 0u)
    {
        cond3 = waterInfoBodies.Load(int3(int2(int(id % 256u), int(id / 256u)), 0)).y;
    }
    else
    {
        cond3 = -1.0f;
    }
    float cls = cond3;
    float cond2;
    if (waterInfoOn)
    {
        cond2 = waterInfoGrid.w;
    }
    else
    {
        cond2 = 32.0f;
    }
    float q = cond2;
    return float4(float((w >> uint(16)) & 255u) * q, (code * code) * 8192.0f, float(id), cls);
}
#endif
#endif	// WW_HLSL_SRC
