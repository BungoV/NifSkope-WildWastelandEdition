/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "features/far-lod/lodgen.h"
#include "wwloadwatch.h"	// lane PERFLOAD1: the main-thread step mark and the log drain
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "features/far-lod/lodgengpu.h"
#include "features/gi/proberelight.h"
#include "celldoorlive.h"	// lane DOORLIVE1: the port's NifSkope::door
#include "glview.h"
#include "nifcli.h"
#include "nifskope.h"
#include "hkxanimui.h"
#include "version.h"
#include "features/shared/cellpasstime.h"
#include "harnesswindow.h"
#include "data/nifvalue.h"
#include "model/nifmodel.h"
#include "model/kfmmodel.h"
#include "ui/widgets/timeline.h"

#include <QApplication>

#include "rdccapture.h"
#include <QColorSpace>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QStack>
#include <QSurfaceFormat>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <cstdio>	// lane PERF1: setvbuf

#include "features/gi/probeincr.h"
#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>	// lane STARTSPEED1: GetProcessTimes (the process's own start, before main)
#endif
#include <QElapsedTimer>
#include <cstring>

/* lane STARTSPEED1: the start's stages, one "startup: <stage> <ms> ms (+<ms>)" line each on stderr: ms since the
 * PROCESS was created (the loader and static init before main count), and the step from the stage before.
 * WW_STARTUP_LOG=<file> appends the same lines to that file (a timing runner's record). Marks end at the first
 * painted frame: a later shader reload or a second window prints nothing. */
static double wwStartupNowMs()
{
#ifdef Q_OS_WIN
	FILETIME c, e, k, u, now;
	if ( GetProcessTimes( GetCurrentProcess(), &c, &e, &k, &u ) ) {
		GetSystemTimePreciseAsFileTime( &now );
		const auto q = []( const FILETIME & t ) { return ( quint64( t.dwHighDateTime ) << 32 ) | t.dwLowDateTime; };
		return double( qint64( q( now ) - q( c ) ) ) / 10000.0;
	}
#endif
	static QElapsedTimer t;
	if ( !t.isValid() )
		t.start();
	return double( t.nsecsElapsed() ) / 1e6;
}

static bool wwStartupDone = false;

void wwStartupMark( const char * stage )
{
	static double last = 0.0;
	if ( wwStartupDone )
		return;
	const double t = wwStartupNowMs();
	char line[256];
	std::snprintf( line, sizeof( line ), "startup: %s %.0f ms (+%.0f)\n", stage, t, t - last );
	last = t;
	std::fputs( line, stderr );
	static const QByteArray logPath = qgetenv( "WW_STARTUP_LOG" );
	if ( !logPath.isEmpty() ) {
		if ( FILE * f = std::fopen( logPath.constData(), "ab" ) ) {
			std::fputs( line, f );
			std::fclose( f );
		}
	}
	if ( std::strcmp( stage, "first frame" ) == 0 ) {
		wwStartupDone = true;
		std::fflush( stderr );
		// lane STARTSPEED1: WW_STARTUP_QUIT=1 quits at the first frame (the build's shader-cache warm start)
		if ( qgetenv( "WW_STARTUP_QUIT" ).trimmed() == "1" && QCoreApplication::instance() )
			QMetaObject::invokeMethod( QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection );
	}
}


QCoreApplication * createApplication( int &argc, char *argv[] )
{
	QCoreApplication::setAttribute( Qt::AA_UseDesktopOpenGL );
	QCoreApplication::setAttribute( Qt::AA_ShareOpenGLContexts );
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCoreApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
#endif
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy( Qt::HighDpiScaleFactorRoundingPolicy::PassThrough );
	if ( auto fmt = QSurfaceFormat::defaultFormat(); true ) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		fmt.setColorSpace( QSurfaceFormat::sRGBColorSpace );
#else
		fmt.setColorSpace( QColorSpace( QColorSpace::SRgb ) );
#endif
		QSurfaceFormat::setDefaultFormat( fmt );
	}

	// Iterate over args
	for ( int i = 1; i < argc; ++i ) {
		// -no-gui: start as core app without all the GUI overhead
		if ( !qstrcmp( argv[i], "-no-gui" ) ) {
			/* A headless LOD bake that may use the GPU needs the GUI platform for
			 * its offscreen OpenGL context (src/features/far-lod/lodgengpu.h); it still opens no
			 * window, and a QGuiApplication is not a QApplication, so main()
			 * below still takes the headless path. */
			if ( lodgenGpuWantedForArgs( argc, argv ) || probeRelightGpuWantedForArgs( argc, argv ) )
				return new QGuiApplication( argc, argv );
			return new QCoreApplication( argc, argv );
		}
	}
	return new QApplication( argc, argv );
}


/*
 *  main
 */

