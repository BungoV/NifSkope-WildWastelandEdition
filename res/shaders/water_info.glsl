// lane WATERDATA1: the baked .lodl water planes (src/gl/cellwaterinfo.h), one lookup a water pixel:
// an index texel per 16 x 16-texel tile, and for a non-uniform tile one atlas texel; the class from the body table.
uniform usampler2D waterInfoIndex;	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
uniform usampler2D waterInfoAtlas;	// R32UI: the non-uniform tiles' words, 64 tiles a row
uniform sampler2D waterInfoBodies;	// RGBA32F, 256 bodies a row: height, class, flags, area (body 0 = none)
uniform vec4 waterInfoGrid;		// world x, y of texel (0, 0), world units a texel, world units a shore step
uniform vec4 waterInfoLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool waterInfoOn;		// built, uploaded, and not WW_CELL_WATER_INFO_RED=off

// The packed word at a world position: body id | shore steps << 16 | depth code << 24; the neutral word off the map
uint wwWaterInfoWord( vec2 worldXY )
{
	const uint neutral = 255u << 16;
	if ( !waterInfoOn )
		return neutral;
	ivec2 t = ivec2( floor( ( worldXY - waterInfoGrid.xy ) / waterInfoGrid.z ) );
	int E = int( waterInfoLayout.z );
	ivec2 tile = t / E;
	if ( t.x < 0 || t.y < 0 || tile.x >= int( waterInfoLayout.x ) || tile.y >= int( waterInfoLayout.y ) )
		return neutral;
	uvec2 ix = texelFetch( waterInfoIndex, tile, 0 ).rg;
	if ( ix.r == 0u )
		return ix.g;
	int slot = int( ix.r ) - 1, perRow = int( waterInfoLayout.w );
	return texelFetch( waterInfoAtlas, ivec2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0 ).r;
}

// (shore distance in world units, depth in world units, body id, class 0 sea 1 river 2 lake); neutral = (8160, 0, 0, -1)
vec4 wwWaterInfo( vec2 worldXY )
{
	uint w = wwWaterInfoWord( worldXY );
	uint id = w & 0xFFFFu;
	float code = float( w >> 24 ) / 255.0;
	float cls = id != 0u ? texelFetch( waterInfoBodies, ivec2( int( id % 256u ), int( id / 256u ) ), 0 ).g : -1.0;
	float q = waterInfoOn ? waterInfoGrid.w : 32.0;
	return vec4( float( ( w >> 16 ) & 0xFFu ) * q, code * code * 8192.0, float( id ), cls );
}
