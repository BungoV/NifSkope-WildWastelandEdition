/* tests/celluni6 (lane CELLUNI6): the CELLUNI5 leftovers on the kit (src/gl/celluniform.h), run against a fake GL state
 * (no driver: the plain C++ the call sites hand their asks to). Each section models one item's call sites as the source
 * runs them, beside the red (the asks before this lane), over a run of frames with the changes that must make the memo
 * forget (a resize, a pass that moves the state, a context made again, a GL error). Green and red must give the same
 * answers frame for frame and green must ask the fake driver less; doctored memos (a note kept too long, a key without
 * the context, a trust that never reads) must give a wrong answer, so the comparison can fail.
 * The red builds (-DCELLUNI6_RED_<ITEM>, one per item) run that item's green as main did: its own "asks less" or
 * "sees" checks must fail and nothing else. run.sh builds green and every red.
 * Usage: celluni6   exit 0 = all ok */
#include "gl/celluniform.h"

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

enum : unsigned {
	kViewport = 0x0BA2, kDrawFb = 0x8CA6, kReadFb = 0x8CAA, kBlend = 0x0BE2, kNoError = 0, kInvalidOp = 0x0502,
	kAttach = 0x8212	// the three attachment asks of a depth format (counted under one name)
};

// the GL state the modelled code reads and writes; every ask counted by its name
struct FakeGl
{
	int vp[4] = { 0, 0, 64, 32 };
	int draw = 0, read = 0;
	bool blend = false;
	std::vector<unsigned> errors;	// the driver's error queue
	std::map<int, int> fbFormat;	// framebuffer -> its depth format (1 = D24S8, 2 = D32FS8, ...)
	std::map<unsigned, long long> asks;
	long long total = 0;
	[[gnu::noinline]] void glGetIntegerv( unsigned p, int * v )	// out of line: g++ -Warray-bounds would follow the viewport copy into a scalar ask
	{
		asks[p]++;
		total++;
		if ( p == kViewport )
			std::memcpy( v, vp, sizeof vp );
		else if ( p == kDrawFb )
			*v = draw;
		else if ( p == kReadFb )
			*v = read;
		else
			*v = 0;
	}
	bool glIsEnabled( unsigned c )
	{
		asks[c]++;
		total++;
		return c == kBlend ? blend : false;
	}
	unsigned glGetError()
	{
		asks[0]++;
		total++;
		if ( errors.empty() )
			return kNoError;
		const unsigned e = errors.front();
		errors.erase( errors.begin() );
		return e;
	}
	//! the depth format of a framebuffer: the three glGetFramebufferAttachmentParameteriv asks
	int attachFormat( int fb )
	{
		asks[kAttach] += 3;
		total += 3;
		return fbFormat[fb];
	}
	void glViewport( int x, int y, int w, int h )
	{
		vp[0] = x, vp[1] = y, vp[2] = w, vp[3] = h;
	}
};

static void resetKit()
{
	WwCellUni::state() = WwCellUni::State();
	WwCellUni::jitterVp() = WwCellUni::VpNote();
}

// ---------------------------------------------------------------- red names
static void redNames()
{
	using WwCellUni::redHas;
	check( redHas( "jitter", "jitter" ) && !redHas( "jitter", "nonlinear" ), "red: one name" );
	check( redHas( "jitter,bsshape", "bsshape" ) && redHas( " jitter , bsshape ", "jitter" ), "red: a list" );
	check( !redHas( "jitterx", "jitter" ) && !redHas( "jit", "jitter" ) && !redHas( "", "jitter" ) && !redHas( nullptr, "jitter" ),
		"red: whole words only" );
	check( redHas( "all", "glerror" ), "red: all" );
}

// ---------------------------------------------------------------- 1. jitter
/* glview paintGL: Qt sets the frame's viewport, the jitter asks it (gametaa.cpp wwGameTaaJitterProjection), then the
 * global uniforms' upload asks it (glcontext.cpp setGlobalUniforms). Later in the same paint another upload runs under
 * another viewport (a lookdev capture). TAA off in paint 6 (no jitter); the window resized at paint 9. */
