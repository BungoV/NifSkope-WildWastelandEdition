#ifndef WW_FXLAW_GLSL
#define WW_FXLAW_GLSL
/* lane MODULES1 M3 (G7, the Fusion Shaders Effect Lighting law; src/features/lights/cellfxlit.cpp wwCellFxLawUniforms):
 * a lit effect's material is a receiver or an emitter by its brightness, max( base colour ) x base colour scale: at or
 * under 1.0 lit, at or over 2.0 emissive (never lit, never darkened), between them graded (its lighting influence x
 * ( 1 - smoothstep )). The unlit share of the influence mix is on the unlit effects' unit: it was 1 while the lit share
 * took real units, so a smoke of influence 0.25 kept a quarter of its light (the fire barrel's black puffs).
 * fxLaw: x the emissive share, 1 - the band weight; y 1 = the unlit share on the unlit unit. (0, 0) = before the law. */
uniform vec2 fxLaw;
uniform float cellFxEmitK;	// lane PHYSUNITS1: an unlit additive effect's cd/m2 per unit (0 = unset = 1)
uniform float cellFxUnlitK;	// lane PHYSUNITS1: an unlit blended effect on the light's scale (0 = 1)
/* MODULES1 R7-1: the local light of a lit effect card / particle outdoors. rgb = the surfaces' sun or moon this frame
 * (the weather's Sunlight row x the surfaces' scale), a > 0.5 = the local law is on: the card's base is that sun under
 * its shadow + the weather's ambient at the card (cellDalc), in place of the game's one Effect Lighting colour, which
 * knows neither the hour's ambient nor a shadow. Additive cards keep the game's base. Red WW_CELL_FXLOCAL_RED=base. */
uniform vec4 vpSun;
float fxLocalVis = 1.0;	// the sun's shadow at the card, set by the caller before cellFxLit
bool fxLocalOn()
{
	return vpSun.a > 0.5;
}

/* MODULES1 R8-1 (bungo: "the fog here should be dark and match the environment lighting"): round 7 gave the card the
 * WHOLE weather ambient (the open sky's DALC). A surface at the same place keeps only (1 - k) of it, k = the share the
 * GI volume's sky replaces (cellGiSkyK), and takes the volume's irradiance for the rest (cellGiE / pi: the sky that
 * really reaches the spot + the lamps' bounce, in its own colour). In a night street k is 1, so the card held 0.03
 * where the walls held 0.001: a grey veil. fxLocalSky sets the card's share (fxAmbShare, cell_lights.glsl) and its GI
 * (fxLocalGi) once a pixel; the lamps stay per pixel with their shadows. fxLocalRed bit 1 = round 7's rule
 * (WW_CELL_FXLOCAL_RED=ambient); an exe that does not set it runs the new rule.
 * The law's lit band is a receiver: the game mixes mix( colour, colour x light, lighting influence ), so a smoke with
 * influence 0.25 (the fire barrel's: 63 / 255) kept 75 % of its own colour unlit, a grey plume in a dark street and
 * no fall with distance from the fire. Such a blended card or particle outdoors now takes the scene's light with
 * weight = the band alone (fxLawRecv). fxLocalRed bit 2 = the influence again (WW_CELL_FXLOCAL_RED=influence). */
#ifdef WW_FX_AMBSHARE
uniform int fxLocalRed;
vec3 fxLocalGi = vec3( 0.0 );	// the GI volume's light on the card (linear, the ambient's unit), added to its lit sum
float fxLawRecv = 0.0;	// 1: the lighting influence reads 1 (a receiver), set per pixel by fxLocalSky
void fxLocalSky( vec3 P, bool additive )
{
	fxAmbShare = 1.0;
	fxLocalGi = vec3( 0.0 );
	fxLawRecv = 0.0;
	if ( !fxLocalOn() || cellInterior || additive )
		return;
	if ( ( fxLocalRed & 2 ) == 0 )
		fxLawRecv = 1.0;
	if ( ( fxLocalRed & 1 ) != 0 )
		return;
	const vec3 up = vec3( 0.0, 0.0, 1.0 );
	fxAmbShare = 1.0 - cellGiSkyK( P, up );
	if ( CELL_GI_UP )
		fxLocalGi = cellGiE( P, up ) * 0.31830989;
}
// the GI's part of a lit effect's multiplier (fxLawMix is affine in E: fxLawMix( E + G ) = fxLawMix( E ) + this)
vec3 fxLawAdd( vec3 G, float influence )
{
	return G * ( max( influence, fxLawRecv ) * ( 1.0 - fxLaw.x ) );
}
#define FX_LOCAL_GI + fxLocalGi
#define FX_LOCAL_GI_LAW( i ) + fxLawAdd( fxLocalGi, i )
#else
// the decal programs sit at the driver's instruction limit: no GI term in their text
#define FX_LOCAL_GI
#define FX_LOCAL_GI_LAW( i )
#endif

// the unlit effects' unit: an unlit effect's linear colour x this (1 while the law's units are off or unset)
float fxLawUnit( bool additive )
{
	float uk = additive ? cellFxEmitK : cellFxUnlitK;
	return ( fxLaw.y > 0.5 && uk > 0.0 ) ? uk : 1.0;
}

// what a lit effect's colour is multiplied by: mix( the unlit unit, E, influence x the band weight )
vec3 fxLawMix( vec3 E, float influence, bool additive )
{
#ifdef WW_FX_AMBSHARE
	influence = max( influence, fxLawRecv );	// R8-1: a receiver outdoors (fxLocalSky)
#endif
	return mix( vec3( fxLawUnit( additive ) ), E, influence * ( 1.0 - fxLaw.x ) );
}
#endif
