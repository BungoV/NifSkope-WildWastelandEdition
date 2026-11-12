#include "cellpasstime.h"
#include "features/shared/wwtelemetry.h"	// lane MODULES1: the shared telemetry module
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "wwloadwatch.h"	// lane PERFLOAD1 P5: every pass names itself to the main-thread watch
#include "gl/gpustage.h"	// MODULES1: the staged-upload counts in the stream1 mem line
#include "filebuf.hpp"	// MODULES1: the archive trims in the ram owners line
#include "framepacer.h"	// lane EMPTY240: the timer resolution helpers live here (Windows headers)

#include <QByteArray>
#include <QMap>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSettings>
#include <QString>

#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <mmsystem.h>	// timeBeginPeriod: the sampler's 1 ms sleep
#endif

// lane PERF1: see cellpasstime.h

namespace PassTime
{

/* lane PERF1: WW_PROF_SAMPLE=<file> (timing harnesses only), a sampling profiler of the GL thread inside the timed
 * frames: ~1 kHz, the thread suspended for the time of a stack walk (the x64 unwind tables, no allocation while
 * it is held), each sample "pass addr addr ..." innermost first, the modules' bases at the end; written at exit.
 * tools-side: the addresses go through addr2line on an unstripped link of the same objects. */
namespace Sampler
{
std::atomic<int> pass{ -2 };	// the innermost open pass; -2 outside a frame (not sampled)
#ifdef Q_OS_WIN
struct Sample { int pass; int ms; int n; DWORD64 pc[48]; };
std::vector<Sample> * samples = nullptr;
std::atomic<bool> stop{ false };
std::thread * thread = nullptr;
QByteArray path;

void write()
{
	stop = true;
	if ( thread ) {
		thread->join();
		delete thread;
		thread = nullptr;
	}
	FILE * f = std::fopen( path.constData(), "wb" );
	if ( !f || !samples )
		return;
	for ( const Sample & sm : *samples ) {
		std::fprintf( f, "%d @%d", sm.pass, sm.ms );
		for ( int i = 0; i < sm.n; i++ )
			std::fprintf( f, " %llx", (unsigned long long)sm.pc[i] );
		std::fputc( '\n', f );
	}
	HMODULE mods[1024];
	DWORD need = 0;
	if ( EnumProcessModules( GetCurrentProcess(), mods, sizeof( mods ), &need ) ) {
		for ( DWORD i = 0; i < need / sizeof( HMODULE ) && i < 1024; i++ ) {
			MODULEINFO mi;
			char name[MAX_PATH] = {};
			if ( GetModuleInformation( GetCurrentProcess(), mods[i], &mi, sizeof( mi ) )
				&& GetModuleFileNameA( mods[i], name, MAX_PATH ) )
				std::fprintf( f, "module %llx %lx %s\n", (unsigned long long)(DWORD64)mi.lpBaseOfDll, mi.SizeOfImage, name );
		}
	}
	std::fclose( f );
}

void start()
{
	path = wwEnv( "WW_PROF_SAMPLE" );
	if ( path.isEmpty() || thread )
		return;
	HANDLE gl = nullptr;
	DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &gl, 0, FALSE, DUPLICATE_SAME_ACCESS );
	samples = new std::vector<Sample>();
	timeBeginPeriod( 1 );
	// WW_PROF_SAMPLE_ALL=1 (started from main): the load too, outside the frames (pass -2), 1M samples at most
	WW_SWITCH_STATIC( bool, all, wwEnvInt( "WW_PROF_SAMPLE_ALL" ) == 1 );
	const size_t cap = all ? 1000000 : 200000;
	samples->reserve( cap );
	const DWORD t0 = timeGetTime();
	thread = new std::thread( [gl, cap, t0, all]() {
		Sample sm;
		// allocated here, never while the thread is stopped: it may hold the heap lock (a hang at startup)
		std::vector<unsigned char> copy( 256 * 1024 );
		while ( !stop ) {
			Sleep( 1 );
			const int p = pass.load();
			if ( ( p == -2 && !all ) || samples->size() >= cap )
				continue;
			sm.ms = int( timeGetTime() - t0 );
			/* Only the context and a copy of the stack are taken while the thread is stopped: the unwind looks up
			 * function tables, which can take the loader lock the stopped thread may hold (a hang at startup). */
			if ( SuspendThread( gl ) == DWORD( -1 ) )
				continue;
			CONTEXT ctx;
			ctx.ContextFlags = CONTEXT_FULL;
			sm.n = 0;
			sm.pass = pass.load();
			DWORD64 lo = 0, len = 0;
			const bool got = GetThreadContext( gl, &ctx );
			if ( got ) {
				MEMORY_BASIC_INFORMATION mi;
				lo = ctx.Rsp;
				if ( VirtualQuery( reinterpret_cast<LPCVOID>( lo ), &mi, sizeof( mi ) ) ) {
					const DWORD64 end = DWORD64( mi.BaseAddress ) + mi.RegionSize;
					len = std::min<DWORD64>( end - lo, copy.size() );
					std::memcpy( copy.data(), reinterpret_cast<const void *>( lo ), size_t( len ) );
				}
			}
			ResumeThread( gl );
			if ( got && len >= 8 ) {
				const DWORD64 cb = DWORD64( copy.data() ), delta = cb - lo;
				auto inStack = [&]( DWORD64 v ) { return v >= lo && v < lo + len; };
				ctx.Rsp += delta;
				if ( inStack( ctx.Rbp ) )
					ctx.Rbp += delta;
				for ( ; sm.n < 48 && ctx.Rip && ctx.Rsp >= cb && ctx.Rsp + 8 <= cb + len; ) {
					sm.pc[sm.n++] = ctx.Rip;
					DWORD64 base = 0;
					PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry( ctx.Rip, &base, nullptr );
					if ( fe ) {
						PVOID hd = nullptr;
						DWORD64 est = 0;
						RtlVirtualUnwind( UNW_FLAG_NHANDLER, base, ctx.Rip, fe, &ctx, &hd, &est, nullptr );
						if ( inStack( ctx.Rbp ) )
							ctx.Rbp += delta;
						if ( inStack( ctx.Rsp ) )	// a frame pointer restored Rsp from a real-stack Rbp
							ctx.Rsp += delta;
					} else {
						ctx.Rip = *reinterpret_cast<DWORD64 *>( ctx.Rsp );
						ctx.Rsp += 8;
					}
				}
			}
			if ( sm.n > 0 )
				samples->push_back( sm );
		}
	} );
	std::atexit( write );
}
#else
void start() {}
#endif
}	// namespace Sampler

