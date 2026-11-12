#ifndef WW_FRAMEPACER_H
#define WW_FRAMEPACER_H

/* lane EMPTY240 ITEM 0 (bungo 11-11: "we need 500+ FPS on an empty window", "Vsync should never be on by default").
 * Build 71 drew a frame only when a 6 ms QTimer ticked, and that coarse timer fires on the system tick (~15.6 ms):
 * 64 frames a second whatever the frame cost. Now the next frame starts as soon as the last one is done while the
 * view wants frames (a cell view with a live clock, a playing animation, a held key); the cap below may hold it back.
 * Pure: no Qt, no GL. tests/framepacer runs it on a model clock; glview.cpp drives it. */

namespace WwFramePacer
{

//! the frame cap a row value means: 0 off, -1 the monitor's refresh, n > 0 that many frames a second (10..1000)
inline int capHz( int row, double refreshHz )
{
	if ( row == 0 )
		return 0;
	if ( row < 0 )
		return refreshHz >= 10.0 ? int( refreshHz + 0.5 ) : 60;
	return row < 10 ? 10 : ( row > 1000 ? 1000 : row );
}

//! the cap's clock: when the next frame may start (seconds on the caller's clock; < 0 = no frame yet)
struct Clock
{
	double due = -1.0;
};

//! seconds to wait before the next frame may start; <= 0 = start it now
inline double waitSeconds( const Clock & c, int capHz, double now )
{
	if ( capHz <= 0 || c.due < 0.0 )
		return 0.0;
	return c.due - now;
}

//! a frame starts at `now`: the next one is due a period later, on the same phase unless a whole period was lost
inline void started( Clock & c, int capHz, double now )
{
	if ( capHz <= 0 ) {
		c.due = -1.0;
		return;
	}
	const double period = 1.0 / double( capHz );
	c.due = ( c.due >= 0.0 && now - c.due < period ) ? c.due + period : now + period;
}

/* how a wait is spent: a high-resolution sleep up to 0.6 ms short of the due time (at most 4 ms, so input still runs),
 * the rest by re-posting. Measured on bungo's PC under full load (tools/hrtprobe.cpp, 11-12): a 0.5 ms high-resolution
 * wait took 0.55 ms (worst 0.99), while a plain Sleep( 1 ) took 14-24 ms even after timeBeginPeriod( 1 ). */
inline double sleepSeconds( double wait )
{
	if ( wait <= 0.0008 )
		return 0.0;
	const double s = wait - 0.0006;
	return s > 0.004 ? 0.004 : s;
}

/* lane EMPTY240 R2-1 (build 72 at empty240_b71: "driven by the 6 ms timer (nothing moves)", between-paints 6 ms): does
 * the view want the next frame as soon as this one is done? A cell view always does, like a game, whatever moves; an
 * editing view only while an animation plays or a key is held, and only with a live clock (a harness stands still).
 * red 2 (WW_FRAMETIMER_RED=2) = build 72's rule: the cell view too only with a live clock. */
inline bool wantsFrames( bool cellView, bool clockLive, bool animPlays, bool keyHeld, int red )
{
	if ( red == 2 )
		return clockLive && ( cellView || animPlays || keyHeld );
	return cellView || ( clockLive && ( animPlays || keyHeld ) );
}

/* lane EMPTY240 R2-3 (build 72's GPU reset at the first goto): the frames the CPU may queue ahead of the GPU. Uncapped
 * (R2-1), the driver queued several heavy cell frames behind the jump's cascade rebuilds; a game holds 2. Red 1
 * (WW_FRAMEQUEUE_RED=1) = build 72: no cap (0). */
inline int framesInFlight( int red )
{
	return red == 1 ? 0 : 2;
}

/* The `frame gpu longest:` window: a camera jump (a goto) opens it for n frames; each frame's GPU ms (read late, so
 * only frames painted from the jump on count) feeds it; add() says when it closed. */
struct GpuLongest
{
	int left = 0, frames = 0;
	long long from = 0, longestAt = -1;
	double longestMs = 0.0, sumMs = 0.0;
	static bool jumped( double dx, double dy, double dz, double reach )
	{
		return dx * dx + dy * dy + dz * dz > reach * reach;
	}
	void open( long long frame, int n )
	{
		if ( left <= 0 ) {
			frames = 0;
			longestMs = 0.0;
			sumMs = 0.0;
			longestAt = -1;
			from = frame;
		}
		left = n;	// a second jump inside the window extends it
	}
	bool add( long long frame, double ms )
	{
		if ( left <= 0 || frame < from )
			return false;
		frames++;
		sumMs += ms;
		if ( ms > longestMs ) {
			longestMs = ms;
			longestAt = frame;
		}
		return --left == 0;
	}
};

}	// namespace WwFramePacer

