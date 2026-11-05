#ifndef CELLDRAWPACKET_H
#define CELLDRAWPACKET_H

/* lane GAPPERF-BINDLESS (PERF5-DRAWPACKETS): a cell-lit shape's material setup (the GL calls of setupProgramCE1 up
 * to the scene globals) is recorded the first time it runs in a frame and replayed in the frame's later passes,
 * so the obscurance pass pays the name lookups and the main pass only the GL calls. WW_DRAW_PACKET_RED=1 runs the
 * setup every time; WW_DRAW_PACKET_CHECK=1 replays, runs the setup anyway and counts any GL state that differs. */

#include "gl/glcontext.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

class Shape;
class TexCache;

namespace WwDp
{
enum Kind : std::uint8_t
{
	U1i, U1f, U2f, U3f, U4f, U1iv, U1fv, U4fv, UM3, UM4,
	ActiveTex, BindTex, TexParamNeeded, TexParamI, Enable, Disable,
	CullFace, BlendSep, PolyOffset, DepthFunc, DepthMask, PolyMode, Live,
	BlTex	// lane BINDLESS2: a = the map's slot, data = its 64-bit handle
};

struct Op
{
	Kind kind;
	std::int32_t a = 0, b = 0;	// a location / unit / target / cap; b a count / id
	std::uint32_t off = 0;	// into Packet::data
};

struct Packet
{
	const void * prog = nullptr;
	std::uint32_t epoch = 0;
	int texunit = 0;
	bool ready = false;
	std::vector<Op> ops;
	std::vector<std::uint32_t> data;
};

//! non-null while a packet records (the setters below add to it)
extern Packet * rec;

int mode();	// 0 = the red (no packets), 1 = on, 2 = the check
void frame();	// once per frame: older packets are void, the counters line is printed when it changes
void invalidate();	// a texture was deleted: every packet is void
void abortRecording();	// something the replay cannot repeat ran (a texture load)

//! the packet slot of this shape and program, or null when packets are off
Packet * slot( const Shape * shape, const void * prog );
bool valid( const Packet & p, const void * prog );
void begin( Packet & p, const void * prog );	// starts recording into p (cleared)
void end( bool ok, int texunit );	// stops recording; the packet is kept only if ok and nothing aborted it
void counted( bool replayed, int mismatches );
std::int32_t lastBound();	// the texture of the recording's last bind (0: none)

void put( Kind k, std::int32_t a, std::int32_t b, const void * v, int words );
inline void put( Kind k, std::int32_t a, std::int32_t b = 0 )
{
	put( k, a, b, nullptr, 0 );
}

//! replays p; live(ctx) runs where the shape's own transforms were set (they are never recorded)
void replay( const Packet & p, NifSkopeOpenGLContext::GLFunctions * f, TexCache * tc, void ( *live )( void * ), void * ctx );
//! the two recordings of one shape list the same calls with the same values
bool sameOps( const Packet & a, const Packet & b );
//! lane BINDLESSMDI1: the shape's packet slot if it has one (no insert), or null
const Packet * peek( const Shape * shape );
//! two shapes' packets set the same state but for their bindless map handles
bool sameButHandles( const Packet & a, const Packet & b );

//! the fixed-function state the setup sets, for the check
struct FixedSnap
{
	std::vector<std::int32_t> v;
};
void snapFixed( NifSkopeOpenGLContext::GLFunctions * f, FixedSnap & o );
}

//! records a GL call (its executed form stays beside it) while a packet records
#define WWDP( ... ) do { if ( WwDp::rec ) WwDp::put( __VA_ARGS__ ); } while ( 0 )

#endif
