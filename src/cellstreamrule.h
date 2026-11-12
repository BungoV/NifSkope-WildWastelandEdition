#ifndef CELLSTREAMRULE_H
#define CELLSTREAMRULE_H

//! lane AIRPERF1 round 4: when a cell view streams. Pure rules (no Qt), tested by tests/cellstreamrule/run.sh.

namespace cellstreamrule {

enum OpenRule
{
	DefaultOn = 0,   //!< every exterior view streams, opened by a spec or not
	PinOn = 1,       //!< WW_CELL_STREAM=1
	PinOff = 2,      //!< WW_CELL_STREAM=0: a harness's fixed block, said explicitly
	CensusWalk = 3,  //!< WW_CELL_CENSUS_TEST opens exterior blocks itself and must weld them
	RedSpecFixed = 4 //!< red WW_CELL_STREAM_RED=specfixed: an open by a spec stays a fixed block (the rule before round 4)
};

//! the rule that decides whether an exterior open streams
inline int openRule( bool pinSet, int pin, bool specOpen, bool censusTest, bool redSpecFixed )
{
#ifdef CELLSTREAMRULE_RED_SPECFIXED
	redSpecFixed = true;
#endif
	if ( pinSet )
		return pin != 0 ? PinOn : PinOff;
	if ( censusTest )
		return CensusWalk;
	if ( redSpecFixed && specOpen )
		return RedSpecFixed;
	return DefaultOn;
}

inline bool openOn( int rule )
{
	return rule == DefaultOn || rule == PinOn;
}

inline const char * openWords( int rule )
{
	switch ( rule ) {
	case PinOn:
		return "on (WW_CELL_STREAM=1)";
	case PinOff:
		return "off (WW_CELL_STREAM=0: a fixed block)";
	case CensusWalk:
		return "off (WW_CELL_CENSUS_TEST welds its own blocks)";
	case RedSpecFixed:
		return "off (RED WW_CELL_STREAM_RED=specfixed: an open by a spec is a fixed block)";
	default:
		return "on (the default: every exterior view streams)";
	}
}

//! a running streamed view: the pin decides, else the Cell Streaming row
inline bool liveOn( bool pinSet, int pin, bool row, bool redNoPause )
{
#ifdef CELLSTREAMRULE_RED_NOPAUSE
	redNoPause = true;
#endif
	if ( redNoPause )
		return true;
	return pinSet ? pin != 0 : row;
}

//! the cell a world coordinate stands in (4096 units a cell)
inline int cellOf( double v )
{
	const double c = v / 4096.0;
	const int i = int( c );
	return c < double( i ) ? i - 1 : i;
}

/*! the block's centre after the eye moved: it shifts when the eye is `shift` units into the neighbour cell
 *  (src/cellstream.cpp shiftCentre, the same arithmetic); `held` = the row is off: no shift */
inline void centreFor( double ex, double ey, double shift, bool held, int & cx, int & cy )
{
#ifdef CELLSTREAMRULE_RED_PARKED
	held = true;
#endif
	if ( held )
		return;
	const double lim = 0.5 * 4096.0 + shift;
	for ( int guard = 0; guard < 256; guard++ ) {
		const double dx = ex - ( double( cx ) + 0.5 ) * 4096.0;
		const double dy = ey - ( double( cy ) + 0.5 ) * 4096.0;
		bool moved = false;
		if ( dx >= lim ) { cx++; moved = true; } else if ( dx <= -lim ) { cx--; moved = true; }
		if ( dy >= lim ) { cy++; moved = true; } else if ( dy <= -lim ) { cy--; moved = true; }
		if ( !moved )
			break;
	}
}

/*! lane AIRPERF1 r5: the Cell Streaming row at the start of a session: the saved setting (CellView/CellStream), on when
 *  nothing was saved (it ships on) */
inline bool rowStart( bool saved, bool savedOn )
{
#ifdef CELLSTREAMRULE_RED_ROWNOSAVE
	(void)saved; (void)savedOn;
	return true;   // red: round 4's row, on at every launch
#else
	return saved ? savedOn : true;
#endif
}

/*! lane AIRPERF1 r5: a loaded cell still owes the ray-traced GI its soup: the traced set follows the stream now, the
 *  cell is drawn, it was built before the row went on (no soup) and no gather was started for it yet */
inline bool soupOwed( bool follows, bool attached, bool hasSoup, bool asked )
{
#ifdef CELLSTREAMRULE_RED_NEVEROWED
	(void)follows; (void)attached; (void)hasSoup; (void)asked;
	return false;   // red: round 4, such a cell is traced only after it was streamed again
#else
	return follows && attached && !hasSoup && !asked;
#endif
}

} // namespace cellstreamrule

#endif
