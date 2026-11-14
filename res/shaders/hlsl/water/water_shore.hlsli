// HLSL source of features/water/water_shore.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// near and far water. The plane has no land side (every dry texel reads 255), so a dry texel counts as distance 0:
	// the field, bilinear between texel centres, then crosses the waterline halfway from a dry to a wet centre.
	int waterShoreOn;	// 1 the soft edge (Water soft edge row), 2 the foam (Shore foam row)
	int waterShoreRed;	// 1 nearest: the field of the nearest texel only (the 128 u staircase)
	float4 waterShoreK0;	// field value at the waterline, edge width, foam width, foam softness (cellwater.cpp)
	float4 waterShoreK1;	// wave length, wave period (s), foam between the crests, foam opacity
	float waterShoreTime;	// the world clock (s)
	int waterShoreLaw;	// WATERFOAMDETAIL1: 1 the wave-energy foam (wwShoreFoamWave), 0 the old band (red old)
	float4 waterShoreK2;	// wind speed (m/s), sea swell height (m), river fetch cap (m), full-foam wave height (m)
	float4 waterShoreK3;	// fresh-water foam persistence, breaker index (H/d), lap line opacity, lap line onset height (m)
	int waterShoreEdgeLaw;	// WATERFOAMDETAIL1: 1 the soft contact (wwShoreEdgeSoft), 0 the baked band only (red old)
	float4 waterShoreK4;	// contact width (u) looking straight down, at a grazing view; the bake's error (u); 0
#else
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
// lane WATERSHORE1: the shore look of fo4_waterlook.prog from the baked shore plane (water_info.glsl), the same for


// one texel: (distance to the shore in world units, 1 wet / 0 dry or off the map)
float2 wwShoreTexelValue( uint w, bool inMap )
{
	if ( !inMap )
		return float2( 255.0 * waterInfoGrid.w, 0.0 );
	if ( ( w & 0xFFFFu ) == 0u )
		return (float2)( 0.0 );
	return float2( float( ( w >> 16 ) & 0xFFu ) * waterInfoGrid.w, 1.0 );
}

// (distance to the shore, wet texels of the 2 x 2 around the point); (0, 0) when the lookup is off
float2 wwShoreField( float2 worldXY )
{
	if ( !waterInfoOn )
		return (float2)( 0.0 );
	float2 g = ( worldXY - waterInfoGrid.xy ) / waterInfoGrid.z - 0.5;	// in texel centres
	int2 t0 = int2( floor( g ) );
	float2 f = g - float2( t0 );
	if ( ( waterShoreRed & 1 ) != 0 )
		f = step( (float2)( 0.5 ), f );
	int E = int( waterInfoLayout.z );
	int2 n = int2( waterInfoLayout.xy ) * E;
	uint w00, w10, w01, w11;
	bool i00 = true, i10 = true, i01 = true, i11 = true;
	int2 tile = t0 / E, in0 = t0 - tile * E;
	if ( t0.x >= 0 && t0.y >= 0 && t0.x + 1 < n.x && t0.y + 1 < n.y && in0.x < E - 1 && in0.y < E - 1 ) {
		// the 2 x 2 inside one tile (15 of 16 per axis): one index fetch, and the atlas only for a sparse tile
		uint2 ix = waterInfoIndex.Load( int3(tile, 0) ).rg;
		if ( ix.r == 0u ) {
			w00 = w10 = w01 = w11 = ix.g;
		} else {
			int slot = int( ix.r ) - 1, perRow = int( waterInfoLayout.w );
			int2 a = int2( slot % perRow, slot / perRow ) * E + in0;
			w00 = waterInfoAtlas.Load( int3(a, 0) ).r;
			w10 = waterInfoAtlas.Load( int3(a + int2( 1, 0 ), 0) ).r;
			w01 = waterInfoAtlas.Load( int3(a + int2( 0, 1 ), 0) ).r;
			w11 = waterInfoAtlas.Load( int3(a + int2( 1, 1 ), 0) ).r;
		}
	} else {
		float2 c = waterInfoGrid.xy + ( float2( t0 ) + 0.5 ) * waterInfoGrid.z;
		float z = waterInfoGrid.z;
		w00 = wwWaterInfoWord( c );
		w10 = wwWaterInfoWord( c + float2( z, 0.0 ) );
		w01 = wwWaterInfoWord( c + float2( 0.0, z ) );
		w11 = wwWaterInfoWord( c + float2( z, z ) );
		i00 = all( (t0 >= (int2)( 0 )) ) && all( (t0 < n) );
		i10 = all( (t0 + int2( 1, 0 ) >= (int2)( 0 )) ) && all( (t0 + int2( 1, 0 ) < n) );
		i01 = all( (t0 + int2( 0, 1 ) >= (int2)( 0 )) ) && all( (t0 + int2( 0, 1 ) < n) );
		i11 = all( (t0 + (int2)( 1 ) >= (int2)( 0 )) ) && all( (t0 + (int2)( 1 ) < n) );
	}
	float2 v00 = wwShoreTexelValue( w00, i00 ), v10 = wwShoreTexelValue( w10, i10 );
	float2 v01 = wwShoreTexelValue( w01, i01 ), v11 = wwShoreTexelValue( w11, i11 );
	float d = lerp( lerp( v00.x, v10.x, f.x ), lerp( v01.x, v11.x, f.x ), f.y );
	return float2( d, v00.y + v10.y + v01.y + v11.y );
}

