/* The far card's frame pick on the GPU (lane FARCARDPICK1, 2026-11-02).
 *
 * src/features/far-lod/impostorpick.h + ImpostorOct::pickFramesForCamera (src/features/far-lod/impostoroct.cpp) + pickCard
 * (src/features/far-lod/impostordraw.cpp), the same steps in the same order: the view direction, the ring's two azimuths or
 * the grid's three barycentric frames under the set's convention, the quarter-turn red, the slider's sharpen /
 * one-frame snap, and the standing billboard. Lengths are written as sqrt( x*x + y*y + z*z ) and divisions as
 * divisions (not normalize()) so the steps match the CPU's; `precise` keeps the driver from fusing them.
 * What still differs is the driver's own atan / sqrt / division / pow: a frame index can move only where the
 * CPU's value sits on a floor or a diagonal (tests/features/far-lod/farcardpick measures every such row).
 *
 * In:  eye, centre (the card's world centre, the CPU's float), N (grid side) or V (ring views), ring, asBaked
 *      (the legacy 180: x, y flipped before the lookup), shuffle (the red), sharpen, frameCount (after the ring's
 *      cap of 2).
 * Out: the three frame indices and weights, the billboard axes (fwd = the view direction).
 */

void wwCardPick( vec3 eye, vec3 centre, int N, int V, bool ring, bool asBaked, bool shuffle, float sharpen,
				 int frameCount, out ivec3 idx, out vec3 w, out vec3 right, out vec3 up, out vec3 fwd )
{
	precise vec3 d = eye - centre;
	precise float len = sqrt( d.x * d.x + d.y * d.y + d.z * d.z );
	if ( len > 1e-6 )
		d = vec3( d.x / len, d.y / len, d.z / len );
	else
		d = vec3( 0.0, 0.0, 1.0 );

	int i3[3] = int[3]( 0, 0, 0 );
	float w3[3] = float[3]( 0.0, 0.0, 0.0 );
	if ( ring ) {
		const float twoPi = 6.28318530718;
		precise float phi = atan( d.y, d.x );
		if ( phi < 0.0 )
			phi += twoPi;
		precise float f = phi / twoPi * float( V );
		float fl = floor( f );
		precise float t = f - fl;
		int i0 = ( ( int( fl ) % V ) + V ) % V;
		int i1 = ( i0 + 1 ) % V;
		if ( t <= 0.5 ) {
			i3[0] = i0; w3[0] = 1.0 - t; i3[1] = i1; w3[1] = t;
		} else {
			i3[0] = i1; w3[0] = t; i3[1] = i0; w3[1] = 1.0 - t;
		}
		i3[2] = i3[0]; w3[2] = 0.0;
		if ( shuffle )
			for ( int k = 0; k < 3; k++ )
				i3[k] = ( i3[k] + V / 4 ) % V;
	} else {
		// gridLookupDir, then dirToGrid
		precise vec3 e = asBaked ? vec3( -d.x, -d.y, d.z ) : d;
		precise float n = sqrt( e.x * e.x + e.y * e.y + e.z * e.z );
		bool ok = n > 1e-20;
		if ( ok )
			e = vec3( e.x / n, e.y / n, e.z / n );
		if ( ok && e.z < 0.0 ) {
			e.z = 0.0;
			n = sqrt( e.x * e.x + e.y * e.y + e.z * e.z );
			ok = n > 1e-20;
			if ( ok )
				e = vec3( e.x / n, e.y / n, e.z / n );
		}
		precise float L = abs( e.x ) + abs( e.y ) + e.z;
		ok = ok && L > 1e-20 && N >= 2 && N <= 16;
		if ( ok ) {
			precise float x = e.x / L;
			precise float y = e.y / L;
			precise float u = x + y;
			precise float v = x - y;
			precise float a = ( u + 1.0 ) * 0.5 * float( N - 1 );
			precise float b = ( v + 1.0 ) * 0.5 * float( N - 1 );
			float hi = float( N - 1 );
			a = clamp( a, 0.0, hi );
			b = clamp( b, 0.0, hi );
			// pickFrames
			int g0 = clamp( int( floor( a ) ), 0, N - 2 );
			int h0 = clamp( int( floor( b ) ), 0, N - 2 );
			precise float fa = a - float( g0 );
			precise float fb = b - float( h0 );
			ivec3 gi, gj;
			if ( fa + fb <= 1.0 ) {
				gi = ivec3( g0, g0 + 1, g0 );     gj = ivec3( h0, h0, h0 + 1 );
				w3[0] = 1.0 - fa - fb; w3[1] = fa; w3[2] = fb;
			} else {
				gi = ivec3( g0 + 1, g0, g0 + 1 ); gj = ivec3( h0 + 1, h0 + 1, h0 );
				w3[0] = fa + fb - 1.0; w3[1] = 1.0 - fa; w3[2] = 1.0 - fb;
			}
			for ( int k = 0; k < 3; k++ ) {
				if ( w3[k] < 0.0 )
					w3[k] = 0.0;
				i3[k] = gi[k] + gj[k] * N;
			}
		}
		// selectFrames turns the grid whether or not the pick answered (a view from straight below keeps 0, 0, 0)
		if ( shuffle )
			for ( int k = 0; k < 3; k++ ) {
				int i = i3[k] % N;
				int j = i3[k] / N;
				i3[k] = j + ( N - 1 - i ) * N;
			}
	}

	// the slider: sharpen and renormalise between the ends, the strongest frame alone at the crisp end
	if ( frameCount > 1 && sharpen != 1.0 ) {
		precise float sum = 0.0;
		for ( int k = 0; k < 3; k++ ) {
			w3[k] = ( 0.0 < w3[k] ) ? pow( w3[k], sharpen ) : 0.0;
			sum += w3[k];
		}
		if ( sum > 0.0 )
			for ( int k = 0; k < 3; k++ )
				w3[k] = w3[k] / sum;
	}
	if ( frameCount == 1 ) {
		int best = 0;
		for ( int k = 1; k < 3; k++ )
			if ( w3[k] > w3[best] )
				best = k;
		i3[0] = i3[best];
		w3[0] = 1.0;
	}
	idx = ivec3( i3[0], i3[1], i3[2] );
	w = vec3( w3[0], w3[1], w3[2] );

	// the standing billboard: right = +Z x fwd, +X straight down the pole; up = fwd x right
	fwd = d;
	precise vec3 r = vec3( -d.y, d.x, 0.0 );
	if ( sqrt( r.x * r.x + r.y * r.y + r.z * r.z ) < 1e-4 )
		r = vec3( 1.0, 0.0, 0.0 );
	precise float rm = sqrt( r.x * r.x + r.y * r.y + r.z * r.z );
	r = ( rm > 0.0 ) ? vec3( r.x / rm, r.y / rm, r.z / rm ) : vec3( 0.0 );
	precise vec3 q = vec3( d.y * r.z - d.z * r.y, d.z * r.x - d.x * r.z, d.x * r.y - d.y * r.x );
	precise float qm = sqrt( q.x * q.x + q.y * q.y + q.z * q.z );
	up = ( qm > 0.0 ) ? vec3( q.x / qm, q.y / qm, q.z / qm ) : vec3( 0.0 );
	right = r;
}