//! The main program
void wwCardTimesMainEntry();	// CARDBAKE2, nifskope_ui.cpp
bool wwCardBatchOn();
bool wwCardBatchStart();
// lane PERFLOAD1 (P2, 11-10): the stderr pump and the main-thread watch.
// Measured 11-09: the frame-end flush of the redirected stderr log sat in ONE WriteFile for 12.2 s and for 29.5 s
// (a stalled disk); live replies are printed to stderr first, so the live port was silent too. With the pump stderr
// is a pipe: the main thread's writes end in memory and a writer thread puts them into the log (same bytes, same
// order). WW_STDERR_PUMP=0 is the old direct write. The watch thread prints a line when the main thread has not come
// back to its event loop (or to a load step mark) for 1 s, and then how long that step took; WW_MAIN_WATCH=0 is the
// old silence. Design: a producer/consumer log queue with one writer thread (as spdlog's async logger).
#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace WwPump
{
struct State
{
	std::atomic<bool> on{ false }, stop{ false }, readerDone{ false }, writerDone{ false }, holding{ false };
	int savedFd = -1;
	HANDLE readH = nullptr, outH = nullptr;
	bool text = false;	// fd 2 was in text mode: a note's line ends CR LF
	std::mutex mu;	// guards queue, notes, writerBusy
	std::string queue, notes;
	bool writerBusy = false;
	std::atomic<long long> bytes{ 0 }, writes{ 0 }, slow{ 0 }, longestMs{ 0 }, backlogPeak{ 0 };
};
// never freed: the detached threads may outlive the statics' destructors
State & st()
{
	static State * p = new State;
	return *p;
}

//! One line from any thread (no newline in it): placed at a line start of the log.
void note( const char * line )
{
	State & s = st();
	if ( !s.on ) {
		std::fprintf( stderr, "%s\n", line );
		std::fflush( stderr );
		return;
	}
	std::lock_guard<std::mutex> lock( s.mu );
	s.notes += line;
	s.notes += s.text ? "\r\n" : "\n";
}

//! Pipe -> memory. Polls: a blocking ReadFile would also block the drain's PeekNamedPipe on this handle.
void reader()
{
	State & s = st();
	static char buf[1 << 16];
	for ( ;; ) {
		const bool stopping = s.stop;	// read BEFORE the peek: text written before the stop is still taken
		DWORD avail = 0;
		if ( !PeekNamedPipe( s.readH, nullptr, 0, nullptr, &avail, nullptr ) )
			break;
		if ( !avail ) {
			if ( stopping )
				break;
			Sleep( 2 );
			continue;
		}
		s.holding = true;
		DWORD got = 0;
		if ( !ReadFile( s.readH, buf, sizeof buf, &got, nullptr ) ) {
			s.holding = false;
			break;
		}
		for ( ;; ) {	// 64 MB waiting = the log's disk is far behind: only then the program waits again
			{
				std::lock_guard<std::mutex> lock( s.mu );
				if ( s.queue.size() < ( size_t( 64 ) << 20 ) || s.stop ) {
					s.queue.append( buf, got );
					if ( (long long) s.queue.size() > s.backlogPeak )
						s.backlogPeak = (long long) s.queue.size();
					break;
				}
			}
			Sleep( 2 );
		}
		s.holding = false;
	}
	s.readerDone = true;
}

void writeAll( const char * p, size_t n )
{
	State & s = st();
	while ( n ) {
		const DWORD part = DWORD( n < ( size_t( 1 ) << 20 ) ? n : ( size_t( 1 ) << 20 ) );
		DWORD done = 0;
		const ULONGLONG t0 = GetTickCount64();
		const BOOL ok = WriteFile( s.outH, p, part, &done, nullptr );
		const long long ms = (long long) ( GetTickCount64() - t0 );
		if ( !ok || !done )
			return;	// the log is gone (closed pipe, full disk): the text is dropped, the program never waits for it
		s.bytes += done;
		s.writes++;
		if ( ms >= 250 ) {
			s.slow++;
			if ( ms > s.longestMs )
				s.longestMs = ms;
			char line[160];
			std::snprintf( line, sizeof line, "stderr pump: one log write took %lld ms; the main thread did not wait", ms );
			std::lock_guard<std::mutex> lock( s.mu );
			s.notes += line;
			s.notes += s.text ? "\r\n" : "\n";
		}
		p += done;
		n -= done;
	}
}

//! Memory -> the log. Notes go in at a line start, or after 1 s without one.
void writer()
{
	State & s = st();
	bool lineStart = true;
	int idle = 0;
	std::string out;
	for ( ;; ) {
		out.clear();
		{
			std::lock_guard<std::mutex> lock( s.mu );
			const bool last = s.stop && s.readerDone;
			if ( s.queue.empty() && s.notes.empty() && last )
				break;
			out.swap( s.queue );
			if ( !s.notes.empty() ) {
				const size_t nl = out.find( '\n' );
				bool used = true;
				if ( lineStart )
					out.insert( 0, s.notes );
				else if ( nl != std::string::npos )
					out.insert( nl + 1, s.notes );
				else if ( out.empty() && ( ++idle > 500 || last ) )
					out = ( s.text ? "\r\n" : "\n" ) + s.notes;
				else
					used = false;
				if ( used )
					s.notes.clear();
			}
			s.writerBusy = !out.empty();
		}
		if ( out.empty() ) {
			Sleep( 2 );
			continue;
		}
		idle = 0;
		writeAll( out.data(), out.size() );
		lineStart = out.back() == '\n';
		std::lock_guard<std::mutex> lock( s.mu );
		s.writerBusy = false;
	}
	s.writerDone = true;
}

//! All text written so far is in the log file (up to 5 s).
void drain()
{
	std::fflush( stderr );
	State & s = st();
	if ( !s.on )
		return;
	const ULONGLONG t0 = GetTickCount64();
	while ( GetTickCount64() - t0 < 5000 ) {
		DWORD avail = 0;
		const bool pipeEmpty = !PeekNamedPipe( s.readH, nullptr, 0, nullptr, &avail, nullptr ) || !avail;
		if ( pipeEmpty && !s.holding ) {
			std::lock_guard<std::mutex> lock( s.mu );
			if ( s.queue.empty() && s.notes.empty() && !s.writerBusy )
				return;
		}
		Sleep( 1 );
	}
}

//! At exit: stderr is the log again, the threads empty the queue, one line sums the run up.
void shutdown()
{
	State & s = st();
	if ( !s.on )
		return;
	std::fflush( stderr );
	_dup2( s.savedFd, 2 );
	s.stop = true;
	const ULONGLONG t0 = GetTickCount64();
	while ( !( s.readerDone && s.writerDone ) && GetTickCount64() - t0 < 5000 )
		Sleep( 1 );
	s.on = false;
	std::fprintf( stderr, "stderr pump: %lld bytes in %lld writes, slow writes (250 ms or more) %lld, longest %lld ms, most text waiting %lld KB\n",
		(long long) s.bytes, (long long) s.writes, (long long) s.slow, (long long) s.longestMs, (long long) s.backlogPeak / 1024 );
	std::fflush( stderr );
}
}	// namespace WwPump