namespace
{

constexpr int kSlots = 3;	// GPU results are read two frames after they were issued

struct Mark
{
	int pass;
	GLuint q0, q1;
	GLuint qs = 0, qp = 0;	// WW_CELL_PASSTIME_CENSUS: samples passed, primitives generated
	GLuint qf = 0;	// MODULES1 p18: WW_CELL_PASSTIME_CENSUS=2 also counts fragment shader invocations (overdraw)
};

struct Slot
{
	qint64 frame = -1;
	std::vector<Mark> marks;
	std::vector<Mark> subs;	// lane PERF2: the sub-stages (Mark::pass = the Sub)
	double subExt[SubCount] = {};	// lane VKFRAME1: sub ms timed outside GL (subAddMs)
	bool subExtSeen[SubCount] = {};
	GLuint f0 = 0, f1 = 0;	// the whole frame
	double cpu[Count] = {};
	double wait[Count] = {};	// sync mode: the closing glFinish's wait (the GPU still working after the CPU issued it all)
	qint64 calls[Count] = {}, ranges[Count] = {};
	double cpuFrame = 0.0;
	double cpuSub[CpuSubCount] = {};	// lane PERF2: CPU ms inside the draws' setup
	qint64 cpuSubN[CpuSubCount] = {};
	CacheCounts cache0, cache;
	qint64 privMb = 0;	// private bytes at the frame's end (the log only)	// the geometry cache at the frame's start; its traffic over the frame
};

struct State
{
	bool loaded = false, on = false, pinned = false, log = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool sync = false;	// WW_CELL_PASSTIME_SYNC=1: a glFinish at each pass edge, so the timestamps hold that pass's work only
	bool census = false;	// WW_CELL_PASSTIME_CENSUS=1: each pass's samples passed (overdraw) and primitives, in the log line
	bool frags = false;	// MODULES1 p18: WW_CELL_PASSTIME_CENSUS=2: + fragment shader invocations (a pipeline statistics query)
	bool lightLoop = false;	// lane TIMING1: WW_CELL_PASSTIME_LIGHTLOOP=1, odd frames unlit (cellpasstime.h lightLoopOff)
	double llSum[2][5] = {};	// [unlit, lit] x opaque, sorted, water, ssr-donor, frame GPU ms
	int llN[2] = {};
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_3_3_Core * gl = nullptr;
	std::vector<GLuint> pool;
	std::vector<GLuint> cpool;	// lane PERF2: the census's own (a query keeps the target it was first used with)
	std::vector<GLuint> fpool;	// MODULES1 p18: the fragment invocation queries' own
	Slot ring[kSlots];
	qint64 frame = 0;
	bool inFrame = false;
	QElapsedTimer clock;
	qint64 frameT0 = 0;
	qint64 open[Count] = {};
	int openMark[Count];
	int openSub[SubCount];
	std::vector<int> stack;	// the open GPU passes, innermost last (countDraw)
	// the last second's sums, for the overlay
	qint64 winT0 = -1;
	int winFrames = 0, winGpuFrames = 0;
	double winCpu[Count] = {}, winGpu[Count] = {}, winCpuFrame = 0.0, winGpuFrame = 0.0;
	QStringList lines;
	int memArmed = 0;
	bool text = true;	// lane PERFLOAD1 (P5): the table is drawn on screen (WW_CELL_PASSTIME_TEXT)
};

State & st()
{
	static State s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_PASSTIME" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		{
			const State d {};
			s.pinned = d.pinned;
		}
		if ( !s.clock.isValid() )
			if ( !s.clock.isValid() )
			if ( !s.clock.isValid() )
			s.clock.start();
		const QByteArray pin = wwEnv( "WW_CELL_PASSTIME" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/PassTimers" ), false ).toBool();
		}
		s.log = wwEnvInt( "WW_CELL_PASSTIME_LOG" ) == 1;
		// lane PERFLOAD1 (P5): switched on by WW_CELL_PASSTIME (the measuring route) the table is not drawn over the
		// view unless asked; the Frame Timers menu row draws it as before. WW_CELL_PASSTIME_TEXT=1 is the old way.
		s.text = wwEnvIsSet( "WW_CELL_PASSTIME_TEXT" ) ? wwEnvInt( "WW_CELL_PASSTIME_TEXT" ) == 1 : pin.isEmpty();
		if ( s.on && s.log )
			std::fprintf( stderr, "pass time text: table on screen %s (WW_CELL_PASSTIME_TEXT=1 draws it)\n", s.text ? "drawn" : "not drawn" );
		s.sync = wwEnvInt( "WW_CELL_PASSTIME_SYNC" ) == 1;
		s.census = wwEnvInt( "WW_CELL_PASSTIME_CENSUS" ) >= 1;
		s.frags = wwEnvInt( "WW_CELL_PASSTIME_CENSUS" ) == 2;
		s.lightLoop = wwEnvInt( "WW_CELL_PASSTIME_LIGHTLOOP" ) == 1;
		for ( int k = 0; k < 2; k++ ) {
			s.llN[k] = 0;
			for ( double & v : s.llSum[k] )
				v = 0.0;
		}
		for ( int & m : s.openMark )
			m = -1;
		for ( int & m : s.openSub )
			m = -1;
	}
	return s;
}

GLuint query()
{
	State & s = st();
	if ( s.pool.empty() ) {
		GLuint q[64];
		s.gl->glGenQueries( 64, q );
		s.pool.insert( s.pool.end(), q, q + 64 );
	}
	const GLuint q = s.pool.back();
	s.pool.pop_back();
	return q;
}

GLuint cquery()
{
	State & s = st();
	if ( s.cpool.empty() ) {
		GLuint q[64];
		s.gl->glGenQueries( 64, q );
		s.cpool.insert( s.cpool.end(), q, q + 64 );
	}
	const GLuint q = s.cpool.back();
	s.cpool.pop_back();
	return q;
}

constexpr GLenum kFragInv = 0x82F4;	// GL_FRAGMENT_SHADER_INVOCATIONS_ARB (ARB_pipeline_statistics_query, core in 4.6)

GLuint fquery()
{
	State & s = st();
	if ( s.fpool.empty() ) {
		GLuint q[64];
		s.gl->glGenQueries( 64, q );
		s.fpool.insert( s.fpool.end(), q, q + 64 );
	}
	const GLuint q = s.fpool.back();
	s.fpool.pop_back();
	return q;
}

double gpuMs( GLuint a, GLuint b )
{
	GLuint64 t0 = 0, t1 = 0;
	st().gl->glGetQueryObjectui64v( a, GL_QUERY_RESULT, &t0 );
	st().gl->glGetQueryObjectui64v( b, GL_QUERY_RESULT, &t1 );
	return t1 > t0 ? double( t1 - t0 ) * 1e-6 : 0.0;
}

//! Reads a finished slot back, logs its frame and adds it to the window; its queries go back to the pool.
//! lane MODULES1: the telemetry cost row, one per WW_TELEMETRY_RATE frames: frame, zones, sub-stages, per-module sums
static void costRow( const Slot & sl, const double * gpu, double gpuFrame, const double * sub, const bool * subSeen,
	const char * const * subName, const char * const * cpuSubName )
{
	using namespace WwTelemetry;
	WwTelemetry::setFrame( sl.frame );
	QMap<QByteArray, QPair<double, double>> mod;
	QByteArray zones, subs, cpus, mods;
	for ( int p = 0; p < Count; p++ ) {
		if ( sl.cpu[p] <= 0.0 && gpu[p] <= 0.0 && sl.calls[p] == 0 )
			continue;
		zones += ( zones.isEmpty() ? "" : "," ) + str( name( p ) ) + ":[" + num( sl.cpu[p] ) + "," + num( gpu[p] ) + ","
			+ QByteArray::number( sl.calls[p] ) + "]";
		const QByteArray m = zoneModule( name( p ) );
		if ( !m.isEmpty() ) {
			mod[m].first += sl.cpu[p];
			mod[m].second += gpu[p];
		}
	}
	for ( int k = 0; k < SubCount; k++ )
		if ( subSeen[k] )
			subs += ( subs.isEmpty() ? "" : "," ) + str( subName[k] ) + ":" + num( sub[k] );
	for ( int k = 0; k < CpuSubCount; k++ )
		if ( sl.cpuSubN[k] > 0 )
			cpus += ( cpus.isEmpty() ? "" : "," ) + str( cpuSubName[k] ) + ":[" + num( sl.cpuSub[k] ) + ","
				+ QByteArray::number( sl.cpuSubN[k] ) + "]";
	for ( auto it = mod.cbegin(); it != mod.cend(); ++it )
		mods += ( mods.isEmpty() ? "" : "," ) + str( it.key() ) + ":[" + num( it.value().first ) + "," + num( it.value().second ) + "]";
	record( "frame", "cost", "\"cpu\":" + num( sl.cpuFrame ) + ",\"gpu\":" + num( gpuFrame ) + ",\"zones\":{" + zones
		+ "},\"sub\":{" + subs + "},\"cpusub\":{" + cpus + "},\"modules\":{" + mods + "}" );
}

