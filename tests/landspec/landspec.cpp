// lanes LANDSPEC1/2: src/landlodspec.h against hand-computed values, and the shaders' literals against the header.
// g++ -std=c++17 -I<tree>/src tests/landspec/landspec.cpp; run with <tree> as argv[1]. Reds, one per goal:
// -DLANDSPEC_RED (the law), -DLANDSPEC_RED_GLIDE (snap), -DLANDSPEC_RED_BAND (no band textures),
// -DLANDSPEC_RED_SSR (dry land flagged); the shader reds run a green build against the base's shaders.
#include "landlodspec.h"

#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#ifdef LANDSPEC_RED	// the viewer before LANDSPEC1: the ground keeps its own gloss/spec at every distance
namespace LandLodSpec
{
inline GlossSpec landRed( GlossSpec own, Mode, float, float, float = kScale ) { return own; }
}
#define LAND LandLodSpec::landRed
#else
#define LAND LandLodSpec::land
#endif

#ifdef LANDSPEC_RED_GLIDE	// the centre snaps to the new cell at once
#define GLIDETO( g, x, y, t ) LandLodSpec::glideTo( g, x, y, t, true )
#else
#define GLIDETO( g, x, y, t ) LandLodSpec::glideTo( g, x, y, t, false )
#endif

#ifdef LANDSPEC_RED_BAND	// the band leaves albedo and normal alone
#define NOISEK( n ) ( (void)( n ), 1.0f )
#define BANDNORMAL( n, d, v ) ( (void)( d ), (void)( v ), ( n ) )
#else
#define NOISEK( n ) LandLodSpec::noiseK( n )
#define BANDNORMAL( n, d, v ) LandLodSpec::bandNormal( n, d, v )
#endif

#ifdef LANDSPEC_RED_SSR	// the material's flag kept on dry land
#define LANDSSR( w ) ( (void)( w ), true )
#else
#define LANDSSR( w ) LandLodSpec::landSsr( w )
#endif

// lane LANDSPEC3 reds: -DLANDSPEC_RED_WIN (LANDSPEC2's 2 x 2 window), _PBR (rough = 1 - gloss), _LOD (bit 0 read)
#ifdef LANDSPEC_RED_WIN
#define WINFIRST( c ) ( int( std::floor( float( ( c ) - 2 ) / 4.0f ) ) * 4 )
#define WINLAYER( cx, cy, fx, fy ) ( ( ( cx ) - ( fx ) ) >= 0 && ( ( cx ) - ( fx ) ) < 8 && ( ( cy ) - ( fy ) ) >= 0 \
	&& ( ( cy ) - ( fy ) ) < 8 ? 0 : -1 )
#else
#define WINFIRST( c ) LandLodSpec::windowFirst( c )
#define WINLAYER( cx, cy, fx, fy ) LandLodSpec::windowLayer( cx, cy, fx, fy )
#endif
#ifdef LANDSPEC_RED_PBR
#define PBRROUGH( g ) ( 1.0f - ( g ) )
#else
#define PBRROUGH( g ) LandLodSpec::pbrRough( g )
#endif
#ifdef LANDSPEC_RED_LOD
#define PARENTLOD( w, p ) ( ( w ) != 0 && ( ( p ) & 1u ) != 0 )
#else
#define PARENTLOD( w, p ) LandLodSpec::usesParentLod( w, p )
#endif

