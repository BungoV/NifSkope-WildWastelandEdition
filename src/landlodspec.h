#ifndef LANDLODSPEC_H
#define LANDLODSPEC_H

/* lane LANDSPEC1: the game's LOD land specular and its land-LOD blend (no Qt; tests/landspec). The far ground takes
 * fLODLandSpecPower / fLODLandSpecScale as its gloss / spec mask; the near ground lerps its own toward them by the
 * blend factor of the distance (xy) to the centre of the camera's cell. fo4_default.frag (landLod) runs the same law. */

#include <algorithm>
#include <cmath>

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
 * side (64 layers, 21.8 MB of vanilla BC3); a wider union falls back to the camera's 4 x 4 */
constexpr int kWindowMax = 8;

struct Window
{
	int fx = 0, fy = 0, w = kWindow;
};

inline int chunkFloor( int c )
{
	return int( std::floor( float( c ) / float( kLodLevel ) ) ) * kLodLevel;
}

inline Window windowFor( int camX, int camY, bool block, int bx0, int by0, int bx1, int by1 )
{
	const Window cam{ windowFirst( camX ), windowFirst( camY ), kWindow };
	if ( !block )
		return cam;
	const int x0 = chunkFloor( std::min( camX - kReach, bx0 ) ), x1 = chunkFloor( std::max( camX + kReach, bx1 ) );
	const int y0 = chunkFloor( std::min( camY - kReach, by0 ) ), y1 = chunkFloor( std::max( camY + kReach, by1 ) );
	const int w = std::max( x1 - x0, y1 - y0 ) / kLodLevel + 1;
	return w <= kWindowMax ? Window{ x0, y0, std::max( w, kWindow ) } : cam;
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
