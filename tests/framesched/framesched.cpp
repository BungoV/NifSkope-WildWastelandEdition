// lane FRAMESCHED1: src/gl/wwframesched.h. g++ -std=c++17 -I<tree>/src tests/framesched/framesched.cpp
// Red: -DFRAMESCHED_RED runs today's schedule (both captures every frame, on the cascade frames too) and must FAIL.
#include "gl/wwframesched.h"
#include "features/gi/rtddgimath.h"

#include <cstdio>

#ifdef FRAMESCHED_RED
static const bool kRed = true;
#else
static const bool kRed = false;
#endif

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

struct Run {
	int maxLoad = 0;      // the most periodic items in one frame (coarse step + captures)
	int camDone = -1, placedDone = -1;   // the frame the last face of each came in
	int captures = 0;
	unsigned long long deferred = 0, alternated = 0;
};

// cascades n, period every (0: no DDGI); camera re-stales all 6 faces at the frames in camMoves; placed faces
// arrive as (frame, count)
static Run simulate( int n, int every, int frames, const int * camMoves, int nCam, const int (*placedAdds)[2], int nPlaced )
{
	WwFrameSched::State st;
	Run run;
	int camLeft = 0, placedLeft = 0;
	for ( int f = 0; f < frames; f++ ) {
		for ( int i = 0; i < nCam; i++ )
			if ( camMoves[i] == f )
				camLeft = 6;
		for ( int i = 0; i < nPlaced; i++ )
			if ( placedAdds[i][0] == f )
				placedLeft += placedAdds[i][1];
		const unsigned long long ddgiFrame = (unsigned long long)( f + 1 );   // cascadesFrame's own count starts at 1
		bool coarse = false;
		for ( int l = 1; l < n && every > 0; l++ )
			coarse = coarse || rtddgi::cascadeDue( l, ddgiFrame, every );
		WwFrameSched::Want w;
		w.cam = camLeft > 0;
		w.placed = placedLeft > 0;
		w.ddgiCoarse = coarse;
		const WwFrameSched::Pick p = WwFrameSched::pick( w, st, kRed );
		int load = coarse ? 1 : 0;
		if ( p.cam ) {
			load++;
			run.captures++;
			if ( --camLeft == 0 )
				run.camDone = f;
		}
		if ( p.placed ) {
			load++;
			run.captures++;
			if ( --placedLeft == 0 )
				run.placedDone = f;
		}
		if ( load > run.maxLoad )
			run.maxLoad = load;
	}
	run.deferred = st.deferred;
	run.alternated = st.alternated;
	return run;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const int camMoves[] = { 0, 100, 220 };
	const int placed[][2] = { { 0, 54 }, { 150, 18 } };

	// the Medium..Ultra preset shape: 3 cascades every 4 (rtddgicell.cpp kPresetCascades / kPresetEvery)
	{
		const Run r = simulate( 3, 4, 400, camMoves, 3, placed, 2 );
		std::printf( "  cascades 3 every 4: max load %d, captures %d, deferred %llu, alternated %llu, cam done %d, placed done %d\n",
			r.maxLoad, r.captures, r.deferred, r.alternated, r.camDone, r.placedDone );
		check( r.maxLoad <= 1, "3x4: no frame carries two periodic items" );
		check( r.captures == 18 + 72, "3x4: every face captured" );
		// one capture a frame, a quarter of the frames lost to cascade 1: 72 placed + 6 camera faces in <= 110 frames
		check( r.placedDone >= 0 && r.placedDone < 150 + 40, "3x4: the placed probes finish within bound" );
		check( r.camDone >= 220 && r.camDone < 220 + 12, "3x4: the camera probe catches up within 12 frames of a move" );
	}
	// Low: 3 cascades every 8
	{
		const Run r = simulate( 3, 8, 400, camMoves, 3, placed, 2 );
		std::printf( "  cascades 3 every 8: max load %d, captures %d, cam done %d\n", r.maxLoad, r.captures, r.camDone );
		check( r.maxLoad <= 1, "3x8: no frame carries two periodic items" );
		check( r.captures == 90, "3x8: every face captured" );
	}
	// 4 cascades every 4 (Ultra): cascades 1..3 never share a frame either
	{
		const Run r = simulate( 4, 4, 400, camMoves, 3, placed, 2 );
		check( r.maxLoad <= 1, "4x4: no frame carries two periodic items" );
		check( r.captures == 90, "4x4: every face captured" );
	}
	// every 1: a coarse cascade every frame; the guard must not starve the captures
	{
		const Run r = simulate( 3, 1, 400, camMoves, 3, placed, 2 );
		std::printf( "  cascades 3 every 1: captures %d, deferred %llu\n", r.captures, r.deferred );
		check( r.captures == 90, "every 1: no starvation, every face captured" );
		// one capture in kMaxDefer + 1 frames, shared with the placed faces: 6 camera faces in <= 48 frames
		check( r.camDone >= 220 && r.camDone < 220 + 8 * ( WwFrameSched::kMaxDefer + 1 ) * 6 / 4 + 12, "every 1: the camera probe still catches up" );
	}
	// no DDGI: no deferral, the two kinds take turns
	{
		const Run r = simulate( 1, 0, 400, camMoves, 3, placed, 2 );
		check( r.maxLoad <= 1, "no DDGI: one capture a frame" );
		check( r.deferred == 0, "no DDGI: nothing deferred" );
		check( r.alternated > 0, "no DDGI: camera and placed took turns" );
		check( r.captures == 90, "no DDGI: every face captured" );
	}
	// a still: nothing waits, nothing is picked
	{
		WwFrameSched::State st;
		WwFrameSched::Want w;
		w.ddgiCoarse = true;
		const WwFrameSched::Pick p = WwFrameSched::pick( w, st, kRed );
		check( !p.cam && !p.placed && st.deferred == 0, "idle: no capture, no census" );
	}
	std::printf( fails ? "FRAMESCHED: %d FAIL\n" : "FRAMESCHED: all ok\n", fails );
	return fails ? 1 : 0;
}
