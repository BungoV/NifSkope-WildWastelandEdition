// lane VOLPART1: the volumetric law shared by particles.frag (a sprite) and fo4_effectshader.frag (a lit effect
// card, as the street steam): included once, after cell_lights.glsl, lookdev_fog.glsl, lightingInfluence and fxLitMode.
/* lane VOLPART1: VOLUMETRIC PARTICLES (src/gl/glparticles.cpp; the row "Volumetric Particles" ships off). A puff of
 * steam or fog is a little piece of the air: lit by the light that is really there, from the very list the air
 * (Volumetric Air, cell_volair.frag) reads -- the directional light x its cascade shadow, every placed light (colour x
 * radial curve x cone x its shadow cube) x the air's transmittance to it, the GI and the ambient as six-axis cubes --
 * evaluated DIRECTLY per pixel at the sprite's own point, never from the integrated volume. Then it is seen through
 * the air between it and the eye (the INTEGRATED froxel volume, as every surface: colour x T + L), so steam and air
 * fade alike.
 *   thin   the light the medium scatters toward the eye, the air's phase: per unit opacity, the air's own J
 *   thick  a wrapped diffuse at the puff's normal (the normal map turned by the sprite's rotation, else a sphere over
 *          the sprite): max(0, N.L + w) / (1 + w) per light, the cubes read at N
 *   alpha as thickness: the pixel's opacity a is 1 - exp(-tau), so its single-scatter share is exp(-tau) = 1 - a;
 *          lit = (1 - a) thin + a thick, wrap w = 1 - a (a thin edge lets the light round it)
 *   soft   the sprite is a ball of half-depth h = r sqrt(1 - |q|^2) round its plane; the share of it in front of the
 *          scene (the opaque pass's depth) is its opacity: no hard line where a card meets the ground or a wall
 *   phase  every light the puff scatters goes through the air's phase (its Rayleigh + forward / back HG lobes, the very
 *          volPhasePi the air reads): the thin share takes pi x p(c), the thick share its wrap x p(c) / p_iso, so a
 *          puff with a light behind it glows and one lit from the eye's side reads duller (p_iso = the weights / 4)
 * A lit system only (the effect's lighting flag, as the game); lightingInfluence mixes from 1 as before. An additive
 * one keeps the effect law (it emits) and takes the air's transmittance alone.
 *
 * A LIT EFFECT CARD (the street's ground steam, fo4_effectshader.frag) is a puff too, a fake volume:
 *   puff   the card is a ball of its own size: the view ray's path through it at the card point q (texCoord over
 *          -1..1) is l = sqrt(1 - |q|^2) of the centre's, so the opacity a = 1 - (1 - a0)^(1.5 l) (the 1.5 keeps the
 *          card's mean optical depth: the mean of l over the disc is 2/3): dense in the middle, thin at the rim
 *   edge   a card turned edge-on fades: x smoothstep(VP_EDGE0, VP_EDGE1, |N.V|) at its face normal (no vertical sheets)
 *   near   a card fades by its depth from the eye: x smoothstep(VP_NEAR0, VP_NEAR1, depth in game units), so walking
 *          into steam never cuts a card across the screen
 *   soft   the slab of the card's soft depth against the opaque pass's depth (vpCardSoft), drawn with no depth test
 * The puff's opacity is its thickness (the light's thin / thick share); edge, near and soft are coverage alone.
 * VOLPART1R (on the landed VOLAIR1): the medium is the air's layered law (its near band and ground layer), and the sky's
 * share (the directional light, the GI, the ambient) takes the records' haze colour as the air's does (airEnvColour,
 * red physcol the physical colour); the placed lights keep their own colours. The far grid's terrain shadow (Far Air,
 * ships off) is not mirrored: a puff takes the cascades alone. */
// the cascade variants (WW_SUNSHADOW) have ww_sunshadow.glsl already (the loader takes no comment after an #include)
#ifndef WW_SUNSHADOW
#include "ww_sunshadow.glsl"
#endif
// SKY3: a puff reads the air's own filled volume as the air pass does, not the grid's shell (red WW_CELL_PARTICLES_AIR_RED=1)
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1
#endif
#include "cell_airmed.glsl"
uniform int volPart;			// 0 off: the law above, as before; 1 on
uniform int volPartRed;			// gates only: 1 flat, 2 nothick, 4 nosoft, 8 nolights, 16 white, 32 nofog, 64 noshadow,
								// 128 noedge, 256 nophase, 512 nonear
