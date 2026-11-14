// HLSL source of land_wet.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// shore plane (land_shore.glsl, included first). Full wet at the waterline, none past a horizontal reach or a
	// height above the water; the height is the shore distance times the ground's slope (tan of its tilt), so a
	// gentle beach wets wide and a steep bank narrow. Porous ground darkens and gains a little gloss (Lagarde,
	// "Water drop 3b": base x mix(1, 0.2, porosity), gloss toward 1 by half that); a thin film near the line is
	// glossy on any material. Neutral is wet 0: the row off, the shore lookup off, or no water near.
	bool landWetOn;		// the Wet shore row
	int landWetRed;		// 1 noslope (the horizontal reach only), 2 nofilm, 4 nodark
	float4 landWetK;		// horizontal reach (u), vertical reach (u), wet porous base factor (linear), film reach (u)
#else
// lane WATERLOOK1 step 13 (WATERWET1): the wet band on the land beside baked water, from the land side of the


// (wet, film) at world point P with the ground's world normal N (geometric, not the normal map's)
float2 wwLandWet( float3 P, float3 N )
{
	if ( or( !landWetOn, !landShoreOn ) )
		return (float2)( 0.0 );
	float d = wwLandShoreSmooth( P.xy );
	if ( d >= landWetK.x )
		return (float2)( 0.0 );
	float nz = clamp( N.z, 0.05, 1.0 );
	float h = d * sqrt( 1.0 - nz * nz ) / nz;	// the height above the water, along the slope
	float wv = ( landWetRed & 1 ) != 0 ? 1.0 : 1.0 - smoothstep( 0.0, landWetK.y, h );
	float wet = ( 1.0 - smoothstep( 0.0, landWetK.x, d ) ) * wv;
	float film = ( landWetRed & 2 ) != 0 ? 0.0 : ( 1.0 - smoothstep( 0.0, landWetK.w, d ) ) * wv;
	return float2( wet, film );
}

// The linear base factor at wetness w and porosity por (1 = dry or the darkening red)
float wwLandWetBase( float w, float por )
{
	return ( landWetRed & 4 ) != 0 ? 1.0 : lerp( 1.0, landWetK.z, por * w );
}

// Perceptual roughness wet: porous ground halfway toward the base factor's gloss, then the film toward water's
float wwLandWetRough( float rough, float2 wet, float por )
{
	float r = rough * lerp( 1.0, lerp( 1.0, landWetK.z, por ), 0.5 * wet.x );
	return lerp( r, 0.05, wet.y );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/land_wet.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwLandWet) || defined(WW_FN_ALL)
float2 wwLandWet(float3 P, float3 N)
{
    if ((!landWetOn) || (!landShoreOn))
    {
        return 0.0f.xx;
    }
    float2 dPart = P.xy;
    float d = wwLandShoreSmooth(dPart);
    if (d >= landWetK.x)
    {
        return 0.0f.xx;
    }
    float nz = clamp(N.z, 0.0500000007450580596923828125f, 1.0f);
    // the height above the water, along the slope
    float h = (d * sqrt(1.0f - (nz * nz))) / nz;
    float wvPick;
    if ((landWetRed & 1) != 0)
    {
        wvPick = 1.0f;
    }
    else
    {
        wvPick = 1.0f - smoothstep(0.0f, landWetK.y, h);
    }
    float wv = wvPick;
    float wet = (1.0f - smoothstep(0.0f, landWetK.x, d)) * wv;
    float filmPick;
    if ((landWetRed & 2) != 0)
    {
        filmPick = 0.0f;
    }
    else
    {
        filmPick = (1.0f - smoothstep(0.0f, landWetK.w, d)) * wv;
    }
    float film = filmPick;
    return float2(wet, film);
}
#endif

#if defined(WW_FN_wwLandWetBase) || defined(WW_FN_ALL)
float wwLandWetBase(float w, float por)
{
    float dPart;
    if ((landWetRed & 4) != 0)
    {
        dPart = 1.0f;
    }
    else
    {
        dPart = lerp(1.0f, landWetK.z, por * w);
    }
    return dPart;
}
#endif

#if defined(WW_FN_wwLandWetRough) || defined(WW_FN_ALL)
float wwLandWetRough(float rough, float2 wet, float por)
{
    float r = rough * lerp(1.0f, lerp(1.0f, landWetK.z, por), 0.5f * wet.x);
    return lerp(r, 0.0500000007450580596923828125f, wet.y);
}
#endif
#endif	// WW_HLSL_SRC
