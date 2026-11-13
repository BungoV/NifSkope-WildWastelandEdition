// HLSL source of features/sky/cell_skybelow.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// Lane SKYBELOW2 (2026-10-31, bungo 10-30 LATE ITEMs 3 + 3b after SKYBELOW1): below the horizon is ground, never sky,
// for the reflection probes' relight too (cell_reflrelight.frag, src/features/reflections/cellreflprobe.cpp). A G-buffer cube texel that
// MISSES (nothing drawn there) in a world direction below the horizon returns the ground, never the sky ambient:
// outdoors the ground's albedo x what an open up-facing white surface shows (rpAmbient(+Z): the sky irradiance from
// above / pi, the shader's own unit), indoors 0 (the void under a cell is never sky). Above the horizon the miss is the
// sky ambient, unchanged. The ground's albedo = the mean albedo of the probe's own below-horizon hits on 9 fixed taps
// (straight down, 4 at 20 degrees below the horizon, 4 at 60), 0.5 when none hit (SKYBELOW1's no-soup albedo).
// MIRROR rtddgimath.h belowHorizon / missRadiance (the same horizon, d.z < 0; the same ground law).
// rule 0 = the red WW_SKYBELOW_RED (today's main: the sky ambient in every direction), set live by cellreflprobe.cpp.
// tests/features/sky/skybelow2 compiles this same file through tests/features/gi/rtddgi_math/glsl_shim.h, so it keeps to the subset C++ also
// reads (no swizzles, no out parameters, no arrays). SKYBELOW2_RED_* exist only in that test.

// a miss direction below the horizon (world z up)
bool wwSkyBelow( float3 d )
{
#ifdef SKYBELOW2_RED_LIFT
	return d.z < 0.1;	// red: a lifted horizon (the near-horizon sky above it turns ground)
#else
	return d.z < 0.0;
#endif
}

// tap k (0..8) of the ground's albedo: straight down, 4 on the axes 20 degrees below the horizon, 4 on the diagonals at 60
float3 wwSkyBelowTap( int k )
{
	if ( k == 0 )
		return float3( 0.0, 0.0, -1.0 );
	if ( k < 5 ) {
		float c = 0.93969262, s = -0.34202014;
		if ( k == 1 )
			return float3( c, 0.0, s );
		if ( k == 2 )
			return float3( -c, 0.0, s );
		if ( k == 3 )
			return float3( 0.0, c, s );
		return float3( 0.0, -c, s );
	}
	float q = 0.35355339, s6 = -0.86602540;
	return float3( or( k == 5, k == 8 ) ? q : -q, k < 7 ? q : -q, s6 );
}

// the ground's albedo from the taps that hit: sum of their albedo (rgb, already squared), n of them
float3 wwSkyBelowAlbedo( float3 sum, float n )
{
#ifdef SKYBELOW2_RED_ALBEDO
	return sum * ( 1.0 / 9.0 );	// red: a tap that missed counted as black
#else
	return n > 0.5 ? sum * ( 1.0 / n ) : (float3)( 0.5 );
#endif
}

// the miss's radiance in direction d: sky = the sky ambient there (today's miss), skyUp = an open up-facing white
// surface's ambient (rpAmbient(+Z)), alb = wwSkyBelowAlbedo; rule 1 on, 0 the red
float3 wwSkyBelowMiss( float3 d, float3 sky, float3 alb, float3 skyUp, bool interior, int rule )
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
		return interior ? (float3)( 0.0 ) : alb * skyUp;
#endif
	}
	return sky;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/sky/cell_skybelow.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): wwSkyBelow, wwSkyBelowTap, wwSkyBelowAlbedo, wwSkyBelowMiss
#endif	// WW_HLSL_SRC
