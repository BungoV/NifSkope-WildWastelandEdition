// lane VKCOMPUTE1: unit gate of src/features/gi/vkasyncrule.h (the async family pick and GL's wait placement).
// tests/vkasync/run.sh builds it green and with each -DVKASYNC_RED_<NAME>; each red must fail its own check.
// The timeline is a two-queue model of one frame at empty240 (build 72 medians: GL passes, DDGI 1.87 ms, ReSTIR 0.39 ms
// on the device); it proves where the waits sit and how much GL work runs beside the device, not live ms.
#include "features/gi/vkasyncrule.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

static int fails = 0;
static void check( bool ok, const char * name, const char * what )
{
	if ( !ok ) {
		std::printf( "FAIL %s: %s\n", name, what );
		fails++;
	}
}

// --- pick ---------------------------------------------------------------------------------------------------------
static void pickChecks()
{
	using namespace vkasync;
	// RTX 5070 Ti (tools/qfprobe, 11-12): 0 G|C|T ts64, 1 T, 2 C|T ts64, 3-5 T
	const uint32_t nvF[6] = { 0x7, 0x4, 0x6, 0x4, 0x4, 0x4 }, nvT[6] = { 64, 0, 64, 0, 0, 0 };
	Pick p = pickFamily( nvF, nvT, 6, true, false );
	check( p.family == 2 && p.why == Why::Async, "pick", "the 5070 Ti takes compute-only family 2" );
	p = pickFamily( nvF, nvT, 6, true, true );
	check( p.family == 0 && p.why == Why::Red, "pick", "the red takes family 0 (the old device)" );
	p = pickFamily( nvF, nvT, 6, false, false );
	check( p.family == 0 && p.why == Why::Off, "pick", "the row off takes family 0" );
	// a compute-only family without timestamps is not taken (the device ms lines need them)
	const uint32_t tsF[3] = { 0x7, 0x6, 0x4 }, tsT[3] = { 64, 0, 0 };
	p = pickFamily( tsF, tsT, 3, true, false );
	check( p.family == 0 && p.why == Why::NoAsyncFamily, "ts", "no timestamps on the compute-only family: family 0" );
	// a device whose only compute family is the graphics one
	const uint32_t oneF[2] = { 0x7, 0x4 }, oneT[2] = { 64, 0 };
	p = pickFamily( oneF, oneT, 2, true, false );
	check( p.family == 0 && p.why == Why::NoAsyncFamily, "pick", "no compute-only family: family 0" );
	const uint32_t noneF[1] = { 0x4 }, noneT[1] = { 0 };
	p = pickFamily( noneF, noneT, 1, true, false );
	check( p.family < 0, "pick", "no compute family at all: none" );
}

// --- timeline -----------------------------------------------------------------------------------------------------
struct Sim
{
	double gl = 0, dev = 0;          // the time each queue is free
	double ddgiDone = 0, restirDone = 0;
	double overlapMs = 0;            // GL work run while a device submit was in flight
	bool readBad = false;
	vkasync::Ledger ddgi, restir;
	void pass( double ms )
	{
		const double busyTill = std::max( ddgiDone, restirDone );
		overlapMs += std::max( 0.0, std::min( gl + ms, busyTill ) - gl );
		gl += ms;
	}
	void read( const vkasync::Ledger & L ) { readBad = readBad || !L.readable(); }
};

// one frame; combined = the cascades' copy (read last frame's at the tick), else the single volume read at opaque
static double frame( Sim & s, bool defer, bool combined )
{
	const double t0 = s.gl;
	auto waitDdgi = [&s]() { s.gl = std::max( s.gl, s.ddgiDone ); };
	auto waitRestir = [&s]() { s.gl = std::max( s.gl, s.restirDone ); };
	s.pass( 0.5 );                               // shadows, flicker, sky probe tick
	// rtddgiCellFrame: last frame's result settled, copied, then this frame's submit
	s.ddgi.settle( waitDdgi );
	if ( combined ) {
		s.read( s.ddgi );
		s.pass( 0.01 );                          // the copies into the one texture buffer
	}
	s.restir.settle( waitRestir );               // the ReSTIR pair settles before GL's next signal on the device
	s.dev = std::max( s.dev, s.gl ) + 1.87;      // the submit waits glDone (signalled now), the device runs
	s.ddgiDone = s.dev;
	s.ddgi.afterSubmit( defer, waitDdgi );
	s.pass( 0.1 );                               // sky
	s.pass( 0.1 );                               // obscurance
	// rtddgiRestirPass: GL writes the g-buffer copy (after any owed wait), signals, submits
	s.restir.settle( waitRestir );
	s.pass( 0.02 );
	s.dev = std::max( s.dev, s.gl ) + 0.39;
	s.restirDone = s.dev;
	s.restir.afterSubmit( defer, waitRestir );
	s.pass( 2.0 );                               // water, SSR, contact, SSGI
	// the opaque pass's program setup: the readers (rtddgiCellUniforms -> restirUniforms, bindOne)
	s.restir.settle( waitRestir );
	s.read( s.restir );
	if ( !combined ) {
		s.ddgi.settle( waitDdgi );
		s.read( s.ddgi );
	}
	s.pass( 1.0 );                               // opaque
	s.pass( 0.76 + 0.38 + 0.4 );                 // tone map, TAA, overlays
	return s.gl - t0;
}

static void timelineChecks()
{
	for ( int combined = 0; combined < 2; combined++ ) {
		Sim a, b;
		double fa = 0, fb = 0;
		for ( int f = 0; f < 32; f++ ) {
			fa = frame( a, true, combined != 0 );
			fb = frame( b, false, combined != 0 );
		}
		// close: the last owed waits settle (a drop or the next frame)
		auto wa = [&a]() { a.gl = std::max( a.gl, std::max( a.ddgiDone, a.restirDone ) ); };
		a.ddgi.settle( wa );
		a.restir.settle( wa );
		std::printf( "%s: frame async %.3f ms (GL beside the device %.2f ms a frame) | old order %.3f ms (%.2f)\n",
			combined ? "cascades (last frame's copy)" : "one volume (read at opaque)", fa, a.overlapMs / 32, fb, b.overlapMs / 32 );
		check( a.ddgi.submits == a.ddgi.waits && a.restir.submits == a.restir.waits, "balance", "one GL wait per submit" );
		check( b.ddgi.submits == b.ddgi.waits && b.restir.submits == b.restir.waits, "balance", "one GL wait per submit (old)" );
		check( !a.readBad && !b.readBad, "readsafe", "every GL read after the wait on the submit that wrote it" );
		check( fa + 1.0 < fb, "overlap", "the async order takes at least 1 ms off the frame (DDGI runs beside GL's passes)" );
		check( b.overlapMs < 1e-9, "old", "the old order runs no GL work beside the device" );
	}
}

int main()
{
	pickChecks();
	timelineChecks();
	if ( fails ) {
		std::printf( "VKASYNC FAIL %d\n", fails );
		return 1;
	}
	std::printf( "VKASYNC PASS 6\n" );
	return 0;
}