void collect( Slot & sl )
{
	State & s = st();
	if ( sl.frame < 0 )
		return;
	double gpu[Count] = {};
	GLuint64 samples[Count] = {}, prims[Count] = {}, frags[Count] = {};
	for ( const Mark & m : sl.marks ) {
		gpu[m.pass] += gpuMs( m.q0, m.q1 );
		s.pool.push_back( m.q0 );
		s.pool.push_back( m.q1 );
		if ( m.qs ) {
			GLuint64 v = 0;
			s.gl->glGetQueryObjectui64v( m.qs, GL_QUERY_RESULT, &v );
			samples[m.pass] += v;
			s.gl->glGetQueryObjectui64v( m.qp, GL_QUERY_RESULT, &v );
			prims[m.pass] += v;
			s.cpool.push_back( m.qs );
			s.cpool.push_back( m.qp );
		}
		if ( m.qf ) {
			GLuint64 v = 0;
			s.gl->glGetQueryObjectui64v( m.qf, GL_QUERY_RESULT, &v );
			frags[m.pass] += v;
			s.fpool.push_back( m.qf );
		}
	}
	double sub[SubCount] = {};
	bool subSeen[SubCount] = {};
	for ( const Mark & m : sl.subs ) {
		sub[m.pass] += gpuMs( m.q0, m.q1 );
		subSeen[m.pass] = true;
		s.pool.push_back( m.q0 );
		s.pool.push_back( m.q1 );
	}
	sl.subs.clear();
	for ( int k = 0; k < SubCount; k++ ) {	// lane VKFRAME1
		if ( sl.subExtSeen[k] ) {
			sub[k] += sl.subExt[k];
			subSeen[k] = true;
		}
		sl.subExt[k] = 0.0;
		sl.subExtSeen[k] = false;
	}
	const double gpuFrame = gpuMs( sl.f0, sl.f1 );
	s.pool.push_back( sl.f0 );
	s.pool.push_back( sl.f1 );
	if ( s.lightLoop ) {	// lane TIMING1: the light loop's GPU ms = the lit frames' mean minus the unlit ones'
		const int k = ( sl.frame & 1 ) ? 0 : 1;
		const double v[5] = { gpu[Opaque], gpu[Sorted], gpu[Water], gpu[SsrDonor], gpuFrame };
		for ( int i = 0; i < 5; i++ )
			s.llSum[k][i] += v[i];
		s.llN[k]++;
		if ( s.llN[0] + s.llN[1] >= 60 && s.llN[0] > 0 && s.llN[1] > 0 ) {
			static const char * const llName[5] = { "opaque", "sorted", "water", "ssr-donor", "frame" };
			QByteArray ll = QByteArray( "pass time: light loop lit " ) + QByteArray::number( s.llN[1] ) + " unlit "
				+ QByteArray::number( s.llN[0] );
			double lit[5], unlit[5];
			for ( int i = 0; i < 5; i++ ) {
				lit[i] = s.llSum[1][i] / double( s.llN[1] );
				unlit[i] = s.llSum[0][i] / double( s.llN[0] );
				ll += QByteArray( " | " ) + llName[i] + " " + QByteArray::number( lit[i], 'f', 2 ) + "/"
					+ QByteArray::number( unlit[i], 'f', 2 );
			}
			const double loop = ( lit[0] - unlit[0] ) + ( lit[1] - unlit[1] ) + ( lit[2] - unlit[2] ) + ( lit[3] - unlit[3] );
			ll += QByteArray( " | loop " ) + QByteArray::number( loop, 'f', 2 ) + " ms (opaque+sorted+water+ssr-donor, lit-unlit)";
			std::fprintf( stderr, "%s\n", ll.constData() );
			for ( int j = 0; j < 2; j++ ) {
				s.llN[j] = 0;
				for ( double & x : s.llSum[j] )
					x = 0.0;
			}
		}
	}
	if ( s.log || WwTelemetry::costDue( sl.frame ) ) {	// lane MODULES1: the cost row reads the same names
		QByteArray line = QByteArray( "pass time: t=" ) + QByteArray::number( processSeconds(), 'f', 1 ) + " frame="
			+ QByteArray::number( sl.frame ) + " cpu="
			+ QByteArray::number( sl.cpuFrame, 'f', 2 ) + " gpu=" + QByteArray::number( gpuFrame, 'f', 2 );
		for ( int p = 0; p < Count; p++ ) {
			if ( sl.cpu[p] <= 0.0 && gpu[p] <= 0.0 && sl.calls[p] == 0 )
				continue;
			line += QByteArray( " | " ) + name( p ) + " " + QByteArray::number( sl.cpu[p], 'f', 2 ) + "/"
				+ QByteArray::number( gpu[p], 'f', 2 );
			if ( s.sync && p < CullCamera )
				line += " w=" + QByteArray::number( sl.wait[p], 'f', 2 );
			if ( sl.calls[p] > 0 )
				line += " d=" + QByteArray::number( sl.calls[p] ) + "/" + QByteArray::number( sl.ranges[p] );
			if ( s.census && p < CullCamera )	// millions of samples that passed the depth test, millions of triangles
				line += " s=" + QByteArray::number( double( samples[p] ) * 1e-6, 'f', 2 ) + "M p="
					+ QByteArray::number( double( prims[p] ) * 1e-6, 'f', 2 ) + "M"
					+ ( s.frags ? QByteArray( QByteArray( " f=" ) + QByteArray::number( double( frags[p] ) * 1e-6, 'f', 2 ) + "M" ) : QByteArray() );
		}
		line += QByteArray( " | cache made " ) + QByteArray::number( sl.cache.made ) + " ("
			+ QByteArray::number( double( sl.cache.madeBytes ) / 1048576.0, 'f', 1 ) + " MB) evicted "
			+ QByteArray::number( sl.cache.evicted ) + " held " + QByteArray::number( sl.cache.usedBytes >> 20 ) + "/"
			+ QByteArray::number( sl.cache.maxBytes >> 20 ) + " MB | textures made " + QByteArray::number( sl.cache.texMade )
			+ " (" + QByteArray::number( double( sl.cache.texBytes ) / 1048576.0, 'f', 1 ) + " MB) | private "
			+ QByteArray::number( sl.privMb ) + " MB";
		static const char * const subName[SubCount] = { "air-tile", "air-inject", "air-integrate", "ao-geometry",
			"ao-stages", "ao-gi", "tone-blit", "tone-measure", "tone-bloom", "tone-quads", "far-dots",
			"far-cards-lit", "tone-vrs", "fxres-down", "fxres-layer", "fxres-comp", "air-pre", "air-far", "air-post", "far-ground-paint",
			"far-ground-draw", "sun-casters", "sun-terrstep", "cloud-shadow", "far-ground-pass",	// lane PERFFIX2 b
			"ssgi-hiz", "ssgi-mips", "ssgi-ecast", "ssgi-echain", "ssgi-cast", "ssgi-temporal",
			"ssgi-denoise", "contact",	// lane PERFFIX2 c
			"water", "water-copy", "underwater",
			"post-dof", "post-mblur", "post-objblur", "post-flare",
			"air-lampvol",	// lane MODULES1 part 25	// lane POSTFX1
			"vk-bloom" };	// lane VKFRAME1
		for ( int k = 0; k < SubCount; k++ )
			if ( subSeen[k] )
				line += QByteArray( " | sub " ) + subName[k] + " " + QByteArray::number( sub[k], 'f', 2 );
		static const char * const cpuSubName[CpuSubCount] = { "setup", "cell-uni", "sun-uni", "fog-uni", "ao-draw", "ssr-draw",
			"air-state", "air-setup", "air-clusters", "air-tile", "air-inject", "air-integrate", "air-far", "air-tail", "air-echo",
			"bl-geo", "bl-row", "sun-casters", "sun-terrstep", "cloud-shadow", "far-ground-pass",	// lane PERFFIX2 b
			"water", "underwater", "sun-walk", "ce1-tail", "ao-collect", "ao-sorted", "ao-effects" };
		for ( int k = 0; k < CpuSubCount; k++ )
			if ( sl.cpuSubN[k] > 0 )
				line += QByteArray( " | cpu " ) + cpuSubName[k] + " " + QByteArray::number( sl.cpuSub[k], 'f', 2 ) + " n="
					+ QByteArray::number( sl.cpuSubN[k] );
		if ( s.lightLoop )
			line += ( sl.frame & 1 ) ? " | lights 0" : " | lights 1";
		if ( s.log )
			std::fprintf( stderr, "%s\n", line.constData() );
		if ( WwTelemetry::costDue( sl.frame ) )
			costRow( sl, gpu, gpuFrame, sub, subSeen, subName, cpuSubName );
	}
	s.winGpuFrames++;
	s.winGpuFrame += gpuFrame;
	for ( int p = 0; p < Count; p++ )
		s.winGpu[p] += gpu[p];
	sl.marks.clear();
	sl.frame = -1;
}

} // namespace

