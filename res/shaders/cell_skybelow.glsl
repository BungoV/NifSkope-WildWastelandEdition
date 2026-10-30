// Lane SKYBELOW2 (2026-10-31, bungo 10-30 LATE ITEMs 3 + 3b after SKYBELOW1): below the horizon is ground, never sky,
// for the reflection probes' relight too (cell_reflrelight.frag, src/gl/cellreflprobe.cpp). A G-buffer cube texel that
// MISSES (nothing drawn there) in a world direction below the horizon returns the ground, never the sky ambient:
// outdoors the ground's albedo x what an open up-facing white surface shows (rpAmbient(+Z): the sky irradiance from
// above / pi, the shader's own unit), indoors 0 (the void under a cell is never sky). Above the horizon the miss is the
// sky ambient, unchanged. The ground's albedo = the mean albedo of the probe's own below-horizon hits on 9 fixed taps
// (straight down, 4 at 20 degrees below the horizon, 4 at 60), 0.5 when none hit (SKYBELOW1's no-soup albedo).
// MIRROR rtddgimath.h belowHorizon / missRadiance (the same horizon, d.z < 0; the same ground law).
// rule 0 = the red WW_SKYBELOW_RED (today's main: the sky ambient in every direction), set live by cellreflprobe.cpp.
// tests/skybelow2 compiles this same file through tests/rtddgi_math/glsl_shim.h, so it keeps to the subset C++ also
// reads (no swizzles, no out parameters, no arrays). SKYBELOW2_RED_* exist only in that test.

// a miss direction below the horizon (world z up)
bool wwSkyBelow( vec3 d )
{
#ifdef SKYBELOW2_RED_LIFT
	return d.z < 0.1;	// red: a lifted horizon (the near-horizon sky above it turns ground)
#else
	return d.z < 0.0;
#endif
}

// tap k (0..8) of the ground's albedo: straight down, 4 on the axes 20 degrees below the horizon, 4 on the diagonals at 60
vec3 wwSkyBelowTap( int k )
{
	if ( k == 0 )
		return vec3( 0.0, 0.0, -1.0 );
	if ( k < 5 ) {
		float c = 0.93969262, s = -0.34202014;
		if ( k == 1 )
			return vec3( c, 0.0, s );
		if ( k == 2 )
			return vec3( -c, 0.0, s );
		if ( k == 3 )
			return vec3( 0.0, c, s );
		return vec3( 0.0, -c, s );
	}
	float q = 0.35355339, s6 = -0.86602540;
	return vec3( k == 5 || k == 8 ? q : -q, k < 7 ? q : -q, s6 );
}

// the ground's albedo from the taps that hit: sum of their albedo (rgb, already squared), n of them
vec3 wwSkyBelowAlbedo( vec3 sum, float n )
{
#ifdef SKYBELOW2_RED_ALBEDO
	return sum * ( 1.0 / 9.0 );	// red: a tap that missed counted as black
#else
	return n > 0.5 ? sum * ( 1.0 / n ) : vec3( 0.5 );
#endif
}

// the miss's radiance in direction d: sky = the sky ambient there (today's miss), skyUp = an open up-facing white
// surface's ambient (rpAmbient(+Z)), alb = wwSkyBelowAlbedo; rule 1 on, 0 the red
vec3 wwSkyBelowMiss( vec3 d, vec3 sky, vec3 alb, vec3 skyUp, bool interior, int rule )
{
#ifdef SKYBELOW2_RED_MISS
	rule = 0;	// red: the sky below the horizon too (today's main)
#endif
#ifdef SKYBELOW2_RED_NOSWITCH
	rule = 1;	// red: the switch ignored (WW_SKYBELOW_RED could not bring today's main back)
#endif
	if ( rule != 0 && wwSkyBelow( d ) ) {
#ifdef SKYBELOW2_RED_INTGROUND
		return alb * skyUp;	// red: an interior's void lit as ground
#else
		return interior ? vec3( 0.0 ) : alb * skyUp;
#endif
	}
	return sky;
}
