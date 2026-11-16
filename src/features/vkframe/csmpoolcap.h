#pragma once
// lane CRASH1116: the sizes of the cascades' mesh pool (vkpresent.cpp csmBuf / csmPool), kept beside a unit test
// (tests/csmpoolcap). No Vulkan here.
#include <algorithm>
#include <cstdint>

namespace WwCsmPool
{
//! csmBuf's capacity for `need` bytes: 64 KB doubled until it holds them
inline std::uint64_t bufCap( std::uint64_t need )
{
	std::uint64_t cap = 65536;
	while ( cap < need )
		cap *= 2;
	return cap;
}
//! the vertices the pool can hold: the smallest of its per-vertex buffers. Each is rounded up on its own, so the
//! positions (12 bytes a vertex) can hold more vertices than the uvs (8) or the wind (4)
inline std::uint64_t vertCap( std::uint64_t posCap, std::uint64_t uvCap, std::uint64_t windCap )
{
	return std::min( { posCap / 12, uvCap / 8, windCap / 4 } );
}
} // namespace WwCsmPool
