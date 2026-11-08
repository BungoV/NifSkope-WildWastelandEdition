#ifndef WWFRAMEPOOL_H
#define WWFRAMEPOOL_H

/* lane PERFFIX6 PHASE 2 (MT): one persistent worker pool for frame work that touches no GL (culling, command and
 * matrix packing). GL calls stay on the render thread. run( n, fn ) calls fn( 0 .. n-1 ) on the workers and the caller
 * and returns when every index is done; a caller that wants byte-identical output writes each index into its own slot
 * and retires the slots in index order afterwards (skill ww-parallelise-a-stage). Workers: hardware_concurrency - 2,
 * started on the first run and never ended (detached; the process exit takes them). A nested run, a run from a second
 * thread at the same time and the red ^WW_MT_RED=1 run inline, in index order (the serial path). */

#include "wwswitch.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>

namespace WwFramePool
{

//! ^WW_MT_RED=1: every run inline on the calling thread (the serial path, the same order)
inline bool red()
{
	WW_SWITCH_STATIC( bool, r, wwSwitchEcho( "frame worker pool red (MT)", wwEnv( "WW_MT_RED" ).trimmed() == "1" ) );
	return r;
}

namespace detail
{

struct Pool
{
	std::mutex m, caller;
	std::condition_variable wake, done;
	const std::function<void( int )> * fn = nullptr;
	int n = 0, threads = 0, left = 0;	// left: workers that have not finished the current run (under m)
	unsigned gen = 0;	// under m
	std::atomic<int> next { 0 };

	void work()
	{
		inWorker() = true;
		unsigned seen = 0;
		std::unique_lock<std::mutex> l( m );
		for ( ;; ) {
			wake.wait( l, [&] { return gen != seen; } );
			seen = gen;
			const std::function<void( int )> * f = fn;
			const int count = n;
			l.unlock();
			for ( int i; ( i = next.fetch_add( 1 ) ) < count; )
				( *f )( i );
			l.lock();
			if ( --left == 0 )
				done.notify_one();
		}
	}

	static bool & inWorker()
	{
		static thread_local bool w = false;
		return w;
	}
};

inline Pool & pool()
{
	static Pool * p = [] {
		Pool * q = new Pool;	// never freed: the detached workers may outlive static destruction
		const unsigned hc = std::thread::hardware_concurrency();
		q->threads = hc > 3 ? int( std::min( hc - 2, 30u ) ) : 0;
		for ( int i = 0; i < q->threads; i++ )
			std::thread( [q] { q->work(); } ).detach();
		std::fprintf( stderr, "frame pool: %d workers (hardware threads %u)\n", q->threads, hc );
		return q;
	}();
	return *p;
}

} // namespace detail

//! the pool's worker count (0: every run inline); starts the pool
inline int workers()
{
	return detail::pool().threads;
}

inline void run( int n, const std::function<void( int )> & fn )
{
	if ( n <= 0 )
		return;
	detail::Pool & p = detail::pool();
	std::unique_lock<std::mutex> one( p.caller, std::try_to_lock );
	if ( n == 1 || p.threads == 0 || detail::Pool::inWorker() || !one.owns_lock() || red() ) {
		for ( int i = 0; i < n; i++ )
			fn( i );
		return;
	}
	{
		std::lock_guard<std::mutex> l( p.m );
		p.fn = &fn;
		p.n = n;
		p.next.store( 0 );
		p.left = p.threads;
		p.gen++;
	}
	p.wake.notify_all();
	for ( int i; ( i = p.next.fetch_add( 1 ) ) < n; )
		fn( i );
	// every worker reports this run before the next one may reset the counter (a late waker never takes a new index)
	std::unique_lock<std::mutex> l( p.m );
	p.done.wait( l, [&] { return p.left == 0; } );
	p.fn = nullptr;
}

} // namespace WwFramePool

#endif
