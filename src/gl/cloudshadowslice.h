#ifndef CLOUDSHADOWSLICE_H
#define CLOUDSHADOWSLICE_H

#include <algorithm>

/* lane PERFFIX3 (item 4): the cloud shadow map's rebuild, time-sliced. No GL: tests/cloudshadowslice drives it.
 *
 * CLOUD3 refreshed size/16 rows a frame and rebuilt the WHOLE map in one frame when its fit moved (the eye crossed a
 * side/8 snap, or the sun turned 0.25 deg): 0.28 ms at 1024 in one frame (live, merge1030a c3_gb1_fix), against
 * 0.03-0.04 for the rows. Here a moved fit starts a second map (the back) built size/16 rows a frame under the new
 * fit, while the receivers keep reading the front (complete, under its own fit, as before); when the back's last row
 * is in, the two swap. A move seen while a back build runs waits for the swap (then the next frame compares again),
 * so a back always completes: the front is at most two builds (2 x 16 frames) behind a camera that keeps moving.
 * A HARD change (no front yet, a new size, mode, density twin or red) rebuilds the front at once as before: there is
 * no complete map under the old rules to stand in. Red WW_CLOUDSHADOW_RED=fullmove: a move rebuilds at once (CLOUD3). */
namespace WwCloudShadowSlice
{

enum class Into
{
	Front,	// rows of the map the receivers read
	Back	// rows of the map being built under a new fit
};

struct State
{
	bool frontValid = false;
	bool backActive = false;
	int cursor = 0;		// the front's next refresh row
	int backCursor = 0;	// the back's next row
};

struct Step
{
	Into into = Into::Front;
	int row0 = 0, rows = 0;
	bool fullNow = false;	// the front rebuilt whole this frame (hard, or the red)
	bool startBack = false;	// the back takes the new fit now (before its first rows)
	bool swap = false;		// the back's last rows: swap after this frame's draw
};

/*! one frame's work. hard: the front cannot stand in; moved: the fit or the sun moved past its step against the front
 *  (looked at only while no back build runs); n: the map's rows; perFrame: rows a frame (size / 16) */
inline Step plan( State & s, bool hard, bool moved, int n, int perFrame, bool redFullMove = false )
{
	Step st;
	const int k = std::max( 1, std::min( perFrame, n ) );
	if ( hard || !s.frontValid || ( redFullMove && moved && !s.backActive ) ) {
		s.frontValid = true;
		s.backActive = false;
		s.backCursor = 0;
		s.cursor = 0;
		st.into = Into::Front;
		st.row0 = 0;
		st.rows = n;
		st.fullNow = true;
		return st;
	}
	if ( !s.backActive && moved ) {
		s.backActive = true;
		s.backCursor = 0;
		st.startBack = true;
	}
	if ( s.backActive ) {
		st.into = Into::Back;
		st.row0 = s.backCursor;
		st.rows = std::min( k, n - st.row0 );
		s.backCursor = st.row0 + st.rows;
		if ( s.backCursor >= n ) {
			st.swap = true;
			s.backActive = false;
			s.backCursor = 0;
			s.cursor = 0;	// the new front is whole: its refresh starts over
		}
		return st;
	}
	st.into = Into::Front;
	st.row0 = s.cursor;
	st.rows = std::min( k, n - st.row0 );
	s.cursor = st.row0 + st.rows >= n ? 0 : st.row0 + st.rows;
	return st;
}

}	// namespace WwCloudShadowSlice

#endif