float wwShoreSmooth( float x )
{
	return x * x * ( 3.0 - 2.0 * x );
}

// the soft edge: 1 at the waterline (the scene behind shows through), 0 an edge width in. No wet texel near the
// point = the bake does not know this water (placed or interior water): 0, the game's own edge stays
float wwShoreEdge( float2 field )
{
	if ( field.y < 0.5 )
		return 0.0;
	return 1.0 - wwShoreSmooth( clamp( ( field.x - waterShoreK0.x ) / waterShoreK0.y, 0.0, 1.0 ) );
}

/* lane WATERFOAMDETAIL1: the soft contact. 1 where the water meets land or any object, 0 a width in. The distance is
 * the shorter of the water path to the scene behind (along the view ray, from the depth buffer: banks, rocks, debris,
 * actors) and the baked distance past the waterline plus the bake's error (land the depth buffer does not hold). The
 * width grows from K4.x looking straight down to K4.y at a grazing view (|viewZ| = the view ray's vertical part) */
float wwShoreEdgeSoft( float2 field, float along, float viewZ )
{
	float d = along;
	if ( field.y > 0.5 )
		d = min( d, max( field.x - waterShoreK0.x, 0.0 ) + waterShoreK4.z );
	if ( d >= 1e8 )
		return 0.0;
	float w = lerp( waterShoreK4.y, waterShoreK4.x, clamp( abs( viewZ ), 0.0, 1.0 ) );
	return 1.0 - wwShoreSmooth( clamp( d / w, 0.0, 1.0 ) );
}

