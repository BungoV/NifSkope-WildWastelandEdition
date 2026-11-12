#pragma once
/* lane VKCOMPUTE1 (bungo 11-12 "everything fully utilized"): the RT device's async compute rules, pure (no Vulkan, no GL,
 * no Qt), unit-tested by tests/vkasync (green + reds -DVKASYNC_RED_<NAME>).
 *  1. pickFamily: the queue family the device takes. A COMPUTE family without GRAPHICS that has timestamps (the device ms
 *     lines need them) runs beside GL's graphics queue; none, or the red WW_VKASYNC_RED=1 / the row off: the first COMPUTE
 *     family (the graphics one on NVIDIA/AMD), as before.
 *  2. Ledger: GL's wait on a submit's vkDone. Before: GL waited right after the submit, so GL's queue sat idle until the
 *     device finished (serial, whatever the family). Async: the wait is OWED and placed at the first GL read of the
 *     result (settle), or before GL's next signal on the pair (one wait per signal, always). */
#include <cstdint>

namespace vkasync {

constexpr uint32_t kGraphics = 0x1, kCompute = 0x2;   // VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT

enum class Why { Async, NoAsyncFamily, Red, Off, NoCompute };

struct Pick
{
	int family = -1;
	Why why = Why::NoCompute;
};

inline const char * whyText( Why w )
{
	switch ( w ) {
	case Why::Async: return "a compute-only family (async, beside GL's graphics queue)";
	case Why::NoAsyncFamily: return "the first compute family (this device has no compute-only family with timestamps)";
	case Why::Red: return "the first compute family (RED WW_VKASYNC_RED=1: the graphics family, as before)";
	case Why::Off: return "the first compute family (the Async compute row is off)";
	case Why::NoCompute: return "no compute family";
	}
	return "?";
}

//! flags / tsBits: per family (VkQueueFamilyProperties queueFlags / timestampValidBits); want = the row on; red = WW_VKASYNC_RED=1
inline Pick pickFamily( const uint32_t * flags, const uint32_t * tsBits, uint32_t n, bool want, bool red )
{
	Pick p;
	int first = -1, async = -1;
	for ( uint32_t k = 0; k < n; k++ ) {
		if ( !( flags[k] & kCompute ) )
			continue;
		if ( first < 0 )
			first = int( k );
#ifndef VKASYNC_RED_NOTS
		const bool ts = tsBits[k] > 0;
#else
		const bool ts = true;
#endif
#ifndef VKASYNC_RED_GRAPHICS
		if ( async < 0 && !( flags[k] & kGraphics ) && ts )
#else
		if ( async < 0 && ts )
#endif
			async = int( k );
	}
	if ( first < 0 )
		return p;
	p.family = first;
	p.why = red ? Why::Red : !want ? Why::Off : async < 0 ? Why::NoAsyncFamily : Why::Async;
	if ( p.why == Why::Async )
		p.family = async;
	return p;
}

/*! one shared result's GL side: every device submit that signals vkDone gets exactly one GL wait. defer = the async route
 *  (the wait is owed until the first read or the next signal); !defer = the old route (the wait right after the
 *  submit). W is a callable that issues GL's wait (glWaitSemaphoreEXT: a server wait, the CPU does not block) */
struct Ledger
{
	bool owed = false;          // a submit's vkDone that GL has not waited on yet
	uint64_t submits = 0, waits = 0, deferred = 0;

	template <class W>
	void afterSubmit( bool defer, W && wait )
	{
#ifdef VKASYNC_RED_EAGER
		defer = false;
#endif
		submits++;
		if ( owed ) {   // a signal always settles first, so this does not happen; kept balanced anyway
			wait();
			waits++;
		}
		owed = defer;
		if ( defer ) {
			deferred++;
			return;
		}
		wait();
		waits++;
	}
	//! before any GL read of the result, and before GL's next signal on this pair: the owed wait, once. True = it waited now
	template <class W>
	bool settle( W && wait )
	{
#ifdef VKASYNC_RED_NOSETTLE
		return false;
#endif
		if ( !owed )
			return false;
		owed = false;
		wait();
		waits++;
		return true;
	}
	//! GL may read the result: no device write in flight that GL has not waited on
	bool readable() const { return !owed; }
	void reset() { owed = false; }
};

} // namespace vkasync
