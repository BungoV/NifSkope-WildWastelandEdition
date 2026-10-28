#ifndef CELLUNIFORM_H
#define CELLUNIFORM_H

/* lane CELLUNI1: the cell lighting uniforms (wwCellLightsUniforms, wwCellClusterUniforms) without driver round trips.
 * A per-program value cache skips an upload that would write what the location already holds; the dedicated units
 * bind through EXT_direct_state_access, so the active unit is never asked for. WW_CELL_UNI_RED=1: the old path.
 * Lane CELLUNI2: the per-draw setters (AO, SSR, SSGI, reflection probes) and the scene globals (fog, sun) use it too.
 * Lane CELLUNI3: the CE1 tail scalars, the draw's blend (a note from its setup) and the sky / air / far units.
 * Lane CELLUNI4: the frame end's passes (TAA, bloom, fog bloom) take the state cellhdr's End read once (KnownGl). */

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

//! lane CELLUNI2: who opened the innermost scope (the telemetry splits the counts by it)
enum Site { Lights, Draw, Globals, Tail, SiteCount };

struct State
{
	int depth = 0;	// inside a cached cell scope (never with the red)
	int inCall = 0;	// inside a cell scope, red or not (the counters)
	int site = Lights;
	bool red = false;
	Counts frame, last;	// all sites
	Counts sites[SiteCount], sitesLast[SiteCount];
	std::uint64_t frameNo = 0;
	// the viewport of an armed setup-memo pass: the memo re-arms when a layer changes the viewport, so one per epoch
	std::uint64_t vpEpoch = 0, vpHave = 0, vpStale = 0;
	bool vpCheck = false;
	int vp[4] = {};
	std::uint64_t blendStale = 0;	// lane CELLUNI3: WW_SETUP_MEMO_CHECK=1 found the blend note wrong
	std::uint64_t knownStale = 0;	// lane CELLUNI4: WW_SETUP_MEMO_CHECK=1 found the frame end's known state wrong
};
inline State & state()
{
	static State s;
	return s;
}

inline void bump( std::uint64_t Counts::*m )
{
	State & st = state();
	st.frame.*m += 1;
	st.sites[st.site].*m += 1;
}

struct Scope
{
	bool cached;
	int prevSite;
	explicit Scope( int site = Lights ) : cached( !state().red )
	{
		State & st = state();
		prevSite = st.site;
		st.site = site;
		if ( !st.inCall++ )
			st.frame.calls++;
		st.sites[site].calls++;
		if ( cached )
			st.depth++;
	}
	~Scope()
	{
		State & st = state();
		st.inCall--;
		st.site = prevSite;
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
			bump( &Counts::uploads );
		if ( l >= 0 && size_t( l ) < c.s.size() )
			c.s[size_t( l )].kind = None;
		return true;
	}
	if ( l < 0 ) {	// glUniform* at -1 writes nothing
		bump( &Counts::unused );
		return false;
	}
	if ( size_t( l ) >= c.s.size() )
		c.s.resize( size_t( l ) + 1 );
	Slot & sl = c.s[size_t( l )];
	std::uint32_t b[4] = {};
	std::memcpy( b, v, size_t( words ) * 4 );
	if ( !rec && sl.kind == k && std::memcmp( sl.v, b, sizeof b ) == 0 ) {	// a recording packet must see every call
		bump( &Counts::kept );
		return false;
	}
	sl.kind = k;
	std::memcpy( sl.v, b, sizeof b );
	bump( &Counts::uploads );
	return true;
}

//! an array or matrix write (never cached): its locations are unknown from now on
inline void wrote( Cache & c, int l, int n )
{
	State & st = state();
	if ( st.inCall )
		bump( &Counts::uploads );
	c.forget( l, n < 1 ? 1 : n );
}

//! lane CELLUNI2: a driver query made inside a scope (a blend or unit ask outside Units)
inline void queried()
{
	if ( state().inCall )
		bump( &Counts::queries );
}

/* lane CELLUNI3: GL_BLEND (and the blend's RGB destination factor) as the draw's setup last wrote it. setupProgram
 * forgets it on entry and every blend write between there and the census notes itself; -1 = not written (ask GL). */