static void wwStderrPumpStart()
{
	WwPump::State & s = WwPump::st();
	if ( s.on )
		return;
	std::fflush( stderr );
	if ( wwEnv( "WW_STDERR_PUMP" ) == "0" ) {
		std::fprintf( stderr, "stderr pump: off (WW_STDERR_PUMP=0, the main thread writes the log itself)\n" );
		return;
	}
	const intptr_t h = _get_osfhandle( 2 );
	if ( h == -1 || h == -2 )
		return;	// no stderr at all
	const DWORD type = GetFileType( (HANDLE) h );
	if ( type != FILE_TYPE_DISK && type != FILE_TYPE_PIPE )
		return;	// a console is left alone
	const int oldMode = _setmode( 2, _O_BINARY );
	if ( oldMode == -1 )
		return;
	_setmode( 2, oldMode );
	s.text = ( oldMode & _O_TEXT ) != 0;
	int fds[2] = { -1, -1 };
	s.savedFd = _dup( 2 );
	if ( s.savedFd < 0 )
		return;
	if ( _pipe( fds, 1 << 20, ( s.text ? _O_TEXT : _O_BINARY ) | _O_NOINHERIT ) != 0 ) {
		_close( s.savedFd );
		return;
	}
	s.readH = (HANDLE) _get_osfhandle( fds[0] );
	s.outH = (HANDLE) _get_osfhandle( s.savedFd );
	if ( _dup2( fds[1], 2 ) != 0 ) {
		_close( fds[0] );
		_close( fds[1] );
		_close( s.savedFd );
		return;
	}
	_close( fds[1] );
	s.on = true;
	wwStderrDrainFn = WwPump::drain;
	std::thread( WwPump::reader ).detach();
	std::thread( WwPump::writer ).detach();
	std::atexit( WwPump::shutdown );
	std::fprintf( stderr, "stderr pump: on (log writes leave the main thread; WW_STDERR_PUMP=0 writes them directly)\n" );
}

namespace WwWatch
{
std::atomic<long long> beat{ 0 };
std::atomic<const char *> stage{ "program start" };
std::atomic<bool> off{ false };

void step( const char * what )
{
	stage = what;
	beat = (long long) GetTickCount64();
}

void run()
{
	bool late = false;
	long long since = 0;
	char line[320];
	while ( !off ) {
		Sleep( 100 );
		const long long b = beat;
		const long long n = (long long) GetTickCount64();
		if ( !late && n - b >= 1000 ) {
			late = true;
			since = b;
			const char * what = stage;
			if ( what )
				std::snprintf( line, sizeof line, "main thread: over 1 s without handling events, in the step after \"%s\" (the window and the live port wait)", what );
			else
				std::snprintf( line, sizeof line, "main thread: over 1 s without handling events, inside one event (a frame, a live step or the UI)" );
			WwPump::note( line );
		} else if ( late && b != since ) {
			std::snprintf( line, sizeof line, "main thread: that step took %lld ms", b - since );
			WwPump::note( line );
			late = false;
		}
	}
}
}	// namespace WwWatch