uniform int volPartDump;		// gates only: the probe row's width in pixels (particles.geom), 0 off
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform int volTerms;			// the air's mask (WW_CELL_AIR_TERMS): 1 directional, 2 placed, 4 GI, 8 ambient

const float VP_EDGE0 = 0.05, VP_EDGE1 = 0.35;	// |N.V| a card is gone at / whole from
const float VP_NEAR0 = 16.0, VP_NEAR1 = 128.0;	// game units from the eye a card is gone at / whole from

// pi x the air's phase at c; red nophase: isotropic (the weights / 4)
vec3 vpPhase( float c )
{
	return ( volPartRed & 256 ) != 0 ? volGi.rgb * 0.25 : volPhasePi( c );
}

// the phase over its isotropic value (1 on average over the sphere)
vec3 vpPhaseN( float c )
{
	return vpPhase( c ) / max( volGi.rgb * 0.25, vec3( 1e-6 ) );
}

float vpEdgeFade( float nv )
{
	return ( volPartRed & 128 ) != 0 ? 1.0 : smoothstep( VP_EDGE0, VP_EDGE1, nv );
}

float vpNearFade( float depth )
{
	return ( volPartRed & 512 ) != 0 ? 1.0 : smoothstep( VP_NEAR0, VP_NEAR1, depth );
}

// the card's opacity at q (-1..1 over it) from its own a0; red nothick: a0 as drawn
float vpPuffAlpha( float a0, vec2 q )
{
	if ( ( volPartRed & 2 ) != 0 )
		return a0;
	float l = sqrt( max( 1.0 - dot( q, q ), 0.0 ) );
	return 1.0 - pow( 1.0 - clamp( a0, 0.0, 0.999 ), 1.5 * l );
}

vec3 vpTint( vec3 e )
{
	return ( volPartRed & 16 ) != 0 ? vec3( dot( e, vec3( 0.2126, 0.7152, 0.0722 ) ) ) : e;	// red white: grey light
}

float vpWrap( float nl, float w )
{
	return max( nl + w, 0.0 ) / ( 1.0 + w );
}

// the six-axis cube at N: the sum of n_a^2 over the axes N faces
vec3 vpCubeAt( vec3 C[6], vec3 N )
{
	vec3 n2 = N * N;
	return n2.x * ( N.x >= 0.0 ? C[0] : C[1] ) + n2.y * ( N.y >= 0.0 ? C[2] : C[3] ) + n2.z * ( N.z >= 0.0 ? C[4] : C[5] );
}

/* the light at the sprite's point (posView, world P, height z in the fog's frame) toward the eye along dW (world):
 * thin = the air's J there (pi x radiance per unit opacity), thick = the wrapped diffuse at world normal N */