struct BlendNote
{
	int on = -1;
	int dst = -1;
};
inline BlendNote & blendNote()
{
	static BlendNote b;
	return b;
}
inline void noteBlend( bool on ) { blendNote().on = on ? 1 : 0; }
inline void noteBlendDst( int dst ) { blendNote().dst = dst; }
inline void forgetBlend() { blendNote() = BlendNote(); }

//! once per frame: true when the last frame's counters should print
inline bool frameTick( std::uint64_t f, std::uint64_t every )
{
	State & st = state();
	if ( f == st.frameNo )
		return false;
	st.last = st.frame;
	st.frame = Counts();
	for ( int i = 0; i < SiteCount; i++ ) {
		st.sitesLast[i] = st.sites[i];
		st.sites[i] = Counts();
	}
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
		bump( &Counts::queries );
	}
	//! lane CELLUNI2: the unit a per-draw cache already holds (no ask; the old path took it from there too)
	void seed( int active ) { known = active; }
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
	bump( &Counts::queries );
	if ( !st.red && st.vpEpoch ) {
		st.vpHave = st.vpEpoch;
		std::memcpy( st.vp, vp, sizeof st.vp );
	}
}

/* lane CELLUNI4: what a frame-end pass saves for its restore, as cellhdr's End read it once. Every pass between there
 * and the tone quads restores what it changed, so the next one takes this instead of asking. */
struct KnownGl
{
	int draw = 0, read = 0, active = 0x84C0, vp[4] = { 0, 0, 1, 1 };
	float clear[4] = {};
	unsigned char colorMask[4] = { 1, 1, 1, 1 }, depthMask = 1;
	bool depth = false, blend = false, cull = false, stencil = false, scissor = false;
	int sFunc = 0x0207, sRef = 0, sMask = 0xFF, sWrite = 0xFF, sFail = 0x1E00, sZFail = 0x1E00, sPass = 0x1E00;
};
inline bool same( const KnownGl & a, const KnownGl & b )
{
	return a.draw == b.draw && a.read == b.read && a.active == b.active && std::memcmp( a.vp, b.vp, sizeof a.vp ) == 0
		&& std::memcmp( a.clear, b.clear, sizeof a.clear ) == 0 && std::memcmp( a.colorMask, b.colorMask, 4 ) == 0
		&& a.depthMask == b.depthMask && a.depth == b.depth && a.blend == b.blend && a.cull == b.cull && a.stencil == b.stencil
		&& a.scissor == b.scissor && a.sFunc == b.sFunc && a.sRef == b.sRef && a.sMask == b.sMask && a.sWrite == b.sWrite
		&& a.sFail == b.sFail && a.sZFail == b.sZFail && a.sPass == b.sPass;
}
//! every field from the driver (the old path's asks; the check)
template <class F> void askAll( F * fn, KnownGl & k )
{
	fn->glGetIntegerv( 0x8CA6, &k.draw );	// GL_DRAW_FRAMEBUFFER_BINDING
	fn->glGetIntegerv( 0x8CAA, &k.read );	// GL_READ_FRAMEBUFFER_BINDING
	fn->glGetIntegerv( 0x84E0, &k.active );
	fn->glGetIntegerv( 0x0BA2, k.vp );
	fn->glGetFloatv( 0x0C22, k.clear );	// GL_COLOR_CLEAR_VALUE
	fn->glGetBooleanv( 0x0C23, k.colorMask );
	fn->glGetBooleanv( 0x0B72, &k.depthMask );
	k.depth = fn->glIsEnabled( 0x0B71 );
	k.blend = fn->glIsEnabled( 0x0BE2 );
	k.cull = fn->glIsEnabled( 0x0B44 );
	k.stencil = fn->glIsEnabled( 0x0B90 );
	k.scissor = fn->glIsEnabled( 0x0C11 );
	const unsigned st[7] = { 0x0B92, 0x0B97, 0x0B93, 0x0B98, 0x0B94, 0x0B95, 0x0B96 };	// func, ref, mask, write, ops
	int * to[7] = { &k.sFunc, &k.sRef, &k.sMask, &k.sWrite, &k.sFail, &k.sZFail, &k.sPass };
	for ( int i = 0; i < 7; i++ )
		fn->glGetIntegerv( st[i], to[i] );
}
//! set by cellhdr's End around the passes that may take it (null: they ask, as before)
inline const KnownGl *& knownSlot()
{
	static const KnownGl * k = nullptr;
	return k;
}
//! the known state, or null (ask). WW_SETUP_MEMO_CHECK=1 asks anyway, counts a wrong field and returns the driver's
template <class F> const KnownGl * known( F * fn )
{
	const KnownGl * k = knownSlot();
	if ( !k || !state().vpCheck )
		return k;
	static KnownGl real;
	askAll( fn, real );
	if ( !same( real, *k ) )
		state().knownStale++;
	return &real;
}
/* lane CELLUNI5: the active texture unit (GL_TEXTURE0 + n) a caller knows, or -1: ask. WW_SETUP_MEMO_CHECK=1 asks
 * anyway, counts a wrong one (knownStale) and returns the driver's. The callers pass -1 under WW_CELL_UNI_RED=1. */