CacheCounts & cacheCounts()
{
	static CacheCounts c;
	return c;
}

const char * name( int pass )
{
	static const char * const k[Count] = { "sun-cascades", "light-cubes", "sky", "obscurance", "reflections", "air",
		"measure", "umbra-depth", "impostors", "opaque", "sorted", "effects", "far-cards", "tone-map", "post",
		"overlays", "ssgi", "refl-probes", "water", "ssr-water", "ssr-donor", "contact", "cloud-shadows", "far-ground",
		"precip", "underwater", "clouds", "taa", "cull-camera", "cull-cascades", "shape-setup" };	// lane TIMING1: water .. taa
	return pass >= 0 && pass < Count ? k[pass] : "?";
}

/* lane EMPTY240 R3-0 (build 74: GPU paints of 1.4 s at the first frames and after cell attaches): the GL work of each
 * paint split by its top-level passes and named zones, always on, read only once available (no stall). A paint over
 * 250 ms on the GPU prints `frame gpu zones:` with every zone, the gap no zone holds and what the paint uploaded.
 * Red WW_GPUZONES_RED=1: no zones. */
namespace
{
struct WatchZone
{
	const char * name;
	GLuint q0, q1;
	bool closed;
};
struct WatchSlot
{
	bool pending = false;
	quint64 frame = 0;
	GLuint f0 = 0, f1 = 0;
	std::vector<WatchZone> zones;
	CacheCounts c0, c;
};
constexpr int kWatchSlots = 8;
constexpr int kNotOpen = -100;
struct Watch
{
	bool loaded = false, red = false, inFrame = false;
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_3_3_Core * gl = nullptr;
	std::vector<GLuint> pool;
	WatchSlot ring[kWatchSlots];
	int cur = 0, depth = 0;
	int open[Count];
	quint64 n = 0;
};
Watch & watch()
{
	static Watch w;
	if ( !w.loaded ) {
		w.loaded = true;
		w.red = wwEnvInt( "WW_GPUZONES_RED" ) == 1;
		for ( int & o : w.open )
			o = kNotOpen;
	}
	return w;
}
GLuint watchQuery()
{
	Watch & w = watch();
	if ( w.pool.empty() ) {
		GLuint q[64];
		w.gl->glGenQueries( 64, q );
		w.pool.insert( w.pool.end(), q, q + 64 );
	}
	const GLuint q = w.pool.back();
	w.pool.pop_back();
	return q;
}
double watchMs( GLuint a, GLuint b )
{
	GLuint64 t0 = 0, t1 = 0;
	watch().gl->glGetQueryObjectui64v( a, GL_QUERY_RESULT, &t0 );
	watch().gl->glGetQueryObjectui64v( b, GL_QUERY_RESULT, &t1 );
	return t1 > t0 ? double( t1 - t0 ) * 1e-6 : 0.0;
}
void watchCollect( WatchSlot & sl )
{
	Watch & w = watch();
	if ( !sl.pending )
		return;
	GLuint avail = 0;
	w.gl->glGetQueryObjectuiv( sl.f1, GL_QUERY_RESULT_AVAILABLE, &avail );
	if ( !avail )
		return;
	sl.pending = false;
	const double total = watchMs( sl.f0, sl.f1 );
	if ( total > 250.0 ) {
		// zones of one name summed, in first-seen order
		std::vector<const char *> names;
		std::vector<double> ms;
		std::vector<int> times;
		double held = 0.0;
		for ( const WatchZone & z : sl.zones ) {
			if ( !z.closed )
				continue;
			const double m = watchMs( z.q0, z.q1 );
			held += m;
			size_t k = 0;
			while ( k < names.size() && std::strcmp( names[k], z.name ) != 0 )
				k++;
			if ( k == names.size() ) {
				names.push_back( z.name );
				ms.push_back( 0.0 );
				times.push_back( 0 );
			}
			ms[k] += m;
			times[k]++;
		}
		QByteArray line = "frame gpu zones: frame " + QByteArray::number( sl.frame ) + " took "
			+ QByteArray::number( total, 'f', 1 ) + " ms on the GPU =";
		for ( size_t k = 0; k < names.size(); k++ ) {
			if ( ms[k] < 0.5 )
				continue;
			line += " " + QByteArray( names[k] ) + " " + QByteArray::number( ms[k], 'f', 1 );
			if ( times[k] > 1 )
				line += " (x" + QByteArray::number( times[k] ) + ")";
			line += " +";
		}
		line += " outside the zones " + QByteArray::number( std::max( 0.0, total - held ), 'f', 1 )
			+ " | this paint uploaded " + QByteArray::number( sl.c.texMade ) + " textures ("
			+ QByteArray::number( double( sl.c.texBytes ) / 1048576.0, 'f', 1 ) + " MB; "
			+ QByteArray::number( sl.c.texPumped ) + " by the pump), " + QByteArray::number( sl.c.made ) + " meshes ("
			+ QByteArray::number( double( sl.c.madeBytes ) / 1048576.0, 'f', 1 ) + " MB)";
		std::fprintf( stderr, "%s\n", line.constData() );
	}
	for ( const WatchZone & z : sl.zones ) {
		w.pool.push_back( z.q0 );
		w.pool.push_back( z.q1 );
	}
	sl.zones.clear();
	w.pool.push_back( sl.f0 );
	w.pool.push_back( sl.f1 );
}
void watchPass( int pass, bool opening )
{
	Watch & w = watch();
	if ( pass < 0 || pass >= CullCamera )
		return;
	if ( opening ) {
		if ( w.open[pass] == kNotOpen && w.inFrame )
			w.open[pass] = zoneBegin( name( pass ) );
	} else if ( w.open[pass] != kNotOpen ) {
		zoneEnd( w.open[pass] );
		w.open[pass] = kNotOpen;
	}
}
}	// namespace

