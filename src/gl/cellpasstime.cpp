#include "cellpasstime.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSettings>
#include <QString>

#include <cstdio>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

// lane PERF1: see cellpasstime.h

namespace PassTime
{

namespace
{

constexpr int kSlots = 3;	// GPU results are read two frames after they were issued

struct Mark
{
	int pass;
	GLuint q0, q1;
};

struct Slot
{
	qint64 frame = -1;
	std::vector<Mark> marks;
	GLuint f0 = 0, f1 = 0;	// the whole frame
	double cpu[Count] = {};
	double cpuFrame = 0.0;
	CacheCounts cache0, cache;
	qint64 privMb = 0;	// private bytes at the frame's end (the log only)	// the geometry cache at the frame's start; its traffic over the frame
};

struct State
{
	bool loaded = false, on = false, pinned = false, log = false;
	bool sync = false;	// WW_CELL_PASSTIME_SYNC=1: a glFinish at each pass edge, so the timestamps hold that pass's work only
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_3_3_Core * gl = nullptr;
	std::vector<GLuint> pool;
	Slot ring[kSlots];
	qint64 frame = 0;
	bool inFrame = false;
	QElapsedTimer clock;
	qint64 frameT0 = 0;
	qint64 open[Count] = {};
	int openMark[Count];
	// the last second's sums, for the overlay
	qint64 winT0 = -1;
	int winFrames = 0, winGpuFrames = 0;
	double winCpu[Count] = {}, winGpu[Count] = {}, winCpuFrame = 0.0, winGpuFrame = 0.0;
	QStringList lines;
	int memArmed = 0;
};

State & st()
{
	static State s;
	if ( !s.loaded ) {
		s.loaded = true;
		s.clock.start();
		const QByteArray pin = qgetenv( "WW_CELL_PASSTIME" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/PassTimers" ), false ).toBool();
		}
		s.log = qEnvironmentVariableIntValue( "WW_CELL_PASSTIME_LOG" ) == 1;
		s.sync = qEnvironmentVariableIntValue( "WW_CELL_PASSTIME_SYNC" ) == 1;
		for ( int & m : s.openMark )
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

double gpuMs( GLuint a, GLuint b )
{
	GLuint64 t0 = 0, t1 = 0;
	st().gl->glGetQueryObjectui64v( a, GL_QUERY_RESULT, &t0 );
	st().gl->glGetQueryObjectui64v( b, GL_QUERY_RESULT, &t1 );
	return t1 > t0 ? double( t1 - t0 ) * 1e-6 : 0.0;
}

//! Reads a finished slot back, logs its frame and adds it to the window; its queries go back to the pool.
void collect( Slot & sl )
{
	State & s = st();
	if ( sl.frame < 0 )
		return;
	double gpu[Count] = {};
	for ( const Mark & m : sl.marks ) {
		gpu[m.pass] += gpuMs( m.q0, m.q1 );
		s.pool.push_back( m.q0 );
		s.pool.push_back( m.q1 );
	}
	const double gpuFrame = gpuMs( sl.f0, sl.f1 );
	s.pool.push_back( sl.f0 );
	s.pool.push_back( sl.f1 );
	if ( s.log ) {
		QByteArray line = QByteArray( "pass time: frame=" ) + QByteArray::number( sl.frame ) + " cpu="
			+ QByteArray::number( sl.cpuFrame, 'f', 2 ) + " gpu=" + QByteArray::number( gpuFrame, 'f', 2 );
		for ( int p = 0; p < Count; p++ ) {
			if ( sl.cpu[p] <= 0.0 && gpu[p] <= 0.0 )
				continue;
			line += QByteArray( " | " ) + name( p ) + " " + QByteArray::number( sl.cpu[p], 'f', 2 ) + "/"
				+ QByteArray::number( gpu[p], 'f', 2 );
		}
		line += QByteArray( " | cache made " ) + QByteArray::number( sl.cache.made ) + " ("
			+ QByteArray::number( double( sl.cache.madeBytes ) / 1048576.0, 'f', 1 ) + " MB) evicted "
			+ QByteArray::number( sl.cache.evicted ) + " held " + QByteArray::number( sl.cache.usedBytes >> 20 ) + "/"
			+ QByteArray::number( sl.cache.maxBytes >> 20 ) + " MB | textures made " + QByteArray::number( sl.cache.texMade )
			+ " (" + QByteArray::number( double( sl.cache.texBytes ) / 1048576.0, 'f', 1 ) + " MB) | private "
			+ QByteArray::number( sl.privMb ) + " MB";
		std::fprintf( stderr, "%s\n", line.constData() );
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
		"overlays", "cull-camera", "cull-cascades" };
	return pass >= 0 && pass < Count ? k[pass] : "?";
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
		for ( Slot & sl : s.ring ) {
			sl.marks.clear();
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
	for ( double & v : sl.cpu )
		v = 0.0;
	sl.f0 = query();
	sl.f1 = query();
	sl.cache0 = cacheCounts();
	s.gl->glQueryCounter( sl.f0, GL_TIMESTAMP );
	s.frameT0 = nowNs();
	s.inFrame = true;
}

void begin( int pass )
{
	State & s = st();
	if ( !s.inFrame || pass < 0 || pass >= Count || s.openMark[pass] >= 0 )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	if ( s.sync && pass < CullCamera )
		s.gl->glFinish();
	s.open[pass] = nowNs();
	if ( pass < CullCamera ) {
		Mark m{ pass, query(), query() };
		s.gl->glQueryCounter( m.q0, GL_TIMESTAMP );
		sl.marks.push_back( m );
		s.openMark[pass] = int( sl.marks.size() ) - 1;
	} else {
		s.openMark[pass] = 0;
	}
}

void end( int pass )
{
	State & s = st();
	if ( !s.inFrame || pass < 0 || pass >= Count || s.openMark[pass] < 0 )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	if ( s.sync && pass < CullCamera )
		s.gl->glFinish();
	sl.cpu[pass] += double( nowNs() - s.open[pass] ) * 1e-6;
	if ( pass < CullCamera )
		s.gl->glQueryCounter( sl.marks[s.openMark[pass]].q1, GL_TIMESTAMP );
	s.openMark[pass] = -1;
}

void addCpu( int pass, qint64 ns )
{
	State & s = st();
	if ( s.inFrame && pass >= 0 && pass < Count )
		s.ring[s.frame % kSlots].cpu[pass] += double( ns ) * 1e-6;
}

void frameEnd()
{
	State & s = st();
	if ( s.memArmed > 0 && --s.memArmed == 0 )
		memLog( "first frames drawn" );
	if ( !s.inFrame )
		return;
	Slot & sl = s.ring[s.frame % kSlots];
	for ( int p = 0; p < Count; p++ ) {	// the passes still open (Overlays) close with the frame
		if ( s.openMark[p] >= 0 )
			end( p );
	}
	s.inFrame = false;
	s.gl->glQueryCounter( sl.f1, GL_TIMESTAMP );
	sl.cpuFrame = double( nowNs() - s.frameT0 ) * 1e-6;
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
	return st().on ? st().lines : QStringList();
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

void memLog( const char * stage )
{
	qint64 pm = 0, wm = 0;
	memory( pm, wm );
	std::fprintf( stderr, "cell mem: %s private=%lld MB ws=%lld MB\n", stage, (long long)pm, (long long)wm );
	State & s = st();
	if ( qstrcmp( stage, "first frames drawn" ) != 0 )
		s.memArmed = 3;	// the next frames' figure, once the build is done
}

} // namespace PassTime