static void wwMainWatchStart( QObject * app )
{
	if ( wwEnv( "WW_MAIN_WATCH" ) == "0" ) {
		std::fprintf( stderr, "main watch: off (WW_MAIN_WATCH=0)\n" );
		return;
	}
	WwWatch::step( "program start" );
	wwMainStepFn = WwWatch::step;
	QTimer * t = new QTimer( app );
	t->setInterval( 100 );
	QObject::connect( t, &QTimer::timeout, app, []() { WwWatch::step( nullptr ); } );
	t->start();
	std::atexit( []() { WwWatch::off = true; } );
	std::thread( WwWatch::run ).detach();
	std::fprintf( stderr, "main watch: on (a main-thread step over 1 s prints its own line; WW_MAIN_WATCH=0 is the old silence)\n" );
}
#else
static void wwStderrPumpStart() {}
static void wwMainWatchStart( QObject * ) {}
#endif

bool wwStderrFrameFlush = false;	// lane FLYFIX1: GLView::paintGL flushes stderr once a frame when set
int main( int argc, char * argv[] )
{
	/* lane PERF1: WW_STDERR_BUFFER=1 (timing harnesses only) holds stderr in a 4 MB buffer, written when full and at
	 * exit. A harness redirects stderr to a file, and there each line cost the frame that printed it ~0.3 s, so the
	 * frame timers measured the harness's writes. Set before any output, as setvbuf asks; a refusal flushes first. */
	const QByteArray wwSb = qgetenv( "WW_STDERR_BUFFER" );
	if ( wwSb == "1" )
		std::setvbuf( stderr, nullptr, _IOFBF, 4 << 20 );
	/* lane FLYFIX1 (bungo 10-23, ~1 fps flying with stderr in a file): unbuffered stderr wrote one char a syscall;
	 * by default it is buffered and flushed once a frame and on warnings. WW_STDERR_BUFFER=0 is the old way. */
	else if ( wwSb != "0" && std::setvbuf( stderr, nullptr, _IOFBF, 64 << 10 ) == 0 )
		wwStderrFrameFlush = true;
	PassTime::profileFromStart();	// lane PERF1: WW_PROF_SAMPLE_ALL=1, the load profiled too
	wwStartupMark( "main entry" );

	wwCardTimesMainEntry();
	// Before anything creates a GL context: RenderDoc cannot hook a driver it
	// was loaded after, and Qt makes the viewport's context when GLView is
	// constructed. No-op unless WW_RDC_FRAMES is set.
	rdcInit();

	/* lane CELLALL1: WW_ALL_ON=1, the "all on" preset for his sheets: every cell-view feature's pin set to on before
	 * anything reads one (each feature reads its pin once, lazily). A pin already set wins (WW_CELL_SSR_EXT=0 beside
	 * WW_ALL_ON=1 keeps that one off). Not in it: the GPU relight (a measurement run: it relights twice and times
	 * it) and Scene mode Lookdev itself (WW_LOOKDEV, the weather that lights an exterior).
	 * MODULES1 (bungo 11-06, "you turn on all our graphical features"): every tier at its top shipped tier, the FO4CS
	 * bloom with the physical glare, the far LOD's AO and sky. Not in it: the old probe GI (WW_CELL_GI, retired: RTXGI
	 * only), depth of field and motion blur (never in his shots). */
	if ( wwEnvInt( "WW_ALL_ON" ) == 1 ) {
		static const char * const kAllOn[][2] = { { "WW_CELL_LIT", "1" }, { "WW_CELL_FAR", "1" }, { "WW_CELL_IS", "1" },
			{ "WW_TAA", "1" }, { "WW_CELL_CULL", "1" }, { "WW_CELL_AO", "1" }, { "WW_CELL_SHADOW", "1" },
			{ "WW_CELL_WATER", "1" }, { "WW_CELL_SSR", "1" }, { "WW_CELL_SSR_EXT", "1" }, { "WW_CELL_AODECAL", "1" },
			{ "WW_LOOKDEV_SHADOWS", "1" }, { "WW_LOOKDEV_FOG", "1" }, { "WW_CELL_AIR", "1" }, { "WW_CELL_VOLPART", "1" },
			{ "WW_CELL_PARTICLES", "1" }, { "WW_CELL_PREVIS", "1" }, { "WW_CELL_FARLOD", "1" }, { "WW_CELL_GRASS", "1" },	// final merge: FARLOD1 + GRASSMB1
			{ "WW_CELL_RTDDGI", "1" }, { "WW_CELL_BLOOM", "fo4cs" }, { "WW_CELL_GLARE", "1" }, { "WW_CELL_LENSFLARE", "1" },
			{ "WW_CELL_FLICKER", "1" }, { "WW_CELL_FOGLEAK", "1" }, { "WW_CELL_SPECOCC", "1" }, { "WW_CELL_XEMI", "1" },
			{ "WW_CELL_XEMI_GI", "1" }, { "WW_CELL_LIGHT_PHYS", "1" }, { "WW_CELL_AIR_CLUSTERS", "1" }, { "WW_CELL_AIR_MS", "1" },
			{ "WW_RENDER_SSGI", "1" }, { "WW_CELL_REFLPROBE", "1" }, { "WW_CELL_WATER_SKYVIS", "1" },
			{ "WW_LOOKDEV_GROUND", "1" }, { "WW_LOOKDEV_CLOUDTIME", "1" }, { "WW_CELL_FARLOD_SWAYTIME", "1" },
			{ "WW_LODL_AO", "1" }, { "WW_LODL_SKY", "1" },
			{ "WW_SHADOW_CASCADE", "ultra" }, { "WW_SHADOW_CONTACT", "ultra" }, { "WW_SHADOW_TERRAIN", "ultra" },
			{ "WW_SUNPATH", "1" }, { "WW_ATMO", "ultra" }, { "WW_CELL_RT_PRESET", "ultra" }, { "WW_CELL_SSGI_QUALITY", "ultra" },
			{ "WW_TAA2", "ultra" }, { "WW_CELL_WATER_PRESET", "ultra" }, { "WW_CELL_LAMPSHADOW", "ultra" },
			{ "WW_CELL_AIR_FAR", "4" }, { "WW_CELL_AO_METHOD", "xegtao" }, { "WW_CELL_AO_QUALITY", "ultra" },
			{ "WW_CLOUDVOL", "ultra" }, { "WW_CELL_AIR_GRID", "ultra" },
			// MODULES1 R6-12: the atmosphere's other rows at their SHIPPED values (a harness pinned them Off, so the
			// Scene panel read Aerial Perspective / Sun and Sky Light / Night Sky / Sky Light in GI = Off in a flight)
			{ "WW_ATMO_AP", "1" }, { "WW_ATMO_SUN", "full" }, { "WW_ATMO_NIGHT", "1" }, { "WW_ATMO_GI", "1" },
			{ "WW_ATMO_BODYSIZE", "game" } };	// R6-12 b: the Sun and Moon Size row's shipped value	// part 18 audit: a harness pins clouds 0 and the grid Low otherwise
		QByteArray set, kept;
		for ( const auto & kv : kAllOn ) {
			const char * k = kv[0];
			if ( wwEnvIsSet( k ) ) {
				kept += QByteArray( " " ) + k + "=" + wwEnv( k );
				continue;
			}
			qputenv( k, kv[1] );
			wwEnvChanged();	// MODULES1 item 8: the env accessors' cache
			set += QByteArray( " " ) + k + "=" + kv[1];
		}
		fprintf( stderr, "all on: set%s | kept%s\n", set.constData(), kept.isEmpty() ? " none" : kept.constData() );
	}

	QScopedPointer<QCoreApplication> app( createApplication( argc, argv ) );
	wwStartupMark( "application" );

	if ( auto a = qobject_cast<QApplication *>(app.data()) ) {
		a->setOrganizationName( "NifTools" );
		a->setOrganizationDomain( "niftools.org" );
		/* WW_SETTINGS_SCOPE gives a harness run its own settings key, so a
		 * gate can PLANT window geometry without borrowing the user's and
		 * without having to restore it afterwards (lane HARNESSWIN1,
		 * 2026-09-19). Empty in an ordinary session. */
		a->setApplicationName( "NifSkope " + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION )
			+ wwHarnessSettingsSuffix() );
		a->setApplicationVersion( NIFSKOPE_VERSION );
		/* The display name carries the edition; applicationName deliberately
		 * does not. applicationName is the QSettings key ("NifSkope 2.0"), so
		 * renaming it would strand every existing user's settings under a path
		 * nothing reads any more. WW_EDITION_VERSION comes from NifSkope.pro.
		 */
		/* The build rev must be TRUE, not merely present: the compiled-in
		 * NIFSKOPE_REVISION is baked when qmake runs, and after incremental
		 * builds it named a commit this binary was not - the user judged a
		 * fixed feature through a title that said he was on a newer build.
		 * QMAKE_PRE_LINK writes build_rev.txt beside the exe at every link;
		 * prefer it, fall back to the define. */
		QString buildRev;
		{
			QFile revFile( QCoreApplication::applicationDirPath() + QStringLiteral( "/build_rev.txt" ) );
			if ( revFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
				buildRev = QString::fromLatin1( revFile.readLine() ).trimmed();
		}
#ifdef NIFSKOPE_REVISION
		if ( buildRev.isEmpty() )
			buildRev = QStringLiteral( NIFSKOPE_REVISION );
#endif
		if ( !buildRev.isEmpty() )
			a->setApplicationDisplayName( QStringLiteral( "NifSkope - Wild Wasteland Edition " WW_EDITION_VERSION " (build " ) + buildRev + ", " + __DATE__ + ")" );
		else
			a->setApplicationDisplayName( QStringLiteral( "NifSkope - Wild Wasteland Edition " WW_EDITION_VERSION " (" ) + __DATE__ + ")" );

		// Must set current directory or this causes issues with several features
		QDir::setCurrent( qApp->applicationDirPath() );

		/* --icon-sheet FILE.png: render every tlMakeIcon glyph and exit.
		 *
		 * The icon set is DRAWN with QPainter, not loaded from files, so there is
		 * no folder to open and look at. Without this the only way to see whether
		 * a new glyph reads - or reads as the thing it is supposed to be - is to
		 * start the program and hunt for the button that carries it. Handled here,
		 * before any window or GL context exists, so it is cheap and works under
		 * -platform offscreen.
		 */
		{
			const QStringList iconArgs = a->arguments();
			const int iconIx = iconArgs.indexOf( QStringLiteral( "--icon-sheet" ) );
			if ( iconIx >= 0 && iconIx + 1 < iconArgs.size() ) {
				const QString out = iconArgs.at( iconIx + 1 );
				// optional third word: draw only the glyphs whose name contains it,
				// and draw them big enough to judge
				const QString filter = ( iconIx + 2 < iconArgs.size()
					&& !iconArgs.at( iconIx + 2 ).startsWith( QLatin1Char( '-' ) ) )
					? iconArgs.at( iconIx + 2 ) : QString();
				const bool ok = tlWriteIconSheet( out, QColor( 0xdd, 0xdd, 0xdd ),
												  QColor( 0x2b, 0x2b, 0x2b ), filter );
				fprintf( ok ? stdout : stderr, "%s %s\n",
					ok ? "wrote" : "could not write", qPrintable( out ) );
				return ok ? 0 : 1;
			}
		}

		// Register message handler
		//qRegisterMetaType<Message>( "Message" );
		qInstallMessageHandler( NifSkope::MessageOutput );

		// Register types
		qRegisterMetaType<NifValue>( "NifValue" );
#if 0
		QMetaType::registerComparators<NifValue>();
#endif

		// Set locale
		QSettings cfg( QString( "%1/nifskope.ini" ).arg( QCoreApplication::applicationDirPath() ), QSettings::IniFormat );
		cfg.beginGroup( "Settings" );
		NifSkope::SetAppLocale( cfg.value( "Locale", "en" ).toLocale() );
		cfg.endGroup();
		wwStartupMark( "settings + locale" );

		// Load XML files
		NifModel::loadXML();
		KfmModel::loadXML();
		wwStartupMark( "nif.xml + kfm.xml" );

		// Init game manager
		(void) Game::GameManager::get();
		wwStartupMark( "game manager" );

		/* WW_LODGEN_RESOURCES=<entry>;<entry>;... - the LOD generator's resource
		 * stack, in Mod Organizer's order (the LAST one overrides the earlier
		 * ones), for a headless card bake: src/features/far-lod/bake_impostor_cards.sh passes
		 * the same stack the generator will read, so a mod's trees photograph
		 * with their own textures instead of vanilla's. Session only, never
		 * written to QSettings (GameManager::save() is what persists, and only
		 * the Settings dialog calls it). WW_LODGEN_MO2=1 builds the stack from
		 * the profile's plugins.txt instead. */
		{
			QStringList stack;
			const QString env = wwEnvStr( "WW_LODGEN_RESOURCES" );
			if ( !env.isEmpty() )
				stack = env.split( QChar( ';' ), Qt::SkipEmptyParts );
			if ( wwEnvInt( "WW_LODGEN_MO2" ) == 1 ) {
				const QString data = Game::GameManager::path( Game::FALLOUT_4 ) + QStringLiteral( "/Data" );
				QString perr;
				stack = lodgenMo2Stack( data, lodgenReadPluginsTxt( lodgenPluginsTxtPath(), &perr ) ) + stack;
			}
			if ( !stack.isEmpty() ) {
				lodgenSetResources( stack );
				QStringList view = lodgenResourceSearchPaths();
				for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) )
					if ( !view.contains( f, Qt::CaseInsensitive ) )
						view.append( f );
				Game::GameManager::update_folders( Game::FALLOUT_4, view );
				Game::GameManager::close_resources();
			}
		}

		/* lane BAKEINCR1: WW_CELL_BAKEINCR_CHECK=... is the incremental probe bake's check: it runs here,
		 * after the game manager and the resource stack are set exactly as a bake sets them, and before
		 * any window, then exits (src/features/gi/probeincr.cpp). */
		if ( !wwEnvIsEmpty( "WW_CELL_BAKEINCR_CHECK" ) )
			return ProbeIncr::checkMain();

		int port = NIFSKOPE_IPC_PORT;

		QStack<QString> fnames;

		// Command Line setup
		QCommandLineParser parser;
		parser.addHelpOption();
		parser.addVersionOption();

		// Add port option
		QCommandLineOption portOption( {"p", "port"}, "Port NifSkope listens on", "port" );
		parser.addOption( portOption );
		// lane MOTION1: --path <file> renders a scripted camera path (gl/campath.h) through the render hook
		QCommandLineOption pathOption( "path", "Render the camera path in <file> to numbered frames or a video (WW_RENDER_SHOT names the output; default <file>.png)", "file" );
		parser.addOption( pathOption );

		// Process options
		parser.process( *a );

		// Override port value
		if ( parser.isSet( portOption ) )
			port = parser.value( portOption ).toInt();
		if ( parser.isSet( pathOption ) ) {
			const QString pf = QFileInfo( parser.value( pathOption ) ).absoluteFilePath();
			qputenv( "WW_RENDER_PATH", pf.toLocal8Bit() );
			wwEnvChanged();	// MODULES1 item 8: the env accessors' cache
			if ( !wwEnvIsSet( "WW_RENDER_SHOT" ) ) {
				const QFileInfo fi( pf );
				qputenv( "WW_RENDER_SHOT", ( fi.path() + QLatin1Char( '/' ) + fi.completeBaseName() + QStringLiteral( ".png" ) ).toLocal8Bit() );
				wwEnvChanged();	// MODULES1 item 8: the env accessors' cache
			}
		}

		// Files were passed to NifSkope
		QStringList missing;
		for ( const QString & arg : parser.positionalArguments() ) {
			QString fname = QDir::current().filePath( arg );

			if ( QFileInfo( fname ).exists() ) {
				fnames.push( fname );
			} else {
				missing << fname;
				fprintf( stderr, "open: %s does not exist, opened without it\n", qPrintable( fname ) );
			}
		}

		/* MERGEFIX1102 item 5 (SHAFTS1 11-03, "a refused rtddgi cell stalls the frame loop"): the stalled holds named
		 * lanes/<tree>/tests/fixtures/empty.wwcell in a sparse tree without tests/. The file was dropped above without a
		 * word, the window opened on the starter scene, and the WW_RENDER_SHOT handler that arms the live driver
		 * (nifskope_ui.cpp createWindow) is connected only for a document: no `live:` line for 90 minutes, every row
		 * NOT REACHED. A driven launch needs its document: it refuses at once and says why. */
		if ( fnames.isEmpty() && ( wwEnvIsSet( "WW_RENDER_SHOT" ) || wwEnvIsSet( "WW_LIVE_STEPS" ) || wwEnvIsSet( "WW_LIVE_SERVE" ) ) ) {
			fprintf( stderr, "LAUNCH REFUSED: the shot/live driver (WW_RENDER_SHOT, WW_LIVE_STEPS, WW_LIVE_SERVE) is armed by the document on the command line, and %s\n",
				missing.isEmpty() ? "none was given" : qPrintable( QStringLiteral( "it does not exist: " ) + missing.join( QLatin1String( ", " ) ) ) );
			fflush( stderr );
			return 89;
		}

		// No files were passed to NifSkope, push empty string
		if ( fnames.isEmpty() ) {
			fnames.push( QString() );
		}

		// CARDBAKE2: one process bakes every base of a card list (nifskope_ui.cpp, wwCardBatchStart)
		if ( wwCardBatchOn() ) {
			IPCsocket * ipc = IPCsocket::create( port );		// held like a per-base launch's, so a second bake on the port refuses
			if ( !ipc || !wwCardBatchStart() )
				return 2;
			return a->exec();
		}
		if ( IPCsocket * ipc = IPCsocket::create( port ) ) {
			//qDebug() << "IPCSocket exec";
			wwStartupMark( "ipc socket" );
			wwStderrPumpStart();	// lane PERFLOAD1 (P2): log writes leave the main thread
			wwMainWatchStart( a );	// and a main-thread step over 1 s prints its own line
			ipc->execCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ) );
			wwStartupMark( "window open command" );

			while ( !fnames.isEmpty() ) {
				IPCsocket::sendCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ), port );
			}

			return a->exec();
		} else {
			//qDebug() << "IPCSocket send";
			while ( !fnames.isEmpty() ) {
				IPCsocket::sendCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ), port );
			}
			return 0;
		}
	} else {
		// Headless batch mode: -no-gui selected a plain QCoreApplication above,
		// so there are no widgets and no GL context. See src/nifcli.cpp.
		return nifskopeCliMain( app->arguments() );
	}

	return 0;
}