void gpuWatchFrame( bool opening, quint64 frame )
{
	Watch & w = watch();
	if ( w.red )
		return;
	if ( opening ) {
		w.inFrame = false;
		QOpenGLContext * c = QOpenGLContext::currentContext();
		if ( !c )
			return;
		if ( c != w.ctx ) {
			w.ctx = c;
			w.gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>( c );
			w.pool.clear();
			for ( WatchSlot & sl : w.ring ) {
				sl.pending = false;
				sl.zones.clear();
			}
		}
		if ( !w.gl )
			return;
		for ( WatchSlot & sl : w.ring )
			watchCollect( sl );
		const int k = int( w.n++ % quint64( kWatchSlots ) );
		WatchSlot & sl = w.ring[k];
		if ( sl.pending )
			return;	// still on the GPU after 8 paints: this paint goes untimed
		sl.frame = frame;
		sl.f0 = watchQuery();
		sl.f1 = watchQuery();
		sl.c0 = cacheCounts();
		w.gl->glQueryCounter( sl.f0, GL_TIMESTAMP );
		w.cur = k;
		w.depth = 0;
		for ( int & o : w.open )
			o = kNotOpen;
		w.inFrame = true;
		return;
	}
	if ( !w.inFrame )
		return;
	WatchSlot & sl = w.ring[w.cur];
	for ( WatchZone & z : sl.zones )
		if ( !z.closed ) {
			w.gl->glQueryCounter( z.q1, GL_TIMESTAMP );
			z.closed = true;
		}
	w.gl->glQueryCounter( sl.f1, GL_TIMESTAMP );
	const CacheCounts & cc = cacheCounts();
	sl.c.made = cc.made - sl.c0.made;
	sl.c.madeBytes = cc.madeBytes - sl.c0.madeBytes;
	sl.c.texMade = cc.texMade - sl.c0.texMade;
	sl.c.texBytes = cc.texBytes - sl.c0.texBytes;
	sl.c.texPumped = cc.texPumped - sl.c0.texPumped;
	sl.pending = true;
	w.inFrame = false;
}

int zoneBegin( const char * zoneName )
{
	Watch & w = watch();
	if ( !w.inFrame )
		return -1;
	if ( w.depth++ > 0 )
		return -2;	// nested: the outer zone holds it
	WatchSlot & sl = w.ring[w.cur];
	WatchZone z{ zoneName, watchQuery(), watchQuery(), false };
	w.gl->glQueryCounter( z.q0, GL_TIMESTAMP );
	sl.zones.push_back( z );
	return int( sl.zones.size() ) - 1;
}

void zoneEnd( int zone )
{
	Watch & w = watch();
	if ( zone == -1 || !w.inFrame )
		return;
	if ( w.depth > 0 )
		w.depth--;
	WatchSlot & sl = w.ring[w.cur];
	if ( zone < 0 || size_t( zone ) >= sl.zones.size() || sl.zones[size_t( zone )].closed )
		return;
	w.gl->glQueryCounter( sl.zones[size_t( zone )].q1, GL_TIMESTAMP );
	sl.zones[size_t( zone )].closed = true;
}

int openPass()
{
	const State & s = st();
	return s.on && s.inFrame && !s.stack.empty() ? s.stack.back() : -1;
}

bool lightLoopOff()
{
	const State & s = st();
	return s.on && s.lightLoop && s.inFrame && ( s.frame & 1 ) != 0;
}

bool on()
{
	return st().on;
}

bool pinned()
{
	return st().pinned;
}

void setOn( bool v )
{
	State & s = st();
	s.on = v;
	if ( !s.pinned )
		QSettings().setValue( QStringLiteral( "WW/PassTimers" ), v );
	if ( !v )
		s.lines.clear();
}

qint64 nowNs()
{
	return st().clock.nsecsElapsed();
}

void frameBegin()
{
	::wwMainStep( "a frame's start" );	// lane PERFLOAD1 P5: a stall inside a frame names its pass, timers on or off
	State & s = st();
	if ( !s.on ) {
		s.inFrame = false;
		return;
	}
	QOpenGLContext * c = QOpenGLContext::currentContext();
	if ( !c ) {
		s.inFrame = false;
		return;
	}
	if ( c != s.ctx ) {
		// another context: the old one's queries are not ours to read here
		s.ctx = c;
		s.gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>( c );
		s.pool.clear();
		s.cpool.clear();
		for ( Slot & sl : s.ring ) {
			sl.marks.clear();
			sl.subs.clear();
			sl.frame = -1;
		}
	}
	if ( !s.gl ) {
		s.inFrame = false;
		return;
	}
	Slot & sl = s.ring[s.frame % kSlots];
	collect( sl );	// three frames old: finished
	sl.frame = s.frame;
	for ( int p = 0; p < Count; p++ ) {
		sl.cpu[p] = sl.wait[p] = 0.0;
		sl.calls[p] = sl.ranges[p] = 0;
	}
	for ( int k = 0; k < CpuSubCount; k++ ) {
		sl.cpuSub[k] = 0.0;
		sl.cpuSubN[k] = 0;
	}
	s.stack.clear();
	for ( int & m : s.openSub )
		m = -1;
	Sampler::start();
	Sampler::pass = -1;
	sl.f0 = query();
	sl.f1 = query();
	sl.cache0 = cacheCounts();
	s.gl->glQueryCounter( sl.f0, GL_TIMESTAMP );
	s.frameT0 = nowNs();
	s.inFrame = true;
}

