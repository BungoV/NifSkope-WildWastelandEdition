// lane WATERSHORE1: the shore look of fo4_waterlook.prog from the baked shore plane (water_info.glsl), the same for
// near and far water. The plane has no land side (every dry texel reads 255), so a dry texel counts as distance 0:
// the field, bilinear between texel centres, then crosses the waterline halfway from a dry to a wet centre.
uniform int waterShoreOn;	// 1 the soft edge (Water soft edge row), 2 the foam (Shore foam row)
uniform int waterShoreRed;	// 1 nearest: the field of the nearest texel only (the 128 u staircase)
uniform vec4 waterShoreK0;	// field value at the waterline, edge width, foam width, foam softness (cellwater.cpp)
uniform vec4 waterShoreK1;	// wave length, wave period (s), foam between the crests, foam opacity
uniform float waterShoreTime;	// the world clock (s)

// one texel: (distance to the shore in world units, 1 wet / 0 dry or off the map)
vec2 wwShoreTexelValue( uint w, bool inMap )
{
	if ( !inMap )
		return vec2( 255.0 * waterInfoGrid.w, 0.0 );
	if ( ( w & 0xFFFFu ) == 0u )
		return vec2( 0.0 );
	return vec2( float( ( w >> 16 ) & 0xFFu ) * waterInfoGrid.w, 1.0 );
}

// (distance to the shore, wet texels of the 2 x 2 around the point); (0, 0) when the lookup is off
vec2 wwShoreField( vec2 worldXY )
{
	if ( !waterInfoOn )
		return vec2( 0.0 );
	vec2 g = ( worldXY - waterInfoGrid.xy ) / waterInfoGrid.z - 0.5;	// in texel centres
	ivec2 t0 = ivec2( floor( g ) );
	vec2 f = g - vec2( t0 );
	if ( ( waterShoreRed & 1 ) != 0 )
		f = step( vec2( 0.5 ), f );
	int E = int( waterInfoLayout.z );
	ivec2 n = ivec2( waterInfoLayout.xy ) * E;
	uint w00, w10, w01, w11;
	bool i00 = true, i10 = true, i01 = true, i11 = true;
	ivec2 tile = t0 / E, in0 = t0 - tile * E;
	if ( t0.x >= 0 && t0.y >= 0 && t0.x + 1 < n.x && t0.y + 1 < n.y && in0.x < E - 1 && in0.y < E - 1 ) {
		// the 2 x 2 inside one tile (15 of 16 per axis): one index fetch, and the atlas only for a sparse tile
		uvec2 ix = texelFetch( waterInfoIndex, tile, 0 ).rg;
		if ( ix.r == 0u ) {
			w00 = w10 = w01 = w11 = ix.g;
		} else {
			int slot = int( ix.r ) - 1, perRow = int( waterInfoLayout.w );
			ivec2 a = ivec2( slot % perRow, slot / perRow ) * E + in0;
			w00 = texelFetch( waterInfoAtlas, a, 0 ).r;
			w10 = texelFetch( waterInfoAtlas, a + ivec2( 1, 0 ), 0 ).r;
			w01 = texelFetch( waterInfoAtlas, a + ivec2( 0, 1 ), 0 ).r;
			w11 = texelFetch( waterInfoAtlas, a + ivec2( 1, 1 ), 0 ).r;
		}
	} else {
		vec2 c = waterInfoGrid.xy + ( vec2( t0 ) + 0.5 ) * waterInfoGrid.z;
		float z = waterInfoGrid.z;
		w00 = wwWaterInfoWord( c );
		w10 = wwWaterInfoWord( c + vec2( z, 0.0 ) );
		w01 = wwWaterInfoWord( c + vec2( 0.0, z ) );
		w11 = wwWaterInfoWord( c + vec2( z, z ) );
		i00 = all( greaterThanEqual( t0, ivec2( 0 ) ) ) && all( lessThan( t0, n ) );
		i10 = all( greaterThanEqual( t0 + ivec2( 1, 0 ), ivec2( 0 ) ) ) && all( lessThan( t0 + ivec2( 1, 0 ), n ) );
		i01 = all( greaterThanEqual( t0 + ivec2( 0, 1 ), ivec2( 0 ) ) ) && all( lessThan( t0 + ivec2( 0, 1 ), n ) );
		i11 = all( greaterThanEqual( t0 + ivec2( 1 ), ivec2( 0 ) ) ) && all( lessThan( t0 + ivec2( 1 ), n ) );
	}
	vec2 v00 = wwShoreTexelValue( w00, i00 ), v10 = wwShoreTexelValue( w10, i10 );
	vec2 v01 = wwShoreTexelValue( w01, i01 ), v11 = wwShoreTexelValue( w11, i11 );
	float d = mix( mix( v00.x, v10.x, f.x ), mix( v01.x, v11.x, f.x ), f.y );
	return vec2( d, v00.y + v10.y + v01.y + v11.y );
}

float wwShoreSmooth( float x )
{
	return x * x * ( 3.0 - 2.0 * x );
}

// the soft edge: 1 at the waterline (the scene behind shows through), 0 an edge width in. No wet texel near the
// point = the bake does not know this water (placed or interior water): 0, the game's own edge stays
float wwShoreEdge( vec2 field )
{
	if ( field.y < 0.5 )
		return 0.0;
	return 1.0 - wwShoreSmooth( clamp( ( field.x - waterShoreK0.x ) / waterShoreK0.y, 0.0, 1.0 ) );
}

// integer hash (a lowbias32 mix), the same bits on any GPU and in the Python model
uint wwShoreHash( ivec2 c )
{
	uint h = uint( c.x ) * 0x8da6b343u ^ uint( c.y ) * 0xd8163841u;
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return h;
}

// value noise 0..1 on a unit lattice: the foam's break-up (procedural, no texture)
float wwShoreNoise( vec2 p )
{
	ivec2 c = ivec2( floor( p ) );
	vec2 f = p - vec2( c );
	f = f * f * ( 3.0 - 2.0 * f );
	float a = float( wwShoreHash( c ) >> 8 ) * ( 1.0 / 16777215.0 );
	float b = float( wwShoreHash( c + ivec2( 1, 0 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	float e = float( wwShoreHash( c + ivec2( 0, 1 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	float g = float( wwShoreHash( c + ivec2( 1, 1 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	return mix( mix( a, b, f.x ), mix( e, g, f.x ), f.y );
}

/* the foam: a band from the waterline out to the foam width (Sea of Thieves 2018, Far Cry 5 2018: foam from a
 * shore distance), crests travelling shoreward one wave length per period, broken up by two octaves of noise
 * (48 u and 17 u clumps); coverage over the noise threshold, softened by K0.w */
float wwShoreFoam( vec2 field, vec2 worldXY, float t )
{
	if ( field.y < 0.5 )
		return 0.0;
	float x = max( field.x - waterShoreK0.x, 0.0 );
	float env = 1.0 - wwShoreSmooth( clamp( x / waterShoreK0.z, 0.0, 1.0 ) );
	float wave = 0.5 + 0.5 * cos( 6.2831853 * ( x / waterShoreK1.x + t / waterShoreK1.y ) );
	float cover = env * mix( waterShoreK1.z, 1.0, wave );
	float n = 0.65 * wwShoreNoise( worldXY * ( 1.0 / 48.0 ) ) + 0.35 * wwShoreNoise( worldXY * ( 1.0 / 17.0 ) + 0.37 );
	return clamp( ( n - ( 1.0 - cover ) ) / waterShoreK0.w, 0.0, 1.0 );
}
