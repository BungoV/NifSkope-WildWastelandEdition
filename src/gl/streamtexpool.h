#ifndef STREAMTEXPOOL_H
#define STREAMTEXPOOL_H

#include <algorithm>
#include <climits>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/*! lane STREAMTEX1: which file textures a streamed cell view may free, and when (no Qt, no GL: tests/streamtex).
 *
 *  Every texture a loaded cell's shader properties name is counted once per cell (note). While any loaded cell names
 *  it, nothing frees it: the keep timer used to free the textures of a loaded cell the camera had turned from, and
 *  they loaded again as soon as it turned back. A texture whose last cell left goes into the pool, least recently
 *  released first; the pool keeps up to `budget` bytes of them (a walk back finds the row it left still loaded) and
 *  plan() names the oldest past it. Textures no streamed cell ever named (the open's own blocks, the far field,
 *  the sky) keep the timer rule they had. A texture freed and loaded again within `reloadN` crossings is a reload.
 *
 *  Red (WW_CELL_STREAMTEX_RED=off): rule() is Timer for every texture and plan() names none, so the view frees what it
 *  freed before this lane; the counting still runs, so the census shows the old path's churn. */
template< class Key, class Hash = std::hash<Key> >
class StreamTexPool
{
public:
	using Cell = std::int64_t;
	static constexpr Cell kBase = INT64_MIN;	//!< a block no streamed cell wrote (the open's own)
	enum class Rule { Keep, Timer, Pool };

	struct Census
	{
		int crossing = 0;
		int refd = 0, kept = 0, evicted = 0, reloaded = 0, loads = 0;	//!< kept = resident, no cell names it
		std::uint64_t refdBytes = 0, keptBytes = 0, evictedBytes = 0, reloadedBytes = 0, loadBytes = 0;
		int released = 0;	//!< resident textures whose last loaded cell left in this crossing (they entered the pool)
		std::uint64_t releasedBytes = 0;
		int keepEvicted = 0;	//!< frees of a texture a loaded cell names (the timer's churn; 0 with the pool on)
	};

	StreamTexPool( std::uint64_t budgetBytes, int reloadCrossings, bool red )
		: budget( budgetBytes ), reloadN( reloadCrossings ), redOff( red ) {}

	static Cell cellId( int x, int y ) { return ( Cell( x ) << 32 ) | Cell( std::uint32_t( y ) ); }

	//! a shader property of `c` names `k` (once per cell however often it is asked)
	void note( Cell c, const Key & k )
	{
		Entry & e = entries[k];
		if ( c == kBase ) {
			e.base = true;
			return;
		}
		if ( !cells[c].insert( k ).second )
			return;
		e.noted = true;
		e.refs++;
	}

	//! cell `c` left: each texture it named loses one count; at 0 it enters the pool, newest last
	void dropCell( Cell c )
	{
		auto it = cells.find( c );
		if ( it == cells.end() )
			return;
		for ( const Key & k : it->second ) {
			auto e = entries.find( k );
			if ( e == entries.end() || e->second.refs <= 0 )
				continue;
			if ( --e->second.refs == 0 ) {
				e->second.released = ++clock;
				if ( e->second.resident && !e->second.base ) {
					cur.released++;
					cur.releasedBytes += e->second.bytes;
				}
			}
		}
		cells.erase( it );
	}

	//! `k` is loaded (its upload took `bytes`)
	void loaded( const Key & k, std::uint64_t bytes )
	{
		Entry & e = entries[k];
		if ( e.resident )
			resident -= e.bytes;
		e.resident = true;
		e.bytes = bytes;
		resident += bytes;
		if ( !e.refs )
			e.released = ++clock;
		cur.loads++;
		cur.loadBytes += bytes;
		if ( e.freedAt >= 0 && crossing - e.freedAt <= reloadN ) {
			cur.reloaded++;
			cur.reloadedBytes += bytes;
		}
		e.freedAt = -1;
	}

