// lane WATERSHORE1: the shore look of fo4_waterlook.prog from the baked shore plane (water_info.glsl), the same for
// near and far water. The plane has no land side (every dry texel reads 255), so a dry texel counts as distance 0:
// the field, bilinear between texel centres, then crosses the waterline halfway from a dry to a wet centre.
uniform int waterShoreOn;	// 1 the soft edge (Water soft edge row), 2 the foam (Shore foam row)
uniform int waterShoreRed;	// 1 nearest: the field of the nearest texel only (the 128 u staircase)
uniform vec4 waterShoreK0;	// field value at the waterline, edge width, foam width, foam softness (cellwater.cpp)
uniform vec4 waterShoreK1;	// wave length, wave period (s), foam between the crests, foam opacity
uniform float waterShoreTime;	// the world clock (s)
uniform int waterShoreLaw;	// WATERFOAMDETAIL1: 1 the wave-energy foam (wwShoreFoamWave), 0 the old band (red old)
uniform vec4 waterShoreK2;	// wind speed (m/s), sea swell height (m), river fetch cap (m), full-foam wave height (m)
uniform vec4 waterShoreK3;	// fresh-water foam persistence, breaker index (H/d), lap line opacity, lap line onset height (m)
uniform int waterShoreEdgeLaw;	// WATERFOAMDETAIL1: 1 the soft contact (wwShoreEdgeSoft), 0 the baked band only (red old)
uniform vec4 waterShoreK4;	// contact width (u) looking straight down, at a grazing view; the bake's error (u); 0

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

/* lane WATERFOAMDETAIL1: the soft contact. 1 where the water meets land or any object, 0 a width in. The distance is
 * the shorter of the water path to the scene behind (along the view ray, from the depth buffer: banks, rocks, debris,
 * actors) and the baked distance past the waterline plus the bake's error (land the depth buffer does not hold). The
 * width grows from K4.x looking straight down to K4.y at a grazing view (|viewZ| = the view ray's vertical part) */
