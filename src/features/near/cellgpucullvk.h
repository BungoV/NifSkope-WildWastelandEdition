#pragma once
/* lane VKOBJ7 (F1): GPU cull step B on Vulkan. Under WW_VKOBJ_SHADE, GpuDrivenB::draw (cellgpucull.cpp) hands a shape to
 * vkCellDraw / vkBlDraw (cellbindless.cpp) before its GL draws, with the CPU cull's kept ranges: the commands step B's own GL
 * fallback issues, the same shape and form as bsshape's cull branch. The GPU cull's cluster commands live in GL's indirect
 * buffer, so the Vulkan draw keeps the frustum cull and loses the Hi-Z one: it draws every kept cluster, GL only those the
 * Hi-Z kept (the dropped ones are hidden by the Hi-Z's own rule, tests/features/near/signdrop1). Refused: GL draws it as before.
 * No GL or Qt here: tests/vkframe/blview (cell rows `stepb`) draws GL's cluster form against these commands. */
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GpuDrivenBVk
{
/*! The draws of one step-B shape: `whole`, one draw of min( numTriangles, recTris ) triangles from index 0; else one draw per kept
 * (first triangle, triangles) pair in `ranges`. Counts in indices, offsets in bytes of the shape's 16-bit element buffer (GL's
 * multi-draw arguments). red 1 (blview's `stepb` red only): the last kept range dropped when more than one is kept (an empty draw
 * list is refused, which would test nothing) */
inline void rangeDraws( bool whole, const std::uint32_t * ranges, std::size_t nRanges, long long numTriangles, std::uint32_t recTris,
	std::vector<int> & counts, std::vector<const void *> & offs, int red = 0 )
{
	counts.clear();
	offs.clear();
	if ( whole ) {
		counts.push_back( int( std::min<long long>( numTriangles, recTris ) * 3 ) );
		offs.push_back( nullptr );
	} else {
		for ( std::size_t k = 0; k + 1 < nRanges; k += 2 ) {
			if ( !ranges[k + 1] )
				continue;  // an empty range draws nothing on either side
			counts.push_back( int( ranges[k + 1] * 3 ) );
			offs.push_back( reinterpret_cast<const void *>( std::size_t( ranges[k] ) * 6 ) );
		}
	}
	if ( red == 1 && counts.size() > 1 ) {
		counts.pop_back();
		offs.pop_back();
	}
}
}  // namespace GpuDrivenBVk
