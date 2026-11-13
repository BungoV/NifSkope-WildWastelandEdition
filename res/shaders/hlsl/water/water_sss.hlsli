// HLSL source of features/water/water_sss.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane WATERLOOK1 step 4 (src/features/water/cellocean.h): the glow of sunlight through the wave crests, near and far alike.
// Sunlight entering the back of a wave scatters forward inside it and leaves through the face turned to the eye: the
// water's own body colour, brightest looking towards a low sun (Henyey-Greenstein, g 0.5: water's forward lobe at
// these angles, an engineering choice) and on faces tilted towards the eye. A pixel sees its resolved slope plus the
// slopes the spectrum's mips lost (Gaussian, the LEAN variance), so it takes the EXPECTED facing part,
// a Phi( a / sd ) + sd phi( a / sd ): near it is the slope's own face, far it becomes the spectrum's mean
// ( sd / sqrt( 2 pi ) ), the far water's colour, with no shimmer and no change in the mean between the two.
/* lane FIX1031B: the gain was 0.25, and bungo's 10-31 sea frame facing a 15.8 deg sun showed the glow as 0.75% of the
 * near water's light (mean +0.3 levels, crest faces p99 +2): nothing. Single scattering through a crest (b ~ 0.3 / m
 * coastal, ~0.5 m of water, the crests ~15% of the area, the HG lobe / 4 pi on the Lambert sun scale) puts the area
 * mean near 10x that: gain 2.5 (crest faces +9 levels at p90 in that frame). Red WW_CELL_WATER_OCEAN_RED=dimglow: 0.25. */
static const float kGlowGain = 2.5;	// the glow per unit facing slope at the forward peak / the sun x body colour

// the expected positive part of a + b, b ~ N( 0, sd^2 )
float wwPosMean( float a, float sd )
{
	if ( sd < 1e-6 )
		return max( a, 0.0 );
	// Phi( -|x| ) = 0.5 erfc( |x| / sqrt 2 ) (Abramowitz and Stegun 7.1.26), kept with phi's own exp so the tail stays
	// relative: sd ( phi( x ) + x Phi( x ) )
	float x = a / sd, z = abs( x ) * 0.7071068, t = 1.0 / ( 1.0 + 0.3275911 * z );
	float e = exp( -0.5 * x * x );
	float tail = 0.5 * t * ( 0.254829592 + t * ( -0.284496736 + t * ( 1.421413741 + t * ( -1.453152027 + t * 1.061405429 ) ) ) );
	float Phi = x < 0.0 ? tail * e : 1.0 - tail * e;
	return max( sd * ( 0.3989423 * e + x * Phi ), 0.0 );
}

// s: the pixel's slope as the normal's tilt ( N.xy / N.z, world ), var: the slope variance x + y it cannot show,
// V: eye to water (unit, world), L: to the sun (unit, world), sun: its radiance, body: the water's deep colour.
float3 wwWaterCrestGlow( float2 s, float var, float3 V, float3 L, float3 sun, float3 body )
{
	float h = length( V.xy );
	if ( h < 1e-4 )
		return (float3)( 0.0 );
	float a = dot( s, -V.xy / h );	// > 0: the face tilts towards the eye
	float c = wwPosMean( a, sqrt( 0.5 * max( var, 0.0 ) ) );
	const float g = 0.5;
	float hg = ( 1.0 - g * g ) / pow( 1.0 + g * g - 2.0 * g * clamp( dot( V, L ), -1.0, 1.0 ), 1.5 );	// / isotropic
	float gain = ( waterOceanRed & 512 ) != 0 ? 0.25 : kGlowGain;	// FIX1031B: red dimglow, the gain before
	return gain * c * hg * smoothstep( 0.0, 0.05, L.z ) * sun * body;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_sss.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kGlowGain = 2.5;
#endif

#if defined(WW_FN_wwPosMean) || defined(WW_FN_ALL)
float wwPosMean(float a, float sd)
{
    if (sd < 9.9999999747524270787835121154785e-07f)
    {
        return max(a, 0.0f);
    }
    // Phi( -|x| ) = 0.5 erfc( |x| / sqrt 2 ) (Abramowitz and Stegun 7.1.26), kept with phi's own exp so the tail stays
    // relative: sd ( phi( x ) + x Phi( x ) )
    float x = a / sd;
    float z = abs(x) * 0.7071068286895751953125f;
    float t = 1.0f / (1.0f + (0.3275910913944244384765625f * z));
    float e = exp(((-0.5f) * x) * x);
    float tail = (0.5f * t) * (0.254829585552215576171875f + (t * ((-0.284496724605560302734375f) + (t * (1.42141377925872802734375f + (t * ((-1.45315206050872802734375f) + (t * 1.0614054203033447265625f))))))));
    float PhiPick;
    if (x < 0.0f)
    {
        PhiPick = tail * e;
    }
    else
    {
        PhiPick = 1.0f - (tail * e);
    }
    float Phi = PhiPick;
    return max(sd * ((0.398942291736602783203125f * e) + (x * Phi)), 0.0f);
}
#endif

#if defined(WW_FN_wwWaterCrestGlow) || defined(WW_FN_ALL)
float3 wwWaterCrestGlow(float2 s, float var, float3 V, float3 L, float3 sun, float3 body)
{
    float h = length(V.xy);
    if (h < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xxx;
    }
    // > 0: the face tilts towards the eye
    float a = dot(s, (-V.xy) / h.xx);
    float PhiPick = a;
    float cPart = sqrt(0.5f * max(var, 0.0f));
    float c = wwPosMean(PhiPick, cPart);
    // / isotropic
    float hg = 0.75f / pow(1.25f - (1.0f * clamp(dot(V, L), -1.0f, 1.0f)), 1.5f);
    // FIX1031B: red dimglow, the gain before
    float gain = ((waterOceanRed & 512) != 0) ? 0.25f : 2.5f;
    return (sun * (((gain * c) * hg) * smoothstep(0.0f, 0.0500000007450580596923828125f, L.z))) * body;
}
#endif
#endif	// WW_HLSL_SRC
