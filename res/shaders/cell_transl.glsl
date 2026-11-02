// lane MATLOOK1 (TRANSLUC1): bTranslucency of a BGSM v8+ material (the FO76 / BGSM22 thin translucency), the law of
// FO4CS's AdvancedLighting port. Read only under WW_CELL_TRANSL (the loader's define for WW_CELL_TRANSL=1);
// src/gl/wwtransl.h is its C++ twin, pinned by tests/translucency.
//   amount = lighting map B x fTranslucencyTransmissiveScale, nothing at or under 0.001 or where the glow mask lights
//   tint   = bTranslucencyMixAlbedoWithSubsurfaceCol ? colour x albedo : colour
//   lobe   = max( mix( dot( L, -N ) x max( dot( V, -L ), 0 )^2, 0.5, turbulence quantised to 1/127 ), 0 )
//   a light gives the pixel max( T, D ): T = tint x amount x the light's colour (every factor of the diffuse but N.L)
//   x lobe, D = albedo x that light's diffuse; the excess max( T - D, 0 ) is added beside the diffuse.
// A thick object (bTranslucencyThickObject) takes nothing from the local lights; the sun reaches it only through a
// thickness shadow map, which NifSkope does not draw yet, so it takes nothing (as FO4CS does with that map absent).

uniform vec4 wwTrCol;	// the material's subsurface colour, a the transmissive scale (src/gl/renderer.cpp)
uniform vec4 wwTrFlags;	// x thick, y mix albedo, z on (a v8+ material with bTranslucency), w turbulence
uniform sampler2D WwTrLightMap;	// the lighting map (_l): B the transmission amount, A the glow mask; white when absent

vec4 wwTrRec = vec4( 0.0 );	// rgb tint x amount, a turbulence; set per pixel by the top shader before cellLit
float wwTrKind = 0.0;	// 0 none, 1 thin, 2 thick
bool wwTrLive = false;	// cellLit sets it around its own cellSumLights call only
vec3 wwTrAlb = vec3( 0.0 );	// the linear albedo cellLit lights
vec3 wwTrAdd = vec3( 0.0 );	// the placed lights' excess, summed by cellSumLights

void wwTrRecord( vec3 colour, bool mixAlbedo, vec3 albedoLin, float lightingMapB, float scale, float turbulence,
                 bool emissiveMasked, bool thick )
{
	float amount = lightingMapB * scale;
	wwTrRec = vec4( 0.0 );
	wwTrKind = 0.0;
	if ( emissiveMasked || !( amount > 0.001 ) )
		return;
	vec3 tint = mixAlbedo ? colour * albedoLin : colour;
	wwTrRec = vec4( max( tint * amount, vec3( 0.0 ) ), floor( clamp( turbulence, 0.0, 1.0 ) * 127.0 + 0.5 ) / 127.0 );
	wwTrKind = thick ? 2.0 : 1.0;
}

float wwTrLobe( vec3 N, vec3 L, vec3 V, float turbulence )
{
	float v = max( dot( V, -L ), 0.0 );
	return max( mix( dot( L, -N ) * v * v, 0.5, turbulence ), 0.0 );
}

// the excess of one light over the diffuse it already gave (thin records only)
vec3 wwTrExcess( vec3 N, vec3 L, vec3 V, vec3 lightColour, vec3 diffuseLit )
{
	if ( wwTrKind < 0.5 || wwTrKind > 1.5 )
		return vec3( 0.0 );
	return max( wwTrRec.rgb * lightColour * wwTrLobe( N, L, V, wwTrRec.a ) - diffuseLit, vec3( 0.0 ) );
}
