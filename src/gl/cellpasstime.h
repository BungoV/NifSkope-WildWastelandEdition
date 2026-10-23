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
	Opaque, Sorted, Effects, FarCards, ToneMap, Post, Overlays, Ssgi, ReflProbes,	// lane REFLPROBE1: the placed reflection probes
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

//! lane PERF2: GPU-only sub-stages inside a pass (two timestamps each), logged after the passes as "sub <name> <ms>".
enum Sub { AirTile, AirInject, AirIntegrate, AoGeometry, AoStages, AoGi, ToneBlit, ToneMeasure, ToneBloom, ToneQuads,
	FarDots, FarCardsLit, ToneVrs,	// lane VRS1: ToneVrs, the rate images' build after the tone map
	FxResDown, FxResLayer, FxResComp, SubCount };	// lane FXRES1: the soft-effect layer's depth, draws and composite
void subBegin( int sub );
//! lane PERF2: CPU time inside the per-draw setup (setupProgram and the cell's uniform calls), summed per frame
enum CpuSub { CpuSetup, CpuCellUni, CpuSunUni, CpuFogUni, CpuAoDraw, CpuSsrDraw, CpuSubCount };
qint64 nowNs();
void cpuSubAdd( int sub, qint64 ns );
class DrawCpuScope
{
public:
	explicit DrawCpuScope( int sub ) : k_( on() ? sub : -1 ), t0_( k_ >= 0 ? nowNs() : 0 ) {}
	~DrawCpuScope() { if ( k_ >= 0 ) cpuSubAdd( k_, nowNs() - t0_ ); }
private:
	int k_;
	qint64 t0_;
	Q_DISABLE_COPY( DrawCpuScope )
};
void subEnd( int sub );
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
	// lane STREAM1: what is resident now: geometry entries, their real bytes and their fp32 size; file textures
	qint64 geoShapes = 0, geoGpuBytes = 0, geoFp32Bytes = 0, texResident = 0, texResidentBytes = 0;
	qint64 texPumped = 0, texPumpedBytes = 0;	// uploaded ahead of their first bind by the streaming pump
};
CacheCounts & cacheCounts();

/*! lane STREAM1: WW_STREAM1_RED=<list> brings an old path (and its cost) back: geocache (the 128 MB geometry
 *  budget), texpump (the read-ahead ends after the open, nothing uploaded ahead), vtxpack (fp32 vertex arrays),
 *  texcopy (each DDS copied once more before upload); "all" = every one. */
bool stream1Red( const char * what );
//! lane STREAM1: the card's dedicated memory in MB (GL_NVX_gpu_memory_info; 0 = unknown), and
//! the MB in use on it now (all processes) when `usedMb` is given. Needs a current GL context.
qint64 videoMemoryMb( qint64 * usedMb );
//! lane STREAM1: "stream1 mem: <stage> geo=.. MB (fp32 .. MB) shapes=.. tex=.. MB (n) vram=../.. MB private=.. MB"
QString stream1MemLine( const char * stage );
//! lane STREAM1: a paint begins (after the streaming pump) / ends. `ready` = no cell queued, building or being
//! written and (streaming) the loaded block is the expected one. The first ready paint after a cell open that
//! needed no texture and under 1 MB of geometry prints "stream1 load: first full frame t=<s> ..." once; after it
//! each paint's CPU time feeds the hitch figures.
void stream1PaintBegin();
//! lane STREAM1: a cell open starts (memLog "before the build" calls it).
void stream1LoadArm();
void stream1PaintEnd( bool ready, double paintMs );
//! lane STREAM1: the open's first full frame has been drawn (or no cell open was timed: false)
bool stream1Loaded();
//! lane STREAM1: the worst paint (ms), paints over 33.3 ms and paints counted since the last reset (after the
//! first full frame only).
void stream1Hitch( double * maxMs, int * over, int * frames, bool reset );
//! lane STREAM1: a file texture `id` now holds `bytes` on the card / was deleted (the resident texture figures)
void texResidentSet( unsigned int id, qint64 bytes );
void texResidentDrop( unsigned int id );

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
