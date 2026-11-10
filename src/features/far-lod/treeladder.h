#ifndef TREELADDER_H
#define TREELADDER_H

/* lane PERFLOAD1 P7 (bungo 11-10, "we follow our doc"): a far tree's LOD step by its size on screen,
 * docs/FO4CS_IMPROVED_LOD_PLAN.md R3 items 4 and 5.
 *
 * Steps, fine to coarse: 0 the full mesh, 1-4 the base's MNAM slots rep[0..3], 5 the octahedral impostor (always last).
 * Size on screen: f = boundRadius x scale / distance x P11 / 2, a fraction of the screen HEIGHT, P11 = projection[1][1]
 * of the live view (1 / tan(fovY / 2)), never a fixed focal constant. thr[s] is the boundary between step s and s + 1:
 * a tree stays on an available step while f >= the boundary just before the next available coarser step, else it goes
 * coarser. Back to a finer step needs f >= that boundary x kHysteresis (20 %). Every switch is a cross-fade of
 * kFadeSeconds; the fade is dithered by the drawer with a per-frame pattern the TAA resolves.
 * Pure (no Qt, no GL): tests/features/far-lod/treeladder/ladder_gate.py twins it and reads its constants from here. */

#include <cmath>
#include <cstdint>

namespace TreeLadder
{
enum { Full = 0, Rep0 = 1, Impostor = 5, Steps = 6 };
constexpr float kHysteresis = 1.2f;
constexpr float kFadeSeconds = 0.35f;

//! boundaries full|rep0, rep0|rep1, rep1|rep2, rep2|rep3, rep3|impostor (the doc's ~1 %); the impostor has none
struct Thresholds
{
	float thr[Steps - 1] = { 0.12f, 0.06f, 0.03f, 0.02f, 0.01f };
};

inline float screenFraction( float radius, float distance, float p11 )
{
	if ( distance <= 1.0f )
		return 1.0f;
	return radius / distance * p11 * 0.5f;
}

//! the boundary that keeps step s (available in `avail`) against the next available coarser step; -1 = none
inline float keepBoundary( int s, uint8_t avail, const Thresholds & t )
{
	for ( int n = s + 1; n < Steps; n++ )
		if ( avail & ( 1u << n ) )
			return t.thr[n - 1];
	return -1.0f;
}

//! the finest available step whose keep boundary x factor f clears; the coarsest available when none; -1 = none available
inline int pick( float f, uint8_t avail, const Thresholds & t, float factor )
{
	int last = -1;
	for ( int s = 0; s < Steps; s++ ) {
		if ( !( avail & ( 1u << s ) ) )
			continue;
		last = s;
		const float b = keepBoundary( s, avail, t );
		if ( b < 0.0f || f >= b * factor )
			return s;
	}
	return last;
}

//! the step this frame: coarser at once, finer only past the boundary x kHysteresis
inline int select( float f, uint8_t avail, const Thresholds & t, int current )
{
	const int now = pick( f, avail, t, 1.0f );
	if ( current < 0 || current >= Steps || !( avail & ( 1u << current ) ) || now >= current )
		return now;
	const int finer = pick( f, avail, t, kHysteresis );
	return finer < current ? finer : current;
}

/*! lane PERFLOAD1 P8: the LOD preview's forced level (src/features/far-lod/lodpreview.h): `want` when available, else
 *  the nearest available step (the finer on a tie); -1 = none available */
inline int forced( int want, uint8_t avail )
{
	for ( int d = 0; d < Steps; d++ ) {
		if ( want - d >= 0 && want - d < Steps && ( avail & ( 1u << ( want - d ) ) ) )
			return want - d;
		if ( want + d < Steps && want + d >= 0 && ( avail & ( 1u << ( want + d ) ) ) )
			return want + d;
	}
	return -1;
}

//! one tree's step and its cross-fade (from -> step, t 0..1; t 1 = no fade in flight)
struct State
{
	int8_t step = -1, from = -1;
	float t = 1.0f;
};

inline bool fading( const State & s )
{
	return s.from >= 0 && s.t < 1.0f;
}

//! advance the fade by dt seconds, then take `want`; true when the step changed
inline bool advance( State & s, int want, float dt, float fadeSeconds = kFadeSeconds )
{
	if ( s.t < 1.0f )
		s.t = fadeSeconds > 0.0f ? std::fmin( 1.0f, s.t + dt / fadeSeconds ) : 1.0f;
	if ( !fading( s ) )
		s.from = -1;
	if ( want == s.step )
		return false;
	if ( s.step < 0 ) {	// first sight: no fade
		s.step = int8_t( want );
		s.from = -1;
		s.t = 1.0f;
		return true;
	}
	if ( fading( s ) && want == s.from ) {	// back mid-fade: the same fade runs the other way
		s.from = s.step;
		s.step = int8_t( want );
		s.t = 1.0f - s.t;
		return true;
	}
	s.from = s.step;
	s.step = int8_t( want );
	s.t = 0.0f;
	return true;
}

//! how much of step `which` shows, 0..1: the new step fades in with t, the old one out with 1 - t
inline float weight( const State & s, int which )
{
	if ( which == s.step )
		return fading( s ) ? s.t : 1.0f;
	if ( fading( s ) && which == s.from )
		return 1.0f - s.t;
	return 0.0f;
}

//! round 2: the steps a tree draws now (its step, and the old one mid-fade), one bit a step
inline uint8_t drawMask( const State & s )
{
	uint8_t m = 0;
	if ( s.step >= 0 && s.step < Steps )
		m |= uint8_t( 1u << s.step );
	if ( fading( s ) && s.from < Steps )
		m |= uint8_t( 1u << s.from );
	return m;
}

/*! round 2: a mesh row's dither for fo4_default.frag (0 = drawn whole). The row serves the steps in `mask` (one mesh
 *  can stand for several MNAM slots); `draw` is what the tree draws now. Mid-fade the finer side keeps noise < w
 *  (1 + w) and the coarser keeps noise >= 1 - w (-(1 + w), the card's rule), so the two sides cover every pixel once.
 *  A row whose steps the tree no longer draws keeps none (1) until the row filter drops it: nothing pops. */
inline float rowFade( const State & s, uint8_t draw, uint8_t mask )
{
	if ( !( mask & draw ) )
		return 1.0f;
	if ( !fading( s ) )
		return 0.0f;
	const bool hasTo = ( mask >> s.step ) & 1u, hasFrom = ( mask >> s.from ) & 1u;
	if ( hasTo == hasFrom )
		return 0.0f;	// one mesh for both steps: no fade
	const float w = hasTo ? s.t : 1.0f - s.t;
	const bool finer = hasTo ? s.step < s.from : s.from < s.step;
	return finer ? 1.0f + w : -( 1.0f + w );
}
}	// namespace TreeLadder

#endif