/*
*  IPC socket
*/

IPCsocket * IPCsocket::create( int port )
{
	QUdpSocket * udp = new QUdpSocket();

	if ( udp->bind( QHostAddress( QHostAddress::LocalHost ), port, QUdpSocket::DontShareAddress ) ) {
		IPCsocket * ipc = new IPCsocket( udp );
		QDesktopServices::setUrlHandler( "nif", ipc, "openNif" );
		return ipc;
	}

	return nullptr;
}

void IPCsocket::sendCommand( const QString & cmd, int port )
{
	QUdpSocket udp;
	udp.writeDatagram( (const char *)cmd.data(), cmd.length() * sizeof( QChar ), QHostAddress( QHostAddress::LocalHost ), port );
}

IPCsocket::IPCsocket( QUdpSocket * s ) : QObject(), socket( s )
{
	QObject::connect( socket, &QUdpSocket::readyRead, this, &IPCsocket::processDatagram );
}

IPCsocket::~IPCsocket()
{
	delete socket;
}

/* lane TESTDRIVER1: "NifSkope::live <step>" runs one live-driver step (wwswitch.h) in the visible window and sends
 * "<step> -> <reply>" back to the sender (UTF-16, as the commands). A step runs the event loop (a reopen, a path), so
 * steps that arrive meanwhile queue and run in order when it returns. */
