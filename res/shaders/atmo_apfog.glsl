// lane SKYA (ATMO2): how the physical sky's aerial perspective and the cell fog share one surface's air.
// Both describe the same lower atmosphere: the cell fog (the game's fog, or the Volumetric Air fitted to it) is the
// weather's own measure of it, and the aerial perspective is the sky's medium (the clear air + the haze rung taken
// from the same fog numbers, atmosphere.cpp hazeFromFog). Laying one over the other counts the haze twice. So the
// combined transmittance is the MIN of the two: the aerial perspective is applied first, then the fog keeps only
// the share it has not already taken, T' = min(1, Tfog / Tap), and its in-scatter scaled by (1 - T') / (1 - Tfog).
// Where the fog is thicker (near, in a fog bank) the fog leads (bungo 10-19: our fog leads); where the clear air
// is thicker (past the fog's curve, the far LOD) the sky's air leads, continuous with the sky behind it.
// Red "double" (WW_ATMO_AP_RED=double): T' = Tfog, the haze counted twice. Offline gate: tests/spells/atmo_lut_check.py.
// Pure functions: no uniforms (the gate compiles this file alone).

// the fog's residual transmittance once the aerial perspective's tAp is in
float wwApFogResidualT( float tAp, float tFog, bool doubled )
{
	if ( doubled )
		return tFog;
	return clamp( tFog / max( tAp, 1e-6 ), 0.0, 1.0 );
}

// a medium's in-scatter L (its whole transmittance tFull) scaled to its residual tRes
vec3 wwApFogResidualL( vec3 L, float tFull, float tRes )
{
	if ( tRes >= 1.0 )
		return vec3( 0.0 );
	return L * clamp( ( 1.0 - tRes ) / max( 1.0 - tFull, 1e-6 ), 0.0, 1.0 );
}

// (L, T) of the surface's whole air: the aerial perspective (ap) then the fog's residual (fog = its L, T)
vec4 wwApFogCombine( vec4 ap, vec4 fog, bool doubled )
{
	float tr = wwApFogResidualT( ap.a, fog.a, doubled );
	vec3 lr = doubled ? fog.rgb : wwApFogResidualL( fog.rgb, fog.a, tr );
	return vec4( ap.rgb * tr + lr, ap.a * tr );
}
