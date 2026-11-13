// HLSL source of features/water/water_detail.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
/* lane FIX1101A (LATE ITEM 1, bungo 10-31: "make sure the water texture you use has the correct scale, and looks good";
 * LATE ITEM 2.3 "the fine ripple octave crisp near"). fo4_waterlook only (after water_refl.glsl and water_ocean.glsl).
 * MEASURED (tests/features/water/waterscale/watr_textures.json, from the game's textures): the sea's finest wave cascade is 15.3 m on
 * 128 texels (12 cm a texel: nothing under ~24 cm), and the record's own finest layer (ExtOceanWater NAM4
 * DefaultWaterTile, 217 u a tile on 512 texels) holds its slope energy at a median 37.6 texels = 23 cm: so on the sea
 * NOTHING drew the capillary / short-chop band (1-10 cm) the near water shows; the spectrum's tail there was roughness
 * only. Two octaves of the RECORD'S OWN finest texture (its NAM4, its uv scale / 4.1 and / 6.7, turned 37 and -71 deg
 * so the two lattices never line up) draw that tail near the camera: their median wavelengths 5.6 and 3.4 cm, their slope
 * normalised to the spectrum's unresolved variance (oc.var: no new slope energy), x the record's falloff (dfade) and the
 * spectrum's share. Where their mips keep the detail it leaves the roughness (keep); where the mips average it out it
 * stays roughness (LEAN). Red WW_CELL_WATER_OCEAN_RED=nodetail (waterOceanRed 4096): the spectrum alone, as before. */

static const float kWwDetA = 4.1, kWwDetB = 6.7;	// the record's finest tile over these
static const float2 kWwDetRotA = float2( 0.79864, 0.60181 ), kWwDetRotB = float2( 0.32557, -0.94552 );	// cos, sin of 37 and -71 deg

float2 wwWaterDetailUv( float2 wxy, float2 cs, float k, float tile, float2 scroll )
{
	float2 r = float2( cs.x * wxy.x - cs.y * wxy.y, cs.y * wxy.x + cs.x * wxy.y );
	return r * ( k / tile ) + scroll * k;
}

/* the detail slope to add (slope space) and, in keep, the share of var it now draws explicitly (0..1).
 * wxy: world xy, tile: the record's finest layer's uv scale (u a tile), scroll its offset, var the spectrum's unresolved
 * slope variance, w the weight (dfade x the spectrum's share oc.w) */
float2 wwWaterOceanDetail( float2 wxy, float tile, float2 scroll, float var, float w, out float keep )
{
	keep = 0.0;
	if ( or( or( or( ( waterOceanRed & 4096 ) != 0, w <= 0.0 ), var <= 0.0 ), tile <= 0.0 ) )
		return (float2)( 0.0 );
	float all = wwLayerVarAll( waterMoment2, _waterMoment2_sampler );
	if ( all < 1e-4 )
		return (float2)( 0.0 );	// the moments are not bound: no normalisation known
	float2 uA = wwWaterDetailUv( wxy, kWwDetRotA, kWwDetA, tile, scroll ), uB = wwWaterDetailUv( wxy, kWwDetRotB, kWwDetB, tile, scroll );
	float2 tA = waterNoise2.Sample( _waterNoise2_sampler, uA ).xy, tB = waterNoise2.Sample( _waterNoise2_sampler, uB ).xy;
	float lost = wwLayerVar( waterMoment2, _waterMoment2_sampler, uA, tA ) + wwLayerVar( waterMoment2, _waterMoment2_sampler, uB, tB );
	keep = clamp( 1.0 - lost / ( 2.0 * all ), 0.0, 1.0 ) * w * w;
	// back to world axes (the slope turns with its octave), each octave half the variance
	float2 sA = tA * 2.0 - 1.0, sB = tB * 2.0 - 1.0;
	float2 s = float2( kWwDetRotA.x * sA.x + kWwDetRotA.y * sA.y, -kWwDetRotA.y * sA.x + kWwDetRotA.x * sA.y )
		+ float2( kWwDetRotB.x * sB.x + kWwDetRotB.y * sB.y, -kWwDetRotB.y * sB.x + kWwDetRotB.x * sB.y );
	return s * ( rsqrt( 2.0 * all ) * sqrt( var ) * w );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_detail.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterDetailUv) || defined(WW_FN_ALL)
float2 wwWaterDetailUv(float2 wxy, float2 cs, float k, float tile, float2 scroll)
{
    float2 r = float2((cs.x * wxy.x) - (cs.y * wxy.y), (cs.y * wxy.x) + (cs.x * wxy.y));
    return (r * (k / tile)) + (scroll * k);
}
#endif

#if defined(WW_FN_wwWaterOceanDetail) || defined(WW_FN_ALL)
float2 wwWaterOceanDetail(float2 wxy, float tile, float2 scroll, float var, float w, out float keep)
{
    keep = 0.0f;
    bool wwh0 = ((((waterOceanRed & 4096) != 0) || (w <= 0.0f)) || (var <= 0.0f));
    bool wwh1 = (tile <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 0.0f.xx;
    }
    float _all = wwLayerVarAll(waterMoment2, _waterMoment2_sampler);
    if (_all < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xx;
    }
    float2 uAPart5 = wxy;
    float2 uAPart4 = float2(0.7986400127410888671875f, 0.601809978485107421875f);
    float uAPart3 = 4.099999904632568359375f;
    float uAPart2 = tile;
    float2 uAPart = scroll;
    float2 uA = wwWaterDetailUv(uAPart5, uAPart4, uAPart3, uAPart2, uAPart);
    float2 uBPart5 = wxy;
    float2 uBPart4 = float2(0.32556998729705810546875f, -0.945519983768463134765625f);
    float uBPart3 = 6.69999980926513671875f;
    float uBPart2 = tile;
    float2 uBPart = scroll;
    float2 uB = wwWaterDetailUv(uBPart5, uBPart4, uBPart3, uBPart2, uBPart);
    float2 tA = waterNoise2.Sample(_waterNoise2_sampler, uA).xy;
    float2 tB = waterNoise2.Sample(_waterNoise2_sampler, uB).xy;
    float2 lostPart4 = uA;
    float2 lostPart3 = tA;
    float2 lostPart2 = uB;
    float2 lostPart = tB;
    float lost = wwLayerVar(waterMoment2, _waterMoment2_sampler, lostPart4, lostPart3) + wwLayerVar(waterMoment2, _waterMoment2_sampler, lostPart2, lostPart);
    keep = (clamp(1.0f - (lost / (2.0f * _all)), 0.0f, 1.0f) * w) * w;
    // back to world axes (the slope turns with its octave), each octave half the variance
    float2 sA = (tA * 2.0f) - 1.0f.xx;
    float2 sB = (tB * 2.0f) - 1.0f.xx;
    float2 s = float2((0.7986400127410888671875f * sA.x) + (0.601809978485107421875f * sA.y), ((-0.601809978485107421875f) * sA.x) + (0.7986400127410888671875f * sA.y)) + float2((0.32556998729705810546875f * sB.x) + ((-0.945519983768463134765625f) * sB.y), (0.945519983768463134765625f * sB.x) + (0.32556998729705810546875f * sB.y));
    return s * ((rsqrt(2.0f * _all) * sqrt(var)) * w);
}
#endif
#endif	// WW_HLSL_SRC