namespace
{
struct WwLiveCmd { QString step; QHostAddress host; quint16 port; };
QList<WwLiveCmd> g_wwLiveQueue;
bool g_wwLiveBusy = false;

void wwLiveDrain( QUdpSocket * socket )
{
	if ( g_wwLiveBusy )
		return;
	g_wwLiveBusy = true;
	while ( !g_wwLiveQueue.isEmpty() ) {
		const WwLiveCmd c = g_wwLiveQueue.takeFirst();
		NifSkope * win = nullptr;
		for ( NifSkope * doc : NifSkope::openDocuments() )
			if ( doc && doc->isVisible() && doc->getGLView() ) {
				win = doc;
				break;
			}
		wwMainStep( "a live port step" );	// lane PERFLOAD1 P5
		const QString reply = c.step + QStringLiteral( " -> " ) + ( win ? wwLiveCommand( win, c.step ) : QStringLiteral( "refused no window" ) );
		fprintf( stderr, "live(port): %s\n", qPrintable( reply ) );
		socket->writeDatagram( (const char *)reply.constData(), reply.size() * qsizetype( sizeof( QChar ) ), c.host, c.port );
	}
	g_wwLiveBusy = false;
}
} // namespace

void IPCsocket::processDatagram()
{
	while ( socket->hasPendingDatagrams() ) {
		QByteArray data;
		data.resize( socket->pendingDatagramSize() );
		QHostAddress host;
		quint16 port = 0;

		socket->readDatagram( data.data(), data.size(), &host, &port );

		if ( host == QHostAddress( QHostAddress::LocalHost ) && (data.size() % sizeof( QChar )) == 0 ) {
			QString cmd;
			cmd.setUnicode( (QChar *)data.data(), data.size() / sizeof( QChar ) );
			if ( cmd.startsWith( QLatin1StringView( "NifSkope::live " ) ) ) {	// lane TESTDRIVER1
				g_wwLiveQueue.append( { cmd.mid( 15 ), host, port } );
				QTimer::singleShot( 0, this, [this]() { wwLiveDrain( socket ); } );
				continue;
			}
			execCommand( cmd );
		}
	}
}

