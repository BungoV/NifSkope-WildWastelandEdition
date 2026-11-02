#ifndef CELLOBJFADE_H
#define CELLOBJFADE_H

/* lane OBJFADE1 (GFXR-GEO G1): the game's per-class distance fade of placed references, as Fallout 4 runs it for
 * every ref with a fade node. Pure math, no Qt and no GL, so tests/objfade checks it alone.
 *
 * Per ref, from its fade node's world bound radius r (uGridsToLoad G = 5):
 *   far  = 2048 (G - 1) - r, and when r > 0 at most fLODBoundRadiusMult (10) x r
 *   near = fLODFadeOutPercent (0.6) x far; when near < iMinNearDistance (500): near = 500, far = max(far, 1000)
 * Per frame, with the camera's FOV f and the class multiplier m (fLODFadeOutMultObjects / Items / Actors):
 *   d = |camera - bound centre| x (f / 80) / m;  alpha = 1 - (d - near) / (far - near)  (1 when d <= near)
 * The ref is switched out when alpha < 0.3 and back in when alpha > 0.7; the drawn alpha then ramps in time.
 * Class: Items = SCRL ARMO BOOK INGR MISC WEAP AMMO KEYM ALCH HAZD SLGM (and a carried LIGH), Actors = NPC_,
 * Objects = the rest. A non-actor ref with r > 2048 is "large": no multiplier and no FOV term, d = |..| x 0.2
 * (an interior keeps it an Object).
 *
 * Rulings of this lane (WW_CHANGES "OBJFADE1"): without the ramp (WW_CELL_OBJFADE_RAMP, off) a ref is hidden where
 * the game's switch-out lands (alpha < 0.3; no memory of the switch-in). The dithered band thins the refs between the
 * switch-in and switch-out distances by a per-ref hash instead (off). Rung 0 is the angular law: hidden when
 * r / distance is below a threshold (off).
 *
 * Late items (each its own switch, off = the above):
 *   RAMP: the game's memory and time ramp per ref (Ramp, rampStep): a shown ref ramps out once alpha_d < 0.3, a hidden
 *     one comes back only past 0.7 (or past 0.3 while still partly in); the drawn alpha moves by
 *     min(dt / 2 s, 0.1) a frame; a ref not seen for 20 frames snaps back to 1. The cull has no per-pixel alpha, so a
 *     ref part way is drawn when its hash is below its alpha (each ref pops at its own point of the ramp).
 *   LOCMULT: the actors' multiplier x the location's "Actor Fade Mult" (LCTN ANAM) when non-zero.
 *   VWD: a non-actor ref whose own or base record has flag 0x8000 (Visible When Distant / Has Distant LOD) is large.
 *   LIGH: a LIGH base with "Can be Carried" (DATA flags 0x2) is an item.
 *   CASTERS: a shadow pass drops a ref the camera faded out (ramp: alpha 0; no ramp: hidden at the camera). */

#include <cmath>
#include <cstdint>
#include <cstring>

