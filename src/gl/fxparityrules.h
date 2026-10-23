#ifndef WW_FXPARITYRULES_H
#define WW_FXPARITYRULES_H

/* lane FXPARITY1: the game's particle modifier rules as pure functions, read from Todd's treat.
 * src/gl/controllers.cpp runs them while "Particle Game Rules" is on (fxparity.h); tests/fxparity checks them. */

#include <algorithm>

namespace FxRules
{

inline float lerp( float a, float b, float t )
{
	return ( b - a ) * t + a;
}

/* BSPSysSimpleColorModifier. f = the six percents in file order: fade in, fade out, colour 1 end, colour 1 start
 * (where colour 2 is reached), colour 2 end, colour 2 start (where colour 3 is reached). col = the three RGBA
 * colours, u = age / life span. The alpha has its own ramps: colour 1's alpha into colour 2's over the fade in,
 * colour 2's into colour 3's after the fade out; a fade of 0 means none. */
inline void simpleColor( const float f[6], const float col[3][4], float u, float out[4] )
{
	const float fi = f[0], fo = f[1], a = f[2], b = f[3], c = f[4], d = f[5];
	if ( b - a == 0.0f || u >= b ) {
		if ( d - c == 0.0f || u <= c ) {
			for ( int i = 0; i < 3; i++ )
				out[i] = col[1][i];
		} else if ( u <= d ) {
			const float t = ( u - c ) / ( d - c );
			for ( int i = 0; i < 3; i++ )
				out[i] = lerp( col[1][i], col[2][i], t );
		} else {
			for ( int i = 0; i < 3; i++ )
				out[i] = col[2][i];
		}
	} else if ( u < a ) {
		for ( int i = 0; i < 3; i++ )
			out[i] = col[0][i];
	} else {
		const float t = ( u - a ) / ( b - a );
		for ( int i = 0; i < 3; i++ )
			out[i] = lerp( col[0][i], col[1][i], t );
	}
	if ( fo == 0.0f || u <= fo )
		out[3] = ( fi == 0.0f || u >= fi ) ? col[1][3] : lerp( col[0][3], col[1][3], u / fi );
	else
		out[3] = lerp( col[1][3], col[2][3], ( u - fo ) / ( 1.0f - fo ) );
}

/* NiPSysDragModifier: the share of the velocity along the drag axis removed this step. Linear in the step and
 * capped at all of it (not an exponential decay). */
inline float dragShare( float percentage, float dt )
{
	return std::min( percentage * 30.0f * dt, 1.0f );
}

/* BSPSysSubTexModifier: the flipbook advances one frame each time int(u * steps) grows, so Frame Count is the
 * number of steps over the particle's life, not a rate. k starts at -1; rnd(x) is a random value in [0, x]. */
struct SubTex
{
	float start = 0, startFudge = 0, end = 0, loopStart = 0, loopFudge = 0, count = 0, countFudge = 0;
};

template <typename Rnd>
inline void subTexStep( const SubTex & s, int & k, int & steps, int & frame, float u, Rnd && rnd )
{
	if ( k == -1 )
		steps = int( s.count + rnd( s.countFudge ) );
	const int now = int( u * float( steps ) );
	if ( now <= k )
		return;
	const bool first = k == -1;
	k = now;
	if ( first )
		frame = int( s.start + rnd( s.startFudge ) );
	else if ( float( frame + 1 ) > s.end )
		frame = int( s.loopStart + rnd( s.loopFudge ) );
	else
		frame++;
}

}	// namespace FxRules

#endif