void begin( int pass )
{
	::wwMainStep( name( pass ) );	// lane PERFLOAD1 P5
	watchPass( pass, true );	// lane EMPTY240 R3-0
	State & s = st();
	if ( !s.inFrame || pass < 0 || pass >= Count || s.openMark[pass] >= 0 )
		return;
	if ( pass < CullCamera && !s.stack.empty() )
		return;	// nested (the measure's own scene draw): the outer pass holds its time and its draws
	Slot & sl = s.ring[s.frame % kSlots];
	if ( s.sync && pass < CullCamera )
		s.gl->glFinish();
	s.open[pass] = nowNs();
	if ( pass < CullCamera ) {
		Mark m{ pass, query(), query() };
		s.gl->glQueryCounter( m.q0, GL_TIMESTAMP );
		if ( s.census ) {	// top-level passes never nest (above), so one query of each target is open at a time
			m.qs = cquery();
			m.qp = cquery();
			s.gl->glBeginQuery( GL_SAMPLES_PASSED, m.qs );
			s.gl->glBeginQuery( GL_PRIMITIVES_GENERATED, m.qp );
			if ( s.frags ) {
				m.qf = fquery();
				s.gl->glBeginQuery( kFragInv, m.qf );
			}
		}
		sl.marks.push_back( m );
		s.openMark[pass] = int( sl.marks.size() ) - 1;
		s.stack.push_back( pass );
		Sampler::pass = pass;
	} else {
		s.openMark[pass] = 0;
	}
}

void end( int pass )
{
	watchPass( pass, false );	// lane EMPTY240 R3-0
	State & s = st();
	if ( !s.inFrame || pass < 0 || pass >= Count || s.openMark[pass] < 0 )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	if ( s.sync && pass < CullCamera ) {
		const qint64 w0 = nowNs();
		s.gl->glFinish();
		sl.wait[pass] += double( nowNs() - w0 ) * 1e-6;
	}
	sl.cpu[pass] += double( nowNs() - s.open[pass] ) * 1e-6;
	if ( pass < CullCamera ) {
		s.gl->glQueryCounter( sl.marks[s.openMark[pass]].q1, GL_TIMESTAMP );
		if ( sl.marks[s.openMark[pass]].qs ) {
			s.gl->glEndQuery( GL_SAMPLES_PASSED );
			s.gl->glEndQuery( GL_PRIMITIVES_GENERATED );
			if ( sl.marks[s.openMark[pass]].qf )
				s.gl->glEndQuery( kFragInv );
		}
		for ( size_t k = s.stack.size(); k-- > 0; )
			if ( s.stack[k] == pass ) {
				s.stack.erase( s.stack.begin() + std::ptrdiff_t( k ) );
				break;
			}
		Sampler::pass = s.stack.empty() ? -1 : s.stack.back();
	}
	s.openMark[pass] = -1;
}

void cpuSubAdd( int sub, qint64 ns )
{
	State & s = st();
	if ( !s.inFrame || sub < 0 || sub >= CpuSubCount )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	sl.cpuSub[sub] += double( ns ) * 1e-6;
	sl.cpuSubN[sub]++;
}

void addCpu( int pass, qint64 ns )
{
	State & s = st();
	if ( s.inFrame && pass >= 0 && pass < Count )
		s.ring[s.frame % kSlots].cpu[pass] += double( ns ) * 1e-6;
}

void countDraw( qint64 ranges )
{
	State & s = st();
	if ( !s.inFrame )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	const int p = s.stack.empty() ? int( Overlays ) : s.stack.back();
	sl.calls[p]++;
	sl.ranges[p] += ranges;
}

void subBegin( int sub )
{
	State & s = st();
	if ( !s.inFrame || sub < 0 || sub >= SubCount || s.openSub[sub] >= 0 )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	Mark m{ sub, query(), query() };
	s.gl->glQueryCounter( m.q0, GL_TIMESTAMP );
	sl.subs.push_back( m );
	s.openSub[sub] = int( sl.subs.size() ) - 1;
}

void subEnd( int sub )
{
	State & s = st();
	if ( !s.inFrame || sub < 0 || sub >= SubCount || s.openSub[sub] < 0 )
		return;
	s.gl->glQueryCounter( s.ring[s.frame % kSlots].subs[s.openSub[sub]].q1, GL_TIMESTAMP );
	s.openSub[sub] = -1;
}

void subAddMs( int sub, double ms )
{
	State & s = st();
	if ( !s.inFrame || sub < 0 || sub >= SubCount )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	sl.subExt[sub] += ms;
	sl.subExtSeen[sub] = true;
}

static double s_lastCpuSum = 0.0;	// lane EMPTY240 item 1

double lastFrameCpuSumMs()
{
	return s_lastCpuSum;
}

void frameEnd()
{
	State & s = st();
	if ( s.memArmed > 0 && --s.memArmed == 0 )
		memLog( "first frames drawn" );
	s_lastCpuSum = 0.0;
	if ( !s.inFrame )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	for ( int p = 0; p < Count; p++ ) {	// the passes still open (Overlays) close with the frame
		if ( s.openMark[p] >= 0 )
			end( p );
	}
	for ( int k = 0; k < SubCount; k++ )
		subEnd( k );
	s.inFrame = false;
	Sampler::pass = -2;
	s.gl->glQueryCounter( sl.f1, GL_TIMESTAMP );
	sl.cpuFrame = double( nowNs() - s.frameT0 ) * 1e-6;
	for ( int p = 0; p < CullCamera; p++ )	// the CPU-only rows after it sit inside the passes
		s_lastCpuSum += sl.cpu[p];
	const CacheCounts & cc = cacheCounts();
	sl.cache.made = cc.made - sl.cache0.made;
	sl.cache.madeBytes = cc.madeBytes - sl.cache0.madeBytes;
	sl.cache.evicted = cc.evicted - sl.cache0.evicted;
	sl.cache.usedBytes = cc.usedBytes;
	sl.cache.maxBytes = cc.maxBytes;
	sl.cache.texMade = cc.texMade - sl.cache0.texMade;
	sl.cache.texBytes = cc.texBytes - sl.cache0.texBytes;
	if ( s.log ) {
		qint64 wm = 0;
		memory( sl.privMb, wm );
	}
	// the frame two before this one: its queries are finished (or about to be)
	collect( s.ring[( s.frame + 1 ) % kSlots] );
	s.frame++;

	const qint64 now = nowNs();
	if ( s.winT0 < 0 )
		s.winT0 = now;
	s.winFrames++;
	s.winCpuFrame += sl.cpuFrame;
	for ( int p = 0; p < Count; p++ )
		s.winCpu[p] += sl.cpu[p];
	if ( now - s.winT0 >= 1000000000LL && s.winGpuFrames > 0 ) {
		const double secs = double( now - s.winT0 ) * 1e-9;
		const double fn = double( s.winFrames ), gn = double( s.winGpuFrames );
		QStringList l;
		l << QStringLiteral( "%1 fps   frame CPU %2 ms   GPU %3 ms" ).arg( fn / secs, 0, 'f', 1 )
			.arg( s.winCpuFrame / fn, 0, 'f', 1 ).arg( s.winGpuFrame / gn, 0, 'f', 1 );
		for ( int p = 0; p < Count; p++ ) {
			const double c = s.winCpu[p] / fn, g = s.winGpu[p] / gn;
			if ( c >= 0.05 || g >= 0.05 )
				l << QStringLiteral( "%1  %2 / %3" ).arg( QLatin1StringView( name( p ) ) ).arg( c, 0, 'f', 1 )
					.arg( p < CullCamera ? QString::number( g, 'f', 1 ) : QStringLiteral( "-" ) );
		}
		qint64 pm = 0, wm = 0;
		memory( pm, wm );
		l << QStringLiteral( "private %1 GB" ).arg( double( pm ) / 1024.0, 0, 'f', 2 );
		s.lines = l;
		s.winT0 = now;
		s.winFrames = s.winGpuFrames = 0;
		s.winCpuFrame = s.winGpuFrame = 0.0;
		for ( int p = 0; p < Count; p++ )
			s.winCpu[p] = s.winGpu[p] = 0.0;
	}
}

