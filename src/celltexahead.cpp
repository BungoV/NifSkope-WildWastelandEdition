#include "celltexahead.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "wwhwuse.h"	// lane EMPTY240 R3-1: the stream pool's busy count

#include "cellspeed.h"
#include "cellstream.h"
#include "features/shared/cellpasstime.h"
#include "gamemanager.h"
#include "model/nifmodel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// see celltexahead.h.

using Game::GameManager;

void wwCovPreserveBytes( QByteArray & data, int ref );	// gltexloaders.cpp (lane EMPTY240 R9-d)

namespace
{

const qint64 AHEAD_BYTES = 384LL * 1024 * 1024;   //!< file bytes read and not yet taken
const int AHEAD_IDLE_FRAMES = 8;                  //!< frames that asked for nothing before any did

enum class St { Pending, Reading, Done, Gone };

struct Entry
{
	QString name;
	St st = St::Pending;
	QByteArray data;
	int covRef = 0;	// lane EMPTY240 R9-d: a "|wwcov<ref>" entry: the worker rewrites its alpha mips for ref
};

struct State
{
	std::mutex lock;
	std::condition_variable cv;
	std::vector<Entry> entries;                    // in the order the materials named them
	std::unordered_map<std::string, int> byKey;
	size_t next = 0;                               // where the workers look next (they go round)
	int pending = 0;                               // entries no thread has started
	qint64 held = 0;
	int workers = 0;
	bool stopping = false;
	int takesThisFrame = 0, idleFrames = 0;
	bool sawTakes = false;
	qint64 hits = 0, waited = 0, own = 0, unknown = 0, gone = 0, left = 0;
	QStringList unknownNames;                      // the timer dump's: the first few nobody named
	std::unordered_set<std::string> forgotten;     // lane GAPFIX1: freed by the view, read again when named again
	qint64 again = 0, drawRead = 0;                // lane GAPFIX1: whole session (the walk dump reads them)
	const NifModel * nif = nullptr;
	bool quitHooked = false;
	qint64 polled = 0, pollWaits = 0, pollOwn = 0;	// lane EMPTY240 R8-2: the `texture reads:` line's window
	qint64 covMoved = 0, covTaken = 0, covRaw = 0;	// lane EMPTY240 R9-d: same window
};

std::atomic<const NifModel *> armed{ nullptr };

/* lane EMPTY240 R9-d: "<file>|wwcov<ref>" (gltex.cpp bind's coverage-preserving copy) is keyed apart, with its ref;
 * *ref = the ref (0: a plain name), *base = the file */
std::string keyOf( const QString & name, const char * ext, int * ref = nullptr, QString * base = nullptr )
{
	const qsizetype at = name.lastIndexOf( QLatin1StringView( "|wwcov" ) );
	const int r = at > 0 ? name.mid( at + 6 ).toInt() : 0;
	const QString b = at > 0 ? name.left( at ) : name;
	if ( ref )
		*ref = r;
	if ( base )
		*base = b;
	std::string k = GameManager::get_full_path( b, "textures", ext );
	if ( r > 0 )
		k += "|wwcov" + std::to_string( r );
	return k;
}

QString nameOf( const Entry & e )
{
	return e.covRef > 0 ? e.name + QStringLiteral( "|wwcov" ) + QString::number( e.covRef ) : e.name;
}

// never destroyed: a worker may still be inside it when the process ends
State & st()
{
	static State * s = new State;
	return *s;
}

bool enabled()
{
	WW_SWITCH_STATIC( bool, on, [] {
		const QByteArray red = wwEnv( "WW_CELL_SPEED_RED" );
		return red != "slow" && red != "notex";
	}() );
	return on;
}

void worker()
{
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	for ( ;; ) {
		if ( s.stopping )
			break;
		/* The next entry no thread has started, from where the drawing thread last had to read for itself
		 * (take() moves `next` there), once round for what that jump passed over. Nothing read is thrown
		 * away to make room: at the bound the workers wait for the drawing thread to take. */
		size_t i = s.entries.size();
		if ( s.pending > 0 && s.held < AHEAD_BYTES ) {
			for ( size_t n = 0; n < s.entries.size(); n++ ) {
				const size_t k = ( s.next + n ) % s.entries.size();
				if ( s.entries[k].st == St::Pending ) {
					i = k;
					break;
				}
			}
		}
		if ( i == s.entries.size() ) {
			s.cv.wait( l );
			continue;
		}
		s.next = i + 1;
		s.pending--;
		s.entries[i].st = St::Reading;
		const QString name = s.entries[i].name;
		const int covRef = s.entries[i].covRef;
		// R9-d: read before it became a coverage entry: only the rewrite is left
		QByteArray data = s.entries[i].data;
		if ( !data.isEmpty() ) {
			s.held -= data.size();
			s.entries[i].data = QByteArray();
		}
		const NifModel * nif = s.nif;
		l.unlock();

		// the two calls TexCache::find and TexCache::texLoad make; a file that is not there is left to
		// the drawing thread, which says so in its own words
		bool ok = !data.isEmpty();
		{
			WwHw::Busy wwBusy( WwHw::BusyTextures );	// lane EMPTY240 R3-1, R5-4: by kind
			CellSpeed::Acc speedAcc( "texture files read ahead (worker threads)" );
			if ( !ok ) {
				const QString full = nif->findResourceFile( name, "textures", ".dds" );
				ok = !full.isEmpty() && nif->getResourceFile( data, full, "textures", "" ) && !data.isEmpty();
			}
			if ( ok && covRef > 0 )
				wwCovPreserveBytes( data, covRef );	// R9-d: the decode and re-encode off the drawing thread
		}

		l.lock();
		Entry & e = s.entries[i];
		if ( ok && !s.stopping ) {
			s.held += data.size();
			e.data = data;
			if ( e.covRef != covRef ) {	// R9-d: became a coverage entry while being read: the rewrite next round
				e.st = St::Pending;
				s.pending++;
			} else {
				e.st = St::Done;
			}
		} else {
			e.st = St::Gone;
		}
		s.cv.notify_all();
	}
	s.workers--;
	s.cv.notify_all();
}

// The lock is held. Workers home, table empty, counts into the timer dump.
void stopLocked( State & s, std::unique_lock<std::mutex> & l )
{
	armed.store( nullptr );
	s.stopping = true;
	s.cv.notify_all();
	while ( s.workers > 0 )
		s.cv.wait( l );
	for ( const Entry & e : s.entries )
		s.left += ( e.st == St::Done ) ? 1 : 0;
	if ( CellSpeed::on() && !s.entries.empty() ) {
		CellSpeed::count( "textures named by materials", qint64( s.entries.size() ) );
		CellSpeed::count( "texture files taken ready", s.hits );
		CellSpeed::count( "  of them waited for", s.waited );
		CellSpeed::count( "texture files the drawing thread read itself (not started)", s.own );
		CellSpeed::count( "texture files nobody named", s.unknown );
		CellSpeed::count( "texture files named, not ready when asked (not there, or stopped)", s.gone );
		CellSpeed::count( "texture files read and never taken", s.left );
		// which ones: beside the timer dump, for the next reader of these counts
		const QByteArray dump = wwEnv( "WW_CELL_SPEED_DUMP" );
		if ( FILE * f = dump.isEmpty() ? nullptr : std::fopen( ( dump + ".texnames" ).constData(), "ab" ) ) {
			int n = 0;
			for ( const QString & u : s.unknownNames )
				std::fprintf( f, "nobody named\t%s\n", u.toUtf8().constData() );
			for ( const Entry & e : s.entries )
				if ( e.st == St::Done && n++ < 60 )
					std::fprintf( f, "never taken\t%s\n", e.name.toUtf8().constData() );
			std::fclose( f );
		}
	}
	s.unknownNames.clear();
	s.forgotten.clear();
	s.entries.clear();
	s.byKey.clear();
	s.next = 0;
	s.pending = 0;
	s.held = 0;
	s.stopping = false;
	s.takesThisFrame = s.idleFrames = 0;
	s.sawTakes = false;
	s.hits = s.waited = s.own = s.unknown = s.gone = s.left = 0;
	s.nif = nullptr;
}

} // namespace

