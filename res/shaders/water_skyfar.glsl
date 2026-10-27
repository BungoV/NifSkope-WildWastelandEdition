// lane WATERSKYVISFAR1 (src/gl/cellwaterinfo.h): the far water's sky visibility through the terrain, from the .lodl
// sky plane (version 4): one byte a 128 u body texel, 0 dry, else 1 + round(vis * 254). Only fo4_waterlook.frag
// compiles this (WW_WATER_SKYFAR). 1 with no sky plane, the red (WW_CELL_WATER_SKYFAR_RED=one) or no wet texel near.
uniform usampler2D waterSkyIndex;	// R32UI: uniform byte, or (slot + 1) << 8
uniform usampler2D waterSkyAtlas;	// R8UI: the non-uniform tiles' bytes, 64 tiles a row
uniform vec4 waterSkyGrid;		// world x, y of texel (0, 0), world units a texel, 254 (byte scale)
uniform vec4 waterSkyLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool waterSkyOn;		// a sky plane uploaded and not the red

// The stored byte of texel t; 0 (dry) off the map
uint wwWaterSkyTexel( ivec2 t )
{
	int E = int( waterSkyLayout.z );
	ivec2 tile = t / E;
	if ( t.x < 0 || t.y < 0 || tile.x >= int( waterSkyLayout.x ) || tile.y >= int( waterSkyLayout.y ) )
		return 0u;
	uint w = texelFetch( waterSkyIndex, tile, 0 ).r;
	if ( ( w >> 8 ) == 0u )
		return w & 0xFFu;
	int slot = int( w >> 8 ) - 1, perRow = int( waterSkyLayout.w );
	return texelFetch( waterSkyAtlas, ivec2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0 ).r;
}

// Bilinear over the four nearest texel centres, wet texels only (a dry neighbour carries no evidence)
float wwWaterSkyFar( vec3 P )
{
	if ( !waterSkyOn )
		return 1.0;
	vec2 p = ( P.xy - waterSkyGrid.xy ) / max( waterSkyGrid.z, 1.0 ) - 0.5;
	ivec2 t = ivec2( floor( p ) );
	vec2 f = p - vec2( t );
	vec4 b = vec4( float( wwWaterSkyTexel( t ) ), float( wwWaterSkyTexel( t + ivec2( 1, 0 ) ) ),
		float( wwWaterSkyTexel( t + ivec2( 0, 1 ) ) ), float( wwWaterSkyTexel( t + ivec2( 1, 1 ) ) ) );
	vec4 w = vec4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y )
		* step( 0.5, b );
	float den = dot( w, vec4( 1.0 ) );
	return den > 1e-4 ? dot( w, ( b - 1.0 ) / waterSkyGrid.w ) / den : 1.0;
}
