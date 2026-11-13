// HLSL source of features/far-ground/cell_farground.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int fgMode;				// 0 = the sheet (the twin's uniforms not set), else the live ground
	int fgDecalsOn;			// red WW_CELL_FARGROUND_RED=nodecals: 0
	float4 fgGridMap;			// origin x, y (world), 1 / sample spacing, LTEX tiling (world units a repeat)
	float2 fgGridN;
#if ((defined( WW_FG_STAMP ) || WW_FARGROUND == 1))
	float4 fgBinMap;			// origin x, y, 1 / bin size, the atlas' last mip
	float4 fgBinN;			// bins x, y, the bins' first texel, the ids' first texel
#endif
#else
#ifndef WW_GLSL_UNPACKUNORM4
#define WW_GLSL_UNPACKUNORM4
float4 ww_unpackUnorm4x8(uint v) { return float4(v & 0xffu, (v >> 8) & 0xffu, (v >> 16) & 0xffu, v >> 24) / 255.0; }
#endif
#ifndef WW_GLSL_SIZE_Texture2D_uint4_
#define WW_GLSL_SIZE_Texture2D_uint4_
int2 textureSize(Texture2D<uint4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
/* lane FARDRAW1 (bungo 10-26): the far ring 0's ground drawn live per pixel (src/features/far-ground/cellfarground.cpp) -- the
 * .lodl splat (the terrain preview's splatGrid: four bilinear samples, the top-4 LTEX layers, x the vertex colour)
 * with the .lodg/.lodd decals over it, in place of the baked sheet's colour. Two decal options:
 *   WW_FARGROUND 1  each pixel walks its 512-unit bin's record list (the preview's unproject, in record order)
 *   WW_FARGROUND 2  the decals were stamped once into a page a cell (premultiplied: ground x (1 - a) + rgb)
 *   WW_FARGROUND 3  lane FARPAGES1: layers + decals painted once into a BC1 page a cell (cell_fgpage + cell_fgbc1)
 * The stamp (cell_fgstamp.frag) defines WW_FG_STAMP and runs option 1's walk into the pages. */


Texture2D<uint4> fgGrid : register(t17, space0); SamplerState _fgGrid_sampler : register(s17, space0);		// per .lodl sample: ids 0|1, ids 2|3, weights unorm8 x4, colour RGB8 + page
Texture2DArray<float4> fgLtex : register(t18, space0); SamplerState _fgLtex_sampler : register(s18, space0);

#if defined( WW_FG_STAMP ) || WW_FARGROUND == 1
Buffer<float4> fgDecal : register(t19, space0);	// 5 texels a record, then (start, count) a bin, then the record ids, 4 a texel
Texture2D<float4> fgAtlas : register(t20, space0); SamplerState _fgAtlas_sampler : register(s20, space0);
#endif
#if !defined( WW_FG_STAMP ) && WW_FARGROUND == 2
Texture2DArray<float4> fgPages : register(t21, space0); SamplerState _fgPages_sampler : register(s21, space0);
#endif
#if !defined( WW_FG_STAMP ) && WW_FARGROUND == 3
// lane FARPAGES1: one page a cell holds layers + decals (BC1); r = pool (0 none, 1 near, 2 far), g = layer, b = fade
Texture2D<uint4> fgCells : register(t22, space0); SamplerState _fgCells_sampler : register(s22, space0);
Texture2DArray<float4> fgPagesN : register(t23, space0); SamplerState _fgPagesN_sampler : register(s23, space0);
Texture2DArray<float4> fgPagesF : register(t24, space0); SamplerState _fgPagesF_sampler : register(s24, space0);
#endif

float3 fgQrot( float4 q, float3 v )
{
	return v + 2.0 * cross( q.xyz, cross( q.xyz, v ) + q.w * v );
}

float3 fgSplat( float2 w, float2 wdx, float2 wdy )
{
	float2 g = ( w - fgGridMap.xy ) * fgGridMap.z;
	float2 g0 = floor( g ), f = g - g0;
	float2 uv = w / fgGridMap.w, dx = wdx / fgGridMap.w, dy = wdy / fgGridMap.w;
	int2 n = int2( fgGridN );
	float3 acc = (float3)( 0.0 ), vc = (float3)( 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int2 o = int2( c & 1, c >> 1 );
		int2 p = clamp( int2( g0 ) + o, (int2)( 0 ), n - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		uint4 s = fgGrid.Load( int3(p, 0) );
		vc += bw * ww_unpackUnorm4x8( s.w ).rgb;
		if ( bw <= 0.0 )
			continue;
		float4 wt = ww_unpackUnorm4x8( s.z );
		uint id[4] = { s.x & 0xFFFFu, s.x >> 16, s.y & 0xFFFFu, s.y >> 16 };
		for ( int k = 0; k < 4; k++ ) {
			if ( wt[k] <= 0.0 )
				continue;
			float3 t = id[k] == 65535u ? (float3)( 0.5 ) : fgLtex.SampleGrad( _fgLtex_sampler, float3( uv, float( id[k] ) ), dx, dy ).rgb;
			acc += bw * wt[k] * t;
		}
	}
	return acc * vc;
}

#if defined( WW_FG_STAMP ) || WW_FARGROUND == 1
// the decals over world point w, premultiplied, composited in record order; pix = world units a pixel
float4 fgDecals( float3 w, float pix )
{
	int2 b = int2( floor( ( w.xy - fgBinMap.xy ) * fgBinMap.z ) );
	int2 bn = int2( fgBinN.xy );
	if ( or( or( or( b.x < 0, b.y < 0 ), b.x >= bn.x ), b.y >= bn.y ) )
		return (float4)( 0.0 );
	float4 bin = fgDecal.Load( int( fgBinN.z ) + b.y * bn.x + b.x );
	int start = int( bin.x ), cnt = int( bin.y );
	float4 acc = (float4)( 0.0 );
	for ( int i = 0; i < cnt; i++ ) {
		int k = start + i;
		int r = int( fgDecal.Load( int( fgBinN.w ) + ( k >> 2 ) )[k & 3] ) * 5;
		float4 ps = fgDecal.Load( r );
		float4 q = fgDecal.Load( r + 1 );
		float4 b0 = fgDecal.Load( r + 2 );
		float4 b1 = fgDecal.Load( r + 3 );
		float3 l = fgQrot( float4( -q.xyz, q.w ), w - ps.xyz ) / ps.w;
		if ( or( or( or( or( or( l.x < b0.x, l.x > b0.z ), l.y < b0.y ), l.y > b0.w ), l.z < b1.x ), l.z > b1.y ) )
			continue;
		float4 at = fgDecal.Load( r + 4 );
		float2 uv = float2( ( l.x - b0.x ) / ( b0.z - b0.x ), ( b0.w - l.y ) / ( b0.w - b0.y ) );
		float lod = clamp( log2( max( 1.0, pix / ( b1.z * ps.w ) ) ), 0.0, fgBinMap.w );
		float4 c = fgAtlas.SampleLevel( _fgAtlas_sampler, lerp( at.xy, at.zw, uv ), lod );
		acc = float4( c.rgb * c.a, c.a ) + acc * ( 1.0 - c.a );
	}
	return acc;
}
#endif

#ifndef WW_FG_STAMP
// the live ground at world point w; `sheet` (the baked colour) where the grid misses
float3 fgGround( float3 w, float3 sheet )
{
	if ( fgMode == 0 )
		return sheet;
#if WW_FARGROUND == 3
	// no splat and no decal walk a frame: the cell's page, faded in over the sheet
	float2 rel = ( w.xy - fgGridMap.xy ) / 4096.0;
	float2 rdx = ddx( rel ), rdy = ddy( rel );
	int2 cn = textureSize( fgCells, 0 );
	int2 ci = int2( floor( rel ) );
	if ( or( or( or( ci.x < 0, ci.y < 0 ), ci.x >= cn.x ), ci.y >= cn.y ) )
		return sheet;
	uint4 c = fgCells.Load( int3(ci, 0) );
	if ( or( c.r == 0u, c.b == 0u ) )
		return sheet;
	float3 uvl = float3( rel - float2( ci ), float( c.g ) );
	float3 page = c.r == 1u ? fgPagesN.SampleGrad( _fgPagesN_sampler, uvl, rdx, rdy ).rgb : fgPagesF.SampleGrad( _fgPagesF_sampler, uvl, rdx, rdy ).rgb;
	return lerp( sheet, page, float( c.b ) / 255.0 );
#else
	float2 wdx = ddx( w.xy ), wdy = ddy( w.xy );
	float3 col = fgSplat( w.xy, wdx, wdy );
	if ( fgDecalsOn == 0 )
		return col;
#if WW_FARGROUND == 1
	float pix = max( length( ddx( w ) ), length( ddy( w ) ) );
	float4 d = fgDecals( w, pix );
#else
	// the page of the cell under w (the grid sample's top byte), uv across the cell
	float2 rel = ( w.xy - fgGridMap.xy ) / 4096.0;
	float2 cell = floor( rel );
	int2 n = int2( fgGridN );
	uint pg = fgGrid.Load( int3(clamp( int2( floor( ( w.xy - fgGridMap.xy ) * fgGridMap.z ) ), (int2)( 0 ), n - 1 ), 0) ).w >> 24;
	float4 d = (float4)( 0.0 );
	if ( pg != 255u )
		d = fgPages.SampleGrad( _fgPages_sampler, float3( rel - cell, float( pg ) ), wdx / 4096.0, wdy / 4096.0 );
#endif
	return col * ( 1.0 - d.a ) + d.rgb;
#endif
}
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/cell_farground.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_fgpage_ps.hlsl, cell_fgstamp_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl

#if defined(WW_FN_fgQrot) || defined(WW_FN_ALL)
float3 fgQrot(float4 q, float3 v)
{
    return v + (cross(q.xyz, cross(q.xyz, v) + (v * q.w)) * 2.0f);
}
#endif

#if defined(WW_FN_fgSplat) || defined(WW_FN_ALL)
float3 fgSplat(float2 w, float2 wdx, float2 wdy)
{
    float2 g = (w - fgGridMap.xy) * fgGridMap.z;
    float2 g0 = floor(g);
    float2 f = g - g0;
    float2 uv = w / fgGridMap.w.xx;
    float2 dx = wdx / fgGridMap.w.xx;
    float2 dy = wdy / fgGridMap.w.xx;
    int2 n = int2(fgGridN);
    float3 acc = 0.0f.xxx;
    float3 vc = 0.0f.xxx;
    float bwPick2;
    float bwPick;
    float3 tPick;
    for (int c = 0; c < 4; c++)
    {
        int2 o = int2(c & 1, c >> 1);
        int2 p = clamp(int2(g0) + o, int2(0, 0), n - int2(1, 1));
        if (o.x == 1)
        {
            bwPick2 = f.x;
        }
        else
        {
            bwPick2 = 1.0f - f.x;
        }
        if (o.y == 1)
        {
            bwPick = f.y;
        }
        else
        {
            bwPick = 1.0f - f.y;
        }
        float bw = bwPick2 * bwPick;
        uint4 s = fgGrid.Load(int3(p, 0));
        vc += (spvUnpackUnorm4x8(s.w).xyz * bw);
        if (bw <= 0.0f)
        {
            continue;
        }
        float4 wt = spvUnpackUnorm4x8(s.z);
        uint idPart[4] = { s.x & 65535u, s.x >> uint(16), s.y & 65535u, s.y >> uint(16) };
        uint id[4] = idPart;
        for (int k = 0; k < 4; k++)
        {
            if (wt[k] <= 0.0f)
            {
                continue;
            }
            if (id[k] == 65535u)
            {
                tPick = 0.5f.xxx;
            }
            else
            {
                tPick = fgLtex.SampleGrad(_fgLtex_sampler, float3(uv, float(id[k])), dx, dy).xyz;
            }
            float3 t = tPick;
            acc += (t * (bw * wt[k]));
        }
    }
    return acc * vc;
}
#endif

#if defined(WW_FN_fgDecals) || defined(WW_FN_ALL)
float4 fgDecals(float3 w, float pix)
{
    int2 b = int2(floor((w.xy - fgBinMap.xy) * fgBinMap.z));
    int2 bn = int2(fgBinN.xy);
    bool bwPick2 = b.x < 0;
    bool bwPick;
    if (!bwPick2)
    {
        bwPick = b.y < 0;
    }
    else
    {
        bwPick = bwPick2;
    }
    bool tPick;
    if (!bwPick)
    {
        tPick = b.x >= bn.x;
    }
    else
    {
        tPick = bwPick;
    }
    bool idPart;
    if (!tPick)
    {
        idPart = b.y >= bn.y;
    }
    else
    {
        idPart = tPick;
    }
    if (idPart)
    {
        return 0.0f.xxxx;
    }
    float4 bin = fgDecal.Load((int(fgBinN.z) + (b.y * bn.x)) + b.x);
    int start = int(bin.x);
    int cnt = int(bin.y);
    float4 acc = 0.0f.xxxx;
    for (int i = 0; i < cnt; i++)
    {
        int k = start + i;
        int r = int(fgDecal.Load(int(fgBinN.w) + (k >> 2))[k & 3]) * 5;
        float4 ps = fgDecal.Load(r);
        float4 q = fgDecal.Load(r + 1);
        float4 b0 = fgDecal.Load(r + 2);
        float4 b1 = fgDecal.Load(r + 3);
        float4 lPart2 = float4(-q.xyz, q.w);
        float3 lPart = w - ps.xyz;
        float3 l = fgQrot(lPart2, lPart) / ps.w.xxx;
        bool cond6 = l.x < b0.x;
        bool cond5;
        if (!cond6)
        {
            cond5 = l.x > b0.z;
        }
        else
        {
            cond5 = cond6;
        }
        bool cond4;
        if (!cond5)
        {
            cond4 = l.y < b0.y;
        }
        else
        {
            cond4 = cond5;
        }
        bool cond3;
        if (!cond4)
        {
            cond3 = l.y > b0.w;
        }
        else
        {
            cond3 = cond4;
        }
        bool cond2;
        if (!cond3)
        {
            cond2 = l.z < b1.x;
        }
        else
        {
            cond2 = cond3;
        }
        bool cond;
        if (!cond2)
        {
            cond = l.z > b1.y;
        }
        else
        {
            cond = cond2;
        }
        if (cond)
        {
            continue;
        }
        float4 at = fgDecal.Load(r + 4);
        float2 uv = float2((l.x - b0.x) / (b0.z - b0.x), (b0.w - l.y) / (b0.w - b0.y));
        float lod = clamp(log2(max(1.0f, pix / (b1.z * ps.w))), 0.0f, fgBinMap.w);
        float4 c = fgAtlas.SampleLevel(_fgAtlas_sampler, lerp(at.xy, at.zw, uv), lod);
        acc = float4(c.xyz * c.w, c.w) + (acc * (1.0f - c.w));
    }
    return acc;
}
#endif

#if defined(WW_FN_fgGround) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 wdx = ddx(w.xy);
    float2 wdy = ddy(w.xy);
    float2 bwPick2 = w.xy;
    float2 bwPick = wdx;
    float2 tPick = wdy;
    float3 col = fgSplat(bwPick2, bwPick, tPick);
    if (fgDecalsOn == 0)
    {
        return col;
    }
    float2 rel = (w.xy - fgGridMap.xy) / 4096.0f.xx;
    float2 cell = floor(rel);
    int2 n = int2(fgGridN);
    uint pg = fgGrid.Load(int3(clamp(int2(floor((w.xy - fgGridMap.xy) * fgGridMap.z)), int2(0, 0), n - int2(1, 1)), 0)).w >> uint(24);
    float4 d = 0.0f.xxxx;
    if (pg != 255u)
    {
        d = fgPages.SampleGrad(_fgPages_sampler, float3(rel - cell, float(pg)), wdx / 4096.0f.xx, wdy / 4096.0f.xx);
    }
    return (col * (1.0f - d.w)) + d.xyz;
}
#endif

#if defined(WW_FN_fgGround_V2) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 wdx = ddx(w.xy);
    float2 wdy = ddy(w.xy);
    float2 bwPick2 = w.xy;
    float2 bwPick = wdx;
    float2 tPick = wdy;
    float3 col = fgSplat(bwPick2, bwPick, tPick);
    if (fgDecalsOn == 0)
    {
        return col;
    }
    float pix = max(length(ddx(w)), length(ddy(w)));
    float3 idPart = w;
    float lPart2 = pix;
    float4 d = fgDecals(idPart, lPart2);
    return (col * (1.0f - d.w)) + d.xyz;
}
#endif