void CellTexAhead::arm( const NifModel * nif )
{
	if ( !enabled() || !nif || !qApp || QThread::currentThread() != qApp->thread() )
		return;
	if ( armed.load() == nif )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( armed.load() )
		stopLocked( s, l );
	s.nif = nif;
	armed.store( nif );
	if ( !s.quitHooked ) {
		s.quitHooked = true;
		QObject::connect( qApp, &QCoreApplication::aboutToQuit, [] { CellTexAhead::stop( armed.load() ); } );
	}
}

bool CellTexAhead::armedFor( const NifModel * nif )
{
	return nif && armed.load( std::memory_order_relaxed ) == nif;
}

void CellTexAhead::want( const NifModel * nif, const QStringList & names )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif || s.stopping )
		return;
	bool added = false;
	for ( const QString & n : names ) {
		if ( n.isEmpty() || n.startsWith( QChar( '#' ) ) )
			continue;
		int ref = 0;
		QString base;
		const std::string key = keyOf( n, ".dds", &ref, &base );
		if ( s.byKey.find( key ) != s.byKey.end() )
			continue;
		s.again += s.forgotten.erase( key ) ? 1 : 0;
		s.byKey.emplace( key, int( s.entries.size() ) );
		Entry e;
		e.name = base;
		e.covRef = ref;
		s.entries.push_back( e );
		s.pending++;
		added = true;
	}
	if ( !added )
		return;
	if ( s.workers == 0 ) {
		const int n = qBound( 1, QThread::idealThreadCount() - 2, 8 );
		for ( int i = 0; i < n; i++ ) {
			s.workers++;
			std::thread( worker ).detach();
		}
	}
	s.cv.notify_all();
}

