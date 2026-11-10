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

// the unlit effects' unit: an unlit effect's linear colour x this (1 while the law's units are off or unset)
float fxLawUnit( bool additive )
{
	float uk = additive ? cellFxEmitK : cellFxUnlitK;
	return ( fxLaw.y > 0.5 && uk > 0.0 ) ? uk : 1.0;
}

// what a lit effect's colour is multiplied by: mix( the unlit unit, E, influence x the band weight )
vec3 fxLawMix( vec3 E, float influence, bool additive )
{
	return mix( vec3( fxLawUnit( additive ) ), E, influence * ( 1.0 - fxLaw.x ) );
}
#endif
