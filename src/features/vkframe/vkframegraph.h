#ifndef WW_VKFRAMEGRAPH_H
#define WW_VKFRAMEGRAPH_H

/* lane VKFRAME1 item 2 (11-12): the rules of the Vulkan frame graph that moves the cell view's passes from GL to Vulkan
 * one at a time (notes/vkframe1/plan.md). Pure: no Qt, no GL, no Vulkan headers (Vulkan enum values are spelled out
 * where a rule needs them). tests/vkframe/framegraph_test.cpp runs every rule; -DVKFRAMEGRAPH_RED feeds red = 1. */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace WwVkFrame
{

enum class Side { Gl, Vk };

/* Which side runs a moved pass this frame. Vulkan only with the device registered (bakert's, wwVkFrameSetDevice), the
 * GL import extensions present, every pipeline of the pass already built, and the pass's red off (WW_VKPOST_RED=1).
 * `why` is the `vk frame:` line's reason. */
struct Route
{
	Side side = Side::Gl;
	const char * why = "";
};

inline Route route( bool device, bool glImport, bool pipelinesBuilt, bool red )
{
	if ( red )
		return { Side::Gl, "red" };
	if ( !device )
		return { Side::Gl, "no device" };
	if ( !glImport )
		return { Side::Gl, "no GL memory/semaphore import" };
	if ( !pipelinesBuilt )
		return { Side::Gl, "pipelines not built" };
	return { Side::Vk, "vulkan" };
}

/* Pipelines are built before first use behind one VkPipelineCache: a route may use Vulkan only once all of its pipelines
 * are built, so no compile lands inside a frame. red 1 = compile on first use (the frame that first needs it). */
inline bool pipelinesUsable( int wanted, int built, int red )
{
	if ( red == 1 )
		return true;
	return wanted > 0 && built >= wanted;
}

/* Present mode when the swapchain takes presentation (VkPresentModeKHR: 0 IMMEDIATE, 1 MAILBOX, 2 FIFO, 3 FIFO_RELAXED):
 * IMMEDIATE, else MAILBOX, never FIFO (a vsync row may ask for FIFO: vsync = true). -1 = neither offered: GL keeps
 * presenting. red 1 = FIFO (the driver default). */
inline int pickPresentMode( const std::vector<int> & offered, bool vsync, int red )
{
	auto has = [&]( int m ) {
		for ( int o : offered )
			if ( o == m )
				return true;
		return false;
	};
	if ( red == 1 || vsync )
		return has( 2 ) ? 2 : -1;
	if ( has( 0 ) )
		return 0;
	if ( has( 1 ) )
		return 1;
	return -1;
}

/* Frames in flight on the frame timeline semaphore: frame n (1, 2, ...) signals value n when its last Vulkan pass is done;
 * before recording frame n the CPU waits until the timeline reached n - cap (cap = WwFramePacer::framesInFlight: 2, red 0
 * = no cap). Returns the value to wait for, 0 = no wait. */
inline std::uint64_t inFlightWait( std::uint64_t frame, int cap )
{
	if ( cap <= 0 || frame <= std::uint64_t( cap ) )
		return 0;
	return frame - std::uint64_t( cap );
}

/* GPU timestamps to ms: valid bits from the queue family (timestampValidBits), period in ns (timestampPeriod). */
inline double ticksToMs( std::uint64_t a, std::uint64_t b, double periodNs, int validBits )
{
	const std::uint64_t mask = validBits >= 64 ? ~std::uint64_t( 0 ) : ( ( std::uint64_t( 1 ) << validBits ) - 1 );
	const std::uint64_t d = ( ( b & mask ) - ( a & mask ) ) & mask;
	return double( d ) * periodNs * 1e-6;
}

/* Image states and the barrier between two uses. Layouts by name (the glue maps them to VkImageLayout); a GL-shared
 * image sits in General while GL may read it. */
enum class Layout { Undefined, General, ShaderRead, TransferSrc, TransferDst, Present };

struct Use
{
	Layout layout = Layout::General;
	bool write = false;
	bool compute = true;	// false = graphics / transfer stage
};

struct Barrier
{
	bool need = false;
	Layout from = Layout::Undefined;
	Layout to = Layout::Undefined;
	bool hazard = false;	// a write before, or a write now: the memory dependency, not only the layout
};

/* A barrier is needed on a layout change and on every hazard (write -> any, read -> write); two reads in one layout need
 * none. red 1 = layout changes only (a write-after-write in General would race). */
inline Barrier between( const Use & before, const Use & now, int red )
{
	Barrier b;
	b.from = before.layout;
	b.to = now.layout;
	b.hazard = before.write || now.write;
	b.need = before.layout != now.layout || ( red != 1 && b.hazard );
	return b;
}

/* One frame's ledger: the passes in frame order with their side and GPU ms; every change of side between consecutive
 * passes is one semaphore edge (GL signals, Vulkan waits, or the other way). red 1 = edges not counted. */
struct Ledger
{
	struct Pass
	{
		std::string name;
		Side side;
		double ms;
	};
	std::vector<Pass> passes;

	void add( const std::string & name, Side side, double ms ) { passes.push_back( { name, side, ms } ); }

	int count( Side s ) const
	{
		int n = 0;
		for ( const Pass & p : passes )
			n += p.side == s ? 1 : 0;
		return n;
	}
	double ms( Side s ) const
	{
		double t = 0.0;
		for ( const Pass & p : passes )
			t += p.side == s ? p.ms : 0.0;
		return t;
	}
	int edges( Side from, int red ) const
	{
		if ( red == 1 )
			return 0;
		int n = 0;
		for ( size_t i = 1; i < passes.size(); i++ )
			n += ( passes[i - 1].side == from && passes[i].side != from ) ? 1 : 0;
		return n;
	}

	/* `vk frame: passes vulkan N [names] gl M | ms vulkan X gl Y | waits W (gl->vk a, vk->gl b) | route <why>` */
	std::string line( const char * why, int red ) const
	{
		std::string vkNames;
		for ( const Pass & p : passes )
			if ( p.side == Side::Vk )
				vkNames += ( vkNames.empty() ? "" : "," ) + p.name;
		const int a = edges( Side::Gl, red ), b = edges( Side::Vk, red );
		char buf[256];
		std::snprintf( buf, sizeof( buf ), "vk frame: passes vulkan %d [%s] gl %d | ms vulkan %.3f gl %.3f | waits %d (gl->vk %d, vk->gl %d) | route %s",
			count( Side::Vk ), vkNames.c_str(), count( Side::Gl ), ms( Side::Vk ), ms( Side::Gl ), a + b, a, b, why );
		return buf;
	}
};

/* The speed bar of a moved pass: never slower than its GL twin by more than 0.1 ms or 5 % (both must hold). */
inline bool withinBar( double vkMs, double glMs )
{
	const double d = vkMs - glMs;
	return d <= 0.1 && ( glMs <= 0.0 ? d <= 0.0 : d / glMs <= 0.05 );
}

}

#endif
