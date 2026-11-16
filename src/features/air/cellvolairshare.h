#pragma once

// Lane VKAIR11 (2026-11-16): cell_volair's stage-1 inputs made on the present device by their owner (cellvolair.cpp), so
// the Vulkan |3 reader samples them in place: the strata history (hist[2]), the lamp volume (lvNow[2], lvHist[2]), the drift
// noise, the terrain air grid and stage 4's blue noise. Keys are VaVkKeys slots (cellvolairkeys.h).
// While vaStage1Reader() is false (the reader has not landed) no site asks: every site runs GL's own calls as before.
// Shared storage is immutable: the owner fills it with glTexSubImage3D, and sets the sampler state here (sharedTarget
// leaves NEAREST). A re-make (an ask flip, a size change, a present shutdown that dropped the name) returns 1: the caller
// resets its fill memo and fills again. A refused spec is remembered and not asked again until the spec changes.
// Gate: tests/vkframe/airshare.sh (GL on the shared copy vs GL on a plain copy, EXACT, with reds).

#include <cstdint>
#include <string>

#include "features/vkframe/vkpresent.h"

//! the |3 reader on Vulkan exists (lane VKAIR14: vkairtemporal.inc, gate tests/vkframe/airtemporalview.sh)
inline bool vaStage1Reader()
{
	return true;
}

//! stage 4's reader (the lamp volume's walk) on Vulkan exists (lane VKAIR16: vkairlamp.inc, gate
//! tests/vkframe/airlampview.sh): its own inputs (the blue noise, lvHist) are shared while stage 1's are
inline bool vaStage4Reader()
{
	return true;
}

//! one input's storage: type 1 a 3D texture, 2 a 2D array (sharedTarget's types); fmt GL's internal format, pf / ty the
//! upload format and type of a GL allocation; linear / repeat the sampler state the reader expects
struct VaShareSpec
{
	int type = 1;
	unsigned fmt = 0, pf = 0, ty = 0;
	int w = 0, h = 0, d = 0;
	bool linear = false, repeat = false;
	std::uint64_t hash() const
	{
		std::uint64_t k = 1469598103934665603ull;
		for ( std::uint64_t v : { std::uint64_t( type ), std::uint64_t( fmt ), std::uint64_t( w ), std::uint64_t( h ), std::uint64_t( d ),
				  std::uint64_t( linear ), std::uint64_t( repeat ) } )
			k = ( k ^ v ) * 1099511628211ull;
		return k;
	}
};

//! what the owner's name holds now
struct VaShareState
{
	bool shared = false;
	std::uint64_t spec = 0;     //!< the spec the name was made for (0: none / made by GL's old calls)
	std::uint64_t refused = 0;  //!< a spec the present device refused (not asked again)
	std::string why;            //!< its reason
};

//! gate-only reds (the app never sets them): 1 the shared path skips the sampler state
inline int & vaShareRed()
{
	static int r = 0;
	return r;
}

//! drop a shared name (the present device's: dropped there, never glDeleteTextures); returns true when it did
inline bool vaShareRelease( std::uintptr_t key, unsigned & tex, VaShareState & st )
{
	if ( !st.shared )
		return false;
	if ( WwVkPresent::sharedTargetIs( key, tex ) )
		WwVkPresent::sharedTargetDrop( key );
	tex = 0;
	st.shared = false;
	st.spec = 0;
	return true;
}

/* tex holds spec, shared when want (and the spec was not refused). Returns 0 when tex already did (nothing called but a
 * lookup), 1 when it was made (empty storage: the caller fills). Leaves the target's binding of the active unit at 0. */
template <class Fn>
int vaShareEnsure( Fn * fn, std::uintptr_t key, const VaShareSpec & sp, bool want, unsigned & tex, VaShareState & st )
{
	const std::uint64_t h = sp.hash();
	const bool wantS = want && st.refused != h;
	if ( tex && st.spec == h && st.shared == wantS && ( !st.shared || WwVkPresent::sharedTargetIs( key, tex ) ) )
		return 0;
	vaShareRelease( key, tex, st );
	const unsigned target = sp.type == 2 ? 0x8C1Au : 0x806Fu;  // GL_TEXTURE_2D_ARRAY : GL_TEXTURE_3D
	if ( wantS ) {
		if ( tex ) {
			fn->glDeleteTextures( 1, &tex );
			tex = 0;
		}
		unsigned t = 0;
		std::string why;
		if ( WwVkPresent::sharedTarget( key, sp.type, sp.w, sp.h, sp.d, sp.fmt, t, why ) ) {
			tex = t;
			st.shared = true;
		} else {
			st.refused = h;
			st.why = why;
		}
	}
	if ( !st.shared ) {
		if ( !tex )
			fn->glGenTextures( 1, &tex );
		fn->glBindTexture( target, tex );
		fn->glTexImage3D( target, 0, int( sp.fmt ), sp.w, sp.h, sp.d, 0, sp.pf, sp.ty, nullptr );
	} else {
		fn->glBindTexture( target, tex );
	}
	if ( !( st.shared && ( vaShareRed() & 1 ) ) ) {
		const int filter = sp.linear ? 0x2601 : 0x2600;          // GL_LINEAR : GL_NEAREST
		const int wrap = sp.repeat ? 0x2901 : 0x812F;            // GL_REPEAT : GL_CLAMP_TO_EDGE
		fn->glTexParameteri( target, 0x2801, filter );          // GL_TEXTURE_MIN_FILTER
		fn->glTexParameteri( target, 0x2800, filter );          // GL_TEXTURE_MAG_FILTER
		fn->glTexParameteri( target, 0x2802, wrap );            // GL_TEXTURE_WRAP_S
		fn->glTexParameteri( target, 0x2803, wrap );            // GL_TEXTURE_WRAP_T
		fn->glTexParameteri( target, 0x8072, wrap );            // GL_TEXTURE_WRAP_R
		fn->glTexParameteri( target, 0x813D, 0 );               // GL_TEXTURE_MAX_LEVEL
	}
	fn->glBindTexture( target, 0 );
	st.spec = h;
	return 1;
}
