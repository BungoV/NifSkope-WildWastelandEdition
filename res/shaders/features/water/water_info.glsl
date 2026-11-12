// lane WATERDATA1: the baked .lodl water planes (src/features/water/cellwaterinfo.h), one lookup a water pixel:
// an index texel per 16 x 16-texel tile, and for a non-uniform tile one atlas texel; the class from the body table.
uniform usampler2D waterInfoIndex;	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
uniform usampler2D waterInfoAtlas;	// R32UI: the non-uniform tiles' words, 64 tiles a row
uniform sampler2D waterInfoBodies;	// RGBA32F, 256 bodies a row: height, class, flags, area (body 0 = none)
uniform vec4 waterInfoGrid;		// world x, y of texel (0, 0), world units a texel, world units a shore step
uniform vec4 waterInfoLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool waterInfoOn;		// built, uploaded, and not WW_CELL_WATER_INFO_RED=off
// lane FARSEA1 (src/features/water/endlesssea.h): the endless sea. x = the seam band (u; 0 = no endless sea, every word
// as baked), y = the sea body's id, z = 1 on the endless sheet's own draw; the bake's box (x0, y0, x1, y1)
uniform vec4 waterSeam;
uniform vec4 waterSeamBox;

// lane FARSEA1: 1 a band inside the bake's box, 0 at its edge (the open sea)
float wwSeamFade( vec2 worldXY )
{
	vec2 a = worldXY - waterSeamBox.xy, b = waterSeamBox.zw - worldXY;
	return clamp( min( min( a.x, a.y ), min( b.x, b.y ) ) / max( waterSeam.x, 1.0 ), 0.0, 1.0 );
}

// The packed word as baked: body id | shore steps << 16 | depth code << 24; the neutral word off the map
uint wwWaterInfoRaw( vec2 worldXY )
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

/* The word every reader takes. With the endless sea (lane FARSEA1): the sea's words fade to the open sea over the
 * seam band at the box edge (shore steps and depth code to 255), and on the endless sheet a dry or off-map word is
 * the open sea itself (the sea body, no shore, deep). Without it, the word as baked. */
uint wwWaterInfoWord( vec2 worldXY )
{
	uint w = wwWaterInfoRaw( worldXY );
	if ( waterSeam.x <= 0.0 || !waterInfoOn )
		return w;
	uint id = w & 0xFFFFu, sea = uint( waterSeam.y );
	if ( id == 0u )
		return waterSeam.z > 0.0 ? ( sea | 0xFFFF0000u ) : w;
	if ( id != sea )
		return w;
	float f = wwSeamFade( worldXY );
	uint sh = uint( mix( 255.0, float( ( w >> 16 ) & 0xFFu ), f ) + 0.5 );
	uint dc = uint( mix( 255.0, float( w >> 24 ), f ) + 0.5 );
	return id | ( sh << 16 ) | ( dc << 24 );
}

// lane SHORE2: a far water fragment on a sample the .lodl calls dry (body 0); never while the lookup is not built
bool wwFarWaterDry( vec2 worldXY )
{
	return waterInfoOn && ( wwWaterInfoRaw( worldXY ) & 0xFFFFu ) == 0u;
}

// lane WATERFLOWBAKE1: the baked flow plane as a velocity, the same sparse layout at the flow plane's rate
uniform usampler2D waterFlowIndex;	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
uniform usampler2D waterFlowAtlas;	// R32UI: the non-uniform tiles' words, 64 tiles a row
uniform vec4 waterFlowGrid;		// world x, y of texel (0, 0), world units a texel, counts a world unit a second
uniform vec4 waterFlowLayout;		// tiles x, tiles y, texels a tile edge, tiles an atlas row
uniform bool waterFlowOn;		// built, uploaded, the file has a flow plane, and not WW_CELL_WATER_INFO_RED=off

// The water's velocity at a world position, world units a second (x east, y north); (0, 0) still, dry, off the map or off
vec2 wwWaterFlow( vec2 worldXY )
{
	if ( !waterFlowOn )
		return vec2( 0.0 );
	ivec2 t = ivec2( floor( ( worldXY - waterFlowGrid.xy ) / waterFlowGrid.z ) );
	int E = int( waterFlowLayout.z );
	ivec2 tile = t / E;
	if ( t.x < 0 || t.y < 0 || tile.x >= int( waterFlowLayout.x ) || tile.y >= int( waterFlowLayout.y ) )
		return vec2( 0.0 );
	uvec2 ix = texelFetch( waterFlowIndex, tile, 0 ).rg;
	uint w = ix.g;
	if ( ix.r != 0u ) {
		int slot = int( ix.r ) - 1, perRow = int( waterFlowLayout.w );
		w = texelFetch( waterFlowAtlas, ivec2( slot % perRow, slot / perRow ) * E + ( t - tile * E ), 0 ).r;
	}
	vec2 v = vec2( float( int( w << 16u ) >> 16 ), float( int( w ) >> 16 ) ) / waterFlowGrid.w;
	return waterSeam.x > 0.0 ? v * wwSeamFade( worldXY ) : v;	// lane FARSEA1: still at the bake's edge
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