void IPCsocket::execCommand( const QString & cmd )
{
	if ( cmd.startsWith( "NifSkope::open" ) ) {
		openNif( cmd.right( cmd.length() - 15 ) );
	} else if ( cmd.startsWith( "NifSkope::door " ) ) {
		// lane DOORLIVE1: NifSkope::door <open|close|cycle|t=<s>|state> drives the visible window's live door
		for ( NifSkope * doc : NifSkope::openDocuments() )
			if ( doc && doc->isVisible() && doc->getGLView() ) {
				fprintf( stderr, "%s\n", qPrintable( wwDoorLiveCommand( doc->getGLView()->getScene(), cmd.mid( 15 ) ) ) );
				break;
			}
	}
}

void IPCsocket::openNif( const QUrl & url )
{
	auto file = url.toString();
	file.remove( 0, 4 );

	openNif( file );
}

void IPCsocket::openNif( const QString & url )
{
	/* A CLIP SENT TO A RUNNING NIFSKOPE PLAYS ON THE MODEL ALREADY OPEN
	 * (bungo 2026-10-05: "give me an animation in nifskope to preview").
	 * `NifSkope.exe --port N clip.hkx`, or a UDP open to that port, used to make
	 * a new empty window, where a clip has nothing to play on. It now goes to
	 * the visible window's Animation list, the same loader as a drop. */
	if ( !WwHkxAnimHub::animationFilesIn( { url } ).isEmpty() ) {
		for ( NifSkope * doc : NifSkope::openDocuments() ) {
			if ( doc && doc->isVisible() && doc->getGLView() ) {
				doc->showTransientMessage(
					WwHkxAnimHub::instance()->loadFiles( doc->getGLView(), { url }, true ), 12000 );
				doc->playAnimationLooped();
				return;
			}
		}
	}
	NifSkope::createWindow( url );
}