// lane LANDSPEC4 reds: -DLANDSPEC_RED_BLOCK (the camera's 4 x 4 only), _SPLAT (each layer pass its own detail frame)
#ifdef LANDSPEC_RED_BLOCK
#define WINDOWFOR( cx, cy, b, x0, y0, x1, y1 ) LandLodSpec::windowFor( cx, cy, false, x0, y0, x1, y1 )
#else
#define WINDOWFOR( cx, cy, b, x0, y0, x1, y1 ) LandLodSpec::windowFor( cx, cy, b, x0, y0, x1, y1 )
#endif
// lane LANDSPEC5 reds: -DLANDSPEC_RED_CLIMB (one parent step: WW_LANDSPEC_RED=climb1)
#ifdef LANDSPEC_RED_CLIMB
#define CLIMB( w, p, s ) LandLodSpec::lodClimb( w, p, s, true )
#else
#define CLIMB( w, p, s ) LandLodSpec::lodClimb( w, p, s )
#endif
// -DLANDSPEC_RED_PAGE (LANDSPEC4's cap 8, no paging: WW_LANDSPEC_RED=win8)
#ifdef LANDSPEC_RED_PAGE
#define WINDOWWIDE( cx, cy, x0, y0, x1, y1 ) LandLodSpec::windowFor( cx, cy, true, x0, y0, x1, y1, LandLodSpec::kWindowMax4, false )
#else
#define WINDOWWIDE( cx, cy, x0, y0, x1, y1 ) LandLodSpec::windowFor( cx, cy, true, x0, y0, x1, y1 )
#endif
//! the per-layer pass before LANDSPEC4: bandNormal's target in the layer normal's own frame, unnormalized
static LandLodSpec::V3 perLayerRaw( LandLodSpec::V3 n, LandLodSpec::V3 d, float v )
{
	using namespace LandLodSpec;
	const V3 t1 = norm3( cross3( n, cross3( n, V3{ 1.0f, 0.0f, 0.0f } ) ) );
	const V3 t2 = norm3( cross3( n, t1 ) );
	const V3 tg{ dot3( t1, d ), dot3( t2, d ), dot3( norm3( n ), d ) };
	return { mixf( n.x, tg.x, v ), mixf( n.y, tg.y, v ), mixf( n.z, tg.z, v ) };
}
#ifdef LANDSPEC_RED_SPLAT
#define SPLATRAW( n, d, v ) perLayerRaw( n, d, v )
#else
#define SPLATRAW( n, d, v ) LandLodSpec::splatNormalRaw( n, d, v )
#endif

bool wwLandLodWantsRepaint()
{
	return false;
}

static int fails = 0;
static void near( const char * what, float got, float want, float tol = 1e-5f )
{
	const bool ok = std::fabs( got - want ) <= tol;
	std::printf( "%s %s: %.7g want %.7g\n", ok ? "ok" : "FAIL", what, got, want );
	fails += ok ? 0 : 1;
}

