#ifndef CLOUDCALJOBS_H
#define CLOUDCALJOBS_H

#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

/* lane PERFFIX3 (item 4): the clouds' coverage tables (cloudvol.cpp calibrated / calibratedVariety) built off the GL
 * thread. No GL, no Qt: tests/cloudcaljobs drives it with the real tables.
 *
 * The old memo recomputed a table ON THE GL THREAD whenever its quantised key (heights to 20 m, density to 0.01) moved,
 * from the exact inputs of that frame: 19-22 ms a Variety set, ~10 ms a band. The bands sit at the eye's height plus
 * the type's (cloudmap.cpp), so a camera that climbs 20 m, or a live row's settle at the previous row's camera and then
 * its path's first key, rebuilt them (merge1030a run2 c3_cv_on2: 3.07..5.07 km then 3.80..5.80 km, twice a row).
 *
 * Here a Slot keeps that trigger exactly (the quantised key moves -> want the exact inputs of that frame) and asks Jobs
 * for the table of those exact inputs: a job keyed on the inputs' BITS, run on a worker (std::async), the last kKeep
 * jobs kept, so the same exact inputs never compute twice (a row that goes back to a camera it had costs nothing) and
 * the bytes are the same function of the same inputs as before. While a job runs the slot keeps the table it has (the
 * frames of that wait draw the previous table); a slot that never had one waits for its first (the old cost, cut by the
 * job's own parallel tables). Mode Sync (the red WW_CLOUDVOL_RED=gltables) is the old memo, on the calling thread. */
namespace WwCloudCalJobs
{

using Table = std::vector<float>;
using Bits = std::vector<std::uint32_t>;	// a table's exact inputs as bit patterns (floats) and ints: the cache key

//! appends a float's bits (so -0 and 0 are two keys, as two inputs the table could tell apart)
inline void put( Bits & k, float v )
{
	std::uint32_t u = 0;
	std::memcpy( &u, &v, sizeof( u ) );
	k.push_back( u );
}

inline void put( Bits & k, int v )
{
	k.push_back( std::uint32_t( v ) );
}

struct Result
{
	Table table;
	double ms = 0.0;	// the job's own wall time (on the worker)
};

class Jobs
{
public:
	static constexpr int kKeep = 16;

	//! the job of these exact inputs: found (any state), or started on a worker
	std::shared_future<Result> get( const Bits & key, const std::function<Table()> & compute )
	{
		std::lock_guard<std::mutex> l( lock_ );
		for ( auto it = jobs_.begin(); it != jobs_.end(); ++it )
			if ( it->first == key ) {
				jobs_.splice( jobs_.begin(), jobs_, it );	// most recent first
				hits_++;
				return jobs_.front().second;
			}
		started_++;
		/* a detached thread and a promise, not std::async: the last future of an std::async job blocks in its destructor
		 * until the job ends, which a dropped, superseded job would make the GL thread do */
		auto p = std::make_shared<std::promise<Result>>();
		std::shared_future<Result> f = p->get_future().share();
		std::thread( [p, compute]() {
			const auto t0 = std::chrono::steady_clock::now();
			Result r;
			r.table = compute();
			r.ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
			p->set_value( std::move( r ) );
		} ).detach();
		jobs_.emplace_front( key, f );
		while ( int( jobs_.size() ) > kKeep )
			jobs_.pop_back();	// forgets the inputs; a slot still holding the future gets its table all the same
		return f;
	}

	int started() const { return started_; }
	int hits() const { return hits_; }

private:
	std::mutex lock_;
	std::list<std::pair<Bits, std::shared_future<Result>>> jobs_;
	int started_ = 0, hits_ = 0;
};

enum class Mode
{
	Async,	// a worker builds it; the slot keeps its table until then (waits only for its first)
	Sync	// red: the old memo, built here on a quantised-key move
};

enum class Got
{
	Same,		// nothing moved: the table it had
	Fresh,		// a new table now (ready, built here (Sync), or the first one waited for)
	Pending		// a new table is being built: the old one is kept this frame
};

class Slot
{
public:
	/*! qkey: the old memo's quantised key (it decides WHEN a table is due, as before); exact: the inputs' bits at that
	 *  moment; compute: builds the table of those inputs (captured by value: it runs on a worker). */
	Got update( Jobs & jobs, const Bits & qkey, const Bits & exact, const std::function<Table()> & compute, Mode mode )
	{
		if ( qkey != lastQ_ || mode != lastMode_ ) {
			lastQ_ = qkey;
			lastMode_ = mode;
			want_ = exact;
			if ( mode == Mode::Sync ) {	// the old memo: now, here, from these inputs
				const auto t0 = std::chrono::steady_clock::now();
				table_ = compute();
				lastMs_ = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
				have_ = want_;
				job_ = {};
				waited_ = 0.0;
				return Got::Fresh;
			}
#ifdef CLOUDCALJOBS_RED	// the unit test's red: jobs keyed on the QUANTISED key (a stale table for other exact inputs)
			job_ = jobs.get( qkey, compute );
#else
			job_ = jobs.get( want_, compute );
#endif
		}
		if ( have_ == want_ && valid_() )
			return Got::Same;
		if ( !job_.valid() )
			return Got::Same;
		const bool ready = job_.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready;
		if ( !ready && valid_() ) {
			pendingFrames_++;
			return Got::Pending;
		}
		const auto t0 = std::chrono::steady_clock::now();
		const Result & r = job_.get();	// ready, or the slot's first table: wait for it
		waited_ = ready ? 0.0 : std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		table_ = r.table;
		lastMs_ = r.ms;
		have_ = want_;
		keptFrames_ = pendingFrames_;
		pendingFrames_ = 0;
		job_ = {};
		return Got::Fresh;
	}

	const Table & table() const { return table_; }
	bool pending() const { return job_.valid() && !( have_ == want_ ); }
	double lastMs() const { return lastMs_; }		// the last table's build time (worker wall time, or here in Sync)
	double waitedMs() const { return waited_; }	// what the GL thread waited for it (0 unless it was the first)
	int keptFrames() const { return keptFrames_; }	// frames the previous table stood in while it was built

private:
	bool valid_() const { return !have_.empty(); }
	Bits lastQ_, want_, have_;
	Mode lastMode_ = Mode::Async;
	Table table_;
	std::shared_future<Result> job_;
	double lastMs_ = 0.0, waited_ = 0.0;
	int pendingFrames_ = 0, keptFrames_ = 0;
};

}	// namespace WwCloudCalJobs

#endif
