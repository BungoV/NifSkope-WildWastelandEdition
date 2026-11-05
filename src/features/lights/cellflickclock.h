#pragma once
/* lane PERFFIX2 a (item 6): the light flicker's phase on the world clock (pure, no Qt; tests/features/lights/flickclock).
 *
 * LIGHTFLICKER1 matched the game's per-frame flicker / pulse update (celllights.cpp wwCellFlickerFrame): each frame
 * moves every phase by dt x Period (flicker: times a fresh xorshift random per phase). It read dt from the wall clock
 * on every PAINTED frame, so two harness rows at the same pinned time drew different lights (RTXGI1 M3: 1.67 M px).
 * Here the phase is a function of the world clock instead (wwCellWorldSeconds: the wall clock in a live window, the
 * pinned WW_CELL_WORLDANIM_TIME, 0 when unset, under any harness): fixed 1/60 s steps from t 0, so the same time gives
 * the same lights whatever frames came before. At 60 fps a live window draws what the per-frame update drew.
 * The red (WW_CELL_FLICKER_CLOCK_RED=1) keeps the old per-frame step (stepFrame). */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace WwFlick
{
constexpr double kStepHz = 60.0;	// world-clock steps a second
constexpr float kTwoPi = 6.2831855f;
constexpr long long kLiveCatchUp = 15;	// a running clock skips ahead past 0.25 s (the old dt clamp): an idle window

struct Phase
{
	float p[3] = { 1.0f, 0.0f, 0.0f };
	std::uint32_t rng = 0;
	long long n = -1;	// world-clock steps advanced; -1 = not on the clock (fresh, or the red stepped it)
};

//! 512-entry sin / cos tables indexed int(x 512 / 2 pi) & 511 (the game's)
inline float tab( float x, bool cosine )
{
	static const std::vector<float> t = [] {
		std::vector<float> v( 1024 );
		for ( int i = 0; i < 512; i++ ) {
			v[size_t( i )] = float( std::sin( double( i ) * 6.283185307179586 / 512.0 ) );
			v[size_t( 512 + i )] = float( std::cos( double( i ) * 6.283185307179586 / 512.0 ) );
		}
		return v;
	}();
	return t[size_t( ( int( x * ( 512.0f / 6.2831855f ) ) & 511 ) + ( cosine ? 512 : 0 ) )];
}

//! a random in [lo, hi); xorshift so a gate's frames repeat
inline float rnd( std::uint32_t & r, float lo, float hi )
{
	r ^= r << 13;
	r ^= r >> 17;
	r ^= r << 5;
	return lo + ( hi - lo ) * float( r >> 8 ) * ( 1.0f / 16777216.0f );
}

inline std::uint32_t seed( std::uint32_t ref, int i )
{
	return 0x9E3779B9u ^ ( ref * 2654435761u ) ^ std::uint32_t( i + 1 );
}

//! one update of s = dt x Period (kind 1 flicker, else pulse)
inline void advance( Phase & ph, int kind, float sp )
{
	if ( kind == 1 ) {
		ph.p[0] += rnd( ph.rng, 1.1f, 13.1f ) * sp;
		ph.p[1] += rnd( ph.rng, 1.2f, 13.2f ) * sp;
		ph.p[2] += rnd( ph.rng, 1.3f, 19.3f ) * sp;
		for ( float & p : ph.p )
			if ( p > kTwoPi )
				p -= kTwoPi;
	} else {
		ph.p[0] += sp;
		if ( ph.p[0] > kTwoPi )
			ph.p[0] -= kTwoPi;
	}
}

//! the value v in [-1, 1] and the local offset (Movement m) from the phases
inline float eval( const Phase & ph, int kind, float m, float local[3] )
{
	if ( kind == 1 ) {
		const float q0 = ph.p[0] + 1.7f, q1 = ph.p[1] + 0.5f, q2 = ph.p[2] + 0.3f;
		const float s0 = tab( q0, false ), s1 = tab( q1, false );
		float o = m * s0 * s1 * 0.5f;
		if ( !( o + m > 0.0f ) )
			o = 0.0f;
		local[0] = o * s0;
		local[1] = o * s1;
		local[2] = o * tab( q2, false );
		const float v = ( tab( q1 * 1.3f, false ) + 1.0f ) * 0.5f * ( tab( q0 * 1.1f, false ) + 1.0f ) * 0.5f * ( 1.0f / 3.0f )
			+ tab( q2 * 3.0f, false ) * 0.2f;
		return std::clamp( v, -1.0f, 1.0f );
	}
	const float c = tab( ph.p[0], true ), n = tab( ph.p[0], false );
	local[0] = m * c;
	local[1] = m * n;
	local[2] = m * n * c;
	return c;
}

//! the red / WW_CELL_FLICKER_STEP: one step of this frame's dt (the old update; leaves the world clock)
inline void stepFrame( Phase & ph, std::uint32_t seedv, int kind, float period, float dt )
{
	if ( !ph.rng )
		ph.rng = seedv;
	ph.n = -1;
	advance( ph, kind, dt * period );
}

//! green: the phase at world time T, whatever frames came before. A clock that went back starts over from t 0;
//! catchUp >= 0 (a running wall clock) skips all but that many steps; -1 (a pinned clock) is exact at any T.
//! Returns the steps taken.
inline long long toTime( Phase & ph, std::uint32_t seedv, int kind, float period, double T, long long catchUp )
{
	const long long N = (long long)std::floor( std::max( T, 0.0 ) * kStepHz );
	if ( ph.n < 0 || N < ph.n ) {
		ph = Phase {};
		ph.rng = seedv;
		ph.n = 0;
	}
	if ( catchUp >= 0 && N - ph.n > catchUp )
		ph.n = N - catchUp;
	const float sp = float( 1.0 / kStepHz ) * period;
	const long long took = N - ph.n;
	for ( ; ph.n < N; ph.n++ )
		advance( ph, kind, sp );
	return took;
}
}	// namespace WwFlick
