// lane WATERLOOK1 step 13 (WATERWET1): the wet band on the land beside baked water, from the land side of the
// shore plane (land_shore.glsl, included first). Full wet at the waterline, none past a horizontal reach or a
// height above the water; the height is the shore distance times the ground's slope (tan of its tilt), so a
// gentle beach wets wide and a steep bank narrow. Porous ground darkens and gains a little gloss (Lagarde,
// "Water drop 3b": base x mix(1, 0.2, porosity), gloss toward 1 by half that); a thin film near the line is
// glossy on any material. Neutral is wet 0: the row off, the shore lookup off, or no water near.
uniform bool landWetOn;		// the Wet shore row
uniform int landWetRed;		// 1 noslope (the horizontal reach only), 2 nofilm, 4 nodark
uniform vec4 landWetK;		// horizontal reach (u), vertical reach (u), wet porous base factor (linear), film reach (u)

// (wet, film) at world point P with the ground's world normal N (geometric, not the normal map's)
vec2 wwLandWet( vec3 P, vec3 N )
{
	if ( !landWetOn || !landShoreOn )
		return vec2( 0.0 );
	float d = wwLandShoreSmooth( P.xy );
	if ( d >= landWetK.x )
		return vec2( 0.0 );
	float nz = clamp( N.z, 0.05, 1.0 );
	float h = d * sqrt( 1.0 - nz * nz ) / nz;	// the height above the water, along the slope
	float wv = ( landWetRed & 1 ) != 0 ? 1.0 : 1.0 - smoothstep( 0.0, landWetK.y, h );
	float wet = ( 1.0 - smoothstep( 0.0, landWetK.x, d ) ) * wv;
	float film = ( landWetRed & 2 ) != 0 ? 0.0 : ( 1.0 - smoothstep( 0.0, landWetK.w, d ) ) * wv;
	return vec2( wet, film );
}

// The linear base factor at wetness w and porosity por (1 = dry or the darkening red)
float wwLandWetBase( float w, float por )
{
	return ( landWetRed & 4 ) != 0 ? 1.0 : mix( 1.0, landWetK.z, por * w );
}

// Perceptual roughness wet: porous ground halfway toward the base factor's gloss, then the film toward water's
float wwLandWetRough( float rough, vec2 wet, float por )
{
	float r = rough * mix( 1.0, mix( 1.0, landWetK.z, por ), 0.5 * wet.x );
	return mix( r, 0.05, wet.y );
}