enum class VpMode { Green, Red, DocKeep, DocUnkeyed };
struct VpRun
{
	std::vector<int> got;
	long long asks = 0;
	std::uint64_t stale = 0;
};
static VpRun vpRun( VpMode m, bool check, bool wrongNote )
{
	resetKit();
	FakeGl gl;
	VpRun out;
	WwCellUni::VpNote doc;	// the doctored notes: kept after a take / taken in any paint
	for ( std::uint64_t f = 1; f <= 12; f++ ) {
		const int w = f < 9 ? 64 : 80, h = f < 9 ? 32 : 48;
		gl.glViewport( 0, 0, w, h );
		const bool jitterOn = f != 6;
		bool green = m != VpMode::Red;
#ifdef CELLUNI6_RED_JITTER
		green = false;
#endif
		if ( jitterOn ) {
			int vp[4];
			gl.glGetIntegerv( kViewport, vp );
			if ( wrongNote )
				vp[2] = 1;
			if ( green )
				( m == VpMode::Green ? WwCellUni::jitterVp() : doc ).put( f, vp );
		}
		for ( int up = 0; up < 2; up++ ) {
			if ( up == 1 )
				gl.glViewport( 0, 0, 16, 16 );	// a capture's own viewport, then its upload
			int v[4] = {};
			if ( m == VpMode::DocKeep || m == VpMode::DocUnkeyed ) {
				if ( doc.frame && ( m == VpMode::DocUnkeyed || doc.frame == f ) )
					std::memcpy( v, doc.vp, sizeof v );	// never forgotten: every upload takes it
				else
					gl.glGetIntegerv( kViewport, v );
			} else {
				WwCellUni::viewportFrom( &gl, WwCellUni::jitterVp(), f, v, check );
			}
			out.got.insert( out.got.end(), v, v + 4 );
		}
		gl.glViewport( 0, 0, w, h );
	}
	out.asks = gl.asks[kViewport];
	out.stale = WwCellUni::state().vpStale;
	return out;
}
static void jitter()
{
	const VpRun g = vpRun( VpMode::Green, false, false ), r = vpRun( VpMode::Red, false, false );
	check( g.got == r.got, "jitter: every upload's viewport as the red's (12 paints, a resize, a capture, TAA off once)" );
	std::printf( "     jitter: viewport asks green %lld red %lld\n", g.asks, r.asks );
	check( g.asks == r.asks - 11, "jitter: green asks the viewport once less in each paint the jitter ran (11 of 12)" );
	check( vpRun( VpMode::DocKeep, false, false ).got != r.got, "jitter: doctored, a note kept after its take differs" );
	check( vpRun( VpMode::DocUnkeyed, false, false ).got != r.got, "jitter: doctored, a note from another paint differs" );
	const VpRun c = vpRun( VpMode::Green, true, false );
	check( c.got == r.got && c.stale == 0, "jitter: WW_SETUP_MEMO_CHECK=1, a right note: same viewports, vpstale 0" );
	const VpRun w = vpRun( VpMode::Green, true, true );
	check( w.got == r.got && w.stale == 11, "jitter: WW_SETUP_MEMO_CHECK=1, a wrong note: the driver's viewport, vpstale 11" );
}

// ---------------------------------------------------------------- 2. nonlinear
/* gametaa.cpp taaResolveImpl's depth part: the depth format (cache or the three asks), the error drain before and the
 * blit's error read after (until a trusted frame), the blit itself (an error when the format the TAA made its depth
 * target for is not the frame's). Frames 1-5 HDR on (linear, the tone map's known state), 6-15 HDR off on the window's
 * framebuffer, 16-20 a harness framebuffer (id 7) that is made again with another format at frame 18 (same id and
 * size), 21-30 the window again, resized at frame 25. */
struct TaaOut
{
	std::vector<int> fmt, landed;
	long long asks = 0;
};
static TaaOut taaRun( bool redNl, bool docAnyFb )
{
	FakeGl gl;
	gl.fbFormat[0] = 1;
	gl.fbFormat[7] = 1;
	WwCellUni::DepthCfg cfg;
	const int owner = 0;
	TaaOut out;
	for ( int f = 1; f <= 30; f++ ) {
		const bool linear = f <= 5, known = linear;
		const int fb = ( f >= 16 && f <= 20 ) ? 7 : 0, W = f < 25 ? 64 : 80, H = 32;
		if ( f == 18 )
			gl.fbFormat[7] = 2;
		if ( f == 9 )
			gl.errors.push_back( kInvalidOp );	// an earlier pass's error, left in the queue
		const bool useCfg = docAnyFb ? ( known || !linear ) : WwCellUni::depthCfgServes( known, linear, fb, redNl );
		const bool cfgHit = useCfg && cfg.hit( &owner, fb, W, H );
		const bool checkErr = !cfgHit || !cfg.trusted;
		if ( checkErr )
			while ( gl.glGetError() != kNoError ) {
			}
		int fmt = 0;
		if ( cfgHit ) {
			fmt = cfg.depthBits;
		} else {
			fmt = gl.attachFormat( fb );
			if ( useCfg )
				cfg.keep( &owner, fb, W, H, fmt, 8, 0 );
		}
		if ( fmt != gl.fbFormat[fb] )
			gl.errors.push_back( kInvalidOp );	// the depth blit between two formats
		const unsigned blitErr = checkErr ? gl.glGetError() : kNoError;
		if ( useCfg && blitErr != kNoError )
			cfg.forget();
		else if ( useCfg && checkErr && cfg.hit( &owner, fb, W, H ) )
			cfg.trusted = true;
		out.fmt.push_back( fmt );
		out.landed.push_back( fmt == gl.fbFormat[fb] ? 1 : 0 );
	}
	out.asks = gl.total;
	return out;
}
static void nonlinear()
{
	bool redGreen = false;
#ifdef CELLUNI6_RED_NONLINEAR
	redGreen = true;
#endif
	const TaaOut g = taaRun( redGreen, false ), r = taaRun( true, false );
	check( g.fmt == r.fmt && g.landed == r.landed, "nonlinear: every frame's depth format and blit as the red's (30 frames)" );
	check( std::count( r.landed.begin(), r.landed.end(), 0 ) == 0, "nonlinear: the red's blits all land (the model is sound)" );
	std::printf( "     nonlinear: asks green %lld red %lld\n", g.asks, r.asks );
	check( g.asks < r.asks - 40, "nonlinear: green asks less on the HDR-off frames (the format and the error reads, once trusted)" );
	const TaaOut d = taaRun( false, true );
	check( d.landed != r.landed, "nonlinear: doctored, the cache on a harness framebuffer misses its new format (frame 18)" );
}