float wwShoreEdgeSoft( vec2 field, float along, float viewZ )
{
	float d = along;
	if ( field.y > 0.5 )
		d = min( d, max( field.x - waterShoreK0.x, 0.0 ) + waterShoreK4.z );
	if ( d >= 1e8 )
		return 0.0;
	float w = mix( waterShoreK4.y, waterShoreK4.x, clamp( abs( viewZ ), 0.0, 1.0 ) );
	return 1.0 - wwShoreSmooth( clamp( d / w, 0.0, 1.0 ) );
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

/* lane WATERFOAMDETAIL1: foam that follows the wave energy at the shore (notes/waterfoamdetail1/RESEARCH.md).
 * The body's significant wave height: wind sea by fetch (JONSWAP, Hasselmann 1973: Hs = 0.0016 U sqrt( F / g ), capped
 * at the fully developed 0.21 U^2 / g), fetch = the body's length (sqrt of its baked area), a river's capped at its
 * width; the sea adds the swell (energies add). Waves break where the water is shallower than H / gamma (McCowan 1894)
 * and make foam there in proportion to their energy flux (H^2.5), less in fresh water (whitecaps decay faster there,
 * Monahan & Zietlow 1969); ripples run up the bank about H: a thin translucent lap line. (Hs u, persistence, baked
 * depth u, 1 when a baked body is under P) */
const float kShoreM = 0.0142875;	// metres a game unit
vec4 wwShoreWave( vec2 P )
{
	if ( !waterInfoOn )
		return vec4( 0.0 );
	float tex = waterInfoGrid.z;
	vec2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	vec2 t0 = floor( g );
	vec2 f = g - t0;
	vec2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uvec4 w = uvec4( wwWaterInfoWord( c0 ), wwWaterInfoWord( c0 + vec2( tex, 0.0 ) ),
		wwWaterInfoWord( c0 + vec2( 0.0, tex ) ), wwWaterInfoWord( c0 + vec2( tex, tex ) ) );
	vec4 bw = vec4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uvec4 id = w & 0xFFFFu;
	uint body = 0u;
	float best = -1.0;
	for ( int k = 0; k < 4; k++ )
		if ( id[k] != 0u && bw[k] > best ) {
			best = bw[k];
			body = id[k];
		}
	if ( body == 0u )
		return vec4( 0.0 );
	vec4 code = vec4( w >> 24 ) / 255.0;
	float depth = dot( bw, vec4( equal( id, uvec4( body ) ) ) * code * code * 8192.0 );	// dry = 0
	vec4 row = texelFetch( waterInfoBodies, ivec2( int( body % 256u ), int( body / 256u ) ), 0 );
	int cls = clamp( int( row.g + 0.5 ), 0, 2 );
	float len = sqrt( max( row.a, 1.0 ) ) * tex * kShoreM;
	float fetch = cls == 1 ? min( len, waterShoreK2.z ) : len;
	float U = waterShoreK2.x;
	float hw = min( 0.0016 * U * sqrt( fetch / 9.81 ), 0.21 * U * U / 9.81 );
	float hs = cls == 0 ? sqrt( hw * hw + waterShoreK2.y * waterShoreK2.y ) : hw;
	return vec4( hs / kShoreM, cls == 0 ? 1.0 : waterShoreK3.x, depth, 1.0 );
}

float wwFoamHash01( ivec2 c )
{
	return float( wwShoreHash( c ) >> 8 ) * ( 1.0 / 16777215.0 );
}

// value noise on a lattice of k cells that repeats every k (the world wrapped by 2048 u, so float32 keeps its detail)
float wwFoamNoise( vec2 p, int k, ivec2 seed )
{
	vec2 fl = floor( p );
	vec2 f = p - fl;
	f = f * f * ( 3.0 - 2.0 * f );
	ivec2 c = ivec2( fl ) % k, c1 = ( c + 1 ) % k;	// p is in [0, k]: no negative %
	float a = wwFoamHash01( c + seed ), b = wwFoamHash01( ivec2( c1.x, c.y ) + seed );
	float e = wwFoamHash01( ivec2( c.x, c1.y ) + seed ), h = wwFoamHash01( c1 + seed );
	return mix( mix( a, b, f.x ), mix( e, h, f.x ), f.y );
}

// the bubble lace: cellular (Worley) F2 - F1 on k cells, 0 on the bubble walls
float wwFoamLace( vec2 p, int k )
{
	vec2 c = floor( p );
	float f1 = 8.0, f2 = 8.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ ) {
			ivec2 cc = ivec2( c ) + ivec2( x, y );
			uint h = wwShoreHash( ( cc + k ) % k + ivec2( 331, 977 ) );	// cc >= -1
			vec2 q = vec2( cc ) + vec2( float( h & 0xFFFFu ), float( h >> 16 ) ) * ( 1.0 / 65535.0 );
			float d = length( p - q );
			f2 = d < f1 ? f1 : min( f2, d );
			f1 = min( f1, d );
		}
	return f2 - f1;
}

/* the foam's texture: five octaves of value noise (24, 8, 2.7, 0.9, 0.3 u) and a 1.2 u bubble lace; each layer fades
 * to its mean once a pixel covers more than half of it (no shimmer far away, full detail close up).
 * Returns (noise 0..1, lace alpha 0..1, the share of the noise's detail still drawn) */
