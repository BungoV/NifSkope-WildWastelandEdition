#ifndef TEXGRAVE_H
#define TEXGRAVE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/*! lane STREAMTEX2 (ported onto the PERFFIX4 / STREAMTEX3 pool by STREAMTEX4): textures the stream view freed, kept until
 *  the GPU is past every frame that may still sample them (no Qt, no GL: tests/texgrave).
 *
 *  The pool frees a texture drawn as recently as four paints ago (the keep timer waited 5 s). With bindless draws on,
 *  rows of frames still queued on the GPU name its handle; dropping the handle (non-resident) and deleting the GL name
 *  at once let a queued frame sample a dead handle, and let the next glGenTextures hand the name to another file. A
 *  freed texture's names go into the open batch here (the cache entry forgets them at once, so the next bind loads into
 *  new names); seal() closes the batch behind one fence taken after the frees; sweep() hands a batch's names to
 *  `release` (non-resident, then glDeleteTextures) once `passed(fence)` says the GPU is past it. Fences pass in the order
 *  they were taken, so a sweep stops at the first that has not.
 *
 *  Red WW_CELL_STREAMTEX_RED=unfenced (also nofence): the view frees at once, as before (TexCache::evictPool and the keep
 *  timer's evictUnused, fenced false). */
template< class Fence >
class TexGrave
{
public:
	//! a freed texture's GL names (id1: FO76's second cube, 0 when none); its bindless handles stay with them
	void bury( std::uint32_t id0, std::uint32_t id1 ) { open.emplace_back( id0, id1 ); }

	//! closes the open batch behind `fence`; false (and `fence` unused) when nothing was buried since the last seal
	bool seal( Fence fence )
	{
		if ( open.empty() )
			return false;
		batches.push_back( Batch{ std::move( open ), fence } );
		open.clear();
		return true;
	}

	bool unsealed() const { return !open.empty(); }
	bool empty() const { return open.empty() && batches.empty(); }

	//! textures waiting (sealed or not)
	std::size_t size() const
	{
		std::size_t n = open.size();
		for ( const Batch & b : batches )
			n += b.ids.size();
		return n;
	}

	/*! each passed batch's names go to `release( id0, id1 )` and its fence to `drop( fence )`, oldest first. `all`: every
	 *  batch and the open one too, `passed` not asked (the caller already waited: TexCache::flush's glFinish). Returns
	 *  the textures released. */
	template< class Passed, class Release, class Drop >
	int sweep( Passed && passed, Release && release, Drop && drop, bool all = false )
	{
		int n = 0;
		std::size_t i = 0;
		for ( ; i < batches.size(); i++ ) {
			if ( !all && !passed( batches[i].fence ) )
				break;
			for ( const auto & p : batches[i].ids ) {
				release( p.first, p.second );
				n++;
			}
			drop( batches[i].fence );
		}
		batches.erase( batches.begin(), batches.begin() + std::ptrdiff_t( i ) );
		if ( all ) {
			for ( const auto & p : open ) {
				release( p.first, p.second );
				n++;
			}
			open.clear();
		}
		return n;
	}

private:
	struct Batch
	{
		std::vector<std::pair<std::uint32_t, std::uint32_t>> ids;
		Fence fence;
	};
	std::vector<std::pair<std::uint32_t, std::uint32_t>> open;
	std::vector<Batch> batches;
};

#endif
