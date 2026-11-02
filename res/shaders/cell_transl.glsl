// lane MATLOOK1 (TRANSLUC1): bTranslucency of a BGSM v8+ material (the FO76 / BGSM22 thin translucency), the law of
// FO4CS's AdvancedLighting port. Read only under WW_CELL_TRANSL (the loader's define for WW_CELL_TRANSL=1);
// src/gl/wwtransl.h is its C++ twin, pinned by tests/translucency.
//   amount = lighting map B x fTranslucencyTransmissiveScale, nothing at or under 0.001 or where the glow mask lights
//   tint   = bTranslucencyMixAlbedoWithSubsurfaceCol ? colour x albedo : colour
//   lobe   = max( mix( dot( L, -N ) x max( dot( V, -L ), 0 )^2, 0.5, turbulence quantised to 1/127 ), 0 )
//   a light gives the pixel max( T, D ): T = tint x amount x the light's colour (every factor of the diffuse but N.L)
//   x lobe, D = albedo x that light's diffuse; the excess max( T - D, 0 ) is added beside the diffuse.
// A thick object (bTranslucencyThickObject) takes nothing from the local lights; the sun reaches it only through a
// thickness shadow (lane MATLOOK3, TRANSLUC1-THICK: wwTrThickSun below, the exterior sun's cascades).

uniform vec4 wwTrCol;	// the material's subsurface colour, a the transmissive scale (src/gl/renderer.cpp)
uniform vec4 wwTrFlags;	// x thick, y mix albedo, z on (a v8+ material with bTranslucency), w turbulence
#ifdef WW_BINDLESS
// lane MATLOOK2 (TRANSLUC1-BINDLESS): the twin reads the lighting map's handle from the draw's row, slot 7 (cellbindless.h)
#define WwTrLightMap sampler2D( wwDraws[wwDrawIdx].tex[7] )
#else
uniform sampler2D WwTrLightMap;	// the lighting map (_l): B the transmission amount, A the glow mask; white when absent
#endif

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

#if defined( WW_SUNSHADOW ) && !defined( WW_TRV ) && !defined( WW_CELL_TRANSL_RED_NOTHICK )
/* lane MATLOOK3 (TRANSLUC1-THICK): FO4CS's sun thickness shadow, visibility = saturate( ( occ - ( recv - 0.6 ) ) / 0.6 ),
 * 1 at no thickness, 0 at 0.6 units of the caster between the sun and the pixel. FO4CS draws the thick casters into a
 * map of their own; here the sun's cascade map stands in (a ruling: any caster counts, not only thick ones). One
 * comparison tap at receiver offsets base + 0.6 (k + 0.5) / N is lit when the thickness is under that offset, so the
 * lit share of the N taps is the linear visibility (within 1 / 2N). Nearer cascade only; outside the cascades'
 * reach, or with no cascade map, a thick object takes nothing (the old rule). Red WW_CELL_TRANSL_RED=nothick. */
#define WW_TR_THICK 1
#define WW_TR_THICK_TAPS 8
float wwTrThickVis( vec3 posView )
{
	if ( ( ussOn != 0 && ussCsmOn == 0 ) || !( -posView.z * csmParams.x <= csmParams.y ) )
		return 0.0;
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	vec3 q = csmUvd( a, posView );
	float base = ( ussOn != 0 && ussCsmFilter.x > 0.5 ) || ussCsmStable.w != 0.0 ? ussCsmBias[a] : csmSplitOffset.z;
	float lit = 0.0;
	for ( int k = 0; k < WW_TR_THICK_TAPS; k++ ) {
		float o = base + 0.6 * ( float( k ) + 0.5 ) / float( WW_TR_THICK_TAPS );
		lit += texture( csmMap, vec4( q.xy, float( a ), q.z - o * csmInvRange[a] ) );
	}
	return lit / float( WW_TR_THICK_TAPS );
}

// the sun's share of a thick pixel: max( T x thickness visibility, the shadowed diffuse ) less that diffuse
vec3 wwTrThickSun( vec3 N, vec3 L, vec3 V, vec3 lightColour, vec3 shadowedDiffuse, vec3 posView )
{
	if ( wwTrKind < 1.5 )
		return vec3( 0.0 );
	vec3 T = wwTrRec.rgb * lightColour * wwTrLobe( N, L, V, wwTrRec.a );
	return max( T * wwTrThickVis( posView ) - shadowedDiffuse, vec3( 0.0 ) );
}
#endif