	//! `k` was freed (by the timer, or by the view after plan() named it)
	void freed( const Key & k )
	{
		auto it = entries.find( k );
		if ( it == entries.end() || !it->second.resident )
			return;
		Entry & e = it->second;
		e.resident = false;
		resident -= e.bytes;
		e.freedAt = crossing;
		cur.evicted++;
		cur.evictedBytes += e.bytes;
		if ( e.refs > 0 )
			cur.keepEvicted++;
	}

	//! `k` went without a free being counted (a flush of the whole cache): not a free, not a reload later
	void gone( const Key & k )
	{
		auto it = entries.find( k );
		if ( it == entries.end() || !it->second.resident )
			return;
		it->second.resident = false;
		resident -= it->second.bytes;
		it->second.freedAt = -1;
	}

	//! who may free `k`: nobody (a loaded cell names it), the timer (never named by a cell, or the red), the pool
	Rule rule( const Key & k ) const
	{
		if ( redOff )
			return Rule::Timer;
		auto it = entries.find( k );
		if ( it == entries.end() || !it->second.noted || it->second.base )
			return it != entries.end() && it->second.refs > 0 ? Rule::Keep : Rule::Timer;
		if ( it->second.refs > 0 )
			return Rule::Keep;
		return it->second.resident ? Rule::Pool : Rule::Timer;	// a load the pool never saw (before the stream began)
	}

	/*! the pool's textures to free now, oldest released first, until what is left fits the budget.
	 *  `busy(k)` true = bound by a recent paint: kept this time (it still counts against the budget) */
	std::vector<Key> plan( const std::function<bool( const Key & )> & busy = nullptr ) const
	{
		std::vector<Key> out;
		if ( redOff )
			return out;
		std::vector<std::pair<std::uint64_t, const Key *>> pool;
		std::uint64_t held = 0;
		for ( const auto & kv : entries ) {
			const Entry & e = kv.second;
			if ( e.resident && e.noted && !e.base && !e.refs ) {
				held += e.bytes;
				pool.emplace_back( e.released, &kv.first );
			}
		}
		if ( held <= budget )
			return out;
		std::sort( pool.begin(), pool.end(), []( const auto & a, const auto & b ) { return a.first < b.first; } );
		for ( const auto & p : pool ) {
			if ( held <= budget )
				break;
			if ( busy && busy( *p.second ) )
				continue;
			held -= entries.at( *p.second ).bytes;
			out.push_back( *p.second );
		}
		return out;
	}

	//! the camera crossed into a new centre cell: this crossing's census (the stock as it stands), the next starts
	Census cross()
	{
		Census c = cur;
		c.crossing = crossing;
		for ( const auto & kv : entries ) {
			const Entry & e = kv.second;
			if ( !e.resident )
				continue;
			if ( e.refs > 0 ) {
				c.refd++;
				c.refdBytes += e.bytes;
			} else if ( e.noted && !e.base ) {
				c.kept++;
				c.keptBytes += e.bytes;
			}
		}
		cur = Census();
		crossing++;
		return c;
	}

	std::uint64_t residentBytes() const { return resident; }
	std::uint64_t budgetBytes() const { return budget; }
	int crossings() const { return crossing; }
	bool red() const { return redOff; }
	int refs( const Key & k ) const
	{
		auto it = entries.find( k );
		return it == entries.end() ? 0 : it->second.refs;
	}
	bool isResident( const Key & k ) const
	{
		auto it = entries.find( k );
		return it != entries.end() && it->second.resident;
	}

private:
	struct Entry
	{
		int refs = 0;
		bool noted = false, base = false, resident = false;
		std::uint64_t bytes = 0, released = 0;
		int freedAt = -1;	//!< the crossing it was freed in (-1: loaded, or never freed)
	};
	std::unordered_map<Key, Entry, Hash> entries;
	std::unordered_map<Cell, std::unordered_set<Key, Hash>> cells;
	std::uint64_t budget = 0, resident = 0, clock = 0;
	int reloadN = 2, crossing = 0;
	bool redOff = false;
	Census cur;
};

#endif
