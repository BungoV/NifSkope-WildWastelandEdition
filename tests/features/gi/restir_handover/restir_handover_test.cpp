// Lane AIRPERF1 round 2: the ReSTIR hand-over rule (src/features/gi/rtddgi/restirhandover.h) on a frame timeline.
// The interop submit starts on the device only after the GPU has run the GL commands queued before the signal
// (the backlog), then runs the kernel. A CPU wait on its fence costs backlog + kernel. Build:
//   g++ -O2 -std=c++17 -I src tests/features/gi/restir_handover/restir_handover_test.cpp -static   (red: -DRESTIR_HANDOVER_RED)
#include "features/gi/rtddgi/restirhandover.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace
{

struct Run
{
	double waitMean = 0, waitMax = 0;
	int late = 0, overlaps = 0, pending = 0, ageMax = 0, submits = 0;
};

uint32_t hash( uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

//! frames of `frameMs` CPU work; at each hand-over the GPU is backlog [lo, hi] ms behind; the kernel takes `kernelMs`
Run simulate( int frames, double frameMs, double lo, double hi, double kernelMs, bool wait )
{
	Run r;
	double now = 0, doneAt = -1.0, waitSum = 0;
	int age = 0;
	for ( int f = 0; f < frames; f++ ) {
		now += frameMs;   // the frame's own CPU work up to the hand-over
		const double backlog = lo + ( hi - lo ) * double( hash( uint32_t( f ) * 2654435761U ) & 0xffff ) / 65535.0;
		double waited = 0;
		const bool lastDone = doneAt <= now;
		const restirhand::Act act = restirhand::before( lastDone, wait );
		if ( act == restirhand::Late ) {
			r.late++;
			age++;
			r.ageMax = std::max( r.ageMax, age );
			continue;
		}
		if ( act == restirhand::Block ) {
			waited += doneAt - now;
			now = doneAt;
		}
		if ( doneAt > now )
			r.overlaps++;   // a submit while the last one still runs: the parameters would be overwritten in use
		doneAt = now + backlog + kernelMs;
		r.submits++;
		age = 0;
		if ( restirhand::after( wait ) ) {
			waited += doneAt - now;
			now = doneAt;
		} else {
			r.pending++;
		}
		waitSum += waited;
		r.waitMax = std::max( r.waitMax, waited );
	}
	r.waitMean = waitSum / double( frames );
	return r;
}

int g_fail = 0;
void check( const char * name, bool ok, const char * fmt, double a, double b )
{
	char line[256];
	std::snprintf( line, sizeof line, fmt, a, b );
	std::printf( "%s %s: %s\n", ok ? "ok" : "FAIL", name, line );
	g_fail += ok ? 0 : 1;
}

} // namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// B1-B3: the flight's numbers (backlog 18-40 ms, kernel 1.5 ms), the GL frame does not wait
	const Run a = simulate( 600, 60.0, 18.0, 40.0, 1.5, false );
	check( "framewait", a.waitMean <= 0.05 && a.waitMax <= 0.05, "CPU wait mean %.3f ms, max %.3f ms (bar 0.05)", a.waitMean, a.waitMax );
	check( "oneinflight", a.overlaps == 0, "overlapping submits %.0f of %.0f", double( a.overlaps ), double( a.submits ) );
	check( "ontime", a.late == 0, "late frames %.0f of %.0f", double( a.late ), 600.0 );
	// B4: the GPU far behind (backlog 150 ms over 60 ms frames): still no wait, some late frames, a bounded age
	const Run b = simulate( 600, 60.0, 150.0, 150.0, 1.5, false );
	check( "behind", b.waitMax <= 0.05 && b.late > 0 && b.ageMax <= 4 && b.overlaps == 0, "CPU wait max %.3f ms, ratio age max %.0f frames (bars 0.05, 4)",
		b.waitMax, double( b.ageMax ) );
	// B5: a waiting caller (the CLI, a read-back) gets its own submit back every step
	const Run c = simulate( 600, 60.0, 18.0, 40.0, 1.5, true );
	check( "waiter", c.pending == 0 && c.late == 0 && c.waitMean >= 19.5, "pending %.0f, CPU wait mean %.3f ms", double( c.pending ), c.waitMean );
	if ( g_fail ) {
		std::printf( "RESTIR_HANDOVER FAILED %d\n", g_fail );
		return 1;
	}
	std::printf( "RESTIR_HANDOVER PASS 5 (late with the GPU behind: %d of 600 frames, age max %d)\n", b.late, b.ageMax );
	return 0;
}