QStringList overlay()
{
	return st().on && st().text ? st().lines : QStringList();
}

void memory( qint64 & privateMb, qint64 & workingMb )
{
	privateMb = workingMb = 0;
#ifdef Q_OS_WIN
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>( &pmc ), sizeof( pmc ) ) ) {
		privateMb = qint64( pmc.PrivateUsage >> 20 );
		workingMb = qint64( pmc.WorkingSetSize >> 20 );
	}
#endif
}

void profileFromStart()
{
	if ( wwEnvInt( "WW_PROF_SAMPLE_ALL" ) == 1 )
		Sampler::start();
}

double processSeconds()
{
#ifdef Q_OS_WIN
	FILETIME c, e, k, u, now;
	if ( GetProcessTimes( GetCurrentProcess(), &c, &e, &k, &u ) ) {
		GetSystemTimeAsFileTime( &now );
		const auto v = []( const FILETIME & f ) { return ( qint64( f.dwHighDateTime ) << 32 ) | qint64( f.dwLowDateTime ); };
		return double( v( now ) - v( c ) ) * 1e-7;
	}
#endif
	return 0.0;
}

void memLog( const char * stage )
{
	qint64 pm = 0, wm = 0;
	memory( pm, wm );
	std::fprintf( stderr, "cell mem: %s private=%lld MB ws=%lld MB t=%.1f s\n", stage, (long long)pm, (long long)wm,
		processSeconds() );
	State & s = st();
	if ( qstrcmp( stage, "first frames drawn" ) != 0 )
		s.memArmed = 3;	// the next frames' figure, once the build is done
	if ( qstrcmp( stage, "before the build" ) == 0 )
		stream1LoadArm();	// lane STREAM1: a cell open starts: time it to its first full frame
}

// lane STREAM1 ------------------------------------------------------------------------------------------------

bool stream1Red( const char * what )
{
	static const QByteArray v = qgetenv( "WW_STREAM1_RED" );
	if ( v.isEmpty() )
		return false;
	if ( v == "all" || v == "1" )
		return true;
	for ( const QByteArray & w : v.split( ',' ) ) {
		if ( w.trimmed() == what )
			return true;
	}
	return false;
}

qint64 videoMemoryMb( qint64 * usedMb )
{
	if ( usedMb )
		*usedMb = 0;
	QOpenGLContext * c = QOpenGLContext::currentContext();
	if ( !c )
		return 0;
	QOpenGLFunctions * f = c->functions();
	if ( c->hasExtension( QByteArrayLiteral( "GL_NVX_gpu_memory_info" ) ) ) {
		GLint total = 0, avail = 0;
		f->glGetIntegerv( 0x9047, &total );	// GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX (KB)
		f->glGetIntegerv( 0x9049, &avail );	// GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX (KB)
		if ( usedMb )
			*usedMb = qint64( total - avail ) >> 10;
		return qint64( total ) >> 10;
	}
	return 0;
}

/* MODULES1 (11-06): where the working set lives, once at the first full frame. Committed regions by type (private
 * <1 MB / 1-64 MB / >=64 MB, mapped files, images) with the resident share sampled through QueryWorkingSetEx (up to 64
 * pages a region, scaled), plus the process heaps' busy bytes. Private >=64 MB with little resident is driver commit. */
#ifdef Q_OS_WIN
static QString ramOwnersLine()
{
	static const char * kName[5] = { "private<1MB", "private1-64MB", "private>=64MB", "mapped", "image" };
	double commit[5] = {}, res[5] = {};
	MEMORY_BASIC_INFORMATION m;
	const char * p = nullptr;
	SYSTEM_INFO si;
	GetSystemInfo( &si );
	const qint64 page = si.dwPageSize ? si.dwPageSize : 4096;
	std::vector<PSAPI_WORKING_SET_EX_INFORMATION> q;
	while ( VirtualQuery( p, &m, sizeof( m ) ) == sizeof( m ) ) {
		if ( m.State == MEM_COMMIT ) {
			const qint64 n = qint64( m.RegionSize );
			const int b = m.Type == MEM_MAPPED ? 3 : m.Type == MEM_IMAGE ? 4
				: n < ( 1 << 20 ) ? 0 : n < ( qint64( 64 ) << 20 ) ? 1 : 2;
			const qint64 pages = n / page;
			const qint64 k = std::min<qint64>( pages, 64 );
			q.assign( size_t( k ), PSAPI_WORKING_SET_EX_INFORMATION {} );
			for ( qint64 i = 0; i < k; i++ )
				q[size_t( i )].VirtualAddress = const_cast<char *>( static_cast<const char *>( m.BaseAddress ) + ( i * pages / k ) * page );
			qint64 valid = 0;
			if ( k && QueryWorkingSetEx( GetCurrentProcess(), q.data(), DWORD( k * sizeof( q[0] ) ) ) )
				for ( const auto & e : q )
					valid += e.VirtualAttributes.Valid ? 1 : 0;
			commit[b] += double( n );
			res[b] += k ? double( n ) * double( valid ) / double( k ) : 0.0;
		}
		const char * next = static_cast<const char *>( m.BaseAddress ) + m.RegionSize;
		if ( next <= p )
			break;
		p = next;
	}
	QString s = QStringLiteral( "ram owners:" );
	for ( int b = 0; b < 5; b++ )
		s += QString::asprintf( " %s commit=%.0f ws~%.0f MB", kName[b], commit[b] / 1048576.0, res[b] / 1048576.0 );
	PROCESS_HEAP_ENTRY e;
	double busy = 0;
	// MODULES1 p18: busy heap bytes by block size (who owns the heaps: many small = document items, big = arrays)
	static const double kEdge[5] = { 256.0, 4096.0, 65536.0, 1048576.0, 16777216.0 };
	double sz[6] = {}, cnt[6] = {};
	HANDLE heaps[64];
	const DWORD nh = std::min<DWORD>( GetProcessHeaps( 64, heaps ), 64 );
	for ( DWORD i = 0; i < nh; i++ ) {
		if ( !HeapLock( heaps[i] ) )
			continue;
		e.lpData = nullptr;
		while ( HeapWalk( heaps[i], &e ) )
			if ( e.wFlags & PROCESS_HEAP_ENTRY_BUSY ) {
				busy += double( e.cbData );
				int k = 0;
				while ( k < 5 && double( e.cbData ) >= kEdge[k] )
					k++;
				sz[k] += double( e.cbData );
				cnt[k] += 1.0;
			}
		HeapUnlock( heaps[i] );
	}
	double trimMs = 0.0;
	const unsigned long long trims = (unsigned long long)FileBuffer::mappedViewTrims( &trimMs );
	s += QString::asprintf( " | heaps %lu busy=%.0f MB | staged=%.0f MB direct=%.0f MB | archive trims=%llu (max %.1f ms)",
		(unsigned long)nh, busy / 1048576.0, double( gpuStageCounts().stagedBytes.load() ) / 1048576.0,
		double( gpuStageCounts().directBytes.load() ) / 1048576.0, trims, trimMs );
	static const char * kSz[6] = { "<256B", "<4K", "<64K", "<1M", "<16M", ">=16M" };
	s += QStringLiteral( " | heap blocks" );
	for ( int k = 0; k < 6; k++ )
		s += QString::asprintf( " %s %.0fk=%.0f MB", kSz[k], cnt[k] / 1000.0, sz[k] / 1048576.0 );
	return s;
}
#endif

