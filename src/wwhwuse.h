#ifndef WWHWUSE_H
#define WWHWUSE_H

/* lane EMPTY240 round 3: what feeds the `hardware use:` and `cell stream load:` lines. R3-1 the stream's workers (slots =
 * the cores minus the draw thread and the driver), R3-2 every archive read (read = the packed bytes paged in from the mapped
 * archive, inflate = the extract after that), R3-3 the RAM cache of extracted archive files (budget = free RAM minus the
 * memguard margin; red WW_RAMCACHE_RED=1), R3-4 the draws' program routes (why a near draw is or is not bindless). */

#include "ba2file.hpp"

#include <QByteArray>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>

namespace WwHw
{

//! R3-1: worker slots for stream work: idealThreadCount() - 2, at least 1
int poolSlots();	// not `slots`: a Qt keyword macro
std::atomic<int> & busyCount();
//! lane EMPTY240 R5-4 (build 77: workers 0.6 / 14 at load): what each busy worker is doing, for the `hardware use:` line
enum BusyKind { BusyOther, BusyBuilder, BusyModels, BusyTextures, BusyTrim, BusyKinds };
inline std::atomic<int> & busyKind( int k )
{
	static std::atomic<int> n[BusyKinds];
	return n[k];
}
//! one stream worker busy while it lives (builder, model parse, texture read-ahead, archive trim)
struct Busy
{
	explicit Busy( int kind = BusyOther ) : kind_( kind )
	{
		busyCount().fetch_add( 1, std::memory_order_relaxed );
		busyKind( kind_ ).fetch_add( 1, std::memory_order_relaxed );
	}
	~Busy()
	{
		busyCount().fetch_sub( 1, std::memory_order_relaxed );
		busyKind( kind_ ).fetch_sub( 1, std::memory_order_relaxed );
	}
	Busy( const Busy & ) = delete;
	Busy & operator=( const Busy & ) = delete;
private:
	int kind_;
};

//! R3-2: archive reads since launch, every thread (ns and bytes)
struct Io
{
	std::int64_t files = 0, cacheHits = 0, packed = 0, bytes = 0, readNs = 0, inflateNs = 0;
	std::int64_t drawFiles = 0, drawNs = 0;	//!< the part read on the drawing (GUI) thread
	std::int64_t indexBuilds = 0, indexNs = 0, indexArchives = 0;
};
Io io();
Io ioDelta( const Io & now, const Io & then );

/*! One archive file read through the RAM cache. `ix` names the index (its cache entries are forgotten with it), `path`
 *  the full path; `mapped`/`packed` the packed bytes inside a mapped archive (null for a loose file: no page-in step);
 *  `extract` fills `out` and may throw (the caller's own catch keeps its meaning). */
bool read( const void * ix, std::string_view path, const unsigned char * mapped, std::size_t packed, QByteArray & out,
	const std::function<void( QByteArray & )> & extract );
//! one archive file of `ix` through read(): a loose file skips the cache and the page-in step
inline bool extract( const BA2File & ix, const BA2File::FileInfo & fd, QByteArray & out,
	unsigned char * ( *alloc )( void *, size_t ) )
{
	const bool loose = fd.archiveFile == 0xFFFFFFFFU;
	const unsigned char * mapped = ( !loose && fd.archiveType == 0 ) ? fd.fileData : nullptr;
	const std::size_t packed = fd.packedSize ? fd.packedSize : fd.unpackedSize;
	return read( loose ? nullptr : &ix, fd.fileName, mapped, packed, out,
		[&]( QByteArray & o ) { ix.extractFile( &o, alloc, fd ); } );
}
//! an index is closed or rebuilt: its cached files go (BA2File::destroyHook calls it)
void forget( const void * ix );
//! archive files opened since launch, the opens that shared a live mapping, the mappings live now
void archiveOpens( std::uint64_t * opens, std::uint64_t * shared, std::size_t * live );
//! an index was built: `who` names the reader, `archives` the archive files it mapped, `ms` its wall
void indexBuilt( const char * who, int archives, double ms );

//! times an index build from here to the scope's end and prints the `archive index:` census line
struct IndexScope
{
	const char * who;
	std::uint64_t opens0 = 0;
	std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	explicit IndexScope( const char * w ) : who( w ) { archiveOpens( &opens0, nullptr, nullptr ); }
	~IndexScope()
	{
		std::uint64_t opens = 0;
		archiveOpens( &opens, nullptr, nullptr );
		indexBuilt( who, int( opens - opens0 ),
			std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count() );
	}
	IndexScope( const IndexScope & ) = delete;
	IndexScope & operator=( const IndexScope & ) = delete;
};

//! R3-3: the RAM cache now
struct Cache
{
	std::int64_t bytes = 0, budget = 0, files = 0, hits = 0, misses = 0, evicted = 0;
	bool on = false;
};
Cache cache();

//! R3-4: the draws' program routes, counted per paint by Renderer::setupProgram
enum Route { RouteCalls, RouteCell, RouteAsked, RouteBindless, RoutePbrm, RouteWater,
	// R4-3: why a cell-lit draw did not ask the bindless twin (renderer.cpp wwBlWants), and an asked twin that was missing
	RouteNoLsp, RouteBlOff, RouteFarRing, RouteVersion, RouteSelecting, RouteRefraction, RouteCensus, RouteTwinMissing,
	RouteCount };
void route( Route r );
//! the counts since the last call (the hardware line's window)
void routeTake( std::int64_t out[RouteCount] );

} // namespace WwHw

#endif
