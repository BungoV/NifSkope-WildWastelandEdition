/* lane FARDRAW1 (bungo 10-26): the far ring 0's ground drawn live per pixel (src/gl/cellfarground.cpp) -- the
 * .lodl splat (the terrain preview's splatGrid: four bilinear samples, the top-4 LTEX layers, x the vertex colour)
 * with the .lodg/.lodd decals over it, in place of the baked sheet's colour. Two decal options:
 *   WW_FARGROUND 1  each pixel walks its 512-unit bin's record list (the preview's unproject, in record order)
 *   WW_FARGROUND 2  the decals were stamped once into a page a cell (premultiplied: ground x (1 - a) + rgb)
 *   WW_FARGROUND 3  lane FARPAGES1: layers + decals painted once into a BC1 page a cell (cell_fgpage + cell_fgbc1)
 * The stamp (cell_fgstamp.frag) defines WW_FG_STAMP and runs option 1's walk into the pages. */

uniform int fgMode;				// 0 = the sheet (the twin's uniforms not set), else the live ground
uniform int fgDecalsOn;			// red WW_CELL_FARGROUND_RED=nodecals: 0
uniform usampler2D fgGrid;		// per .lodl sample: ids 0|1, ids 2|3, weights unorm8 x4, colour RGB8 + page
uniform sampler2DArray fgLtex;
uniform vec4 fgGridMap;			// origin x, y (world), 1 / sample spacing, LTEX tiling (world units a repeat)
uniform vec2 fgGridN;

#if defined( WW_FG_STAMP ) || WW_FARGROUND == 1
uniform samplerBuffer fgDecal;	// 5 texels a record, then (start, count) a bin, then the record ids, 4 a texel
uniform sampler2D fgAtlas;
uniform vec4 fgBinMap;			// origin x, y, 1 / bin size, the atlas' last mip
uniform vec4 fgBinN;			// bins x, y, the bins' first texel, the ids' first texel
#endif
#if !defined( WW_FG_STAMP ) && WW_FARGROUND == 2
uniform sampler2DArray fgPages;
#endif
#if !defined( WW_FG_STAMP ) && WW_FARGROUND == 3
// lane FARPAGES1: one page a cell holds layers + decals (BC1); r = pool (0 none, 1 near, 2 far), g = layer, b = fade
uniform usampler2D fgCells;
uniform sampler2DArray fgPagesN;
uniform sampler2DArray fgPagesF;
#endif

vec3 fgQrot( vec4 q, vec3 v )
{
	return v + 2.0 * cross( q.xyz, cross( q.xyz, v ) + q.w * v );
}

vec3 fgSplat( vec2 w, vec2 wdx, vec2 wdy )
{
	vec2 g = ( w - fgGridMap.xy ) * fgGridMap.z;
	vec2 g0 = floor( g ), f = g - g0;
	vec2 uv = w / fgGridMap.w, dx = wdx / fgGridMap.w, dy = wdy / fgGridMap.w;
	ivec2 n = ivec2( fgGridN );
	vec3 acc = vec3( 0.0 ), vc = vec3( 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), n - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		uvec4 s = texelFetch( fgGrid, p, 0 );
		vc += bw * unpackUnorm4x8( s.w ).rgb;
		if ( bw <= 0.0 )
			continue;
		vec4 wt = unpackUnorm4x8( s.z );
		uint id[4] = uint[4]( s.x & 0xFFFFu, s.x >> 16, s.y & 0xFFFFu, s.y >> 16 );
		for ( int k = 0; k < 4; k++ ) {
			if ( wt[k] <= 0.0 )
				continue;
			vec3 t = id[k] == 65535u ? vec3( 0.5 ) : textureGrad( fgLtex, vec3( uv, float( id[k] ) ), dx, dy ).rgb;
			acc += bw * wt[k] * t;
		}
	}
	return acc * vc;
}

