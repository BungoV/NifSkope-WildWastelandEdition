// Lane HORIZONBAND1 (FIX1031B, 2026-10-31; bungo's pictures: a saturated cyan strip with a HARD top edge along the
// horizon, notes/shots1031 sheet_1 / sheet_spot5). The Volumetric Air over the physical sky (cell_volsky.frag, the
// aerial perspective on) kept only its residual T' = min(1, Tfog / tSky) (atmo_apfog.glsl). Toward the horizon both are
// ~0 (1e-7 .. 1e-3 on Clear noon from 134 m) and their ratio moves by ~e^4 a degree, so the clamp switched the backdrop
// (the sky and the cloud layers) to the fog record's colour at infinity within ~0.2 deg: the hard edge, the clouds cut
// off under it, the cyan below (notes/fix1031b/sky: the twin puts the switch at +0.55 deg, the picture at +0.69).
// Rule 1 (shipped): a horizon band where the sky's own air is opaque, w = 1 - smoothstep(0, kSkyBandT, tSky). The cover
// is at least w (the air hides the backdrop toward the horizon and fades out by tSky = kSkyBandT, ~2 deg there), and
// its colour in the band is the aerial perspective's own colour at its far end (what far geometry fades to) in place of
// the fog record's, so the far terrain, the band and the sky above meet without a step. Where the fog leads high up
// (a foggy weather: w ~ 0) the cover and its colour are today's.
// Rule 0 = the red WW_ATMO_AP_RED=skyclamp: today's clamp and colour (cell_volsky.frag calls today's functions for it).
// tests/horizonband1 compiles this file through its own shim: scalars and vec3 only (no swizzles, no mix/smoothstep).

const float kSkyBandT = 0.03;

// the band's weight: 1 where the sky's own air is opaque, 0 from tSky = kSkyBandT up
float wwSkyBandW( float tSky )
{
	float t = clamp( tSky / kSkyBandT, 0.0, 1.0 );
	return 1.0 - t * t * ( 3.0 - 2.0 * t );
}

// the transmittance the backdrop keeps under the air: tFog the air's along the sky ray, tSky the sky's own
float wwSkyBandT( float tFog, float tSky, int rule )
{
	float tr = clamp( tFog / max( tSky, 1e-30 ), 0.0, 1.0 );
	if ( rule == 0 )
		return tr;
	return min( tr, 1.0 - wwSkyBandW( tSky ) );
}

// the cover's light, premultiplied by its coverage 1 - tr: fogL the air's light along the ray (its T tFog), apL / tAp
// the aerial perspective's light and T to its far end
vec3 wwSkyBandL( vec3 fogL, float tFog, vec3 apL, float tAp, float tSky, float tr, int rule )
{
	if ( tr >= 1.0 )
		return vec3( 0.0 );
	if ( rule == 0 )
		return fogL * clamp( ( 1.0 - tr ) / max( 1.0 - tFog, 1e-6 ), 0.0, 1.0 );
	float w = wwSkyBandW( tSky );
	vec3 fogInf = fogL * ( 1.0 / max( 1.0 - tFog, 1e-6 ) );
	vec3 hz = apL * ( 1.0 / max( 1.0 - tAp, 1e-4 ) );
	return ( fogInf * ( 1.0 - w ) + hz * w ) * ( 1.0 - tr );
}
