// lane REDRETIRE2 unit test: FLYFIX1's free-camera rules (src/wwflyfix.h), the same checks as the in-app gate
// (src/glview.cpp GLView::wwFlyRefocusCheck, live verb "flycheck") on a twin of GLView's fly branch:
//   control: a plain move looks;
//   refocus: after FocusIn / WindowActivate / Enter the first move (the cursor back at the corner) leaves the view
//            alone, the next move looks again;
//   tick:    one gears tick after a 5 s stall moves at most 0.1 s worth (and more than nothing); a normal frame is kept.
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Reds (the red build, -DWW_REDTWIN, src/wwredtwin.h), each must FAIL: REDTWIN=FLYFIX=tick (no stall clamp),
// REDTWIN=FLYFIX=refocus (the first move after a refocus looks). tests/flyfix/run.sh runs them all.

#include "wwflyfix.h"

#include <cmath>
#include <cstdio>

namespace {

int checks = 0, fails = 0;

void check( bool ok, const char * what, double v )
{
	checks++;
	fails += !ok;
	std::printf( "  %s %s (%.4f)\n", ok ? "ok  " : "FAIL", what, v );
}

//! GLView's free-camera state that the two rules touch: the look (Rot), the place (Pos), the resync flag.
struct Fly
{
	float rotX = 0.0f, rotZ = 0.0f, pos = 0.0f;
	bool flyResync = false;
	const float cx = 640.0f, cy = 360.0f;	// the window's centre (a 1280 x 720 view)

	//! GLView::event: focus, activation or the cursor leaving/entering
	void refocus() { flyResync = true; }

	//! GLView::mouseMoveEvent's fly branch (freeCameraLook with the 0.2 deg/px gain)
	void moveTo( float x, float y )
	{
		if ( wwFlyResyncOnly( flyResync ) )
			return;	// re-centre only
		rotX += ( y - cy ) * 0.2f;
		rotZ += ( x - cx ) * 0.2f;
	}

	//! GLView::advanceGears with W held: moveSpd units a second
	void tick( float dT, float moveSpd ) { pos += moveSpd * wwFlyTickSeconds( dT ); }

	float moved( const Fly & b ) const
	{
		return std::fmax( std::fabs( rotX - b.rotX ), std::fmax( std::fabs( rotZ - b.rotZ ), std::fabs( pos - b.pos ) ) );
	}
};

} // namespace

int main()
{
	const Fly rest;
	{
		Fly f;
		f.moveTo( f.cx + 40.0f, f.cy );
		check( f.moved( rest ) > 0.0f, "control: a plain move looks", f.moved( rest ) );
	}
	const char * names[3] = { "FocusIn: the first move after it leaves the view alone",
		"WindowActivate: the first move after it leaves the view alone", "Enter: the first move after it leaves the view alone" };
	for ( int k = 0; k < 3; k++ ) {
		Fly f;
		f.refocus();
		f.moveTo( 3.0f, 3.0f );	// the cursor came back at the window's corner
		const float jump = f.moved( rest );
		check( jump == 0.0f, names[k], jump );
		Fly g = f;
		g.moveTo( g.cx + 40.0f, g.cy );
		check( g.moved( f ) > 0.0f, "  ... and the next move looks again", g.moved( f ) );
	}
	{
		const float moveSpd = 350.0f;
		Fly f;
		f.tick( 5.0f, moveSpd );
		const float secs = f.pos / moveSpd;
		check( secs > 0.0f && secs <= 0.1001f, "tick after a 5 s stall moves at most 0.1 s of flight", secs );
		Fly g;
		g.tick( 0.016f, moveSpd );
		check( std::fabs( g.pos / moveSpd - 0.016f ) < 1.0e-6f, "a normal 16 ms frame moves its own time", g.pos / moveSpd );
	}
	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
