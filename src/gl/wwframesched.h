#ifndef WWFRAMESCHED_H
#define WWFRAMESCHED_H

/* lane FRAMESCHED1 (11-02): the frame slots of the periodic work that can pile up in one frame. Pure (no Qt, no GL):
 * tests/framesched. The reflection probes' camera face and nearest placed face were both captured in the same frame
 * (two scene walks of casters), and on the frames a coarse DDGI cascade steps (rtddgimath.h cascadeDue: frame % every^L
 * == L) a capture landed on top of it. Rule: one capture a frame, the camera face and the placed face take turns when
 * both wait, and no capture on a frame a coarse cascade stepped on its own period (a jump or a picture spec's
 * every-frame step does not count: rtddgicell.cpp). A capture waits at most kMaxDefer frames in a row for the cascades
 * (cascades 1..3 step on consecutive frames by design; every = 1 steps one every frame). Lamp cubes are event-driven and already sliced
 * (gl/lampslice.h) and the exposure measure runs every frame: neither has a period to move.
 * Red WW_FRAMESCHED_RED=1 = today's schedule (both captures, every frame). */

namespace WwFrameSched {

static const int kMaxDefer = 3;   // rtddgi::kMaxCascades - 1: the coarse cascades' longest run of consecutive frames

struct Want {
	bool cam = false;         // the camera probe has a stale face
	bool placed = false;      // a placed probe has an uncaptured face
	bool ddgiCoarse = false;  // a coarse DDGI cascade stepped this frame on its period
};

struct Pick {
	bool cam = false;
	bool placed = false;
};

struct State {
	int last = 0;              // the last capture taken: 0 none, 1 camera, 2 placed
	int deferRun = 0;          // frames in a row the capture waited for a cascade
	unsigned long long deferred = 0, alternated = 0;   // census
};

inline Pick pick( const Want & w, State & st, bool red )
{
	Pick p;
	if ( red ) {
		p.cam = w.cam;
		p.placed = w.placed;
		return p;
	}
	if ( !w.cam && !w.placed ) {
		st.deferRun = 0;
		return p;
	}
	if ( w.ddgiCoarse && st.deferRun < kMaxDefer ) {
		st.deferRun++;
		st.deferred++;
		return p;
	}
	st.deferRun = 0;
	if ( w.cam && w.placed ) {
		st.alternated++;
		if ( st.last == 1 )
			p.placed = true;
		else
			p.cam = true;
	} else {
		p.cam = w.cam;
		p.placed = w.placed;
	}
	st.last = p.cam ? 1 : 2;
	return p;
}

}	// namespace WwFrameSched

#endif
