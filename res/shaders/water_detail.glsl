/* lane FIX1101A (LATE ITEM 1, bungo 10-31: "make sure the water texture you use has the correct scale, and looks good";
 * LATE ITEM 2.3 "the fine ripple octave crisp near"). fo4_waterlook only (after water_refl.glsl and water_ocean.glsl).
 * MEASURED (tests/waterscale/watr_textures.json, from the game's textures): the sea's finest wave cascade is 15.3 m on
 * 128 texels (12 cm a texel: nothing under ~24 cm), and the record's own finest layer (ExtOceanWater NAM4
 * DefaultWaterTile, 217 u a tile on 512 texels) holds its slope energy at a median 37.6 texels = 23 cm: so on the sea
 * NOTHING drew the capillary / short-chop band (1-10 cm) the near water shows; the spectrum's tail there was roughness
 * only. Two octaves of the RECORD'S OWN finest texture (its NAM4, its uv scale / 4.1 and / 6.7, turned 37 and -71 deg
 * so the two lattices never line up) draw that tail near the camera: their median wavelengths 5.6 and 3.4 cm, their slope
 * normalised to the spectrum's unresolved variance (oc.var: no new slope energy), x the record's falloff (dfade) and the
 * spectrum's share. Where their mips keep the detail it leaves the roughness (keep); where the mips average it out it
 * stays roughness (LEAN). Red WW_CELL_WATER_OCEAN_RED=nodetail (waterOceanRed 4096): the spectrum alone, as before. */

const float kWwDetA = 4.1, kWwDetB = 6.7;	// the record's finest tile over these
const vec2 kWwDetRotA = vec2( 0.79864, 0.60181 ), kWwDetRotB = vec2( 0.32557, -0.94552 );	// cos, sin of 37 and -71 deg

vec2 wwWaterDetailUv( vec2 wxy, vec2 cs, float k, float tile, vec2 scroll )
{
	vec2 r = vec2( cs.x * wxy.x - cs.y * wxy.y, cs.y * wxy.x + cs.x * wxy.y );
	return r * ( k / tile ) + scroll * k;
}

/* the detail slope to add (slope space) and, in keep, the share of var it now draws explicitly (0..1).
 * wxy: world xy, tile: the record's finest layer's uv scale (u a tile), scroll its offset, var the spectrum's unresolved
 * slope variance, w the weight (dfade x the spectrum's share oc.w) */
vec2 wwWaterOceanDetail( vec2 wxy, float tile, vec2 scroll, float var, float w, out float keep )
{
	keep = 0.0;
	if ( ( waterOceanRed & 4096 ) != 0 || w <= 0.0 || var <= 0.0 || tile <= 0.0 )
		return vec2( 0.0 );
	float all = wwLayerVarAll( waterMoment2 );
	if ( all < 1e-4 )
		return vec2( 0.0 );	// the moments are not bound: no normalisation known
	vec2 uA = wwWaterDetailUv( wxy, kWwDetRotA, kWwDetA, tile, scroll ), uB = wwWaterDetailUv( wxy, kWwDetRotB, kWwDetB, tile, scroll );
	vec2 tA = texture( waterNoise2, uA ).xy, tB = texture( waterNoise2, uB ).xy;
	float lost = wwLayerVar( waterMoment2, uA, tA ) + wwLayerVar( waterMoment2, uB, tB );
	keep = clamp( 1.0 - lost / ( 2.0 * all ), 0.0, 1.0 ) * w * w;
	// back to world axes (the slope turns with its octave), each octave half the variance
	vec2 sA = tA * 2.0 - 1.0, sB = tB * 2.0 - 1.0;
	vec2 s = vec2( kWwDetRotA.x * sA.x + kWwDetRotA.y * sA.y, -kWwDetRotA.y * sA.x + kWwDetRotA.x * sA.y )
		+ vec2( kWwDetRotB.x * sB.x + kWwDetRotB.y * sB.y, -kWwDetRotB.y * sB.x + kWwDetRotB.x * sB.y );
	return s * ( inversesqrt( 2.0 * all ) * sqrt( var ) * w );
}
