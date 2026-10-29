#pragma once

/*! lane ROOMFIT11: the small rooms' ray casts, merged across rooms into one cast.
 *
 *  The fine fit runs its small rooms a room per thread, and each room sends its own edge and classification batches to
 *  the scene. The scene casts one batch at a time (one device lock), so many small batches queue behind each other and
 *  each pays a submit, a wait and a post pass of its own. Here a room's batch waits in a queue; whichever room finds no
 *  cast running takes every queued batch of the same kind (any-hit or nearest, in queue order, up to a ray cap), casts
 *  them as one, and hands each room its own hits back. A ray's answer depends only on the ray and its kind, so every
 *  room gets the hits it would have got alone, in its own order.
 *
 *  Pure header (std only) so tests/roomfit11 runs it under g++ with a stand-in cast. */

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

template <class Ray, class Hit>
class RoomCastBatcher
{
public:
	using CastFn = std::function<void( const Ray *, size_t, Hit *, bool )>;

	//! fn casts n rays into n hits; one cast carries at most maxRays (a single larger batch goes alone)
	explicit RoomCastBatcher( CastFn fn, size_t maxRays = 4000000 ) : fn_( std::move( fn ) ), maxRays_( std::max<size_t>( 1, maxRays ) ) {}

	//! the room's rays, cast with whatever other rooms have queued; returns when out[0..n) holds this room's hits
	void cast( const Ray * rays, size_t n, Hit * out, bool anyHit )
	{
		if ( !n )
			return;
		Req q { rays, n, out, anyHit };
		std::unique_lock<std::mutex> lk( m_ );
		pend_.push_back( &q );
		requests_++;
		for ( ;; ) {
			if ( q.done )
				return;
			if ( busy_ ) {
				cv_.wait( lk );
				continue;
			}
			busy_ = true;
			// the oldest queued batch decides the kind; every later one of that kind joins it, in queue order, to the cap
			std::vector<Req *> take, keep;
			const bool kind = pend_.front()->any;
			size_t total = 0;
			for ( Req * p : pend_ ) {
				if ( p->any == kind && ( take.empty() || total + p->n <= maxRays_ ) ) {
					take.push_back( p );
					total += p->n;
				} else {
					keep.push_back( p );
				}
			}
			pend_.swap( keep );
			lk.unlock();
			run( take, total, kind );
			lk.lock();
			calls_++;
			mostRooms_ = std::max( mostRooms_, take.size() );
			for ( Req * p : take )
				p->done = true;
			busy_ = false;
			cv_.notify_all();
		}
	}

	// the census: casts sent to the scene, room batches they carried, the most batches one cast carried
	size_t calls() const { std::lock_guard<std::mutex> lk( m_ ); return calls_; }
	size_t requests() const { std::lock_guard<std::mutex> lk( m_ ); return requests_; }
	size_t mostRooms() const { std::lock_guard<std::mutex> lk( m_ ); return mostRooms_; }

private:
	struct Req
	{
		const Ray * rays;
		size_t n;
		Hit * out;
		bool any;
		bool done = false;
	};

	void run( const std::vector<Req *> & take, size_t total, bool kind )
	{
		if ( take.size() == 1 ) {   // alone: straight into the room's own arrays
			fn_( take[0]->rays, take[0]->n, take[0]->out, kind );
			return;
		}
		rays_.resize( total );
		hits_.resize( total );
		size_t at = 0;
		for ( const Req * p : take ) {
			std::copy( p->rays, p->rays + p->n, rays_.begin() + std::ptrdiff_t( at ) );
			at += p->n;
		}
		fn_( rays_.data(), total, hits_.data(), kind );
		at = 0;
#ifdef ROOMFIT11_RED
		// the red (tests/roomfit11 only): the hits handed back in reverse room order, so each room reads another's
		for ( size_t i = take.size(); i-- > 0; ) {
			const Req * p = take[i];
			std::copy( hits_.begin() + std::ptrdiff_t( at ), hits_.begin() + std::ptrdiff_t( std::min( total, at + p->n ) ), p->out );
			at += p->n;
		}
#else
		for ( const Req * p : take ) {
			std::copy( hits_.begin() + std::ptrdiff_t( at ), hits_.begin() + std::ptrdiff_t( at + p->n ), p->out );
			at += p->n;
		}
#endif
	}

	CastFn fn_;
	size_t maxRays_;
	mutable std::mutex m_;
	std::condition_variable cv_;
	std::vector<Req *> pend_;
	bool busy_ = false;
	size_t calls_ = 0, requests_ = 0, mostRooms_ = 0;
	std::vector<Ray> rays_;   // touched only by the room running the cast
	std::vector<Hit> hits_;
};
