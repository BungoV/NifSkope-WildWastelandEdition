// lane PERFFIX2 a (item 6): the light flicker on the world clock (src/gl/cellflickclock.h).
// Build: g++ -std=c++17 -O1 -I<tree>/src tests/flickclock/flickclock.cpp -o flickclock.exe
// Red:   the same with -DFLICK_RED (the old per-frame wall-clock step stands in for the clock): must exit non-zero.
#include "gl/cellflickclock.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
int fails = 0, checks = 0;
void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s\n", what );
	} else {
		std::printf( "ok   %s\n", what );
	}
}

// the pre-lane copies (celllights.cpp b0e5e9a3 flickTab / flickRand) to prove the move kept them
float origTab( float x, bool cosine )
{
	static std::vector<float> tab;
	if ( tab.empty() ) {
		tab.resize( 1024 );
		for ( int i = 0; i < 512; i++ ) {
			tab[size_t( i )] = float( std::sin( double( i ) * 6.283185307179586 / 512.0 ) );
			tab[size_t( 512 + i )] = float( std::cos( double( i ) * 6.283185307179586 / 512.0 ) );
		}
	}
	return tab[size_t( ( int( x * ( 512.0f / 6.2831855f ) ) & 511 ) + ( cosine ? 512 : 0 ) )];
}
float origRand( std::uint32_t & r, float lo, float hi )
{
	r ^= r << 13;
	r ^= r >> 17;
	r ^= r << 5;
	return lo + ( hi - lo ) * float( r >> 8 ) * ( 1.0f / 16777216.0f );
}

struct Out
{
	float v, local[3], p[3];
	bool operator==( const Out & o ) const { return std::memcmp( this, &o, sizeof( Out ) ) == 0; }
};