QString stream1MemLine( const char * stage )
{
	const CacheCounts & cc = cacheCounts();
	qint64 pm = 0, wm = 0, used = 0;
	memory( pm, wm );
	const qint64 total = videoMemoryMb( &used );
	return QString::asprintf( "stream1 mem: %s geo=%.1f MB (fp32 %.1f MB) shapes=%lld budget=%.0f MB tex=%.1f MB (%lld)"
		" pumped=%lld (%.1f MB) vram=%lld/%lld MB private=%lld MB ws=%lld MB t=%.1f s staged=%.1f MB direct=%.1f MB", stage,
		double( cc.geoGpuBytes ) / 1048576.0, double( cc.geoFp32Bytes ) / 1048576.0, (long long)cc.geoShapes,
		double( cc.maxBytes ) / 1048576.0, double( cc.texResidentBytes ) / 1048576.0, (long long)cc.texResident,
		(long long)cc.texPumped, double( cc.texPumpedBytes ) / 1048576.0, (long long)used, (long long)total,
		(long long)pm, (long long)wm, processSeconds(), double( gpuStageCounts().stagedBytes.load() ) / 1048576.0,
		double( gpuStageCounts().directBytes.load() ) / 1048576.0 );
}

namespace
{
struct Stream1Load
{
	bool armed = false, fired = false;
	double t0 = 0.0;
	qint64 made0 = 0, tex0 = 0;	// counts at the open's start
	qint64 madeP = 0, madeBytesP = 0, texP = 0;	// counts after this paint's pump
	double hitchMax = 0.0;
	int hitchOver = 0, hitchFrames = 0;
};
Stream1Load & s1()
{
	static Stream1Load v;
	return v;
}
} // namespace

void stream1LoadArm()
{
	Stream1Load & l = s1();
	const CacheCounts & cc = cacheCounts();
	l.armed = true;
	l.fired = false;
	l.t0 = processSeconds();
	l.made0 = cc.made;
	l.tex0 = cc.texMade;
	l.hitchMax = 0.0;
	l.hitchOver = l.hitchFrames = 0;
}

bool stream1Loaded()
{
	return s1().fired;
}

void stream1PaintBegin()
{
	Stream1Load & l = s1();
	const CacheCounts & cc = cacheCounts();
	l.madeP = cc.made;
	l.madeBytesP = cc.madeBytes;
	l.texP = cc.texMade;
}

void stream1PaintEnd( bool ready, double paintMs )
{
	Stream1Load & l = s1();
	const CacheCounts & cc = cacheCounts();
	if ( l.armed && !l.fired ) {
		const qint64 tex = cc.texMade - l.texP, made = cc.made - l.madeP, madeBytes = cc.madeBytes - l.madeBytesP;
		if ( ready && tex == 0 && madeBytes < ( 1LL << 20 ) && cc.texMade > l.tex0 && cc.made > l.made0 ) {
			l.fired = true;
			const double t = processSeconds();
			std::fprintf( stderr, "stream1 load: first full frame t=%.1f s (%.1f s after the build started) textures=%lld"
				" shapes=%lld this paint: %lld shapes %lld bytes\n", t, t - l.t0, (long long)( cc.texMade - l.tex0 ),
				(long long)( cc.made - l.made0 ), (long long)made, (long long)madeBytes );
			std::fprintf( stderr, "%s\n", qPrintable( stream1MemLine( "first full frame" ) ) );
#ifdef Q_OS_WIN
			std::fprintf( stderr, "%s\n", qPrintable( ramOwnersLine() ) );
#endif
		}
		return;
	}
	if ( !l.fired )
		return;
	l.hitchFrames++;
	l.hitchMax = std::max( l.hitchMax, paintMs );
	if ( paintMs > 33.3 )
		l.hitchOver++;
}

namespace
{
std::unordered_map< unsigned int, qint64 > & texResidentMap()
{
	static std::unordered_map< unsigned int, qint64 > v;
	return v;
}
} // namespace

void texResidentSet( unsigned int id, qint64 bytes )
{
	CacheCounts & cc = cacheCounts();
	qint64 & b = texResidentMap()[id];
	if ( b ) {
		cc.texResident--;
		cc.texResidentBytes -= b;
	}
	b = bytes;
	cc.texResident++;
	cc.texResidentBytes += bytes;
}

qint64 texResidentBytesOf( unsigned int id )
{
	const auto & m = texResidentMap();
	const auto it = m.find( id );
	return it == m.end() ? 0 : it->second;
}

void texResidentDrop( unsigned int id )
{
	auto & m = texResidentMap();
	auto it = m.find( id );
	if ( it == m.end() )
		return;
	CacheCounts & cc = cacheCounts();
	cc.texResident--;
	cc.texResidentBytes -= it->second;
	m.erase( it );
}

void stream1Hitch( double * maxMs, int * over, int * frames, bool reset )
{
	Stream1Load & l = s1();
	if ( maxMs )
		*maxMs = l.hitchMax;
	if ( over )
		*over = l.hitchOver;
	if ( frames )
		*frames = l.hitchFrames;
	if ( reset ) {
		l.hitchMax = 0.0;
		l.hitchOver = l.hitchFrames = 0;
	}
}

} // namespace PassTime

// lane EMPTY240 ITEM 0: the system timer's resolution, said in the frame pacing line (framepacer.h)
double wwTimerResolutionMs()
{
#ifdef Q_OS_WIN
	using QueryFn = LONG ( WINAPI * )( PULONG, PULONG, PULONG );
	static const QueryFn query = reinterpret_cast<QueryFn>(
		reinterpret_cast<void *>( GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "NtQueryTimerResolution" ) ) );
	ULONG coarsest = 0, finest = 0, current = 0;
	if ( query && query( &coarsest, &finest, &current ) == 0 )
		return double( current ) * 1.0e-4;	// 100 ns units
#endif
	return -1.0;
}

bool wwPreciseSleep( double seconds )
{
#ifdef Q_OS_WIN
	// a Sleep( 1 ) took 14-24 ms on bungo's PC even after timeBeginPeriod( 1 ); this timer 0.55 ms for 0.5 (11-12)
	static HANDLE timer = CreateWaitableTimerExW( nullptr, nullptr, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */,
		TIMER_ALL_ACCESS );
	if ( !timer || seconds <= 0.0 )
		return false;
	LARGE_INTEGER due;
	due.QuadPart = -LONGLONG( seconds * 1.0e7 );	// relative, 100 ns units
	if ( !SetWaitableTimerEx( timer, &due, 0, nullptr, nullptr, nullptr, 0 ) )
		return false;
	WaitForSingleObject( timer, 50 );
	return true;
#else
	(void) seconds;
	return false;
#endif
}