namespace WwObjFade
{

enum Type : std::uint8_t
{
	None = 0,	// never faded (terrain, the far field, a run with no placed ref)
	Objects = 1,
	Items = 2,
	Actors = 3,
	Large = 6	// set by the law from the radius, never by typeOf
};

//! bits a run's fade type carries beside its class; the law reads them only when their switch is on
constexpr std::uint8_t VisibleDistant = 0x80;	// the ref's or its base's record flag 0x8000
constexpr std::uint8_t CarriedLight = 0x40;		// a LIGH base with DATA flag 0x2 (class Objects)
constexpr std::uint8_t ClassMask = 0x3f;

inline std::uint8_t classOf( std::uint8_t t ) { return std::uint8_t( t & ClassMask ); }

//! the three multipliers, Objects / Items / Actors
struct Mults
{
	float objects = 4.5f, items = 2.5f, actors = 6.0f;	// the exe's defaults (Fallout4_Default.ini)
};

//! the shipped presets' [LOD] values: 0 Low, 1 Medium, 2 High, 3 Ultra; any other value = the exe's defaults
inline Mults presetMults( int preset )
{
	static const float t[4][3] = { { 5.0f, 1.5f, 5.0f }, { 5.0f, 3.0f, 5.0f }, { 9.0f, 6.0f, 9.0f }, { 30.0f, 10.0f, 15.0f } };
	Mults m;
	if ( preset >= 0 && preset <= 3 ) {
		m.objects = t[preset][0];
		m.items = t[preset][1];
		m.actors = t[preset][2];
	}
	return m;
}

//! a base record's fourcc (as stored, little-endian) -> its fade class
inline std::uint8_t typeOf( std::uint32_t fourcc, bool actor, bool carriedLight = false )
{
	if ( actor )
		return Actors;
	static const char items[][5] = { "SCRL", "ARMO", "BOOK", "INGR", "MISC", "WEAP", "AMMO", "KEYM", "ALCH", "HAZD", "SLGM" };
	for ( const char * s : items )
		if ( std::memcmp( &fourcc, s, 4 ) == 0 )
			return Items;
	if ( carriedLight && std::memcmp( &fourcc, "LIGH", 4 ) == 0 )
		return Items;
	if ( std::memcmp( &fourcc, "NPC_", 4 ) == 0 )
		return Actors;
	return Objects;
}

struct Range
{
	float nearR = 500.0f, farR = 1000.0f;
};

inline Range rangeOf( float r, int grids = 5 )
{
	Range g;
	float f = 2048.0f * float( grids - 1 ) - r;
	if ( r != 0.0f && 10.0f * r < f )
		f = 10.0f * r;
	float n = 0.6f * f;
	if ( n < 500.0f ) {
		n = 500.0f;
		if ( f < 1000.0f )
			f = 1000.0f;
	}
	g.nearR = n;
	g.farR = f;
	return g;
}

//! the class the law runs with: a large non-actor ref outdoors loses its multiplier (vwd: also a Visible When
//! Distant one; ligh: a carried light is an item)
inline std::uint8_t effectiveType( std::uint8_t type, float r, bool interior, bool vwd = false, bool ligh = false )
{
	std::uint8_t c = classOf( type );
	if ( ligh && ( type & CarriedLight ) && c == Objects )
		c = Items;
	if ( c != None && c != Actors && !interior && ( r > 2048.0f || ( vwd && ( type & VisibleDistant ) ) ) )
		return Large;
	return c;
}

//! the per-unit factor k of d = distance x k; 0 = never faded
inline float factorOf( std::uint8_t eff, const Mults & m, float fovDeg )
{
	float mult = 0.0f;
	switch ( eff ) {
	case Objects: mult = m.objects; break;
	case Items: mult = m.items; break;
	case Actors: mult = m.actors; break;
	case Large: return 0.2f;
	default: return 0.0f;
	}
	if ( !( mult > 0.0f ) )
		return 0.2f;	// the game takes 0.2 for a multiplier <= 0
	return ( fovDeg / 80.0f ) / mult;
}

//! the game's alpha_d for an effective class
inline float alphaEff( float dist, float r, std::uint8_t eff, const Mults & m, float fovDeg, int grids )
{
	const float k = factorOf( eff, m, fovDeg );
	if ( k <= 0.0f )
		return 1.0f;
	const Range g = rangeOf( r, grids );
	const float d = dist * k;
	if ( d <= g.nearR )
		return 1.0f;
	return 1.0f - ( d - g.nearR ) / ( g.farR - g.nearR );
}

//! the game's alpha_d at this camera distance (1 inside near; unclamped below 0 past far)
inline float alphaAt( float dist, float r, std::uint8_t type, const Mults & m, float fovDeg, bool interior, int grids = 5 )
{
	return alphaEff( dist, r, effectiveType( type, r, interior ), m, fovDeg, grids );
}

//! the camera distance where alpha reaches `a` (0.3 = the switch-out, 0.7 = the switch-in); 0 = never
inline float distanceAt( float a, float r, std::uint8_t type, const Mults & m, float fovDeg, bool interior, int grids = 5 )
{
	const std::uint8_t eff = effectiveType( type, r, interior );
	const float k = factorOf( eff, m, fovDeg );
	if ( k <= 0.0f )
		return 0.0f;
	const Range g = rangeOf( r, grids );
	return ( g.nearR + ( 1.0f - a ) * ( g.farR - g.nearR ) ) / k;
}

enum Law : int
{
	Game = 0,		// the game's switch-out distance
	Angular = 1		// rung 0: the bound's angular size
};

struct Config
{
	Mults mults;
	float fovDeg = 70.0f;
	int grids = 5;
	int law = Game;
	bool band = false;			// the dithered band between the switch-in and switch-out distances
	float minAngle = 0.0014f;	// Angular: r / distance below this is hidden (about 2 px of 1080 at 43 deg tall)
	// late items, each off by default
	bool ramp = false;			// the game's switch memory and time ramp
	bool locMult = false;		// the actors' multiplier x the location's Actor Fade Mult
	bool vwd = false;			// Visible When Distant refs are large
	bool ligh = false;			// carried lights are items
	bool casters = false;		// shadow passes drop what the camera faded out
	float rampDt = 0.0f;		// > 0: a fixed frame time for the ramp (the harness), else the measured one
	float fadeInSec = 2.0f, fadeOutSec = 2.0f, maxInc = 0.1f;	// fFadeInTime, fFadeOutTime, maxFadeIncrement
};

//! the game's alpha_d with the config's switches
inline float alphaOf( float dist, float r, std::uint8_t type, bool interior, const Config & c )
{
	return alphaEff( dist, r, effectiveType( type, r, interior, c.vwd, c.ligh ), c.mults, c.fovDeg, c.grids );
}

//! a per-ref value in [0, 1) for the dithered band (FNV-1a over the form id's bytes)
inline float refHash01( std::uint32_t ref )
{
	std::uint32_t h = 2166136261u;
	for ( int i = 0; i < 4; i++ ) {
		h ^= ( ref >> ( 8 * i ) ) & 0xffu;
		h *= 16777619u;
	}
	return float( h >> 8 ) / 16777216.0f;
}

//! is the ref hidden at this camera distance
inline bool hidden( float dist, float r, std::uint8_t type, std::uint32_t ref, bool interior, const Config & c )
{
	if ( classOf( type ) == None )
		return false;
	if ( c.law == Angular )
		return dist > 0.0f && r / dist < c.minAngle;
	const float a = alphaOf( dist, r, type, interior, c );
	if ( !c.band )
		return a < 0.3f;
	if ( a >= 0.7f )
		return false;
	if ( a < 0.3f )
		return true;
	return refHash01( ref ) >= ( a - 0.3f ) / 0.4f;	// the share kept falls from 1 at the switch-in to 0 at the switch-out
}

//! RAMP: one ref's switch state and drawn alpha (a node first seen, or unseen for 20 frames, starts shown at 1)
struct Ramp
{
	bool in = true;
	float a = 1.0f;
};

//! one frame of the game's fade update for alpha_d `ad` and frame time dt
inline void rampStep( Ramp & s, float ad, float dt, const Config & c )
{
	const float up = std::fmin( dt / c.fadeInSec, c.maxInc ), down = std::fmin( dt / c.fadeOutSec, c.maxInc );
	if ( s.in ) {
		if ( ad >= 0.3f ) {
			s.a = std::fmin( 1.0f, s.a + up );
		} else {
			s.a -= down;
			if ( s.a <= 0.0f ) {
				s.a = 0.0f;
				s.in = false;
			}
		}
	} else if ( ad > 0.7f || ( s.a > 0.0f && s.a < 1.0f && ad > 0.3f ) ) {
		s.a += up;
		if ( s.a >= 1.0f ) {
			s.a = 1.0f;
			s.in = true;
		}
	} else {
		s.a = std::fmax( 0.0f, s.a - down );
	}
}

//! is a ramping ref drawn: whole at 1, none at 0, part way when its hash is below its alpha
inline bool rampDrawn( const Ramp & s, std::uint32_t ref )
{
	return s.a >= 1.0f || ( s.a > 0.0f && refHash01( ref ) < s.a );
}

//! CASTERS: does a ref cast in a shadow pass (ramp: while its alpha is above 0; else: unless hidden at the camera)
inline bool casterCasts( const Ramp * s, bool hiddenAtCamera )
{
	return s ? s->a > 0.0f : !hiddenAtCamera;
}

//! LOCMULT: the actors' multiplier in a location with Actor Fade Mult `loc` (0 = none: unchanged)
inline Mults withLocation( Mults m, float loc )
{
	if ( loc != 0.0f )
		m.actors *= loc;
	return m;
}

} // namespace WwObjFade

#endif // CELLOBJFADE_H
