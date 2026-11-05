// lane PERFFIX3 (item 4): the cloud shadow map's time-sliced rebuild (src/features/clouds/cloudshadowslice.h), driven the way
// cloudshadow.cpp drives it, over a model of the two maps' rows (which fit each row was built under).
//   g++ -O2 -std=c++17 -I src tests/features/clouds/cloudshadowslice/cloudshadowslice_test.cpp -o x.exe   (red: -DCLOUDSHADOWSLICE_RED,
//   the CLOUD3 rule: a moved fit rebuilds the whole map that frame)
// Checks:
//  W  whole: every frame, every row of the map the receivers read was built under ONE fit, the fit they are given
//  C  cost: after the first build, no frame builds more than size/16 rows unless a HARD change asked (size, mode...)
//  L  lag: a camera that stops moving has the front on its last fit within 2 x 16 + 1 frames
//  S  still: with no move, the frames are CLOUD3's exactly (a full first build, then size/16 refresh rows in order)
//  H  a hard change rebuilds the front whole at once, a back under way is dropped
// The red must fail C.
#include "features/clouds/cloudshadowslice.h"

#include <cstdio>
#include <vector>

namespace SL = WwCloudShadowSlice;
static int g_fail = 0, g_checks = 0;

static void check( bool ok, const char * what )
{
	g_checks++;
	if ( !ok )
		g_fail++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
}

#ifdef CLOUDSHADOWSLICE_RED
constexpr bool kRed = true;
#else
constexpr bool kRed = false;
#endif

struct Sim
{
	int n, k;
	SL::State s;
	std::vector<int> front, back;	// per row: the fit id it was built under (-1 none)
	int frontFit = -1, backFit = -1;	// the fit the receivers are given / the back's
	int maxRows = 0;	// the most rows one non-hard frame built after the first build
	int frames = 0;
	bool whole = true;
	Sim( int n_ ) : n( n_ ), k( n_ / 16 ), front( n_, -1 ), back( n_, -1 ) {}

	void frame( int fitNow, bool hard )
	{
		const bool moved = fitNow != frontFit;
		const SL::Step st = SL::plan( s, hard, moved, n, k, kRed );
		if ( st.fullNow )
			frontFit = fitNow;
		if ( st.startBack )
			backFit = fitNow;
		std::vector<int> & m = st.into == SL::Into::Back ? back : front;
		const int f = st.into == SL::Into::Back ? backFit : frontFit;
		for ( int r = st.row0; r < st.row0 + st.rows; r++ )
			m[r] = f;
		if ( st.swap ) {
			std::swap( front, back );
			frontFit = backFit;
		}
		if ( frames > 0 && !hard )
			maxRows = std::max( maxRows, st.rows );
		for ( int r = 0; r < n; r++ )
			whole = whole && front[r] == frontFit;
		frames++;
	}
};

int main()
{
	const int n = 1024;
	// a walk: the fit snaps every 5 frames for 60 frames (a fast fly), then stands still
	{
		Sim sim( n );
		int fit = 0;
		for ( int f = 0; f < 60; f++ ) {
			if ( f % 5 == 0 )
				fit++;
			sim.frame( fit, false );
		}
		int lag = 0;
		while ( sim.frontFit != fit && lag < 200 ) {
			sim.frame( fit, false );
			lag++;
		}
		std::printf( "  walk: most rows in a frame after the first %d (size/16 = %d), front on the last fit %d frames after the stop\n",
			sim.maxRows, n / 16, lag );
		check( sim.whole, "W: every frame, the receivers' map is whole under the fit they are given" );
		check( sim.maxRows <= n / 16, "C: no moved-fit frame builds more than size/16 rows" );
		check( lag <= 2 * 16 + 1, "L: the front reaches the last fit within 2 x 16 + 1 frames of the stop" );
	}
	// still: CLOUD3's sequence exactly
	{
		SL::State s;
		bool ok = true;
		int cursor = 0;
		for ( int f = 0; f < 70; f++ ) {
			const SL::Step st = SL::plan( s, false, false, n, n / 16, kRed );
			const int rows = f == 0 ? n : n / 16, row0 = f == 0 ? 0 : cursor;	// cloudshadow.cpp at 4c9ea88e
			const int rowN = std::min( rows, n - row0 );
			ok = ok && st.into == SL::Into::Front && st.row0 == row0 && st.rows == rowN && st.fullNow == ( f == 0 );
			cursor = f == 0 ? 0 : ( row0 + rowN >= n ? 0 : row0 + rowN );
		}
		check( ok, "S: a still camera builds what CLOUD3 built, frame for frame (first whole, then size/16 rows in order)" );
	}
	// hard: drops a back under way, rebuilds whole
	{
		Sim sim( n );
		sim.frame( 1, false );
		sim.frame( 2, false );	// a back starts
		sim.frame( 2, false );
		const bool backWas = sim.s.backActive;
		SL::State before = sim.s;
		const SL::Step st = SL::plan( sim.s, true, true, n, n / 16, kRed );
		check( ( kRed || backWas ) && st.fullNow && st.rows == n && !sim.s.backActive && before.frontValid,
			"H: a hard change rebuilds the front whole at once and drops the back" );
	}
	std::printf( "CLOUDSHADOWSLICE %d checks, %d failures: %s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}
