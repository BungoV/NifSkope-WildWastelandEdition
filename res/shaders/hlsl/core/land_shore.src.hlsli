// HLSL source of land_shore.glsl, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
#pragma once
#include "GlslCompat.hlsli"
// lane WATERWETBAKE1: the land side of the .lodl shore plane (src/features/water/cellwaterinfo.h), for the terrain shaders:
// a dry texel's distance to the nearest wet texel in 32 u steps (255 = 8160 u or more, 0 on wet texels).
// One index texel per 16 x 16-texel tile (the uniform tile's byte, or (slot + 1) << 8), and for a non-uniform
// tile one atlas texel. The cell programs' Wet shore row reads it (land_wet.glsl, lane WATERLOOK1 step 13).
Texture2D<uint4> landShoreIndex; SamplerState _landShoreIndex_sampler;	// R32UI: uniform byte, or (slot + 1) << 8
Texture2D<uint4> landShoreAtlas; SamplerState _landShoreAtlas_sampler;	// R8UI: the non-uniform tiles' bytes, 64 tiles a row
uniform float4 landShoreGrid;		// world x, y of texel (0, 0), world units a texel, world units a step
uniform float4 landShoreLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool landShoreOn;		// built, uploaded, and not WW_CELL_LAND_SHORE_RED=off
uniform bool landShoreTerrain;		// the drawn shape is land (the cells' LAND or the far LOD terrain)

// The stored steps of texel t (0..255); 255 (far) off the map or with the lookup off
uint wwLandShoreTexel( int2 t )
{
	if ( !landShoreOn )
		return 255u;
	int E = (int)(landShoreLayout.z);
	int2 tile = t / E;
	if ( or(or(or(t.x < 0, t.y < 0), tile.x >= (int)(landShoreLayout.x)), tile.y >= (int)(landShoreLayout.y)) )
		return 255u;
	uint w = landShoreIndex.Load(int3(tile, 0)).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = (int)(w >> 8) - 1, perRow = (int)(landShoreLayout.w);
	return landShoreAtlas.Load(int3(int2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0)).r;
}

// The texel's distance to the waterline in world units at a world position (nearest texel); 8160 = far
float wwLandShore( float2 worldXY )
{
	int2 t = (int2)(floor( ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) ));
	return (float)(wwLandShoreTexel( t )) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}

// The same, bilinear between the four nearest texel centres (a smooth wet edge across texels and tiles)
float wwLandShoreSmooth( float2 worldXY )
{
	float2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	int2 t = (int2)(floor( p ));
	float2 f = p - (float2)(t);
	float a = (float)(wwLandShoreTexel( t )), b = (float)(wwLandShoreTexel( t + int2( 1, 0 ) ));
	float c = (float)(wwLandShoreTexel( t + int2( 0, 1 ) )), d = (float)(wwLandShoreTexel( t + int2( 1, 1 ) ));
	return lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}