/* lane EMPTY240 R8-2: build 82's frames 137-152 held 250-580 ms of CPU each in the obscurance pass's geometry, one or a
 * few textures a paint: the first ground draw rebuilds the landspec band window, whose chunk bind read its file on the
 * drawing thread ("landspec: band window rebuild took 420.3 ms on the main thread: reads 420.1"), and a texture the
 * read-ahead had queued but not read yet was read there too (take() of a Pending entry) or waited for (Reading). A bind
 * in a paint now asks poll() first: a file not read yet waits a paint while the workers read it (first in line), as a
 * texture over the upload budget does; only the upload stays on the drawing thread. Red WW_TEXREADWAIT_RED=1: the
 * drawing thread reads or waits, build 82. Line `texture reads:` every 5 s. */
CellTexAhead::Poll CellTexAhead::poll( const NifModel * nif, const QString & name )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif || name.isEmpty() || name.startsWith( QChar( '#' ) ) )
		return Own;
	int ref = 0;
	QString base;
	const std::string key = keyOf( name, ".dds", &ref, &base );
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif || s.stopping )
		return Own;
	auto it = s.byKey.find( key );
	if ( it == s.byKey.end() && ref > 0 ) {
		/* lane EMPTY240 R9-d: a coverage copy of a file the materials named plain: that entry becomes the coverage
		 * entry (no second read); read already, it goes back to the workers for the rewrite */
		auto pl = s.byKey.find( keyOf( base, ".dds" ) );
		if ( pl != s.byKey.end() && s.entries[size_t( pl->second )].covRef == 0
			&& s.entries[size_t( pl->second )].st != St::Gone ) {
			const int idx = pl->second;
			s.byKey.erase( pl );
			it = s.byKey.emplace( key, idx ).first;
			Entry & pe = s.entries[size_t( idx )];
			pe.covRef = ref;
			if ( pe.st == St::Done ) {
				pe.st = St::Pending;
				s.pending++;
			}
			s.covMoved++;
		}
	}
	if ( it == s.byKey.end() ) {
		if ( s.held >= AHEAD_BYTES ) {
			s.pollOwn++;
			return Own;
		}
		s.again += s.forgotten.erase( key ) ? 1 : 0;
		it = s.byKey.emplace( key, int( s.entries.size() ) ).first;
		Entry e;
		e.name = base;
		e.covRef = ref;
		s.entries.push_back( e );
		s.pending++;
		s.polled++;
		if ( s.workers == 0 ) {
			const int n = qBound( 1, QThread::idealThreadCount() - 2, 8 );
			for ( int i = 0; i < n; i++ ) {
				s.workers++;
				std::thread( worker ).detach();
			}
		}
	}
	Entry & e = s.entries[size_t( it->second )];
	if ( e.st == St::Done )
		return Ready;
	if ( e.st == St::Pending && s.held >= AHEAD_BYTES ) {
		s.pollOwn++;
		return Own;	// the workers wait for takes: never hold a draw on them
	}
	if ( e.st == St::Pending || e.st == St::Reading ) {
		if ( e.st == St::Pending )
			s.next = size_t( it->second );	// a draw asks for it: the workers read it next
		s.pollWaits++;
		s.cv.notify_all();
		return Wait;
	}
	return Own;	// read and handed out before, or a read that failed: the drawing thread's own words
}