void vpLight( vec3 posView, vec3 P, vec3 dW, float z, vec3 N, float w, out vec3 thin, out vec3 thick )
{
	thin = vec3( 0.0 );
	thick = vec3( 0.0 );
	vec3 skyThin = vec3( 0.0 ), skyThick = vec3( 0.0 );	// the sky's share: the records' colour at the end (the air's rule)
	airGroundAt( P );	// FOGTERRAIN2: the ground layer on the pass's terrain grid, as the froxels at this point
	if ( ( volPartRed & 1 ) != 0 )
		N = cellWorldDir( vec3( 0.0, 0.0, 1.0 ) );	// red flat: every pixel faces the eye's axis
	if ( ( volTerms & 1 ) != 0 && cellHasDir ) {
		vec3 to = normalize( cellDirTo );
		float vis = ( !cellInterior && volCsm && ( volPartRed & 64 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		vec3 e = vpTint( cellDirColor ) * vis * airTup( z, to.z );
		skyThin += e * vpPhase( dot( to, dW ) );
		skyThick += e * vpWrap( dot( N, to ), w ) * vpPhaseN( dot( to, dW ) );
	}
	float zOff = z - P.z;	// the fog's height frame minus world z
	if ( ( volTerms & 2 ) != 0 && ( volPartRed & 8 ) == 0 ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
			float d = length( t0.xyz - P );
			if ( d >= t0.w || d < 0.001 )
				continue;
			vec3 L = ( t0.xyz - P ) / d, Lo;
			bool noSpec;
			vec3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L, as the air: no N.L, the shadow lifted toward the light
			if ( all( equal( e, vec3( 0.0 ) ) ) )
				continue;
			e = vpTint( e ) * exp( -airOd( z, t0.z + zOff, d ) );
			thin += e * vpPhase( dot( L, dW ) );
			thick += e * vpWrap( dot( N, L ), w ) * vpPhaseN( dot( L, dW ) );
		}
	}
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
	if ( wantGi || wantAmb ) {
	vec3 E[6];
	float share[6];
	for ( int a = 0; a < 6; a++ ) {
		E[a] = vec3( 0.0 );
		share[a] = 0.0;
	}
	if ( cellGiOn )
		volCubeAll( P, E, share );
	if ( wantGi ) {
		vec3 Eg[6];
		for ( int a = 0; a < 6; a++ )
			Eg[a] = E[a] * ( 1.0 / PI );	// the surfaces' GI unit -> the ambient's, as the air
		skyThin += volCubeScatter( Eg, dW );
		skyThick += vpCubeAt( Eg, N );
	}
	if ( wantAmb ) {
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
		vec3 Am[6];
		for ( int a = 0; a < 6; a++ )
			Am[a] = cellAmbient( axes[a], P ) * ( replace ? 1.0 - share[a] : 1.0 );
		skyThin += volCubeScatter( Am, dW );
		skyThick += vpCubeAt( Am, N );
	}
	}
	float t = length( posView ) * fogDistScale;
	vec3 dirView = posView / max( length( posView ), 1e-12 );
	thin += airEnvColour( skyThin, t, z, dirView );
	thick += airEnvColour( skyThick, t, z, dirView );
}

// the lit factor at this pixel (linear), in place of cellFxLit
vec3 vpLit( vec3 posView, vec3 Nview, float a )
{
	if ( ( fxLitMode & 8 ) != 0 || ( !cellInterior && !cellHasDir ) )
		return vec3( 1.0 );	// self-lit, as cellFxLit
	float m = ( volPartRed & 2 ) != 0 ? 1.0 : 1.0 - clamp( a, 0.0, 1.0 );
	vec3 thin, thick;
	vpLight( posView, cellWorldPos( posView ), cellWorldDir( posView ), dot( fogView.xyz, posView ) + fogView.w,
		cellWorldDir( Nview ), m, thin, thick );
	return mix( vec3( 1.0 ), m * thin + ( 1.0 - m ) * thick, lightingInfluence );
}

/* gates only: the lit share (no influence) of ONE white light from direction `to` (world, toward the light) on a puff
 * of opacity a facing N (world): m pi p(c) + (1 - m) wrap(N.to) p(c) / p_iso, c = to.dW -- the phase gate's turntable */
vec3 vpOneLight( vec3 dW, vec3 N, vec3 to, float a )
{
	float m = ( volPartRed & 2 ) != 0 ? 1.0 : 1.0 - clamp( a, 0.0, 1.0 );
	float c = dot( to, dW );
	return m * vpPhase( c ) + ( 1.0 - m ) * vpWrap( dot( N, to ), m ) * vpPhaseN( c );
}

/* gates only (volPartDump): probe slot `slot` at view point posView with opacity a: 0 thin (the air's J there), 1 thick at
 * N = +Z view, 2 thick at N = -Z view (w = 0), 3 world P + height z, 4 the eye's ray (world) + its distance (game
 * units), 5 the integrated air (L, T) at its column as the draw applies it (a = -1: none applied), 6 the lit factor at N = +Z with its own alpha (alpha in a),
 * 7 its view point + alpha */
vec4 vpProbe( int slot, vec3 posView, float a )
{
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( posView );
	float z = dot( fogView.xyz, posView ) + fogView.w;
	vec3 Zw = cellWorldDir( vec3( 0.0, 0.0, 1.0 ) );
	vec3 thin, thick;
	if ( slot <= 1 ) {
		vpLight( posView, P, dW, z, Zw, 0.0, thin, thick );
		return vec4( slot == 0 ? thin : thick, 1.0 );
	}
	if ( slot == 2 ) {
		vpLight( posView, P, dW, z, -Zw, 0.0, thin, thick );
		return vec4( thick, 1.0 );
	}
	if ( slot == 3 )
		return vec4( P, z );
	if ( slot == 4 )
		return vec4( dW, length( posView ) * fogDistScale );
	if ( slot == 5 ) {
		vec4 c = projectionMatrix * vec4( posView, 1.0 );
		return ( fogOn && ( volPartRed & 32 ) == 0 && volOn && volSlices.w > 0.5 ) ? wwVolAirAtUv( posView, c.xy / c.w * 0.5 + 0.5 ) : vec4( 0.0, 0.0, 0.0, -1.0 );
	}
	if ( slot == 6 )
		return vec4( vpLit( posView, vec3( 0.0, 0.0, 1.0 ), a ), a );
	return vec4( posView, a );
}
