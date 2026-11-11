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

}	// namespace WwFramePacer

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
