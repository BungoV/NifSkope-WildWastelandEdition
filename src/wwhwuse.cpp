#include "wwhwuse.h"

#include "ba2file.hpp"
#include "filebuf.hpp"
#include "wwswitch.h"

#include <QCoreApplication>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace WwHw
{

int poolSlots()
{
	static const int n = std::max( 1, QThread::idealThreadCount() - 2 );
	return n;
}

std::atomic<int> & busyCount()
{
	static std::atomic<int> n { 0 };
	return n;
}

namespace
{

using Clock = std::chrono::steady_clock;

std::int64_t nsSince( Clock::time_point t0 )
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>( Clock::now() - t0 ).count();
}

struct IoAtomics
{
	std::atomic<std::int64_t> files { 0 }, cacheHits { 0 }, packed { 0 }, bytes { 0 }, readNs { 0 }, inflateNs { 0 };
	std::atomic<std::int64_t> drawFiles { 0 }, drawNs { 0 };
	std::atomic<std::int64_t> indexBuilds { 0 }, indexNs { 0 }, indexArchives { 0 };
};

IoAtomics & ioA()
{
	static IoAtomics a;
	return a;
}

bool onDrawThread()
{
	const QCoreApplication * app = QCoreApplication::instance();
	return app && QThread::currentThread() == app->thread();
}

constexpr std::int64_t kMarginBytes = std::int64_t( 3 ) << 30;	// the memguard free-RAM floor (3 GB)

std::int64_t availPhys()
{
#ifdef Q_OS_WIN
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof( ms );
	if ( GlobalMemoryStatusEx( &ms ) )
		return std::int64_t( ms.ullAvailPhys );
#endif
	return 0;
}

/* R3-3: extracted archive files by (index, path), least recently read first out. The budget is what the cache holds
 * plus the free RAM minus the margin, re-read every 250 ms: the cache grows while RAM is free and gives back as soon
 * as free RAM falls under the floor. A QByteArray is shared, so a hit hands out the cached bytes without a copy. */
struct RamCache
{
	struct Entry
	{
		const void * ix;
		std::string path;
		QByteArray bytes;
	};
	std::mutex lock;
	std::list<Entry> lru;	// front = newest
	std::unordered_map<std::string, std::list<Entry>::iterator> map;
	std::int64_t bytes = 0, budget = 0, hits = 0, misses = 0, evicted = 0;
	Clock::time_point budgetAt {};
	std::atomic<bool> on { true };	// red WW_RAMCACHE_RED=1 (live: re-read with the budget)

	RamCache()
	{
		on = wwEnvInt( "WW_RAMCACHE_RED" ) != 1;
		BA2File::destroyHook = &WwHw::forget;
	}

	static std::string key( const void * ix, std::string_view path )
	{
		std::string k( reinterpret_cast<const char *>( &ix ), sizeof( ix ) );
		k.append( path.data(), path.size() );
		return k;
	}

	// under the lock
	void budgetNow()
	{
		const auto now = Clock::now();
		if ( budgetAt != Clock::time_point {} && now - budgetAt < std::chrono::milliseconds( 250 ) )
			return;
		budgetAt = now;
		on = wwEnvInt( "WW_RAMCACHE_RED" ) != 1;
		budget = on ? std::max<std::int64_t>( 0, bytes + availPhys() - kMarginBytes ) : 0;	// off: trim() empties it
	}

	// under the lock
	void trim()
	{
		while ( bytes > budget && !lru.empty() ) {
			Entry & e = lru.back();
			bytes -= e.bytes.size();
			map.erase( key( e.ix, e.path ) );
			lru.pop_back();
			evicted++;
		}
	}
};

RamCache & ram()
{
	static RamCache c;
	return c;
}

std::atomic<std::int64_t> g_route[RouteCount];

} // namespace

Io io()
{
	const IoAtomics & a = ioA();
	Io o;
	o.files = a.files.load();
	o.cacheHits = a.cacheHits.load();
	o.packed = a.packed.load();
	o.bytes = a.bytes.load();
	o.readNs = a.readNs.load();
	o.inflateNs = a.inflateNs.load();
	o.drawFiles = a.drawFiles.load();
	o.drawNs = a.drawNs.load();
	o.indexBuilds = a.indexBuilds.load();
	o.indexNs = a.indexNs.load();
	o.indexArchives = a.indexArchives.load();
	return o;
}

Io ioDelta( const Io & now, const Io & then )
{
	Io d;
	d.files = now.files - then.files;
	d.cacheHits = now.cacheHits - then.cacheHits;
	d.packed = now.packed - then.packed;
	d.bytes = now.bytes - then.bytes;
	d.readNs = now.readNs - then.readNs;
	d.inflateNs = now.inflateNs - then.inflateNs;
	d.drawFiles = now.drawFiles - then.drawFiles;
	d.drawNs = now.drawNs - then.drawNs;
	d.indexBuilds = now.indexBuilds - then.indexBuilds;
	d.indexNs = now.indexNs - then.indexNs;
	d.indexArchives = now.indexArchives - then.indexArchives;
	return d;
}