// integer hash (a lowbias32 mix), the same bits on any GPU and in the Python model
uint wwShoreHash( int2 c )
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
float wwShoreNoise( float2 p )
{
	int2 c = int2( floor( p ) );
	float2 f = p - float2( c );
	f = f * f * ( 3.0 - 2.0 * f );
	float a = float( wwShoreHash( c ) >> 8 ) * ( 1.0 / 16777215.0 );
	float b = float( wwShoreHash( c + int2( 1, 0 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	float e = float( wwShoreHash( c + int2( 0, 1 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	float g = float( wwShoreHash( c + int2( 1, 1 ) ) >> 8 ) * ( 1.0 / 16777215.0 );
	return lerp( lerp( a, b, f.x ), lerp( e, g, f.x ), f.y );
}

/* the foam: a band from the waterline out to the foam width (Sea of Thieves 2018, Far Cry 5 2018: foam from a
 * shore distance), crests travelling shoreward one wave length per period, broken up by two octaves of noise
 * (48 u and 17 u clumps); coverage over the noise threshold, softened by K0.w */
float wwShoreFoam( float2 field, float2 worldXY, float t )
{
	if ( field.y < 0.5 )
		return 0.0;
	float x = max( field.x - waterShoreK0.x, 0.0 );
	float env = 1.0 - wwShoreSmooth( clamp( x / waterShoreK0.z, 0.0, 1.0 ) );
	float wave = 0.5 + 0.5 * cos( 6.2831853 * ( x / waterShoreK1.x + t / waterShoreK1.y ) );
	float cover = env * lerp( waterShoreK1.z, 1.0, wave );
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
static const float kShoreM = 0.0142875;	// metres a game unit
float4 wwShoreWave( float2 P )
{
	if ( !waterInfoOn )
		return (float4)( 0.0 );
	float tex = waterInfoGrid.z;
	float2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	float2 t0 = floor( g );
	float2 f = g - t0;
	float2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uint4 w = uint4( wwWaterInfoWord( c0 ), wwWaterInfoWord( c0 + float2( tex, 0.0 ) ),
		wwWaterInfoWord( c0 + float2( 0.0, tex ) ), wwWaterInfoWord( c0 + float2( tex, tex ) ) );
	float4 bw = float4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uint4 id = w & 0xFFFFu;
	uint body = 0u;
	float best = -1.0;
	for ( int k = 0; k < 4; k++ )
		if ( id[k] != 0u && bw[k] > best ) {
			best = bw[k];
			body = id[k];
		}
	if ( body == 0u )
		return (float4)( 0.0 );
	float4 code = float4( w >> 24 ) / 255.0;
	float depth = dot( bw, float4( (id == (uint4)( body )) ) * code * code * 8192.0 );	// dry = 0
	float4 row = waterInfoBodies.Load( int3(int2( int( body % 256u ), int( body / 256u ) ), 0) );
	int cls = clamp( int( row.g + 0.5 ), 0, 2 );
	float len = sqrt( max( row.a, 1.0 ) ) * tex * kShoreM;
	float fetch = cls == 1 ? min( len, waterShoreK2.z ) : len;
	float U = waterShoreK2.x;
	float hw = min( 0.0016 * U * sqrt( fetch / 9.81 ), 0.21 * U * U / 9.81 );
	float hs = cls == 0 ? sqrt( hw * hw + waterShoreK2.y * waterShoreK2.y ) : hw;
	return float4( hs / kShoreM, cls == 0 ? 1.0 : waterShoreK3.x, depth, 1.0 );
}

float wwFoamHash01( int2 c )
{
	return float( wwShoreHash( c ) >> 8 ) * ( 1.0 / 16777215.0 );
}

// value noise on a lattice of k cells that repeats every k (the world wrapped by 2048 u, so float32 keeps its detail)
float wwFoamNoise( float2 p, int k, int2 seed )
{
	float2 fl = floor( p );
	float2 f = p - fl;
	f = f * f * ( 3.0 - 2.0 * f );
	int2 c = int2( fl ) % k, c1 = ( c + 1 ) % k;	// p is in [0, k]: no negative %
	float a = wwFoamHash01( c + seed ), b = wwFoamHash01( int2( c1.x, c.y ) + seed );
	float e = wwFoamHash01( int2( c.x, c1.y ) + seed ), h = wwFoamHash01( c1 + seed );
	return lerp( lerp( a, b, f.x ), lerp( e, h, f.x ), f.y );
}

// the bubble lace: cellular (Worley) F2 - F1 on k cells, 0 on the bubble walls
float wwFoamLace( float2 p, int k )
{
	float2 c = floor( p );
	float f1 = 8.0, f2 = 8.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ ) {
			int2 cc = int2( c ) + int2( x, y );
			uint h = wwShoreHash( ( cc + k ) % k + int2( 331, 977 ) );	// cc >= -1
			float2 q = float2( cc ) + float2( float( h & 0xFFFFu ), float( h >> 16 ) ) * ( 1.0 / 65535.0 );
			float d = length( p - q );
			f2 = d < f1 ? f1 : min( f2, d );
			f1 = min( f1, d );
		}
	return f2 - f1;
}

/* the foam's texture: five octaves of value noise (24, 8, 2.7, 0.9, 0.3 u) and a 1.2 u bubble lace; each layer fades
 * to its mean once a pixel covers more than half of it (no shimmer far away, full detail close up).
 * Returns (noise 0..1, lace alpha 0..1, the share of the noise's detail still drawn) */
float3 wwFoamDetail( float2 P, float foot, float t )
{
	float2 Pw = P - 2048.0 * floor( P * ( 1.0 / 2048.0 ) );
	const int K[5] = { 85, 256, 758, 2276, 6827 };
	const float Wt[5] = { 0.36, 0.26, 0.18, 0.12, 0.08 };
	float n = 0.0, drawn = 0.0;
	for ( int i = 0; i < 5; i++ ) {
		float s = 2048.0 / float( K[i] );
		float vis = 1.0 - smoothstep( 0.35, 0.7, foot / s );
		float2 p = Pw / s + t * float2( 0.11, 0.07 ) * float( i );	// the finer layers drift slowly (bubbles turning over)
		p = mod( p, float( K[i] ) );
		float v = vis > 0.0 ? wwFoamNoise( p, K[i], int2( 17 * i, 101 * i ) ) : 0.5;
		n += Wt[i] * lerp( 0.5, v, vis );
		drawn += Wt[i] * vis;
	}
	n = clamp( 0.5 + ( n - 0.5 ) * 2.2, 0.0, 1.0 );
	float lvis = 1.0 - smoothstep( 0.35, 0.7, foot / 1.2 );
	float lace = 0.45;
	if ( lvis > 0.0 ) {
		float2 p = mod( Pw / 1.2 + t * float2( 0.05, -0.04 ), 1707.0 );
		float wall = 1.0 - smoothstep( 0.04, 0.30, wwFoamLace( p, 1707 ) );
		lace = lerp( 0.45, 0.15 + 0.85 * wall, lvis );
	}
	return float3( n, lace, drawn );
}

/* the foam alpha (before the opacity K1.w) at P: field = wwShoreField, column = the water above the scene behind
 * (vertical, u; >= 1e8 when unknown: the baked depth then), colFoot = its change across the pixel, foot = world units
 * a pixel. dbg = (Hs / 64 u, energy amount, the column used / 64 u) for probe 38 */
float wwShoreFoamWave( float2 field, float2 P, float t, float column, float colFoot, float foot, out float3 dbg )
{
	dbg = (float3)( 0.0 );
	if ( field.y < 0.5 )
		return 0.0;
	float4 wv = wwShoreWave( P );
	if ( wv.w < 0.5 )
		return 0.0;
	float hs = max( wv.x, 1e-3 );	// no wind on a river: no foam, and no empty smoothstep
	bool scene = column < 1e8;
	float dep = scene ? column : wv.z;
	float dF = scene ? colFoot : 0.0;
	float amount = clamp( pow( hs * kShoreM / waterShoreK2.w, 2.5 ) * wv.y, 0.0, 1.0 );
	dbg = float3( hs / 64.0, amount, dep / 64.0 );
	// the surf: breaking where the column is under H / gamma, crests running in as before
	float db = hs / waterShoreK3.y;
	float surf = 1.0 - smoothstep( 0.6 * db, 1.2 * db + dF, dep );
	float x = max( field.x - waterShoreK0.x, 0.0 );
	float wave = 0.5 + 0.5 * cos( 6.2831853 * ( x / waterShoreK1.x + t / waterShoreK1.y ) );
	surf *= lerp( waterShoreK1.z, 1.0, wave );
	// the lap line: run-up of about H on the bank; thinner than a pixel = fainter, not wider
	float lap = ( 1.0 - smoothstep( 0.0, hs + dF, dep ) ) * ( hs / ( hs + dF + 1e-4 ) )
		* smoothstep( 0.0, waterShoreK3.w, hs * kShoreM );	// ripples under K3.w (m) run up without foaming
	if ( waterShoreEdgeLaw == 1 )
		lap *= smoothstep( 0.0, 0.3 * ( hs + dF ), dep );	// off the contact itself: no white seam on the sand
	float D = max( surf * amount, lap * waterShoreK3.z );
	if ( D < 0.002 )
		return 0.0;
	float3 det = wwFoamDetail( P, foot, t );
	float s = lerp( 0.5, waterShoreK0.w, det.z );	// a soft threshold where the detail has faded: alpha ~ D on average
	float a = smoothstep( 1.0 - D - s, 1.0 - D + s, det.x );
	return a * lerp( det.y, 1.0, D * D ) * min( 1.0, 1.5 * D );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_shore.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwFoamDetail
#ifdef WW_FN_ALL
static const float kShoreM = 0.0142875;
#endif

#if defined(WW_FN_wwShoreTexelValue) || defined(WW_FN_ALL)
float2 wwShoreTexelValue(uint w, bool inMap)
{
    if (!inMap)
    {
        return float2(255.0f * waterInfoGrid.w, 0.0f);
    }
    if ((w & 65535u) == 0u)
    {
        return 0.0f.xx;
    }
    return float2(float((w >> uint(16)) & 255u) * waterInfoGrid.w, 1.0f);
}
#endif

#if defined(WW_FN_wwShoreField) || defined(WW_FN_ALL)
float2 wwShoreField(float2 worldXY)
{
    if (!waterInfoOn)
    {
        return 0.0f.xx;
    }
    // in texel centres
    float2 g = ((worldXY - waterInfoGrid.xy) / waterInfoGrid.z.xx) - 0.5f.xx;
    int2 t0 = int2(floor(g));
    float2 f = g - float2(t0);
    if ((waterShoreRed & 1) != 0)
    {
        f = step(0.5f.xx, f);
    }
    int E = int(waterInfoLayout.z);
    int2 n = int2(waterInfoLayout.xy) * E.xx;
    bool i00 = true;
    bool i10 = true;
    bool i01 = true;
    bool i11 = true;
    int2 tile = t0 / E.xx;
    int2 in0 = t0 - (tile * E.xx);
    bool cond10 = t0.x >= 0;
    bool cond9;
    if (cond10)
    {
        cond9 = t0.y >= 0;
    }
    else
    {
        cond9 = cond10;
    }
    bool cond8;
    if (cond9)
    {
        cond8 = (t0.x + 1) < n.x;
    }
    else
    {
        cond8 = cond9;
    }
    bool cond7;
    if (cond8)
    {
        cond7 = (t0.y + 1) < n.y;
    }
    else
    {
        cond7 = cond8;
    }
    bool cond6;
    if (cond7)
    {
        cond6 = in0.x < (E - 1);
    }
    else
    {
        cond6 = cond7;
    }
    bool cond5;
    if (cond6)
    {
        cond5 = in0.y < (E - 1);
    }
    else
    {
        cond5 = cond6;
    }
    uint w00;
    uint w10;
    uint w01;
    uint w11;
    if (cond5)
    {
        // the 2 x 2 inside one tile (15 of 16 per axis): one index fetch, and the atlas only for a sparse tile
        uint2 ix = waterInfoIndex.Load(int3(tile, 0)).xy;
        if (ix.x == 0u)
        {
            w11 = ix.y;
            w01 = ix.y;
            w10 = ix.y;
            w00 = ix.y;
        }
        else
        {
            int slot = int(ix.x) - 1;
            int perRow = int(waterInfoLayout.w);
            int2 a = (int2(slot % perRow, slot / perRow) * E.xx) + in0;
            w00 = waterInfoAtlas.Load(int3(a, 0)).x;
            w10 = waterInfoAtlas.Load(int3(a + int2(1, 0), 0)).x;
            w01 = waterInfoAtlas.Load(int3(a + int2(0, 1), 0)).x;
            w11 = waterInfoAtlas.Load(int3(a + int2(1, 1), 0)).x;
        }
    }
    else
    {
        float2 c = waterInfoGrid.xy + ((float2(t0) + 0.5f.xx) * waterInfoGrid.z);
        float z = waterInfoGrid.z;
        float2 w00Part = c;
        w00 = wwWaterInfoWord(w00Part);
        float2 w10Part = c + float2(z, 0.0f);
        w10 = wwWaterInfoWord(w10Part);
        float2 w01Part = c + float2(0.0f, z);
        w01 = wwWaterInfoWord(w01Part);
        float2 w11Part = c + z.xx;
        w11 = wwWaterInfoWord(w11Part);
        bool cond4 = all(bool2(t0.x >= int2(0, 0).x, t0.y >= int2(0, 0).y));
        bool i00Pick;
        if (cond4)
        {
            i00Pick = all(bool2(t0.x < n.x, t0.y < n.y));
        }
        else
        {
            i00Pick = cond4;
        }
        i00 = i00Pick;
        int2 condPart3 = t0 + int2(1, 0);
        bool cond3 = all(bool2(condPart3.x >= int2(0, 0).x, condPart3.y >= int2(0, 0).y));
        bool i10Pick;
        if (cond3)
        {
            int2 i10Part = t0 + int2(1, 0);
            i10Pick = all(bool2(i10Part.x < n.x, i10Part.y < n.y));
        }
        else
        {
            i10Pick = cond3;
        }
        i10 = i10Pick;
        int2 condPart2 = t0 + int2(0, 1);
        bool cond2 = all(bool2(condPart2.x >= int2(0, 0).x, condPart2.y >= int2(0, 0).y));
        bool i01Pick;
        if (cond2)
        {
            int2 i01Part = t0 + int2(0, 1);
            i01Pick = all(bool2(i01Part.x < n.x, i01Part.y < n.y));
        }
        else
        {
            i01Pick = cond2;
        }
        i01 = i01Pick;
        int2 condPart = t0 + int2(1, 1);
        bool cond = all(bool2(condPart.x >= int2(0, 0).x, condPart.y >= int2(0, 0).y));
        bool i11Pick;
        if (cond)
        {
            int2 i11Part = t0 + int2(1, 1);
            i11Pick = all(bool2(i11Part.x < n.x, i11Part.y < n.y));
        }
        else
        {
            i11Pick = cond;
        }
        i11 = i11Pick;
    }
    uint v00Part2 = w00;
    bool v00Part = i00;
    float2 v00 = wwShoreTexelValue(v00Part2, v00Part);
    uint v10Part2 = w10;
    bool v10Part = i10;
    float2 v10 = wwShoreTexelValue(v10Part2, v10Part);
    uint v01Part2 = w01;
    bool v01Part = i01;
    float2 v01 = wwShoreTexelValue(v01Part2, v01Part);
    uint v11Part2 = w11;
    bool v11Part = i11;
    float2 v11 = wwShoreTexelValue(v11Part2, v11Part);
    float d = lerp(lerp(v00.x, v10.x, f.x), lerp(v01.x, v11.x, f.x), f.y);
    return float2(d, ((v00.y + v10.y) + v01.y) + v11.y);
}
#endif

#if defined(WW_FN_wwShoreSmooth) || defined(WW_FN_ALL)
float wwShoreSmooth(float x)
{
    return (x * x) * (3.0f - (2.0f * x));
}
#endif

#if defined(WW_FN_wwShoreEdge) || defined(WW_FN_ALL)
float wwShoreEdge(float2 field)
{
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float cond10 = clamp((field.x - waterShoreK0.x) / waterShoreK0.y, 0.0f, 1.0f);
    return 1.0f - wwShoreSmooth(cond10);
}
#endif

#if defined(WW_FN_wwShoreEdgeSoft) || defined(WW_FN_ALL)
float wwShoreEdgeSoft(float2 field, float along, float viewZ)
{
    float d = along;
    if (field.y > 0.5f)
    {
        d = min(d, max(field.x - waterShoreK0.x, 0.0f) + waterShoreK4.z);
    }
    if (d >= 100000000.0f)
    {
        return 0.0f;
    }
    float w = lerp(waterShoreK4.y, waterShoreK4.x, clamp(abs(viewZ), 0.0f, 1.0f));
    float cond10 = clamp(d / w, 0.0f, 1.0f);
    return 1.0f - wwShoreSmooth(cond10);
}
#endif

#if defined(WW_FN_wwShoreHash) || defined(WW_FN_ALL)
uint wwShoreHash(int2 c)
{
    uint h = (uint(c.x) * 2376512323u) ^ (uint(c.y) * 3625334849u);
    h ^= (h >> uint(16));
    h *= 2146121005u;
    h ^= (h >> uint(15));
    h *= 2221713035u;
    h ^= (h >> uint(16));
    return h;
}
#endif

#if defined(WW_FN_wwShoreNoise) || defined(WW_FN_ALL)
float wwShoreNoise(float2 p)
{
    int2 c = int2(floor(p));
    float2 f = p - float2(c);
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 cond10 = c;
    float a = float(wwShoreHash(cond10) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 cond9 = c + int2(1, 0);
    float b = float(wwShoreHash(cond9) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 cond8 = c + int2(0, 1);
    float e = float(wwShoreHash(cond8) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    int2 cond7 = c + int2(1, 1);
    float g = float(wwShoreHash(cond7) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
    return lerp(lerp(a, b, f.x), lerp(e, g, f.x), f.y);
}
#endif

#if defined(WW_FN_wwShoreFoam) || defined(WW_FN_ALL)
float wwShoreFoam(float2 field, float2 worldXY, float t)
{
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float x = max(field.x - waterShoreK0.x, 0.0f);
    float cond10 = clamp(x / waterShoreK0.z, 0.0f, 1.0f);
    float env = 1.0f - wwShoreSmooth(cond10);
    float wave = 0.5f + (0.5f * cos(6.283185482025146484375f * ((x / waterShoreK1.x) + (t / waterShoreK1.y))));
    float cover = env * lerp(waterShoreK1.z, 1.0f, wave);
    float2 cond9 = worldXY * 0.02083333395421504974365234375f;
    float2 cond8 = (worldXY * 0.0588235296308994293212890625f) + 0.37000000476837158203125f.xx;
    float n = (0.64999997615814208984375f * wwShoreNoise(cond9)) + (0.3499999940395355224609375f * wwShoreNoise(cond8));
    return clamp((n - (1.0f - cover)) / waterShoreK0.w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwShoreWave) || defined(WW_FN_ALL)
float4 wwShoreWave(float2 P)
{
    if (!waterInfoOn)
    {
        return 0.0f.xxxx;
    }
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 cond10 = c0;
    float2 cond9 = c0 + float2(tex, 0.0f);
    float2 cond8 = c0 + float2(0.0f, tex);
    float2 cond7 = c0 + tex.xx;
    uint4 w = uint4(wwWaterInfoWord(cond10), wwWaterInfoWord(cond9), wwWaterInfoWord(cond8), wwWaterInfoWord(cond7));
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = w & uint4(65535u, 65535u, 65535u, 65535u);
    uint body = 0u;
    float best = -1.0f;
    for (int k = 0; k < 4; k++)
    {
        bool cond6 = id[k] != 0u;
        bool cond5;
        if (cond6)
        {
            cond5 = bw[k] > best;
        }
        else
        {
            cond5 = cond6;
        }
        if (cond5)
        {
            best = bw[k];
            body = id[k];
        }
    }
    if (body == 0u)
    {
        return 0.0f.xxxx;
    }
    float4 code = float4(w >> uint4(int4(24, 24, 24, 24))) / 255.0f.xxxx;
    uint4 w00Part = body.xxxx;
    // dry = 0
    float depth = dot(bw, ((float4(bool4(id.x == w00Part.x, id.y == w00Part.y, id.z == w00Part.z, id.w == w00Part.w)) * code) * code) * 8192.0f);
    float4 row = waterInfoBodies.Load(int3(int2(int(body % 256u), int(body / 256u)), 0));
    int cls = clamp(int(row.y + 0.5f), 0, 2);
    float len = (sqrt(max(row.w, 1.0f)) * tex) * 0.01428749971091747283935546875f;
    float w10Part;
    if (cls == 1)
    {
        w10Part = min(len, waterShoreK2.z);
    }
    else
    {
        w10Part = len;
    }
    float fetch = w10Part;
    float U = waterShoreK2.x;
    float hw = min((0.001599999959580600261688232421875f * U) * sqrt(fetch / 9.81000041961669921875f), ((0.20999999344348907470703125f * U) * U) / 9.81000041961669921875f);
    float w01Part;
    if (cls == 0)
    {
        w01Part = sqrt((hw * hw) + (waterShoreK2.y * waterShoreK2.y));
    }
    else
    {
        w01Part = hw;
    }
    float hs = w01Part;
    float w11Part;
    if (cls == 0)
    {
        w11Part = 1.0f;
    }
    else
    {
        w11Part = waterShoreK3.x;
    }
    return float4(hs / 0.01428749971091747283935546875f, w11Part, depth, 1.0f);
}
#endif

#if defined(WW_FN_wwFoamHash01) || defined(WW_FN_ALL)
float wwFoamHash01(int2 c)
{
    int2 cond10 = c;
    return float(wwShoreHash(cond10) >> uint(8)) * 5.9604651880817982601001858711243e-08f;
}
#endif

#if defined(WW_FN_wwFoamNoise) || defined(WW_FN_ALL)
float wwFoamNoise(float2 p, int k, int2 seed)
{
    float2 fl = floor(p);
    float2 f = p - fl;
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    // p is in [0, k]: no negative %
    int2 c = int2(fl) % k.xx;
    int2 c1 = (c + int2(1, 1)) % k.xx;
    int2 cond10 = c + seed;
    float a = wwFoamHash01(cond10);
    int2 cond9 = int2(c1.x, c.y) + seed;
    float b = wwFoamHash01(cond9);
    int2 cond8 = int2(c.x, c1.y) + seed;
    float e = wwFoamHash01(cond8);
    int2 cond7 = c1 + seed;
    float h = wwFoamHash01(cond7);
    return lerp(lerp(a, b, f.x), lerp(e, h, f.x), f.y);
}
#endif

#if defined(WW_FN_wwFoamLace) || defined(WW_FN_ALL)
float wwFoamLace(float2 p, int k)
{
    float2 c = floor(p);
    float f1 = 8.0f;
    float f2 = 8.0f;
    float cond10;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int2 cc = int2(c) + int2(x, y);
            int2 cond9 = ((cc + k.xx) % k.xx) + int2(331, 977);
            // cc >= -1
            uint h = wwShoreHash(cond9);
            float2 q = float2(cc) + (float2(float(h & 65535u), float(h >> uint(16))) * 1.525902189314365386962890625e-05f);
            float d = length(p - q);
            if (d < f1)
            {
                cond10 = f1;
            }
            else
            {
                cond10 = min(f2, d);
            }
            f2 = cond10;
            f1 = min(f1, d);
        }
    }
    return f2 - f1;
}
#endif

#if defined(WW_FN_wwShoreFoamWave) || defined(WW_FN_ALL)
float wwShoreFoamWave(float2 field, float2 P, float t, float column, float colFoot, float foot, out float3 dbg)
{
    dbg = 0.0f.xxx;
    if (field.y < 0.5f)
    {
        return 0.0f;
    }
    float2 cond10 = P;
    float4 wv = wwShoreWave(cond10);
    if (wv.w < 0.5f)
    {
        return 0.0f;
    }
    // no wind on a river: no foam, and no empty smoothstep
    float hs = max(wv.x, 0.001000000047497451305389404296875f);
    bool scene = column < 100000000.0f;
    float cond9;
    if (scene)
    {
        cond9 = column;
    }
    else
    {
        cond9 = wv.z;
    }
    float dep = cond9;
    float dF = scene ? colFoot : 0.0f;
    float amount = clamp(pow((hs * 0.01428749971091747283935546875f) / waterShoreK2.w, 2.5f) * wv.y, 0.0f, 1.0f);
    dbg = float3(hs / 64.0f, amount, dep / 64.0f);
    // the surf: breaking where the column is under H / gamma, crests running in as before
    float db = hs / waterShoreK3.y;
    float surf = 1.0f - smoothstep(0.60000002384185791015625f * db, (1.2000000476837158203125f * db) + dF, dep);
    float x = max(field.x - waterShoreK0.x, 0.0f);
    float wave = 0.5f + (0.5f * cos(6.283185482025146484375f * ((x / waterShoreK1.x) + (t / waterShoreK1.y))));
    surf *= lerp(waterShoreK1.z, 1.0f, wave);
    // the lap line: run-up of about H on the bank; thinner than a pixel = fainter, not wider
    float lap = ((1.0f - smoothstep(0.0f, hs + dF, dep)) * (hs / ((hs + dF) + 9.9999997473787516355514526367188e-05f))) * smoothstep(0.0f, waterShoreK3.w, hs * 0.01428749971091747283935546875f);
    if (waterShoreEdgeLaw == 1)
    {
        // off the contact itself: no white seam on the sand
        lap *= smoothstep(0.0f, 0.300000011920928955078125f * (hs + dF), dep);
    }
    float D_1 = max(surf * amount, lap * waterShoreK3.z);
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return 0.0f;
    }
    float2 cond8 = P;
    float cond7 = foot;
    float cond6 = t;
    float3 det = wwFoamDetail(cond8, cond7, cond6);
    // a soft threshold where the detail has faded: alpha ~ D on average
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return (a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1);
}
#endif
#endif	// WW_HLSL_SRC