/* lane EMPTY240 item 1: the whole frame, start to start, in parts that add up to it exactly (the overlay's fps is
 * 1000 / this wall). The pass timers cover only `passes`; every other part is named. glview.cpp marks the times. */
namespace WwFrameWall
{

enum Part
{
	Before,	// paintGL's start to the pass timers' frame start (stream evict, texture pump)
	Passes,	// the pass timers' CPU sum (WW_CELL_PASSTIME)
	GiFrame,	// rtddgiCellFrame (the traced GI volume: builds, Vulkan submits and waits, uploads)
	Restir,	// rtddgiRestirPass (ReSTIR DI)
	ProbeSky,	// probeSkyTick
	PassTimers,	// lane EMPTY240 R10-4: PassTime::frameBegin + frameEnd (the timers' own stamps, read-back and pass line)
	Transform,	// lane EMPTY240 R10-4: the scene's transform with the skeleton, door, world clock and sea steps before it
	OtherInFrame,	// inside the timed frame, in no pass zone and no part above
	After,	// the timed frame's end to paintGL's return (log flush)
	Swap,	// paintGL's return to Qt's frameSwapped (the swap; vsync or a GPU-bound queue waits here)
	CapWait,	// the frame cap's sleeps
	Gears,	// lane EMPTY240 R4-2: the pacer's advanceGears (the cell stream's step, physics, animation) before update()
	Between,	// the rest until the next paintGL (Qt's event loop, the redraw timer or the posted chain)
	PartCount
};

inline const char * partName( int p )
{
	static const char * const n[PartCount] = { "before-frame", "passes", "gi-frame", "restir", "probe-sky",
		"pass-timers", "scene-transform", "other-in-frame", "after-frame", "swap", "cap-wait", "gears", "between-paints" };
	return p >= 0 && p < PartCount ? n[p] : "?";
}

//! one paint's marks (seconds on one clock; < 0 = not reached) and its measured parts (seconds)
struct Marks
{
	double start = -1.0, begin = -1.0, end = -1.0, finish = -1.0, swapAt = -1.0;
	double passes = 0.0, gi = 0.0, restir = 0.0, probe = 0.0, capWait = 0.0, gears = 0.0;
	double timers = 0.0, transform = 0.0;	// lane EMPTY240 R10-4
};

struct Window
{
	double t0 = -1.0;
	int frames = 0;
	double wall = 0.0;
	double part[PartCount] = {};
};

/* the paint `m` ends when the next one starts at `next`: its wall and parts go into `w`. The parts sum to the wall by
 * construction (other-in-frame and between-paints take the remainders). False = `m` was not a whole paint. */
inline bool add( Window & w, const Marks & m, double next )
{
	if ( m.start < 0.0 || m.finish < m.start || next < m.finish )
		return false;
	const bool timed = m.begin >= m.start && m.end >= m.begin && m.finish >= m.end;
	const double begin = timed ? m.begin : m.finish, end = timed ? m.end : m.finish;
	double p[PartCount] = {};
	p[Before] = begin - m.start;
	p[Passes] = timed ? m.passes : 0.0;
	p[GiFrame] = m.gi;
	p[Restir] = m.restir;
	p[ProbeSky] = m.probe;
	p[PassTimers] = timed ? m.timers : 0.0;
	p[Transform] = timed ? m.transform : 0.0;
	p[OtherInFrame] = ( end - begin ) - p[Passes] - p[GiFrame] - p[Restir] - p[ProbeSky] - p[PassTimers] - p[Transform];
	p[After] = m.finish - end;
	p[Swap] = m.swapAt >= m.finish && m.swapAt <= next ? m.swapAt - m.finish : 0.0;
	p[CapWait] = m.capWait;
	p[Gears] = m.gears;
	p[Between] = ( next - m.finish ) - p[Swap] - p[CapWait] - p[Gears];
	if ( w.t0 < 0.0 )
		w.t0 = m.start;
	w.frames++;
	w.wall += next - m.start;
	for ( int i = 0; i < PartCount; i++ )
		w.part[i] += p[i];
	return true;
}

}	// namespace WwFrameWall

/* The rows (glview.cpp; View menu, nifskope_ui.cpp). Saved in QSettings under GLView/Display/; env pins first:
 * WW_VSYNC=0|1, WW_FRAME_CAP=0|-1|n. WW_FRAMETIMER_RED=1: build 71's pacing (the 6 ms coarse timer, vsync on). */
bool wwVsyncOn();
void wwSetVsync( bool on );
int wwFrameCapRow();
void wwSetFrameCap( int row );

/* Windows timers (cellpasstime.cpp, which already holds the Windows headers): the system timer resolution in ms
 * (NtQueryTimerResolution; -1 unknown), and the frame cap's sleep on a high-resolution waitable timer (false = none
 * made: the caller re-posts instead of sleeping). */
double wwTimerResolutionMs();
bool wwPreciseSleep( double seconds );

#endif
