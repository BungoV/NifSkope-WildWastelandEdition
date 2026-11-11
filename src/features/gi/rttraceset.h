#ifndef RTTRACESET_H
#define RTTRACESET_H

/*! lane AIRPERF1 round 4: the ray-traced GI's traced geometry follows the streamed cells.
 *
 *  Before, an open with the Ray-traced GI row on welded a fixed block and kept its soup once: the view did not
 *  stream, and the volume's cascades stayed inside that block (the GI parked at the spawn). Now every streamed cell
 *  gathers its own soup on the stream's worker (a unit), the stream hands the loaded units and the far terrain round
 *  them over here whenever the loaded set changed, and this store merges them on a worker and publishes the result as
 *  the kept soup (probesky.h) the volume traces. A publish is one BLAS build for the whole set (rtddgicell.cpp keeps
 *  the old volume drawn until the new one is up).
 *
 *  Reds: WW_CELL_RTSET_WELD_RED=1 (read at the open) = the welded fixed block, no stream; WW_CELL_RTSET_RED (live) =
 *  parked (the first set stays), nofar (no far terrain in the set), drop (the old volume is dropped at a publish). */

#include <QString>
#include <QtGlobal>

#include <memory>
#include <vector>

struct ProbeSoup;

//! one unit's soup: its mask and glow bitmaps live in a pool shared by every unit that names the same map
struct RtTraceUnitSoup;

struct RtTraceUnit
{
	int x = 0, y = 0;
	quint32 lref = 0;   //!< a large reference drawn near (x, y: the cell it stands in)
	std::shared_ptr<const RtTraceUnitSoup> soup;
};

//! red WW_CELL_RTSET_WELD_RED (read at the open): an open with the Ray-traced GI on welds a fixed block and does not stream
bool rtTraceSetWeldRed();
//! streamed cells gather their soup now: the Ray-traced GI is on, the old probe GI is off, no weld red (main thread)
bool rtTraceSetFollows();
//! WW_CELL_RTSET_RED holds this word (live)
bool rtTraceSetRed( const char * word );
//! a cell's soup becomes a unit: its bitmaps move to the pool, what the trace never reads is freed (any thread)
std::shared_ptr<const RtTraceUnitSoup> rtTraceSetIntern( ProbeSoup && soup );
qint64 rtTraceUnitTris( const RtTraceUnitSoup & u );
//! the far terrain's ring wanted now, cells round the block's centre (the coarsest cascade's reach)
int rtTraceSetRing( const void * nif, int gridHalf );
//! the stream's loaded set (after its lights were published): merged and published when it differs from the last
void rtTraceSetSync( const void * nif, std::vector<RtTraceUnit> units, int cx, int cy, int ex, int ey, int ring,
	std::shared_ptr<const ProbeSoup> farSoup, int farCells, int unitsNoSoup );
//! every stream tick: a finished merge is published, a waiting one starts
void rtTraceSetTick( const void * nif, int ex, int ey );
//! the stream of this document ended
void rtTraceSetForget( const void * nif );

// ---- for the volume (rtddgicell.cpp)
//! this document's kept soup is a stream's traced set
bool rtTraceSetActive( const void * nif );
//! a changed set waits for its merge or its publish
bool rtTraceSetPending( const void * nif );
//! the publishes so far (0: none yet)
quint64 rtTraceSetGen( const void * nif );
//! the worker-made copy of the published soup for a build (null: not this soup; the caller copies as before)
std::shared_ptr<const ProbeSoup> rtTraceSetBuildSoup( const void * nif, const ProbeSoup * kept );
//! once a frame: the coarsest cascade's reach (world units), and whether a build is in flight (the builds' copy is
//! freed after two seconds without one)
void rtTraceSetFrame( const void * nif, float reachUnits, bool building );

#endif
