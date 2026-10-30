/* lane GLARESB2 (2026-10-30): a live set of WW_CELL_PSYS_OBJ_RED leaves no particle of the other value behind.
 *
 * GLAREBLOW2's fix reads the gravity / drag / bomb objects' frames each step and its red (`setup`: the frames update()
 * read before the cell copy is placed) each step too. But the PARTICLES its steps moved stay: a repeat row shot after
 * live red rows (glareblow2 run2 c3_t2z_fix2b) drew the red's two strands again. The fix (src/gl/psyslive.h, called at
 * the top of PSysSimController::updateTime): the first step after the red was set or unset empties the system and takes
 * the step a freshly built controller takes first (lastTime as constructed).
 *
 * This is a twin of updateTime's clock (restart check, backward scrub, dt clamp 0.25, age death) and of the spherical
 * pull (outward, strength x dt / len, the viewer rule), with the REAL switch store (src/wwswitch.cpp: wwSwitchSet /
 * wwSwitchReset move the generation exactly as the live driver's `set` / `reset`). Emission is deterministic in the
 * birth time, so "the same state as a fresh build" can be checked bit for bit.
 * Green: the header's restart. Red (-DPSYSLIVE_RED, today's code = no restart): the strands stay -> FAIL.
 * Prints "PSYSLIVE PASS" (exit 0) or "PSYSLIVE FAIL" (exit 1). */