static void must( const std::string & tree, const char * file, std::initializer_list<const char *> lits )
{
	std::ifstream f( tree + "/res/shaders/" + file );
	std::stringstream ss;
	ss << f.rdbuf();
	const std::string s = ss.str();
	for ( const char * m : lits ) {
		const bool ok = s.find( m ) != std::string::npos;
		std::printf( "%s %s has: %s\n", ok ? "ok" : "FAIL", file, m );
		fails += ok ? 0 : 1;
	}
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using namespace LandLodSpec;
	// hand values: blend 0 up to 6963.2, 1 from 9625.6; at 8294.4 (mid) 0.5; at 9000 (9625.6-9000)/2662.4 = 0.234976 -> 0.765024
	near( "blend 0", blend( 0.0f ), 0.0f );
	near( "blend 6963.2", blend( 6963.2f ), 0.0f, 1e-4f );
	near( "blend 8294.4", blend( 8294.4f ), 0.5f, 1e-4f );
	near( "blend 9000", blend( 9000.0f ), 0.765024f, 1e-4f );
	near( "blend 20000", blend( 20000.0f ), 1.0f );
	near( "cell centre 100", cellCentre( 100.0f ), 0.0f );
	near( "cell centre 2049", cellCentre( 2049.0f ), 4096.0f );
	near( "cell centre -2049", cellCentre( -2049.0f ), -4096.0f );
	// a sand texel: _s gloss 0.129, mask 0.5 (material scalars 1)
	const GlossSpec sand{ 0.129f, 0.5f };
	GlossSpec g = LAND( sand, Near, 3000.0f, 0.0f );	// inside 1.7 cells: untouched
	near( "near 3000 gloss", g.gloss, 0.129f );
	near( "near 3000 spec", g.spec, 0.5f );
	g = LAND( sand, Near, 6000.0f, 6000.0f );	// d 8485.28: v = 1 - (9625.6-8485.28)/2662.4 = 0.571697
	near( "near 8485 gloss", g.gloss, 0.129f + ( 0.22f - 0.129f ) * 0.571697f, 1e-4f );
	near( "near 8485 spec", g.spec, 0.5f + ( 0.06f - 0.5f ) * 0.571697f, 1e-4f );
	g = LAND( sand, Near, 12000.0f, 0.0f );
	near( "near 12000 gloss", g.gloss, 0.22f );
	near( "near 12000 spec", g.spec, 0.06f );
	g = LAND( sand, Far, 0.0f, 0.0f );
	near( "far gloss", g.gloss, 0.22f );
	near( "far spec", g.spec, 0.06f );
	g = LAND( sand, Off, 50000.0f, 0.0f );
	near( "off gloss", g.gloss, 0.129f );
	// wet: gloss 0.22 spec 0.06 at wet 0.5: spec' = 0.06*0.85+0.15 = 0.201; gloss 1.875*0.22 = 0.4125; spec 0.75*0.201 = 0.15075
	GlossSpec w = wetLand( GlossSpec{ 0.22f, 0.06f }, 0.5f );
	near( "wet 0.5 gloss", w.gloss, 0.4125f );
	near( "wet 0.5 spec", w.spec, 0.15075f );
	w = wetLand( GlossSpec{ 0.22f, 0.06f }, 0.0f );
	near( "wet 0 gloss", w.gloss, 0.22f );
	near( "wet 0 spec", w.spec, 0.06f );
	w = wetLand( GlossSpec{ 0.3f, 0.2f }, 1.0f );	// spec' 0.2*0.7+0.3 = 0.44 -> x0.5 = 0.22; gloss x2.75 = 0.825
	near( "wet 1 gloss", w.gloss, 0.825f );
	near( "wet 1 spec", w.spec, 0.22f );
	near( "wet albedo 1", wetAlbedoK( 1.0f ), 0.65f );
	near( "wet clock rain 10 s", wetStep( 0.0f, true, 10.0f ), 0.3f );
	near( "wet clock dry 10 s", wetStep( 1.0f, false, 10.0f ), 0.8f );

	// goal 3, the glide: first frame snaps; a one-cell cross at t 10 is half way at 12.5, whole at 15
	Glide gl;
	float x = 0.0f, y = 0.0f;
	GLIDETO( gl, 0.0f, 0.0f, 0.0 );
	glideAt( gl, 0.0, x, y );
	near( "glide first frame", x, 0.0f );
	GLIDETO( gl, 4096.0f, 0.0f, 10.0 );
	glideAt( gl, 12.5, x, y );
	near( "glide 2.5 s of 5", x, 2048.0f );
	near( "glide gliding at 2.5 s", float( gliding( gl, 12.5 ) ), 1.0f );
	GLIDETO( gl, 8192.0f, 0.0f, 12.5 );	// a second cross mid-glide starts from where the centre stands (2048)
	glideAt( gl, 15.0, x, y );
	near( "glide re-cross 2.5 s", x, 5120.0f );
	glideAt( gl, 17.5, x, y );
	near( "glide re-cross whole", x, 8192.0f );
	near( "glide done", float( gliding( gl, 17.5 ) ), 0.0f );
	GLIDETO( gl, 40000.0f, 0.0f, 20.0 );	// a jump of more than one cell snaps (the game: a shift > 1 cell)
	glideAt( gl, 20.0, x, y );
	near( "glide jump snaps", x, 40000.0f );

	// goal 2, the band: window, uv, noise, detail normal
	// LANDSPEC3: the 4 x 4 window holds every cell within 6 of the camera's (a 13 x 13 near block)
	near( "window cam 0", float( WINFIRST( 0 ) ), -8.0f );
	near( "window cam 5", float( WINFIRST( 5 ) ), -4.0f );
	near( "window cam -3", float( WINFIRST( -3 ) ), -12.0f );
	int holes = 0;
	for ( int c = -9; c <= 9; c++ )
		for ( int dx = -kReach; dx <= kReach; dx++ )
			for ( int dy = -kReach; dy <= kReach; dy++ )
				holes += WINLAYER( c + dx, c + dy, WINFIRST( c ), WINFIRST( c ) ) < 0 ? 1 : 0;
	near( "window holes within 6 cells", float( holes ), 0.0f );
	near( "window layer x + 4 y", float( WINLAYER( 5, 9, 0, 0 ) ), 9.0f );
	near( "window layer outside", float( WINLAYER( 16, 0, 0, 0 ) ), -1.0f );
	// LANDSPEC4: the window also holds the whole near block, the camera anywhere in it (blocks 7 x 7 .. 21 x 21)
	int blockHoles = 0, blockCells = 0;
	for ( int half = 3; half <= 10; half++ )
		for ( int camX = -half; camX <= half; camX++ )
			for ( int camY = -half; camY <= half; camY++ ) {
				const Window w = WINDOWFOR( camX + 3, camY - 2, true, 3 - half, -2 - half, 3 + half, -2 + half );
				for ( int x = -half; x <= half; x++ )
					for ( int y = -half; y <= half; y++ ) {
						blockCells++;
						blockHoles += windowLayer( x + 3, y - 2, w.fx, w.fy, w.w ) < 0 ? 1 : 0;
					}
			}
	std::printf( "window block cells checked %d\n", blockCells );
	near( "window holes in blocks to 21 x 21", float( blockHoles ), 0.0f );
	const Window w5 = WINDOWFOR( 0, 0, true, -2, -2, 2, 2 );	// a 5 x 5 round the camera: the LANDSPEC3 window
	near( "window 5 x 5 fx", float( w5.fx ), float( windowFirst( 0 ) ) );
	near( "window 5 x 5 w", float( w5.w ), 4.0f );
	const Window w21 = WINDOWFOR( 10, 10, true, -10, -10, 10, 10 );	// cam at a corner: chunks -12..16 = 8 a side
	near( "window 21 x 21 corner w", float( w21.w ), 8.0f );
	near( "window 21 x 21 corner fx", float( w21.fx ), -12.0f );
	const Window wBig = windowFor( 0, 0, true, -20, -20, 20, 20, kWindowMax4, false );	// LANDSPEC4's cap: the camera's 4 x 4
	near( "window cap 8 too wide w", float( wBig.w ), 4.0f );
	// LANDSPEC5: blocks 23 x 23 .. 61 x 61 whole (the camera anywhere in them, every third cell); wider ones paged: every
	// block cell within kPageReach (28) of the camera, blocks to 101 x 101
	{
		long cells = 0, holes = 0;
		for ( int half = 11; half <= 30; half++ )
			for ( int camX = -half; camX <= half; camX += 3 )
				for ( int camY = -half; camY <= half; camY += 3 ) {
					const Window w = WINDOWWIDE( camX + 1, camY - 3, 1 - half, -3 - half, 1 + half, -3 + half );
					for ( int x = -half; x <= half; x++ )
						for ( int y = -half; y <= half; y++ ) {
							cells++;
							holes += windowLayer( x + 1, y - 3, w.fx, w.fy, w.w ) < 0 ? 1 : 0;
						}
				}
		std::printf( "window wide block cells checked %ld\n", cells );
		near( "window holes in blocks 23 x 23 .. 61 x 61", float( holes ), 0.0f );
		cells = 0, holes = 0;
		int widest = 0;
		for ( int half = 31; half <= 50; half += 3 )
			for ( int camX = -half; camX <= half; camX += 5 )
				for ( int camY = -half; camY <= half; camY += 5 ) {
					const Window w = WINDOWWIDE( camX, camY, -half, -half, half, half );
					widest = std::max( widest, w.w );
					for ( int x = std::max( -half, camX - kPageReach ); x <= std::min( half, camX + kPageReach ); x++ )
						for ( int y = std::max( -half, camY - kPageReach ); y <= std::min( half, camY + kPageReach ); y++ ) {
							cells++;
							holes += windowLayer( x, y, w.fx, w.fy, w.w ) < 0 ? 1 : 0;
						}
				}
		std::printf( "window paged cells checked %ld\n", cells );
		near( "window paged holes within 28 cells", float( holes ), 0.0f );
		near( "window paged widest", float( widest ), 16.0f );
		const Window w61 = WINDOWWIDE( 30, 30, -30, -30, 30, 30 );	// cells -30..30: chunks -32..28 = 16 a side
		near( "window 61 x 61 corner w", float( w61.w ), 16.0f );
		near( "window 61 x 61 corner fx", float( w61.fx ), -32.0f );
		const Window wp = WINDOWWIDE( 0, 0, -50, -50, 50, 50 );	// paged: the camera's chunk 7 in
		near( "window paged fx", float( wp.fx ), -28.0f );
		const Window we = WINDOWWIDE( 50, 0, -50, -50, 50, 50 );	// paged at the edge: kept inside chunks -52..48
		near( "window paged edge fx", float( we.fx ), -12.0f );
		near( "window bit words", float( kBitWords ), 4.0f );
	}
	near( "window layer x + 8 y", float( windowLayer( 5, 9, 0, 0, 8 ) ), 17.0f );
	float u = 0.0f, v = 0.0f;
	lodUv( 6144.0f, 0.0f, 0, -4, u, v );
	near( "lod uv u", u, 0.375f );
	near( "lod uv v", v, 0.0f );
	near( "noise 0.5", NOISEK( 0.5f ), -0.1171111f, 1e-5f );
	near( "noise neutral", NOISEK( 0.7957059f ), 1.0f, 1e-5f );
	V3 n = BANDNORMAL( ( V3{ 0.0f, 0.0f, 1.0f } ), ( V3{ 0.6f, 0.0f, 0.8f } ), 1.0f );	// flat: (-d.x, -d.y, d.z)
	near( "band normal v1 x", n.x, -0.6f );
	near( "band normal v1 z", n.z, 0.8f );
	n = BANDNORMAL( ( V3{ 0.0f, 0.0f, 1.0f } ), ( V3{ 0.6f, 0.0f, 0.8f } ), 0.5f );	// (-0.3, 0, 0.9) / 0.948683
	near( "band normal v0.5 x", n.x, -0.3162278f );
	near( "band normal v0.5 z", n.z, 0.9486833f );
	// LANDSPEC4: the splat passes (shares 0.5 / 0.3 / 0.2 of three tilted layer normals) sum to the blended normal moved
	// toward ONE target; the single-pass value is mix(sum w_i n_i, T, v) with T the flat frame's (-d.x, -d.y, d.z)
	{
		const V3 ln[3] = { norm3( V3{ 0.4f, 0.0f, 0.9f } ), norm3( V3{ -0.3f, 0.35f, 0.9f } ), norm3( V3{ 0.1f, -0.5f, 0.8f } ) };
		const float sw[3] = { 0.5f, 0.3f, 0.2f };
		const V3 d{ 0.6f, 0.0f, 0.8f };
		V3 sum, nb;
		for ( int i = 0; i < 3; i++ ) {
			const V3 p = SPLATRAW( ln[i], d, 0.5f );
			sum = { sum.x + sw[i] * p.x, sum.y + sw[i] * p.y, sum.z + sw[i] * p.z };
			nb = { nb.x + sw[i] * ln[i].x, nb.y + sw[i] * ln[i].y, nb.z + sw[i] * ln[i].z };
		}
		const V3 once = splatNormalRaw( nb, d, 0.5f );
		const float err = std::fabs( sum.x - once.x ) + std::fabs( sum.y - once.y ) + std::fabs( sum.z - once.z );
		near( "splat passes = one blend (sum |error|)", err, 0.0f, 1e-5f );
		const V3 t = splatTarget( d ), b = BANDNORMAL( ( V3{ 0.0f, 0.0f, 1.0f } ), d, 1.0f );
		near( "splat target = band frame at flat x", t.x, b.x );
		near( "splat target = band frame at flat z", t.z, b.z );
	}

	// goal 4: dry land writes no SSR flag
	near( "ssr dry", float( LANDSSR( 0.0f ) ), 0.0f );
	near( "ssr wet", float( LandLodSpec::landSsr( 0.5f ) ), 1.0f );

	// LANDSPEC3: the .pbrm ground's roughness = the GGX lobe of Blinn-Phong n = 2^(10 g + 1): g 0.22 -> n 9.1896,
	// alpha^2 = 2 / 11.1896 = 0.178738, rough = 0.178738^0.25 = 0.650212; g 1 -> n 2048 -> 0.176733
	near( "pbr rough 0.22", PBRROUGH( 0.22f ), 0.650212f, 1e-4f );
	near( "pbr rough 1", PBRROUGH( 1.0f ), 0.176733f, 1e-4f );
	// a child worldspace draws its parent's LOD under PNAM bit 1 (Goodneighbor 0x1F); DiamondCity 0x16 too, bit 0 not
	near( "lod Goodneighbor", float( PARENTLOD( 0x3Cu, 0x1Fu ) ), 1.0f );
	near( "lod DiamondCity", float( PARENTLOD( 0x3Cu, 0x16u ) ), 1.0f );
	near( "lod land data only", float( PARENTLOD( 0x3Cu, 0x01u ) ), 0.0f );
	near( "lod no parent", float( PARENTLOD( 0u, 0x02u ) ), 0.0f );
	// LANDSPEC5: the chain's end. Worlds 1 -> 2 -> 3 -> 4: 1 and 2 set "Use LOD Data", 3 only "Use Land Data", so 1
	// draws 3's (two steps); 5 <-> 6 loop and stop at kLodClimbMax; a parent that cannot be read stops at once
	{
		const unsigned wn[8] = { 0, 2, 3, 4, 0, 6, 5, 0 }, pn[8] = { 0, 0x16, 0x1F, 0x01, 0, 0x02, 0x02, 0 };
		auto climbFrom = [&]( unsigned w0, bool readable, unsigned * end ) {
			unsigned at = w0;
			const int n = CLIMB( wn[w0], pn[w0], [&]( unsigned & w, unsigned & p ) {
				if ( !readable )
					return false;
				at = w;
				w = wn[at];
				p = pn[at];
				return true;
			} );
			*end = at;
			return n;
		};
		unsigned end = 0;
		near( "lod climb 1 -> 3 steps", float( climbFrom( 1, true, &end ) ), 2.0f );
		near( "lod climb 1 -> 3 end", float( end ), 3.0f );
		near( "lod climb own (3)", float( climbFrom( 3, true, &end ) ), 0.0f );
		near( "lod climb loop capped", float( climbFrom( 5, true, &end ) ), float( kLodClimbMax ) );
		near( "lod climb unreadable", float( climbFrom( 1, false, &end ) ), 0.0f );
	}

	// the shaders run the same laws (goal 1: pbrm_default.frag; goal 2: fo4_default.frag)
	if ( argc > 1 ) {
		const std::string t = argv[1];
		must( t, "fo4_default.frag", { "1.0 - clamp( ( landLodK.z - distance( landBandW, landLod.yz ) ) * landLodK.w, 0.0, 1.0 )",
			"smoothness = mix( smoothness, landLodK.x, landLodV );", "specMask = mix( specMask, landLodK.y, landLodV );",
			"* 3.777778 - 2.006;", "lod * noiseK", "landBandW * 0.00025", "w * 0.00035", "( 1.0 / 16384.0 )",
			"vec3( dd, sqrt( 1.0 - min( dot( dd, dd ), 1.0 ) ) )",
			"normalize( cross( n, cross( n, vec3( 1.0, 0.0, 0.0 ) ) ) )",
			// LANDSPEC3: the window array, the far ground's noise and detail normal
			"uniform sampler2DArray LandLodDiff;", "baseMap.rgb *= noiseK;",
			"float landDetailV = landLod.x > 1.5 ? 1.0 : landBandV;", "msnN = landDetailN( msnN,",
			// LANDSPEC4: the n x n window and its bits, the far ring 0's noise, one detail target for the splat passes
			"int k = ci.x + nW * ci.y;", "( ( int( landLodBits[k >> 6][( k >> 4 ) & 3] ) >> ( k & 15 ) ) & 1 ) != 0",
			"uniform vec4 landLodBits[4];",
			"baseMap.rgb *= noiseK;	// lane LANDSPEC4", "vec3( -dd, sqrt( 1.0 - min( dot( dd, dd ), 1.0 ) ) )",
			"normal = landSplat ? landDetailSplatN(" } );
		must( t, "pbrm_default.frag", { "1.0 - clamp( ( landLodK.z - distance( cellWorldPos( -ViewDir ).xy, landLod.yz ) ) * landLodK.w, 0.0, 1.0 )",
			"s.rough = mix( s.rough, sqrt( sqrt( 2.0 / ( exp2( landLodK.x * 10.0 + 1.0 ) + 2.0 ) ) ), landLodV );",
			"s.weight = mix( s.weight, landLodK.y, landLodV );" } );
	}
	std::printf( "LANDSPEC %s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
