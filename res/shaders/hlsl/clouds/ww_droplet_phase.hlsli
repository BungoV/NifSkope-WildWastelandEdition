// HLSL source of features/clouds/ww_droplet_phase.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane CLOUD2: the droplet phase for clouds and fog (Jendersie & d'Eon 2023), HG + Draine blend. prm = (gHG, gD,
// alpha, wD) from src/features/clouds/cloudvolmath.h dropletParams(d); k scales both g (the octaves' c^i). Twin: dropletPhase.

float wwHg( float g, float c )
{
	return ( 1.0 - g * g ) / ( 12.5663706 * pow( max( 1e-4, 1.0 + g * g - 2.0 * g * c ), 1.5 ) );
}

float wwDropletPhase( float4 prm, float c, float k )
{
	float gh = prm.x * k, gd = prm.y * k;
	float dr = wwHg( gd, c ) * ( 1.0 + prm.z * c * c ) / ( 1.0 + prm.z * ( 1.0 + 2.0 * gd * gd ) / 3.0 );
	return ( 1.0 - prm.w ) * wwHg( gh, c ) + prm.w * dr;
}

// lane CLOUDRECX1: the ice phase (clouds.iceFraction, SPEC.md 1.4): the roughened crystals' body (HG 0.75), the smooth
// faces' forward peak (HG 0.98, 10 %) and the 22 deg halo (a 1 deg Gaussian ring, 3 %); k as the octaves'. Twin and
// sources: src/features/clouds/cloudrecx14math.h icePhase.
float wwIcePhase( float c, float k )
{
	float a = acos( clamp( c, -1.0, 1.0 ) ) - 0.383972;
	float ring = exp( -( a * a ) / ( 0.0174533 * 0.0174533 ) ) / 0.0728124;
	return 0.87 * wwHg( 0.75 * k, c ) + 0.10 * wwHg( 0.98 * k, c ) + 0.03 * ( k * ring + ( 1.0 - k ) / 12.5663706 );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/clouds/ww_droplet_phase.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl

#if defined(WW_FN_wwHg) || defined(WW_FN_ALL)
float wwHg(float g, float c)
{
    return (1.0f - (g * g)) / (12.56637096405029296875f * pow(max(9.9999997473787516355514526367188e-05f, (1.0f + (g * g)) - ((2.0f * g) * c)), 1.5f));
}
#endif

#if defined(WW_FN_wwDropletPhase) || defined(WW_FN_ALL)
float wwDropletPhase(float4 prm, float c, float k)
{
    float gh = prm.x * k;
    float gd = prm.y * k;
    float drPart2 = gd;
    float drPart = c;
    float dr = (wwHg(drPart2, drPart) * (1.0f + ((prm.z * c) * c))) / (1.0f + ((prm.z * (1.0f + ((2.0f * gd) * gd))) / 3.0f));
    float wwDropletPhaseRet2 = gh;
    float wwDropletPhaseRet = c;
    return ((1.0f - prm.w) * wwHg(wwDropletPhaseRet2, wwDropletPhaseRet)) + (prm.w * dr);
}
#endif

#if defined(WW_FN_wwIcePhase) || defined(WW_FN_ALL)
float wwIcePhase(float c, float k)
{
    float a = acos(clamp(c, -1.0f, 1.0f)) - 0.383971989154815673828125f;
    float ring = exp((-(a * a)) / 0.00030461768619716167449951171875f) / 0.072812400758266448974609375f;
    float drPart2 = 0.75f * k;
    float drPart = c;
    float wwDropletPhaseRet2 = 0.980000019073486328125f * k;
    float wwDropletPhaseRet = c;
    return ((0.87000000476837158203125f * wwHg(drPart2, drPart)) + (0.100000001490116119384765625f * wwHg(wwDropletPhaseRet2, wwDropletPhaseRet))) + (0.02999999932944774627685546875f * ((k * ring) + ((1.0f - k) / 12.56637096405029296875f)));
}
#endif
#endif	// WW_HLSL_SRC
