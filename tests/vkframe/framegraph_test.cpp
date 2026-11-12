// lane VKFRAME1 item 2 (11-12): the frame graph's pure rules (src/features/vkframe/vkframegraph.h).
// g++ -std=c++17 -I src tests/vkframe/framegraph_test.cpp; -DVKFRAMEGRAPH_RED feeds red = 1 to every rule and must fail.
// Last line: `FRAMEGRAPH OK` (rc 0) or `FRAMEGRAPH FAILED n` (rc 1).
#include "framepacer.h"
#include "features/vkframe/vkframegraph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef VKFRAMEGRAPH_RED
static const int R = 1;
#else
static const int R = 0;
#endif

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using namespace WwVkFrame;

	// route
	check( route( false, true, true, false ).side == Side::Gl, "no device -> GL" );
	check( route( true, false, true, false ).side == Side::Gl, "no GL import -> GL" );
	check( route( true, true, false, false ).side == Side::Gl, "pipelines not built -> GL" );
	check( route( true, true, true, true ).side == Side::Gl, "WW_VKPOST_RED -> GL" );
	check( route( true, true, true, false ).side == Side::Vk, "all present -> Vulkan" );

	// pipelines before first use
	check( !pipelinesUsable( 3, 2, R ), "2 of 3 pipelines built: not usable (no compile in a frame)" );
	check( pipelinesUsable( 3, 3, R ), "3 of 3 built: usable" );

	// present mode: IMMEDIATE, else MAILBOX, never FIFO
	check( pickPresentMode( { 2, 1, 0 }, false, R ) == 0, "IMMEDIATE first" );
	check( pickPresentMode( { 2, 1 }, false, R ) == 1, "MAILBOX without IMMEDIATE" );
	check( pickPresentMode( { 2, 3 }, false, R ) == -1, "only FIFO offered: GL keeps presenting" );
	check( pickPresentMode( { 2, 1, 0 }, true, R ) == 2, "vsync row asks FIFO" );

	// frames in flight: cap 2 from the pacer, red WW_FRAMEQUEUE_RED=1 = none
	const int cap = WwFramePacer::framesInFlight( R == 1 ? 1 : 0 );
	check( inFlightWait( 1, cap ) == 0 && inFlightWait( 2, cap ) == 0, "frames 1 and 2 record at once" );
	check( inFlightWait( 3, cap ) == 1 && inFlightWait( 10, cap ) == 8, "frame n waits for timeline n - 2" );
	{
		// a model GPU 3x slower than the CPU: the CPU is never more than 2 frames ahead
		std::uint64_t gpuDone = 0;
		double cpu = 0.0, gpuFree = 0.0;
		int ahead = 0;
		std::vector<double> doneAt;
		for ( std::uint64_t f = 1; f <= 64; f++ ) {
			const std::uint64_t w = inFlightWait( f, cap );
			if ( w )
				cpu = std::max( cpu, doneAt[w - 1] );
			cpu += 1.0;	// record + submit
			gpuFree = std::max( gpuFree, cpu ) + 3.0;
			doneAt.push_back( gpuFree );
			gpuDone = 0;
			for ( double d : doneAt )
				gpuDone += d <= cpu ? 1 : 0;
			ahead = std::max( ahead, int( f - gpuDone ) );
		}
		check( ahead <= 2, "model: CPU at most 2 frames ahead of a slow GPU" );
	}

	// timestamps
	check( std::fabs( ticksToMs( 1000, 2000, 1.0, 64 ) - 0.001 ) < 1e-12, "1000 ticks at 1 ns = 0.001 ms" );
	check( std::fabs( ticksToMs( 0xFFFFFFF0ull, 0x10ull, 1.0, 32 ) - 32e-6 ) < 1e-12, "32-bit wrap" );

	// barriers
	const Use gw { Layout::General, true, true }, gr { Layout::General, false, true }, sr { Layout::ShaderRead, false, true };
	check( between( gw, gw, R ).need, "write after write in General: barrier" );
	check( between( gw, gr, R ).need, "read after write: barrier" );
	check( between( gr, gw, R ).need, "write after read: barrier" );
	check( !between( gr, gr, R ).need, "read after read, same layout: none" );
	check( between( gr, sr, R ).need, "layout change: barrier" );

	// ledger and the `vk frame:` line
	Ledger l;
	l.add( "bloom-src", Side::Gl, 0.05 );
	l.add( "bloom", Side::Vk, 0.0865 );
	l.add( "tone-quads", Side::Gl, 0.15 );
	check( l.count( Side::Vk ) == 1 && l.count( Side::Gl ) == 2, "pass counts" );
	check( l.edges( Side::Gl, R ) == 1 && l.edges( Side::Vk, R ) == 1, "half-moved stage: two edges" );
	const std::string line = l.line( "vulkan", R );
	std::printf( "     %s\n", line.c_str() );
	check( line.find( "waits 2 (gl->vk 1, vk->gl 1)" ) != std::string::npos, "line counts the waits" );
	check( line.find( "vulkan 1 [bloom] gl 2" ) != std::string::npos, "line names the Vulkan passes" );

	// the speed bar (the item 3 numbers)
	check( withinBar( 0.0865, 0.0884 ) && withinBar( 0.0497, 0.0555 ), "bloom real_A / real_B within the bar" );
	check( !withinBar( 0.30, 0.15 ) && !withinBar( 10.6, 10.0 ), "0.15 ms slower / 6 % slower: over the bar" );

	if ( fails ) {
		std::printf( "FRAMEGRAPH FAILED %d\n", fails );
		return 1;
	}
	std::printf( "FRAMEGRAPH OK\n" );
	return 0;
}
