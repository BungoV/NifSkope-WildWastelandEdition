#include "celltexahead.h"

#include "cellspeed.h"
#include "gamemanager.h"
#include "model/nifmodel.h"

#include <QCoreApplication>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// lane SPEED1 (2026-10-02): see celltexahead.h.

using Game::GameManager;

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
	const NifModel * nif = nullptr;
	bool quitHooked = false;
};

std::atomic<const NifModel *> armed{ nullptr };

// never destroyed: a worker may still be inside it when the process ends
State & st()
{
	static State * s = new State;
	return *s;
}

bool enabled()
{
	static const bool on = [] {
		const QByteArray red = qgetenv( "WW_CELL_SPEED_RED" );
		return red != "slow" && red != "notex";
	}();
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
		const NifModel * nif = s.nif;
		l.unlock();

		// the two calls TexCache::find and TexCache::texLoad make; a file that is not there is left to
		// the drawing thread, which says so in its own words
		QByteArray data;
		bool ok = false;
		{
			CellSpeed::Acc speedAcc( "texture files read ahead (worker threads)" );
			const QString full = nif->findResourceFile( name, "textures", ".dds" );
			ok = !full.isEmpty() && nif->getResourceFile( data, full, "textures", "" ) && !data.isEmpty();
		}

		l.lock();
		Entry & e = s.entries[i];
		if ( ok && !s.stopping ) {
			s.held += data.size();
			e.data = data;
			e.st = St::Done;
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
		const QByteArray dump = qgetenv( "WW_CELL_SPEED_DUMP" );
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
		const std::string key = GameManager::get_full_path( n, "textures", ".dds" );
		if ( s.byKey.find( key ) != s.byKey.end() )
			continue;
		s.byKey.emplace( key, int( s.entries.size() ) );
		Entry e;
		e.name = n;
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

bool CellTexAhead::take( const NifModel * nif, const QString & filepath, QByteArray & data )
{
	if ( !nif || armed.load( std::memory_order_relaxed ) != nif )
		return false;
	const std::string key = GameManager::get_full_path( filepath, "textures", "" );
	State & s = st();
	std::unique_lock<std::mutex> l( s.lock );
	if ( s.nif != nif )
		return false;
	s.takesThisFrame++;
	auto it = s.byKey.find( key );
	if ( it == s.byKey.end() ) {
		s.unknown++;
		if ( CellSpeed::on() && s.unknownNames.size() < 60 )
			s.unknownNames << filepath;
		return false;
	}
	const int i = it->second;
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
	s.cv.notify_all();
	return true;
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
	if ( !armed.load( std::memory_order_relaxed ) )
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