#if defined( WW_FG_STAMP ) || WW_FARGROUND == 1
// the decals over world point w, premultiplied, composited in record order; pix = world units a pixel
vec4 fgDecals( vec3 w, float pix )
{
	ivec2 b = ivec2( floor( ( w.xy - fgBinMap.xy ) * fgBinMap.z ) );
	ivec2 bn = ivec2( fgBinN.xy );
	if ( b.x < 0 || b.y < 0 || b.x >= bn.x || b.y >= bn.y )
		return vec4( 0.0 );
	vec4 bin = texelFetch( fgDecal, int( fgBinN.z ) + b.y * bn.x + b.x );
	int start = int( bin.x ), cnt = int( bin.y );
	vec4 acc = vec4( 0.0 );
	for ( int i = 0; i < cnt; i++ ) {
		int k = start + i;
		int r = int( texelFetch( fgDecal, int( fgBinN.w ) + ( k >> 2 ) )[k & 3] ) * 5;
		vec4 ps = texelFetch( fgDecal, r );
		vec4 q = texelFetch( fgDecal, r + 1 );
		vec4 b0 = texelFetch( fgDecal, r + 2 );
		vec4 b1 = texelFetch( fgDecal, r + 3 );
		vec3 l = fgQrot( vec4( -q.xyz, q.w ), w - ps.xyz ) / ps.w;
		if ( l.x < b0.x || l.x > b0.z || l.y < b0.y || l.y > b0.w || l.z < b1.x || l.z > b1.y )
			continue;
		vec4 at = texelFetch( fgDecal, r + 4 );
		vec2 uv = vec2( ( l.x - b0.x ) / ( b0.z - b0.x ), ( b0.w - l.y ) / ( b0.w - b0.y ) );
		float lod = clamp( log2( max( 1.0, pix / ( b1.z * ps.w ) ) ), 0.0, fgBinMap.w );
		vec4 c = textureLod( fgAtlas, mix( at.xy, at.zw, uv ), lod );
		acc = vec4( c.rgb * c.a, c.a ) + acc * ( 1.0 - c.a );
	}
	return acc;
}
#endif

#ifndef WW_FG_STAMP
// the live ground at world point w; `sheet` (the baked colour) where the grid misses
vec3 fgGround( vec3 w, vec3 sheet )
{
	if ( fgMode == 0 )
		return sheet;
#if WW_FARGROUND == 3
	// no splat and no decal walk a frame: the cell's page, faded in over the sheet
	vec2 rel = ( w.xy - fgGridMap.xy ) / 4096.0;
	vec2 rdx = dFdx( rel ), rdy = dFdy( rel );
	ivec2 cn = textureSize( fgCells, 0 );
	ivec2 ci = ivec2( floor( rel ) );
	if ( ci.x < 0 || ci.y < 0 || ci.x >= cn.x || ci.y >= cn.y )
		return sheet;
	uvec4 c = texelFetch( fgCells, ci, 0 );
	if ( c.r == 0u || c.b == 0u )
		return sheet;
	vec3 uvl = vec3( rel - vec2( ci ), float( c.g ) );
	vec3 page = c.r == 1u ? textureGrad( fgPagesN, uvl, rdx, rdy ).rgb : textureGrad( fgPagesF, uvl, rdx, rdy ).rgb;
	return mix( sheet, page, float( c.b ) / 255.0 );
#else
	vec2 wdx = dFdx( w.xy ), wdy = dFdy( w.xy );
	vec3 col = fgSplat( w.xy, wdx, wdy );
	if ( fgDecalsOn == 0 )
		return col;
#if WW_FARGROUND == 1
	float pix = max( length( dFdx( w ) ), length( dFdy( w ) ) );
	vec4 d = fgDecals( w, pix );
#else
	// the page of the cell under w (the grid sample's top byte), uv across the cell
	vec2 rel = ( w.xy - fgGridMap.xy ) / 4096.0;
	vec2 cell = floor( rel );
	ivec2 n = ivec2( fgGridN );
	uint pg = texelFetch( fgGrid, clamp( ivec2( floor( ( w.xy - fgGridMap.xy ) * fgGridMap.z ) ), ivec2( 0 ), n - 1 ), 0 ).w >> 24;
	vec4 d = vec4( 0.0 );
	if ( pg != 255u )
		d = textureGrad( fgPages, vec3( rel - cell, float( pg ) ), wdx / 4096.0, wdy / 4096.0 );
#endif
	return col * ( 1.0 - d.a ) + d.rgb;
#endif
}
#endif