// ---------------------------------------------------------------- 3. hdrdepth
/* cellhdr.cpp wwCellHdrDepthMax and its readers in one HDR frame, in glview's order: the far cards take it and then
 * draw (they write depth), the volume clouds take it, the underwater pass takes it (frames 4-8: the camera under water).
 * The depth max is a texture made from the frame's depth as it is at the call: the model gives each make the depth's
 * version, and each reader what it read. Frames 10-12 have no far cards. */
struct DmOut
{
	std::vector<long long> seen;	// per reader: the depth version its texture holds
	long long makes = 0;
	std::uint64_t stale = 0;
};
enum class DmMode { Green, Red, DocNoWrote, DocNoFrame };
static DmOut dmRun( DmMode m, bool check )
{
	resetKit();
	WwCellUni::FrameOnce once;
	const int owner = 0;
	DmOut out;
	long long depth = 0, texHolds = -1;
	std::uint64_t serial = 0;
	bool red = m == DmMode::Red;
#ifdef CELLUNI6_RED_HDRDEPTH
	red = true;
#endif
	auto make = [&]() {
		out.makes++;
		texHolds = depth;
		return 5u;	// the texture's name: the same every frame
	};
	auto take = [&]() -> unsigned {	// the wrapper, as cellhdr.cpp has it
		const std::uint64_t f = m == DmMode::DocNoFrame ? 1 : serial;
		unsigned tex = 0;
		if ( red )
			return make();
		if ( !once.take( &owner, f, 64ull << 32 | 32, tex ) ) {
			tex = make();
			once.keep( &owner, f, 64ull << 32 | 32, tex );
			return tex;
		}
		if ( check ) {
			const long long kept = texHolds;
			const unsigned made = make();
			if ( made != tex || texHolds != kept )
				WwCellUni::state().knownStale++;
			return made;
		}
		return tex;
	};
	for ( int f = 1; f <= 12; f++ ) {
		serial++;	// Begin
		depth += 100;	// the scene draws
		if ( f < 10 ) {
			take();
			out.seen.push_back( texHolds );
			if ( m != DmMode::DocNoWrote )
				once.dirty();	// wwCellHdrDepthMaxWrote
			depth += 1;	// the cards' depth
		}
		take();	// the volume clouds
		out.seen.push_back( texHolds );
		if ( f >= 4 && f <= 8 ) {
			take();	// under water
			out.seen.push_back( texHolds );
		}
	}
	out.stale = WwCellUni::state().knownStale;
	return out;
}
static void hdrdepth()
{
	const DmOut g = dmRun( DmMode::Green, false ), r = dmRun( DmMode::Red, false );
	check( g.seen == r.seen, "hdrdepth: every reader's depth max as the red's (12 frames, cards in 9, under water in 5)" );
	std::printf( "     hdrdepth: makes green %lld red %lld\n", g.makes, r.makes );
	check( g.makes == r.makes - 5, "hdrdepth: green makes it once less in each frame with two readers after the cards" );
	check( dmRun( DmMode::DocNoWrote, false ).seen != r.seen, "hdrdepth: doctored, the cards' depth writes not said: differs" );
	check( dmRun( DmMode::DocNoFrame, false ).seen != r.seen, "hdrdepth: doctored, kept across frames: differs" );
	const DmOut c = dmRun( DmMode::Green, true );
	check( c.seen == r.seen && c.stale == 0, "hdrdepth: WW_SETUP_MEMO_CHECK=1: same textures, knownstale 0" );
}

