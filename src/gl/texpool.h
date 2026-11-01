#ifndef TEXPOOL_H
#define TEXPOOL_H

/* Lane PERFFIX4 (2026-11-01): THE STREAMING TEXTURE POOL. Pure C++ (tests/texpool, g++ alone).
 *
 * Before: every file texture no paint had bound for 5 s was freed (WW_CELL_STREAM_TEXKEEP). A walk read 1456 of them
 * again after they came back into view (SHOTPASS1: texAheadAgain), and each free/reload left NVIDIA's driver pool at a
 * higher mark: private memory x1.275 over 20 crossings with the resident textures back at their start figure.
 * Now: textures stay until the resident bytes pass the preset's budget; then the least recently bound go first, down
 * to 90 % of the budget (no free/reload ping-pong at the edge), and never one bound by the last 3 paints. A working set
 * bigger than the budget keeps what the last 3 paints bound (the old rule's floor), nothing more.
 * Budgets (PERFRES1101 C/streaming.md, REASONED from Epic's 564-674 MB Paragon pool and our 6.74 GB c3 set):
 * Low 1024, Medium 2048, High 4096, Ultra 6144 MB. */

#include <algorithm>
#include <cstdint>
#include <vector>

namespace WwTexPool
{

constexpr int kBudgetMb[4] = { 1024, 2048, 4096, 6144 };

struct Entry
{
	int index = 0;			//!< the caller's slot
	std::uint32_t useTick = 0;	//!< the use clock when last bound
	std::int64_t bytes = 0;		//!< on the card
};

/*! The slots to free, oldest first: nothing while `total` <= `budget`; else the least recently used whose last bind is
 *  before `protectTick`, until the total is <= 90 % of the budget (or nothing more may go). */
inline std::vector<int> pick( std::vector<Entry> e, std::int64_t total, std::int64_t budget, std::uint32_t protectTick )
{
	std::vector<int> out;
	if ( budget <= 0 || total <= budget )
		return out;
	const std::int64_t target = budget - budget / 10;
	std::stable_sort( e.begin(), e.end(), []( const Entry & a, const Entry & b ) { return a.useTick < b.useTick; } );
	for ( const Entry & x : e ) {
		if ( total <= target )
			break;
		if ( x.useTick >= protectTick )
			break;	// sorted: everything after was bound as recently
		out.push_back( x.index );
		total -= x.bytes;
	}
	return out;
}

//! Budget bytes for a preset 0..3 (Low..Ultra), or an explicit MB count (> 3).
inline std::int64_t budgetBytes( int presetOrMb )
{
	const int mb = presetOrMb >= 0 && presetOrMb <= 3 ? kBudgetMb[presetOrMb] : presetOrMb;
	return std::int64_t( std::max( mb, 0 ) ) << 20;
}

} // namespace WwTexPool

#endif // TEXPOOL_H
