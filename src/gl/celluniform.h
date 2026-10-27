#ifndef CELLUNIFORM_H
#define CELLUNIFORM_H

/* lane CELLUNI1: the cell lighting uniforms (wwCellLightsUniforms, wwCellClusterUniforms) without driver round trips.
 * A per-program value cache skips an upload that would write what the location already holds; the dedicated units
 * bind through EXT_direct_state_access, so the active unit is never asked for. WW_CELL_UNI_RED=1: the old path. */

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#if defined( _WIN32 ) && !defined( _WIN64 )
#define WWCU_API __stdcall
#else
#define WWCU_API
#endif

namespace WwCellUni
{
enum Kind : std::uint8_t { None, K1i, K1f, K2f, K3f, K4f };

struct Slot
{
	Kind kind = None;
	std::uint32_t v[4] = {};
};

//! one per Program: what each location holds, as far as the cell scope knows (None = unknown)
struct Cache
{
	std::vector<Slot> s;
	void reset() { s.clear(); }
	void forget( int l, int n )
	{
		for ( int i = 0; i < n; i++ )
			if ( l + i >= 0 && size_t( l + i ) < s.size() )
				s[size_t( l + i )].kind = None;
	}
};

struct Counts
{
	std::uint64_t calls = 0, uploads = 0, kept = 0, unused = 0, queries = 0;
};

struct State
{
	int depth = 0;	// inside a cached cell scope (never with the red)
	int inCall = 0;	// inside a cell scope, red or not (the counters)
	bool red = false;
	Counts frame, last;
	std::uint64_t frameNo = 0;
	// the viewport of an armed setup-memo pass: the memo re-arms when a layer changes the viewport, so one per epoch
	std::uint64_t vpEpoch = 0, vpHave = 0, vpStale = 0;
	bool vpCheck = false;
	int vp[4] = {};
};
inline State & state()
{
	static State s;
	return s;
}

struct Scope
{
	bool cached;
	Scope() : cached( !state().red )
	{
		State & st = state();
		if ( !st.inCall++ )
			st.frame.calls++;
		if ( cached )
			st.depth++;
	}
	~Scope()
	{
		State & st = state();
		st.inCall--;
		if ( cached )
			st.depth--;
	}
	Scope( const Scope & ) = delete;
	Scope & operator=( const Scope & ) = delete;
};

//! a scalar or vector write: false = the location already holds these bits, the caller skips the GL call
inline bool want( Cache & c, int l, Kind k, const void * v, int words, bool rec )
{
	State & st = state();
	if ( !st.depth ) {	// a write the scope does not see: the slot is unknown from now on
		if ( st.inCall )
			st.frame.uploads++;
		if ( l >= 0 && size_t( l ) < c.s.size() )
			c.s[size_t( l )].kind = None;
		return true;
	}
	if ( l < 0 ) {	// glUniform* at -1 writes nothing
		st.frame.unused++;
		return false;
	}
	if ( size_t( l ) >= c.s.size() )
		c.s.resize( size_t( l ) + 1 );
	Slot & sl = c.s[size_t( l )];
	std::uint32_t b[4] = {};
	std::memcpy( b, v, size_t( words ) * 4 );
	if ( !rec && sl.kind == k && std::memcmp( sl.v, b, sizeof b ) == 0 ) {	// a recording packet must see every call
		st.frame.kept++;
		return false;
	}
	sl.kind = k;
	std::memcpy( sl.v, b, sizeof b );
	st.frame.uploads++;
	return true;
}

//! an array or matrix write (never cached): its locations are unknown from now on
inline void wrote( Cache & c, int l, int n )
{
	State & st = state();
	if ( st.inCall )
		st.frame.uploads++;
	c.forget( l, n < 1 ? 1 : n );
}

//! once per frame: true when the last frame's counters should print
inline bool frameTick( std::uint64_t f, std::uint64_t every )
{
	State & st = state();
	if ( f == st.frameNo )
		return false;
	st.last = st.frame;
	st.frame = Counts();
	st.frameNo = f;
	return every && f % every == 0;
}

using BindMultiFn = void( WWCU_API * )( unsigned unit, unsigned target, unsigned tex );
using TexBufferFn = void( WWCU_API * )( unsigned tex, unsigned target, unsigned format, unsigned buf );
struct Dsa
{
	BindMultiFn bindMulti = nullptr;
	TexBufferFn texBuffer = nullptr;
};

/* binds on the dedicated units. With DSA nothing moves the active unit, so nothing asks for it; an upload that must
 * move it asks once (lazily) and done() puts it back. Without DSA (or the red) each section asks first, as before. */
template <class F> struct Units
{
	F * fn;
	const Dsa * dsa;
	int known = -1, restore = -1;
	bool moved = false;
	Units( F * f, const Dsa * d ) : fn( f ), dsa( d && d->bindMulti && d->texBuffer && !state().red ? d : nullptr ) {}
	void ask()
	{
		int a = 0;
		fn->glGetIntegerv( 0x84E0, &a );	// GL_ACTIVE_TEXTURE
		known = a;
		state().frame.queries++;
	}
	void section()
	{
		if ( !dsa )
			ask();
	}
	//! moves the active unit (an upload, or a bind without DSA); done() restores it
	void raw( int unit )
	{
		if ( !moved ) {
			if ( known < 0 )
				ask();
			restore = known;
			moved = true;
		}
		fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
		known = 0x84C0 + unit;
	}
	//! moves it for good (the old code left it there too)
	void leave( int unit )
	{
		fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
		known = 0x84C0 + unit;
		moved = false;
	}
	void bind( int unit, unsigned target, unsigned tex )
	{
		if ( dsa ) {
			dsa->bindMulti( unsigned( 0x84C0 + unit ), target, tex );
		} else {
			raw( unit );
			fn->glBindTexture( target, tex );
		}
	}
	//! attaches buf to tex (bound on unit by bind() just before)
	void texBuffer( unsigned tex, unsigned format, unsigned buf )
	{
		if ( dsa )
			dsa->texBuffer( tex, 0x8C2A, format, buf );	// GL_TEXTURE_BUFFER
		else
			fn->glTexBuffer( 0x8C2A, format, buf );
	}
	void done()
	{
		if ( moved ) {
			fn->glActiveTexture( unsigned( restore ) );
			known = restore;
			moved = false;
		}
	}
};

//! the viewport: once per armed setup-memo epoch (WW_SETUP_MEMO_CHECK=1 asks anyway and counts a stale one)
template <class F> void viewport( F * fn, int vp[4] )
{
	State & st = state();
	if ( !st.red && st.vpEpoch && st.vpHave == st.vpEpoch ) {
		if ( st.vpCheck ) {
			int real[4] = {};
			fn->glGetIntegerv( 0x0BA2, real );
			if ( std::memcmp( real, st.vp, sizeof real ) != 0 ) {
				st.vpStale++;
				std::memcpy( st.vp, real, sizeof real );
			}
		}
		std::memcpy( vp, st.vp, sizeof st.vp );
		return;
	}
	fn->glGetIntegerv( 0x0BA2, vp );	// GL_VIEWPORT
	st.frame.queries++;
	if ( !st.red && st.vpEpoch ) {
		st.vpHave = st.vpEpoch;
		std::memcpy( st.vp, vp, sizeof st.vp );
	}
}
}

//! the DSA entry points of the current context (null members: the extension is missing)
const WwCellUni::Dsa * wwCellUniDsa();
//! WW_CELL_UNI_RED=1 (live): the old path, a query per section and every upload
bool wwCellUniRed();

#endif