template <class F> int activeUnit( F * fn, int knownActive )
{
	if ( knownActive >= 0 && !state().vpCheck )
		return knownActive;
	int real = 0;
	fn->glGetIntegerv( 0x84E0, &real );	// GL_ACTIVE_TEXTURE
	if ( knownActive >= 0 && real != knownActive )
		state().knownStale++;
	return real;
}
/* lane CELLUNI5: a pass's GL error drain and reads, kept as the temporal AA and the VRS build keep theirs (lane SYNC1's
 * regime): read until one clean frame on the same targets, then skipped; new targets or a failed frame read again.
 * WW_CELL_UNI_RED=1 reads every frame. */
struct ErrTrust
{
	bool trusted = false;
	bool check( bool red, bool remade ) const { return red || remade || !trusted; }
	void after( bool checked, bool ok )
	{
		if ( checked )
			trusted = ok;
	}
	void forget() { trusted = false; }
};
template <class F> void drainErrors( F * fn )
{
	while ( fn->glGetError() != 0 ) {
	}
}

//! lane CELLUNI4: a framebuffer's depth format, asked once per (context, framebuffer, size); another key asks again
struct DepthCfg
{
	const void * ctx = nullptr;
	int fbo = -1, w = 0, h = 0;
	int depthBits = 0, stencilBits = 0, compType = 0;
	bool trusted = false;	// a frame on this key raised no GL error: its error reads are skipped
	bool hit( const void * c, int f, int ww, int hh ) const { return fbo >= 0 && ctx == c && fbo == f && w == ww && h == hh; }
	void keep( const void * c, int f, int ww, int hh, int d, int s, int t )
	{
		ctx = c;
		fbo = f;
		w = ww;
		h = hh;
		depthBits = d;
		stencilBits = s;
		compType = t;
		trusted = false;
	}
	void forget()
	{
		fbo = -1;
		trusted = false;
	}
};

/*! lane FRAMESPEED1: an owner (a Renderer) whose GL context was made again (a context loss) shows a new serial: what was
 *  cached for the old context (DepthCfg: the same fbo id and size may now hold another format) must be forgotten */
struct CtxWatch
{
	std::vector<std::pair<const void *, std::uint64_t>> seen;
	bool changed( const void * owner, std::uint64_t serial )
	{
		for ( auto & p : seen ) {
			if ( p.first == owner ) {
				const bool c = p.second != serial;
				p.second = serial;
				return c;
			}
		}
		seen.emplace_back( owner, serial );
		return false;
	}
};
}

//! the DSA entry points of the current context (null members: the extension is missing)
const WwCellUni::Dsa * wwCellUniDsa();
//! WW_CELL_UNI_RED=1 (live): the old path, a query per section and every upload
bool wwCellUniRed();
//! lane CELLUNI2: the per-frame tick and the `cell uni:` line (any scope's first call in a frame)
void wwCellUniFrame();

#endif