// what a light shows at the end of a run of painted frames (dts), the clock reading the sum
Out run( const float * dts, int frames, int kind, float period, float m, std::uint32_t sd, WwFlick::Phase * keep = nullptr )
{
	WwFlick::Phase ph = keep ? *keep : WwFlick::Phase {};
	double T = 0.0;
	for ( int f = 0; f < frames; f++ ) {
		T += double( dts[f] );
#ifdef FLICK_RED
		WwFlick::stepFrame( ph, sd, kind, period, dts[f] );
#else
		WwFlick::toTime( ph, sd, kind, period, T, -1 );
#endif
	}
	if ( keep )
		*keep = ph;
	Out o {};
	o.v = WwFlick::eval( ph, kind, m, o.local );
	std::memcpy( o.p, ph.p, sizeof( o.p ) );
	return o;
}
}	// namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// 1. the moved helpers equal the old ones
	{
		bool same = true;
		for ( int i = -4000; i < 40000; i++ ) {
			const float x = float( i ) * 0.00731f;
			same = same && WwFlick::tab( x, false ) == origTab( x, false ) && WwFlick::tab( x, true ) == origTab( x, true );
		}
		std::uint32_t a = 0x1234567u, b = a;
		for ( int i = 0; i < 100000; i++ )
			same = same && WwFlick::rnd( a, 1.1f, 13.1f ) == origRand( b, 1.1f, 13.1f );
		check( same, "tables and random equal the pre-lane copies" );
	}
	// frame runs that all reach T = 3.7583 s (225.5 world steps: no rounding at a step edge) by different painted frames
	const double TT = 3.7583;
	auto frames = []( double TT, float lo, float hi, std::uint32_t r ) {
		std::vector<float> f;
		double T = 0.0;
		while ( T < TT - 0.05 ) {
			const float dt = lo == hi ? lo : WwFlick::rnd( r, lo, hi );
			f.push_back( dt );
			T += double( dt );
		}
		f.push_back( float( TT - T ) );
		return f;
	};
	const std::vector<float> f60 = frames( TT, 1.0f / 60.0f, 1.0f / 60.0f, 1u ), f30 = frames( TT, 1.0f / 30.0f, 1.0f / 30.0f, 1u ),
		fj = frames( TT, 0.004f, 0.045f, 77u );	// fj: a busy live hold, 4-45 ms frames
	struct Light { int kind; float period, m; std::uint32_t ref; };
	const Light lights[] = { { 1, 1.0f, 8.0f, 0x0001A2B3u }, { 1, 2.7f, 0.0f, 0x00C0FFEEu }, { 2, 0.6f, 4.0f, 0x00051234u }, { 4, 1.9f, 2.0f, 0x00999999u } };
	// 2. the same time draws the same light, whatever frames reached it (the row-repeatability defect)
	{
		bool same = true;
		for ( const Light & l : lights ) {
			const std::uint32_t sd = WwFlick::seed( l.ref, 3 );
			const Out a = run( f60.data(), int( f60.size() ), l.kind, l.period, l.m, sd );
			const Out b = run( f30.data(), int( f30.size() ), l.kind, l.period, l.m, sd );
			const Out c = run( fj.data(), int( fj.size() ), l.kind, l.period, l.m, sd );
			same = same && a == b && a == c;
		}
		check( same, "T 3.7583 s: 60 fps == 30 fps == jittered 4-45 ms frames, every light kind" );
	}
	// 3. two rows at one pinned time repeat even when another row (a later time) ran between them
	{
		bool same = true;
		for ( const Light & l : lights ) {
			const std::uint32_t sd = WwFlick::seed( l.ref, 3 );
			WwFlick::Phase ph;
			const float one[1] = { 2.5f };
			const Out a = run( one, 1, l.kind, l.period, l.m, sd, &ph );	// row 1 at t 2.5
			WwFlick::Phase ph2 = ph;
#ifdef FLICK_RED
			WwFlick::stepFrame( ph2, sd, l.kind, l.period, 1.0f / 60.0f );	// one more painted frame of the same row
#else
			WwFlick::toTime( ph2, sd, l.kind, l.period, 2.5, -1 );
#endif
			Out a2 {};
			a2.v = WwFlick::eval( ph2, l.kind, l.m, a2.local );
			std::memcpy( a2.p, ph2.p, sizeof( a2.p ) );
#ifndef FLICK_RED
			WwFlick::toTime( ph2, sd, l.kind, l.period, 7.0, -1 );	// row 2 at t 7
			WwFlick::toTime( ph2, sd, l.kind, l.period, 2.5, -1 );	// row 3 back at t 2.5
#else
			WwFlick::stepFrame( ph2, sd, l.kind, l.period, 0.016f );
#endif
			Out a3 {};
			a3.v = WwFlick::eval( ph2, l.kind, l.m, a3.local );
			std::memcpy( a3.p, ph2.p, sizeof( a3.p ) );
			same = same && a == a2 && a == a3;
		}
		check( same, "pinned t 2.5: a second paint and a row after a t 7 row draw the first row's light" );
	}
	// 4. at 60 fps the clock IS the game's per-frame update (dt 1/60 every frame): frame by frame, 900 frames
	{
		bool same = true;
		for ( const Light & l : lights ) {
			const std::uint32_t sd = WwFlick::seed( l.ref, 1 );
			WwFlick::Phase g, o;
			for ( int f = 1; f <= 900 && same; f++ ) {
				WwFlick::toTime( g, sd, l.kind, l.period, double( f ) / 60.0 + 1e-9, -1 );
				WwFlick::stepFrame( o, sd, l.kind, l.period, 1.0f / 60.0f );
				same = std::memcmp( g.p, o.p, sizeof( g.p ) ) == 0 && g.rng == o.rng;
			}
		}
		check( same, "60 fps: clock phases == the per-frame update's, 900 frames" );
	}
	// 5. the clock moves the light (a frozen flicker would pass 2-3 too) and t 0 is the start phase
	{
		const Light & l = lights[0];
		const std::uint32_t sd = WwFlick::seed( l.ref, 3 );
		WwFlick::Phase a, b;
		WwFlick::toTime( a, sd, l.kind, l.period, 0.0, -1 );
		WwFlick::toTime( b, sd, l.kind, l.period, 1.37, -1 );
		float la[3], lb[3];
		const float va = WwFlick::eval( a, l.kind, l.m, la ), vb = WwFlick::eval( b, l.kind, l.m, lb );
		check( a.p[0] == 1.0f && a.p[1] == 0.0f && a.p[2] == 0.0f && va != vb, "t 0 = the start phase (1, 0, 0); t 1.37 differs" );
	}
	// 6. a running wall clock left idle catches up at most 15 steps (no stall after an hour away)
	{
		WwFlick::Phase ph;
		WwFlick::toTime( ph, 5u, 1, 1.0f, 1.0, WwFlick::kLiveCatchUp );
		const long long took = WwFlick::toTime( ph, 5u, 1, 1.0f, 3600.0, WwFlick::kLiveCatchUp );
		check( took == WwFlick::kLiveCatchUp && ph.n == 216000, "live catch-up after an hour: 15 steps, n 216000" );
	}
	std::printf( "%d checks, %d failures\n", checks, fails );
	return fails ? 1 : 0;
}