bool read( const void * ix, std::string_view path, const unsigned char * mapped, std::size_t packed, QByteArray & out,
	const std::function<void( QByteArray & )> & extract )
{
	IoAtomics & a = ioA();
	RamCache & c = ram();
	const bool draw = onDrawThread();
	const auto t0 = Clock::now();
	const bool cacheable = c.on && ix;
	if ( cacheable ) {
		std::lock_guard<std::mutex> g( c.lock );
		auto it = c.map.find( RamCache::key( ix, path ) );
		if ( it != c.map.end() ) {
			c.lru.splice( c.lru.begin(), c.lru, it->second );
			out = it->second->bytes;
			c.hits++;
			a.cacheHits++;
			if ( draw ) {
				a.drawFiles++;
				a.drawNs += nsSince( t0 );
			}
			return true;
		}
		c.misses++;
		c.budgetNow();	// the red, read live
	}
	// read: page the packed bytes in from the mapped archive (one byte a page), so the inflate below times only itself
	if ( mapped && packed ) {
		unsigned sum = 0;
		for ( std::size_t i = 0; i < packed; i += 4096 )
			sum += reinterpret_cast<const volatile unsigned char *>( mapped )[i];
		sum += reinterpret_cast<const volatile unsigned char *>( mapped )[packed - 1];
		(void) sum;
	}
	const auto t1 = Clock::now();
	const std::int64_t readNs = std::chrono::duration_cast<std::chrono::nanoseconds>( t1 - t0 ).count();
	struct Tally
	{
		IoAtomics & a;
		bool draw;
		Clock::time_point t0, t1;
		std::int64_t readNs, packed;
		const QByteArray & out;
		~Tally()
		{
			a.files++;
			a.readNs += readNs;
			a.inflateNs += nsSince( t1 );
			a.packed += packed;
			a.bytes += out.size();
			if ( draw ) {
				a.drawFiles++;
				a.drawNs += nsSince( t0 );
			}
		}
	} tally { a, draw, t0, t1, readNs, std::int64_t( packed ), out };
	extract( out );
	if ( cacheable && !out.isEmpty() ) {
		std::lock_guard<std::mutex> g( c.lock );
		c.budgetNow();
		const std::string k = RamCache::key( ix, path );
		if ( out.size() <= c.budget / 4 && c.map.find( k ) == c.map.end() ) {
			c.lru.push_front( RamCache::Entry { ix, std::string( path ), out } );
			c.map.emplace( k, c.lru.begin() );
			c.bytes += out.size();
		}
		c.trim();
	}
	return false;
}

void forget( const void * ix )
{
	RamCache & c = ram();
	std::lock_guard<std::mutex> g( c.lock );
	for ( auto it = c.lru.begin(); it != c.lru.end(); ) {
		if ( it->ix == ix ) {
			c.bytes -= it->bytes.size();
			c.map.erase( RamCache::key( it->ix, it->path ) );
			it = c.lru.erase( it );
		} else {
			++it;
		}
	}
}

void archiveOpens( std::uint64_t * opens, std::uint64_t * shared, std::size_t * live )
{
	FileBuffer::archiveShareStats( opens, shared, live );
}

void indexBuilt( const char * who, int archives, double ms )
{
	ram();	// the destroy hook is in before any index can go
	IoAtomics & a = ioA();
	a.indexBuilds++;
	a.indexNs += std::int64_t( ms * 1.0e6 );
	a.indexArchives += archives;
	std::fprintf( stderr, "archive index: %s built on the %s thread, %d archive files opened, %.0f ms (%lld index builds "
		"since launch)\n", who, onDrawThread() ? "drawing" : "worker", archives, ms, (long long) a.indexBuilds.load() );
}

Cache cache()
{
	RamCache & c = ram();
	std::lock_guard<std::mutex> g( c.lock );
	c.budgetNow();
	c.trim();
	Cache o;
	o.on = c.on;
	o.bytes = c.bytes;
	o.budget = c.budget;
	o.files = std::int64_t( c.lru.size() );
	o.hits = c.hits;
	o.misses = c.misses;
	o.evicted = c.evicted;
	return o;
}

void route( Route r )
{
	g_route[r].fetch_add( 1, std::memory_order_relaxed );
}

void routeTake( std::int64_t out[RouteCount] )
{
	for ( int i = 0; i < RouteCount; ++i )
		out[i] = g_route[i].exchange( 0, std::memory_order_relaxed );
}

} // namespace WwHw
