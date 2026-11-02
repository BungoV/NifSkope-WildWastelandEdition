// lane MATLOOK3: two material lobes FO4 and FO4CS do not draw for placed lights. Read only under the loader's defines
// WW_CELL_HAIRKK (row "Hair specular") and WW_CELL_CORNEA (row "Eye cornea"); src/gl/wwhaireye.h is the C++ twin,
// pinned by tests/haireye. Included by fo4_default.frag only, so effect / PBR programs keep their old code.
//
// HAIRKK1 (bHair, or a NIF Hair Tint shader): the spec lobe becomes a double-shifted Kajiya-Kay (Scheuermann's form,
// SkyrimCS Hair Specular): T = the strand (the UV v bitangent, made normal to N), n = 2^(gloss x 10 + 1) as the game's
// lobe, primary along T + 0.105 N (tan 6 deg = -2 alpha of Marschner's R, alpha 3 deg), secondary n / 2 along
// T - 0.158 N (tan 9 deg = 3 alpha, TRT) tinted by sqrt(albedo); Schlick F at IOR 1.55; x 1/4, clamp 15, x pi as
// cellSpecGame. The caller multiplies the light (N.L inside), mask and colour as before; the diffuse stays the game's.
//
// EYECORNEA1 (eye environment mapping, opaque draws only: eyewet / lashes are blended): a smooth dielectric coat on
// the GEOMETRIC normal (not the iris normal map): GGX, roughness 0.05, F0 of IOR 1.376 (0.025), Smith visibility,
// added per light unmasked and white; the iris spec under it takes (1 - F(N.V)). The diffuse stays the game's.

#ifdef WW_CELL_HAIRKK
#define WW_HAIRKK 1
uniform bool wwHairOn;	// the draw's material is hair (src/gl/renderer.cpp)
vec3 wwHairT = vec3( 0.0 );	// the strand, world (the cell path) or view (the viewport path)
bool wwHairLive = false;	// set per pixel by fo4_default.frag
vec3 wwHairAlb = vec3( 0.0 );	// linear albedo, the secondary lobe's tint

#ifdef WW_HAIRKK_RED
const float wwHairS1 = 0.0, wwHairS2 = 0.0;	// red: unshifted lobes
#else
const float wwHairS1 = 0.105, wwHairS2 = -0.158;
#endif
const float wwHairF0 = 0.046521;	// ( ( 1 - 1.55 ) / ( 1 + 1.55 ) )^2

float wwHairD( vec3 T, vec3 H, float n )
{
	float th = dot( T, H );
	return clamp( th + 1.0, 0.0, 1.0 ) * ( n + 2.0 ) * 0.159155 * pow( clamp( 1.0 - th * th, 0.0, 1.0 ), 0.5 * n );
}

// the strand from a tangent-frame vector and the shading normal
vec3 wwHairStrand( vec3 B, vec3 N )
{
	vec3 t = B - N * dot( N, B );
	float l = length( t );
	return l > 1e-5 ? t / l : B;
}

vec3 wwHairSpec( vec3 T, vec3 N, vec3 L, vec3 V, float gloss, vec3 albLin )
{
	float n = exp2( gloss * 10.0 + 1.0 );
	vec3 H = normalize( V + L );
	vec3 T1 = normalize( T + N * wwHairS1 );
	vec3 T2 = normalize( T + N * wwHairS2 );
	float f1 = 1.0 - clamp( dot( H, L ), 0.0, 1.0 );
	float f2 = f1 * f1;
	float F = wwHairF0 + ( 1.0 - wwHairF0 ) * f2 * f2 * f1;
	vec3 s = 0.25 * F * ( vec3( wwHairD( T1, H, n ) ) + wwHairD( T2, H, n * 0.5 ) * sqrt( max( albLin, vec3( 0.0 ) ) ) );
	return min( s, vec3( 15.0 ) ) * 3.141593;
}
#endif

#ifdef WW_CELL_CORNEA
#define WW_CORNEA 1
uniform bool wwEyeOn;	// the draw's material is an opaque eye (src/gl/renderer.cpp)
vec3 wwEyeN = vec3( 0.0 );	// the geometric normal, world (cell) or view (viewport)
bool wwEyeLive = false;
vec3 wwEyeAdd = vec3( 0.0 );	// the placed lights' coat, summed by cellSumLights

const float wwEyeF0 = 0.025043;	// ( ( 1.376 - 1 ) / ( 1.376 + 1 ) )^2
#ifdef WW_CORNEA_RED
const float wwEyeA = 0.25;	// red: a rough coat (alpha 0.25), no tight highlight
#else
const float wwEyeA = 0.0025;	// alpha = roughness 0.05 squared
#endif

float wwEyeFv( vec3 Ng, vec3 V )
{
	float f1 = 1.0 - clamp( dot( Ng, V ), 0.0, 1.0 );
	float f2 = f1 * f1;
	return wwEyeF0 + ( 1.0 - wwEyeF0 ) * f2 * f2 * f1;
}

// the coat's BRDF x pi (the game's light convention); the caller multiplies the light with N.L of ITS normal
float wwEyeCoat( vec3 Ng, vec3 L, vec3 V )
{
	float NdotL = dot( Ng, L );
	float NdotV = dot( Ng, V );
	if ( NdotL <= 0.0 || NdotV <= 0.0 )
		return 0.0;
	vec3 H = normalize( V + L );
	float NdotH = clamp( dot( Ng, H ), 0.0, 1.0 );
	float a2 = wwEyeA * wwEyeA;
	float d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	float D = a2 / ( 3.141593 * d * d );
	float vis = 0.5 / ( NdotL * sqrt( NdotV * NdotV * ( 1.0 - a2 ) + a2 ) + NdotV * sqrt( NdotL * NdotL * ( 1.0 - a2 ) + a2 ) );
	float f1 = 1.0 - clamp( dot( V, H ), 0.0, 1.0 );
	float f2 = f1 * f1;
	float F = wwEyeF0 + ( 1.0 - wwEyeF0 ) * f2 * f2 * f1;
	return min( D * vis * F, 15.0 ) * 3.141593;
}
#endif