bool CellTexAhead::take( const NifModel * nif, const QString & filepath, QByteArray & data, int covRef, bool * covDone )
{
	if ( covDone )
		*covDone = false;
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return false;
	const std::string plainKey = GameManager::get_full_path( filepath, "textures", "" );
	const std::string key = covRef > 0 ? plainKey + "|wwcov" + std::to_string( covRef ) : plainKey;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif )
		return false;
	s.takesThisFrame++;
	auto it = s.byKey.find( key );
	if ( it == s.byKey.end() && covRef > 0 )
		it = s.byKey.find( plainKey );	// R9-d: read plain only: the drawing thread rewrites, as before
	if ( it == s.byKey.end() ) {
		s.drawRead += s.forgotten.count( key ) ? 1 : 0;
		s.unknown++;
		if ( CellSpeed::on() && s.unknownNames.size() < 60 )
			s.unknownNames << filepath;
		return false;
	}
	const int i = it->second;
	s.drawRead += ( s.entries[size_t( i )].st == St::Gone ) ? 1 : 0;
	if ( s.entries[size_t( i )].st == St::Pending && !s.entries[size_t( i )].data.isEmpty() ) {
		Entry & r = s.entries[size_t( i )];	// R9-d: read, its rewrite not started: the raw bytes, rewritten here
		data = r.data;
		r.data = QByteArray();
		r.st = St::Gone;
		s.pending--;
		s.held -= data.size();
		s.hits++;
		s.covRaw++;
		s.cv.notify_all();
		return true;
	}
	if ( s.entries[size_t( i )].st == St::Pending ) {
		s.entries[size_t( i )].st = St::Gone;   // no worker has started it: the old way, now
		s.pending--;
		s.own++;
		s.next = size_t( i ) + 1;               // and the workers go on from where the drawing thread is
		return false;
	}
	if ( s.entries[size_t( i )].st == St::Reading ) {
		s.waited++;
		while ( s.entries[size_t( i )].st == St::Reading )
			s.cv.wait( l );
	}
	Entry & e = s.entries[size_t( i )];
	if ( e.st != St::Done ) {
		s.gone++;
		return false;
	}
	data = e.data;
	e.data = QByteArray();
	e.st = St::Gone;
	s.held -= data.size();
	s.hits++;
	if ( covRef > 0 && e.covRef == covRef ) {
		s.covTaken++;
		if ( covDone )
			*covDone = true;
	}
	s.cv.notify_all();
	return true;
}

