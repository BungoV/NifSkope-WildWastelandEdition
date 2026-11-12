// lane EMPTY240 ITEM 0: the frame pacer's tick-rate test (no NifSkope, no GL).
// Part A (model): src/framepacer.h's rule on a model clock: frames a second for a frame cost, per cap; the red
//   (-DFRAMEPACER_RED) is build 71's route: a 6 ms coarse timer on a 15.625 ms system tick, and it must fail part A.
// Part B (this PC, Windows): the two mechanisms the routes use, measured: a 6 ms SetTimer (Qt's coarse QTimer) at
//   the current system tick, and a posted-message chain (QTimer::singleShot( 0 )) with 1.5 ms of work a frame.
//   g++ -O2 -std=c++17 -I src tests/framepacer/framepacer.cpp -o fp.exe -lwinmm   (tests/framepacer/run.sh)
#include "framepacer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

// stubs: framepacer.h declares the rows' functions; this test never calls them
bool wwVsyncOn() { return false; }
void wwSetVsync( bool ) {}
int wwFrameCapRow() { return 0; }
void wwSetFrameCap( int ) {}
double wwTimerResolutionMs() { return -1.0; }
bool wwPreciseSleep( double ) { return false; }

static int fails = 0;

static void check( bool ok, const char * what, double got, double lo, double hi )
{
	std::printf( "%s %-58s %8.1f fps (bar %.0f..%.0f)\n", ok ? "PASS" : "FAIL", what, got, lo, hi );
	if ( !ok )
		fails++;
}

/* The model event loop: a frame costs `cost` s; after it, the green rule queues the next one at once (posted event,
 * `post` s of loop latency) and waits out the cap. The red: frames start only on a 6 ms timer that the system
 * tick (`tick` s) rounds up. Returns frames a second over 10 s. */
static double modelFps( double cost, int row, double refresh, bool red, double tick, double post = 20e-6 )
{
	const double end = 10.0;
	double t = 0.0;
	int frames = 0;
	WwFramePacer::Clock clock;
	const int cap = WwFramePacer::capHz( row, refresh );
	if ( red ) {
		// SetTimer: interval clamped to 10 ms (USER_TIMER_MINIMUM), fired on the first tick at or after it is due
		const double interval = 0.010;
		double due = interval;
		while ( t < end ) {
			const double fire = std::ceil( due / tick - 1e-9 ) * tick;
			t = fire > t ? fire : t;
			t += cost;
			frames++;
			due = t + interval;	// a coarse timer restarts from the delivered tick (no catch-up)
		}
		return frames / end;
	}
	while ( t < end ) {
		double wait = WwFramePacer::waitSeconds( clock, cap, t );
		while ( wait > 0.0 ) {	// high-resolution sleeps (0.3 ms late, as measured under load), then re-posts
			const double s = WwFramePacer::sleepSeconds( wait );
			t += s > 0.0 ? s + 0.3e-3 : post;
			wait = WwFramePacer::waitSeconds( clock, cap, t );
		}
		WwFramePacer::started( clock, cap, t );
		t += cost + post;
		frames++;
	}
	return frames / end;
}

#ifdef _WIN32
static double nowS()
{
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency( &f );
	QueryPerformanceCounter( &c );
	return double( c.QuadPart ) / double( f.QuadPart );
}

static void spin( double s )
{
	const double t0 = nowS();
	while ( nowS() - t0 < s ) {
	}
}

// a 6 ms SetTimer for 2 s: ticks a second
static double timerRate( HWND w )
{
	MSG m;
	SetTimer( w, 1, 6, nullptr );
	int n = 0;
	const double t0 = nowS();
	while ( nowS() - t0 < 2.0 && GetMessageW( &m, nullptr, 0, 0 ) ) {
		if ( m.message == WM_TIMER )
			n++;
	}
	KillTimer( w, 1 );
	return n / ( nowS() - t0 );
}

// a posted-message chain with `work` s a frame for 2 s: frames a second
static double postedRate( HWND w, double work )
{
	MSG m;
	int n = 0;
	const double t0 = nowS();
	PostMessageW( w, WM_APP + 1, 0, 0 );
	while ( nowS() - t0 < 2.0 && GetMessageW( &m, nullptr, 0, 0 ) ) {
		if ( m.message == WM_APP + 1 ) {
			spin( work );
			n++;
			PostMessageW( w, WM_APP + 1, 0, 0 );
		}
	}
	return n / ( nowS() - t0 );
}

static double sysTickMs()
{
	using QueryFn = LONG ( WINAPI * )( PULONG, PULONG, PULONG );
	const QueryFn q = reinterpret_cast<QueryFn>( reinterpret_cast<void *>( GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "NtQueryTimerResolution" ) ) );
	ULONG a = 0, b = 0, c = 0;
	return ( q && q( &a, &b, &c ) == 0 ) ? c * 1e-4 : -1.0;
}
#endif

