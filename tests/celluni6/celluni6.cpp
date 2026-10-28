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
	kViewport = 0x0BA2, kDrawFb = 0x8CA6, kReadFb = 0x8CAA, kBlend = 0x0BE2, kNoError = 0, kInvalidOp = 0x0502
};

// the GL state the modelled code reads and writes; every ask counted by its name
struct FakeGl
{
	int vp[4] = { 0, 0, 64, 32 };
	int draw = 0, read = 0;
	bool blend = false;
	std::vector<unsigned> errors;	// the driver's error queue
	std::map<unsigned, long long> asks;
	long long total = 0;
	void glGetIntegerv( unsigned p, int * v )
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

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	redNames();
	jitter();
	std::printf( "%s (%d failed)\n", fails ? "CELLUNI6 FAIL" : "CELLUNI6 OK", fails );
	return fails ? 1 : 0;
}
