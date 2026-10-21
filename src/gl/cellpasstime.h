#ifndef CELLPASSTIME_H
#define CELLPASSTIME_H

/* lane PERF1 (2026-10-16): WHERE A FRAME SPENDS ITS TIME, PASS BY PASS.
 *
 * Each pass of a frame (sun cascades, cell-light cubes, sky, obscurance, reflections, air, the opaque and the
 * sorted pass, far cards, tone map, post) is bracketed by a scope that adds its CPU time (the time to issue it)
 * and its GPU time (two timestamp queries, read back two frames later so the frame never waits for them).
 * The cell's run culling is CPU-only (it runs inside the opaque pass, per shape).
 *
 * The Frame Timers row (Scene window, Diagnostics) shows the averages over the view; WW_CELL_PASSTIME=0|1 pins the
 * row, WW_CELL_PASSTIME_LOG=1 prints one line per frame to stderr ("pass time: ..."). Off, a scope is one bool test. */

#include <QStringList>
#include <QtGlobal>

namespace PassTime
{

enum Pass
{
	SunShadows, CellShadows, Background, Obscurance, Reflections, Air, Measure, UmbraDepth, Impostors,
	Opaque, Sorted, Effects, FarCards, ToneMap, Post, Overlays, Ssgi,
	CullCamera, CullCascades,	// CPU only: the run culling inside the passes above
	ShapeSetup,	// CPU only: the shapes' program and uniform setup inside the passes above
	Count
};

const char * name( int pass );

//! The Frame Timers row; the pin WW_CELL_PASSTIME wins over the stored setting.
bool on();
void setOn( bool on );
bool pinned();

//! paintGL's first and last lines. GL calls only while on.
void frameBegin();
void frameEnd();

void begin( int pass );
void end( int pass );
//! CPU-only time added to a pass (the culling).
void addCpu( int pass, qint64 ns );
//! One draw call issued (`ranges` index ranges in it: a multi-draw's count, else 1), counted to the innermost open pass.
void countDraw( qint64 ranges );
qint64 nowNs();

class Scope
{
public:
	explicit Scope( int pass ) : p_( on() ? pass : -1 ) { if ( p_ >= 0 ) begin( p_ ); }
	~Scope() { if ( p_ >= 0 ) end( p_ ); }
private:
	int p_;
	Q_DISABLE_COPY( Scope )
};

class CpuScope
{
public:
	explicit CpuScope( int pass ) : p_( on() ? pass : -1 ), t0_( p_ >= 0 ? nowNs() : 0 ) {}
	~CpuScope() { if ( p_ >= 0 ) addCpu( p_, nowNs() - t0_ ); }
private:
	int p_;
	qint64 t0_;
	Q_DISABLE_COPY( CpuScope )
};

//! The geometry cache's traffic, counted by glcontext.cpp: entries made (uploads) and evicted, bytes held.
struct CacheCounts
{
	qint64 made = 0, madeBytes = 0, evicted = 0, usedBytes = 0, maxBytes = 0;
	qint64 texMade = 0, texBytes = 0;	// textures uploaded by the texture cache (gltexloaders.cpp), their bytes
};
CacheCounts & cacheCounts();

//! The overlay's lines (averages of the last second), empty while off.
QStringList overlay();

//! This process's private bytes (commit charge) and working set in MB.
void memory( qint64 & privateMb, qint64 & workingMb );
//! Prints "cell mem: <stage> private=<MB> ws=<MB> t=<s since the process started>" to stderr.
void memLog( const char * stage );
//! Seconds since this process was created (the load timeline).
double processSeconds();
//! WW_PROF_SAMPLE=<file> with WW_PROF_SAMPLE_ALL=1: the sampler from the calling (GL) thread's start, the load included.
void profileFromStart();

} // namespace PassTime

#endif