QStringList CellTexAhead::ready( const NifModel * nif, int max )
{
	QStringList out;
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return out;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif )
		return out;
	for ( const Entry & e : s.entries ) {
		if ( out.size() >= max )
			break;
		if ( e.st == St::Done )
			out << nameOf( e );	// R9-d: a coverage entry by its "|wwcov" name
	}
	return out;
}

void CellTexAhead::drop( const NifModel * nif, const QString & name )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif )
		return;
	auto it = s.byKey.find( keyOf( name, ".dds" ) );
	if ( it == s.byKey.end() )
		return;
	Entry & e = s.entries[size_t( it->second )];
	if ( e.st != St::Done )
		return;
	s.held -= e.data.size();
	e.data = QByteArray();
	e.st = St::Gone;
	s.cv.notify_all();
}

void CellTexAhead::forget( const NifModel * nif, const QString & name )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif )
		return;
	const std::string key = keyOf( name, ".dds" );
	auto it = s.byKey.find( key );
	if ( it == s.byKey.end() || s.entries[size_t( it->second )].st != St::Gone )
		return;   // still to read, or read and waiting: the bytes come anyway
	s.byKey.erase( it );   // the entry stays (indices hold); a later want() appends a fresh one
	s.forgotten.insert( key );
}

void CellTexAhead::counts( qint64 & again, qint64 & drawRead )
{
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	again = s.again;
	drawRead = s.drawRead;
}

void CellTexAhead::stop( const NifModel * nif )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif == nif )
		stopLocked( s, l );
}

void CellTexAhead::frameEnd()
{
	{	// lane EMPTY240 R8-2: the `texture reads:` line, every 5 s while binds waited for the workers
		static QElapsedTimer win;
		if ( !win.isValid() )
			win.start();
		if ( win.elapsed() >= 5000 ) {
			win.start();
			State & s = st();
			std::unique_lock<std::mutex> l( s.lock );
			if ( s.covMoved + s.covTaken + s.covRaw > 0 )
				std::fprintf( stderr, "texture coverage ahead: %lld coverage copies handed to the workers, %lld taken rewritten "
					"(no decode on the drawing thread), %lld taken raw (R9-d; red WW_COVAHEAD_RED=1: the drawing thread "
					"rewrites, build 83)\n", (long long) s.covMoved, (long long) s.covTaken, (long long) s.covRaw );
			s.covMoved = s.covTaken = s.covRaw = 0;
			if ( s.pollWaits + s.pollOwn > 0 )
				std::fprintf( stderr, "texture reads: %lld binds waited a paint for their file (read by the workers, not the "
					"drawing thread), %lld files first asked by a draw, %lld read on the drawing thread at the byte bound; "
					"drawing-thread reads %lld, waits for a worker %lld (R8-2; red WW_TEXREADWAIT_RED=1: the drawing thread "
					"reads or waits, build 82)\n", (long long) s.pollWaits, (long long) s.polled, (long long) s.pollOwn,
					(long long) s.own, (long long) s.waited );
			s.pollWaits = s.polled = s.pollOwn = 0;
		}
	}
	const NifModel * a = armed.load( std::memory_order_relaxed );
	if ( !a )
		return;
	// lane STREAM1: a streaming document keeps reading ahead, for the cells still to come (they arrive while the
	// camera moves; the view's pump uploads what is read). Red WW_STREAM1_RED=texpump: it ends as before.
	static const bool pumpRed = PassTime::stream1Red( "texpump" );
	if ( !pumpRed && cellStreamOn( a ) )
		return;
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( !s.nif || s.entries.empty() )
		return;   // frames painted while the cell is still being built
	const int takes = s.takesThisFrame;
	s.takesThisFrame = 0;
	if ( takes > 0 ) {
		s.sawTakes = true;
		return;
	}
	// a frame that loaded no texture: the cell has what it draws with
	if ( s.sawTakes || ++s.idleFrames >= AHEAD_IDLE_FRAMES )
		stopLocked( s, l );
}