#if defined(WW_FN_fgGround_V3) || defined(WW_FN_ALL)
float3 fgGround(float3 w, float3 sheet)
{
    if (fgMode == 0)
    {
        return sheet;
    }
    float2 rel = (w.xy - fgGridMap.xy) / 4096.0f.xx;
    float2 rdx = ddx(rel);
    float2 rdy = ddy(rel);
    int2 cn = int2(spvTextureSize(fgCells, uint(0), dummy_parameter));
    int2 ci = int2(floor(rel));
    bool bwPick2 = ci.x < 0;
    bool bwPick;
    if (!bwPick2)
    {
        bwPick = ci.y < 0;
    }
    else
    {
        bwPick = bwPick2;
    }
    bool tPick;
    if (!bwPick)
    {
        tPick = ci.x >= cn.x;
    }
    else
    {
        tPick = bwPick;
    }
    bool idPart;
    if (!tPick)
    {
        idPart = ci.y >= cn.y;
    }
    else
    {
        idPart = tPick;
    }
    if (idPart)
    {
        return sheet;
    }
    uint4 c = fgCells.Load(int3(ci, 0));
    bool lPart2 = c.x == 0u;
    bool lPart;
    if (!lPart2)
    {
        lPart = c.z == 0u;
    }
    else
    {
        lPart = lPart2;
    }
    if (lPart)
    {
        return sheet;
    }
    float3 uvl = float3(rel - float2(ci), float(c.y));
    float3 cond6;
    if (c.x == 1u)
    {
        cond6 = fgPagesN.SampleGrad(_fgPagesN_sampler, uvl, rdx, rdy).xyz;
    }
    else
    {
        cond6 = fgPagesF.SampleGrad(_fgPagesF_sampler, uvl, rdx, rdy).xyz;
    }
    float3 page = cond6;
    return lerp(sheet, page, (float(c.z) / 255.0f).xxx);
}
#endif
#endif	// WW_HLSL_SRC