vec3 wwFoamDetail( vec2 P, float foot, float t )
{
	vec2 Pw = P - 2048.0 * floor( P * ( 1.0 / 2048.0 ) );
	const int K[5] = int[5]( 85, 256, 758, 2276, 6827 );
	const float Wt[5] = float[5]( 0.36, 0.26, 0.18, 0.12, 0.08 );
	float n = 0.0, drawn = 0.0;
	for ( int i = 0; i < 5; i++ ) {
		float s = 2048.0 / float( K[i] );
		float vis = 1.0 - smoothstep( 0.35, 0.7, foot / s );
		vec2 p = Pw / s + t * vec2( 0.11, 0.07 ) * float( i );	// the finer layers drift slowly (bubbles turning over)
		p = mod( p, float( K[i] ) );
		float v = vis > 0.0 ? wwFoamNoise( p, K[i], ivec2( 17 * i, 101 * i ) ) : 0.5;
		n += Wt[i] * mix( 0.5, v, vis );
		drawn += Wt[i] * vis;
	}
	n = clamp( 0.5 + ( n - 0.5 ) * 2.2, 0.0, 1.0 );
	float lvis = 1.0 - smoothstep( 0.35, 0.7, foot / 1.2 );
	float lace = 0.45;
	if ( lvis > 0.0 ) {
		vec2 p = mod( Pw / 1.2 + t * vec2( 0.05, -0.04 ), 1707.0 );
		float wall = 1.0 - smoothstep( 0.04, 0.30, wwFoamLace( p, 1707 ) );
		lace = mix( 0.45, 0.15 + 0.85 * wall, lvis );
	}
	return vec3( n, lace, drawn );
}

/* the foam alpha (before the opacity K1.w) at P: field = wwShoreField, column = the water above the scene behind
 * (vertical, u; >= 1e8 when unknown: the baked depth then), colFoot = its change across the pixel, foot = world units
 * a pixel. dbg = (Hs / 64 u, energy amount, the column used / 64 u) for probe 38 */
float wwShoreFoamWave( vec2 field, vec2 P, float t, float column, float colFoot, float foot, out vec3 dbg )
{
	dbg = vec3( 0.0 );
	if ( field.y < 0.5 )
		return 0.0;
	vec4 wv = wwShoreWave( P );
	if ( wv.w < 0.5 )
		return 0.0;
	float hs = max( wv.x, 1e-3 );	// no wind on a river: no foam, and no empty smoothstep
	bool scene = column < 1e8;
	float dep = scene ? column : wv.z;
	float dF = scene ? colFoot : 0.0;
	float amount = clamp( pow( hs * kShoreM / waterShoreK2.w, 2.5 ) * wv.y, 0.0, 1.0 );
	dbg = vec3( hs / 64.0, amount, dep / 64.0 );
	// the surf: breaking where the column is under H / gamma, crests running in as before
	float db = hs / waterShoreK3.y;
	float surf = 1.0 - smoothstep( 0.6 * db, 1.2 * db + dF, dep );
	float x = max( field.x - waterShoreK0.x, 0.0 );
	float wave = 0.5 + 0.5 * cos( 6.2831853 * ( x / waterShoreK1.x + t / waterShoreK1.y ) );
	surf *= mix( waterShoreK1.z, 1.0, wave );
	// the lap line: run-up of about H on the bank; thinner than a pixel = fainter, not wider
	float lap = ( 1.0 - smoothstep( 0.0, hs + dF, dep ) ) * ( hs / ( hs + dF + 1e-4 ) )
		* smoothstep( 0.0, waterShoreK3.w, hs * kShoreM );	// ripples under K3.w (m) run up without foaming
	if ( waterShoreEdgeLaw == 1 )
		lap *= smoothstep( 0.0, 0.3 * ( hs + dF ), dep );	// off the contact itself: no white seam on the sand
	float D = max( surf * amount, lap * waterShoreK3.z );
	if ( D < 0.002 )
		return 0.0;
	vec3 det = wwFoamDetail( P, foot, t );
	float s = mix( 0.5, waterShoreK0.w, det.z );	// a soft threshold where the detail has faded: alpha ~ D on average
	float a = smoothstep( 1.0 - D - s, 1.0 - D + s, det.x );
	return a * mix( det.y, 1.0, D * D ) * min( 1.0, 1.5 * D );
}
