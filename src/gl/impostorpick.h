#ifndef IMPOSTORPICK_H
#define IMPOSTORPICK_H

/* The far card's frame pick, as plain float arithmetic (lane FARCARDPICK1, 2026-11-02).
 *
 * The CPU half of every card: the view direction, the ring's two azimuths, the grid's quarter-turn red, the
 * slider's sharpen / one-frame snap and the standing billboard. src/gl/impostordraw.cpp (selectFrames,
 * pickCard) calls these, and so does the offline unit (tests/farcardpick), so the unit's "CPU" column is the
 * shipped code. res/shaders/impostor_pick.glsl is the same arithmetic on the GPU, line for line in the same
 * order. The grid's own pick stays ImpostorOct::pickFramesForCamera. No Qt here. */

#include <cmath>

namespace ImpostorPick
{

//! selectFrames' direction: from the card's centre to the eye, unit length; straight up when they meet.
inline void viewDir( const float eye[3], const float centre[3], float d[3] )
{
	d[0] = eye[0] - centre[0];
	d[1] = eye[1] - centre[1];
	d[2] = eye[2] - centre[2];
	const float len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
	if ( len > 1e-6f ) {
		d[0] = d[0] / len; d[1] = d[1] / len; d[2] = d[2] / len;
	} else {
		d[0] = 0.0f; d[1] = 0.0f; d[2] = 1.0f;
	}
}

/*! THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1). Frame v was photographed from azimuth 360*v/V at
 *  elevation 0: the two frames that bracket the camera's azimuth, the stronger first, slot 2 the first again at
 *  weight 0. `shuffle` is the red control: every frame becomes the view from 90 degrees round. */
inline void ringFrames( const float d[3], int V, bool shuffle, int idx[3], float w[3] )
{
	const float twoPi = 6.28318530718f;
	float phi = std::atan2( d[1], d[0] );
	if ( phi < 0.0f )
		phi += twoPi;
	const float f = phi / twoPi * float( V );
	const float fl = std::floor( f );
	const float t = f - fl;
	const int i0 = ( ( int( fl ) % V ) + V ) % V, i1 = ( i0 + 1 ) % V;
	if ( t <= 0.5f ) {
		idx[0] = i0; w[0] = 1.0f - t; idx[1] = i1; w[1] = t;
	} else {
		idx[0] = i1; w[0] = t; idx[1] = i0; w[1] = 1.0f - t;
	}
	idx[2] = idx[0]; w[2] = 0.0f;
	if ( shuffle )
		for ( int k = 0; k < 3; k++ )
			idx[k] = ( idx[k] + V / 4 ) % V;
}

//! The grid's red control: the fixed quarter turn (i, j) -> (j, N-1-i) (see selectFrames for why a quarter).
inline void gridShuffle( int N, int idx[3] )
{
	for ( int k = 0; k < 3; k++ ) {
		const int i = idx[k] % N;
		const int j = idx[k] / N;
		idx[k] = j + ( N - 1 - i ) * N;
	}
}

//! A ring blends its two neighbouring azimuths, never three.
inline int framesDrawn( bool ring, int frameCount )
{
	return ring ? ( frameCount < 2 ? frameCount : 2 ) : frameCount;
}

/*! THE SLIDER (lane IMPOSTORDEPTH2) on the picked weights: between the ends the weights are sharpened and
 *  renormalised; at the crisp end (frameCount 1) the strongest frame alone (the first of a tie) at weight 1. */
inline void sharpenWeights( int frameCount, float sharpen, int idx[3], float w[3] )
{
	if ( frameCount > 1 && sharpen != 1.0f ) {
		float sum = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			w[k] = std::pow( ( 0.0f < w[k] ) ? w[k] : 0.0f, sharpen );
			sum += w[k];
		}
		if ( sum > 0.0f )
			for ( int k = 0; k < 3; k++ )
				w[k] /= sum;
	}
	if ( frameCount == 1 ) {
		int best = 0;
		for ( int k = 1; k < 3; k++ ) {
			if ( w[k] > w[best] )
				best = k;
		}
		idx[0] = idx[best];
		w[0] = 1.0f;
	}
}

//! Vector3::normalize: divide by the length when it is above 0, else zero.
inline void normaliseOrZero( float v[3] )
{
	const float m = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( m > 0.0f ) {
		v[0] /= m; v[1] /= m; v[2] /= m;
	} else {
		v[0] = v[1] = v[2] = 0.0f;
	}
}

/*! The quad faces the camera and stands UP: world +Z projected into the plane perpendicular to the view
 *  direction (Vector3::crossproduct( +Z, fwd ), written out). Straight down the pole: +X, the bake's top frame. */
inline void billboard( const float fwd[3], float right[3], float up[3] )
{
	right[0] = 0.0f * fwd[2] - 1.0f * fwd[1];
	right[1] = 1.0f * fwd[0] - 0.0f * fwd[2];
	right[2] = 0.0f * fwd[1] - 0.0f * fwd[0];
	if ( std::sqrt( right[0] * right[0] + right[1] * right[1] + right[2] * right[2] ) < 1e-4f ) {
		right[0] = 1.0f; right[1] = 0.0f; right[2] = 0.0f;
	}
	normaliseOrZero( right );
	up[0] = fwd[1] * right[2] - fwd[2] * right[1];
	up[1] = fwd[2] * right[0] - fwd[0] * right[2];
	up[2] = fwd[0] * right[1] - fwd[1] * right[0];
	normaliseOrZero( up );
}

} // namespace ImpostorPick

#endif
