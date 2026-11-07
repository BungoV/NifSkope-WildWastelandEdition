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
	/* lane TIMING1 (11-01): the costs no zone held. Water = the wind's waves + the water's draws in the sorted pass
	 * (refraction copies included); SsrWater = the SSR pass's water rays (step 1b); SsrDonor = the SSR pass's second
	 * opaque scene pass (the SSGI donor, or the colour when the previous frame cannot stand in); Contact = the contact
	 * mask (was inside "reflections"); CloudShadows, FarGround = cut out of "sun-cascades"; Precip, Underwater,
	 * Clouds = passes outside every zone before; Taa = the temporal AA's blits, vectors, resolve and copy-back (and
	 * TAAINPUTS1's object vectors), cut out of "post" */
	Water, SsrWater, SsrDonor, Contact, CloudShadows, FarGround, Precip, Underwater, Clouds, Taa,
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
	FxResDown, FxResLayer, FxResComp,	// lane FXRES1: the soft-effect layer's depth, draws and composite
	AirPre, AirFar, AirPost,
	FarGroundPaint, FarGroundDraw,	// lane FARPAGES1: the far ground's page paint + BC1, ring 0's ground draws
	SunCasters, SunTerrStep, CloudShadow, FarGroundPass,	// lane PERFFIX2 b: the four parts of the sun-cascades zone
	// lane PERFFIX2 (item 3a): the ssgi pass's stages (cellssgi.cpp): the interval pyramid, the donor's mips, the
	// emissive cast and its own temporal + denoise chain, the ordinary cast, its temporal, its denoise (robust + pyramid)
	SsgiHiz, SsgiMips, SsgiEcast, SsgiEchain, SsgiCast, SsgiTemporal, SsgiDenoise,
	ContactMarch,	// lane PERFFIX2 (item 7): the contact pass (near march, far tier, upsample) inside reflections
	WaterDraw, WaterCopy, UnderwaterSub,	// lane PERFFIX3: the water's colour draws (each, summed; the copy inside), its refraction copies, the camera-under-water pass
	PostDof, PostMblur, PostObjBlur, PostFlare,	// lane POSTFX1: the post zone's parts (cellpost.cpp, the lens flares)
	SubCount };	// lane FOGPERF1: the air before stage 0 (depth blit), its far grid, after it
void subBegin( int sub );
//! lane PERF2: CPU time inside the per-draw setup (setupProgram and the cell's uniform calls), summed per frame
enum CpuSub { CpuSetup, CpuCellUni, CpuSunUni, CpuFogUni, CpuAoDraw, CpuSsrDraw,
	// lane FOGPERF1: the air pass's CPU, stage by stage (they sum to the air zone's CPU)
	CpuAirState, CpuAirSetup, CpuAirClusters, CpuAirTile, CpuAirInject, CpuAirInteg, CpuAirFar, CpuAirTail, CpuAirEcho,
	CpuBlGeo, CpuBlRow,	// lane BINDLESSPERF1: a bindless draw's pooled geometry bind and its row write
	CpuSunCasters, CpuSunTerrStep, CpuCloudShadow, CpuFarGroundPass,	// lane PERFFIX2 b: the sun-cascades zone's parts
	CpuWater, CpuUnder,	// lane PERFFIX3: the water's colour draws (setup + issue), the under-water decision + pass
	CpuSunWalk, CpuCe1Tail,	// lane PERFHUNT2: the casters' scene walk inside sun-casters; the CE1 setup's per-shape tail inside setup
	CpuAoCollect, CpuAoSorted, CpuAoEffects,	// lane MODULES1: the AO walk's parts (scene walk + opaque draws, sorted draws, effects)
	CpuSubCount };
qint64 nowNs();
void cpuSubAdd( int sub, qint64 ns );
#ifdef WW_TRACY
/* lane PERFCPU1 (11-07): the Tracy profile build only (qmake CONFIG+=tracy -> release/NifSkope.tracy.exe). Each CPU sub
 * is a Tracy zone named like its "cpu <name>" field; the default build has none of this (no WW_TRACY). */
void tracySubBegin( int sub );
void tracySubEnd();
void tracyMessage( const char * text );
#endif
class DrawCpuScope
{
public:
#ifdef WW_TRACY
	explicit DrawCpuScope( int sub ) : k_( on() ? sub : -1 ), t0_( k_ >= 0 ? nowNs() : 0 ) { if ( k_ >= 0 ) tracySubBegin( k_ ); }
	~DrawCpuScope() { if ( k_ >= 0 ) { cpuSubAdd( k_, nowNs() - t0_ ); tracySubEnd(); } }
#else
	explicit DrawCpuScope( int sub ) : k_( on() ? sub : -1 ), t0_( k_ >= 0 ? nowNs() : 0 ) {}
	~DrawCpuScope() { if ( k_ >= 0 ) cpuSubAdd( k_, nowNs() - t0_ ); }
#endif
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

//! lane TIMING1: the GPU pass open at the top of the stack (-1: none, or the timers off)
int openPass();
/*! lane TIMING1: a part of a pass timed as a pass of its own. Only when `from` is the open pass: it closes, `to`
 *  opens, and at the scope's end `to` closes and `from` opens again (its CPU, GPU and draws then sum both pieces).
 *  Anywhere else (another pass open, the timers off) it does nothing, so a shared draw path called from another pass
 *  stays in that pass. */
class Swap
{
public:
	Swap( int from, int to ) : from_( openPass() == from ? from : -1 ), to_( to ) { if ( from_ >= 0 ) { end( from_ ); begin( to_ ); } }
	~Swap() { if ( from_ >= 0 ) { end( to_ ); begin( from_ ); } }
private:
	int from_, to_;
	Q_DISABLE_COPY( Swap )
};

/*! lane TIMING1: WW_CELL_PASSTIME_LIGHTLOOP=1 (timing rows only; the picture flickers): every odd timed frame the
 *  cell-lit draws get 0 lights (celllights.cpp), and every 60 frames read back stderr gets
 *  "pass time: light loop ..." with the opaque / sorted / water / ssr-donor / frame GPU means of the lit frames, the
 *  unlit ones and the difference (the light loop's GPU ms). Each "pass time:" line ends "| lights 1|0". */
bool lightLoopOff();

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
//! lane PERFFIX4: the bytes texResidentSet recorded for `id` (0 = none)
qint64 texResidentBytesOf( unsigned int id );

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