// ---------------------------------------------------------------- 4. hdrbegin
/* glview's paint up to cellhdr.cpp wwCellHdrBegin: the paint's target (the window, framebuffer 0, or a harness shot's
 * framebuffer 9 in paints 5 and 6 at another size), the obscurance pass (asks the three, binds its own, puts them back;
 * off in paints 3 and 5), the other cell passes (bind their own, put back), then Begin's three asks. */
enum class TgMode { Green, Red, DocKeep, DocLeakRead };
struct TgOut
{
	std::vector<int> begin;	// Begin's draw, read, viewport per paint
	long long asks = 0;
	std::uint64_t stale = 0;
};
static TgOut tgRun( TgMode m, bool check )
{
	resetKit();
	WwCellUni::frameTarget() = WwCellUni::TargetNote();
	FakeGl gl;
	TgOut out;
	bool red = m == TgMode::Red;
#ifdef CELLUNI6_RED_HDRBEGIN
	red = true;
#endif
	WwCellUni::TargetNote kept;	// DocKeep: a note never forgotten
	for ( std::uint64_t f = 1; f <= 10; f++ ) {
		const bool shot = f == 5 || f == 6;
		gl.draw = gl.read = shot ? 9 : 0;
		gl.glViewport( 0, 0, shot ? 128 : 64, shot ? 128 : 32 );
		if ( f != 3 && f != 5 ) {	// the obscurance pass
			int d = 0, rd = 0, vp[4];
			gl.glGetIntegerv( kDrawFb, &d );
			gl.glGetIntegerv( kReadFb, &rd );
			gl.glGetIntegerv( kViewport, vp );
			if ( !red )
				( m == TgMode::DocKeep ? kept : WwCellUni::frameTarget() ).put( m == TgMode::DocKeep ? 1 : f, d, rd, vp );
			gl.draw = gl.read = 40;
			gl.glViewport( 0, 0, 32, 16 );
			gl.draw = d, gl.read = rd;
			gl.glViewport( vp[0], vp[1], vp[2], vp[3] );
		}
		for ( int pass = 0; pass < 3; pass++ ) {	// SSR, SSGI, the air: their own targets, then the frame's back
			int d = 0, rd = 0, vp[4];
			gl.glGetIntegerv( kDrawFb, &d );
			gl.glGetIntegerv( kReadFb, &rd );
			gl.glGetIntegerv( kViewport, vp );
			gl.draw = gl.read = 50 + pass;
			gl.glViewport( 0, 0, 8, 8 );
			gl.draw = d, gl.read = ( m == TgMode::DocLeakRead && pass == 2 && f == 8 ) ? 52 : rd;
			gl.glViewport( vp[0], vp[1], vp[2], vp[3] );
		}
		int d = 0, rd = 0, vp[4] = {};
		if ( m == TgMode::DocKeep ) {
			if ( !kept.take( 1, d, rd, vp ) ) {
				gl.glGetIntegerv( kViewport, vp );
				gl.glGetIntegerv( kDrawFb, &d );
				gl.glGetIntegerv( kReadFb, &rd );
			}
			kept.frame = 1;	// never forgotten
		} else {
			WwCellUni::targetFrom( &gl, WwCellUni::frameTarget(), f, d, rd, vp, check );
		}
		out.begin.push_back( d );
		out.begin.push_back( rd );
		out.begin.insert( out.begin.end(), vp, vp + 4 );
	}
	out.asks = gl.total;
	out.stale = WwCellUni::state().knownStale;
	return out;
}
static void hdrbegin()
{
	const TgOut g = tgRun( TgMode::Green, false ), r = tgRun( TgMode::Red, false );
	check( g.begin == r.begin, "hdrbegin: Begin's draw, read and viewport as the red's (10 paints, a shot, no obscurance twice)" );
	std::printf( "     hdrbegin: asks green %lld red %lld\n", g.asks, r.asks );
	check( g.asks == r.asks - 3 * 8, "hdrbegin: green asks 3 less in each paint with the obscurance pass (8 of 10)" );
	check( tgRun( TgMode::DocKeep, false ).begin != r.begin, "hdrbegin: doctored, a note kept across paints differs" );
	const TgOut c = tgRun( TgMode::Green, true );
	check( c.begin == r.begin && c.stale == 0, "hdrbegin: WW_SETUP_MEMO_CHECK=1, every pass puts it back: knownstale 0" );
	const TgOut l = tgRun( TgMode::DocLeakRead, true ), lr = tgRun( TgMode::Red, false );
	check( l.stale == 1 && l.begin != lr.begin, "hdrbegin: WW_SETUP_MEMO_CHECK=1, a pass that leaves the read framebuffer moved: knownstale 1" );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	redNames();
	jitter();
	nonlinear();
	hdrdepth();
	hdrbegin();
	std::printf( "%s (%d failed)\n", fails ? "CELLUNI6 FAIL" : "CELLUNI6 OK", fails );
	return fails ? 1 : 0;
}
