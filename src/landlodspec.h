#ifndef LANDLODSPEC_H
#define LANDLODSPEC_H

/* lane LANDSPEC1: the game's LOD land specular and its land-LOD blend (no Qt; tests/landspec). The far ground takes
 * fLODLandSpecPower / fLODLandSpecScale as its gloss / spec mask; the near ground lerps its own toward them by the
 * blend factor of the distance (xy) to the centre of the camera's cell. fo4_default.frag (landLod) runs the same law. */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace LandLodSpec
{
constexpr float kPower = 0.22f;	// fLODLandSpecPower:TerrainManager (the far ground's gloss)
constexpr float kScale = 0.06f;	// fLODLandSpecScale:TerrainManager (its spec mask)
constexpr float kBlendEnd = 9625.6f;	// the blend is whole from 2.35 cells off the centre
constexpr float kBlendInv = 0.000375600968f;	// 1 / 2662.4: it starts 0.65 cells nearer (1.7 cells)
constexpr float kCell = 4096.0f;

enum Mode { Off = 0, Near = 1, Far = 2 };

inline float blend( float d )
{
	return 1.0f - std::clamp( ( kBlendEnd - d ) * kBlendInv, 0.0f, 1.0f );
}

//! the middle of the cell holding p, in a cell scene whose origin is a cell's middle
inline float cellCentre( float p )
{
	return std::floor( ( p + 0.5f * kCell ) / kCell ) * kCell;
}

struct GlossSpec
{
	float gloss = 1.0f, spec = 1.0f;
};

inline float mixf( float a, float b, float t )
{
	return a * ( 1.0f - t ) + b * t;
}

//! a ground point's gloss / spec mask (the material's x its _s map) as the game draws it
inline GlossSpec land( GlossSpec own, Mode m, float dx, float dy, float scale = kScale )
{
	const float v = m == Far ? 1.0f : ( m == Near ? blend( std::sqrt( dx * dx + dy * dy ) ) : 0.0f );
	return { mixf( own.gloss, kPower, v ), mixf( own.spec, scale, v ) };
}

/* Wetness: the land pre-pass law and the game's wetness clock. Nothing calls these yet: the viewer has no
 * precipitation (LEFTOVER in notes/landspec1). Defaults = fDefaultLandscape_WetnessControl_*. */
struct Wet
{
	float specScale = 0.5f, specPowerScale = 2.75f, specMin = 0.3f, metalness = 0.35f, envMapScale = 1.5f;
};

inline GlossSpec wetLand( GlossSpec gs, float wet, const Wet & w = Wet() )
{
	const float m = std::clamp( w.specMin, 0.0f, 1.0f );
	const float spec = gs.spec * ( 1.0f - wet * m ) + wet * m;
	return { mixf( 1.0f, w.specPowerScale, wet ) * gs.gloss, mixf( 1.0f, w.specScale, wet ) * spec };
}

inline float wetAlbedoK( float wet, const Wet & w = Wet() )
{
	return std::clamp( 1.0f - wet * w.metalness, 0.0f, 1.0f );
}

//! one step: rain (and ripples on) wets at fWetSpeed 0.03 a second, else dries at fDrySpeed 0.02
inline float wetStep( float wet, bool raining, float dt )
{
	return raining ? std::min( wet + 0.03f * dt, 1.0f ) : std::max( wet - 0.02f * dt, 0.0f );
}

//! lane LANDSPEC2: the land pre-pass writes its reflection (SSR) flag only on wet ground; dry land has none
inline bool landSsr( float wet )
{
	return wet != 0.0f;
}

/* lane LANDSPEC2: the blend centre glides from the old cell's centre to the new one over 5 s (linear); it snaps on
 * the first frame or a move of more than one cell. Times in seconds. */
constexpr double kGlideSeconds = 5.0;

struct Glide
{
	bool init = false;
	float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;
	double t0 = 0.0;
};

inline float glideT( const Glide & g, double now )
{
	return float( std::clamp( ( now - g.t0 ) / kGlideSeconds, 0.0, 1.0 ) );
}

inline void glideAt( const Glide & g, double now, float & x, float & y )
{
	const float t = glideT( g, now );
	x = mixf( g.fromX, g.toX, t );
	y = mixf( g.fromY, g.toY, t );
}

inline void glideTo( Glide & g, float x, float y, double now, bool snap )
{
	if ( g.init && x == g.toX && y == g.toY )
		return;
	if ( !g.init || snap || std::fabs( x - g.toX ) > 1.5f * kCell || std::fabs( y - g.toY ) > 1.5f * kCell ) {
		g = { true, x, y, x, y, now };
		return;
	}
	float cx, cy;
	glideAt( g, now, cx, cy );	// a cross mid-glide starts from where the centre stands
	g = { true, cx, cy, x, y, now };
}

inline bool gliding( const Glide & g, double now )
{
	return g.init && ( g.fromX != g.toX || g.fromY != g.toY ) && glideT( g, now ) < 1.0f;
}

/* lane LANDSPEC2: the band's textures. The near land PS lerps its albedo toward the level-4 LOD diffuse x the
 * terrain noise, and its normal toward the LOD detail normal, by the same blend factor. */
constexpr int kLodLevel = 4;	// the near land samples the level-4 terrain LOD chunk holding it
constexpr float kChunk = kCell * kLodLevel;
constexpr float kNoiseUv = 0.00025f;	// Textures\Terrain\Noise.dds, world xy
constexpr float kDetailUv = 0.00035f;	// Textures\Terrain\DetailNormals.dds, world xy

//! the noise texel's albedo factor
inline float noiseK( float n )
{
	return n * 3.777778f - 2.006f;
}

/* lane LANDSPEC3: a 4 x 4 window of chunks (one texture array, layer = x + 4 y), so a near block wider than the game's
 * 5 x 5 still finds its band texture: it holds every cell within kReach of the camera's */
constexpr int kWindow = 4;
constexpr int kReach = ( kWindow - 1 ) * kLodLevel / 2;

//! the first chunk (cells) of the window round the camera's cell
inline int windowFirst( int camCell )
{
	return int( std::floor( float( camCell - kReach ) / float( kLodLevel ) ) ) * kLodLevel;
}

//! the window layer holding cell (cx, cy), -1 outside (w chunks a side)
inline int windowLayer( int cx, int cy, int fx, int fy, int w = kWindow )
{
	const int x = int( std::floor( float( cx - fx ) / float( kLodLevel ) ) );
	const int y = int( std::floor( float( cy - fy ) / float( kLodLevel ) ) );
	return x >= 0 && y >= 0 && x < w && y < w ? x + w * y : -1;
}

/* lane LANDSPEC4: the window also covers the whole near block (cells bx0..bx1, by0..by1), up to kWindowMax chunks a
 * side; a wider union falls back to the camera's 4 x 4.
 * LANDSPEC5: kWindowMax 16 (256 layers): n x n layers of one vanilla level-4 chunk each (512 x 512 BC3, 10 levels,
 * 341 KB), so blocks to 61 x 61 are whole at n^2 x 341 KB (21 x 21: 8 x 8 = 21.8 MB; 61 x 61: 16 x 16 = 87.4 MB).
 * A wider union pages: a 16 x 16 window round the camera (every cell within kPageReach of it), kept inside the union
 * and rebuilt when the camera crosses a chunk. Red WW_LANDSPEC_RED=win8: LANDSPEC4's cap 8, no paging. */
constexpr int kWindowMax = 16;
constexpr int kWindowMax4 = 8;	// LANDSPEC4's cap (the red)
constexpr int kBitWords = kWindowMax * kWindowMax / 64;	// the bound layers, 64 a word
constexpr int kPageReach = ( kWindowMax / 2 - 1 ) * kLodLevel;	// 28 cells

struct Window
{
	int fx = 0, fy = 0, w = kWindow;
	int mip = 0;	// lane LANDSPEC7: the chunks' mip each layer holds (the outer window's 1 or 2)
	int pack = 1;	// lane LANDSPEC8: chunks a side in one layer (2: the packed outer window)
};

/* lane LANDSPEC8: a chunk's texture id from bind( id ), which returns the mip count and sets id only when the file was
 * already loaded (TexCache::bind leaves wwBound 0 on the call that loads it), so a load binds once more. Red
 * WW_LANDSPEC_RED=firstload: no second bind (every chunk a window loads stays out, SHOTPASS1 D6's "0 of 36"). */
template <class Bind>
inline unsigned chunkTex( Bind bind, bool once = false )
{
	unsigned id = 0;
	int m = bind( id );
	if ( m > 0 && !id && !once )
		m = bind( id );
	return m > 0 ? id : 0;
}

inline int chunkFloor( int c )
{
	return int( std::floor( float( c ) / float( kLodLevel ) ) ) * kLodLevel;
}

//! the first chunk of a page of `w` chunks: the camera's chunk (w / 2 - 1) in, kept inside the chunks lo..hi
inline int pageFirst( int cam, int lo, int hi, int w )
{
	const int f = chunkFloor( cam ) - ( w / 2 - 1 ) * kLodLevel;
	return std::max( lo, std::min( f, hi - ( w - 1 ) * kLodLevel ) );
}

inline Window windowFor( int camX, int camY, bool block, int bx0, int by0, int bx1, int by1, int cap = kWindowMax,
	bool page = true )
{
	const Window cam{ windowFirst( camX ), windowFirst( camY ), kWindow };
	if ( !block )
		return cam;
	const int x0 = chunkFloor( std::min( camX - kReach, bx0 ) ), x1 = chunkFloor( std::max( camX + kReach, bx1 ) );
	const int y0 = chunkFloor( std::min( camY - kReach, by0 ) ), y1 = chunkFloor( std::max( camY + kReach, by1 ) );
	const int w = std::max( x1 - x0, y1 - y0 ) / kLodLevel + 1;
	if ( w <= cap )
		return Window{ x0, y0, std::max( w, kWindow ) };
	if ( !page )
		return cam;
	// LANDSPEC5: the block alone when it fits (the camera's reach past the block's edge has no near land), else a page
	const int c0 = chunkFloor( bx0 ), c1 = chunkFloor( bx1 ), r0 = chunkFloor( by0 ), r1 = chunkFloor( by1 );
	const int wb = std::max( c1 - c0, r1 - r0 ) / kLodLevel + 1;
	if ( wb <= cap )
		return Window{ c0, r0, std::max( wb, kWindow ) };
	return Window{ pageFirst( camX, c0, c1, cap ), pageFirst( camY, r0, r1, cap ), cap };
}

/* lane LANDSPEC6: a block wider than the page (61 cells) also gets an outer window, one layer a chunk read from the
 * chunk's mip kOuterMip (256 x 256 BC3, 9 levels, 85.4 KiB a layer), up to kOuterMax chunks a side: 81 x 81 = 21 x 21
 * = 36.8 MiB, 125 x 125 and wider = 32 x 32 = 85.4 MiB. The page wins where it holds the cell; the outer window covers
 * the rest. A block wider than kOuterMax chunks pages it round the camera (every cell within kOuterReach of it).
 * w 0 = no outer window (the page holds the whole block). Red WW_LANDSPEC_RED=page28: none (LANDSPEC5). */
constexpr int kOuterMax = 32;
constexpr int kOuterMip = 1;
constexpr int kOuterReach = ( kOuterMax / 2 - 1 ) * kLodLevel;	// 60 cells

/* lane LANDSPEC7: a block wider than kOuterMax chunks (125 cells) reads its outer window from the chunks' mip
 * kOuterMip2 instead (128 x 128 BC3, 8 levels, 21,872 B a layer, a quarter of mip 1's), so up to kOuterMax2 chunks a
 * side are whole: 129 x 129 = 33 x 33 = 22.7 MiB, 177 x 177 = 45 x 45 = 42.2 MiB (LANDSPEC6 paged them at 32 x 32 mip 1
 * = 85.4 MiB and left the land past 60 cells bare). 45 x 45 = 2025 layers, under the 2048 a GL 4.5 driver must give
 * (GL_MAX_ARRAY_TEXTURE_LAYERS, read at draw: `layers`; a smaller limit narrows the window). Wider (the whole
 * Commonwealth, 49 x 49 = 2401 layers) pages the 45 x 45 round the camera: every cell within kOuterReach2 = 84 of it.
 * Red WW_LANDSPEC_RED=outer32: LANDSPEC6's 32 x 32 at mip 1, paged past 125 cells. */
constexpr int kOuterMax2 = 45;
constexpr int kOuterMip2 = 2;
constexpr int kOuterReach2 = ( kOuterMax2 / 2 - 1 ) * kLodLevel;	// 84 cells

/* lane LANDSPEC8: a block wider than the driver's mip-2 cap (45 chunks, the whole Commonwealth's 49) packs 2 x 2 chunks
 * at mip 2 into one 256 x 256 layer (6 levels: the chunk's 128..4, BC3 copies stay block-aligned; 87,360 B a layer), up
 * to kOuterPackMax chunks a side: 49 x 49 = 25 x 25 = 625 layers = 52.1 MiB, whole. Wider pages the 50 x 50 round the
 * camera (every cell within kOuterReachPack = 96 of it). Layer = (x / 2) + ((n + 1) / 2)(y / 2); the chunk at odd x
 * sits right, at odd y (north) on the layer's top rows. Red WW_LANDSPEC_RED=outer45: LANDSPEC7's 45 x 45, paged. */
constexpr int kOuterPackMax = 50;
constexpr int kOuterReachPack = ( kOuterPackMax / 2 - 1 ) * kLodLevel;	// 96 cells
constexpr int kOuterPackMinPx = 4;	// a packed level holds the chunk's levels at least one BC3 block wide
constexpr int kOuterBitWords = ( kOuterPackMax * kOuterPackMax + 63 ) / 64;	// 40 (LANDSPEC7: 32, LANDSPEC6: 16)

inline int packSide( int n )
{
	return ( n + 1 ) / 2;
}

//! the layer and the corner (in chunk widths: 0 or 1 each) of chunk x, y of a packed window n chunks a side
inline int packLayer( int x, int y, int n, int & ox, int & oy )
{
	ox = x & 1;
	oy = 1 - ( y & 1 );	// north (odd y) on the top rows: v = 1 - f.y as the unpacked layers
	return ( x >> 1 ) + packSide( n ) * ( y >> 1 );
}

//! a packed layer's levels for chunks cw x ch (at the window's mip) that have `levels` levels
inline int packLevels( int cw, int ch, int levels )
{
	int l = 0;
	while ( l < levels && ( cw >> l ) >= kOuterPackMinPx && ( ch >> l ) >= kOuterPackMinPx )
		l++;
	return l;
}

//! the outer window's chunks a side packed for a driver giving `layers` layers: even, (c / 2)^2 <= layers
inline int outerCapPack( int layers )
{
	int c = 0;
	while ( c + 2 <= kOuterPackMax && ( ( c + 2 ) / 2 ) * ( ( c + 2 ) / 2 ) <= layers )
		c += 2;
	return c;
}

//! a BC3 array's bytes (16 B a 4 x 4 block, every level at least one block): w x h, `levels` levels, `layers` layers
inline long long arrayBytes( int w, int h, int levels, int layers )
{
	long long b = 0;
	for ( int l = 0; l < levels; l++ )
		b += 16LL * std::max( 1, ( std::max( w >> l, 1 ) + 3 ) / 4 ) * std::max( 1, ( std::max( h >> l, 1 ) + 3 ) / 4 );
	return b * layers;
}

//! the outer window's chunks a side at mip kOuterMip2 for a driver giving `layers` array layers (0: LANDSPEC6's)
inline int outerCap2( int layers )
{
	int c = 0;
	while ( c < kOuterMax2 && ( c + 1 ) * ( c + 1 ) <= layers )
		c++;
	return c > kOuterMax ? c : 0;
}

inline Window outerFor( int camX, int camY, int bx0, int by0, int bx1, int by1, int layers = 2048, bool red32 = false,
	bool red45 = false )
{
	const int c0 = chunkFloor( bx0 ), c1 = chunkFloor( bx1 ), r0 = chunkFloor( by0 ), r1 = chunkFloor( by1 );
	const int wb = std::max( c1 - c0, r1 - r0 ) / kLodLevel + 1;
	if ( wb <= kWindowMax )
		return Window{ 0, 0, 0 };
	if ( wb <= kOuterMax )
		return Window{ c0, r0, wb, kOuterMip };
	const int cap = red32 ? 0 : outerCap2( layers );
	if ( !cap )
		return Window{ pageFirst( camX, c0, c1, kOuterMax ), pageFirst( camY, r0, r1, kOuterMax ), kOuterMax, kOuterMip };
	if ( wb <= cap )
		return Window{ c0, r0, wb, kOuterMip2 };
	const int pc = red45 ? 0 : outerCapPack( layers );	// LANDSPEC8: packed 2 x 2 a layer
	if ( pc > cap )
		return wb <= pc ? Window{ c0, r0, wb, kOuterMip2, 2 }
			: Window{ pageFirst( camX, c0, c1, pc ), pageFirst( camY, r0, r1, pc ), pc, kOuterMip2, 2 };
	return Window{ pageFirst( camX, c0, c1, cap ), pageFirst( camY, r0, r1, cap ), cap, kOuterMip2 };
}

/* lane LANDSPEC6: a "Fixed Dimensions" world (WRLD DATA bit 0x40: DiamondCity, DiamondCityFX, NukaWorldAmphitheater,
 * NukaWorldMarket) never moves its grid. The game puts the grid and the land-LOD blend centre on the middle of the
 * WCTR cell when the world loads, skips every grid shift after, and places the terrain LOD once at the arrival point
 * (notes/landspec6/re). Red WW_LANDSPEC_RED=fixedfollow: the centre and the streamed block follow the camera. */
constexpr unsigned kFixedDimensions = 0x40u;

inline bool fixedWorld( unsigned dataFlags )
{
	return ( dataFlags & kFixedDimensions ) != 0;
}

//! a fixed world's blend centre in scene units (the scene's origin at world ox, oy): the WCTR cell's middle
inline void fixedCentre( int wx, int wy, float ox, float oy, float & x, float & y )
{
	x = ( float( wx ) + 0.5f ) * kCell - ox;
	y = ( float( wy ) + 0.5f ) * kCell - oy;
}

//! the streamed near block's centre cell at the open: a fixed world's WCTR, else the open's centre (which then shifts)
inline void streamCentre( bool fixed, int wx, int wy, int openX, int openY, int & x, int & y )
{
	x = fixed ? wx : openX;
	y = fixed ? wy : openY;
}

/* lane LANDSPEC7: the game loads a fixed world's cell only within uGridsToLoad / 2 of WCTR (notes/landspec6/re,
 * notes/landspec7/re), so an open's block (n cells a side round cx, cy) is fitted to the cells it would load: at most
 * uGridsToLoad a side, moved (not cut: a spec block is square round its centre) until it lies inside WCTR +- g. The
 * camera stands on the fitted block's centre. Returns whether the block changed. Red WW_LANDSPEC_RED=fixedopen: the
 * asked block, cells the game never loads included (LANDSPEC6). */
constexpr int kFixedGridHalf = 2;	// uGridsToLoad 5 / 2 (the default; LANDSPEC8 reads the ini: gridHalf)

/* lane LANDSPEC8: [General] uGridsToLoad from the game's inis (Fallout4_Default.ini, then My Games' Fallout4.ini,
 * Fallout4Prefs.ini, Fallout4Custom.ini, a later file overriding), not a hard-coded 5: the loaded block's half width
 * (u - 1) / 2, unset or < 1 the game's 5, at most kGridsHalfMax. Red WW_LANDSPEC_RED=grids5: 5 whatever the ini says. */
constexpr int kGridsDefault = 5;
constexpr int kGridsHalfMax = 10;	// 21 x 21 cells

inline int gridHalf( int grids )
{
	return std::min( kGridsHalfMax, ( ( grids >= 1 ? grids : kGridsDefault ) - 1 ) / 2 );
}

//! one ini line: tracks the [General] section; returns the uGridsToLoad it sets, else -1
inline int gridsLine( const std::string & raw, bool & general )
{
	const size_t a = raw.find_first_not_of( " \t" ), b = raw.find_last_not_of( " \t\r\n" );
	if ( a == std::string::npos )
		return -1;
	std::string l = raw.substr( a, b - a + 1 );
	for ( char & c : l )
		c = char( std::tolower( static_cast<unsigned char>( c ) ) );
	if ( l[0] == '[' ) {
		general = l == "[general]";
		return -1;
	}
	const size_t eq = l.find( '=' );
	if ( !general || eq == std::string::npos )
		return -1;
	std::string key = l.substr( 0, eq );
	key.erase( key.find_last_not_of( " \t" ) + 1 );
	if ( key != "ugridstoload" )
		return -1;
	const size_t v = l.find_first_not_of( " \t", eq + 1 );
	if ( v == std::string::npos || !std::isdigit( static_cast<unsigned char>( l[v] ) ) )
		return -1;
	return std::atoi( l.c_str() + v );
}

inline bool fixedBlock( int wx, int wy, int g, int & cx, int & cy, int & n )
{
	const int h0 = ( std::max( 1, n ) - 1 ) / 2;
	const int h = std::min( h0, std::max( 0, g ) );
	const int x = std::max( wx - ( g - h ), std::min( cx, wx + ( g - h ) ) );
	const int y = std::max( wy - ( g - h ), std::min( cy, wy + ( g - h ) ) );
	const bool changed = h != h0 || x != cx || y != cy;
	if ( h != h0 )
		n = 2 * h + 1;
	cx = x;
	cy = y;
	return changed;
}

/* lane LANDSPEC3: a .pbrm ground's roughness at the LOD gloss g: the GGX lobe matching the legacy draw's Blinn-Phong
 * power n = 2^(10 g + 1) (alpha^2 = 2 / (n + 2), rough = sqrt(alpha)); pbrm_default.frag has the same line */
inline float pbrRough( float g )
{
	return std::sqrt( std::sqrt( 2.0f / ( std::exp2( g * 10.0f + 1.0f ) + 2.0f ) ) );
}

/* lane LANDSPEC3: whose terrain LOD a worldspace draws: its parent's when WRLD PNAM bit 1 "Use LOD Data" is set
 * (Goodneighbor -> Commonwealth), else its own */
inline bool usesParentLod( unsigned parentForm, unsigned pnam )
{
	return parentForm != 0 && ( pnam & 2u ) != 0;
}

/* lane LANDSPEC5: the game's terrain manager is the one of the END of the chain: it climbs WNAM while each world sets
 * "Use LOD Data" (a child of a child draws the root's). step( wnam, pnam ) moves to the parent and reads its own
 * WNAM / PNAM (false: unreadable, stop). `once` = LANDSPEC3/4's one step (red WW_LANDSPEC_RED=climb1). Returns the
 * steps taken; kLodClimbMax stops a WNAM loop. */
constexpr int kLodClimbMax = 8;

template <class Step>
inline int lodClimb( unsigned wnam, unsigned pnam, Step step, bool once = false )
{
	int n = 0;
	while ( n < kLodClimbMax && usesParentLod( wnam, pnam ) && step( wnam, pnam ) ) {
		n++;
		if ( once )
			break;
	}
	return n;
}

//! a world xy's uv in the level-4 chunk whose corner cell is (cx, cy), as the near land VS writes it
inline void lodUv( float wx, float wy, int cx, int cy, float & u, float & v )
{
	u = ( wx - float( cx ) * kCell ) / kChunk;
	v = 1.0f - ( wy - float( cy ) * kCell ) / kChunk;
}

struct V3
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline V3 cross3( V3 a, V3 b )
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline float dot3( V3 a, V3 b )
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline V3 norm3( V3 a )
{
	const float l = std::sqrt( dot3( a, a ) );
	return l > 0.0f ? V3{ a.x / l, a.y / l, a.z / l } : a;
}

/* the tangent-space normal n moved toward the detail normal d (decoded, z = sqrt(1 - x^2 - y^2)) in n's own frame:
 * t1 = n x (n x X), t2 = n x t1; then normalize(mix(n, (t1.d, t2.d, n.d), v)) */
inline V3 bandNormal( V3 n, V3 d, float v )
{
	const V3 t1 = norm3( cross3( n, cross3( n, V3{ 1.0f, 0.0f, 0.0f } ) ) );
	const V3 t2 = norm3( cross3( n, t1 ) );
	const V3 nn = norm3( n );
	const V3 tg{ dot3( t1, d ), dot3( t2, d ), dot3( nn, d ) };
	return norm3( V3{ mixf( n.x, tg.x, v ), mixf( n.y, tg.y, v ), mixf( n.z, tg.z, v ) } );
}

/* lane LANDSPEC4: a splat layer pass (cellsplat draws one additive pass a layer) moves toward ONE target, d in the
 * vertex frame (tangent z), so the passes' share-weighted sum is the blended normal moved once: sum w_i mix(n_i, T, v)
 * = mix(sum w_i n_i, T, v). Per-layer frames (bandNormal) gave each layer its own target. */
inline V3 splatTarget( V3 d )
{
	return { -d.x, -d.y, d.z };	// bandNormal's frame at n = (0, 0, 1): t1 = -X, t2 = -Y
}

inline V3 splatNormalRaw( V3 n, V3 d, float v )
{
	const V3 tg = splatTarget( d );
	return { mixf( n.x, tg.x, v ), mixf( n.y, tg.y, v ), mixf( n.z, tg.z, v ) };
}
} // namespace LandLodSpec

//! lane LANDSPEC2: the blend centre is gliding (src/gl/renderer.cpp); glview repaints while it does
bool wwLandLodWantsRepaint();

#endif