#include "wwswitch.h"
#include "gl/psyslive.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct V3 { double x = 0, y = 0, z = 0; };
V3 operator+( V3 a, V3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator-( V3 a, V3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 operator*( V3 a, double s ) { return { a.x * s, a.y * s, a.z * s }; }
double len( V3 a ) { return std::sqrt( a.x * a.x + a.y * a.y + a.z * a.z ); }

// FXBitsDebrisSpiral01 in Concord: the copy root placed ~90 km from the world origin; Gravity004 at (0, 0, 3.3) under it
const V3 kPlace { -58085.0, 70814.0, 600.0 };
const V3 kGravLocal { 0.0, 0.0, 3.3 };
const double kStrength = 45.0;
const double kLife = 4.0;	// s; NiPSysAgeDeathModifier
const double kRate = 30.0;	// particles per second

struct P { V3 pos, vel; double age = 0, life = kLife; };

struct Sim
{
	std::vector<P> parts;
	double accum = 0;
	float lastTime = kWwPsysFreshLastTime;
	quint64 objRedSeen = 0;
	int restarts = 0;

	// the particle born at time t: deterministic in t alone (a fresh build and a restarted one emit the same bits)
	static P born( double t )
	{
		P p;
		const double a = t * 2.399963;	// golden angle per second
		p.pos = kPlace + V3 { 20.0 * std::cos( a ), 20.0 * std::sin( a ), 0.0 };
		p.vel = V3 { 30.0 * std::cos( a + 1.0 ), 30.0 * std::sin( a + 1.0 ), 60.0 };
		return p;
	}

	void updateTime( float time )
	{
#ifndef PSYSLIVE_RED
		if ( wwPsysObjRedMoved( objRedSeen ) ) {	// as PSysSimController::updateTime
			parts.clear();
			accum = 0;
			lastTime = kWwPsysFreshLastTime;
			restarts++;
		}
#endif
		float dt = time - lastTime;
		if ( dt < 0.0f ) {
			parts.clear();
			accum = 0;
			lastTime = time;
			return;
		}
		lastTime = time;
		if ( dt <= 0.0f )
			return;
		dt = std::min( dt, 0.25f );
		WW_SWITCH_STATIC( bool, objSetupRed, wwEnv( "WW_CELL_PSYS_OBJ_RED" ).trimmed() == "setup" );
		// live: the object's frame now (placed); red: the frame update() read (the copy root not placed yet)
		const V3 origin = objSetupRed ? kGravLocal : kPlace + kGravLocal;
		size_t n = 0;
		while ( n < parts.size() ) {
			P & p = parts[n];
			p.age += dt;
			if ( p.age >= p.life ) {
				parts.erase( parts.begin() + long( n ) );
				continue;
			}
			const V3 d = p.pos - origin;
			const double l = len( d );
			if ( l > 1.0e-4 )
				p.vel = p.vel + d * ( kStrength * dt / l );
			p.pos = p.pos + p.vel * dt;
			n++;
		}
		accum += kRate * dt;
		int num = int( accum );
		accum -= num;
		while ( num-- > 0 )
			parts.push_back( born( time ) );
	}

	double farthest() const
	{
		double m = 0;
		for ( const P & p : parts )
			m = std::max( m, len( p.pos - kPlace ) );
		return m;
	}
	/* the strand test: how far the cloud's centre sits from the devil's, along "away from the world origin" (units).
	 * The devil's own pull is radial, so its cloud stays centred; a pull point at the unplaced frame pushes every bit
	 * one way, the streaks hanging off to one side. */
	double drift() const
	{
		if ( parts.empty() )
			return 0.0;
		const V3 away = kPlace * ( 1.0 / len( kPlace ) );
		double s = 0;
		for ( const P & p : parts ) {
			const V3 r = p.pos - ( kPlace + kGravLocal );
			s += r.x * away.x + r.y * away.y + r.z * away.z;
		}
		return s / double( parts.size() );
	}
};

bool same( const Sim & a, const Sim & b, double * worst )
{
	*worst = 0;
	if ( a.parts.size() != b.parts.size() )
		return false;
	for ( size_t i = 0; i < a.parts.size(); i++ ) {
		*worst = std::max( *worst, len( a.parts[i].pos - b.parts[i].pos ) );
		*worst = std::max( *worst, std::fabs( a.parts[i].age - b.parts[i].age ) );
	}
	return *worst == 0.0;
}

int fails = 0;
void check( bool ok, const char * what, const char * detail )
{
	std::printf( "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail );
	fails += ok ? 0 : 1;
}

const float kStep = 1.0f / 30.0f;

}	// namespace

int main()
{
	char buf[256];
	qunsetenv( "WW_CELL_PSYS_OBJ_RED" );
	qunsetenv( "WW_CELL_BLOOM_SKY_RED" );

	check( wwSwitchStageOf( "WW_CELL_PSYS_OBJ_RED" ) == WwSwitchStage::Live, "stage table: WW_CELL_PSYS_OBJ_RED is live", "" );

	// live: 3 s off, 3 s of the red set live, unset (the live driver's reset), then 2 s off
	Sim live;
	float t = 0.0f;
	for ( int i = 0; i < 90; i++, t += kStep )
		live.updateTime( t );
	const Sim offBefore = live;
	std::printf( "set: %s\n", qPrintable( wwSwitchSet( "WW_CELL_PSYS_OBJ_RED", "setup", false ) ) );
	for ( int i = 0; i < 90; i++, t += kStep )
		live.updateTime( t );
	const double redDrift = live.drift(), offDrift = offBefore.drift();
	std::snprintf( buf, sizeof buf, "cloud centre %.1f u off the devil's (off before the red: %.1f u), %d bits, farthest %.0f u",
		redDrift, offDrift, int( live.parts.size() ), live.farthest() );
	check( redDrift > offDrift + 40.0, "the red draws strands in the twin (the twin's red is a red)", buf );
	std::printf( "reset: %s\n", qPrintable( wwSwitchReset( "WW_CELL_PSYS_OBJ_RED" ) ) );
	const float tUnset = t;

	// fresh: a controller built when the red went off, stepped at the same times
	Sim fresh;
	for ( int i = 0; i < 60; i++, t += kStep ) {
		live.updateTime( t );
		fresh.updateTime( t );
	}
	double worst = 0;
	const bool eq = same( live, fresh, &worst );
	std::snprintf( buf, sizeof buf, "live %d bits, fresh %d bits, worst difference %.3g (unset at t %.2f)",
		int( live.parts.size() ), int( fresh.parts.size() ), worst, tUnset );
	check( eq, "after the red goes off live, the system equals a fresh build's", buf );
	std::snprintf( buf, sizeof buf, "cloud centre %.1f u off the devil's (fresh %.1f u)", live.drift(), fresh.drift() );
	check( live.drift() < fresh.drift() + 5.0, "no strand left after the red", buf );

	// another switch set and unset live restarts nothing (the default path runs on, bit for bit)
	Sim a = offBefore, b = offBefore;
	a.objRedSeen = b.objRedSeen = wwSwitchGen();
	for ( int i = 0; i < 30; i++ )	// b first: no set at all while it runs
		b.updateTime( 103.0f + i * kStep );
	for ( int i = 0; i < 30; i++ ) {	// a: the bloom red set and unset live mid-run
		if ( i == 10 )
			wwSwitchSet( "WW_CELL_BLOOM_SKY_RED", "add", false );
		if ( i == 20 )
			wwSwitchReset( "WW_CELL_BLOOM_SKY_RED" );
		a.updateTime( 103.0f + i * kStep );
	}
	const bool otherSame = same( a, b, &worst );
	std::snprintf( buf, sizeof buf, "restarts %d, %d bits", a.restarts, int( a.parts.size() ) );
	check( otherSame && a.restarts == 0, "a live set of another switch restarts no system", buf );

	/* a controller built after earlier sets (its stamp 0, the generation not): its first step's restart finds it empty
	 * and fresh already, so it runs exactly as one stamped now; after that, with no set, nothing restarts again */
	Sim c, d;
	d.objRedSeen = wwSwitchGen();
	for ( int i = 0; i < 30; i++ ) {
		c.updateTime( 500.0f + i * kStep );
		d.updateTime( 500.0f + i * kStep );
	}
	const bool firstSame = same( c, d, &worst );
	std::snprintf( buf, sizeof buf, "restarts %d (one stamped now: %d), %d bits", c.restarts, d.restarts, int( c.parts.size() ) );
	check( firstSame && c.restarts <= 1 && d.restarts == 0, "no set since the stamp: no restart", buf );

#ifdef PSYSLIVE_RED
	std::printf( "(red: today's updateTime, no restart on a live set)\n" );
#endif
	std::printf( "PSYSLIVE %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
