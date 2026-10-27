// lane WATERWETBAKE1: the land side of the .lodl shore plane (src/gl/cellwaterinfo.h), for the terrain shaders:
// a dry texel's distance to the nearest wet texel in 32 u steps (255 = 8160 u or more, 0 on wet texels).
// One index texel per 16 x 16-texel tile (the uniform tile's byte, or (slot + 1) << 8), and for a non-uniform
// tile one atlas texel. The cell programs' Wet shore row reads it (land_wet.glsl, lane WATERLOOK1 step 13).
uniform usampler2D landShoreIndex;	// R32UI: uniform byte, or (slot + 1) << 8
uniform usampler2D landShoreAtlas;	// R8UI: the non-uniform tiles' bytes, 64 tiles a row
uniform vec4 landShoreGrid;		// world x, y of texel (0, 0), world units a texel, world units a step
uniform vec4 landShoreLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool landShoreOn;		// built, uploaded, and not WW_CELL_LAND_SHORE_RED=off
uniform bool landShoreTerrain;		// the drawn shape is land (the cells' LAND or the far LOD terrain)

// The stored steps of texel t (0..255); 255 (far) off the map or with the lookup off
uint wwLandShoreTexel( ivec2 t )
{
	if ( !landShoreOn )
		return 255u;
	int E = int( landShoreLayout.z );
	ivec2 tile = t / E;
	if ( t.x < 0 || t.y < 0 || tile.x >= int( landShoreLayout.x ) || tile.y >= int( landShoreLayout.y ) )
		return 255u;
	uint w = texelFetch( landShoreIndex, tile, 0 ).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = int( w >> 8 ) - 1, perRow = int( landShoreLayout.w );
	return texelFetch( landShoreAtlas, ivec2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0 ).r;
}

// The texel's distance to the waterline in world units at a world position (nearest texel); 8160 = far
float wwLandShore( vec2 worldXY )
{
	ivec2 t = ivec2( floor( ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) ) );
	return float( wwLandShoreTexel( t ) ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}

// The same, bilinear between the four nearest texel centres (a smooth wet edge across texels and tiles)
float wwLandShoreSmooth( vec2 worldXY )
{
	vec2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	ivec2 t = ivec2( floor( p ) );
	vec2 f = p - vec2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + ivec2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + ivec2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + ivec2( 1, 1 ) ) );
	return mix( mix( a, b, f.x ), mix( c, d, f.x ), f.y ) * ( landShoreOn ? landShoreGrid.w : 32.0 );
}
