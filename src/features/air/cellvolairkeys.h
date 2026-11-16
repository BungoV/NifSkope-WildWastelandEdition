#pragma once

// Lane VKAIR7 (2026-11-16): the air's sharedTarget keys (vkpresent.h). A VaGpu (cellvolair.cpp) lives as a QHash value
// keyed by renderer, and a rehash or a span growth moves the value: an address inside it (&g.inject, &g.tile) is not a
// stable key. The old route then made the targets again under the new address and the old key's targets stayed until
// a present shutdown. The keys are heap bytes the value holds by shared_ptr, which moves with the value.
// Gate: tests/vkframe/airkeys.sh.

#include <cstdint>
#include <memory>

struct VaVkKeys
{
	enum Slot
	{
		Inject = 0,	// the injected volume (vkfog.inc)
		Integ = 1,	// the integrated volume (WwVolAirShare.integKey)
		Tile = 2,	// stage 0's per-column distances (vkairinject.inc)
		Z = 3,		// stage 0's depth
		// VKAIR10 (appended, never renumber the above): stage 1's history and the lamp volume (|3 / |2), each ping-pong
		Hist0 = 4,	// hist[0] / hist[1]: the strata history (volHist)
		Hist1 = 5,
		LvNow0 = 6,	// lvNow[0] / lvNow[1]: the lamp in-scatter (volLampNow)
		LvNow1 = 7,
		LvHist0 = 8,	// lvHist[0] / lvHist[1]: the lamp steady term (volLampHist, stage 4)
		LvHist1 = 9,
		Noise = 10,	// the drift lumps (volNoise)
		Terr = 11,	// the terrain air grid (airTerr)
		Stbn = 12,	// stage 4's blue noise (volStbn)
		kSlots = 13
	};
	std::shared_ptr<char> bytes { new char[kSlots](), std::default_delete<char[]>() };
	std::uintptr_t at( int slot ) const { return reinterpret_cast<std::uintptr_t>( bytes.get() + slot ); }
};