int main()
{
	const double cost = 1.5e-3;	// a 1.5 ms frame: 666 fps uncapped
	const double tick = 0.015625;
#ifdef FRAMEPACER_RED
	const bool red = true;
	std::printf( "RED build: build 71's 6 ms coarse timer route\n" );
#else
	const bool red = false;
#endif
	std::printf( "part A: model, frame cost %.1f ms, system tick %.3f ms\n", cost * 1e3, tick * 1e3 );
	check( modelFps( cost, 0, 60.0, red, tick ) >= 500.0, "cap off: 500+ fps on a 1.5 ms frame", modelFps( cost, 0, 60.0, red, tick ), 500, 1e9 );
	const double c144 = modelFps( cost, 144, 60.0, red, tick );
	check( std::fabs( c144 - 144.0 ) <= 2.0, "cap 144", c144, 142, 146 );
	const double c500 = modelFps( cost, 500, 60.0, red, tick );
	check( std::fabs( c500 - 500.0 ) <= 8.0, "cap 500 (a 1.5 ms frame reaches it)", c500, 492, 508 );
	const double cRef = modelFps( cost, -1, 165.0, red, tick );
	check( std::fabs( cRef - 165.0 ) <= 2.0, "cap = monitor refresh 165 Hz", cRef, 163, 167 );
	const double slow = modelFps( 9.0e-3, 0, 60.0, red, tick );
	check( slow >= 105.0, "cap off, a 9 ms frame: the frame sets the rate", slow, 105, 112 );
	const double redOld = modelFps( cost, 0, 60.0, true, tick );
	std::printf( "info build 71 route (the red's model) on a 1.5 ms frame: %.1f fps\n", redOld );
	check( redOld < 70.0, "build 71 route stays at the tick (the 68 fps reading)", redOld, 0, 70 );
	check( WwFramePacer::capHz( -1, 0.0 ) == 60 && WwFramePacer::capHz( 5, 0.0 ) == 10 && WwFramePacer::capHz( 5000, 0.0 ) == 1000
		&& WwFramePacer::capHz( 0, 144.0 ) == 0, "capHz rows (refresh unknown -> 60, clamps 10..1000)", 0, 0, 0 );
	{	// R2-1: a cell view draws continuously whatever moves (build 72 at empty240_b71: clock "stopped", 6 ms timer)
#ifdef FRAMEPACER_RED
		const int rule = 2;	// build 72's rule
#else
		const int rule = 0;
#endif
		check( WwFramePacer::wantsFrames( true, false, false, false, rule ), "a cell view with nothing moving and no live clock wants frames", 0, 0, 0 );
		check( WwFramePacer::wantsFrames( true, true, false, false, rule ), "a cell view with a live clock wants frames", 0, 0, 0 );
		check( !WwFramePacer::wantsFrames( false, true, false, false, rule ), "an editing view with nothing moving idles on the timer", 0, 0, 0 );
		check( WwFramePacer::wantsFrames( false, true, true, false, rule ) && WwFramePacer::wantsFrames( false, true, false, true, rule ),
			"an editing view wants frames while an animation plays or a key is held", 0, 0, 0 );
		check( !WwFramePacer::wantsFrames( false, false, true, true, rule ), "an editing view in a harness (clock stopped) idles", 0, 0, 0 );
		check( !WwFramePacer::wantsFrames( true, false, false, false, 2 ), "the red route (WW_FRAMETIMER_RED=2) is build 72's rule", 0, 0, 0 );
	}
	{	// R2-3: frames in flight capped at 2; the `frame gpu longest:` window after a jump
#ifdef FRAMEPACER_RED
		const int qred = 1;	// build 72: no cap
#else
		const int qred = 0;
#endif
		check( WwFramePacer::framesInFlight( qred ) == 2, "at most 2 frames queued ahead of the GPU", 0, 0, 0 );
		check( WwFramePacer::framesInFlight( 1 ) == 0, "the red route (WW_FRAMEQUEUE_RED=1) has no cap", 0, 0, 0 );
		check( WwFramePacer::GpuLongest::jumped( 5000.0, 0.0, 0.0, 4096.0 )
			&& !WwFramePacer::GpuLongest::jumped( 3000.0, 2000.0, 0.0, 4096.0 ), "a jump is a move past the reach", 0, 0, 0 );
		WwFramePacer::GpuLongest g;
		bool early = g.add( 5, 99.0 );	// no window open: ignored
		g.open( 100, 4 );
		early = early || g.add( 97, 900.0 );	// painted before the jump (read late): not counted
		early = early || g.add( 100, 12.0 ) || g.add( 101, 640.0 ) || g.add( 102, 30.0 );
		const bool closed = g.add( 103, 8.0 );
		check( !early && closed && g.frames == 4 && g.longestAt == 101 && g.longestMs == 640.0 && g.sumMs == 690.0,
			"the window holds the 4 frames from the jump and names the longest", 0, 0, 0 );
		check( !g.add( 104, 5000.0 ) && g.longestMs == 640.0, "a closed window takes nothing", 0, 0, 0 );
	}
	{	// item 1: the frame wall's parts add up to the wall (timed paints, an untimed one, a swap mark out of range)
		WwFrameWall::Window w;
		double t = 10.0;
		for ( int i = 0; i < 100; i++ ) {
			WwFrameWall::Marks m;
			m.start = t;
			if ( i % 10 != 3 ) {
				m.begin = t + 0.1e-3;
				m.end = t + 5.0e-3;
				m.passes = 2.0e-3;
				m.gi = 1.5e-3;
				m.restir = 0.3e-3;
				m.probe = 0.05e-3;
				m.timers = 0.2e-3;	// lane EMPTY240 R10-4
				m.transform = 0.1e-3;
			}
			m.finish = t + 5.1e-3;
			m.swapAt = i % 7 == 0 ? t - 1.0 : t + 6.0e-3;
			m.capWait = i % 2 ? 1.0e-3 : 0.0;
			const double next = t + 8.0e-3 + 1e-4 * ( i % 5 );
			WwFrameWall::add( w, m, next );
			t = next;
		}
		double sum = 0.0;
		for ( double p : w.part )
			sum += p;
		const double err = std::fabs( sum - w.wall ) / w.wall * 100.0;
#ifdef FRAMEPACER_RED
		const double redErr = std::fabs( ( sum - w.part[WwFrameWall::Between] ) - w.wall ) / w.wall * 100.0;	// a part left out
		check( redErr < 0.001, "frame wall: parts sum to the wall (red: between-paints left out)", redErr, 0, 0 );
#else
		check( err < 0.001 && w.frames == 100, "frame wall: parts sum to the wall (% off)", err, 0, 0 );
#endif
		std::printf( "info frame wall model: %.3f ms a frame, passes %.3f, gi %.3f, other-in-frame %.3f, swap %.3f, cap %.3f, between %.3f\n",
			w.wall * 10.0, w.part[WwFrameWall::Passes] * 10.0, w.part[WwFrameWall::GiFrame] * 10.0,
			w.part[WwFrameWall::OtherInFrame] * 10.0, w.part[WwFrameWall::Swap] * 10.0, w.part[WwFrameWall::CapWait] * 10.0,
			w.part[WwFrameWall::Between] * 10.0 );
	}
#ifdef _WIN32
	if ( !std::getenv( "FRAMEPACER_NO_PC" ) ) {
		std::printf( "part B: this PC (Windows timers)\n" );
		HWND w = CreateWindowExW( 0, L"STATIC", L"fp", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr );
		std::printf( "info system timer resolution now %.3f ms\n", sysTickMs() );
		const double t6 = timerRate( w );
		std::printf( "info 6 ms SetTimer (Qt's coarse timer) at that resolution: %.1f ticks a second\n", t6 );
		// the chain's own cost must be tiny; with 1.5 ms of spin a frame the rate also shows how busy this PC is
		const double chain0 = postedRate( w, 0.0 );
		check( chain0 >= 5000.0, "posted chain (singleShot 0) with no work: the route's own cost", chain0, 5000, 1e9 );
		std::printf( "info posted chain with 1.5 ms work a frame: %.1f fps (666 on an idle PC; less = other load)\n",
			postedRate( w, cost ) );
		HANDLE ht = CreateWaitableTimerExW( nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS );
		if ( ht ) {
			double sum = 0.0, worst = 0.0;
			for ( int i = 0; i < 50; i++ ) {
				LARGE_INTEGER due;
				due.QuadPart = -5000;	// 0.5 ms
				const double t0 = nowS();
				SetWaitableTimerEx( ht, &due, 0, nullptr, nullptr, nullptr, 0 );
				WaitForSingleObject( ht, 50 );
				const double got = ( nowS() - t0 ) * 1e3;
				sum += got;
				worst = got > worst ? got : worst;
			}
			std::printf( "info high-resolution 0.5 ms wait (the cap's sleep): mean %.3f ms, worst %.3f ms\n", sum / 50, worst );
			CloseHandle( ht );
		}
		timeBeginPeriod( 1 );
		std::printf( "info after timeBeginPeriod( 1 ): resolution %.3f ms, 6 ms SetTimer %.1f ticks a second (USER_TIMER_MINIMUM 10 ms)\n",
			sysTickMs(), timerRate( w ) );
		timeEndPeriod( 1 );
		DestroyWindow( w );
	}
#endif
	std::printf( "%s: %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
